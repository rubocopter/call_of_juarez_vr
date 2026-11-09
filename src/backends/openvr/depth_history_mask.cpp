#include "backends/openvr/depth_history_mask.hpp"
#include <d3dcompiler.h>
#include <cmath>

namespace cojvr::backends::openvr::dlss {
namespace {
constexpr char shader[]=R"(
cbuffer C:register(b0) {row_major float4x4 reproject;float4 current;float4 previous;float4 policy;};
Texture2D<float> depth:register(t0);Texture2D<float> oldDepth:register(t1);Texture2D<float> excluded:register(t2);
float4 VS(uint id:SV_VertexID):SV_Position {float2 uv=float2((id<<1)&2,id&2);return float4(uv.x*2-1,1-uv.y*2,0,1);}
float Axial(float z) {
    return policy.y>.5?previous.z/(1-z):previous.z*previous.w/(previous.w-(previous.w-previous.z)*z);
}
float Compare(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);float z=depth.Load(int3(p,0));
    if(!isfinite(z)||z<0||z>=1)return 1;
    float2 uv=(position.xy-current.zw)/current.xy;
    float4 q=mul(float4(uv.x*2-1,1-uv.y*2,z,1),reproject);
    if(!all(isfinite(q))||q.w<=1e-6||q.z<0||q.z>=q.w)return 1;
    float3 ndc=q.xyz/q.w;
    float2 oldPixel=float2(ndc.x*.5+.5,.5-ndc.y*.5)*current.xy+previous.xy;
    if(!all(isfinite(oldPixel))||any(oldPixel<0)||any(oldPixel>=current.xy))return 1;
    float oldZ=oldDepth.Load(int3(int2(floor(oldPixel)),0));
    if(!isfinite(oldZ)||oldZ<0||oldZ>=1)return 1;
    float predicted=Axial(ndc.z),measured=Axial(oldZ);
    if(!isfinite(predicted)||!isfinite(measured)||predicted<=0||measured<=0||
        abs(predicted-measured)>policy.x*measured)return 1;
    return saturate(excluded.Load(int3(p,0)));
}
// Max-filter the exclusion mask: uncertainty and out-of-image regions always
// choose current color. Partial native exclusions are preserved, not thresholded.
float Erode(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);float value=0;int radius=int(policy.z);
    for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x) {
        int2 q=p+int2(x,y);
        value=max(value,(any(q<0)||any(q>=int2(current.xy)))?1:depth.Load(int3(q,0)));
    }
    return value;
}
)";
struct Constants {runtime::TemporalMatrix matrix;float current[4],previous[4],policy[4];};
static_assert(sizeof(Constants)==112);
bool Adjacent(const runtime::TemporalFrameIdentity& c,const runtime::TemporalFrameIdentity& p,ID3D11Device* device) noexcept {
    return c.device==reinterpret_cast<std::uintptr_t>(device)&&c.device==p.device&&c.generation&&c.generation==p.generation&&
        c.owner&&c.owner==p.owner&&p.frame&&p.frame<c.frame&&c.frame-p.frame==1&&p.pose&&c.pose>p.pose&&
        c.width&&c.height&&c.width==p.width&&c.height==p.height&&c.width<=8192&&c.height<=8192&&
        std::uint64_t(c.width)*c.height<=16ULL*1024*1024&&
        c.output_width>=c.width&&c.output_height>=c.height&&c.output_width==p.output_width&&c.output_height==p.output_height&&
        c.quality==p.quality&&c.native_stereo&&p.native_stereo&&c.tracking_valid&&p.tracking_valid;
}
bool Jitter(float x,float y) noexcept {return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=.5F&&std::abs(y)<=.5F;}
}
void DepthHistoryMask::Reset() noexcept {
    outputs_={};raw_={};views_={};textures_={};constants_.Reset();vertex_.Reset();compare_.Reset();erode_.Reset();
    raster_.Reset();blend_.Reset();state_.Reset();owner_.Reset();width_=height_=0;
}
bool DepthHistoryMask::Prepare(ID3D11Device* device,unsigned width,unsigned height) noexcept {
    const auto check=[&](HRESULT result){result_=result;return SUCCEEDED(result);};
    if(owner_.Get()!=device||width_!=width||height_!=height){Reset();owner_=device;}
    if(state_&&erode_&&raw_[1]&&views_[1][1])return true;
    ComPtr<ID3D11Device1> device1;if(!check(device->QueryInterface(IID_PPV_ARGS(&device1))))return false;
    const auto feature=device->GetFeatureLevel();
    const auto flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    if(!state_&&!check(device1->CreateDeviceContextState(flags,&feature,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&state_)))return false;
    ComPtr<ID3DBlob> code;
    if(!check(D3DCompile(shader,sizeof(shader)-1,"CoJVR depth history mask",nullptr,nullptr,"VS","vs_4_0",0,0,&code,nullptr))||
        !check(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,vertex_.ReleaseAndGetAddressOf())))return false;
    for(unsigned pass=0;pass<2;++pass) {
        code.Reset();auto& target=pass?erode_:compare_;
        if(!check(D3DCompile(shader,sizeof(shader)-1,"CoJVR depth history mask",nullptr,nullptr,pass?"Erode":"Compare","ps_4_0",0,0,&code,nullptr))||
            !check(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,target.ReleaseAndGetAddressOf())))return false;
    }
    D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=sizeof(Constants);buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
    D3D11_BLEND_DESC blend{};blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(!check(device->CreateBuffer(&buffer,nullptr,constants_.ReleaseAndGetAddressOf()))||
        !check(device->CreateRasterizerState(&raster,raster_.ReleaseAndGetAddressOf()))||
        !check(device->CreateBlendState(&blend,blend_.ReleaseAndGetAddressOf())))return false;
    for(unsigned eye=0;eye<2;++eye)for(unsigned pass=0;pass<2;++pass) {
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.ArraySize=d.MipLevels=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(!check(device->CreateTexture2D(&d,nullptr,textures_[eye][pass].ReleaseAndGetAddressOf()))||
            !check(device->CreateRenderTargetView(textures_[eye][pass].Get(),nullptr,views_[eye][pass].ReleaseAndGetAddressOf())))return false;
        if(!pass&&!check(device->CreateShaderResourceView(textures_[eye][0].Get(),nullptr,raw_[eye].ReleaseAndGetAddressOf())))return false;
    }
    width_=width;height_=height;return true;
}
bool DepthHistoryMask::Generate(ID3D11Device* device,ID3D11DeviceContext* context,
    const PairIdentity& current_identity,const PairIdentity& previous_identity,
    const std::array<DepthHistoryMaskSource,2>& sources) noexcept {
    outputs_={};result_=E_INVALIDARG;
    const auto& current=current_identity.frame;const auto& previous=previous_identity.frame;
    if(!current_identity.settings_revision||!current_identity.history_epoch||!current_identity.input_context||
        current_identity.settings_revision!=previous_identity.settings_revision||
        current_identity.history_epoch!=previous_identity.history_epoch||
        current_identity.input_context!=previous_identity.input_context)return false;
    if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!Adjacent(current,previous,device))return false;
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);if(owner.Get()!=device)return false;
    ComPtr<ID3D11DeviceContext1> context1;if(FAILED(context->QueryInterface(IID_PPV_ARGS(&context1))))return false;
    std::array<std::array<ComPtr<ID3D11ShaderResourceView>,3>,2> inputs;
    for(unsigned eye=0;eye<2;++eye) {
        const auto& s=sources[eye];
        if(!s.source_qualified||!s.current_token||!s.previous_token||s.current_token==s.previous_token||s.depth==s.previous_depth||
            !Jitter(s.jitter_x,s.jitter_y)||!Jitter(s.previous_jitter_x,s.previous_jitter_y)||
            !std::isfinite(s.previous_near_metres)||!std::isfinite(s.previous_far_metres)||
            s.previous_near_metres<=0||s.previous_far_metres<=s.previous_near_metres||
            !std::isfinite(s.relative_tolerance)||s.relative_tolerance<0||s.relative_tolerance>1||s.erosion_radius>2)return false;
        runtime::TemporalMatrix inverse;
        if(!runtime::TemporalReprojection(s.clip_to_previous,runtime::TemporalIdentity(),inverse))return false;
        const std::array<ID3D11Texture2D*,3> textures{s.depth,s.previous_depth,s.exclusions};
        for(unsigned slot=0;slot<3;++slot) {
            auto* texture=textures[slot];if(!texture)return false;
            for(const auto& pair:textures_)for(const auto& own:pair)if(own.Get()==texture)return false;
            D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);owner.Reset();texture->GetDevice(&owner);
            if(owner.Get()!=device||d.Width!=current.width||d.Height!=current.height||d.ArraySize!=1||d.MipLevels!=1||
                d.SampleDesc.Count!=1||d.MiscFlags||!(d.BindFlags&D3D11_BIND_SHADER_RESOURCE)||
                d.Format!=(slot==2?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R32_FLOAT))return false;
            result_=device->CreateShaderResourceView(texture,nullptr,&inputs[eye][slot]);if(FAILED(result_))return false;
        }
    }
    if(!Prepare(device,current.width,current.height))return false;
    ComPtr<ID3DDeviceContextState> oldState;context1->SwapDeviceContextState(state_.Get(),&oldState);
    context->VSSetShader(vertex_.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetState(raster_.Get());context->OMSetBlendState(blend_.Get(),nullptr,UINT_MAX);
    const D3D11_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};context->RSSetViewports(1,&viewport);
    auto* buffer=constants_.Get();context->PSSetConstantBuffers(0,1,&buffer);
    for(unsigned eye=0;eye<2;++eye) {
        const auto& s=sources[eye];const Constants c{s.clip_to_previous,{float(width_),float(height_),s.jitter_x,s.jitter_y},
            {s.previous_jitter_x,s.previous_jitter_y,s.previous_near_metres,s.previous_far_metres},
            {s.relative_tolerance,s.previous_infinite_far?1.F:0.F,float(s.erosion_radius),0}};
        context->UpdateSubresource(buffer,0,nullptr,&c,0,0);
        for(unsigned pass=0;pass<2;++pass) {
            // Unbind the previous target before sampling it in the halo pass.
            context->OMSetRenderTargets(0,nullptr,nullptr);
            const std::array<ID3D11ShaderResourceView*,3> in{pass?raw_[eye].Get():inputs[eye][0].Get(),
                pass?nullptr:inputs[eye][1].Get(),pass?nullptr:inputs[eye][2].Get()};
            context->PSSetShaderResources(0,3,in.data());auto* out=views_[eye][pass].Get();
            context->OMSetRenderTargets(1,&out,nullptr);context->PSSetShader(pass?erode_.Get():compare_.Get(),nullptr,0);context->Draw(3,0);
        }
    }
    context->ClearState();context1->SwapDeviceContextState(oldState.Get(),nullptr);
    result_=device->GetDeviceRemovedReason();if(FAILED(result_))return false;
    for(unsigned eye=0;eye<2;++eye)outputs_[eye]=textures_[eye][1].Get();
    return true;
}
}
