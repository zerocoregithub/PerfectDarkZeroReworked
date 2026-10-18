// Isolated D3D12 shader/readback test. Never attaches to or launches the game.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <rex/graphics/display_grade.h>
#include <rex/graphics/pdz_scene_grade_dxbc.h>
#include <rex/graphics/pdz_scene_grade.h>
#include "../../rexglue-sdk-v13/thirdparty/dxbc/DXBCChecksum.h"
#include "../../rexglue-sdk-v13/src/graphics/shaders/bytecode/d3d12_5_1/pdz_display_grade_cs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <bit>
// Standalone equivalent of the SDK's bit-scan utility used by the assembler.
namespace rex {
bool bit_scan_forward(uint32_t value, uint32_t* index) {
  if (!value) return false;
  *index = std::countr_zero(value);
  return true;
}
}
using Microsoft::WRL::ComPtr;
static void Check(HRESULT hr) {
  if (FAILED(hr)) { std::printf("FAIL HRESULT %08lx\n", hr); std::exit(1); }
}
static D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
  return barrier;
}

static std::vector<uint32_t> BuildActualSceneEpilogue() {
  namespace d = rex::graphics::dxbc;
  std::vector<uint32_t> code = {d::VersionToken(d::ProgramType::kComputeShader,5,1),0};
  d::Statistics stats{};
  d::Assembler a(code,stats);
  a.OpDclGlobalFlags(0);
  a.OpDclConstantBuffer(d::Src::CB(d::Src::Dcl,0,0,0),2);
  a.OpDclUnorderedAccessViewTyped(d::ResourceDimension::kTexture2D,0,0x5555,d::Src::U(d::Src::Dcl,0,0,0));
  a.OpDclInput(d::Dest::VThreadID(3));
  a.OpDclTemps(2);
  a.OpDclThreadGroup(16,8,1);
  a.OpUGE(d::Dest::R(1,3),d::Src::VThreadID(),d::Src::CB(0,0,0));
  a.OpOr(d::Dest::R(1,1),d::Src::R(1).Select(0),d::Src::R(1).Select(1));
  a.OpRetC(true,d::Src::R(1).Select(0));
  a.OpLdUAVTyped(d::Dest::R(0),d::Src::VThreadID(),2,d::Src::U(0,0));
  rex::graphics::EmitPdzSceneGrade(a,0,1,d::Src::CB(0,0,1));
  a.OpStoreUAVTyped(d::Dest::U(0,0),d::Src::VThreadID(),2,d::Src::R(0));
  a.OpRet();
  code[1]=uint32_t(code.size());
  // Keep the FXC container's feature/signature chunks; replace shader code
  // with the exact assembler helper used by the real guest translator.
  std::vector<uint32_t> original(sizeof(pdz_display_grade_cs)/4);
  std::memcpy(original.data(),pdz_display_grade_cs,sizeof(pdz_display_grade_cs));
  std::vector<uint32_t> result(original.begin(),original.begin()+8+original[7]);
  for (uint32_t i=0;i<original[7];++i) {
    uint32_t offset=original[8+i]/4;
    result[8+i]=uint32_t(result.size()*4);
    if (original[offset] == d::MakeFourCC('S','H','E','X')) {
      result.push_back(original[offset]); result.push_back(uint32_t(code.size()*4));
      result.insert(result.end(),code.begin(),code.end());
    } else {
      uint32_t words=2+original[offset+1]/4;
      result.insert(result.end(),original.begin()+offset,original.begin()+offset+words);
    }
  }
  result[6]=uint32_t(result.size()*4);
  CalculateDXBCChecksum(reinterpret_cast<unsigned char*>(result.data()),result[6],result.data()+1);
  return result;
}

