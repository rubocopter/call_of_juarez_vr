// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Penumbra VR Framework 7b014c96ed928440abf4452664f35220d06c1fb6.
// CoJ standalone laboratory: stderr diagnostics; no swapchain/game integration.
#include "NeuralPass.h"
#include <cstdio>
#include <cstdarg>
#include <filesystem>
#if defined(_WIN64)
#include "../vendor/nr-bridge/nr_direct.h"

void BridgeLog(const char* fmt,...) {
    fprintf(stderr,"[CoJ Neural %llu] ",GetTickCount64());
    va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);fputc('\n',stderr);
}
struct NeuralPass::State {
    DirectNR processor;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Texture2D> work;
    D3D11_TEXTURE2D_DESC desc{};
    uint64_t lastFrame{},runs{},lastTrackingTick{};
    bool valid{},failed{},inputLogged{};
    unsigned bypassReason{};
    ~State(){processor.Shutdown(false);}
};
NeuralPass::NeuralPass()=default;
NeuralPass::~NeuralPass()=default;
void NeuralPass::Reset(){state.reset();}
bool NeuralPass::Initialize(ID3D11Device* device,const Options& o,const std::wstring& dir,int diagnosticMode){
    Reset();if(!o.enabled)return false;
    if(!device || (diagnosticMode==0 && (!std::filesystem::exists(dir+L"\\nvngx_dlssnr.dll") || !std::filesystem::exists(dir+L"\\nvngx.dll_nr_bridge.dll")))){
        BridgeLog("Neural unavailable: runtime missing; original stereo retained");return false;
    }
    auto s=std::make_unique<State>();s->device=device;
    DirectConfig c;c.ngx=diagnosticMode?diagnosticMode:std::clamp(o.backend,0,2);
    c.scale=std::clamp(o.scalePercent,25,100)*.01f;
    c.intensity=std::clamp(o.intensityPercent,0,100)*.01f;
    c.local_structure=o.localStructure;c.local_tone=o.localTone;c.skin_structure=o.skinAuto?-1.f:o.skinStrength;
    c.style=o.style;c.preset=o.preset;c.auto_mask=o.autoMask;c.passes=o.passes;c.residual=o.residual;
    c.fovea=o.foveaPercent*.01f;c.fovea_shape=o.foveaShape;c.feather=o.featherPixels;
    c.outer_scale=o.outerEnabled?o.outerScalePercent*.01f:0.f;c.outer_pack=o.outerPack;c.viewport_ref=o.viewportPercent*.01f;
    c.mv=o.motionVectors;c.mv_sign=o.motionSign;c.mv_scale=o.motionScale;c.depth_value=o.depth;
    c.app_id=o.appId;c.log_frames=o.logFrames;
    if(!s->processor.Init(device,c,dir,dir))return false;
    state=std::move(s);
    BridgeLog("DEUSEXVR Neural ready: scale=%d intensity=%d estimated rotation vectors, constant depth, paired-eye model history",o.scalePercent,o.intensityPercent);
    return true;
}
void NeuralPass::Suspend(){if(state){state->valid=false;state->processor.RequestReset();}}
bool NeuralPass::Process(ID3D11DeviceContext* ctx,ID3D11Texture2D* destination,const Transport::Tracking& tracking,uint64_t frame,DXGI_FORMAT viewFormat){
    if(!state||state->failed||!ctx||!destination||!tracking.valid){Suspend();return false;}
    auto& s=*state;
    D3D11_TEXTURE2D_DESC d{};destination->GetDesc(&d);
    if(!s.inputLogged){BridgeLog("DEUSEXVR Neural input: resourceFormat=%u viewFormat=%u extent=%ux%u array=%u mip=%u samples=%u bind=%u",unsigned(d.Format),unsigned(viewFormat),d.Width,d.Height,d.ArraySize,d.MipLevels,d.SampleDesc.Count,d.BindFlags);s.inputLogged=true;}
    auto bypass=[&](unsigned code,const char* reason){if(s.bypassReason!=code)BridgeLog("DEUSEXVR Neural BYPASS: %s resourceFormat=%u viewFormat=%u array=%u mip=%u samples=%u",reason,unsigned(d.Format),unsigned(viewFormat),d.ArraySize,d.MipLevels,d.SampleDesc.Count);s.bypassReason=code;Suspend();return false;};
    if(d.ArraySize!=2||d.MipLevels!=1||d.SampleDesc.Count!=1||!d.Width||!d.Height)return bypass(1,"unsupported image layout");
    // OpenXR may expose a typeless resource for a typed swapchain. Process a
    // compatible typed private image; CopyResource preserves the original bytes.
    if((d.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS&&(viewFormat==DXGI_FORMAT_R8G8B8A8_UNORM||viewFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))||
       (d.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS&&(viewFormat==DXGI_FORMAT_B8G8R8A8_UNORM||viewFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)))d.Format=viewFormat;
    if(d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)return bypass(2,"unsupported image format");
    ComPtr<ID3D11Device> owner;destination->GetDevice(&owner);
    if(owner.Get()!=s.device.Get())return bypass(3,"different device");
    if(!s.work||s.desc.Width!=d.Width||s.desc.Height!=d.Height||s.desc.Format!=d.Format){
        s.work.Reset();s.valid=false;s.processor.RequestReset();
        auto privateDesc=d;privateDesc.Usage=D3D11_USAGE_DEFAULT;privateDesc.CPUAccessFlags=0;privateDesc.MiscFlags=0;
        privateDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        HRESULT hr=s.device->CreateTexture2D(&privateDesc,nullptr,&s.work);
        if(FAILED(hr)){s.failed=true;BridgeLog("DEUSEXVR Neural fault: private image creation failed hr=0x%08x; original pair retained",unsigned(hr));return false;}s.desc=d;
    }
    if(frame<s.lastFrame || tracking.tick<s.lastTrackingTick || tracking.tick-s.lastTrackingTick>250)s.processor.RequestReset();
    DirectView views[2]{};
    for(unsigned eye=0;eye<2;eye++){
        const auto& e=tracking.eyes[eye];auto& v=views[eye];
        const float norm=e.pose.orientation.x*e.pose.orientation.x+e.pose.orientation.y*e.pose.orientation.y+e.pose.orientation.z*e.pose.orientation.z+e.pose.orientation.w*e.pose.orientation.w;
        if(!std::isfinite(norm)||norm<.9f||norm>1.1f||!std::isfinite(e.left)||!std::isfinite(e.right)||!std::isfinite(e.up)||!std::isfinite(e.down)||e.left>=e.right||e.down>=e.up)return bypass(4,"invalid pose or frustum");
        v.texture=s.work.Get();v.arrayIndex=eye;v.right=d.Width;v.bottom=d.Height;
        memcpy(v.q,&e.pose.orientation,sizeof(v.q));memcpy(v.p,&e.pose.position,sizeof(v.p));
        v.fovL=e.left;v.fovR=e.right;v.fovU=e.up;v.fovD=e.down;
    }
    if(s.valid && frame==s.lastFrame){ctx->CopyResource(destination,s.work.Get());return true;}
    ctx->CopyResource(s.work.Get(),destination);
    if(!s.processor.Process(views,2)){
        s.failed=true;s.valid=false;BridgeLog("DEUSEXVR Neural fault: retaining unmodified pair; disabled until next launch");return false;
    }
    ctx->CopyResource(destination,s.work.Get());
    s.valid=true;s.lastFrame=frame;s.lastTrackingTick=tracking.tick;s.bypassReason=0;++s.runs;
    if(s.runs<=3||s.runs%600==0)BridgeLog("DEUSEXVR Neural APPLIED frame=%llu eyes=0,1 extent=%ux%u runs=%llu gpuMs=%.3f",frame,d.Width,d.Height,s.runs,s.processor.avgGpuMs());
    return true;
}
uint64_t NeuralPass::Runs()const{return state?state->runs:0;}
double NeuralPass::GpuMs()const{return state?state->processor.avgGpuMs():0;}
#else
struct NeuralPass::State {};
NeuralPass::NeuralPass()=default;NeuralPass::~NeuralPass()=default;
bool NeuralPass::Initialize(ID3D11Device*,const Options&,const std::wstring&,int){return false;}
bool NeuralPass::Process(ID3D11DeviceContext*,ID3D11Texture2D*,const Transport::Tracking&,uint64_t,DXGI_FORMAT){return false;}
void NeuralPass::Suspend(){} void NeuralPass::Reset(){state.reset();}
uint64_t NeuralPass::Runs()const{return 0;}double NeuralPass::GpuMs()const{return 0;}
#endif
