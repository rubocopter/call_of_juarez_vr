#include "backends/openvr/depth_mask_history.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>
using namespace cojvr::backends::openvr::dlss;
using Microsoft::WRL::ComPtr;
namespace {
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
ComPtr<ID3D11Texture2D> Make(ID3D11Device* device,DXGI_FORMAT format,const void* data,UINT pitch) {
    D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=16;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=format;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA init{data,pitch,0};
    ComPtr<ID3D11Texture2D> result;Check(SUCCEEDED(device->CreateTexture2D(&d,&init,&result)),"fixture texture");return result;
}
std::vector<unsigned char> Read(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source) {
    Check(source!=nullptr,"missing mask");D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);
    d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging");
    context->CopyResource(staging.Get(),source);D3D11_MAPPED_SUBRESOURCE m{};
    Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"map");std::vector<unsigned char> out(256);
    for(unsigned y=0;y<16;++y)std::memcpy(out.data()+y*16,static_cast<unsigned char*>(m.pData)+y*m.RowPitch,16);
    context->Unmap(staging.Get(),0);return out;
}
PairFrame Frame(ID3D11Device* device,unsigned number,bool history=false) {
    PairFrame f;f.identity={{reinterpret_cast<std::uintptr_t>(device),1,number,number,1,16,16,16,16,0,true,true},1,1,1};
    f.depth_mask.enabled=true;f.depth_mask.relative_tolerance=.05F;
    for(unsigned eye=0;eye<2;++eye) {
        auto& e=f.eyes[eye];e.identity=f.identity;e.eye=eye;e.input_token=number*2+eye;
        e.guides.source_qualified=true;e.guides.near_metres=.5F;e.guides.far_metres=100;
        e.guides.depth_encoding=DepthEncoding::standard_device_z;e.guides.history=history;
    }
    return f;
}
}
int main()try {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP");
    std::vector<float> left_depth(256,.5F),right_depth(256,.5F);std::vector<unsigned char> excluded(256,0);
    auto left=Make(device.Get(),DXGI_FORMAT_R32_FLOAT,left_depth.data(),64);
    auto right=Make(device.Get(),DXGI_FORMAT_R32_FLOAT,right_depth.data(),64);
    auto exclusions=Make(device.Get(),DXGI_FORMAT_R8_UNORM,excluded.data(),16);
    std::array<Inputs,2> inputs{};inputs[0].depth=left.Get();inputs[1].depth=right.Get();
    for(auto& i:inputs)i.bias=exclusions.Get();
    DepthMaskHistory history;std::array<ID3D11Texture2D*,2> masks{};
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    Check(!history.Commit(context.Get()),"commit without prepared pair accepted");
    auto first=Frame(device.Get(),1);
    Check(history.Prepare(device.Get(),context.Get(),first,inputs,masks),"first mask seed");
    for(auto* mask:masks)for(auto value:Read(device.Get(),context.Get(),mask))Check(value==255,"first pair consumed unseeded history");
    Check(history.Commit(context.Get())&&!history.Commit(context.Get()),"pair history commit was not single-use");
    left_depth[8*16+8]=.25F;
    context->UpdateSubresource(left.Get(),0,nullptr,left_depth.data(),64,0);
    auto second=Frame(device.Get(),2,true);
    Check(history.Prepare(device.Get(),context.Get(),second,inputs,masks),"consecutive mask");
    Check(Read(device.Get(),context.Get(),masks[0])[8*16+8]==255&&Read(device.Get(),context.Get(),masks[0])[5*16+5]==0,
        "retained depth aliased overwritten input or static geometry lost history");
    Check(Read(device.Get(),context.Get(),masks[1])[8*16+8]==0,"left depth contaminated right history");
    history.Invalidate();Check(!history.Commit(context.Get()),"aborted pair committed history");
    auto third=Frame(device.Get(),3);
    Check(history.Prepare(device.Get(),context.Get(),third,inputs,masks),"reset seed");
    for(auto value:Read(device.Get(),context.Get(),masks[0]))Check(value==255,"failed pair advanced reusable history");
    Check(history.Commit(context.Get()),"reset seed commit");
    auto fourth=Frame(device.Get(),4,true);
    Check(history.Prepare(device.Get(),context.Get(),fourth,inputs,masks)&&Read(device.Get(),context.Get(),masks[0])[8*16+8]==0,
        "fresh seed did not become usable history");
    history.Invalidate();auto reset=Frame(device.Get(),5);reset.depth_mask.infinite_far[1]=true;
    Check(history.Prepare(device.Get(),context.Get(),reset,inputs,masks)&&history.Commit(context.Get()),"infinite projection seed");
    auto bad=Frame(device.Get(),6,true);bad.depth_mask=reset.depth_mask;bad.eyes[1].input_token=reset.eyes[1].input_token;
    Check(!history.Prepare(device.Get(),context.Get(),bad,inputs,masks)&&!masks[0]&&!masks[1]&&!history.Commit(context.Get()),
        "repeated source token exposed mask or committed stale pair");
    history.Reset();Check(!history.Commit(context.Get()),"reset retained pending depth");
    D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
    Check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST,"history preparation/commit changed caller pipeline");
    std::cout<<"Transactional GPU mask history seed, private depth snapshot, abort, independent eyes and token retirement passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
