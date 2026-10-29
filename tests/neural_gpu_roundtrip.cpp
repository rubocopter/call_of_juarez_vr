// Standalone synthetic D3D11 experiment. No game hooks or compositor submission.
// Residual composition follows Penumbra VR Framework / DirectNR; preserves the
// original full-size image instead of enlarging the small inference proxy.
#include "backends/openvr/neural_session.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::openvr::neural;
namespace {
void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void CheckSession(bool ok,const Session& session){if(!ok)throw std::runtime_error(session.Error());}
void Hr(HRESULT hr,const char* what){Check(SUCCEEDED(hr),what);}
constexpr unsigned full_size=256,proxy_size=64;
using Pixels=std::vector<std::uint8_t>;
const char shader[]=R"(
Texture2D<float4> original : register(t0);
Texture2D<float4> proxyIn : register(t1);
Texture2D<float4> proxyOut : register(t2);
RWTexture2D<float4> target : register(u0);
SamplerState linearClamp : register(s0);
cbuffer Params : register(b0){uint width,height,resolve,sourceSrgb;};
float3 encode(float3 c){return lerp(1.055*pow(max(c,0),1.0/2.4)-0.055,12.92*c,step(c,0.0031308));}
[numthreads(8,8,1)]void CS(uint3 id:SV_DispatchThreadID){
 if(id.x>=width||id.y>=height)return;
 float2 uv=(float2(id.xy)+0.5)/float2(width,height);
 float4 base=resolve!=0?original.Load(int3(id.xy,0)):original.SampleLevel(linearClamp,uv,0);
 if(sourceSrgb!=0)base.rgb=encode(base.rgb);
 if(resolve!=0)base.rgb=saturate(base.rgb+proxyOut.SampleLevel(linearClamp,uv,0).rgb-proxyIn.SampleLevel(linearClamp,uv,0).rgb);
 target[id.xy]=base;
})";
struct Gpu {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ComputeShader> compute;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11Buffer> constants;
    Gpu(){
        const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&context),"hardware D3D11 device unavailable");
        ComPtr<ID3DBlob> code,error;
        auto hr=D3DCompile(shader,sizeof(shader)-1,"Neural residual fixture",nullptr,nullptr,"CS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);
        if(FAILED(hr)&&error)std::cerr<<static_cast<const char*>(error->GetBufferPointer());Hr(hr,"compute shader failed");
        Hr(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&compute),"compute shader allocation failed");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        Hr(device->CreateSamplerState(&sd,&sampler),"sampler allocation failed");
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        Hr(device->CreateBuffer(&bd,nullptr,&constants),"constant allocation failed");
    }
    ComPtr<ID3D11Texture2D> Texture(unsigned size,const Pixels* bytes=nullptr,bool srgb=false){
        D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=size;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
        d.BindFlags=D3D11_BIND_SHADER_RESOURCE|(srgb?0:D3D11_BIND_UNORDERED_ACCESS);
        D3D11_SUBRESOURCE_DATA initial{};if(bytes){initial.pSysMem=bytes->data();initial.SysMemPitch=size*4;}
        ComPtr<ID3D11Texture2D> t;Hr(device->CreateTexture2D(&d,bytes?&initial:nullptr,&t),"texture allocation failed");return t;
    }
    Pixels Read(ID3D11Texture2D* texture){
        D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Hr(device->CreateTexture2D(&d,nullptr,&staging),"readback allocation failed");
        context->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE m{};
        // Blocking readback is deliberate in this isolated lab, not an admitted
        // render-thread transport or a frame-latency guarantee.
        Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m),"readback map failed");
        Pixels pixels(std::size_t(d.Width)*d.Height*4);
        for(unsigned y=0;y<d.Height;++y)std::memcpy(pixels.data()+std::size_t(y)*d.Width*4,static_cast<const std::uint8_t*>(m.pData)+std::size_t(y)*m.RowPitch,d.Width*4);
        context->Unmap(staging.Get(),0);return pixels;
    }
    ComPtr<ID3D11Texture2D> Run(ID3D11Texture2D* original,ID3D11Texture2D* input,ID3D11Texture2D* output,unsigned size,bool srgb){
        auto target=Texture(size);std::array<ComPtr<ID3D11ShaderResourceView>,3> views;
        ID3D11Texture2D* resources[]{original,input,output};
        for(unsigned i=0;i<3;++i)if(resources[i])Hr(device->CreateShaderResourceView(resources[i],nullptr,&views[i]),"source view failed");
        ComPtr<ID3D11UnorderedAccessView> write;Hr(device->CreateUnorderedAccessView(target.Get(),nullptr,&write),"output view failed");
        const unsigned parameters[]{size,size,input?1U:0U,srgb?1U:0U};context->UpdateSubresource(constants.Get(),0,nullptr,parameters,0,0);
        ID3D11ShaderResourceView* reads[]{views[0].Get(),views[1].Get(),views[2].Get()};auto* uav=write.Get();auto* cb=constants.Get();auto* ss=sampler.Get();
        context->CSSetShader(compute.Get(),nullptr,0);context->CSSetShaderResources(0,3,reads);context->CSSetUnorderedAccessViews(0,1,&uav,nullptr);
        context->CSSetSamplers(0,1,&ss);context->CSSetConstantBuffers(0,1,&cb);context->Dispatch((size+7)/8,(size+7)/8,1);
        ID3D11ShaderResourceView* empty[3]{};ID3D11UnorderedAccessView* no_write=nullptr;
        context->CSSetShaderResources(0,3,empty);context->CSSetUnorderedAccessViews(0,1,&no_write,nullptr);
        return target;
    }
};
Pixels Source(unsigned eye){
    Pixels p(full_size*full_size*4);for(unsigned y=0;y<full_size;++y)for(unsigned x=0;x<full_size;++x){auto i=(y*full_size+x)*4;
        p[i]=std::uint8_t(eye?170:30);p[i+1]=std::uint8_t(x%2?150:60);p[i+2]=std::uint8_t(eye?30:170);p[i+3]=std::uint8_t((x+y)%256);
    }return p;
}
}
int wmain(int argc,wchar_t** argv)try{
    if(argc<2){std::cerr<<"usage: neural_gpu_roundtrip <absolute-helper> [runtime-directory for real model]\n";return 2;}
    Gpu gpu;
    for(bool srgb:{false,true})for(const auto* mode:{L"identity",L"edit",L"model"}){
        const bool model=std::wstring(mode)==L"model";if(model&&argc<3)continue;
        Session session;const bool started=session.Start(argv[1],model?std::filesystem::path(argv[2]):std::filesystem::path{},mode);CheckSession(started,session);
        auto end=GetTickCount64()+12000;while(session.Poll()==State::starting&&GetTickCount64()<end)Sleep(1);
        CheckSession(session.Poll()==State::ready,session);
        std::array<ComPtr<ID3D11Texture2D>,2> originals,proxies,resolved;
        std::array<Pixels,2> sources,proxy_pixels;
        Pixels pair;
        for(unsigned eye=0;eye<2;++eye){sources[eye]=Source(eye);originals[eye]=gpu.Texture(full_size,&sources[eye],srgb);
            proxies[eye]=gpu.Run(originals[eye].Get(),nullptr,nullptr,proxy_size,srgb);proxy_pixels[eye]=gpu.Read(proxies[eye].Get());pair.insert(pair.end(),proxy_pixels[eye].begin(),proxy_pixels[eye].end());}
        Header h;h.width=h.height=proxy_size;h.request=1;h.input={1,1,1,1,1,1};
        // The first real-model evaluation may create/warm model features. Its
        // explicit lab budget is 5s; timeout does not publish old output.
        const auto begin=GetTickCount64();
        CheckSession(session.Process(h,pair,5000),session);
        for(unsigned eye=0;eye<2;++eye){const auto offset=eye*eye_bytes(proxy_size,proxy_size);
            Pixels processed(session.Output().begin()+offset,session.Output().begin()+offset+proxy_pixels[eye].size());
            auto output=gpu.Texture(proxy_size,&processed);resolved[eye]=gpu.Run(originals[eye].Get(),proxies[eye].Get(),output.Get(),full_size,srgb);}
        for(unsigned eye=0;eye<2;++eye){const auto actual=gpu.Read(resolved[eye].Get());
            for(std::size_t i=0;i<actual.size();++i){
                if(i%4==3)Check(actual[i]==sources[eye][i],"source alpha changed");
                else if(!model){const int expected=int(sources[eye][i])+(std::wstring(mode)==L"edit"?16:0);
                    Check(std::abs(int(actual[i])-expected)<=1,"residual lost detail, color space or eye identity");}
            }
            Check(gpu.Read(originals[eye].Get())==sources[eye],"native source was modified");
        }
        std::wcout<<L"Neural D3D11 x86/x64 "<<mode<<L";sRGB="<<srgb<<L";full=256;proxy=64;pair/alpha/original preserved;lab_wall_ms="<<GetTickCount64()-begin<<L'\n';
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
