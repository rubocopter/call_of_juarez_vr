#include "backends/openvr/spatial_image_processor.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace cojvr::backends::openvr {
namespace {
// Original five-tap spatial filter, not AMD CAS, FXAA, DLAA or DLSS. Work in the
// captured UNORM presentation encoding; neutral controls bypass the shader.
constexpr char shader[]=R"(
cbuffer Parameters : register(b0) {
    int2 extent; float sharpness; float contrast;
    float saturation; float inverseGamma; float smoothEdges; float peripheralDimming;
    float scopeView; float scopeReticle; float2 scopeCenter;
    float2 scopeRadius; float scopeWarning; float comfortInverseRadius;
    float4 comfortTangents; float4 comfortRotation;
};
Texture2D<float4> image : register(t0);
struct Vertex {float4 position:SV_Position;};
Vertex VS(uint id:SV_VertexID) {
    Vertex o;float2 uv=float2((id<<1)&2,id&2);
    o.position=float4(uv.x*2-1,1-uv.y*2,0,1);return o;
}
float4 Read(int2 p) {return image.Load(int3(clamp(p,int2(0,0),extent-1),0));}
float Luma(float3 color) {return dot(color,float3(.2126,.7152,.0722));}
float4 PS(Vertex v):SV_Target {
    int2 p=int2(v.position.xy);float4 original=Read(p);
    float3 c=original.rgb;
    if(sharpness>0||smoothEdges>0) {
        float3 n=Read(p+int2(0,-1)).rgb,s=Read(p+int2(0,1)).rgb;
        float3 e=Read(p+int2(1,0)).rgb,w=Read(p+int2(-1,0)).rgb;
        float lc=Luma(c),ln=Luma(n),ls=Luma(s),le=Luma(e),lw=Luma(w);
        float lo=min(lc,min(min(ln,ls),min(le,lw)));
        float hi=max(lc,max(max(ln,ls),max(le,lw)));
        float range=hi-lo;
        float gx=abs(le-lw),gy=abs(ln-ls);
        float edgeWeight=0;
        if(smoothEdges>0&&range>max(.03125,hi*.125))
            edgeWeight=.5*saturate(min(gx,gy)/max(max(gx,gy),.00001));
        float3 average=(n+s+e+w)*.25;
        c=lerp(c,average,edgeWeight);
        // Reduce gain on strong contrast and on edges already being smoothed.
        float gain=sharpness*.5*(1-saturate(range))*(1-edgeWeight);
        c=saturate(c+(c-average)*gain);
    }
    c=saturate((c-.5)*contrast+.5);
    c=saturate(lerp(Luma(c).xxx,c,saturation));
    if(inverseGamma!=1)c=pow(c,inverseGamma);
    // Keep the existing strengths and clear/feather fractions. Their reference
    // is now a common angular cone through physical eye optics, not equal UVs.
    float comfortProfile=saturate((peripheralDimming-.55)/.30);
    float clearRadius=lerp(.42,.28,comfortProfile);
    float outerRadius=lerp(.78,.60,comfortProfile);
    if(scopeView>0) {
        float2 delta=v.position.xy-scopeCenter*float2(extent);
        float2 lensPixels=scopeRadius*float2(extent);
        if(scopeReticle>0) {
            // An angular circle projects to this per-eye UV ellipse. The local
            // radial gradient converts its rim width to native pixels without
            // imposing texture aspect or a different angle on either eye.
            float2 q=delta/lensPixels;
            float radial=length(q);
            float gradient=length(q/lensPixels)/max(radial,.000001);
            float inset=(1-radial)/max(gradient,.000001);
            float aperture=smoothstep(0,1,inset);
            // Thin rim wholly inside the aperture, with a dark backing.
            float rim=abs(inset-2);
            float dark=1-smoothstep(1.25,1.75,rim);
            float white=1-smoothstep(.25,.75,rim);
            // Long fine hairs, an open centre gap and a separate centre dot.
            // Fixed pixel widths keep the sight crisp after all scene filters.
            float2 a=abs(delta);
            float horizontal=max(a.y,max(7-a.x,0));
            float vertical=max(a.x,max(7-a.y,0));
            float hair=min(horizontal,vertical);
            dark=max(dark,1-smoothstep(1.25,1.75,hair));
            white=max(white,1-smoothstep(.25,.75,hair));
            float dotRadius=length(delta);
            dark=max(dark,1-smoothstep(1.25,1.75,dotRadius));
            white=max(white,1-smoothstep(.5,1.0,dotRadius));
            c=lerp(c,0,dark);
            float3 sightColor=scopeWarning>0?float3(1,.25,.22):float3(1,1,1);
            c=lerp(c,sightColor,white)*aperture;
        } else {
            // Legacy null-lens fixtures retain their exact centered pixel mask.
            float apertureRadius=.42*min(extent.x,extent.y);
            float distance=length(delta);
            c*=1-smoothstep(apertureRadius-min(1.0,apertureRadius),apertureRadius,distance);
        }
    } else if(peripheralDimming>0) {
        float2 tangent=comfortTangents.xy+(v.position.xy/float2(extent))*comfortTangents.zw;
        float3 eyeRay=float3(tangent,-1);
        float3 t=2*cross(comfortRotation.xyz,eyeRay);
        float3 headRay=eyeRay+comfortRotation.w*t+cross(comfortRotation.xyz,t);
        float radius=headRay.z<0?length(headRay.xy)*comfortInverseRadius/(-headRay.z):2;
        c*=1-peripheralDimming*smoothstep(clearRadius,outerRadius,radius);
    }
    return float4(c,original.a);
}
)";
struct Parameters {
    std::int32_t width,height;
    float sharpness,contrast,saturation,inverse_gamma,smooth_edges,peripheral_dimming;
    float scope_view,scope_reticle,scope_center[2],scope_radius[2],scope_warning,comfort_inverse_radius;
    float comfort_tangents[4],comfort_rotation[4];
};
static_assert(sizeof(Parameters)==96);
struct ScopedContextState {
    ID3D11DeviceContext1* context;
    Microsoft::WRL::ComPtr<ID3DDeviceContextState> previous;
    ScopedContextState(ID3D11DeviceContext1* c,ID3DDeviceContextState* state) noexcept:context(c) {
        context->SwapDeviceContextState(state,&previous);
    }
    ~ScopedContextState() {context->ClearState();context->SwapDeviceContextState(previous.Get(),nullptr);}
};
}

