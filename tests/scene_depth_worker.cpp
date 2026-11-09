#include "backends/openvr/scene_depth_worker.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "games/call_of_juarez/sampled_worker_audit.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

using Microsoft::WRL::ComPtr;
namespace d=cojvr::backends::d3d9;
namespace s=cojvr::backends::openvr::dlss;
using Clock=std::chrono::steady_clock;
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void Check(HRESULT hr,const char* text){Require(SUCCEEDED(hr),text);}
template<class Poll>void Wait(Poll poll,const char* text){
    const auto end=Clock::now()+std::chrono::seconds(25);
    while(!poll()){Require(Clock::now()<end,text);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
float Half(std::uint16_t x){
    const auto e=(x>>10)&31,m=x&1023;
    if(e==31)return std::nanf("");
    return (x&0x8000?-1.F:1.F)*(e?std::ldexp(1.F+m/1024.F,e-15):std::ldexp(float(m),-24));
}
struct Producer {
    ComPtr<IDirect3D9Ex> api;
    ComPtr<IDirect3DDevice9Ex> device;
    ComPtr<IDirect3DTexture9> color;
    ComPtr<IDirect3DSurface9> surface;
    d::D3D9SampledDepthCapture capture;
    std::unique_ptr<d::D3D9SampledDepthCapture> resized_capture;
    unsigned size,height;
    std::uint64_t sequence=0,integrity=1,generation=1,epoch=1;
    Clock::time_point capture_time{};
    explicit Producer(unsigned extent,unsigned image_height=0):size(extent),height(image_height?image_height:extent){
        const auto create=d::SystemDirect3DCreate9Ex();Require(create!=nullptr,"system D3D9Ex absent");
        Check(create(D3D_SDK_VERSION,&api),"create source API");
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow=GetDesktopWindow();pp.BackBufferWidth=size;pp.BackBufferHeight=height;
        Check(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &pp,nullptr,&device),"create source HAL");
        const d::D3D9SampledDepthConfig config{size,height,D3DFMT_A16B16G16R16F,D3DFMT_D24X8,
            D3DMULTISAMPLE_NONE,0,1,true};
        Check(capture.Initialize(device.Get(),config),"source INTZ/FP16 init");
        Check(device->CreateTexture(size,height,1,D3DUSAGE_RENDERTARGET,config.native_color_format,
            D3DPOOL_DEFAULT,&color,nullptr),"native scene color");
        Check(color->GetSurfaceLevel(0,&surface),"native scene surface");
        Check(device->SetRenderTarget(0,surface.Get()),"bind scene target");
        Check(device->SetDepthStencilSurface(capture.native_depth_surface()),"bind scene depth");
        Check(device->SetRenderState(D3DRS_STENCILENABLE,FALSE),"D24X8 stencil off");
    }
    ~Producer(){(void)device->SetDepthStencilSurface(nullptr);}
    void Resize(unsigned new_width,unsigned new_height){
        // The same D3D9Ex owner/device continues across new native capture generations.
        // Retire old render bindings before replacing the producer's extent.
        Check(device->SetDepthStencilSurface(nullptr),"unbind previous source depth");
        ComPtr<IDirect3DSurface9> backbuffer;
        Check(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer),"source backbuffer");
        Check(device->SetRenderTarget(0,backbuffer.Get()),"unbind previous source color");
        surface.Reset();color.Reset();resized_capture.reset();
        size=new_width;height=new_height;++generation;++epoch;
        resized_capture=std::make_unique<d::D3D9SampledDepthCapture>();
        const d::D3D9SampledDepthConfig config{size,height,D3DFMT_A16B16G16R16F,D3DFMT_D24X8,
            D3DMULTISAMPLE_NONE,0,generation,true};
        Check(resized_capture->Initialize(device.Get(),config),"resized source INTZ/FP16 init");
        Check(device->CreateTexture(size,height,1,D3DUSAGE_RENDERTARGET,config.native_color_format,
            D3DPOOL_DEFAULT,&color,nullptr),"resized native scene color");
        Check(color->GetSurfaceLevel(0,&surface),"resized native scene surface");
        Check(device->SetRenderTarget(0,surface.Get()),"bind resized scene target");
        Check(device->SetDepthStencilSurface(resized_capture->native_depth_surface()),"bind resized scene depth");
    }
    s::SceneDepthStamp Stamp(std::uint64_t frame)const{
        return {reinterpret_cast<std::uintptr_t>(device.Get()),generation,frame,frame,42,integrity,epoch};
    }
    std::array<s::SceneDepthEye,2> Produce(std::uint64_t frame,bool expose_background=false){
        capture_time=Clock::now();
        std::array<s::SceneDepthEye,2> pair;
        for(unsigned eye=0;eye<2;++eye){
            Check(device->BeginScene(),"begin source scene");
            const D3DCOLOR base_color=eye?0xff2244cc:0xffcc4422;
            const D3DCOLOR color_now=expose_background&&frame%2==0?(eye?0xff22cc44:0xff44cc22):base_color;
            Check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                color_now,eye?.75F:.25F,0),"native color/depth scene");
            if(expose_background&&frame==1){
                const D3DRECT rect{LONG(size/2)-24,LONG(height/2)-24,LONG(size/2)+24,LONG(height/2)+24};
                Check(device->Clear(1,&rect,D3DCLEAR_ZBUFFER,0,.125F,0),"previous native occluder");
            }
            pair[eye].stamp=Stamp(frame);pair[eye].eye=eye;
            pair[eye].capture_time=capture_time;
            auto& active_capture=resized_capture?*resized_capture:capture;
            Check(active_capture.Snapshot(++sequence,generation,pair[eye].snapshot),"immutable source capture");
            Check(device->EndScene(),"end source scene");
            Wait([&]{const auto hr=pair[eye].snapshot.PollReady();Check(hr,"source query");return hr==S_OK;},"source query timeout");
        }
        return pair;
    }
};
struct Control {
    std::mutex mutex;std::condition_variable cv;
    bool allow_start=false,allow_evaluate=false;
    std::atomic_bool started=false,evaluating=false;
    std::atomic<unsigned> evaluations=0,worker_thread=0,stop_thread=0;
    std::atomic<int> fail_eye=-1;
    void Release(){std::lock_guard lock(mutex);allow_start=allow_evaluate=true;cv.notify_all();}
};
struct ReleaseOnExit{std::shared_ptr<Control> state;~ReleaseOnExit(){state->Release();}};
class ControlledExecutor final:public s::PairExecutor {
    std::shared_ptr<Control> control_;
    ComPtr<ID3D11Device> device_;
    std::array<ComPtr<ID3D11Texture2D>,2> outputs_;
public:
    explicit ControlledExecutor(std::shared_ptr<Control> c):control_(std::move(c)){}
    bool Start(ID3D11Device* device,const std::filesystem::path&)noexcept override{
        device_=device;control_->worker_thread=GetCurrentThreadId();control_->started=true;
        std::unique_lock lock(control_->mutex);control_->cv.wait(lock,[&]{return control_->allow_start;});return true;
    }
    bool Build(unsigned eye,const cojvr::runtime::TemporalFrameIdentity& f)noexcept override{
        if(GetCurrentThreadId()!=control_->worker_thread)return false;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=f.output_width;desc.Height=f.output_height;
        desc.ArraySize=desc.MipLevels=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
        return SUCCEEDED(device_->CreateTexture2D(&desc,nullptr,outputs_[eye].ReleaseAndGetAddressOf()));
    }
    bool Generate(ID3D11Device*,ID3D11DeviceContext*,const std::array<s::GuideSource,2>& source,
        std::array<s::Inputs,2>& out)noexcept override{
        // Actor/fault fixture only; real guide generation has independent GPU coverage.
        for(unsigned eye=0;eye<2;++eye)out[eye]={source[eye].color,source[eye].linear_depth_alpha,
            source[eye].color,source[eye].current_color_mask};
        return true;
    }
    bool Evaluate(ID3D11DeviceContext* context,unsigned eye,const s::Inputs& in)noexcept override{
        control_->evaluating=true;++control_->evaluations;
        std::unique_lock lock(control_->mutex);control_->cv.wait(lock,[&]{return control_->allow_evaluate;});
        if(GetCurrentThreadId()!=control_->worker_thread||control_->fail_eye==int(eye))return false;
        context->CopyResource(outputs_[eye].Get(),in.color);return true;
    }
    ID3D11Texture2D* Output(unsigned eye)const noexcept override{return outputs_[eye].Get();}
    std::string_view last_error()const noexcept override{return "controlled worker failure";}
    void Stop()noexcept override{control_->stop_thread=GetCurrentThreadId();outputs_={};device_.Reset();}
};
s::SceneWorkerRequest Request(const Producer& p,s::SceneWorkerAuthority authority,bool reconstruct=true){
    s::SceneWorkerRequest r;r.authority=authority;r.capture_time=p.capture_time;
    r.max_age=std::chrono::seconds(5);r.reconstruct=reconstruct;
    r.output_width=p.size;r.output_height=p.height;r.quality=s::Quality::dlaa;
    for(auto& eye:r.guides){eye.source_qualified=true;eye.near_metres=.05F;eye.far_metres=200;
        eye.current_color_mask.assign(p.size*p.height,255);}
    return r;
}
void ControlledWorker(){
    Producer producer(64);auto control=std::make_shared<Control>();
    s::SceneDepthWorker worker(std::make_unique<ControlledExecutor>(control));
    ReleaseOnExit release{control};
    s::SceneWorkerAuthority authority{1,1,1,true,true};
    Require(worker.Start(producer.device.Get(),{L"E:/controlled-worker.exe",1000}),"async worker did not start");
    Require(worker.SetAuthority(authority),"worker authority rejected");
    Wait([&]{return control->started.load();},"worker startup did not reach executor");
    Require(worker.status().phase==s::SceneWorkerPhase::starting,"slow startup escaped worker thread");
    auto eyes=producer.Produce(1);
    Require(!worker.Submit(eyes,Request(producer,authority)),"accepted source during slow startup");
    {std::lock_guard lock(control->mutex);control->allow_start=true;control->cv.notify_all();}
    Wait([&]{return worker.status().phase==s::SceneWorkerPhase::idle;},"worker did not become idle");
    Require(worker.Submit(eyes,Request(producer,authority)),"worker rejected source pair");eyes={};
    Wait([&]{return control->evaluating.load();},"worker did not enter slow evaluation");
    s::SceneWorkerReport report;report.width=777;
    Require(!worker.Poll(producer.Stamp(1),report)&&report.width==777,"published unfinished inference");
    Require(worker.status().source_pairs_retired==1,"inference retained native source leases");
    auto reused=producer.Produce(2); // Both slots MUST be reusable while inference is blocked.
    Require(!worker.Submit(reused,Request(producer,authority)),"overwrote active job");reused={};
    auto blocked=authority;blocked.available=false;
    Require(worker.SetAuthority(blocked)&&worker.SetAuthority(authority),"authority cycle failed");
    control->Release();
    Wait([&]{return worker.Poll(producer.Stamp(2),report);},"cancelled worker result absent");
    Require(report.outcome==s::SceneWorkerOutcome::cancelled,"restored authority revived stale result");
    ++authority.settings_revision;Require(worker.SetAuthority(authority),"new settings rejected");
    eyes=producer.Produce(3);Require(worker.Submit(eyes,Request(producer,authority)),"second job rejected");eyes={};
    Wait([&]{return worker.Poll(producer.Stamp(4),report);},"second result absent");
    Require(report.outcome==s::SceneWorkerOutcome::reconstructed&&report.source.frame==3&&
        report.center_rgba16f[0]!=report.center_rgba16f[1],"completed pair lost identity/eyes");
    Require(report.worker_thread==control->worker_thread&&report.worker_thread!=GetCurrentThreadId(),
        "NGX work ran on producer or changed worker thread");
    eyes=producer.Produce(4);
    auto unqualified=Request(producer,authority);unqualified.guides[1].source_qualified=false;
    Require(!worker.Submit(eyes,std::move(unqualified)),"unqualified guide source reached inference");
    auto missing=Request(producer,authority);missing.guides[0].current_color_mask.clear();
    Require(!worker.Submit(eyes,std::move(missing)),"worker invented a missing motion mask");
    auto invalid_mask=Request(producer,authority);invalid_mask.depth_mask.enabled=true;invalid_mask.depth_mask.erosion_radius=3;
    Require(!worker.Submit(eyes,std::move(invalid_mask)),"invalid GPU mask policy reached worker");
    auto audit_mask=Request(producer,authority,false);audit_mask.depth_mask.enabled=true;
    Require(!worker.Submit(eyes,std::move(audit_mask)),"copy-only audit admitted reconstruction mask policy");
    auto future=Request(producer,authority);future.capture_time+=std::chrono::seconds(10);
    Require(!worker.Submit(eyes,std::move(future)),"future capture timestamp admitted");
    auto obsolete=Request(producer,authority);++obsolete.authority.input_context;
    Require(!worker.Submit(eyes,std::move(obsolete)),"wrong input authority admitted");
    auto restamped=Request(producer,authority);restamped.capture_time+=std::chrono::nanoseconds(1);
    Require(!worker.Submit(eyes,std::move(restamped)),"old capture was re-stamped at submission");
    Require(worker.Submit(eyes,Request(producer,authority,false)),"audit request rejected");eyes={};
    Wait([&]{return worker.Poll(producer.Stamp(4),report);},"audit result absent");
    Require(report.outcome==s::SceneWorkerOutcome::copied,"audit was mislabeled reconstruction");
    control->evaluating=false;{std::lock_guard lock(control->mutex);control->allow_evaluate=false;}
    eyes=producer.Produce(5);auto expiry=Request(producer,authority);expiry.max_age=std::chrono::seconds(1);
    Require(worker.Submit(eyes,std::move(expiry)),"expiry job rejected");eyes={};
    Wait([&]{return control->evaluating.load();},"expiry fixture did not enter evaluation");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));control->Release();
    Wait([&]{return worker.Poll(producer.Stamp(5),report);},"expiry result absent");
    Require(report.outcome==s::SceneWorkerOutcome::expired,"late result published");
    control->fail_eye=1;eyes=producer.Produce(6);
    Require(worker.Submit(eyes,Request(producer,authority)),"failure job rejected");eyes={};
    Wait([&]{return worker.Poll(producer.Stamp(6),report);},"failure result absent");
    Require(report.outcome==s::SceneWorkerOutcome::failed&&report.center_rgba16f==decltype(report.center_rgba16f){},
        "partial eye output escaped failed job");
    control->fail_eye=-1;control->evaluating=false;
    {std::lock_guard lock(control->mutex);control->allow_evaluate=false;}
    eyes=producer.Produce(7);Require(worker.Submit(eyes,Request(producer,authority)),"unknown authority fixture submit");eyes={};
    Wait([&]{return control->evaluating.load();},"unknown authority fixture did not evaluate");
    auto unknown=authority;unknown.settings_revision=0;
    Require(!worker.SetAuthority(unknown),"unknown available authority admitted");
    control->Release();
    Wait([&]{return worker.Poll(producer.Stamp(7),report);},"unknown authority result absent");
    Require(report.outcome==s::SceneWorkerOutcome::cancelled,"invalid authority retained old permission");
    Require(worker.SetAuthority(authority),"valid authority restore failed");
    for(unsigned frame=8;frame<=10;++frame){
        if(frame==10)++producer.integrity;
        eyes=producer.Produce(frame);
        Require(worker.Submit(eyes,Request(producer,authority)),"history fixture submit failed");eyes={};
        Wait([&]{return worker.Poll(producer.Stamp(frame),report);},"history fixture result absent");
        Require(report.outcome==s::SceneWorkerOutcome::reconstructed&&report.history_reused==(frame==9),
            "worker history ignored source integrity or adjacent identity");
    }
    eyes=producer.Produce(11);
    auto late=Request(producer,authority);late.max_age=std::chrono::seconds(1);
    const auto delivery_deadline=late.capture_time+late.max_age+std::chrono::milliseconds(1);
    Require(worker.Submit(eyes,std::move(late)),"late-delivery fixture submit failed");eyes={};
    Wait([&]{return worker.status().phase==s::SceneWorkerPhase::completed;},"completed reconstruction absent");
    std::this_thread::sleep_until(delivery_deadline);
    Require(worker.Poll(producer.Stamp(11),report)&&report.outcome==s::SceneWorkerOutcome::expired&&
        !report.history_reused,"expired delivery preserved a successful-history report");
    eyes=producer.Produce(12);
    Require(worker.Submit(eyes,Request(producer,authority)),"post-expiry fixture submit failed");eyes={};
    Wait([&]{return worker.Poll(producer.Stamp(12),report);},"post-expiry result absent");
    Require(report.outcome==s::SceneWorkerOutcome::reconstructed&&!report.history_reused,
        "rejected delivery advanced the next frame's history");
    Require(worker.Stop(),"worker source retirement failed at stop");
    Require(control->stop_thread==control->worker_thread,"session stopped on wrong thread");
    Require(!worker.Start(producer.device.Get()),"stopped worker silently restarted its authority domain");
    s::SceneDepthWorker terminal;Require(terminal.Stop(),"empty worker stop failed");
    Require(!terminal.Start(producer.device.Get()),"stop-before-start was not terminal");
}
void RealWorker(const std::filesystem::path& helper){
    Producer producer(256);s::SceneDepthWorker worker;
    s::SceneWorkerAuthority authority{1,1,1,true,true};
    Require(worker.Start(producer.device.Get(),{helper,1000})&&worker.SetAuthority(authority),"real worker start");
    Wait([&]{const auto state=worker.status();Require(state.phase!=s::SceneWorkerPhase::faulted,state.error.data());
        return state.phase==s::SceneWorkerPhase::idle;},"real worker startup timeout");
    unsigned owner=0;
    for(unsigned frame=1;frame<=4;++frame){
        if(frame==3)++producer.integrity;
        if(frame==4)++authority.history_epoch;
        Require(worker.SetAuthority(authority),"real worker authority");
        auto eyes=producer.Produce(frame,true);auto request=Request(producer,authority);
        request.output_width=request.output_height=384;request.quality=s::Quality::quality;
        request.depth_mask.enabled=true;request.depth_mask.relative_tolerance=.05F;request.depth_mask.erosion_radius=1;
        std::array<d::D3D9SampledDepthPoint,2> expected;
        for(unsigned eye=0;eye<2;++eye)Check(eyes[eye].snapshot.ReadDiagnosticPoint(expected[eye]),"native masked source point");
        for(unsigned eye=0;eye<2;++eye){
            auto& guide=request.guides[eye];
            guide.view_projection[12]=float(frame)*(eye?.03125F:.0625F);
            std::fill(guide.current_color_mask.begin(),guide.current_color_mask.end(),0);
        }
        Require(worker.Submit(eyes,std::move(request)),"real worker source rejection");eyes={};
        s::SceneWorkerReport report;
        Wait([&]{return worker.Poll(producer.Stamp(frame),report);},"real worker job timeout");
        Require(report.outcome==s::SceneWorkerOutcome::reconstructed,report.error.data());
        Require(report.history_reused==(frame==2),"history did not follow adjacent source and authority boundaries");
        Require(report.width==384&&report.height==384&&report.center_rgba16f[0]!=report.center_rgba16f[1],
            "real worker output extent/eyes");
        for(unsigned eye=0;eye<2;++eye)Require(report.center_rgba16f[eye]==expected[eye].color,
            "first/reset or newly exposed current color changed in integrated mask history");
        for(const auto& eye:report.center_rgba16f)for(unsigned c=0;c<3;++c)
            Require(std::isfinite(Half(eye[c])),"nonfinite real worker result");
        if(!owner)owner=report.worker_thread;Require(owner==report.worker_thread,"real worker thread changed");
    }
    Require(worker.Stop(),"real worker stop failed");
    std::cout<<"Persistent worker: 4 real NGX Quality stereo pairs at 384x384, transactional GPU masks, camera motion, adjacent history and integrity/authority resets.\n";
}
void NativeAuditCoordinator(unsigned width=64,unsigned height=0){
    Producer producer(width,height);cojvr::games::call_of_juarez::CoJSampledWorkerAudit audit;
    s::SceneWorkerAuthority authority{1,1,1,true,true};
    Require(audit.Start(producer.device.Get()),"native audit start failed");
    Wait([&]{(void)audit.Poll(authority);return audit.status().phase==s::SceneWorkerPhase::idle;},"native audit startup");
    audit.BeginFrame(1);auto pair=producer.Produce(1);
    Require(audit.Offer(pair[1])==decltype(audit)::OfferResult::pending,"right-first audit source rejected");
    Require(audit.Offer(pair[0])==decltype(audit)::OfferResult::queued,"complete audit pair not queued");pair={};
    std::optional<cojvr::games::call_of_juarez::CoJSampledWorkerAuditReport> report;
    Wait([&]{report=audit.Poll(authority);return report.has_value();},"native audit result");
    Require(report->worker.outcome==s::SceneWorkerOutcome::copied&&report->pixels_checked&&report->pixels_match,
        "native/worker color-depth center samples disagree");
    Require(report->worker.width==producer.size&&report->worker.height==producer.height,
        "audit dimensions changed during transport");
    audit.BeginFrame(2);pair=producer.Produce(2);
    Require(audit.Offer(pair[0])==decltype(audit)::OfferResult::pending,"partial audit source rejected");pair={};
    audit.BeginFrame(3);pair=producer.Produce(3); // New scope must retire the missing-eye partial.
    Require(audit.Offer(pair[0])==decltype(audit)::OfferResult::pending&&
        audit.Offer(pair[1])==decltype(audit)::OfferResult::queued,"partial source starved next frame");pair={};
    Wait([&]{report=audit.Poll(authority);return report.has_value();},"second native audit result");
    Require(report->pixels_match,"second native audit changed pixels");
    Require(audit.Retire(),"native audit retirement failed");
    Require(!audit.Start(producer.device.Get()),"retired audit restarted a failed/old owner");
}
void QueuedStop(){
    Producer producer(64);s::SceneWorkerAuthority authority{1,1,1,true,true};
    for(unsigned frame=1;frame<=3;++frame){
        s::SceneDepthWorker worker;
        Require(worker.Start(producer.device.Get())&&worker.SetAuthority(authority),"queued-stop startup");
        Wait([&]{return worker.status().phase==s::SceneWorkerPhase::idle;},"queued-stop readiness");
        auto eyes=producer.Produce(frame);
        Require(worker.Submit(eyes,Request(producer,authority,false)),"queued-stop source submit");
        Require(worker.Stop(),"queued/in-flight stop did not release source tokens before retirement");
        Require(worker.status().source_pairs_retired==1,"queued-stop source retirement not recorded");
        eyes={};
    }
}
void WorkerExtentTransitions(){
    Producer producer(64);s::SceneDepthWorker worker;
    s::SceneWorkerAuthority authority{1,1,1,true,true};
    Require(worker.Start(producer.device.Get())&&worker.SetAuthority(authority),"resizing worker startup");
    Wait([&]{return worker.status().phase==s::SceneWorkerPhase::idle;},"resizing worker readiness");
    for(unsigned frame=1;frame<=3;++frame){
        if(frame==2)producer.Resize(96,72);
        if(frame==3)producer.Resize(64,64);
        auto eyes=producer.Produce(frame);
        std::array<d::D3D9SampledDepthPoint,2> expected{};
        for(unsigned eye=0;eye<2;++eye)
            Check(eyes[eye].snapshot.ReadDiagnosticPoint(expected[eye]),"resizing source diagnostic pixel");
        Require(worker.Submit(eyes,Request(producer,authority,false)),"resizing worker source submit");
        eyes={};
        s::SceneWorkerReport report;
        Wait([&]{return worker.Poll(producer.Stamp(frame),report);},"resizing worker delivery");
        Require(report.outcome==s::SceneWorkerOutcome::copied&&
            report.width==producer.size&&report.height==producer.height,
            "resizing worker lost native source extent");
        for(unsigned eye=0;eye<2;++eye){
            Require(report.center_rgba16f[eye]==expected[eye].color,
                "resizing worker copied stale or corrupted source color");
            Require(report.center_depth_rgba16f[eye]==expected[eye].depth,
                "resizing worker copied stale or corrupted source depth");
        }
    }
    Require(worker.Stop(),"resizing worker stop");
}
int wmain(int argc,wchar_t** argv){try{
    ControlledWorker();NativeAuditCoordinator();NativeAuditCoordinator(2560,1440);
    QueuedStop();WorkerExtentTransitions();
    if(argc==2)RealWorker(argv[1]);else Require(argc==1,"unexpected arguments");
    std::cout<<"Persistent worker lifecycle/authority/source retirement tests passed. No game/headset qualification.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
