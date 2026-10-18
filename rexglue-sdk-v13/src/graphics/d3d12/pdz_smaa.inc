// Three-pass SMAA 1x. Called with color input SRV and guest output UAV.
bool D3D12CommandProcessor::ApplySmaa(ID3D12Resource* destination,uint32_t width,uint32_t height) {
  const auto& provider=GetD3D12Provider();auto* device=provider.GetDevice();
  if(!smaa_root_) {
    D3D12_ROOT_PARAMETER parameters[6]={};D3D12_DESCRIPTOR_RANGE ranges[5]={};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants={0,0,4};
    for(UINT i=0;i<5;++i) {
      ranges[i].RangeType=i==0?D3D12_DESCRIPTOR_RANGE_TYPE_UAV:D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      ranges[i].NumDescriptors=1;ranges[i].BaseShaderRegister=i==0?0:i-1;
      parameters[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      parameters[i+1].DescriptorTable={1,&ranges[i]};
    }
    D3D12_STATIC_SAMPLER_DESC samplers[2]={};
    for(UINT i=0;i<2;++i) {
      samplers[i].Filter=i?D3D12_FILTER_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
      samplers[i].AddressU=samplers[i].AddressV=samplers[i].AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      samplers[i].MaxAnisotropy=1;samplers[i].ShaderRegister=i;
      samplers[i].ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;
    }
    D3D12_ROOT_SIGNATURE_DESC desc={6,parameters,2,samplers,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    *smaa_root_.ReleaseAndGetAddressOf()=ui::d3d12::util::CreateRootSignature(provider,desc);
    if(!smaa_root_)return false;
  }
  if(!smaa_pipelines_[2]) {
    const void* bytes[]={shaders::pdz_smaa_edges_cs,shaders::pdz_smaa_weights_cs,shaders::pdz_smaa_blend_cs};
    const size_t sizes[]={sizeof(shaders::pdz_smaa_edges_cs),sizeof(shaders::pdz_smaa_weights_cs),sizeof(shaders::pdz_smaa_blend_cs)};
    for(int i=0;i<3;++i) {
      *smaa_pipelines_[i].ReleaseAndGetAddressOf()=ui::d3d12::util::CreateComputePipeline(device,bytes[i],sizes[i],smaa_root_.Get());
      if(!smaa_pipelines_[i])return false;
    }
    REXLOG_INFO("SMAA 1x High: all three pipelines created");
  }
  const auto create_texture=[&](uint32_t w,uint32_t h,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state,Microsoft::WRL::ComPtr<ID3D12Resource>& resource) {
    D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=w;desc.Height=h;desc.DepthOrArraySize=1;desc.MipLevels=1;
    desc.Format=format;desc.SampleDesc.Count=1;desc.Flags=flags;
    return SUCCEEDED(device->CreateCommittedResource(&ui::d3d12::util::kHeapPropertiesDefault,provider.GetHeapFlagCreateNotZeroed(),&desc,state,nullptr,IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())));
  };
  for(int i=0;i<2;++i) {
    if(smaa_intermediate_[i]&&(smaa_intermediate_[i]->GetDesc().Width!=width||smaa_intermediate_[i]->GetDesc().Height!=height)) {
      if(submission_completed_<smaa_submission_) {
        smaa_intermediate_[i]->AddRef();resources_for_deletion_.emplace_back(smaa_submission_,smaa_intermediate_[i].Get());
      }
      smaa_intermediate_[i].Reset();
    }
    if(!smaa_intermediate_[i]&&!create_texture(width,height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,smaa_intermediate_[i]))return false;
  }
  // Keep uploads alive alongside the tiny immutable LUTs until shutdown.
  for(int i=0;i<2;++i)if(!smaa_lookup_[i]) {
    const uint32_t w=i?SEARCHTEX_WIDTH:AREATEX_WIDTH,h=i?SEARCHTEX_HEIGHT:AREATEX_HEIGHT;
    const uint32_t pitch=i?SEARCHTEX_PITCH:AREATEX_PITCH;
    const auto* bytes=i?searchTexBytes:areaTexBytes;
    const DXGI_FORMAT format=i?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R8G8_UNORM;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture,upload;
    if(!create_texture(w,h,format,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,texture))return false;
    const auto desc=texture->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};UINT64 total;
    device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&total);
    D3D12_RESOURCE_DESC buffer={};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=total;
    buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap={};heap.Type=D3D12_HEAP_TYPE_UPLOAD;heap.CreationNodeMask=heap.VisibleNodeMask=1;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))))return false;
    uint8_t* mapping=nullptr;D3D12_RANGE read={0,0};if(FAILED(upload->Map(0,&read,reinterpret_cast<void**>(&mapping))))return false;
    for(uint32_t row=0;row<h;++row)std::memcpy(mapping+footprint.Offset+row*footprint.Footprint.RowPitch,bytes+row*pitch,pitch);
    upload->Unmap(0,nullptr);
    D3D12_TEXTURE_COPY_LOCATION target={};target.pResource=texture.Get();target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION source={};source.pResource=upload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprint;
    deferred_command_list_.D3DCopyTextureRegion(&target,0,0,0,&source,nullptr);
    PushTransitionBarrier(texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    smaa_lookup_[i]=std::move(texture);smaa_upload_[i]=std::move(upload);
  }
  // Acquire everything before dispatch; a failure cannot leave partial AA output.
  ui::d3d12::util::DescriptorCpuGpuHandlePair descriptors[15];
  if(!RequestOneUseSingleViewDescriptors(15,descriptors))return false;
  deferred_command_list_.D3DSetComputeRootSignature(smaa_root_.Get());
  FxaaConstants constants={{width,height},{1.f/width,1.f/height}};
  deferred_command_list_.D3DSetComputeRoot32BitConstants(0,4,&constants,0);
  for(int pass=0;pass<3;++pass) {
    ID3D12Resource* dest=pass<2?smaa_intermediate_[pass].Get():destination;
    if(pass<2)PushTransitionBarrier(dest,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav={};uav.Format=pass<2?DXGI_FORMAT_R8G8B8A8_UNORM:ui::d3d12::D3D12Presenter::kGuestOutputFormat;
    uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(dest,nullptr,&uav,descriptors[pass*5].first);
    deferred_command_list_.D3DSetComputeRootDescriptorTable(1,descriptors[pass*5].second);
    ID3D12Resource* inputs[]={fxaa_source_texture_.Get(),pass==0?fxaa_source_texture_.Get():smaa_intermediate_[pass==2?1:0].Get(),smaa_lookup_[0].Get(),smaa_lookup_[1].Get()};
    for(int slot=0;slot<4;++slot) {
      D3D12_SHADER_RESOURCE_VIEW_DESC srv={};srv.Format=inputs[slot]->GetDesc().Format;
      srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
      device->CreateShaderResourceView(inputs[slot],&srv,descriptors[pass*5+1+slot].first);
      deferred_command_list_.D3DSetComputeRootDescriptorTable(2+slot,descriptors[pass*5+1+slot].second);
    }
    SetExternalPipeline(smaa_pipelines_[pass].Get());SubmitBarriers();
    deferred_command_list_.D3DDispatch((width+15)/16,(height+7)/8,1);
    if(pass<2)PushTransitionBarrier(dest,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }
  smaa_submission_=submission_current_;
  return true;
}