void SpatialImageProcessor::Reset() noexcept {
    for(auto& v:views_)v.Reset();for(auto& t:targets_)t.Reset();
    source_.Reset();scratch_.Reset();vertex_.Reset();pixel_.Reset();constants_.Reset();
    raster_.Reset();depth_.Reset();blend_.Reset();state_.Reset();owner_.Reset();
    width_=height_=allocations_=0;format_=DXGI_FORMAT_UNKNOWN;result_=S_OK;queue_ms_=0;
}

bool SpatialImageProcessor::Prepare(ID3D11Device* device,
    const std::array<ID3D11Texture2D*,2>& eyes,const D3D11_TEXTURE2D_DESC& desc) noexcept {
    if(owner_.Get()!=device) {Reset();owner_=device;}
    if(!state_||!vertex_||!pixel_||!constants_||!raster_||!depth_||!blend_) {
        ComPtr<ID3D11Device1> device1;
        if(!Check(device->QueryInterface(IID_PPV_ARGS(&device1))))return false;
        const auto feature=device->GetFeatureLevel();
        const UINT flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)
            ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
        if(!state_&&!Check(device1->CreateDeviceContextState(flags,&feature,1,D3D11_SDK_VERSION,
                __uuidof(ID3D11Device),nullptr,&state_)))return false;
        ComPtr<ID3DBlob> v,p;
        if(!Check(D3DCompile(shader,sizeof(shader)-1,"CoJVR spatial image",nullptr,nullptr,
            "VS","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,nullptr))||
           !Check(D3DCompile(shader,sizeof(shader)-1,"CoJVR spatial image",nullptr,nullptr,
            "PS","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,nullptr))||
           !Check(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,vertex_.ReleaseAndGetAddressOf()))||
           !Check(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,pixel_.ReleaseAndGetAddressOf())))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Parameters);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;dd.DepthFunc=D3D11_COMPARISON_ALWAYS;
        D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(!Check(device->CreateBuffer(&cb,nullptr,constants_.ReleaseAndGetAddressOf()))||
           !Check(device->CreateRasterizerState(&rd,raster_.ReleaseAndGetAddressOf()))||
           !Check(device->CreateDepthStencilState(&dd,depth_.ReleaseAndGetAddressOf()))||
           !Check(device->CreateBlendState(&bd,blend_.ReleaseAndGetAddressOf())))return false;
    }
    // Prepare the complete pair and scratch before modifying either eye.
    std::array<ComPtr<ID3D11RenderTargetView>,2> next_views;
    for(unsigned eye=0;eye<2;++eye)if(targets_[eye].Get()!=eyes[eye]||!views_[eye])
        if(!Check(device->CreateRenderTargetView(eyes[eye],nullptr,&next_views[eye])))return false;
    if(!scratch_||width_!=desc.Width||height_!=desc.Height||format_!=desc.Format) {
        auto sd=desc;sd.BindFlags=D3D11_BIND_SHADER_RESOURCE;sd.MiscFlags=sd.CPUAccessFlags=0;
        ComPtr<ID3D11Texture2D> scratch;ComPtr<ID3D11ShaderResourceView> view;
        if(!Check(device->CreateTexture2D(&sd,nullptr,&scratch))||
           !Check(device->CreateShaderResourceView(scratch.Get(),nullptr,&view)))return false;
        scratch_=std::move(scratch);source_=std::move(view);
        width_=desc.Width;height_=desc.Height;format_=desc.Format;++allocations_;
    }
    for(unsigned eye=0;eye<2;++eye)if(next_views[eye]) {
        targets_[eye]=eyes[eye];views_[eye]=std::move(next_views[eye]);
    }
    return true;
}

