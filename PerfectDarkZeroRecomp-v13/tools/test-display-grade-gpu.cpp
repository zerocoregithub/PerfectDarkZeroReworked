// Isolated D3D12 shader/readback test. Never attaches to or launches the game.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <rex/graphics/display_grade.h>
#include "../../rexglue-sdk-source/src/graphics/shaders/bytecode/d3d12_5_1/pdz_display_grade_cs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
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
int main() {
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
  params[0].Constants.Num32BitValues = 6;
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
  pipeline_desc.CS = {pdz_display_grade_cs,sizeof(pdz_display_grade_cs)};
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
    for (rex::gpu::DisplayGrade grade : {rex::gpu::DisplayGrade{}, {1,0,0}, {1.3f,.7f,.2f}, {.5f,2,-.3f}}) {
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
      struct {uint32_t size[2]; float contrast,saturation,tint; uint32_t fxaa;} constants{{width,height},grade.contrast,grade.saturation,grade.tint,wide ? 1u : 0u};
      commands->SetComputeRoot32BitConstants(0,6,&constants,0);
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
        auto expected=rex::gpu::ApplyDisplayGrade(input[y*width+x],grade);
        for (unsigned c=0;c<3;++c) {
          uint32_t actual=uint32_t(packed>>(c*(wide?16:10)))&maximum;
          auto reference=uint32_t(std::lround(expected[c]*maximum));
          assert(std::abs(int(actual)-int(reference))<=2); ++comparisons;
        }
        if (wide) {
          float luma=expected[0]*.299f+expected[1]*.587f+expected[2]*.114f;
          assert(std::abs(int(packed>>48)-int(std::lround(luma*65535)))<=2);
        } else assert(packed>>30 == 2); // Alpha retained when not FXAA luma.
      }
      readback->Unmap(0,nullptr);
    }
  }
  CloseHandle(event);
  std::printf("PASS: %zu GPU RGB comparisons, both output formats, FXAA luma, alpha, partial groups\n",comparisons);
}
