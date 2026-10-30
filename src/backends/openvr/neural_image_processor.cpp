#include "backends/openvr/neural_image_processor.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <new>

namespace cojvr::backends::openvr::neural {
namespace {
template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
constexpr unsigned max_extent=1280,min_extent=256;
constexpr char shader[]=R"(
cbuffer Parameters : register(b0) {uint width,height,resolve,sourceSrgb;};
Texture2D<float4> original : register(t0);
Texture2D<float4> proxyIn : register(t1);
Texture2D<float4> proxyOut : register(t2);
SamplerState linearClamp : register(s0);
struct Vertex {float4 position:SV_Position;};
Vertex VS(uint id:SV_VertexID) {
    Vertex v;float2 uv=float2((id<<1)&2,id&2);
    v.position=float4(uv.x*2-1,1-uv.y*2,0,1);return v;
}
float3 Encode(float3 c) {
    return lerp(1.055*pow(max(c,0),1.0/2.4)-0.055,12.92*c,step(c,0.0031308));
}
float3 Decode(float3 c) {
    return lerp(pow(max((c+0.055)/1.055,0),2.4),c/12.92,step(c,0.04045));
}
float4 PS(Vertex v):SV_Target {
    float2 uv=v.position.xy/float2(width,height);
    float4 c=resolve!=0?original.Load(int3(int2(v.position.xy),0)):
        original.SampleLevel(linearClamp,uv,0);
    // Typed sRGB SRVs decode automatically. Model bytes and residual are encoded.
    if(sourceSrgb!=0)c.rgb=Encode(c.rgb);
    if(resolve!=0) {
        c.rgb=saturate(c.rgb+proxyOut.SampleLevel(linearClamp,uv,0).rgb-
                              proxyIn.SampleLevel(linearClamp,uv,0).rgb);
        // The typed sRGB destination RTV encodes once; alpha is never transformed.
        if(sourceSrgb!=0)c.rgb=Decode(c.rgb);
    }
    return c;
}
)";
bool Srgb(DXGI_FORMAT f) noexcept {
    return f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}
bool Known(DXGI_FORMAT f) noexcept {
    return f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM||
        f==DXGI_FORMAT_B8G8R8X8_UNORM||Srgb(f);
}
HRESULT ValidateContext(ID3D11Device* device,ID3D11DeviceContext* context,
                        ComPtr<ID3D11DeviceContext1>& context1) noexcept {
    if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return E_INVALIDARG;
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);
    if(owner.Get()!=device)return E_INVALIDARG;
    const auto hr=context->QueryInterface(IID_PPV_ARGS(&context1));
    return FAILED(hr)?hr:device->GetDeviceRemovedReason();
}
HRESULT Validate(ID3D11Device* device,ID3D11DeviceContext* context,
                 const std::array<ID3D11Texture2D*,2>& eyes,D3D11_TEXTURE2D_DESC& desc,
                 ComPtr<ID3D11DeviceContext1>& context1) noexcept {
    const auto hr=ValidateContext(device,context,context1);if(FAILED(hr))return hr;
    if(!eyes[0]||!eyes[1])return E_INVALIDARG;
    ComPtr<ID3D11Device> owner;
    std::array<ComPtr<IUnknown>,2> identities;
    std::array<D3D11_TEXTURE2D_DESC,2> descriptions{};
    for(unsigned eye=0;eye<2;++eye) {
        eyes[eye]->GetDevice(owner.ReleaseAndGetAddressOf());eyes[eye]->GetDesc(&descriptions[eye]);
        const auto& d=descriptions[eye];
        constexpr UINT sharing=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        if(owner.Get()!=device||!d.Width||!d.Height||d.Width>16384||d.Height>16384||
           std::uint64_t(d.Width)*d.Height*4>128ULL*1024*1024||!Known(d.Format)||
           d.MipLevels!=1||d.ArraySize!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality!=0||
           d.Usage!=D3D11_USAGE_DEFAULT||d.CPUAccessFlags!=0||!(d.BindFlags&D3D11_BIND_RENDER_TARGET)||
           (d.MiscFlags&~sharing)!=0)return E_INVALIDARG;
        const auto identity_hr=eyes[eye]->QueryInterface(IID_PPV_ARGS(&identities[eye]));if(FAILED(identity_hr))return identity_hr;
    }
    if(identities[0]==identities[1]||descriptions[0].Width!=descriptions[1].Width||
       descriptions[0].Height!=descriptions[1].Height||descriptions[0].Format!=descriptions[1].Format)return E_INVALIDARG;
    desc=descriptions[0];
    return S_OK;
}
bool Stage(const D3D11_TEXTURE2D_DESC& d,unsigned requested,unsigned& width,unsigned& height) noexcept {
    if(requested<min_extent||requested>max_extent)return false;
    height=std::min(requested,d.Height);
    width=static_cast<unsigned>(std::llround(double(height)*d.Width/d.Height));
    if(width>max_extent) {
        width=max_extent;height=static_cast<unsigned>(std::llround(double(width)*d.Height/d.Width));
    }
    return width>=min_extent&&height>=min_extent&&width<=max_extent&&height<=max_extent;
}
bool Layout(const D3D11_TEXTURE2D_DESC& d,unsigned w,unsigned h,
            std::span<const std::uint8_t> input,std::span<const std::uint8_t> output) noexcept {
    if(w<min_extent||h<min_extent||w>max_extent||h>max_extent||w>d.Width||h>d.Height||
       input.size()!=std::size_t(w)*h*8||output.size()!=input.size())return false;
    // Either axis can have been rounded after capping width (1960x400 ->1280x261).
    return std::abs(double(w)*d.Height-double(h)*d.Width)<=.5*std::max(d.Width,d.Height);
}
struct ScopedContextState {
    ID3D11DeviceContext1* context;
    ComPtr<ID3DDeviceContextState> previous;
    ScopedContextState(ID3D11DeviceContext1* c,ID3DDeviceContextState* state) noexcept:context(c) {
        context->SwapDeviceContextState(state,&previous);
    }
    ~ScopedContextState() {context->ClearState();context->SwapDeviceContextState(previous.Get(),nullptr);}
};
struct MappedPair {
    ID3D11DeviceContext* context;
    std::array<ComPtr<ID3D11Texture2D>,2>& textures;
    std::array<D3D11_MAPPED_SUBRESOURCE,2> mapped{};
    unsigned count=0;
    ~MappedPair() {for(unsigned i=0;i<count;++i)context->Unmap(textures[i].Get(),0);}
};
}