bool SpatialImageProcessor::Process(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,const runtime::GraphicsSettings& requested,
    float peripheral_dimming,bool scope_view,const std::array<runtime::ScopeLens,2>* scope_lenses,bool scope_no_shoot,
    const std::array<runtime::EyeView,2>* comfort_eyes) noexcept {
    result_=E_INVALIDARG;queue_ms_=0;
    if(!std::isfinite(peripheral_dimming)||peripheral_dimming<0||peripheral_dimming>1)return false;
    if(!runtime::ValidGraphicsSettings(requested))return false;
    // Validate the complete optical pair before resources, context swaps or any
    // eye writes. An absent lens never borrows the other eye's geometry.
    if(scope_view&&scope_lenses)
        for(const auto& lens:*scope_lenses)if(!runtime::ValidScopeLens(lens))return false;
    std::array<runtime::ComfortEyeProjection,2> comfort{};
    if(peripheral_dimming>0&&!scope_view&&
        (!comfort_eyes||!runtime::BuildComfortEyeProjections(*comfort_eyes,comfort)))return false;
    const auto settings=requested.enabled?requested:runtime::GraphicsSettings{};
    if(!runtime::ValidGraphicsSettings(settings))return false;
    if(!runtime::GraphicsEffectsActive(settings)&&peripheral_dimming==0&&!scope_view){result_=S_OK;return true;}
    if(!device||!context||!eyes[0]||!eyes[1]||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    ComPtr<ID3D11Device> context_device;context->GetDevice(&context_device);
    if(context_device.Get()!=device)return false;
    ComPtr<ID3D11DeviceContext1> context1;
    if(!Check(context->QueryInterface(IID_PPV_ARGS(&context1))))return false;
    std::array<D3D11_TEXTURE2D_DESC,2> desc{};
    std::array<ComPtr<IUnknown>,2> identities;
    for(unsigned eye=0;eye<2;++eye) {
        ComPtr<ID3D11Device> owner;eyes[eye]->GetDevice(&owner);eyes[eye]->GetDesc(&desc[eye]);
        if(!Check(eyes[eye]->QueryInterface(IID_PPV_ARGS(&identities[eye]))))return false;
        const auto& d=desc[eye];
        if(owner.Get()!=device||!d.Width||!d.Height||d.Width>16384||d.Height>16384||
           std::uint64_t(d.Width)*d.Height*4>128ULL*1024*1024||
           d.ArraySize!=1||d.MipLevels!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality!=0||d.MiscFlags!=0||
           d.Usage!=D3D11_USAGE_DEFAULT||!(d.BindFlags&D3D11_BIND_RENDER_TARGET)||
           (d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8X8_UNORM)) {
            result_=E_INVALIDARG;return false;
        }
    }
    if(identities[0].Get()==identities[1].Get()||desc[0].Width!=desc[1].Width||
       desc[0].Height!=desc[1].Height||desc[0].Format!=desc[1].Format) {result_=E_INVALIDARG;return false;}
    if(!Prepare(device,eyes,desc[0]))return false;
    if(!Check(device->GetDeviceRemovedReason()))return false;
    const auto begin=std::chrono::steady_clock::now();
    {
        // Clean private state avoids input/output alias unbinding in the caller;
        // restoring the state also restores all shader slots/class instances.
        ScopedContextState scope(context1.Get(),state_.Get());
        Parameters parameters{static_cast<std::int32_t>(width_),static_cast<std::int32_t>(height_),
            settings.sharpness,settings.contrast,settings.saturation,1.F/settings.gamma,settings.edge_smoothing?1.F:0.F,
            peripheral_dimming,scope_view?1.F:0.F,scope_view&&scope_lenses?1.F:0.F,{.5F,.5F},{},scope_no_shoot?1.F:0.F,0,
            {},{0,0,0,1}};
        auto* constants=constants_.Get();
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_.Get(),nullptr,0);context->PSSetShader(pixel_.Get(),nullptr,0);
        context->PSSetConstantBuffers(0,1,&constants);context->RSSetState(raster_.Get());
        context->OMSetDepthStencilState(depth_.Get(),0);context->OMSetBlendState(blend_.Get(),nullptr,0xFFFFFFFF);
        const D3D11_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};context->RSSetViewports(1,&viewport);
        for(unsigned eye=0;eye<2;++eye) {
            if(peripheral_dimming>0&&!scope_view) {
                const auto& c=comfort[eye];
                parameters.comfort_inverse_radius=c.inverse_radius;
                std::copy(c.tangents.begin(),c.tangents.end(),parameters.comfort_tangents);
                parameters.comfort_rotation[0]=c.eye_to_head.x;parameters.comfort_rotation[1]=c.eye_to_head.y;
                parameters.comfort_rotation[2]=c.eye_to_head.z;parameters.comfort_rotation[3]=c.eye_to_head.w;
            }
            if(scope_view&&scope_lenses) {
                const auto& lens=(*scope_lenses)[eye];
                parameters.scope_center[0]=lens.center_u;parameters.scope_center[1]=lens.center_v;
                parameters.scope_radius[0]=lens.radius_u;parameters.scope_radius[1]=lens.radius_v;
            }
            context->UpdateSubresource(constants_.Get(),0,nullptr,&parameters,0,0);
            context->CopyResource(scratch_.Get(),eyes[eye]);
            auto* view=views_[eye].Get();auto* source=source_.Get();
            context->OMSetRenderTargets(1,&view,nullptr);context->PSSetShaderResources(0,1,&source);context->Draw(3,0);
            ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
            context->OMSetRenderTargets(0,nullptr,nullptr);
        }
    }
    queue_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    return Check(device->GetDeviceRemovedReason());
}
}
