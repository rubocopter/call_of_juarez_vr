#include "backends/openvr/current_color_resolve.hpp"
#include <d3dcompiler.h>
#include <cmath>
#include <cstdint>
#include <utility>

namespace cojvr::backends::openvr::dlss {
namespace {
constexpr auto kColorFormat=DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr char kShader[]=R"(
cbuffer Mapping : register(b0) { float2 inputSize; uint2 outputSize; float2 jitter; float2 pad; };
Texture2D<float4> reconstructed : register(t0);
Texture2D<float4> currentColor : register(t1);
Texture2D<float> currentMask : register(t2);
SamplerState linearClamp : register(s0);
RWTexture2D<float4> output : register(u0);

[numthreads(8,8,1)]
void Resolve(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= outputSize)) return;
    // Positive jitter shifts the rendered image right/down. Look for the
    // unjittered output position at that shifted input position in both planes.
    float2 source = (float2(id.xy)+0.5)*inputSize/float2(outputSize)+jitter;
    int2 nearest = clamp(int2(floor(source)),int2(0,0),int2(inputSize)-1);
    if (currentMask.Load(int3(nearest,0)) >= 1.0) {
        output[id.xy] = currentColor.SampleLevel(linearClamp,source/inputSize,0);
    } else {
        output[id.xy] = reconstructed.Load(int3(id.xy,0));
    }
})";
struct Mapping {
    float input[2];
    UINT output[2];
    float jitter[2],padding[2];
};
static_assert(sizeof(Mapping)==32);

bool ValidExtent(UINT width,UINT height) noexcept {
    return width&&height&&width<=8192&&height<=8192&&
        std::uint64_t(width)*height<=32ULL*1024*1024;
}

bool Describe(ID3D11Device* device,ID3D11Texture2D* texture,DXGI_FORMAT format,UINT bind,
              D3D11_TEXTURE2D_DESC& desc) noexcept {
    if(!device||!texture)return false;
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    texture->GetDevice(&owner);
    texture->GetDesc(&desc);
    return owner.Get()==device&&desc.Format==format&&(desc.BindFlags&bind)!=0&&
        desc.ArraySize==1&&desc.MipLevels==1&&desc.SampleDesc.Count==1&&ValidExtent(desc.Width,desc.Height);
}
}

bool CurrentColorResolve::Bind(ID3D11Device* device,ID3D11Texture2D* color,ID3D11Texture2D* mask,
    ID3D11Texture2D* output,Targets& targets) noexcept {
    targets={};
    D3D11_TEXTURE2D_DESC c{},m{},o{};
    if(color==output||color==scratch_.Get()||output==scratch_.Get()||
        !Describe(device,color,kColorFormat,D3D11_BIND_SHADER_RESOURCE,c)||
        !Describe(device,mask,DXGI_FORMAT_R8_UNORM,D3D11_BIND_SHADER_RESOURCE,m)||
        !Describe(device,output,kColorFormat,D3D11_BIND_UNORDERED_ACCESS,o)||
        c.Width!=m.Width||c.Height!=m.Height||c.Width>o.Width||c.Height>o.Height)return false;
    Targets next;
    if(FAILED(device->CreateShaderResourceView(color,nullptr,&next.color_))||
        FAILED(device->CreateShaderResourceView(mask,nullptr,&next.mask_))||
        FAILED(device->CreateUnorderedAccessView(output,nullptr,&next.output_)))return false;
    next.owner_=device;
    next.in_width_=c.Width;next.in_height_=c.Height;
    next.out_width_=o.Width;next.out_height_=o.Height;
    targets=std::move(next);
    return true;
}

ID3D11Texture2D* CurrentColorResolve::Prepare(ID3D11Device* device,UINT width,UINT height) noexcept {
    if(!device||!ValidExtent(width,height))return nullptr;
    if(owner_.Get()!=device) {
        *this={};
        ComPtr<ID3D11Device1> device1;
        ComPtr<ID3DBlob> bytecode;
        const auto feature=device->GetFeatureLevel();
        const auto flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?
            D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Mapping);buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxAnisotropy=1;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)))||
            FAILED(device1->CreateDeviceContextState(flags,&feature,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&state_))||
            FAILED(D3DCompile(kShader,sizeof(kShader)-1,"CoJVR current color resolve",nullptr,nullptr,"Resolve","cs_5_0",0,0,&bytecode,nullptr))||
            FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&shader_))||
            FAILED(device->CreateSamplerState(&sampler,&sampler_))||
            FAILED(device->CreateBuffer(&buffer,nullptr,&constants_)))return nullptr;
        owner_=device;
    }
    if(scratch_&&width==width_&&height==height_)return scratch_.Get();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=kColorFormat;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> scratch;
    ComPtr<ID3D11ShaderResourceView> reconstructed;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&scratch))||
        FAILED(device->CreateShaderResourceView(scratch.Get(),nullptr,&reconstructed)))return nullptr;
    scratch_=std::move(scratch);reconstructed_=std::move(reconstructed);
    width_=width;height_=height;
    return scratch_.Get();
}

bool CurrentColorResolve::Compose(ID3D11DeviceContext* context,const Targets& targets,
    float jitter_x,float jitter_y) noexcept {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!scratch_||
        targets.owner_.Get()!=owner_.Get()||width_!=targets.out_width_||height_!=targets.out_height_||
        !std::isfinite(jitter_x)||!std::isfinite(jitter_y)||std::abs(jitter_x)>.5F||std::abs(jitter_y)>.5F)return false;
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    ComPtr<ID3D11DeviceContext1> context1;
    if(device.Get()!=owner_.Get()||FAILED(context->QueryInterface(IID_PPV_ARGS(&context1))))return false;
    ComPtr<ID3DDeviceContextState> previous;
    context1->SwapDeviceContextState(state_.Get(),&previous);
    const Mapping mapping{{float(targets.in_width_),float(targets.in_height_)},{width_,height_},{jitter_x,jitter_y},{}};
    context->UpdateSubresource(constants_.Get(),0,nullptr,&mapping,0,0);
    auto* constants=constants_.Get();auto* sampler=sampler_.Get();auto* output=targets.output_.Get();
    ID3D11ShaderResourceView* inputs[]{reconstructed_.Get(),targets.color_.Get(),targets.mask_.Get()};
    context->CSSetShader(shader_.Get(),nullptr,0);
    context->CSSetShaderResources(0,3,inputs);
    context->CSSetUnorderedAccessViews(0,1,&output,nullptr);
    context->CSSetSamplers(0,1,&sampler);
    context->CSSetConstantBuffers(0,1,&constants);
    context->Dispatch((width_+7)/8,(height_+7)/8,1);
    context->ClearState();
    context1->SwapDeviceContextState(previous.Get(),nullptr);
    return SUCCEEDED(device->GetDeviceRemovedReason());
}
}