struct ImageProcessor::Resources {
    enum class Phase {idle,capture,captured,resolve};
    Phase phase=Phase::idle;
    bool cancelled=false;
    ComPtr<ID3D11DeviceContext> submission_context;
    std::array<ComPtr<ID3D11Texture2D>,2> destinations;
    std::array<ComPtr<IUnknown>,2> destination_ids;
    ComPtr<ID3D11Device> owner;
    ComPtr<ID3DDeviceContextState> state;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11Query> ready;
    std::array<ComPtr<ID3D11Texture2D>,2> originals,targets,proxies,staging,uploads;
    std::array<ComPtr<ID3D11ShaderResourceView>,2> original_views,upload_views;
    std::array<ComPtr<ID3D11RenderTargetView>,2> target_views,proxy_views;
    unsigned full_width=0,full_height=0,stage_width=0,stage_height=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    std::vector<std::uint8_t> bytes;

    void Finish() noexcept {
        phase=Phase::idle;cancelled=false;submission_context.Reset();
        for(auto& d:destinations)d.Reset();for(auto& id:destination_ids)id.Reset();
    }

    HRESULT Initialize(ID3D11Device* device) noexcept {
        owner=device;HRESULT hr;
        ComPtr<ID3D11Device1> device1;
        if(FAILED(hr=device->QueryInterface(IID_PPV_ARGS(&device1))))return hr;
        const auto feature=device->GetFeatureLevel();
        const UINT flags=(device->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)
            ?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
        if(FAILED(hr=device1->CreateDeviceContextState(flags,&feature,1,D3D11_SDK_VERSION,
            __uuidof(ID3D11Device),nullptr,&state)))return hr;
        ComPtr<ID3DBlob> v,p;
        if(FAILED(hr=D3DCompile(shader,sizeof(shader)-1,"CoJVR neural proxy",nullptr,nullptr,"VS","vs_4_0",
                D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,nullptr))||
           FAILED(hr=D3DCompile(shader,sizeof(shader)-1,"CoJVR neural residual",nullptr,nullptr,"PS","ps_4_0",
                D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,nullptr))||
           FAILED(hr=device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vertex))||
           FAILED(hr=device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&pixel)))return hr;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;dd.DepthFunc=D3D11_COMPARISON_ALWAYS;
        D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT,0};
        if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&constants))||
           FAILED(hr=device->CreateSamplerState(&sd,&sampler))||
           FAILED(hr=device->CreateRasterizerState(&rd,&raster))||
           FAILED(hr=device->CreateDepthStencilState(&dd,&depth))||
           FAILED(hr=device->CreateBlendState(&bd,&blend))||
           FAILED(hr=device->CreateQuery(&qd,&ready)))return hr;
        return S_OK;
    }
    HRESULT Prepare(const D3D11_TEXTURE2D_DESC& full,unsigned w,unsigned h) {
        HRESULT hr;
        if(full_width!=full.Width||full_height!=full.Height||format!=full.Format) {
            // Private-only allocation may be retried after failure. Publish keys last.
            full_width=full_height=0;format=DXGI_FORMAT_UNKNOWN;
            auto d=full;d.MiscFlags=d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            for(unsigned eye=0;eye<2;++eye) {
                if(FAILED(hr=owner->CreateTexture2D(&d,nullptr,originals[eye].ReleaseAndGetAddressOf()))||
                   FAILED(hr=owner->CreateShaderResourceView(originals[eye].Get(),nullptr,original_views[eye].ReleaseAndGetAddressOf())))return hr;
                auto target=d;target.BindFlags=D3D11_BIND_RENDER_TARGET;
                if(FAILED(hr=owner->CreateTexture2D(&target,nullptr,targets[eye].ReleaseAndGetAddressOf()))||
                   FAILED(hr=owner->CreateRenderTargetView(targets[eye].Get(),nullptr,target_views[eye].ReleaseAndGetAddressOf())))return hr;
            }
            full_width=full.Width;full_height=full.Height;format=full.Format;
        }
        if(stage_width!=w||stage_height!=h) {
            stage_width=stage_height=0;
            D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
            d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
            for(unsigned eye=0;eye<2;++eye) {
                if(FAILED(hr=owner->CreateTexture2D(&d,nullptr,proxies[eye].ReleaseAndGetAddressOf()))||
                   FAILED(hr=owner->CreateRenderTargetView(proxies[eye].Get(),nullptr,proxy_views[eye].ReleaseAndGetAddressOf())))return hr;
                auto read=d;read.BindFlags=0;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                if(FAILED(hr=owner->CreateTexture2D(&read,nullptr,staging[eye].ReleaseAndGetAddressOf())))return hr;
                auto upload=d;upload.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                if(FAILED(hr=owner->CreateTexture2D(&upload,nullptr,uploads[eye].ReleaseAndGetAddressOf()))||
                   FAILED(hr=owner->CreateShaderResourceView(uploads[eye].Get(),nullptr,upload_views[eye].ReleaseAndGetAddressOf())))return hr;
            }
            bytes.resize(std::size_t(w)*h*8);stage_width=w;stage_height=h;
        } else bytes.resize(std::size_t(w)*h*8);
        return S_OK;
    }
    void Bind(ID3D11DeviceContext* context,unsigned width,unsigned height,bool resolve) noexcept {
        const unsigned parameters[]{width,height,resolve?1U:0U,Srgb(format)?1U:0U};
        context->UpdateSubresource(constants.Get(),0,nullptr,parameters,0,0);
        auto* cb=constants.Get();auto* ss=sampler.Get();
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);
        context->PSSetConstantBuffers(0,1,&cb);context->PSSetSamplers(0,1,&ss);
        context->RSSetState(raster.Get());context->OMSetDepthStencilState(depth.Get(),0);
        context->OMSetBlendState(blend.Get(),nullptr,0xFFFFFFFF);
        const D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
    }
    static void Unbind(ID3D11DeviceContext* context) noexcept {
        ID3D11ShaderResourceView* empty[3]{};context->PSSetShaderResources(0,3,empty);
        context->OMSetRenderTargets(0,nullptr,nullptr);
    }
};
ImageProcessor::ImageProcessor() noexcept=default;
ImageProcessor::~ImageProcessor()=default;

