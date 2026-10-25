#include "backends/openvr/temporal_guides.hpp"
#include <d3dcompiler.h>
#include <cmath>

namespace cojvr::backends::openvr::dlss {
namespace {
constexpr std::array<DXGI_FORMAT,4> formats{DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R8_UNORM};
constexpr char shader[]=R"(
cbuffer C:register(b0) {row_major float4x4 reproject;float4 optics;float4 sampling;};
Texture2D<float4> color:register(t0);Texture2D<float4> depthAlpha:register(t1);Texture2D<float> mask:register(t2);
float4 VS(uint id:SV_VertexID):SV_Position {float2 uv=float2((id<<1)&2,id&2);return float4(uv.x*2-1,1-uv.y*2,0,1);}
struct Output {float4 color:SV_Target0;float depth:SV_Target1;float2 motion:SV_Target2;float bias:SV_Target3;};
Output PS(float4 position:SV_Position) {
    int2 p=int2(position.xy);Output o;o.color=color.Load(int3(p,0));
    float z=depthAlpha.Load(int3(p,0)).a;bool valid=isfinite(z)&&z>=optics.x&&z<=optics.y;
    float standardZ=optics.y/(optics.y-optics.x)-optics.y*optics.x/((optics.y-optics.x)*max(z,optics.x));
    o.depth=valid?saturate(standardZ):1;
    float2 uv=(position.xy+sampling.xy)/optics.zw;
    float4 previous=mul(float4(uv.x*2-1,1-uv.y*2,o.depth,1),reproject);
    bool projectionValid=isfinite(previous.w)&&previous.w>1e-6;
    float2 oldUV=float2(previous.x/max(previous.w,1e-6)*.5+.5,.5-previous.y/max(previous.w,1e-6)*.5);
    projectionValid=projectionValid&&all(isfinite(oldUV))&&all(oldUV>=0)&&all(oldUV<=1);
    o.motion=(sampling.z>.5&&valid&&projectionValid)?oldUV-uv:float2(0,0);
    o.bias=(!valid||!projectionValid)?1:saturate(mask.Load(int3(p,0)));
    return o;
})";
struct Constants {runtime::TemporalMatrix matrix;float optics[4],sampling[4];};
static_assert(sizeof(Constants)==96);
}
void TemporalGuides::Reset() noexcept {
    outputs_={};for(auto& pair:views_)for(auto& view:pair)view.Reset();
    for(auto& pair:textures_)for(auto& texture:pair)texture.Reset();
    constants_.Reset();vertex_.Reset();pixel_.Reset();raster_.Reset();blend_.Reset();state_.Reset();owner_.Reset();width_=height_=0;
}
bool TemporalGuides::Prepare(ID3D11Device* device,unsigned width,unsigned height) noexcept {
    const auto check=[&](HRESULT result){result_=result;return SUCCEEDED(result);};
    if(owner_.Get()!=device||width_!=width||height_!=height) {Reset();owner_=device;}
    if(state_&&pixel_&&textures_[1][3])return true;
    ComPtr<ID3D11Device1> device1;if(!check(device->QueryInterface(IID_PPV_ARGS(&device1))))return false;
    const auto feature=device->GetFeatureLevel();
    const auto flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    if(!state_&&!check(device1->CreateDeviceContextState(flags,&feature,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&state_)))return false;
    ComPtr<ID3DBlob> vertex,pixel;
    if(!check(D3DCompile(shader,sizeof(shader)-1,"CoJVR temporal guides",nullptr,nullptr,"VS","vs_4_0",0,0,&vertex,nullptr))||
       !check(D3DCompile(shader,sizeof(shader)-1,"CoJVR temporal guides",nullptr,nullptr,"PS","ps_4_0",0,0,&pixel,nullptr))||
       !check(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,vertex_.ReleaseAndGetAddressOf()))||
       !check(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,pixel_.ReleaseAndGetAddressOf())))return false;
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Constants);buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_BLEND_DESC blend{};for(auto& target:blend.RenderTarget)target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(!check(device->CreateBuffer(&buffer,nullptr,constants_.ReleaseAndGetAddressOf()))||
       !check(device->CreateRasterizerState(&raster,raster_.ReleaseAndGetAddressOf()))||
       !check(device->CreateBlendState(&blend,blend_.ReleaseAndGetAddressOf())))return false;
    for(unsigned eye=0;eye<2;++eye)for(unsigned slot=0;slot<4;++slot) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=formats[slot];d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(!check(device->CreateTexture2D(&d,nullptr,textures_[eye][slot].ReleaseAndGetAddressOf()))||
           !check(device->CreateRenderTargetView(textures_[eye][slot].Get(),nullptr,views_[eye][slot].ReleaseAndGetAddressOf())))return false;
    }
    width_=width;height_=height;return true;
}
bool TemporalGuides::Generate(ID3D11Device* device,ID3D11DeviceContext* context,const std::array<GuideSource,2>& sources) noexcept {
    outputs_={};result_=E_INVALIDARG;
    if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);if(owner.Get()!=device)return false;
    ComPtr<ID3D11DeviceContext1> context1;if(FAILED(context->QueryInterface(IID_PPV_ARGS(&context1))))return false;
    std::array<std::array<ComPtr<ID3D11ShaderResourceView>,3>,2> inputs;
    unsigned width=0,height=0;
    for(unsigned eye=0;eye<2;++eye) {
        const auto& s=sources[eye];
        if(!s.source_qualified||!std::isfinite(s.near_metres)||!std::isfinite(s.far_metres)||
           s.near_metres<=0||s.far_metres<=s.near_metres||!std::isfinite(s.jitter_x)||!std::isfinite(s.jitter_y)||
           std::abs(s.jitter_x)>.5F||std::abs(s.jitter_y)>.5F)return false;
        for(auto v:s.clip_to_previous)if(!std::isfinite(v))return false;
        const std::array<ID3D11Texture2D*,3> textures{s.color,s.linear_depth_alpha,s.current_color_mask};
        for(unsigned slot=0;slot<3;++slot) {
            if(!textures[slot])return false;D3D11_TEXTURE2D_DESC d{};textures[slot]->GetDesc(&d);
            // Private outputs must never feed a pass that writes them again.
            for(const auto& own_eye:textures_)for(const auto& own:own_eye)
                if(own.Get()==textures[slot])return false;
            owner.Reset();textures[slot]->GetDevice(&owner);
            if(!width){width=d.Width;height=d.Height;}
            if(owner.Get()!=device||!width||!height||width>8192||height>8192||std::uint64_t(width)*height>16ULL*1024*1024||
               d.Width!=width||d.Height!=height||d.ArraySize!=1||d.MipLevels!=1||d.SampleDesc.Count!=1||
               !(d.BindFlags&D3D11_BIND_SHADER_RESOURCE))return false;
            if((slot==0&&d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8X8_UNORM&&d.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT)||
               (slot==1&&d.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT)||(slot==2&&d.Format!=DXGI_FORMAT_R8_UNORM))return false;
            result_=device->CreateShaderResourceView(textures[slot],nullptr,&inputs[eye][slot]);if(FAILED(result_))return false;
        }
    }
    if(!Prepare(device,width,height))return false;
    ComPtr<ID3DDeviceContextState> previous;context1->SwapDeviceContextState(state_.Get(),&previous);
    context->VSSetShader(vertex_.Get(),nullptr,0);context->PSSetShader(pixel_.Get(),nullptr,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetState(raster_.Get());context->OMSetBlendState(blend_.Get(),nullptr,UINT_MAX);
    const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
    auto* buffer=constants_.Get();context->PSSetConstantBuffers(0,1,&buffer);
    for(unsigned eye=0;eye<2;++eye) {
        const auto& s=sources[eye];const Constants c{s.clip_to_previous,{s.near_metres,s.far_metres,float(width),float(height)},
            {s.jitter_x,s.jitter_y,s.history?1.F:0.F,0}};
        context->UpdateSubresource(buffer,0,nullptr,&c,0,0);
        const std::array<ID3D11ShaderResourceView*,3> in{inputs[eye][0].Get(),inputs[eye][1].Get(),inputs[eye][2].Get()};
        const std::array<ID3D11RenderTargetView*,4> out{views_[eye][0].Get(),views_[eye][1].Get(),views_[eye][2].Get(),views_[eye][3].Get()};
        context->PSSetShaderResources(0,3,in.data());context->OMSetRenderTargets(4,out.data(),nullptr);context->Draw(3,0);
    }
    context->ClearState();context1->SwapDeviceContextState(previous.Get(),nullptr);
    result_=device->GetDeviceRemovedReason();if(FAILED(result_))return false;
    for(unsigned eye=0;eye<2;++eye)outputs_[eye]={textures_[eye][0].Get(),textures_[eye][1].Get(),textures_[eye][2].Get(),textures_[eye][3].Get(),
        !sources[eye].history,sources[eye].jitter_x,sources[eye].jitter_y};
    return true;
}
Inputs TemporalGuides::EyeInputs(unsigned eye) const noexcept {return eye<2?outputs_[eye]:Inputs{};}
}
