// Isolated SMAA shader/readback validation; never launches the game.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../../rexglue-sdk-v13/src/graphics/shaders/bytecode/d3d12_5_1/pdz_smaa_edges_cs.h"
#include "../../rexglue-sdk-v13/src/graphics/shaders/bytecode/d3d12_5_1/pdz_smaa_weights_cs.h"
#include "../../rexglue-sdk-v13/src/graphics/shaders/bytecode/d3d12_5_1/pdz_smaa_blend_cs.h"
#include "../../rexglue-sdk-v13/src/graphics/shaders/bytecode/d3d12_5_1/pdz_smaa_copy_cs.h"
#include "../../rexglue-sdk-v13/thirdparty/smaa/AreaTex.h"
#include "../../rexglue-sdk-v13/thirdparty/smaa/SearchTex.h"
using Microsoft::WRL::ComPtr;
static void Check(HRESULT hr){if(FAILED(hr)){std::printf("FAIL HRESULT %08lx\n",hr);std::exit(1);}}
static void Barrier(ID3D12GraphicsCommandList* commands,ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
 D3D12_RESOURCE_BARRIER b={};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};commands->ResourceBarrier(1,&b);
}
int main(){
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 ComPtr<IDXGIFactory4> factory;Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;Check(factory->EnumAdapters1(0,&adapter));
 DXGI_ADAPTER_DESC1 adapter_desc={};adapter->GetDesc1(&adapter_desc);std::wprintf(L"SMAA test: %ls\n",adapter_desc.Description);
 ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 ComPtr<ID3D12InfoQueue> info;device.As(&info);
 D3D12_ROOT_PARAMETER params[6]={};D3D12_DESCRIPTOR_RANGE ranges[5]={};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[0].Constants={0,0,4};
 for(int i=0;i<5;++i){ranges[i]={i?D3D12_DESCRIPTOR_RANGE_TYPE_SRV:D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,UINT(i?i-1:0),0,0};params[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i+1].DescriptorTable={1,&ranges[i]};}
 D3D12_STATIC_SAMPLER_DESC samplers[2]={};for(int i=0;i<2;++i){samplers[i].Filter=i?D3D12_FILTER_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;samplers[i].AddressU=samplers[i].AddressV=samplers[i].AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samplers[i].ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;samplers[i].MaxAnisotropy=1;samplers[i].ShaderRegister=i;}
 D3D12_ROOT_SIGNATURE_DESC rd={6,params,2,samplers,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob,errors;
 Check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));ComPtr<ID3D12RootSignature> root;
 Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 const void* bytes[]={pdz_smaa_edges_cs,pdz_smaa_weights_cs,pdz_smaa_blend_cs,pdz_smaa_copy_cs};
 const size_t sizes[]={sizeof(pdz_smaa_edges_cs),sizeof(pdz_smaa_weights_cs),sizeof(pdz_smaa_blend_cs),sizeof(pdz_smaa_copy_cs)};
 std::array<ComPtr<ID3D12PipelineState>,4> pipelines;
 for(int i=0;i<4;++i){D3D12_COMPUTE_PIPELINE_STATE_DESC p={};p.pRootSignature=root.Get();p.CS={bytes[i],sizes[i]};Check(device->CreateComputePipelineState(&p,IID_PPV_ARGS(&pipelines[i])));}
 D3D12_COMMAND_QUEUE_DESC q={};ComPtr<ID3D12CommandQueue> queue;Check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> commands;Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));
 constexpr UINT width=97,height=65;
 const auto resource=[&](D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
   D3D12_HEAP_PROPERTIES heap={};heap.Type=type;heap.CreationNodeMask=heap.VisibleNodeMask=1;
   ComPtr<ID3D12Resource> r;Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&r)));return r;
 };
 const auto texture=[&](UINT w,UINT h,DXGI_FORMAT format,bool uav){D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Format=format;d.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;return resource(d,D3D12_HEAP_TYPE_DEFAULT,uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_COPY_DEST);};
 const auto buffer=[&](UINT64 size,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return resource(d,type,state);};
 std::array<ComPtr<ID3D12Resource>,6> textures={texture(width,height,DXGI_FORMAT_R16G16B16A16_UNORM,false),texture(width,height,DXGI_FORMAT_R8G8B8A8_UNORM,true),texture(width,height,DXGI_FORMAT_R8G8B8A8_UNORM,true),texture(AREATEX_WIDTH,AREATEX_HEIGHT,DXGI_FORMAT_R8G8_UNORM,false),texture(SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,DXGI_FORMAT_R8_UNORM,false),texture(width,height,DXGI_FORMAT_R10G10B10A2_UNORM,true)};
 std::vector<ComPtr<ID3D12Resource>> uploads;
 const auto upload=[&](int index,const void* data,UINT pitch){auto desc=textures[index]->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT f={};UINT64 size;device->GetCopyableFootprints(&desc,0,1,0,&f,nullptr,nullptr,&size);auto r=buffer(size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* ptr;D3D12_RANGE range={0,0};Check(r->Map(0,&range,&ptr));for(UINT y=0;y<desc.Height;++y)std::memcpy(static_cast<unsigned char*>(ptr)+f.Offset+y*f.Footprint.RowPitch,static_cast<const unsigned char*>(data)+y*pitch,pitch);r->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION dst={};dst.pResource=textures[index].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION src={};src.pResource=r.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=f;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);Barrier(commands.Get(),textures[index].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(r);};
 std::vector<uint16_t> input(width*height*4);
 for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){const uint16_t c=x>y*1.35+10?65535:0;for(int channel=0;channel<3;++channel)input[(y*width+x)*4+channel]=c;input[(y*width+x)*4+3]=65535;}
 upload(0,input.data(),width*8);upload(3,areaTexBytes,AREATEX_PITCH);upload(4,searchTexBytes,SEARCHTEX_PITCH);
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=15;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> heap;Check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));ID3D12DescriptorHeap* heaps[]={heap.Get()};commands->SetDescriptorHeaps(1,heaps);
 const UINT stride=device->GetDescriptorHandleIncrementSize(hd.Type);const auto cpu=[&](int i){auto h=heap->GetCPUDescriptorHandleForHeapStart();h.ptr+=i*stride;return h;};const auto gpu=[&](int i){auto h=heap->GetGPUDescriptorHandleForHeapStart();h.ptr+=i*stride;return h;};
 commands->SetComputeRootSignature(root.Get());struct Constants{UINT w,h;float iw,ih;} constants={width,height,1.f/width,1.f/height};commands->SetComputeRoot32BitConstants(0,4,&constants,0);
 for(int pass=0;pass<3;++pass){const int dest=pass<2?pass+1:5;D3D12_UNORDERED_ACCESS_VIEW_DESC u={};u.Format=textures[dest]->GetDesc().Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(textures[dest].Get(),nullptr,&u,cpu(pass*5));commands->SetComputeRootDescriptorTable(1,gpu(pass*5));const int indices[]={0,pass==0?0:pass==1?1:2,3,4};for(int i=0;i<4;++i){D3D12_SHADER_RESOURCE_VIEW_DESC s={};s.Format=textures[indices[i]]->GetDesc().Format;s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;s.Texture2D.MipLevels=1;device->CreateShaderResourceView(textures[indices[i]].Get(),&s,cpu(pass*5+1+i));commands->SetComputeRootDescriptorTable(i+2,gpu(pass*5+1+i));}commands->SetPipelineState(pipelines[pass].Get());commands->Dispatch((width+15)/16,(height+7)/8,1);Barrier(commands.Get(),textures[dest].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,pass<2?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COPY_SOURCE);}
 auto desc=textures[5]->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};UINT64 total;device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&total);auto readback=buffer(total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 D3D12_TEXTURE_COPY_LOCATION dst={};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;D3D12_TEXTURE_COPY_LOCATION src={};src.pResource=textures[5].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 Check(commands->Close());ID3D12CommandList* lists[]={commands.Get()};queue->ExecuteCommandLists(1,lists);ComPtr<ID3D12Fence> fence;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));Check(queue->Signal(fence.Get(),1));HANDLE event=CreateEvent(nullptr,FALSE,FALSE,nullptr);Check(fence->SetEventOnCompletion(1,event));assert(WaitForSingleObject(event,30000)==WAIT_OBJECT_0);CloseHandle(event);
 void* ptr;D3D12_RANGE read={0,SIZE_T(total)};Check(readback->Map(0,&read,&ptr));UINT softened=0,unchanged=0;for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){const UINT pixel=*reinterpret_cast<const UINT*>(static_cast<const unsigned char*>(ptr)+footprint.Offset+y*footprint.Footprint.RowPitch+x*4);const UINT value=pixel&1023;if(value>0&&value<1023)softened++;else unchanged++;if(x==0)assert(value==0);if(x==width-1&&y<height-4)assert(value==1023);}readback->Unmap(0,nullptr);assert(softened>30&&softened<width*height/4);assert(unchanged>width*height*3/4);Check(device->GetDeviceRemovedReason());
 if(info)for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n=0;info->GetMessage(i,nullptr,&n);std::vector<unsigned char> message(n);auto* m=reinterpret_cast<D3D12_MESSAGE*>(message.data());info->GetMessage(i,m,&n);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::printf("GPU error: %s\n",m->pDescription);return 1;}}
 std::printf("PASS: SMAA three passes, LUT uploads, %u softened diagonal pixels, %u untouched pixels, odd-sized bounds, matching game texture formats\n",softened,unchanged);
}