int main() {
  assert(rex::gpu::IsPdzSceneComposite(0x16C954AF5D9DEBD0ull));
  assert(rex::gpu::IsPdzSceneComposite(0xEDCCE21F19A013D1ull));
  assert(!rex::gpu::IsPdzSceneComposite(0));
  ComPtr<IDXGIFactory4> factory;
  Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
  ComPtr<IDXGIAdapter1> adapter;
  Check(factory->EnumAdapters1(0,&adapter));
  DXGI_ADAPTER_DESC1 adapter_desc{};
  Check(adapter->GetDesc1(&adapter_desc));
  std::wprintf(L"Testing shader on %ls\n",adapter_desc.Description);
  ComPtr<ID3D12Device> device;
  Check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
  D3D12_ROOT_PARAMETER params[2]{};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  params[0].Constants.Num32BitValues = 8;
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0};
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable = {1,&range};
  D3D12_ROOT_SIGNATURE_DESC signature_desc{2,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  ComPtr<ID3DBlob> blob, error;
  Check(D3D12SerializeRootSignature(&signature_desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error));
  ComPtr<ID3D12RootSignature> signature;
  Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&signature)));
  D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};
  pipeline_desc.pRootSignature = signature.Get();
  auto actual_shader = BuildActualSceneEpilogue();
  pipeline_desc.CS = {actual_shader.data(),actual_shader.size()*4};
  ComPtr<ID3D12PipelineState> pipeline;
  Check(device->CreateComputePipelineState(&pipeline_desc,IID_PPV_ARGS(&pipeline)));
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  ComPtr<ID3D12CommandQueue> queue;
  Check(device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue)));
  ComPtr<ID3D12CommandAllocator> allocator;
  Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
  ComPtr<ID3D12GraphicsCommandList> commands;
  Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));
  Check(commands->Close());
  ComPtr<ID3D12Fence> fence;
  Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
  HANDLE event = CreateEventW(nullptr,FALSE,FALSE,nullptr);
  assert(event);
  uint64_t fence_value = 0;
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
  ComPtr<ID3D12DescriptorHeap> heap;
  Check(device->CreateDescriptorHeap(&heap_desc,IID_PPV_ARGS(&heap)));
  size_t comparisons = 0;
  for (DXGI_FORMAT format : {DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R16G16B16A16_UNORM}) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)));
    assert(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD);
    const bool wide = format == DXGI_FORMAT_R16G16B16A16_UNORM;
    const uint32_t maximum = wide ? 65535 : 1023;
    const uint32_t bytes = wide ? 8 : 4;
    constexpr uint32_t width=19,height=11; // Non-multiple thread dimensions.
    D3D12_RESOURCE_DESC texture_desc{};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Width=width; texture_desc.Height=height;
    texture_desc.DepthOrArraySize=1; texture_desc.MipLevels=1;
    texture_desc.Format=format; texture_desc.SampleDesc.Count=1;
    texture_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES default_heap{D3D12_HEAP_TYPE_DEFAULT};
    ComPtr<ID3D12Resource> texture;
    Check(device->CreateCommittedResource(&default_heap,D3D12_HEAP_FLAG_NONE,&texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&texture)));
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 buffer_size;
    device->GetCopyableFootprints(&texture_desc,0,1,0,&footprint,nullptr,nullptr,&buffer_size);
    D3D12_RESOURCE_DESC buffer_desc{};
    buffer_desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer_desc.Width=buffer_size; buffer_desc.Height=1;
    buffer_desc.DepthOrArraySize=1; buffer_desc.MipLevels=1;
    buffer_desc.SampleDesc.Count=1; buffer_desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES upload_heap{D3D12_HEAP_TYPE_UPLOAD}, read_heap{D3D12_HEAP_TYPE_READBACK};
    ComPtr<ID3D12Resource> upload,readback;
    Check(device->CreateCommittedResource(&upload_heap,D3D12_HEAP_FLAG_NONE,&buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)));
    Check(device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,&buffer_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    uint8_t* mapped;
    Check(upload->Map(0,nullptr,reinterpret_cast<void**>(&mapped)));
    std::vector<std::array<float,3>> input;
    for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x) {
      uint32_t rgb[3] = {x*maximum/(width-1),y*maximum/(height-1),(x+y)*maximum/(width+height-2)};
      input.push_back({rgb[0]/float(maximum),rgb[1]/float(maximum),rgb[2]/float(maximum)});
      uint64_t packed = wide ? uint64_t(rgb[0])|(uint64_t(rgb[1])<<16)|(uint64_t(rgb[2])<<32)|(uint64_t(32768)<<48)
                             : uint64_t(rgb[0])|(uint64_t(rgb[1])<<10)|(uint64_t(rgb[2])<<20)|(uint64_t(2)<<30);
      std::memcpy(mapped+y*footprint.Footprint.RowPitch+x*bytes,&packed,bytes);
    }
    upload->Unmap(0,nullptr);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format=format; uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(texture.Get(),nullptr,&uav,heap->GetCPUDescriptorHandleForHeapStart());
    bool first = true;
    for (float gamma : {.5f,1.f,1.48f,2.f}) for (rex::gpu::DisplayGrade grade : {rex::gpu::DisplayGrade{}, {1,0,0}, {1.3f,.7f,.2f}, {.5f,2,-.3f}}) {
      Check(allocator->Reset()); Check(commands->Reset(allocator.Get(),pipeline.Get()));
      if (!first) { auto barrier=Transition(texture.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST); commands->ResourceBarrier(1,&barrier); }
      first=false;
      D3D12_TEXTURE_COPY_LOCATION source{},dest{};
      source.pResource=upload.Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; source.PlacedFootprint=footprint;
      dest.pResource=texture.Get(); dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      commands->CopyTextureRegion(&dest,0,0,0,&source,nullptr);
      auto barrier=Transition(texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commands->ResourceBarrier(1,&barrier);
      commands->SetComputeRootSignature(signature.Get());
      ID3D12DescriptorHeap* heaps[]={heap.Get()}; commands->SetDescriptorHeaps(1,heaps);
      struct {uint32_t size[2]; uint32_t padding[2]; float inverse_gamma,contrast,saturation,tint;} constants{{width,height},{0,0},1.f/gamma,grade.contrast,grade.saturation,grade.tint};
      commands->SetComputeRoot32BitConstants(0,8,&constants,0);
      commands->SetComputeRootDescriptorTable(1,heap->GetGPUDescriptorHandleForHeapStart());
      commands->Dispatch((width+15)/16,(height+7)/8,1);
      barrier=Transition(texture.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
      commands->ResourceBarrier(1,&barrier);
      source=dest; dest.pResource=readback.Get(); dest.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dest.PlacedFootprint=footprint;
      commands->CopyTextureRegion(&dest,0,0,0,&source,nullptr);
      Check(commands->Close()); ID3D12CommandList* lists[]={commands.Get()}; queue->ExecuteCommandLists(1,lists);
      Check(queue->Signal(fence.Get(),++fence_value)); Check(fence->SetEventOnCompletion(fence_value,event));
      assert(WaitForSingleObject(event,10000)==WAIT_OBJECT_0);
      Check(readback->Map(0,nullptr,reinterpret_cast<void**>(&mapped)));
      for (uint32_t y=0;y<height;++y) for (uint32_t x=0;x<width;++x) {
        uint64_t packed=0; std::memcpy(&packed,mapped+y*footprint.Footprint.RowPitch+x*bytes,bytes);
        auto rgb = input[y*width+x];
        if (gamma != 1.f) for (auto& channel : rgb) channel=std::pow(channel,1.f/gamma);
        auto expected=rex::gpu::ApplyDisplayGrade(rgb,grade);
        for (unsigned c=0;c<3;++c) {
          uint32_t actual=uint32_t(packed>>(c*(wide?16:10)))&maximum;
          auto reference=uint32_t(std::lround(expected[c]*maximum));
          assert(std::abs(int(actual)-int(reference))<=2); ++comparisons;
        }
        if (wide) assert(packed>>48 == 32768);
        else assert(packed>>30 == 2); // Scene grading never changes alpha.
      }
      readback->Unmap(0,nullptr);
    }
  }
  CloseHandle(event);
  std::printf("PASS: %zu GPU RGB comparisons, actual translator bytecode, gamma, grading, both formats, alpha, partial groups\n",comparisons);
}
