#include "backends/d3d9/render_resource_probe.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include <wrl/client.h>
#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
using namespace cojvr::backends::d3d9;
using Microsoft::WRL::ComPtr;
namespace {
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Output { unsigned begins=0; std::vector<D3D9RenderResourceRecord> records; std::vector<D3D9RenderResourceSummary> ends;
    std::vector<D3D9RenderResourceContext> contexts; };
std::vector<RenderResourceEvent> hook_events;
unsigned call_enters=0,call_leaves=0;
void HookCall(IDirect3DDevice9*,bool entering) noexcept {
    if(entering)++call_enters;else ++call_leaves;SetLastError(0xBAD);
}
void HookEvent(IDirect3DDevice9*,const RenderResourceEvent& event) noexcept {
    hook_events.push_back(event);SetLastError(0xBAD);
}
void Sink(void* ctx,const D3D9RenderResourceContext& context,const D3D9RenderResourceRecord* record,
          const D3D9RenderResourceSummary* summary) noexcept {
    auto& out=*static_cast<Output*>(ctx);
    out.contexts.push_back(context);
    if(record)out.records.push_back(*record);else if(summary)out.ends.push_back(*summary);else ++out.begins;
}
}
int main(int argc,char**){
    const bool use_ex=argc>1;
    RenderResourceCallBoundary boundary;
    auto* identity=reinterpret_cast<IDirect3DDevice9*>(0x1234);
    boundary.Enter(identity);
    Check(!boundary.Begin(identity),"in-flight ingress before scope was admitted");
    boundary.Leave();
    Check(boundary.Begin(identity),"idle call boundary rejected");
    std::thread foreign_ingress([&]{boundary.Enter(identity);});foreign_ingress.join();
    Check(!boundary.End(identity),"foreign native call pending callback retained completion");
    boundary.Leave();
    Check(boundary.Begin(identity)&&boundary.End(identity),"healthy boundary failed to recover");
    Check(!ObserveD3D9RenderResources(nullptr).valid,"null device must not produce valid observation");
    HWND window=CreateWindowExW(0,L"STATIC",L"cojvr resource probe host",WS_OVERLAPPEDWINDOW,
        0,0,160,120,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"host window creation failed");
    auto create=SystemDirect3DCreate9();Check(create!=nullptr,"system D3D9 unavailable");
    ComPtr<IDirect3D9> factory;factory.Attach(create(D3D_SDK_VERSION));Check(factory!=nullptr,"factory failed");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;pp.BackBufferWidth=128;pp.BackBufferHeight=96;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9> device;
    HRESULT hr=S_OK;ComPtr<IDirect3DDevice9Ex> device_ex;
    if(use_ex) {
        ComPtr<IDirect3D9Ex> factory_ex;auto create_ex=SystemDirect3DCreate9Ex();
        Check(create_ex&&SUCCEEDED(create_ex(D3D_SDK_VERSION,&factory_ex)),"Ex factory failed");
        hr=factory_ex->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,nullptr,&device_ex);
        if(SUCCEEDED(hr))device=device_ex;
    } else hr=factory->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
    if(hr==D3DERR_NOTAVAILABLE){DestroyWindow(window);return 77;}
    Check(SUCCEEDED(hr)&&device,"host D3D9 device failed");
    ComPtr<IDirect3DSurface9> native;Check(SUCCEEDED(device->GetRenderTarget(0,&native)),"native RT missing");
    ComPtr<IDirect3DTexture9> texture;Check(SUCCEEDED(device->CreateTexture(64,48,1,D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)),"texture failed");
    ComPtr<IDirect3DSurface9> target,depth;
    Check(SUCCEEDED(texture->GetSurfaceLevel(0,&target)),"texture alias failed");
    Check(SUCCEEDED(device->CreateDepthStencilSurface(64,48,D3DFMT_D16,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr)),"depth failed");
    Check(SUCCEEDED(device->SetRenderTarget(0,target.Get()))&&SUCCEEDED(device->SetDepthStencilSurface(depth.Get())),"bind failed");
    // Bind the same texture as input just for passive state observation; no draw uses this invalid feedback combination.
    Check(SUCCEEDED(device->SetTexture(3,texture.Get())),"sampler bind failed");
    Check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xFF123456,1,0)),"pixel sentinel clear failed");
    const D3DVIEWPORT9 viewport{2,3,60,42,.1F,.9F};const RECT scissor{4,5,58,40};
    Check(SUCCEEDED(device->SetViewport(&viewport))&&SUCCEEDED(device->SetScissorRect(&scissor)),"viewport/scissor failed");
    Check(SUCCEEDED(device->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE)),"scissor state failed");
    ComPtr<IDirect3DSurface9> readback;
    Check(SUCCEEDED(device->CreateOffscreenPlainSurface(64,48,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)),"readback allocation failed");
    SetLastError(0x7134);const auto observed=ObserveD3D9RenderResources(device.Get());
    Check(GetLastError()==0x7134,"observation changed caller last-error");
    Check(observed.valid&&observed.targets[0].bound&&observed.targets[0].width==64&&
        observed.targets[0].height==48&&observed.depth.bound&&observed.depth.format==D3DFMT_D16,"wrong scene descriptors");
    Check(observed.textures[3].bound&&observed.textures[3].surface==observed.targets[0].object&&
        observed.textures[3].object==observed.targets[0].parent,"texture/surface alias not preserved");
    Check(observed.viewport==std::array<DWORD,4>{2,3,60,42}&&observed.scissor==std::array<LONG,4>{4,5,58,40}&&
        observed.scissor_enabled==TRUE,"effective viewport/scissor lost");
    Check(ObserveD3D9RenderResources(device.Get())==observed,"reading changed graphics state");
    Output out;D3D9RenderResourceProbe probe(&Sink,&out,2);
    D3D9RenderResourceContext context{};context.device=reinterpret_cast<std::uintptr_t>(device.Get());
    context.generation=1;context.frame=4;context.owner=3;context.eye=0;
    context.device_generation=2;context.generation=7; // Different counter namespaces.
    auto invalid_context=context;invalid_context.frame=0;
    Check(!probe.Begin(device.Get(),invalid_context),"zero frame admitted");
    invalid_context=context;invalid_context.generation=0;
    Check(!probe.Begin(device.Get(),invalid_context),"zero generation admitted");
    invalid_context=context;invalid_context.device+=4;
    Check(!probe.Begin(device.Get(),invalid_context),"wrong device identity admitted");
    Check(!probe.Begin(nullptr,context)&&out.begins==0,"invalid Begin emitted an admitted scope");
    Check(probe.Begin(device.Get(),context),"valid scope rejected");
    const RenderResourceEvent event{RenderResourceOperation::draw,0,S_OK,0x1234,0,0};
    Check(probe.Record(device.Get(),event)&&probe.Record(device.Get(),event),"record failed");
    Check(probe.End(device.Get(),true),"valid scope failed to finish");
    Check(out.contexts.size()==3,"missing begin/record/end context");
    for(const auto& emitted:out.contexts)
        Check(emitted.generation==7&&emitted.device_generation==2,
            "device generation was lost or confused with probe continuity epoch");
    Check(out.begins==1&&out.records.size()==1&&out.records[0].repeat==2&&out.ends.size()==1&&
        out.ends[0].complete&&out.ends[0].calls==2,"coalescing lost a draw or completion");
    Check(!probe.End(device.Get(),true),"orphan End succeeded");
    Check(probe.Begin(device.Get(),context),"mismatch scope rejected");
    Check(!probe.Record(nullptr,event),"foreign/null device admitted");
    Check(!probe.End(device.Get(),true)&&!out.ends.back().complete,"rejected device retained completion");
    Check(probe.Begin(device.Get(),context),"foreign-thread scope rejected");
    bool foreign_record=true,foreign_end=true;
    std::thread foreign([&]{foreign_record=probe.Record(device.Get(),event);foreign_end=probe.End(device.Get(),true);});foreign.join();
    Check(!foreign_record&&!foreign_end,"foreign thread modified native-eye scope");
    Check(!probe.End(device.Get(),true)&&!out.ends.back().complete,"foreign thread retained completion");

    Check(probe.Begin(device.Get(),context),"second scope failed");
    for(unsigned i=0;i<3;++i){auto unique=event;unique.selector=i;probe.Record(device.Get(),unique);}
    Check(!probe.End(device.Get(),true)&&out.ends.back().truncated&&!out.ends.back().complete&&
        out.ends.back().calls==3&&out.ends.back().records==2,"overflow manufactured complete trace");
    Check(probe.Begin(device.Get(),context),"cancel scope failed");
    Check(!probe.End(device.Get(),false)&&!out.ends.back().complete,"cancel manufactured completion");
    Check(probe.Begin(device.Get(),context),"abandoned scope failed");
    Check(!probe.Begin(device.Get(),context),"nested scope overwrote active evidence");
    Check(!probe.End(device.Get(),true)&&!out.ends.back().complete,"nested scope retained completion authority");
    Check(ObserveD3D9RenderResources(device.Get())==observed,"probe mutated target state");
    Check(SUCCEEDED(device->GetRenderTargetData(target.Get(),readback.Get())),"pixel readback failed");
    D3DLOCKED_RECT pixels{};Check(SUCCEEDED(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY)),"pixel lock failed");
    for(unsigned y=0;y<48;++y)for(unsigned x=0;x<64;++x)
        Check(reinterpret_cast<const DWORD*>(static_cast<const std::byte*>(pixels.pBits)+y*pixels.Pitch)[x]==0xFF123456,
            "observer changed source pixels");
    readback->UnlockRect();readback.Reset();
    Check(SUCCEEDED(device->SetTexture(3,nullptr))&&SUCCEEDED(device->SetDepthStencilSurface(nullptr)),"unbind failed");
    auto unbound=ObserveD3D9RenderResources(device.Get());
    Check(unbound.valid&&!unbound.depth.bound&&!unbound.textures[3].bound,"valid unbound differs from observation failure");
    Check(SUCCEEDED(device->SetRenderTarget(0,native.Get())),"native target restore failed");
    target.Reset();texture.Reset();depth.Reset();native.Reset();
    Check(SUCCEEDED(device->Reset(&pp)),"observer retained DEFAULT COM references across Reset");
    const D3DVIEWPORT9 invalid_viewport{0,0,0,0,0,1};
    const auto invalid=device->SetViewport(&invalid_viewport);const auto native_error=GetLastError();
    DeviceHookCallbacks callbacks{};callbacks.resource_probe_event=&HookEvent;callbacks.resource_probe_call=&HookCall;
    Check(InstallDeviceVtableHook(device.Get(),callbacks),"probe hook install failed");
    SetLastError(native_error);
    Check(device->SetViewport(&invalid_viewport)==invalid&&GetLastError()==native_error,
        "probe changed native HRESULT/last-error");
    Check(hook_events.size()==1&&hook_events.back().operation==RenderResourceOperation::set_viewport&&
        hook_events.back().result==invalid&&hook_events.back().caller!=0,"failed call not observed exactly once");
    Check(call_enters==1&&call_leaves==1,"native-call ingress/retirement not balanced");
    Check(SUCCEEDED(device->SetViewport(&viewport)),"hook changed viewport forwarding");
    D3DVIEWPORT9 forwarded{};device->GetViewport(&forwarded);
    Check(forwarded.X==viewport.X&&forwarded.Height==viewport.Height,"hook changed viewport arguments");
    const auto integrity=InspectResourceProbeHookContinuity(device.Get());
    Check(integrity.intact,"probe slots incomplete after installation");
    for(const auto& slot:InspectAllDeviceVtableHooks())if(slot.index==81) {
        DWORD protection=0;Check(VirtualProtect(&slot.vtable[81],sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"cannot reproduce lost draw hook");
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&slot.vtable[81]),slot.original);
        DWORD ignored=0;Check(VirtualProtect(&slot.vtable[81],sizeof(void*),protection,&ignored)!=FALSE,"cannot restore draw slot protection");
    }
    Check(!InspectResourceProbeHookContinuity(device.Get()).intact,"lost draw hook was declared intact");
    (void)ReacquireDeviceVtableHookDetailed(device.Get());
    const auto recovered=InspectResourceProbeHookContinuity(device.Get());
    Check(recovered.intact&&recovered.epoch>integrity.epoch,"reacquisition did not invalidate active trace epoch");
    const auto before_reset=hook_events.size();
    const auto reset_result=use_ex?device_ex->ResetEx(&pp,nullptr):device->Reset(&pp);
    Check(SUCCEEDED(reset_result),"observed reset failed");
    unsigned reset_events=0;
    for(std::size_t i=before_reset;i<hook_events.size();++i) {
        if(hook_events[i].operation==RenderResourceOperation::reset) {
            ++reset_events;Check(hook_events[i].selector==(use_ex?1u:0u),"wrong reset API observed");
        }
    }
    Check(reset_events==1,"Reset/ResetEx did not invalidate observation exactly once");
    Check(call_enters==call_leaves,"native calls remain in flight after Reset");
    Check(RestoreAllDeviceVtableHooks(),"probe hook restore failed");
    DestroyWindow(window);std::cout<<"read-only resource snapshot and bounded scope passed\n";
}
