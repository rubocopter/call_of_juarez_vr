#include "games/call_of_juarez/independent_render_scope.hpp"
#include "games/call_of_juarez/independent_render_fields.hpp"
#include "games/call_of_juarez/native_render_queue_witness.hpp"
#include "backends/d3d9/independent_eye_targets.hpp"
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>

namespace cojvr::games::call_of_juarez {
namespace {
using Microsoft::WRL::ComPtr;
using backends::d3d9::IndependentEyeSurface;
using backends::d3d9::IndependentEyeTargetConfig;
constexpr std::uint64_t MiB=1024ULL*1024;
bool ReadLocal(void*,std::uint32_t address,void* out,std::size_t size) noexcept {
    const DWORD error=GetLastError();SIZE_T n=0;
    const bool ok=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(address)),out,size,&n)&&n==size;
    SetLastError(error);return ok;
}
bool WriteLocal(void*,std::uint32_t address,const void* in,std::size_t size) noexcept {
    const DWORD error=GetLastError();SIZE_T n=0;
    const bool ok=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(address)),in,size,&n)&&n==size;
    SetLastError(error);return ok;
}
template<class T> std::uint32_t Id(T* p) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}
template<class T> T* Pointer(std::uint32_t id) noexcept {
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(id));
}
std::uint64_t LargestFreeRegion() noexcept {
    std::uintptr_t cursor=0;std::uint64_t largest=0;MEMORY_BASIC_INFORMATION region{};
    while(VirtualQuery(reinterpret_cast<const void*>(cursor),&region,sizeof(region))==sizeof(region)){
        if(region.State==MEM_FREE)largest=(std::max)(largest,std::uint64_t(region.RegionSize));
        const auto next=reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize;
        if(next<=cursor)break;cursor=next;
    }
    return largest;
}
bool MemoryHeadroom(std::uint64_t bank,std::uint32_t height) noexcept {
    MEMORYSTATUSEX memory{sizeof(memory)};
    if(!GlobalMemoryStatusEx(&memory))return false;
    // Payload is not commitment or driver VA. Include the six shared capture
    // slots, four fixed outputs, flat CPU uploads, scratch and a second complete
    // private/transport mapping allowance plus a separate 512 MiB reserve.
    const std::uint64_t transport=6ULL*2560*height*4;
    const std::uint64_t envelope=2*(bank+transport)+128*MiB+
        2ULL*2560*1440*4+128*MiB+512*MiB;
    if(memory.ullAvailVirtual<envelope)return false;
    return LargestFreeRegion()>=128*MiB;
}
}
struct CoJIndependentRenderScope::Impl {
    explicit Impl(CoJRenderMemory memory):io(memory.read&&memory.write?memory:
        CoJRenderMemory{ReadLocal,WriteLocal,nullptr}),tx(io),production(!memory.read){}
    CoJRenderMemory io;
    CoJNativeRenderTransaction tx;
    CoJNativeRenderQueueWitness queue;
    bool production=false,queue_open=false,queue_ok=true;
    std::uint64_t scope_serial=0;
    backends::d3d9::IndependentEyeTargets targets;
    ComPtr<IDirect3DDevice9> device;
    std::array<ComPtr<IDirect3DTexture9>,14> originals;
    std::array<ComPtr<IDirect3DSurface9>,5> surfaces;
    CoJIndependentResolutionRecipe recipe{};
    std::array<std::uint32_t,14> native_counts{};
    std::array<std::uint32_t,3> surface_counts{};
    std::array<D3DSURFACE_DESC,5> layout{};
    std::uint32_t engine=0,renderer=0,manager=0;
    DWORD thread=0;std::uint64_t generation=0,attempted_generation=0;
    ULONGLONG last_budget_tick=0;
    bool scope=false,disabled=false,ended_right=false;
    CoJIndependentRenderEvidence evidence{};
    const char* error="";
    bool Fail(const char* value) noexcept {error=value;return false;}
    template<class T> bool Read(std::uint32_t address,T& value) noexcept {
        return io.read(io.context,address,&value,sizeof(value));
    }
    bool Roots() noexcept {
        std::uint32_t a=0,b=0,c=0;std::uint8_t dirty=1;
        return Read(engine+0x590074,a)&&Read(engine+0x590078,b)&&Read(engine+0x5902ac,c)&&
            a==renderer&&b==Id(device.Get())&&c==manager&&Read(manager+0x6b5,dirty)&&!dirty;
    }
    bool Counts() noexcept {
        for(std::size_t i=0;i<recipe.textures.size();++i){
            std::uint32_t count=0,handle=0;
            if(!Read(recipe.textures[i].handle+0x168,count)||count!=native_counts[i]||
               !Read(recipe.textures[i].entry,handle)||handle!=recipe.textures[i].handle)return false;
        }
        constexpr std::uint32_t offsets[]{0x1c20,0x1c30,0x1c40};
        for(std::size_t i=0;i<3;++i){std::uint32_t count=0;
            if(!Read(renderer+offsets[i],count)||count!=surface_counts[i])return false;}
        return true;
    }
    bool Unbind() noexcept {
        D3DCAPS9 caps{};if(!device||FAILED(device->GetDeviceCaps(&caps)))return false;
        bool ok=true;
        for(DWORD i=0;i<16;++i)ok=SUCCEEDED(device->SetTexture(i,nullptr))&&ok;
        if(caps.VertexShaderVersion>=D3DVS_VERSION(3,0)&&caps.VertexTextureFilterCaps)
            for(DWORD i=0;i<4;++i)ok=SUCCEEDED(device->SetTexture(D3DVERTEXTEXTURESAMPLER0+i,nullptr))&&ok;
        return ok;
    }
    void ReleaseOriginals() noexcept {
        for(auto& p:originals)p.Reset();for(auto& p:surfaces)p.Reset();device.Reset();
    }
};
CoJIndependentRenderScope::CoJIndependentRenderScope(CoJRenderMemory memory):impl_(std::make_unique<Impl>(memory)){}
CoJIndependentRenderScope::~CoJIndependentRenderScope(){
    if(impl_->scope)(void)End();
    // A failed rollback owns retained state. Never destroy its proof/resources.
    // The production owner is already process-lifetime; this also fences misuse.
    if(restoration_pending())(void)impl_.release();
}
bool CoJIndependentRenderScope::Begin(IDirect3DDevice9* device,std::uint64_t generation,
    const CoJPostprocessResourceSnapshot& s,std::uint32_t height,runtime::Eye eye) noexcept {
    auto& b=*impl_;
    if(b.scope||b.tx.restoration_pending()||b.targets.active()||b.disabled||!device||!generation||
       (b.thread&&b.thread!=GetCurrentThreadId()))return b.Fail("scope/thread/rollback admission rejected");
    const auto tick=GetTickCount64();
    if(b.targets.ready()&&tick-b.last_budget_tick>=1000){
        b.last_budget_tick=tick;MEMORYSTATUSEX memory{sizeof(memory)};
        if(!GlobalMemoryStatusEx(&memory)||memory.ullAvailVirtual<512*MiB||LargestFreeRegion()<128*MiB){
            b.disabled=true;return b.Fail("live VA reserve exhausted; natural fallback required");
        }
    }
    CoJIndependentResolutionRecipe recipe;
    if(!BuildCoJIndependentResolutionRecipe(s,height,recipe)||s.device!=Id(device)||
       s.renderer_vtable!=s.engine_base+0x312334||s.manager_vtable!=s.engine_base+0x314c48)
        return b.Fail("unqualified exact native family or device");
    b.thread=GetCurrentThreadId();b.engine=s.engine_base;b.renderer=s.renderer;b.manager=s.manager;
    b.evidence={};
    b.device=device;b.recipe=recipe;b.generation=generation;b.ended_right=false;
    if(!b.Roots()) {b.ReleaseOriginals();return b.Fail("native roots changed before scope");}
    std::uint32_t samples=~0u;
    if(!b.Read(s.renderer+0x1c10,samples)||samples!=D3DMULTISAMPLE_NONE){
        b.ReleaseOriginals();return b.Fail("native internal MSAA rejected");}
    constexpr std::uint32_t count_offsets[]{0x1c20,0x1c30,0x1c40};
    for(std::size_t i=0;i<3;++i)if(!b.Read(s.renderer+count_offsets[i],b.surface_counts[i])){
        b.ReleaseOriginals();return b.Fail("native surface counts unreadable");}
    IndependentEyeTargetConfig config;config.width=recipe.width;config.height=recipe.height;
    config.max_payload_bytes=384*MiB;
    for(std::size_t i=0;i<recipe.textures.size();++i){
        const auto& t=recipe.textures[i];auto* source=Pointer<IDirect3DTexture9>(t.original_interface);
        D3DSURFACE_DESC desc{};ComPtr<IDirect3DDevice9> owner;
        if(!b.Read(t.handle+0x168,b.native_counts[i])||!b.native_counts[i]||
           source->GetLevelCount()!=1||FAILED(source->GetLevelDesc(0,&desc))||
           FAILED(source->GetDevice(&owner))||owner.Get()!=device||
           desc.Width!=s.buffers[i].texture.width||desc.Height!=s.buffers[i].texture.height||
           desc.Format!=static_cast<D3DFORMAT>(t.format)||desc.Pool!=D3DPOOL_DEFAULT||
           desc.Usage!=D3DUSAGE_RENDERTARGET||desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
            b.ReleaseOriginals();return b.Fail("original PP COM layout/lifetime rejected");}
        b.originals[i]=source;
        config.textures[i]={t.width,t.height,desc.Format,D3DUSAGE_RENDERTARGET,t.history};
    }
    const auto& r=s.renderer_state;
    const std::array<std::uint32_t,5> ids{r.default_color_surface,r.default_depth_surface,
        r.internal_color_surface,r.internal_depth_surface,r.additional_depth_surface};
    for(std::size_t i=0;i<5;++i){
        if(!ids[i]){b.ReleaseOriginals();return b.Fail("missing native surface group");}
        auto* source=Pointer<IDirect3DSurface9>(ids[i]);ComPtr<IDirect3DDevice9> owner;
        D3DSURFACE_DESC desc{};
        if(FAILED(source->GetDesc(&desc))||FAILED(source->GetDevice(&owner))||owner.Get()!=device||
           desc.Pool!=D3DPOOL_DEFAULT||desc.MultiSampleType!=D3DMULTISAMPLE_NONE||desc.MultiSampleQuality||
           (i<4&&(desc.Width!=2560||desc.Height!=1440))){
            b.ReleaseOriginals();return b.Fail("native surface extent/ownership rejected");}
        b.surfaces[i]=source;
        desc.Type=D3DRTYPE_SURFACE;desc.Usage=(i==0||i==2)?D3DUSAGE_RENDERTARGET:D3DUSAGE_DEPTHSTENCIL;
        desc.Width=i==4?(std::max)(desc.Width,recipe.width):recipe.width;
        desc.Height=i==4?(std::max)(desc.Height,recipe.height):recipe.height;
        config.surfaces[i]=desc;
    }
    if(config.surfaces[0].Format!=D3DFMT_A8R8G8B8||config.surfaces[2].Format!=D3DFMT_A16B16G16R16F){
        b.ReleaseOriginals();return b.Fail("native color format not in fixed contract");}
    if(b.targets.ready()&&b.targets.generation()!=generation){
        if(!b.targets.InvalidateBeforeReset()){b.disabled=true;return b.Fail("old bank retirement failed");}}
    if(!b.targets.ready()){
        if(b.attempted_generation==generation){b.ReleaseOriginals();return b.Fail("allocation already attempted for source generation");}
        b.attempted_generation=generation;
        std::uint64_t estimate=0;
        if(!b.targets.EstimatePayload(config,estimate)||!MemoryHeadroom(estimate,height)||
           !b.targets.Create(device,generation,config)){
            b.ReleaseOriginals();return b.Fail("private allocation/capability/VA headroom rejected");}
        b.layout=config.surfaces;
    } else {
        for(std::size_t i=0;i<5;++i)if(std::memcmp(&b.layout[i],&config.surfaces[i],sizeof(D3DSURFACE_DESC))){
            b.ReleaseOriginals();return b.Fail("native layout changed within fixed generation");}
    }
    CoJIndependentFieldPayload payload;
    for(std::size_t i=0;i<14;++i)payload.textures[i]=Id(b.targets.texture(i,eye==runtime::Eye::right?1:0));
    for(std::size_t i=0;i<5;++i)payload.surfaces[i]=Id(b.targets.surface(static_cast<IndependentEyeSurface>(i)));
    payload.additional_width=config.surfaces[4].Width;payload.additional_height=config.surfaces[4].Height;
    if(!b.tx.Clear()||!StageCoJIndependentRenderFields(b.tx,b.io,s.renderer,recipe,payload)||!b.Roots()||!b.Counts()){
        b.ReleaseOriginals();return b.Fail("native field preimage staging rejected");}
    if(b.production&&!b.queue.Begin(s,generation,++b.scope_serial,eye)){
        b.ReleaseOriginals();return b.Fail(b.queue.awaiting_natural_completion()?
            "awaiting natural queue completion":"native queue admission failed");
    }
    b.queue_open=b.production;b.queue_ok=true;
    // All allocations, retained original interfaces and native snapshots exist
    // before the first COM bind or native write. No native factory is called.
    if(!b.targets.Begin(device,generation)){
        if(b.queue_open){b.queue_ok=b.queue.Complete();b.queue_open=false;}
        if(b.targets.active()){b.scope=true;(void)End();}
        if(!b.scope)b.ReleaseOriginals();return b.Fail("GPU scope bind/restoration failed");}
    b.scope=true;
    if(!b.Unbind()||!b.tx.Apply()){
        (void)End();return b.Fail("native scope apply rejected; restoration attempted");}
    b.ended_right=eye==runtime::Eye::right;b.error="";return true;
}
bool CoJIndependentRenderScope::End() noexcept {
    auto& b=*impl_;
    if(!b.scope&&b.targets.active()&&!b.tx.restoration_pending()&&b.thread==GetCurrentThreadId()){
        // A mirror-only failure owns a GPU snapshot but no native field scope.
        const bool restored=b.targets.RestoreAll();
        if(restored)b.ReleaseOriginals();
        return restored?true:b.Fail("mirror GPU rollback remains pending");
    }
    if(!b.scope||b.thread!=GetCurrentThreadId())return b.Fail("no owned eye scope to restore");
    std::uint32_t target=~0u;
    const bool complete=b.Read(b.renderer+0x1db4,target)&&target==0;
    if(!b.Roots()||!b.Counts()){b.disabled=true;return b.Fail("native owner changed; rollback retained in quarantine");}
    if(b.queue_open){b.queue_ok=b.queue.Complete();b.queue_open=false;}
    const bool unbound=!b.targets.active()||b.Unbind();
    const bool native=b.tx.Restore();
    const bool gpu=b.targets.active()?b.targets.End():true;
    if(!native||!gpu){b.disabled=true;return b.Fail("native/GPU rollback pending; saved resources retained");}
    b.scope=false;
    std::uint32_t dirty=0;
    bool invalidated=b.Read(b.renderer+0x1bc,dirty);dirty|=0x00622000;
    invalidated=invalidated&&b.io.write(b.io.context,b.renderer+0x1bc,&dirty,sizeof(dirty));
    b.originals={}; // GPU commands/transport retain their own COM references.
    if(!complete||!unbound||!invalidated||!b.queue_ok){b.ended_right=false;return b.Fail("incomplete eye rejected after restored scope");}
    b.error="";return true;
}
bool CoJIndependentRenderScope::Mirror() noexcept {
    auto& b=*impl_;
    if(b.scope||b.disabled||!b.ended_right||!b.surfaces[0])return b.Fail("mirror pair/rollback not ready");
    b.ended_right=false;
    const bool ok=b.targets.MirrorRightTo(b.surfaces[0].Get());
    b.ReleaseOriginals();return ok?true:b.Fail("desktop mirror failed");
}
bool CoJIndependentRenderScope::ClearHistories() noexcept {
    auto& b=*impl_;return !b.scope&&!b.tx.restoration_pending()&&
        (!b.targets.ready()||b.targets.ClearHistories());
}
bool CoJIndependentRenderScope::InvalidateBeforeReset() noexcept {
    auto& b=*impl_;
    if(restoration_pending()&&!End()&&restoration_pending())return false;
    if(b.tx.restoration_pending()||!b.targets.InvalidateBeforeReset())return b.Fail("reset deferred: private resources still owned");
    b.ReleaseOriginals();b.disabled=false;b.ended_right=false;
    // Even failed Reset attempts retire DEFAULT resources. A new logical
    // admission may recreate the bank without changing the fixed output bank.
    b.attempted_generation=0;return true;
}
bool CoJIndependentRenderScope::Shutdown() noexcept {
    const bool retired=InvalidateBeforeReset();
    return impl_->queue.Restore()&&retired;
}
bool CoJIndependentRenderScope::active() const noexcept{return impl_->scope;}
bool CoJIndependentRenderScope::restoration_pending() const noexcept{
    return impl_->scope||impl_->tx.restoration_pending()||impl_->targets.active();
}
std::uint64_t CoJIndependentRenderScope::payload_bytes() const noexcept{return impl_->targets.payload_bytes();}
void CoJIndependentRenderScope::RecordNativeDraw(IDirect3DDevice9* device,std::uintptr_t caller) noexcept {
    auto& b=*impl_;
    if(!b.scope||b.thread!=GetCurrentThreadId()||device!=b.device.Get()||
       caller<b.engine||caller-b.engine>=0x300000)return;
    ++b.evidence.draws;
    if(b.evidence.full_extent_depth_draws)return;
    ComPtr<IDirect3DSurface9> color,depth;
    D3DSURFACE_DESC c{},d{};D3DVIEWPORT9 v{};DWORD z=0;
    if(FAILED(device->GetRenderTarget(0,&color))||!color||FAILED(color->GetDesc(&c))||
       FAILED(device->GetDepthStencilSurface(&depth))||!depth||FAILED(depth->GetDesc(&d))||
       FAILED(device->GetViewport(&v))||FAILED(device->GetRenderState(D3DRS_ZENABLE,&z))||!z||
       c.Width!=b.recipe.width||c.Height!=b.recipe.height||d.Width<c.Width||d.Height<c.Height||
       v.X||v.Y||v.Width!=c.Width||v.Height!=c.Height)return;
    b.evidence.full_extent_depth_draws=1;
    b.evidence.color_width=c.Width;b.evidence.color_height=c.Height;
    b.evidence.depth_width=d.Width;b.evidence.depth_height=d.Height;
    b.evidence.viewport_width=v.Width;b.evidence.viewport_height=v.Height;
}
CoJIndependentRenderEvidence CoJIndependentRenderScope::evidence() const noexcept{return impl_->evidence;}
std::string_view CoJIndependentRenderScope::last_error() const noexcept{return impl_->error;}
}
