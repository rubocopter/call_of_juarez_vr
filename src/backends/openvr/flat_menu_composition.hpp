#pragma once
#include <array>
#include <cstdint>
#include <utility>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace cojvr::backends::openvr {
struct MenuDrawRectangle { std::uint32_t left=0,top=0,width=0,height=0; };
struct MenuDrawPointer { bool active=false; float x=0,y=0,origin_x=0,origin_y=0; };

// Menu-only GPU sampling. Owns ONE source SRV texture, never an output target.
// Geometry changes reuse every texture. Output views are scoped to each call.
class FlatMenuCompositor final {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<ID3D11Device> owner_;
    ComPtr<ID3D11Texture2D> source_;
    ComPtr<ID3D11ShaderResourceView> source_view_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11DepthStencilState> depth_;
    ComPtr<ID3D11BlendState> blend_;
    std::uint32_t width_=0,height_=0,allocations_=0;
    HRESULT result_=S_OK;
    bool Check(HRESULT hr) noexcept {result_=hr;return SUCCEEDED(hr);}
    bool Prepare(ID3D11Device* device,std::uint32_t width,std::uint32_t height) noexcept {
        if(owner_.Get()!=device||!vs_||!ps_||!constants_||!sampler_||!raster_||!depth_||!blend_) {
            Reset();owner_=device;
            constexpr char shader[]=
                "struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD;};"
                "V VS(uint i:SV_VertexID){V o;o.uv=float2(i&1,i>>1);o.p=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}"
                "cbuffer C:register(b0){float4 source;float4 pointer;};"
                "Texture2D img:register(t0);SamplerState smp:register(s0);"
                "float4 PS(V v):SV_TARGET{float3 c=img.Sample(smp,v.uv).rgb;"
                "if(source.z>0){float2 p=v.uv*source.xy;float2 a=pointer.zw;float2 d=pointer.xy-a;"
                "float t=saturate(dot(p-a,d)/max(dot(d,d),1));"
                "float2 q=abs(p-pointer.xy);"
                "if(length(p-(a+t*d))<1.5)c=float3(.125,.941,1);"
                "if((q.x<1&&q.y<9)||(q.y<1&&q.x<9))c=float3(.125,.941,1);"
                "if(max(q.x,q.y)<1)c=1;}return float4(c,1);}";
            ComPtr<ID3DBlob> v,p;
            if(!Check(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",0,0,&v,nullptr))||
               !Check(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",0,0,&p,nullptr))||
               !Check(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_))||
               !Check(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps_)))return false;
            D3D11_BUFFER_DESC cb{};cb.ByteWidth=32;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
            D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
            D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;dd.DepthFunc=D3D11_COMPARISON_ALWAYS;
            D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
            if(!Check(device->CreateBuffer(&cb,nullptr,&constants_))||!Check(device->CreateSamplerState(&sd,&sampler_))||
               !Check(device->CreateRasterizerState(&rd,&raster_))||!Check(device->CreateDepthStencilState(&dd,&depth_))||
               !Check(device->CreateBlendState(&bd,&blend_)))return false;
        }
        if(!source_||width_!=width||height_!=height) {
            ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;
            D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(!Check(device->CreateTexture2D(&desc,nullptr,&texture))||!Check(device->CreateShaderResourceView(texture.Get(),nullptr,&view)))return false;
            source_=std::move(texture);source_view_=std::move(view);width_=width;height_=height;++allocations_;
        }
        return true;
    }
public:
    void Reset() noexcept {
        source_view_.Reset();source_.Reset();vs_.Reset();ps_.Reset();constants_.Reset();sampler_.Reset();
        raster_.Reset();depth_.Reset();blend_.Reset();owner_.Reset();width_=height_=allocations_=0;result_=S_OK;
    }
    HRESULT last_result() const noexcept{return result_;}
    std::uint32_t source_allocations() const noexcept{return allocations_;}
    bool Draw(ID3D11Device* device,ID3D11DeviceContext* context,const std::uint8_t* pixels,
        std::uint32_t width,std::uint32_t height,std::uint32_t stride,
        const std::array<ID3D11Texture2D*,2>& outputs,const std::array<MenuDrawRectangle,2>& rects,
        MenuDrawPointer pointer,bool clear) noexcept {
        result_=E_INVALIDARG;
        if(!device||!context||!pixels||!width||!height||width>16384||height>16384||
            std::uint64_t(stride)<std::uint64_t(width)*4||std::uint64_t(width)*height*4>128ULL*1024*1024)return false;
        ComPtr<ID3D11Device> context_device;context->GetDevice(&context_device);
        if(context_device.Get()!=device||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
        std::array<ComPtr<ID3D11RenderTargetView>,2> views;
        for(unsigned eye=0;eye<2;++eye) {
            if(!outputs[eye]||!rects[eye].width||!rects[eye].height)return false;
            ComPtr<ID3D11Device> target_device;outputs[eye]->GetDevice(&target_device);
            D3D11_TEXTURE2D_DESC desc{};outputs[eye]->GetDesc(&desc);
            const auto& r=rects[eye];
            if(target_device.Get()!=device||desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||desc.SampleDesc.Count!=1||
                std::uint64_t(r.left)+r.width>desc.Width||std::uint64_t(r.top)+r.height>desc.Height) {
                result_=E_INVALIDARG;return false;
            }
            if(!Check(device->CreateRenderTargetView(outputs[eye],nullptr,&views[eye])))return false;
        }
        if(!Prepare(device,width,height))return false;
        context->UpdateSubresource(source_.Get(),0,nullptr,pixels,stride,0);
        struct Constants{float source[4],pointer[4];} constants{{float(width),float(height),pointer.active?1.F:0.F,0},
            {pointer.x,pointer.y,pointer.origin_x,pointer.origin_y}};
        context->UpdateSubresource(constants_.Get(),0,nullptr,&constants,0,0);
        auto* cb=constants_.Get();auto* srv=source_view_.Get();auto* smp=sampler_.Get();
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        context->VSSetShader(vs_.Get(),nullptr,0);context->GSSetShader(nullptr,nullptr,0);
        context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);
        context->PSSetShader(ps_.Get(),nullptr,0);context->PSSetConstantBuffers(0,1,&cb);
        context->PSSetShaderResources(0,1,&srv);context->PSSetSamplers(0,1,&smp);
        context->RSSetState(raster_.Get());context->OMSetDepthStencilState(depth_.Get(),0);
        context->OMSetBlendState(blend_.Get(),nullptr,0xFFFFFFFF);
        for(unsigned eye=0;eye<2;++eye) {
            auto* view=views[eye].Get();const auto& r=rects[eye];
            if(clear){constexpr float black[4]={0,0,0,1};context->ClearRenderTargetView(view,black);}
            context->OMSetRenderTargets(1,&view,nullptr);
            const D3D11_VIEWPORT viewport{float(r.left),float(r.top),float(r.width),float(r.height),0,1};
            context->RSSetViewports(1,&viewport);context->Draw(4,0);
        }
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
        context->OMSetRenderTargets(0,nullptr,nullptr);
        return Check(device->GetDeviceRemovedReason());
    }
};
}
