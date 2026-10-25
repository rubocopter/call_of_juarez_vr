#include "backends/openvr/temporal_guides.hpp"
#include <d3d11.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstring>

using namespace cojvr::backends::openvr::dlss;
using Microsoft::WRL::ComPtr;
namespace {
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
ComPtr<ID3D11Texture2D> Make(ID3D11Device* device,DXGI_FORMAT format,const void* data,UINT pitch) {
    D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=16;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
    d.Format=format;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{data,pitch,0};
    ComPtr<ID3D11Texture2D> texture;Check(SUCCEEDED(device->CreateTexture2D(&d,&initial,&texture)),"fixture texture");return texture;
}
std::array<unsigned char,8> Read(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* input,UINT x,UINT y,UINT bytes) {
    D3D11_TEXTURE2D_DESC d{};input->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=0;
    ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging");
    context->CopyResource(staging.Get(),input);D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"map");
    std::array<unsigned char,8> result{};std::copy_n(static_cast<const unsigned char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch+x*bytes,bytes,result.data());
    context->Unmap(staging.Get(),0);return result;
}
}
int main()try {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"WARP");
    std::vector<std::uint32_t> colors(256,0xFF4080C0);std::vector<std::uint16_t> depth(256*4,0);std::vector<unsigned char> mask(256,0);
    for(unsigned p=0;p<256;++p)depth[p*4+3]=0x4900;
    depth[3]=0x7E00;mask[8*16+8]=255;
    auto c=Make(device.Get(),DXGI_FORMAT_B8G8R8A8_UNORM,colors.data(),64);
    auto z=Make(device.Get(),DXGI_FORMAT_R16G16B16A16_FLOAT,depth.data(),128);
    auto m=Make(device.Get(),DXGI_FORMAT_R8_UNORM,mask.data(),16);
    GuideSource source;source.color=c.Get();source.linear_depth_alpha=z.Get();source.current_color_mask=m.Get();
    source.near_metres=.5F;source.far_metres=100;source.history=true;
    TemporalGuides guides;std::array<GuideSource,2> pair{source,source};
    Check(!guides.Generate(device.Get(),context.Get(),pair)&&!guides.EyeInputs(0).color,"unqualified source cannot become a guide");
    for(auto& s:pair)s.source_qualified=true;
    pair[1].clip_to_previous[12]=.25F;
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    const D3D11_VIEWPORT sentinel{2,3,7,9,.1F,.9F};context->RSSetViewports(1,&sentinel);
    Check(guides.Generate(device.Get(),context.Get(),pair),"guide generation");
    D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{};UINT viewports=1;context->RSGetViewports(&viewports,&restored);
    Check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&viewports==1&&std::memcmp(&sentinel,&restored,sizeof(sentinel))==0,
        "guide generation changed caller pipeline state");
    const auto left=guides.EyeInputs(0),right=guides.EyeInputs(1);
    auto pixel=Read(device.Get(),context.Get(),left.depth,4,4,4);float value=0;std::memcpy(&value,pixel.data(),4);
    const float expected=100.F/99.5F-50.F/(99.5F*10.F);
    Check(std::abs(value-expected)<1e-6F,"axial metre to standard device-depth conversion");
    pixel=Read(device.Get(),context.Get(),left.motion,4,4,4);std::uint16_t zero[2]{};std::memcpy(zero,pixel.data(),4);
    Check(zero[0]==0&&zero[1]==0,"static-eye camera vectors not zero");
    pixel=Read(device.Get(),context.Get(),right.motion,4,4,4);std::uint16_t motion[2]{};std::memcpy(motion,pixel.data(),4);
    Check(motion[0]==0x3000&&motion[1]==0,"per-eye current-to-previous UV vector sign/scale");
    Check(Read(device.Get(),context.Get(),left.bias,8,8,1)[0]==255&&Read(device.Get(),context.Get(),left.bias,0,0,1)[0]==255&&
        Read(device.Get(),context.Get(),left.bias,4,4,1)[0]==0,"dynamic/invalid source masks");
    pair[0].color=left.color;
    Check(!guides.Generate(device.Get(),context.Get(),pair)&&!guides.EyeInputs(0).color,"private output accepted as its own shader input");
    pair[0].color=c.Get();
    pair[1].source_qualified=false;
    Check(!guides.Generate(device.Get(),context.Get(),pair)&&!guides.EyeInputs(0).color,"invalid second eye cannot expose stale guides");
    guides.Reset();Check(!guides.EyeInputs(1).color,"reset retains output");
    std::cout<<"Real GPU temporal guide depth, per-eye reprojection, dynamic/invalid masks and qualification gates passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
