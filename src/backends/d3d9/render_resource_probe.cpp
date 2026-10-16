#include "backends/d3d9/render_resource_probe.hpp"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>

namespace cojvr::backends::d3d9 {
bool RenderResourceCallBoundary::Begin(IDirect3DDevice9* device) noexcept {
    std::lock_guard lock(mutex_);
    if(thread_){faulted_=true;return false;}
    if(!device||in_flight_)return false;
    device_=device;thread_=GetCurrentThreadId();faulted_=false;return true;
}
void RenderResourceCallBoundary::Enter(IDirect3DDevice9* device) noexcept {
    std::lock_guard lock(mutex_);++in_flight_;
    if(thread_&&device==device_&&thread_!=GetCurrentThreadId())faulted_=true;
}
void RenderResourceCallBoundary::Leave() noexcept {
    std::lock_guard lock(mutex_);
    if(in_flight_)--in_flight_;else if(thread_)faulted_=true;
}
bool RenderResourceCallBoundary::End(IDirect3DDevice9* device) noexcept {
    std::lock_guard lock(mutex_);
    if(!thread_)return false;
    if(thread_!=GetCurrentThreadId()||device!=device_){faulted_=true;return false;}
    const bool complete=!faulted_&&!in_flight_;
    thread_=0;device_=nullptr;return complete;
}
namespace {
using Microsoft::WRL::ComPtr;
struct LastErrorScope { DWORD value=GetLastError();~LastErrorScope(){SetLastError(value);} };
std::uintptr_t Identity(IUnknown* object) noexcept {
    if(!object)return 0;
    ComPtr<IUnknown> canonical;
    return SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&canonical)))&&canonical
        ? reinterpret_cast<std::uintptr_t>(canonical.Get()):0;
}
D3D9RenderResourceInfo Surface(IDirect3DSurface9* surface,HRESULT read) noexcept {
    D3D9RenderResourceInfo out{};
    if(!surface){out.valid=SUCCEEDED(read)||read==D3DERR_NOTFOUND;return out;}
    out.bound=true;out.object=Identity(surface);out.surface=out.object;
    D3DSURFACE_DESC desc{};
    out.valid=SUCCEEDED(read)&&out.object&&SUCCEEDED(surface->GetDesc(&desc));
    out.width=desc.Width;out.height=desc.Height;out.format=desc.Format;out.usage=desc.Usage;
    out.pool=desc.Pool;out.samples=desc.MultiSampleType;out.quality=desc.MultiSampleQuality;out.type=desc.Type;
    ComPtr<IDirect3DBaseTexture9> parent;
    if(SUCCEEDED(surface->GetContainer(IID_PPV_ARGS(&parent)))&&parent)out.parent=Identity(parent.Get());
    return out;
}
D3D9RenderResourceInfo Texture(IDirect3DBaseTexture9* texture,HRESULT read) noexcept {
    D3D9RenderResourceInfo out{};
    if(!texture){out.valid=SUCCEEDED(read);return out;}
    out.bound=true;out.object=Identity(texture);out.type=texture->GetType();out.levels=texture->GetLevelCount();
    out.valid=SUCCEEDED(read)&&out.object;
    D3DSURFACE_DESC desc{};ComPtr<IDirect3DSurface9> surface;
    if(out.type==D3DRTYPE_TEXTURE){
        auto* tex=static_cast<IDirect3DTexture9*>(texture);
        out.valid=SUCCEEDED(tex->GetLevelDesc(0,&desc))&&out.valid;
        if(SUCCEEDED(tex->GetSurfaceLevel(0,&surface))&&surface)out.surface=Identity(surface.Get());
        else out.valid=false;
    }else if(out.type==D3DRTYPE_CUBETEXTURE){
        auto* tex=static_cast<IDirect3DCubeTexture9*>(texture);
        out.valid=SUCCEEDED(tex->GetLevelDesc(0,&desc))&&out.valid;
        // Cube ownership is recorded; no face is guessed as the bound output.
    }else if(out.type==D3DRTYPE_VOLUMETEXTURE){
        D3DVOLUME_DESC volume{};
        out.valid=SUCCEEDED(static_cast<IDirect3DVolumeTexture9*>(texture)->GetLevelDesc(0,&volume))&&out.valid;
        out.width=volume.Width;out.height=volume.Height;out.depth=volume.Depth;
        out.format=volume.Format;out.usage=volume.Usage;out.pool=volume.Pool;return out;
    }else {out.valid=false;return out;}
    out.width=desc.Width;out.height=desc.Height;out.format=desc.Format;out.usage=desc.Usage;out.pool=desc.Pool;
    return out;
}
}
D3D9RenderResourceSnapshot ObserveD3D9RenderResources(IDirect3DDevice9* device) noexcept {
    LastErrorScope error;
    D3D9RenderResourceSnapshot out{};if(!device)return out;
    D3DCAPS9 caps{};if(FAILED(device->GetDeviceCaps(&caps))||!caps.NumSimultaneousRTs)return out;
    out.target_slots=std::min<DWORD>(4,caps.NumSimultaneousRTs);
    out.valid=caps.NumSimultaneousRTs<=4;
    for(DWORD i=0;i<out.target_slots;++i){
        ComPtr<IDirect3DSurface9> target;const auto hr=device->GetRenderTarget(i,&target);
        out.targets[i]=Surface(target.Get(),hr);out.valid=out.valid&&out.targets[i].valid;
    }
    out.valid=out.valid&&out.targets[0].bound;
    ComPtr<IDirect3DSurface9> depth;const auto hr=device->GetDepthStencilSurface(&depth);
    out.depth=Surface(depth.Get(),hr);out.valid=out.valid&&out.depth.valid;
    D3DVIEWPORT9 viewport{};RECT scissor{};
    out.valid=SUCCEEDED(device->GetViewport(&viewport))&&out.valid;
    out.viewport={viewport.X,viewport.Y,viewport.Width,viewport.Height};out.viewport_depth={viewport.MinZ,viewport.MaxZ};
    out.valid=SUCCEEDED(device->GetScissorRect(&scissor))&&out.valid;
    out.scissor={scissor.left,scissor.top,scissor.right,scissor.bottom};
    out.valid=SUCCEEDED(device->GetRenderState(D3DRS_SCISSORTESTENABLE,&out.scissor_enabled))&&out.valid;
    constexpr D3DRENDERSTATETYPE states[]{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC};
    for(std::size_t i=0;i<out.z.size();++i)out.valid=SUCCEEDED(device->GetRenderState(states[i],&out.z[i]))&&out.valid;
    out.texture_slots=caps.VertexShaderVersion>=D3DVS_VERSION(3,0)?20:16;
    for(DWORD i=0;i<out.texture_slots;++i){
        ComPtr<IDirect3DBaseTexture9> texture;const DWORD stage=i<16?i:D3DVERTEXTEXTURESAMPLER0+i-16;
        const auto result=device->GetTexture(stage,&texture);out.textures[i]=Texture(texture.Get(),result);
        out.valid=out.valid&&out.textures[i].valid;
    }
    ComPtr<IDirect3DVertexShader9> vertex;ComPtr<IDirect3DPixelShader9> pixel;
    out.valid=SUCCEEDED(device->GetVertexShader(&vertex))&&out.valid;
    out.valid=SUCCEEDED(device->GetPixelShader(&pixel))&&out.valid;
    out.vertex_shader=Identity(vertex.Get());out.pixel_shader=Identity(pixel.Get());
    if((vertex&&!out.vertex_shader)||(pixel&&!out.pixel_shader))out.valid=false;
    D3DMATRIX view{},projection{};
    out.transforms_valid=SUCCEEDED(device->GetTransform(D3DTS_VIEW,&view))&&
        SUCCEEDED(device->GetTransform(D3DTS_PROJECTION,&projection));
    if(out.transforms_valid){std::memcpy(out.transforms.data(),&view,sizeof(view));
        std::memcpy(out.transforms.data()+16,&projection,sizeof(projection));}
    return out;
}
D3D9RenderResourceProbe::D3D9RenderResourceProbe(D3D9RenderResourceSink sink,void* context,std::size_t capacity) noexcept
    :sink_(sink),sink_context_(context),capacity_(std::min(capacity,max_records)){}
