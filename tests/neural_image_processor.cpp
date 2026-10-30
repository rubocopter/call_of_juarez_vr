#include "backends/openvr/neural_image_processor.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using cojvr::backends::openvr::neural::ImageProcessor;
namespace cojvr::backends::openvr::neural {
struct ImageProcessorTestAccess {
    static void Probe(ImageProcessor& p,HRESULT (*probe)(ID3D11DeviceContext*,ID3D11Query*) noexcept) {p.readiness_probe_=probe;}
};
}
namespace {
using Bytes=std::vector<std::uint8_t>;
void Require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* message) {Require(SUCCEEDED(hr),message);}
bool hold_query=false;
unsigned query_polls=0;
HRESULT DelayedQuery(ID3D11DeviceContext* context,ID3D11Query* query) noexcept {
    ++query_polls;BOOL done=FALSE;
    const auto hr=context->GetData(query,&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(FAILED(hr))return hr;
    return hold_query||hr==S_FALSE||!done?S_FALSE:S_OK;
}
bool Bgra(DXGI_FORMAT f) {return f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8X8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;}
struct Gpu {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Gpu() {const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &level,1,D3D11_SDK_VERSION,&device,nullptr,&context),"hardware D3D11 unavailable");}
    ComPtr<ID3D11Texture2D> Texture(unsigned w,unsigned h,DXGI_FORMAT format,const Bytes& pixels={},unsigned mips=1) {
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.Format=format;d.ArraySize=1;d.MipLevels=mips;
        d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{pixels.data(),w*4,0};ComPtr<ID3D11Texture2D> texture;
        Hr(device->CreateTexture2D(&d,pixels.empty()?nullptr:&initial,&texture),"fixture texture");return texture;
    }
    Bytes Read(ID3D11Texture2D* texture) {
        D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;
        d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Hr(device->CreateTexture2D(&d,nullptr,&staging),"fixture staging");
        context->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE m{};
        // Blocking is confined to this independent fixture's verification.
        Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m),"fixture readback");
        Bytes bytes(std::size_t(d.Width)*d.Height*4);
        for(unsigned y=0;y<d.Height;++y)std::memcpy(bytes.data()+std::size_t(y)*d.Width*4,
            static_cast<const std::uint8_t*>(m.pData)+std::size_t(y)*m.RowPitch,d.Width*4);
        context->Unmap(staging.Get(),0);return bytes;
    }
};
Bytes Pattern(unsigned w,unsigned h,unsigned eye) {
    Bytes bytes(std::size_t(w)*h*4);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
        auto* p=bytes.data()+(std::size_t(y)*w+x)*4;
        p[0]=std::uint8_t(eye?140:30);p[1]=std::uint8_t(x%2?180:60);
        p[2]=std::uint8_t(y%2?80:120);p[3]=std::uint8_t((x+y+eye*17)%256);
    }return bytes;
}
struct CallerState {
    Gpu& gpu;ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11ShaderResourceView> srv;ComPtr<ID3D11RenderTargetView> rtv;
    D3D11_VIEWPORT viewport{7,9,111,123,.2F,.8F};
    CallerState(Gpu& g,ID3D11Texture2D* input,ID3D11Texture2D* target):gpu(g) {
        const char shader[]="float4 VS(uint id:SV_VertexID):SV_Position{return float4(0,0,0,1);} float4 PS():SV_Target{return 1;}";
        ComPtr<ID3DBlob> v,p;
        Hr(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",0,0,&v,nullptr),"fixture VS");
        Hr(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",0,0,&p,nullptr),"fixture PS");
        Hr(g.device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs),"fixture VS object");
        Hr(g.device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps),"fixture PS object");
        Hr(g.device->CreateShaderResourceView(input,nullptr,&srv),"fixture SRV");
        Hr(g.device->CreateRenderTargetView(target,nullptr,&rtv),"fixture RTV");
        g.context->VSSetShader(vs.Get(),nullptr,0);g.context->PSSetShader(ps.Get(),nullptr,0);
        auto* s=srv.Get();auto* r=rtv.Get();g.context->PSSetShaderResources(5,1,&s);
        g.context->OMSetRenderTargets(1,&r,nullptr);g.context->RSSetViewports(1,&viewport);
    }
    void Verify() {
        ComPtr<ID3D11VertexShader> v;ComPtr<ID3D11PixelShader> p;
        ComPtr<ID3D11ShaderResourceView> s;ComPtr<ID3D11RenderTargetView> r;
        gpu.context->VSGetShader(&v,nullptr,nullptr);gpu.context->PSGetShader(&p,nullptr,nullptr);
        gpu.context->PSGetShaderResources(5,1,&s);gpu.context->OMGetRenderTargets(1,&r,nullptr);
        UINT count=1;D3D11_VIEWPORT actual{};gpu.context->RSGetViewports(&count,&actual);
        Require(v==vs&&p==ps&&s==srv&&r==rtv&&count==1&&std::memcmp(&actual,&viewport,sizeof(actual))==0,
            "caller shader/view/viewport state was not restored");
    }
};
void AsyncCaptureReady(ImageProcessor& p,Gpu& g,Bytes& bytes,CallerState* state=nullptr) {
    for(unsigned i=0;i<500;++i) {
        const bool done=p.PollCapture(g.device.Get(),g.context.Get(),bytes);if(state)state->Verify();
        if(done)return;
        Require(p.last_result()==E_PENDING,"async capture terminal failure");Sleep(1);
    }throw std::runtime_error("async capture failed to drain");
}
void AsyncResolveReady(ImageProcessor& p,Gpu& g,const std::array<ID3D11Texture2D*,2>& eyes,CallerState* state=nullptr) {
    for(unsigned i=0;i<500;++i) {
        const bool done=p.PollResolve(g.device.Get(),g.context.Get(),eyes);if(state)state->Verify();
        if(done)return;
        Require(p.last_result()==E_PENDING,"async resolve terminal failure");Sleep(1);
    }throw std::runtime_error("async resolve failed to drain");
}
void RetainedLifecycle(Gpu& g,DXGI_FORMAT format) {
    constexpr unsigned width=640,height=512;
    const std::array<Bytes,2> original{Pattern(width,height,0),Pattern(width,height,1)};
    auto a=g.Texture(width,height,format,original[0]),b=g.Texture(width,height,format,original[1]);
    const std::array<ID3D11Texture2D*,2> eyes{a.Get(),b.Get()};CallerState state(g,a.Get(),b.Get());
    ImageProcessor p;cojvr::backends::openvr::neural::ImageProcessorTestAccess::Probe(p,DelayedQuery);
    unsigned w=0,h=0;hold_query=true;query_polls=0;
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h)&&w==320&&h==256&&p.busy(),"async BeginCapture");state.Verify();
    Bytes bytes{9,8,7};
    const auto delayed=std::chrono::steady_clock::now();
    for(unsigned i=0;i<8;++i) {
        const unsigned before=query_polls;
        Require(!p.PollCapture(g.device.Get(),g.context.Get(),bytes)&&p.last_result()==E_PENDING&&
            bytes==Bytes({9,8,7})&&p.busy()&&query_polls==before+1,"pending poll restarted/spun/published work");
        unsigned x=17,y=19;
        Require(!p.BeginCapture(g.device.Get(),g.context.Get(),eyes,384,x,y)&&p.last_result()==E_PENDING&&x==17&&y==19,
            "busy BeginCapture overwrote retained frame");state.Verify();Sleep(1);
    }
    Require(std::chrono::steady_clock::now()-delayed>std::chrono::milliseconds(2),"fixture did not delay query beyond old cutoff");
    // Change current sources after the one capture submission. Snapshot and
    // eventual resolve must still correspond to the original detailed pair.
    const Bytes changed(std::size_t(width)*height*4,210);
    for(auto* eye:eyes)g.context->UpdateSubresource(eye,0,nullptr,changed.data(),width*4,0);
    hold_query=false;AsyncCaptureReady(p,g,bytes,&state);Require(p.busy(),"capture released original ownership before inference");
    if(format==DXGI_FORMAT_R8G8B8A8_UNORM)Require(bytes[0]==30&&bytes[1]==120,"late capture read newer frame");
    const auto immutable=bytes;auto edited=bytes;
    for(std::size_t i=0;i<edited.size();++i)if(i%4!=3)edited[i]+=16;
    hold_query=true;
    Require(p.BeginResolve(g.device.Get(),g.context.Get(),eyes,bytes,edited,w,h),"retained BeginResolve");state.Verify();
    for(unsigned i=0;i<8;++i) {
        const unsigned before=query_polls;
        Require(!p.PollResolve(g.device.Get(),g.context.Get(),eyes)&&p.last_result()==E_PENDING&&query_polls==before+1&&p.busy(),
            "pending resolve spun/committed/restarted");state.Verify();Sleep(1);
    }
    Require(g.Read(a.Get())==changed&&g.Read(b.Get())==changed,"pending resolve modified destination pair");
    auto replacement=g.Texture(width,height,format,changed);
    Require(!p.PollResolve(g.device.Get(),g.context.Get(),{a.Get(),replacement.Get()})&&p.last_result()==E_INVALIDARG&&p.busy(),
        "destination replacement was accepted/released in-flight resources");
    hold_query=false;
    Require(!p.PollResolve(g.device.Get(),g.context.Get(),{a.Get(),replacement.Get()})&&p.last_result()==E_INVALIDARG&&p.busy()&&
        g.Read(a.Get())==changed&&g.Read(replacement.Get())==changed,"ready resolve committed to a replaced destination");
    AsyncResolveReady(p,g,eyes,&state);Require(!p.busy()&&bytes==immutable,"resolve changed caller's immutable input");
    for(unsigned eye=0;eye<2;++eye) {
        const auto result=g.Read(eyes[eye]);
        for(std::size_t i=0;i<result.size();++i) {
            if(i%4==3&&format==DXGI_FORMAT_B8G8R8X8_UNORM)continue;
            Require(std::abs(int(result[i])-int(original[eye][i])-(i%4==3?0:16))<=(i%4==3?0:1),
                "async resolve used current sources instead of retained originals/alpha");
        }
    }
    g.context->ClearState();
    std::cout<<"retained >2ms capture/resolve, immutable pair, identities/state format="<<format<<" passed\n";
}
void Cancellation(Gpu& g) {
    const auto initial=Pattern(512,512,0);auto a=g.Texture(512,512,DXGI_FORMAT_R8G8B8A8_UNORM,initial);
    auto b=g.Texture(512,512,DXGI_FORMAT_R8G8B8A8_UNORM,initial);
    const std::array<ID3D11Texture2D*,2> eyes{a.Get(),b.Get()};ImageProcessor p;
    cojvr::backends::openvr::neural::ImageProcessorTestAccess::Probe(p,DelayedQuery);
    unsigned w=0,h=0;Bytes bytes{4,5,6};hold_query=true;
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h),"cancel capture begin");p.Discard();
    Require(p.busy()&&!p.PollCapture(g.device.Get(),g.context.Get(),bytes)&&p.last_result()==E_PENDING&&bytes==Bytes({4,5,6}),
        "cancelled capture reused/unpublished before drain");
    Require(!p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h)&&p.last_result()==E_PENDING,"cancelled in-flight capture overwritten");
    hold_query=false;
    for(unsigned i=0;p.busy()&&i<500;++i){Require(!p.PollCapture(g.device.Get(),g.context.Get(),bytes),"cancelled capture published");Sleep(1);}
    Require(!p.busy()&&p.last_result()==E_ABORT&&bytes==Bytes({4,5,6}),"cancel capture did not drain");
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h),"post-cancel capture");AsyncCaptureReady(p,g,bytes);
    auto out=bytes;for(std::size_t i=0;i<out.size();++i)if(i%4!=3)out[i]+=16;
    hold_query=true;Require(p.BeginResolve(g.device.Get(),g.context.Get(),eyes,bytes,out,w,h),"cancel resolve begin");p.Discard();
    const auto immutable=bytes;
    Require(p.busy()&&!p.PollCapture(g.device.Get(),g.context.Get(),bytes)&&p.last_result()==E_PENDING&&bytes==immutable,
        "universal capture poll did not retain cancelled resolve");
    Require(!p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h),"cancelled in-flight resolve overwritten");
    hold_query=false;
    for(unsigned i=0;p.busy()&&i<500;++i){Require(!p.PollCapture(g.device.Get(),g.context.Get(),bytes),"cancelled resolve published bytes");Sleep(1);}
    Require(!p.busy()&&p.last_result()==E_ABORT&&bytes==immutable&&g.Read(a.Get())==initial&&g.Read(b.Get())==initial,
        "universal cancellation drain changed native pair or input");
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h),"reset pending begin");p.Discard();p.Reset();
    Require(!p.busy(),"reset retained pending lease");
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h),"reset resource recreation");AsyncCaptureReady(p,g,bytes);p.Discard();
    Require(!p.busy(),"discard completed snapshot retained busy ownership");
    // Retire caller ownership while a cancelled resolve is retained. The universal
    // drain takes no destination pointers and must never redirect output to a
    // replacement pair, even after the GPU query has actually completed.
    std::array<ComPtr<ID3D11Texture2D>,2> retiring{
        g.Texture(512,512,DXGI_FORMAT_R8G8B8A8_UNORM,initial),g.Texture(512,512,DXGI_FORMAT_R8G8B8A8_UNORM,initial)};
    Require(p.BeginCapture(g.device.Get(),g.context.Get(),{retiring[0].Get(),retiring[1].Get()},256,w,h),"retirement capture");
    AsyncCaptureReady(p,g,bytes);hold_query=true;
    Require(p.BeginResolve(g.device.Get(),g.context.Get(),{retiring[0].Get(),retiring[1].Get()},bytes,out,w,h),"retirement resolve");
    p.Discard();for(auto& eye:retiring)eye.Reset();
    auto replacement=g.Texture(512,512,DXGI_FORMAT_R8G8B8A8_UNORM,initial);
    Require(!p.BeginCapture(g.device.Get(),g.context.Get(),eyes,256,w,h)&&p.last_result()==E_PENDING,"retirement reused pending private textures");
    const auto retained_input=bytes;
    Require(!p.PollCapture(g.device.Get(),g.context.Get(),bytes)&&p.last_result()==E_PENDING&&bytes==retained_input,"retirement prematurely published/released");
    hold_query=false;
    for(unsigned i=0;p.busy()&&i<500;++i){Require(!p.PollCapture(g.device.Get(),g.context.Get(),bytes),"retired output published");Sleep(1);}
    Require(!p.busy()&&p.last_result()==E_ABORT&&bytes==retained_input&&g.Read(replacement.Get())==initial,"retired output changed replacement/input");
    std::cout<<"capture/resolve cancellation drain and reset recreation passed\n";
}
void CaptureReady(ImageProcessor& processor,Gpu& g,const std::array<ID3D11Texture2D*,2>& eyes,
                  unsigned requested,Bytes& bytes,unsigned& w,unsigned& h,CallerState* state=nullptr) {
    if(processor.busy()) {
        processor.Discard();
        for(unsigned i=0;processor.busy()&&i<500;++i) {
            Bytes ignored;(void)processor.PollCapture(g.device.Get(),g.context.Get(),ignored);Sleep(1);
        }
        Require(!processor.busy(),"previous cancelled GPU operation did not drain");
    }
    Require(processor.BeginCapture(g.device.Get(),g.context.Get(),eyes,requested,w,h),"capture begin");
    if(state)state->Verify();AsyncCaptureReady(processor,g,bytes,state);
}
void ResolveReady(ImageProcessor& processor,Gpu& g,const std::array<ID3D11Texture2D*,2>& eyes,
                  const Bytes& input,const Bytes& output,unsigned w,unsigned h,CallerState* state=nullptr) {
    if(!processor.busy()) {
        Bytes ignored;unsigned x=0,y=0;CaptureReady(processor,g,eyes,w==1280?1280:h,ignored,x,y,state);
        Require(x==w&&y==h,"fixture recapture layout mismatch");
    }
    Require(processor.BeginResolve(g.device.Get(),g.context.Get(),eyes,input,output,w,h),"resolve begin");
    if(state)state->Verify();AsyncResolveReady(processor,g,eyes,state);
}
void PixelsAndState(Gpu& g,DXGI_FORMAT format) {
    constexpr unsigned width=640,height=512;
    const std::array<Bytes,2> source{Pattern(width,height,0),Pattern(width,height,1)};
    auto left=g.Texture(width,height,format,source[0]),right=g.Texture(width,height,format,source[1]);
    const std::array<ID3D11Texture2D*,2> eyes{left.Get(),right.Get()};
    CallerState state(g,left.Get(),right.Get());ImageProcessor processor;
    Bytes pair;unsigned w=0,h=0;CaptureReady(processor,g,eyes,256,pair,w,h,&state);
    Require(w==320&&h==256&&pair.size()==320U*256*8,"proxy dimensions/layout lost aspect");
    // UNORM capture's exact 2x footprint must average the fine alternating stripes,
    // swizzle BGRA to transport RGBA and keep left/right distinct.
    if(format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8X8_UNORM) {
        Require(pair[0]==(Bgra(format)?100:30)&&pair[1]==120&&pair[2]==(Bgra(format)?30:100),"capture filtering or RGBA encoding");
        Require(pair[w*h*4+(Bgra(format)?2:0)]==140,"capture stereo order");
    }
    for(unsigned edit:{0U,16U}) {
        for(unsigned eye=0;eye<2;++eye)g.context->UpdateSubresource(eyes[eye],0,nullptr,source[eye].data(),width*4,0);
        auto out=pair;for(std::size_t i=0;i<out.size();++i)if(i%4!=3)out[i]=std::uint8_t(std::min(255U,unsigned(out[i])+edit));
        // Model alpha is deliberately unrelated to source alpha.
        for(std::size_t i=3;i<out.size();i+=4)out[i]=0;
        ResolveReady(processor,g,eyes,pair,out,w,h,&state);
        for(unsigned eye=0;eye<2;++eye) {
            const auto actual=g.Read(eyes[eye]);
            for(std::size_t i=0;i<actual.size();++i) {
                if(i%4==3&&format==DXGI_FORMAT_B8G8R8X8_UNORM)continue; // X is undefined, not alpha.
                const int expected=source[eye][i]+(i%4==3?0:int(edit));
                Require(std::abs(int(actual[i])-expected)<=(i%4==3?0:1),"residual erased detail/alpha or used wrong color space");
            }
        }
    }
    // A half-plane proxy edit has fractional coverage at these four native
    // pixel centers. Nearest-neighbor residuals or full-image enlargement fail.
    for(unsigned eye=0;eye<2;++eye)g.context->UpdateSubresource(eyes[eye],0,nullptr,source[eye].data(),width*4,0);
    auto edge=pair;
    for(unsigned eye=0;eye<2;++eye)for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w/2;++x)
        edge[(std::size_t(eye)*w*h+y*w+x)*4]+=16;
    ResolveReady(processor,g,eyes,pair,edge,w,h,&state);
    const auto actual=g.Read(eyes[0]);const unsigned red=Bgra(format)?2:0;
    for(unsigned x=318;x<=321;++x) {
        constexpr int deltas[]{16,12,4,0};const auto i=(std::size_t(100)*width+x)*4+red;
        Require(std::abs(int(actual[i])-int(source[0][i])-deltas[x-318])<=1,"residual is not bilinear in native pixel coordinates");
    }
    std::cout<<"pixels/state format="<<format<<"; stage="<<w<<'x'<<h<<"; readback_ms="<<processor.last_readback_ms()<<'\n';
    g.context->ClearState();processor.Reset();Require(processor.last_result()==S_OK,"reset HRESULT");
    CaptureReady(processor,g,eyes,256,pair,w,h);
}
void InvalidPairs(Gpu& g) {
    constexpr unsigned size=512;const auto bytes=Pattern(size,size,0);
    auto a=g.Texture(size,size,DXGI_FORMAT_R8G8B8A8_UNORM,bytes),b=g.Texture(size,size,DXGI_FORMAT_R8G8B8A8_UNORM,bytes);
    auto wrong=g.Texture(256,size,DXGI_FORMAT_R8G8B8A8_UNORM);
    auto mips=g.Texture(size,size,DXGI_FORMAT_R8G8B8A8_UNORM,{},2);
    Gpu foreign;auto other=foreign.Texture(size,size,DXGI_FORMAT_R8G8B8A8_UNORM);
    ImageProcessor p;Bytes pair;unsigned w=0,h=0;std::array<ID3D11Texture2D*,2> valid{a.Get(),b.Get()};
    CaptureReady(p,g,valid,256,pair,w,h);auto out=pair;
    for(std::size_t i=0;i<out.size();++i)if(i%4!=3)out[i]=std::uint8_t(out[i]+16);
    for(auto* invalid:{static_cast<ID3D11Texture2D*>(nullptr),a.Get(),wrong.Get(),mips.Get(),other.Get()}) {
        std::array<ID3D11Texture2D*,2> eyes{a.Get(),invalid};
        Require(!p.Resolve(g.device.Get(),g.context.Get(),eyes,pair,out,w,h)&&p.last_result()==E_INVALIDARG,"invalid second eye admitted");
        Require(g.Read(a.Get())==bytes&&g.Read(b.Get())==bytes,"invalid second eye partially wrote first");
        Bytes sentinel{1,2,3};unsigned x=77,y=88;
        Require(!p.Capture(g.device.Get(),g.context.Get(),eyes,256,sentinel,x,y)&&sentinel==Bytes({1,2,3})&&x==77&&y==88,"invalid capture published bytes");
    }
    Require(!p.Resolve(g.device.Get(),g.context.Get(),valid,pair,std::span<const std::uint8_t>(out).first(out.size()-1),w,h),"short output accepted");
    Require(g.Read(a.Get())==bytes&&g.Read(b.Get())==bytes,"short pair wrote originals");
    Require(!p.Capture(g.device.Get(),foreign.context.Get(),valid,256,pair,w,h),"foreign context admitted");
    ComPtr<ID3D11DeviceContext> deferred;Hr(g.device->CreateDeferredContext(0,&deferred),"deferred fixture");
    Require(!p.Resolve(g.device.Get(),deferred.Get(),valid,pair,out,w,h),"deferred context admitted");
    Require(!p.Capture(g.device.Get(),g.context.Get(),valid,255,pair,w,h),"sub-model dimensions admitted");
    auto narrow=g.Texture(256,1024,DXGI_FORMAT_R8G8B8A8_UNORM),narrow2=g.Texture(256,1024,DXGI_FORMAT_R8G8B8A8_UNORM);
    Require(!p.Capture(g.device.Get(),g.context.Get(),{narrow.Get(),narrow2.Get()},384,pair,w,h),"aspect was distorted to force minimum");
    auto wide=g.Texture(2048,512,DXGI_FORMAT_R8G8B8A8_UNORM),wide2=g.Texture(2048,512,DXGI_FORMAT_R8G8B8A8_UNORM);
    CaptureReady(p,g,{wide.Get(),wide2.Get()},1280,pair,w,h);Require(w==1280&&h==320,"max proxy extent/aspect");
    auto rounded=g.Texture(1960,400,DXGI_FORMAT_R8G8B8A8_UNORM),rounded2=g.Texture(1960,400,DXGI_FORMAT_R8G8B8A8_UNORM);
    CaptureReady(p,g,{rounded.Get(),rounded2.Get()},384,pair,w,h);
    Require(w==1280&&h==261,"width-cap rounding");
    ResolveReady(p,g,{rounded.Get(),rounded2.Get()},pair,pair,w,h);
    auto tall=g.Texture(2804,2860,DXGI_FORMAT_R8G8B8A8_UNORM),tall2=g.Texture(2804,2860,DXGI_FORMAT_R8G8B8A8_UNORM);
    CaptureReady(p,g,{tall.Get(),tall2.Get()},384,pair,w,h);Require(w==376&&h==384,"candidate stage aspect");
    std::cout<<"invalid pairs/layout/devices and candidate dimensions passed\n";
}
void NativeLatency(Gpu& g) {
    constexpr unsigned width=2804,height=2860;
    auto left=g.Texture(width,height,DXGI_FORMAT_B8G8R8A8_UNORM),right=g.Texture(width,height,DXGI_FORMAT_B8G8R8A8_UNORM);
    const std::array<ID3D11Texture2D*,2> eyes{left.Get(),right.Get()};
    std::array<ComPtr<ID3D11RenderTargetView>,2> rtvs;
    for(unsigned i=0;i<2;++i)Hr(g.device->CreateRenderTargetView(eyes[i],nullptr,&rtvs[i]),"native RTV");
    ImageProcessor p;Bytes bytes;unsigned w=0,h=0;
    std::vector<double> capture_begin,capture_poll,resolve_begin,resolve_poll,pair_age;
    unsigned pending_capture=0,pending_resolve=0;
    auto elapsed=[](auto begin){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();};
    for(unsigned frame=0;frame<20;++frame) {
        const float colors[2][4]{{float(30+frame)/255.F,.25F,.5F,.4F},{.75F,float(40+frame)/255.F,.125F,.6F}};
        for(unsigned eye=0;eye<2;++eye)g.context->ClearRenderTargetView(rtvs[eye].Get(),colors[eye]);
        const auto age_begin=std::chrono::steady_clock::now();
        Require(p.BeginCapture(g.device.Get(),g.context.Get(),eyes,384,w,h),"native retained BeginCapture");
        if(frame)capture_begin.push_back(elapsed(age_begin));
        bool captured=false;
        for(unsigned attempt=0;attempt<500;++attempt) {
            const auto begin=std::chrono::steady_clock::now();captured=p.PollCapture(g.device.Get(),g.context.Get(),bytes);
            if(frame)capture_poll.push_back(elapsed(begin));
            if(captured)break;
            Require(p.last_result()==E_PENDING,"native retained capture error");++pending_capture;Sleep(1);
        }
        Require(captured&&w==376&&h==384&&std::abs(int(bytes[0])-int(30+frame))<=1&&
            std::abs(int(bytes[std::size_t(w)*h*4+1])-int(40+frame))<=1,"native capture returned stale pair");
        const auto begin_resolve=std::chrono::steady_clock::now();
        Require(p.BeginResolve(g.device.Get(),g.context.Get(),eyes,bytes,bytes,w,h),"native retained BeginResolve");
        if(frame)resolve_begin.push_back(elapsed(begin_resolve));
        bool resolved=false;
        for(unsigned attempt=0;attempt<500;++attempt) {
            const auto begin=std::chrono::steady_clock::now();resolved=p.PollResolve(g.device.Get(),g.context.Get(),eyes);
            if(frame)resolve_poll.push_back(elapsed(begin));
            if(resolved)break;
            Require(p.last_result()==E_PENDING,"native retained resolve error");++pending_resolve;Sleep(1);
        }
        Require(resolved&&!p.busy(),"native retained pair did not complete");
        if(frame)pair_age.push_back(elapsed(age_begin));
    }
    auto report=[](const char* label,std::vector<double> samples) {
        std::sort(samples.begin(),samples.end());
        std::cout<<label<<"_ms p50="<<samples[samples.size()/2]<<" p95="<<samples[(samples.size()-1)*95/100]<<" max="<<samples.back()<<'\n';
    };
    std::cout<<"native BGRA 2804x2860 -> 376x384; retained pairs=20; pending_capture_polls="<<pending_capture
        <<"; pending_resolve_polls="<<pending_resolve<<"; pair age includes fixture 1ms poll pacing\n";
    report("capture_begin",capture_begin);report("capture_poll",capture_poll);
    report("resolve_begin",resolve_begin);report("resolve_poll",resolve_poll);report("pair_age",pair_age);

}
}
int main() try {
    Gpu gpu;
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8X8_UNORM,
                    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB})RetainedLifecycle(gpu,format);
    Cancellation(gpu);
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8X8_UNORM,
                    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB})PixelsAndState(gpu,format);
    InvalidPairs(gpu);NativeLatency(gpu);std::cout<<"Neural image processor hardware fixtures passed\n";
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
