#include "backends/d3d9/hdr_alpha_probe.hpp"
#include "backends/d3d9/hdr_source_writes.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "games/call_of_juarez/temporal_source_probe.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
using cojvr::backends::d3d9::D3D9HdrAlphaProbe;
using Microsoft::WRL::ComPtr;
void Check(bool value,const char* message) {if(!value){std::cerr<<message<<'\n';std::exit(1);}}
void CheckProducer(const cojvr::backends::d3d9::HdrSourceWrites& value,std::uintptr_t surface,bool complete,
    std::uint64_t draws,std::uint64_t alpha,std::uint64_t blend,std::uint64_t clears) {
    const auto& p=value.copy_producer;
    Check(p.surface==surface&&value.copy_producer_complete==complete&&p.draw_calls==draws&&
        p.alpha_draw_calls==alpha&&p.alpha_blend_draw_calls==blend&&p.clear_calls==clears,
        "copy producer snapshot lost surface/mask/blend/clear call intents");
}
void CheckFirstBlend(const cojvr::backends::d3d9::HdrProducerWrites& p,std::uintptr_t shader) {
        const auto& b=p.first_alpha_blend;
        Check(b.observed&&b.shader==shader&&b.state_complete&&b.separate_alpha&&
            b.source_blend==D3DBLEND_ZERO&&b.destination_blend==D3DBLEND_ONE&&
            b.blend_operation==D3DBLENDOP_ADD&&b.blend_factor==0x7f112233,
            "first blend draw did not retain effective separate-alpha state");
        Check(b.shader_complete&&b.shader_bytes==20&&b.bytecode[0]==0xffff0200&&
            b.bytecode[1]==0x02000001&&b.bytecode[2]==0x800f0800&&
            b.bytecode[3]==0xa0e40000&&b.bytecode[4]==0x0000ffff,
            "first blend draw lost copied shader bytecode");
}
// Fault only the bytecode getter boundary; render-state reads use the real GPU
// device below. Exercise size rejection before any buffer read is attempted.
class ShaderReadFixture final : public IDirect3DPixelShader9 {
public:
    UINT bytes=20;unsigned queries=0,reads=0;bool fail_query=false,fail_read=false,change_size=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetFunction(void* data,UINT* count) override {
        if(!data){++queries;*count=bytes;return fail_query?E_FAIL:S_OK;}
        ++reads;if(fail_read)return E_FAIL;
        if(*count<bytes)return D3DERR_INVALIDCALL;
        std::memset(data,0,bytes);*count=change_size?bytes-4:bytes;return S_OK;
    }
};
int main() {
    using namespace cojvr::games::call_of_juarez;
    {
        using namespace cojvr::backends::d3d9;
        HdrSourceWrites boundary;
        Check(!HasCurrentEyeHdrCopy(boundary,32,32),"previous-eye/default copy admitted");
        boundary.calls=2;boundary.alpha_calls=2;boundary.copy_calls=1;
        boundary.copy_write_call=2;boundary.source_surface=2;boundary.copy_source=3;
        boundary.copy_source_format=D3DFMT_A16B16G16R16F;
        boundary.copy_source_width=boundary.copy_source_height=32;
        boundary.copy_source_rect=boundary.copy_destination_rect=RECT{0,0,32,32};
        boundary.copy_rectangles_observed=true;
        Check(HasCurrentEyeHdrCopy(boundary,32,32),"complete current-eye HDR copy rejected");
        auto changed=boundary;changed.calls=3;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"copy stayed current after a later write");
        changed=boundary;changed.complete=false;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"incomplete observation admitted");
        changed=boundary;changed.copy_destination_rect.right=16;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"partial destination admitted");
        changed=boundary;changed.copy_source_rect.bottom=16;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"scaled source rectangle admitted");
        changed=boundary;changed.copy_source_width=64;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"cropped foreign extent admitted");
        changed=boundary;changed.copy_source_format=D3DFMT_A8R8G8B8;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"non-HDR source admitted");
        changed=boundary;changed.copy_source=changed.source_surface;
        Check(!HasCurrentEyeHdrCopy(changed,32,32),"self-copy admitted");
        changed={};Check(!HasCurrentEyeHdrCopy(changed,32,32),"eye reset retained source readiness");
    }
    CoJTemporalProbeSchedule schedule;
    Check(!schedule.Acquire(1,1,3,2,true)&&!schedule.Acquire(2000,2,3,2,true),"startup admitted before stable owner");
    Check(schedule.Acquire(2001,3,3,2,true)&&!schedule.Acquire(2002,4,3,2,true),"stable window missing or repeated");
    Check(!schedule.Acquire(12001,3,3,2,true)&&schedule.Acquire(12001,5,3,2,true),"repeated frame consumed a new observation");
    Check(!schedule.Acquire(12002,6,4,3,true)&&!schedule.Acquire(14001,7,4,3,true)&&
        schedule.Acquire(14002,8,4,3,true),"new generation did not requalify its observation window");
    for(unsigned i=0;i<5;++i)Check(schedule.Acquire(24002+i*10000,9+i,4,3,true),"bounded stable window missing");
    Check(!schedule.Acquire(100000,20,4,3,true)&&!schedule.Acquire(1,21,5,4,false)&&
        !schedule.Acquire(2,22,5,4,true)&&!schedule.Acquire(2002,23,5,4,true),"owner/loss reset replenished exhausted budget");
    CoJTemporalProbePhases phases;
    Check(phases.Observe(1,2,0)==1&&!phases.Observe(1,2,0)&&phases.Observe(3,2,0)==2&&
        phases.Observe(3,4,2)==3&&!phases.Observe(5,6,0),"phase collector repeats or exceeds three consumers");
    if(D3D9HdrAlphaProbe::Half(0x3C00)!=1||D3D9HdrAlphaProbe::Half(0x4900)!=10||
       D3D9HdrAlphaProbe::Half(0xBC00)!=-1||D3D9HdrAlphaProbe::Half(0)!=0||
       !std::isnan(D3D9HdrAlphaProbe::Half(0x7E00))||!std::isinf(D3D9HdrAlphaProbe::Half(0x7C00)))return 1;
    D3D9HdrAlphaProbe probe;cojvr::backends::d3d9::HdrAlphaSample sample{};
    if(probe.Queue(nullptr,nullptr,sample)||probe.Poll(sample))return 2;
    const auto window=CreateWindowExW(0,L"STATIC",L"HDR diagnostic fixture",WS_OVERLAPPEDWINDOW,0,0,128,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"fixture window failed");
    const auto create=cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    ComPtr<IDirect3D9Ex> factory;ComPtr<IDirect3DDevice9Ex> device;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=128;pp.BackBufferHeight=96;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if(!create||FAILED(create(D3D_SDK_VERSION,&factory))||FAILED(factory->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,nullptr,&device))) {
        DestroyWindow(window);std::cout<<"SKIP: D3D9Ex HAL unavailable\n";return 77;
    }
    ComPtr<IDirect3DTexture9> source;ComPtr<IDirect3DSurface9> surface,upload;
    Check(SUCCEEDED(device->CreateTexture(32,32,1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,&source,nullptr)),"FP16 texture failed");
    Check(SUCCEEDED(source->GetSurfaceLevel(0,&surface))&&SUCCEEDED(device->CreateOffscreenPlainSurface(32,32,D3DFMT_A16B16G16R16F,D3DPOOL_SYSTEMMEM,&upload,nullptr)),"fixture surfaces failed");
    {
        using namespace cojvr::backends::d3d9;
        ComPtr<IDirect3DSurface9> original_target;DWORD original_mask=0;
        Check(SUCCEEDED(device->GetRenderTarget(0,&original_target))&&
            SUCCEEDED(device->GetRenderState(D3DRS_COLORWRITEENABLE,&original_mask)),"write witness sentinel unavailable");
        Check(SUCCEEDED(device->SetRenderTarget(0,surface.Get())),"source target bind failed");
        HdrSourceWrites writes;
        const auto identity=reinterpret_cast<std::uintptr_t>(source.Get());
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_TARGET},writes);
        Check(writes.calls==1&&writes.alpha_calls==1&&writes.complete&&
            writes.source_surface==reinterpret_cast<std::uintptr_t>(surface.Get()),"source clear call witness missing");
        Check(SUCCEEDED(device->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED)),"RGB mask failed");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::draw},writes);
        Check(writes.calls==2&&writes.alpha_calls==1,"RGB-only draw became alpha provenance");
        Check(SUCCEEDED(device->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA)),"alpha mask failed");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::draw},writes);
        Check(writes.calls==3&&writes.alpha_calls==2,"alpha draw intent lost");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::draw,0,E_FAIL},writes);
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_ZBUFFER},writes);
        ObserveHdrSourceWrites(device.Get(),identity+1,32,32,{RenderResourceOperation::draw},writes);
        Check(writes.calls==3&&writes.complete,"failed/depth-only/foreign calls became source writes");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::stretch_rect,0,S_OK,0,0,
            reinterpret_cast<std::uintptr_t>(surface.Get())},writes);
        Check(writes.calls==3&&writes.alpha_calls==2,"value-only copy event was dereferenced or double-counted");
        ComPtr<IDirect3DSurface9> copy_source;
        Check(SUCCEEDED(device->CreateRenderTarget(32,32,D3DFMT_A16B16G16R16F,D3DMULTISAMPLE_NONE,0,FALSE,&copy_source,nullptr)),"copy source allocation failed");
        // Observe real successful RT0 calls to the plain scene surface before
        // it becomes a typed copy source. No parent texture identity exists.
        ComPtr<IDirect3DStateBlock9> producer_state;
        Check(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&producer_state))&&
            SUCCEEDED(device->SetRenderTarget(0,copy_source.Get())),"producer state setup failed");
        const auto cleared=device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
        Check(SUCCEEDED(cleared),"producer clear failed");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_TARGET,cleared},writes);
        struct Vertex {float x,y,z,w;DWORD color;};
        const Vertex triangle[]={{0,0,.5F,1,0xffffffff},{32,0,.5F,1,0xffffffff},{0,32,.5F,1,0xffffffff}};
        Check(SUCCEEDED(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE))&&
            SUCCEEDED(device->SetPixelShader(nullptr))&&SUCCEEDED(device->SetRenderState(D3DRS_ZENABLE,FALSE))&&
            SUCCEEDED(device->BeginScene()),"producer draw setup failed");
        const DWORD program[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
        ComPtr<IDirect3DPixelShader9> blend_shader;
        Check(SUCCEEDED(device->CreatePixelShader(program,&blend_shader))&&
            SUCCEEDED(device->SetPixelShader(blend_shader.Get()))&&
            SUCCEEDED(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA))&&
            SUCCEEDED(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA))&&
            SUCCEEDED(device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,TRUE))&&
            SUCCEEDED(device->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO))&&
            SUCCEEDED(device->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ONE))&&
            SUCCEEDED(device->SetRenderState(D3DRS_BLENDOPALPHA,D3DBLENDOP_ADD))&&
            SUCCEEDED(device->SetRenderState(D3DRS_BLENDFACTOR,0x7f112233)),"blend diagnostic state setup failed");
        for(const auto mask:{DWORD(D3DCOLORWRITEENABLE_RED),DWORD(D3DCOLORWRITEENABLE_ALPHA)}) {
            Check(SUCCEEDED(device->SetRenderState(D3DRS_COLORWRITEENABLE,mask))&&
                SUCCEEDED(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE)),"producer mask setup failed");
            const auto drawn=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(Vertex));
            Check(SUCCEEDED(drawn),"producer draw failed");
            ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::draw_up,0,drawn},writes);
        }
        // Ordinary color blend states apply to alpha when separate alpha is off.
        Check(SUCCEEDED(device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE)),"ordinary blend state failed");
        auto ordinary=ReadHdrAlphaBlend(device.Get(),blend_shader.Get());
        Check(ordinary.state_complete&&!ordinary.separate_alpha&&ordinary.source_blend==D3DBLEND_SRCALPHA&&
            ordinary.destination_blend==D3DBLEND_INVSRCALPHA&&ordinary.blend_operation==D3DBLENDOP_ADD,
            "disabled separate alpha read stale alpha-specific factors");
        auto fixed=ReadHdrAlphaBlend(device.Get(),nullptr);
        Check(fixed.observed&&fixed.shader_complete&&!fixed.shader&&!fixed.shader_bytes,"fixed function became unknown shader data");
        for(const UINT bytes:{0U,3U,4096U,4100U}) {
            ShaderReadFixture reader;reader.bytes=bytes;
            auto bounded_program=ReadHdrAlphaBlend(device.Get(),&reader);
            Check(reader.queries==1&&reader.reads==(bytes==4096?1U:0U)&&
                bounded_program.shader_complete==(bytes==4096)&&bounded_program.shader_bytes==bytes,
                "program reader exceeded its fixed buffer or lost oversize/invalid-size uncertainty");
        }
        for(unsigned fault=0;fault<3;++fault) {
            ShaderReadFixture reader;reader.fail_query=fault==0;reader.fail_read=fault==1;reader.change_size=fault==2;
            auto failed=ReadHdrAlphaBlend(device.Get(),&reader);
            Check(!failed.shader_complete&&reader.queries==1&&reader.reads==(fault?1U:0U),
                "failed/short shader read claimed a complete program or retried");
        }
        Check(SUCCEEDED(device->EndScene())&&SUCCEEDED(producer_state->Apply())&&
            SUCCEEDED(device->SetRenderTarget(0,surface.Get())),"producer state restore failed");
        const auto copied=device->StretchRect(copy_source.Get(),nullptr,surface.Get(),nullptr,D3DTEXF_POINT);
        Check(SUCCEEDED(copied),"native fixture source copy failed");
        ObserveHdrSourceCopy(device.Get(),identity,32,32,copy_source.Get(),nullptr,surface.Get(),nullptr,copied,writes);
        Check(writes.calls==4&&writes.alpha_calls==3&&writes.copy_calls==1&&writes.copy_rectangles_observed&&
            writes.copy_source_rect.right==32&&writes.copy_destination_rect.bottom==32,"typed source copy/rectangles missing");
        Check(HasCurrentEyeHdrCopy(writes,32,32)&&writes.copy_write_call==4,
            "typed equal-extent GPU copy did not establish scoped readiness");
        CheckProducer(writes,reinterpret_cast<std::uintptr_t>(copy_source.Get()),true,2,1,1,1);
        CheckFirstBlend(writes.copy_producer,reinterpret_cast<std::uintptr_t>(blend_shader.Get()));
        Check(SUCCEEDED(device->SetRenderTarget(0,copy_source.Get())),"producer rebind failed");
        const auto later_clear=device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
        Check(SUCCEEDED(later_clear),"later producer clear failed");
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_TARGET,later_clear},writes);
        CheckProducer(writes,reinterpret_cast<std::uintptr_t>(copy_source.Get()),true,2,1,1,1);
        Check(SUCCEEDED(device->SetRenderTarget(0,surface.Get())),"source target restore failed");
        auto refreshed=writes;
        ObserveHdrSourceCopy(device.Get(),identity,32,32,copy_source.Get(),nullptr,surface.Get(),nullptr,
            device->StretchRect(copy_source.Get(),nullptr,surface.Get(),nullptr,D3DTEXF_POINT),refreshed);
        CheckProducer(refreshed,reinterpret_cast<std::uintptr_t>(copy_source.Get()),true,2,1,1,2);
        HdrSourceWrites bounded;
        std::array<ComPtr<IDirect3DSurface9>,5> producers;
        for(auto& producer:producers) {
            Check(SUCCEEDED(device->CreateRenderTarget(32,32,D3DFMT_A16B16G16R16F,D3DMULTISAMPLE_NONE,0,FALSE,&producer,nullptr))&&
                SUCCEEDED(device->SetRenderTarget(0,producer.Get())),"bounded producer allocation failed");
            const auto result=device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
            Check(SUCCEEDED(result),"bounded producer clear failed");
            ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_TARGET,result},bounded);
        }
        Check(SUCCEEDED(device->SetRenderTarget(0,surface.Get())),"bounded target restore failed");
        const auto bounded_copy=device->StretchRect(producers[0].Get(),nullptr,surface.Get(),nullptr,D3DTEXF_POINT);
        Check(SUCCEEDED(bounded_copy),"bounded native copy failed");
        ObserveHdrSourceCopy(device.Get(),identity,32,32,producers[0].Get(),nullptr,surface.Get(),nullptr,bounded_copy,bounded);
        CheckProducer(bounded,reinterpret_cast<std::uintptr_t>(producers[0].Get()),false,0,0,0,1);
        bounded={};
        ObserveHdrSourceCopy(device.Get(),identity,32,32,producers[0].Get(),nullptr,surface.Get(),nullptr,bounded_copy,bounded);
        CheckProducer(bounded,0,false,0,0,0,0);
        auto overwritten=writes;
        ObserveHdrSourceWrites(device.Get(),identity,32,32,{RenderResourceOperation::clear,D3DCLEAR_TARGET},overwritten);
        Check(!HasCurrentEyeHdrCopy(overwritten,32,32),"later source clear retained copied readiness");
        HdrSourceWrites unknown_copy;
        RECT part{0,0,16,32};
        const auto partial_copy=device->StretchRect(copy_source.Get(),&part,surface.Get(),&part,D3DTEXF_POINT);
        Check(SUCCEEDED(partial_copy),"native fixture partial copy failed");
        ObserveHdrSourceCopy(device.Get(),identity,32,32,copy_source.Get(),&part,surface.Get(),&part,partial_copy,unknown_copy);
        Check(unknown_copy.complete&&unknown_copy.copy_calls==1&&unknown_copy.copy_destination_rect.right==16,
            "typed copy missed first source use or expanded a partial rectangle");
        Check(!HasCurrentEyeHdrCopy(unknown_copy,32,32),"real partial copy admitted a current-eye full copy");
        ObserveHdrSourceCopy(device.Get(),identity,32,32,copy_source.Get(),nullptr,surface.Get(),nullptr,E_FAIL,unknown_copy);
        Check(unknown_copy.calls==1,"failed copy became a source write");
        Check(SUCCEEDED(device->SetRenderTarget(0,original_target.Get()))&&
            SUCCEEDED(device->SetRenderState(D3DRS_COLORWRITEENABLE,original_mask)),"write sentinel restore failed");
        sample.source_write_calls=writes.calls;sample.source_alpha_write_calls=writes.alpha_calls;
        sample.source_write_observation_complete=writes.complete;
        sample.source_copy_calls=writes.copy_calls;sample.source_copy_source=writes.copy_source;
        sample.source_copy_source_format=writes.copy_source_format;
        sample.source_copy_source_width=writes.copy_source_width;sample.source_copy_source_height=writes.copy_source_height;
        sample.source_copy_write_call=writes.copy_write_call;
        sample.source_copy_boundary_current=HasCurrentEyeHdrCopy(writes,32,32);
        sample.source_copy_source_rect=writes.copy_source_rect;sample.source_copy_destination_rect=writes.copy_destination_rect;
        sample.source_copy_rectangles_observed=writes.copy_rectangles_observed;
        sample.source_copy_producer=writes.copy_producer;sample.source_copy_producer_complete=writes.copy_producer_complete;
    }
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(upload->LockRect(&lock,nullptr,0)),"fixture upload lock failed");
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
        auto* p=reinterpret_cast<std::uint16_t*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch)+x*4;
        p[0]=0x3400;p[1]=0x3800;p[2]=0x3c00;p[3]=0x4900; // Co-located .25/.5/1 RGB and ten-metre alpha.
    }
    Check(SUCCEEDED(upload->UnlockRect())&&SUCCEEDED(device->UpdateSurface(upload.Get(),nullptr,surface.Get(),nullptr)),"fixture upload failed");
    // Captured CoJ 1B_16 consumption uses registers 0 and 2, although the
    // material's logical depth sampler is 10. Query the real owned binding.
    ComPtr<IDirect3DTexture9> found;unsigned sampler=~0U;
    for(const auto stage:{0U,2U,10U}) {
        Check(SUCCEEDED(device->SetTexture(stage,source.Get())),"fixture sampler bind failed");
        Check(cojvr::backends::d3d9::FindHdrAlphaSource(device.Get(),reinterpret_cast<std::uintptr_t>(source.Get()),32,32,found,sampler)&&
            found.Get()==source.Get()&&sampler==stage,"native HDR source at actual sampler was missed");
        Check(!cojvr::backends::d3d9::FindHdrAlphaSource(device.Get(),reinterpret_cast<std::uintptr_t>(source.Get()),31,32,found,sampler)&&
            !found&&sampler==~0U,"wrong HDR extent retained a candidate");
        Check(SUCCEEDED(device->SetTexture(stage,nullptr)),"fixture sampler unbind failed");
    }
    Check(!cojvr::backends::d3d9::FindHdrAlphaSource(device.Get(),reinterpret_cast<std::uintptr_t>(source.Get()),32,32,found,sampler)&&
        !found&&sampler==~0U,"unbound copied HDR address authorized a source");
    sample.frame=7;sample.generation=2;sample.scope=5;sample.owner=3;sample.eye=1;
    sample.pose=19;sample.phase=2;sample.sampler=2;sample.target=123;sample.shader=456;
    ComPtr<IDirect3DSurface9> before,after;D3DVIEWPORT9 viewport_before{},viewport_after{};
    Check(SUCCEEDED(device->GetRenderTarget(0,&before))&&SUCCEEDED(device->GetViewport(&viewport_before)),"sentinel state unavailable");
    Check(SUCCEEDED(device->BeginScene()),"fixture BeginScene failed");
    Check(probe.Queue(device.Get(),source.Get(),sample),"FP16 point capture failed");
    Check(probe.Queue(device.Get(),source.Get(),sample)&&!probe.Queue(device.Get(),source.Get(),sample)&&probe.last_result()==S_FALSE,"capture ring not bounded");
    Check(SUCCEEDED(device->EndScene()),"fixture EndScene failed");
    Check(SUCCEEDED(device->GetRenderTarget(0,&after))&&before==after&&SUCCEEDED(device->GetViewport(&viewport_after))&&
        std::memcmp(&viewport_before,&viewport_after,sizeof(viewport_before))==0,"probe changed native target/viewport");
    // Mutate the native texture after Queue: the probe must retain its copied pixels.
    Check(SUCCEEDED(device->ColorFill(surface.Get(),nullptr,0)),"fixture mutation failed");
    // This hidden window may be occluded: Present need not submit an empty
    // scene. Explicit submission belongs to this GPU fixture, not probe Poll.
    ComPtr<IDirect3DQuery9> submission;
    Check(SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT,&submission))&&
        SUCCEEDED(submission->Issue(D3DISSUE_END)),"fixture submission query failed");
    const auto deadline=GetTickCount64()+4000;unsigned received=0;
    while(received<2&&GetTickCount64()<deadline) {
        cojvr::backends::d3d9::HdrAlphaSample result;
        if(probe.Poll(result)) {
            Check(result.frame==7&&result.generation==2&&result.scope==5&&result.owner==3&&result.eye==1&&result.width==32&&result.height==32,"capture provenance lost");
            for(auto alpha:result.alpha)Check(alpha==10,"queued FP16 alpha was normalized, lost or read late");
            for(unsigned i=0;i<256;++i)Check(result.color[i*3]==.25F&&result.color[i*3+1]==.5F&&result.color[i*3+2]==1.F,
                "RGB and alpha did not retain the same source sample");
            Check(result.pose==19&&result.phase==2&&result.sampler==2&&result.target==123&&result.shader==456,
                "temporal source consumer/pose witness lost");
            Check(result.source_write_calls==4&&result.source_alpha_write_calls==3&&result.source_write_observation_complete,
                "source write-call provenance lost across queued copy");
            Check(result.source_copy_calls==1&&result.source_copy_source&&result.source_copy_rectangles_observed&&
                result.source_copy_source_rect.right==32&&result.source_copy_destination_rect.bottom==32&&
                result.source_copy_source_format==D3DFMT_A16B16G16R16F,
                "native copy rectangles crossed the queued sample");
            Check(result.source_copy_boundary_current&&result.source_copy_write_call==4&&
                result.source_copy_source_width==32&&result.source_copy_source_height==32,
                "current-eye source-copy boundary lost across queued pixels");
            const auto& producer=result.source_copy_producer;
            CheckFirstBlend(producer,sample.source_copy_producer.first_alpha_blend.shader);
            Check(result.source_copy_producer_complete&&producer.surface==sample.source_copy_source&&
                producer.draw_calls==2&&producer.alpha_draw_calls==1&&producer.alpha_blend_draw_calls==1&&producer.clear_calls==1,
                "copy-time producer witness crossed queued pixels or absorbed later writes");
            ++received;
        } else {BOOL complete=FALSE;Check(SUCCEEDED(submission->GetData(&complete,sizeof(complete),D3DGETDATA_FLUSH)),"fixture submission failed");Sleep(1);}
    }
    if(received!=2)std::cerr<<"capture count="<<received<<" hr="<<std::hex<<static_cast<unsigned>(probe.last_result())<<'\n';
    Check(received==2,"capture did not retire after fixture submission");
    probe.Reset();Check(!probe.Poll(sample),"reset retained diagnostic sample");
    D3D9HdrAlphaProbe phases_probe{6};
    for(unsigned i=0;i<6;++i) {
        auto phase_sample=sample;phase_sample.eye=i/3;phase_sample.phase=i%3+1;
        Check(phases_probe.Queue(device.Get(),source.Get(),phase_sample),"paired three-phase source did not fit its bounded ring");
    }
    Check(!phases_probe.Queue(device.Get(),source.Get(),sample)&&phases_probe.last_result()==S_FALSE,
        "paired source phase ring exceeded six copies");
    Check(SUCCEEDED(submission->Issue(D3DISSUE_END)),"phase fixture submission failed");
    received=0;const auto phase_deadline=GetTickCount64()+4000;
    while(received<6&&GetTickCount64()<phase_deadline) {
        cojvr::backends::d3d9::HdrAlphaSample result;
        if(phases_probe.Poll(result)) {
            Check(result.eye==received/3&&result.phase==received%3+1,"phase sample crossed an eye/consumer");
            ++received;
        } else {BOOL complete=FALSE;Check(SUCCEEDED(submission->GetData(&complete,sizeof(complete),D3DGETDATA_FLUSH)),"phase fixture submit failed");Sleep(1);}
    }
    Check(received==6,"paired phase queries did not retire");phases_probe.Reset();
    source.Reset();surface.Reset();upload.Reset();before.Reset();after.Reset();device.Reset();factory.Reset();DestroyWindow(window);
    std::cout<<"HDR alpha GPU point capture, bounded ring, copied pixel lifetime, provenance and native-state preservation passed\n";
}