HRESULT ImageProcessor::Ready(ID3D11DeviceContext* context,ID3D11Query* query) noexcept {
    if(readiness_probe_) {
        const auto hr=readiness_probe_(context,query);
        return hr==S_FALSE?E_PENDING:hr;
    }
    BOOL done=FALSE;
    const auto hr=context->GetData(query,&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(FAILED(hr))return hr;
    return hr==S_OK&&done?S_OK:E_PENDING;
}
bool ImageProcessor::busy() const noexcept {
    return resources_&&resources_->phase!=Resources::Phase::idle;
}
void ImageProcessor::Discard() noexcept {
    if(!busy())return;
    auto& r=*resources_;
    if(r.phase==Resources::Phase::captured)r.Finish(); // Capture query already proved complete.
    else r.cancelled=true; // Keep the query and every referenced resource until drain/reset.
}

bool ImageProcessor::BeginCapture(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,unsigned requested_height,
    unsigned& stage_width,unsigned& stage_height) noexcept try {
    if(busy()){result_=E_PENDING;return false;}
    readback_ms_=0;ComPtr<ID3D11DeviceContext1> context1;D3D11_TEXTURE2D_DESC d{};
    result_=Validate(device,context,eyes,d,context1);if(FAILED(result_))return false;
    unsigned w,h;if(!Stage(d,requested_height,w,h)){result_=E_INVALIDARG;return false;}
    if(!resources_||resources_->owner.Get()!=device) {
        auto next=std::make_unique<Resources>();result_=next->Initialize(device);
        if(FAILED(result_))return false;resources_=std::move(next);
    }
    auto& r=*resources_;result_=r.Prepare(d,w,h);if(FAILED(result_))return false;
    r.submission_context=context;r.cancelled=false;r.phase=Resources::Phase::capture;
    {
        ScopedContextState state(context1.Get(),r.state.Get());r.Bind(context,w,h,false);
        for(unsigned eye=0;eye<2;++eye) {
            context->CopyResource(r.originals[eye].Get(),eyes[eye]);
            auto* srv=r.original_views[eye].Get();auto* rtv=r.proxy_views[eye].Get();
            context->PSSetShaderResources(0,1,&srv);context->OMSetRenderTargets(1,&rtv,nullptr);
            context->Draw(3,0);r.Unbind(context);
            context->CopyResource(r.staging[eye].Get(),r.proxies[eye].Get());
        }
        context->End(r.ready.Get());
    }
    context->Flush(); // Submit once. Subsequent polls never restart or flush this work.
    stage_width=w;stage_height=h;result_=S_OK;return true;
}catch(const std::bad_alloc&) {result_=E_OUTOFMEMORY;return false;}
catch(...) {result_=E_FAIL;return false;}

bool ImageProcessor::PollCapture(ID3D11Device* device,ID3D11DeviceContext* context,
    std::vector<std::uint8_t>& pair) noexcept {
    readback_ms_=0;
    if(!busy()){result_=E_UNEXPECTED;return false;}
    auto& r=*resources_;
    if(r.phase==Resources::Phase::captured){result_=S_FALSE;return false;}
    // PollCapture is also the universal cancellation drain, including resolve.
    if(r.phase!=Resources::Phase::capture&&!r.cancelled){result_=E_UNEXPECTED;return false;}
    if(device!=r.owner.Get()||context!=r.submission_context.Get()){result_=E_INVALIDARG;return false;}
    ComPtr<ID3D11DeviceContext1> context1;
    result_=ValidateContext(device,context,context1);if(FAILED(result_))return false;
    const auto begin=std::chrono::steady_clock::now();
    struct Timer {
        double& ms;std::chrono::steady_clock::time_point begin;
        ~Timer(){ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}
    } timer{readback_ms_,begin};
    result_=Ready(context,r.ready.Get());if(result_!=S_OK)return false;
    if(r.cancelled){r.Finish();result_=E_ABORT;return false;}
    MappedPair maps{context,r.staging};
    for(unsigned eye=0;eye<2;++eye) {
        result_=context->Map(r.staging[eye].Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&maps.mapped[eye]);
        if(result_==DXGI_ERROR_WAS_STILL_DRAWING){result_=E_PENDING;return false;}
        if(FAILED(result_))return false;++maps.count;
    }
    result_=device->GetDeviceRemovedReason();if(FAILED(result_))return false;
    const auto row_bytes=std::size_t(r.stage_width)*4,eye_bytes=row_bytes*r.stage_height;
    for(unsigned eye=0;eye<2;++eye)for(unsigned y=0;y<r.stage_height;++y)
        std::memcpy(r.bytes.data()+eye*eye_bytes+y*row_bytes,
            static_cast<const std::uint8_t*>(maps.mapped[eye].pData)+std::size_t(y)*maps.mapped[eye].RowPitch,row_bytes);
    pair.swap(r.bytes);r.phase=Resources::Phase::captured;result_=S_OK;return true;
}

bool ImageProcessor::BeginResolve(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& destinations,std::span<const std::uint8_t> input,
    std::span<const std::uint8_t> output,unsigned w,unsigned h) noexcept {
    if(!busy()){result_=E_UNEXPECTED;return false;}
    auto& r=*resources_;
    if(r.phase!=Resources::Phase::captured||r.cancelled){result_=E_PENDING;return false;}
    if(device!=r.owner.Get()||context!=r.submission_context.Get()){result_=E_INVALIDARG;return false;}
    ComPtr<ID3D11DeviceContext1> context1;D3D11_TEXTURE2D_DESC d{};
    result_=Validate(device,context,destinations,d,context1);if(FAILED(result_))return false;
    if(d.Width!=r.full_width||d.Height!=r.full_height||d.Format!=r.format||
       w!=r.stage_width||h!=r.stage_height||!Layout(d,w,h,input,output)){result_=E_INVALIDARG;return false;}
    // Acquire both destination identities before submitting anything. Originals,
    // textures, stage extents and CPU pair all belong to the retained capture.
    std::array<ComPtr<IUnknown>,2> identities;
    for(unsigned eye=0;eye<2;++eye) {
        result_=destinations[eye]->QueryInterface(IID_PPV_ARGS(&identities[eye]));if(FAILED(result_))return false;
    }
    for(unsigned eye=0;eye<2;++eye){r.destinations[eye]=destinations[eye];r.destination_ids[eye]=std::move(identities[eye]);}
    r.phase=Resources::Phase::resolve;
    {
        ScopedContextState state(context1.Get(),r.state.Get());r.Bind(context,d.Width,d.Height,true);
        const auto eye_bytes=std::size_t(w)*h*4;
        for(unsigned eye=0;eye<2;++eye) {
            // Never recopy destinations here: they can already contain later HUD
            // or other changes. Residual and alpha refer to BeginCapture only.
            context->UpdateSubresource(r.uploads[0].Get(),0,nullptr,input.data()+eye*eye_bytes,w*4,0);
            context->UpdateSubresource(r.uploads[1].Get(),0,nullptr,output.data()+eye*eye_bytes,w*4,0);
            ID3D11ShaderResourceView* srvs[]{r.original_views[eye].Get(),r.upload_views[0].Get(),r.upload_views[1].Get()};
            auto* rtv=r.target_views[eye].Get();context->PSSetShaderResources(0,3,srvs);
            context->OMSetRenderTargets(1,&rtv,nullptr);context->Draw(3,0);r.Unbind(context);
        }
        context->End(r.ready.Get());
    }
    context->Flush();result_=S_OK;return true;
}

bool ImageProcessor::PollResolve(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& destinations) noexcept {
    if(!busy()||resources_->phase!=Resources::Phase::resolve){result_=E_UNEXPECTED;return false;}
    auto& r=*resources_;
    if(device!=r.owner.Get()||context!=r.submission_context.Get()){result_=E_INVALIDARG;return false;}
    ComPtr<ID3D11DeviceContext1> context1;
    result_=ValidateContext(device,context,context1);if(FAILED(result_))return false;
    if(!r.cancelled) {
        D3D11_TEXTURE2D_DESC d{};
        result_=Validate(device,context,destinations,d,context1);if(FAILED(result_))return false;
        if(d.Width!=r.full_width||d.Height!=r.full_height||d.Format!=r.format){result_=E_INVALIDARG;return false;}
        for(unsigned eye=0;eye<2;++eye) {
            ComPtr<IUnknown> id;
            result_=destinations[eye]->QueryInterface(IID_PPV_ARGS(&id));if(FAILED(result_))return false;
            if(id!=r.destination_ids[eye]){result_=E_INVALIDARG;return false;}
        }
    }
    result_=Ready(context,r.ready.Get());if(result_!=S_OK)return false;
    if(r.cancelled){r.Finish();result_=E_ABORT;return false;}
    result_=device->GetDeviceRemovedReason();if(FAILED(result_))return false;
    {
        ScopedContextState state(context1.Get(),r.state.Get());
        // Both private outputs are complete and the pair identity is revalidated.
        // D3D11 cannot make two void CopyResource calls device-loss atomic.
        context->CopyResource(destinations[0],r.targets[0].Get());
        context->CopyResource(destinations[1],r.targets[1].Get());
    }
    r.Finish();result_=S_OK;return true;
}

bool ImageProcessor::Capture(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,unsigned requested_height,
    std::vector<std::uint8_t>& pair,unsigned& stage_width,unsigned& stage_height_out) noexcept {
    // Legacy synchronous-budget probe only. It never publishes a cancelled old
    // frame on a newer call. The runtime uses the retained Begin/Poll lifecycle.
    if(busy()) {
        auto& r=*resources_;
        if(r.phase==Resources::Phase::captured)Discard();
        else if(r.cancelled){std::vector<std::uint8_t> ignored;(void)PollCapture(device,context,ignored);}
        if(busy()){result_=E_PENDING;return false;}
    }
    unsigned w=0,h=0;if(!BeginCapture(device,context,eyes,requested_height,w,h))return false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(2);
    do {
        if(PollCapture(device,context,pair)){stage_width=w;stage_height_out=h;return true;}
        if(result_!=E_PENDING){Discard();return false;}
    }while(std::chrono::steady_clock::now()<deadline);
    Discard();result_=E_PENDING;return false;
}

bool ImageProcessor::Resolve(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,std::span<const std::uint8_t> input,
    std::span<const std::uint8_t> output,unsigned w,unsigned h) noexcept {
    // Preserve validation-first behavior of the original standalone fixture.
    ComPtr<ID3D11DeviceContext1> context1;D3D11_TEXTURE2D_DESC d{};
    result_=Validate(device,context,eyes,d,context1);if(FAILED(result_))return false;
    if(!Layout(d,w,h,input,output)){result_=E_INVALIDARG;return false;}
    if(busy()&&resources_->cancelled) {
        std::vector<std::uint8_t> ignored;(void)PollCapture(device,context,ignored);
    }
    if(!busy()) {
        // Old probes may resolve multiple independent edits without another
        // capture. Explicitly snapshot current sources for that legacy path only.
        std::vector<std::uint8_t> ignored;unsigned captured_w=0,captured_h=0;
        if(!Capture(device,context,eyes,w==max_extent?max_extent:h,ignored,captured_w,captured_h))return false;
        if(w!=captured_w||h!=captured_h){Discard();result_=E_INVALIDARG;return false;}
    }
    if(!BeginResolve(device,context,eyes,input,output,w,h))return false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(2);
    do {
        if(PollResolve(device,context,eyes))return true;
        if(result_!=E_PENDING){Discard();return false;}
    }while(std::chrono::steady_clock::now()<deadline);
    Discard();result_=E_PENDING;return false;
}
void ImageProcessor::Reset() noexcept {resources_.reset();result_=S_OK;readback_ms_=0;}
}
