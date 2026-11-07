#include "backends/openvr/scene_depth_worker.hpp"
#include "backends/openvr/d3d9_shared_texture_bridge.hpp"
#include "backends/openvr/d3d11_sync.hpp"
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>

namespace cojvr::backends::openvr::dlss {
namespace {
using Clock=std::chrono::steady_clock;
template<class T>using ComPtr=Microsoft::WRL::ComPtr<T>;
template<std::size_t N>void Error(std::array<char,N>& out,std::string_view value) noexcept {
    out={};const auto count=std::min(value.size(),N-1);std::memcpy(out.data(),value.data(),count);
}
bool Available(const SceneWorkerAuthority& a) noexcept {
    return a.available&&a.tracking_valid&&a.settings_revision&&a.input_context&&a.history_epoch;
}
bool CurrentDomain(const SceneDepthStamp& source,const SceneDepthStamp& current) noexcept {
    return source.device==current.device&&source.generation==current.generation&&source.owner==current.owner&&
        source.integrity==current.integrity&&source.epoch==current.epoch&&
        current.frame>=source.frame&&current.pose>=source.pose;
}
bool Expired(const SceneWorkerRequest& r) noexcept {return Clock::now()-r.capture_time>r.max_age;}
HRESULT DeviceForLuid(LUID luid,ComPtr<ID3D11Device>& device,ComPtr<ID3D11DeviceContext>& context) noexcept {
    ComPtr<IDXGIFactory1> factory;auto hr=CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if(FAILED(hr))return hr;
    for(UINT i=0;;++i){
        ComPtr<IDXGIAdapter1> adapter;hr=factory->EnumAdapters1(i,&adapter);if(FAILED(hr))return hr;
        DXGI_ADAPTER_DESC1 desc{};hr=adapter->GetDesc1(&desc);if(FAILED(hr))return hr;
        if(desc.AdapterLuid.LowPart!=luid.LowPart||desc.AdapterLuid.HighPart!=luid.HighPart)continue;
        return D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&device,nullptr,&context);
    }
}
struct Scratch {
    std::array<ComPtr<ID3D11Texture2D>,4> planes;
    std::array<ComPtr<ID3D11Texture2D>,2> masks;
    unsigned width=0,height=0;
    bool Prepare(ID3D11Device* device,unsigned w,unsigned h) noexcept {
        if(width==w&&height==h&&planes[3])return true;
        std::array<ComPtr<ID3D11Texture2D>,4> candidate;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        for(auto& plane:candidate)if(FAILED(device->CreateTexture2D(&desc,nullptr,&plane)))return false;
        planes=std::move(candidate);masks={};width=w;height=h;return true;
    }
};
}
struct SceneDepthWorker::Impl {
    struct Job {SceneDepthWork work;SceneWorkerRequest request;std::uint64_t version=0;};
    DWORD owner_thread=GetCurrentThreadId();
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::thread thread;
    std::atomic_bool stop=false;
    std::atomic<std::uint64_t> version=1;
    bool started=false,has_source=false,source_retired=false,source_accepted=false,revoked=false;
    bool fatal_result=false;
    SceneDepthStamp submitted_source{};
    SceneWorkerRequest submitted_request;
    SceneWorkerAuthority authority{};
    std::uint64_t submitted_version=0;
    SceneDepthTransfer transfer;
    SceneWorkerOptions options;
    LUID luid{};
    std::unique_ptr<PairExecutor> injected;
    std::optional<Job> job;
    std::optional<SceneWorkerReport> result;
    SceneWorkerStatus state;
    explicit Impl(std::unique_ptr<PairExecutor> p):injected(std::move(p)){}
    bool Owner()const noexcept{return owner_thread==GetCurrentThreadId();}
    bool Live(const Job& j)const noexcept{return !stop.load()&&version.load()==j.version;}
    void Revoke() noexcept {
        if(has_source&&!revoked){revoked=true;++version;transfer.Invalidate();}
    }
    void RetireSource() noexcept {
        if(!has_source||source_retired)return;
        SceneDepthStamp completed{};
        const auto delivery=transfer.Poll(submitted_source,completed);
        if(delivery==SceneDepthDelivery::ready||delivery==SceneDepthDelivery::discarded){
            source_retired=true;source_accepted=delivery==SceneDepthDelivery::ready;
            std::lock_guard lock(mutex);++state.source_pairs_retired;
        }
    }
    bool RequestValid(const std::array<SceneDepthEye,2>& eyes,const SceneWorkerRequest& r)const noexcept {
        const auto now=Clock::now();
        if(eyes[0].capture_time==Clock::time_point{}||eyes[1].capture_time==Clock::time_point{}||
            eyes[0].capture_time>now||eyes[1].capture_time>now||
            r.capture_time!=std::min(eyes[0].capture_time,eyes[1].capture_time))return false;
        if(!Available(r.authority)||r.capture_time==Clock::time_point{}||r.capture_time>now||
            r.max_age.count()<=0||Expired(r))return false;
        const auto w=eyes[0].snapshot.width(),h=eyes[0].snapshot.height();
        if(!w||!h)return false;
        const auto pixels=std::uint64_t{w}*h;
        if(pixels*32>d3d9::D3D9SampledDepthCapture::max_payload_bytes)return false;
        if(!r.reconstruct)return true;
        if(options.helper.empty())return false;
        dlss::Request extent;extent.width=w;extent.height=h;extent.out_width=r.output_width;
        extent.out_height=r.output_height;extent.quality=r.quality;
        if(!ValidExtent(extent)||pixels*34+std::uint64_t{r.output_width}*r.output_height*16>
            d3d9::D3D9SampledDepthCapture::max_payload_bytes)return false;
        for(const auto& eye:r.guides){
            runtime::TemporalMatrix inverse{};
            if(!eye.source_qualified||eye.current_color_mask.size()!=pixels||
                !std::isfinite(eye.near_metres)||!std::isfinite(eye.far_metres)||eye.near_metres<=0||
                eye.far_metres<=eye.near_metres||!std::isfinite(eye.jitter_x)||!std::isfinite(eye.jitter_y)||
                std::abs(eye.jitter_x)>.5F||std::abs(eye.jitter_y)>.5F||
                !runtime::TemporalReprojection(eye.view_projection,runtime::TemporalIdentity(),inverse))return false;
        }
        return true;
    }
    bool WaitCopies(D3D9SharedTextureBridge& color,D3D9SharedTextureBridge& depth,ID3D11DeviceContext* context) noexcept {
        const auto end=Clock::now()+std::chrono::milliseconds(options.gpu_timeout_ms);
        context->Flush();
        do{
            color.Poll(context);depth.Poll(context);
            if(!color.stats().pending_copy_fences&&!depth.stats().pending_copy_fences)return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }while(Clock::now()<end);
        color.Shutdown(context);depth.Shutdown(context);
        return !color.stats().pending_copy_fences&&!depth.stats().pending_copy_fences;
    }
    bool Sample(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& images,SceneWorkerReport& report) noexcept {
        std::array<ComPtr<ID3D11Texture2D>,2> pixels;
        for(unsigned eye=0;eye<2;++eye){
            if(!images[eye])return false;
            D3D11_TEXTURE2D_DESC desc{};images[eye]->GetDesc(&desc);
            if(desc.Width!=report.width||desc.Height!=report.height||desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT||
                desc.ArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1)return false;
            const D3D11_BOX box{desc.Width/2,desc.Height/2,0,desc.Width/2+1,desc.Height/2+1,1};
            desc.Width=desc.Height=1;desc.Usage=D3D11_USAGE_STAGING;desc.MiscFlags=desc.BindFlags=0;
            desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,&pixels[eye])))return false;
            context->CopySubresourceRegion(pixels[eye].Get(),0,0,0,0,images[eye],0,&box);
        }
        D3D11SyncResult sync;std::string error;
        if(!SynchronizeD3D11(device,context,D3D11SyncStrategy::event_query,options.gpu_timeout_ms,sync,error))return false;
        std::array<std::array<std::uint16_t,4>,2> values{};
        for(unsigned eye=0;eye<2;++eye){
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context->Map(pixels[eye].Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
            if(!mapped.pData){context->Unmap(pixels[eye].Get(),0);return false;}
            std::memcpy(values[eye].data(),mapped.pData,8);context->Unmap(pixels[eye].Get(),0);
        }
        report.center_rgba16f=values;return true;
    }
    SceneWorkerReport Process(Job& j,ID3D11Device* device,ID3D11DeviceContext* context,
        Scratch& scratch,D3D9SharedTextureBridge& color,D3D9SharedTextureBridge& depth,PairSession* session,
        std::uint64_t& history_version,SceneDepthStamp& history_source,bool& fatal){
        SceneWorkerReport report;report.source=j.work.stamp;report.authority=j.request.authority;
        const auto process_begin=Clock::now();
        report.worker_thread=GetCurrentThreadId();
        if(!Live(j)){report.outcome=SceneWorkerOutcome::cancelled;return report;}
        if(Expired(j.request)){report.outcome=SceneWorkerOutcome::expired;return report;}
        const auto w=j.work.color->shared_eyes[0].width,h=j.work.color->shared_eyes[0].height;
        if(!scratch.Prepare(device,w,h)){Error(report.error,"worker input allocation failed");return report;}
        D3D9SharedTextureCopyTiming timing{};
        const bool color_ok=color.CopyFrame(device,context,*j.work.color,
            {scratch.planes[0].Get(),scratch.planes[1].Get()},timing);
        const bool depth_ok=depth.CopyFrame(device,context,*j.work.depth,
            {scratch.planes[2].Get(),scratch.planes[3].Get()},timing);
        if(color_ok&&depth_ok)j.work.AcceptCopies();
        if(!WaitCopies(color,depth,context)){
            fatal=true;Error(report.error,"worker GPU copies have unproven retirement");return report;
        }
        if(!color_ok||!depth_ok){Error(report.error,"worker source copy rejected");return report;}
        report.copy_ms=std::chrono::duration<double,std::milli>(Clock::now()-process_begin).count();
        if(!Live(j)){report.outcome=SceneWorkerOutcome::cancelled;return report;}
        if(Expired(j.request)){report.outcome=SceneWorkerOutcome::expired;return report;}
        std::array<ID3D11Texture2D*,2> output{scratch.planes[0].Get(),scratch.planes[1].Get()};
        report.width=w;report.height=h;
        if(j.request.reconstruct){
            if(!session){Error(report.error,"worker reconstruction not enabled");return report;}
            if(history_version!=j.version||!CurrentDomain(history_source,j.work.stamp))
                session->Invalidate();
            PairFrame pair;
            pair.identity={{reinterpret_cast<std::uintptr_t>(device),j.work.stamp.epoch,j.work.stamp.frame,
                j.work.stamp.pose,j.work.stamp.owner,w,h,j.request.output_width,j.request.output_height,
                static_cast<unsigned>(j.request.quality),true,true},j.request.authority.settings_revision,
                j.request.authority.history_epoch,j.request.authority.input_context};
            for(unsigned eye=0;eye<2;++eye){
                const auto& guide=j.request.guides[eye];
                D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;
                desc.ArraySize=desc.MipLevels=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8_UNORM;
                desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                const D3D11_SUBRESOURCE_DATA data{guide.current_color_mask.data(),w,0};
                if(FAILED(device->CreateTexture2D(&desc,&data,scratch.masks[eye].ReleaseAndGetAddressOf()))){
                    session->Invalidate();Error(report.error,"worker mask upload failed");return report;
                }
                auto& input=pair.eyes[eye];input.identity=pair.identity;input.eye=eye;
                input.input_token=j.work.input_tokens[eye];input.unjittered_view_projection=guide.view_projection;
                input.guides.color=scratch.planes[eye].Get();input.guides.linear_depth_alpha=scratch.planes[2+eye].Get();
                input.guides.current_color_mask=scratch.masks[eye].Get();
                input.guides.near_metres=guide.near_metres;input.guides.far_metres=guide.far_metres;
                input.guides.jitter_x=guide.jitter_x;input.guides.jitter_y=guide.jitter_y;
                input.guides.source_qualified=guide.source_qualified;
                input.guides.depth_encoding=DepthEncoding::packed_device_z_rgb24;
            }
            if(!session->Reconstruct(context,pair,&report.history_reused)){Error(report.error,session->last_error());return report;}
            if(!Live(j)){session->Invalidate();report.outcome=SceneWorkerOutcome::cancelled;return report;}
            if(Expired(j.request)){session->Invalidate();report.outcome=SceneWorkerOutcome::expired;return report;}
            output=session->Outputs(pair);report.width=j.request.output_width;report.height=j.request.output_height;
        }
        if(!Sample(device,context,output,report)){
            fatal=true;if(session)session->Invalidate();Error(report.error,"worker output sample/synchronization failed");return report;
        }
        if(!j.request.reconstruct){
            SceneWorkerReport depth_report;depth_report.width=w;depth_report.height=h;
            if(!Sample(device,context,{scratch.planes[2].Get(),scratch.planes[3].Get()},depth_report)){
                fatal=true;report.center_rgba16f={};Error(report.error,"worker depth sample failed");return report;
            }
            report.center_depth_rgba16f=depth_report.center_rgba16f;
        }
        if(!Live(j)){if(session)session->Invalidate();report.center_rgba16f={};report.outcome=SceneWorkerOutcome::cancelled;return report;}
        if(Expired(j.request)){if(session)session->Invalidate();report.center_rgba16f={};report.outcome=SceneWorkerOutcome::expired;return report;}
        if(j.request.reconstruct){
            history_version=j.version;
            history_source=j.work.stamp;
        }
        report.outcome=j.request.reconstruct?SceneWorkerOutcome::reconstructed:SceneWorkerOutcome::copied;
        report.processing_ms=std::chrono::duration<double,std::milli>(Clock::now()-process_begin).count();
        return report;
    }
    void Run() noexcept {
        try{
            ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
            if(FAILED(DeviceForLuid(luid,device,context))){
                std::lock_guard lock(mutex);state.phase=SceneWorkerPhase::faulted;Error(state.error,"worker D3D11 adapter/device creation failed");return;
            }
            std::unique_ptr<PairSession> session;
            if(!options.helper.empty()){
                session=injected?std::make_unique<PairSession>(std::move(injected)):std::make_unique<PairSession>();
                if(!session->Start(device.Get(),options.helper)){
                    std::lock_guard lock(mutex);state.phase=SceneWorkerPhase::faulted;Error(state.error,session->last_error());return;
                }
            }
            Scratch scratch;D3D9SharedTextureBridge color,depth;std::uint64_t history_version=0;
            SceneDepthStamp history_source{};
            {
                std::lock_guard lock(mutex);state.worker_thread=GetCurrentThreadId();
                if(!stop)state.phase=SceneWorkerPhase::idle;
            }
            while(!stop){
                std::optional<Job> current;
                {
                    std::unique_lock lock(mutex);cv.wait(lock,[&]{return stop.load()||job.has_value();});
                    if(stop)break;current=std::move(job);job.reset();
                }
                bool fatal=false;
                auto report=Process(*current,device.Get(),context.Get(),scratch,color,depth,session.get(),history_version,history_source,fatal);
                current->work={}; // Unqueued completion tokens release before result visibility.
                current.reset();
                {
                    std::lock_guard lock(mutex);result=report;fatal_result=fatal;
                    ++state.completed;state.phase=SceneWorkerPhase::completed;
                }
                if(fatal)break;
            }
            color.Shutdown(context.Get());depth.Shutdown(context.Get());
            if(session)session->Stop();
        }catch(...){
            std::lock_guard lock(mutex);
            if(has_source){SceneWorkerReport report;report.source=submitted_source;report.authority=submitted_request.authority;
                Error(report.error,"worker exception");result=report;fatal_result=true;++state.completed;state.phase=SceneWorkerPhase::completed;}
            else {state.phase=SceneWorkerPhase::faulted;Error(state.error,"worker startup exception");}
        }
    }
};
SceneDepthWorker::SceneDepthWorker(std::unique_ptr<PairExecutor> executor):impl_(std::make_unique<Impl>(std::move(executor))){}
SceneDepthWorker::~SceneDepthWorker(){(void)Stop();}
bool SceneDepthWorker::Start(IDirect3DDevice9* producer,SceneWorkerOptions options) noexcept {
    auto& p=*impl_;if(!p.Owner()||p.started||p.stop||!producer||!options.gpu_timeout_ms||options.gpu_timeout_ms>10000||
        (!options.helper.empty()&&!options.helper.is_absolute()))return false;
    try{
        ComPtr<IDirect3D9> api;ComPtr<IDirect3D9Ex> ex;D3DDEVICE_CREATION_PARAMETERS creation{};
        auto hr=producer->GetCreationParameters(&creation);
        if(SUCCEEDED(hr))hr=producer->GetDirect3D(&api);
        if(SUCCEEDED(hr))hr=api.As(&ex);
        if(SUCCEEDED(hr))hr=ex->GetAdapterLUID(creation.AdapterOrdinal,&p.luid);
        if(FAILED(hr))return false;
        p.options=std::move(options);p.started=true;
        {std::lock_guard lock(p.mutex);p.state.phase=SceneWorkerPhase::starting;}
        p.thread=std::thread([&p]{p.Run();});return true;
    }catch(...){std::lock_guard lock(p.mutex);p.state.phase=SceneWorkerPhase::faulted;Error(p.state.error,"worker thread creation failed");return false;}
}
bool SceneDepthWorker::SetAuthority(SceneWorkerAuthority authority) noexcept {
    auto& p=*impl_;if(!p.Owner())return false;
    const bool valid=!(authority.available&&authority.tracking_valid)||Available(authority);
    if(!valid)authority.available=false; // Unknown authority revokes, never preserves old permission.
    bool changed=false;
    {std::lock_guard lock(p.mutex);changed=p.authority!=authority;p.authority=authority;if(changed)++p.version;}
    if(changed&&p.has_source){p.revoked=true;p.transfer.Invalidate();}
    return valid;
}
bool SceneDepthWorker::Submit(const std::array<SceneDepthEye,2>& eyes,SceneWorkerRequest request) noexcept {
    auto& p=*impl_;if(!p.Owner()||!p.started||p.stop||!p.RequestValid(eyes,request))return false;
    try{
        std::lock_guard lock(p.mutex);
        if(p.state.phase!=SceneWorkerPhase::idle||p.authority!=request.authority||!Available(p.authority))return false;
        Impl::Job job;
        if(!p.transfer.Queue(eyes,job.work))return false;
        job.version=p.version.load();job.request=std::move(request);
        p.submitted_source=job.work.stamp;p.submitted_request.authority=job.request.authority;
        p.submitted_request.capture_time=job.request.capture_time;p.submitted_request.max_age=job.request.max_age;
        p.submitted_request.reconstruct=job.request.reconstruct;
        p.submitted_version=job.version;p.has_source=true;p.source_retired=p.source_accepted=p.revoked=false;
        p.job=std::move(job);p.state.phase=SceneWorkerPhase::busy;++p.state.submitted;p.cv.notify_one();return true;
    }catch(...){return false;}
}
bool SceneDepthWorker::Poll(const SceneDepthStamp& current,SceneWorkerReport& output) noexcept {
    auto& p=*impl_;if(!p.Owner()||!p.has_source)return false;
    if(!CurrentDomain(p.submitted_source,current))p.Revoke();
    p.RetireSource();
    std::lock_guard lock(p.mutex);if(!p.result)return false;
    auto report=*p.result;p.result.reset();
    report.age_ms=std::chrono::duration<double,std::milli>(Clock::now()-p.submitted_request.capture_time).count();
    const bool success=report.outcome==SceneWorkerOutcome::copied||report.outcome==SceneWorkerOutcome::reconstructed;
    if(success||report.outcome==SceneWorkerOutcome::cancelled||report.outcome==SceneWorkerOutcome::expired){
        if(Expired(p.submitted_request))report.outcome=SceneWorkerOutcome::expired;
        else if(p.revoked||p.submitted_version!=p.version||p.authority!=report.authority||
            !Available(p.authority)||!CurrentDomain(report.source,current))report.outcome=SceneWorkerOutcome::cancelled;
        else if(!p.source_retired||!p.source_accepted){report.outcome=SceneWorkerOutcome::failed;Error(report.error,"source completion was not accepted");}
    }
    if(report.outcome!=SceneWorkerOutcome::copied&&report.outcome!=SceneWorkerOutcome::reconstructed){
        if(p.submitted_request.reconstruct)++p.version;
        report.history_reused=false;
        report.center_rgba16f={};report.center_depth_rgba16f={};
    }
    p.state.error=report.error;p.state.phase=p.fatal_result||!p.source_retired?SceneWorkerPhase::faulted:SceneWorkerPhase::idle;
    p.has_source=!p.source_retired;output=report;return true;
}
SceneWorkerStatus SceneDepthWorker::status()const noexcept {
    const auto& p=*impl_;std::lock_guard lock(p.mutex);return p.state;
}
bool SceneDepthWorker::Stop() noexcept {
    auto& p=*impl_;p.stop=true;++p.version;p.cv.notify_all();
    if(p.Owner()&&p.has_source){p.revoked=true;p.transfer.Invalidate();}
    if(p.thread.joinable()){
        if(p.thread.get_id()==std::this_thread::get_id())return false;
        p.thread.join();
    }
    {std::lock_guard lock(p.mutex);p.job.reset();p.result.reset();}
    // A queued job may never have reached the worker. Return its untouched
    // completion tokens before polling producer-owned source retirement.
    if(p.Owner())p.RetireSource();
    std::lock_guard lock(p.mutex);p.state.phase=SceneWorkerPhase::stopped;
    return p.Owner()&&(!p.has_source||p.source_retired);
}
} // namespace cojvr::backends::openvr::dlss