bool D3D9RenderResourceProbe::Begin(IDirect3DDevice9* device,D3D9RenderResourceContext context) noexcept {
    LastErrorScope error;
    if(owner_thread_.load()){faulted_=true;return false;}
    if(!sink_||!capacity_||!device||context.device!=reinterpret_cast<std::uintptr_t>(device)||
        !context.generation||!context.frame||!context.owner||context.eye>1||context.route>2)return false;
    DWORD expected=0;
    if(!owner_thread_.compare_exchange_strong(expected,GetCurrentThreadId())){faulted_=true;return false;}
    try{records_.clear();records_.reserve(capacity_);}catch(...){owner_thread_=0;return false;}
    context_=context;context_.thread=GetCurrentThreadId();context_.scope=++next_scope_;
    summary_={};faulted_=false;active_=true;sink_(sink_context_,context_,nullptr,nullptr);return true;
}
bool D3D9RenderResourceProbe::Record(IDirect3DDevice9* device,const RenderResourceEvent& event) noexcept {
    LastErrorScope error;
    const DWORD owner=owner_thread_.load();
    if(!owner)return false;
    if(owner&&owner!=GetCurrentThreadId()){faulted_=true;return false;}
    if(!active_)return false;
    if(!device||context_.device!=reinterpret_cast<std::uintptr_t>(device)){faulted_=true;return false;}
    ++summary_.calls;
    if(summary_.truncated)return false;
    const auto snapshot=ObserveD3D9RenderResources(device);
    if(!snapshot.valid)++summary_.read_failures;
    if(!records_.empty()&&records_.back().event==event&&records_.back().state==snapshot){++records_.back().repeat;return true;}
    if(records_.size()==capacity_){summary_.truncated=true;return false;}
    records_.push_back({event,snapshot,summary_.calls,1});return true;
}
bool D3D9RenderResourceProbe::End(IDirect3DDevice9* device,bool complete) noexcept {
    LastErrorScope error;
    const DWORD owner=owner_thread_.load();
    if(!owner)return false;
    if(owner&&owner!=GetCurrentThreadId()){faulted_=true;return false;}
    if(!active_)return false;
    if(context_.device!=reinterpret_cast<std::uintptr_t>(device)){faulted_=true;return false;}
    active_=false;summary_.records=records_.size();
    summary_.complete=complete&&!faulted_&&!summary_.truncated&&!summary_.read_failures;
    for(const auto& record:records_)sink_(sink_context_,context_,&record,nullptr);
    sink_(sink_context_,context_,nullptr,&summary_);owner_thread_=0;return summary_.complete;
}
} // namespace cojvr::backends::d3d9
