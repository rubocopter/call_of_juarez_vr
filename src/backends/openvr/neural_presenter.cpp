#include "backends/openvr/neural_presenter.hpp"
#include "backends/openvr/neural_image_processor.hpp"
#include <windows.h>
#include <chrono>

namespace cojvr::backends::openvr::neural {
namespace {
enum class Phase {idle,capture,inference,resolve,draining};
void Snapshot(const d3d9::StereoCpuFrame& f,d3d9::StereoCpuFrame& o){
    // Value metadata only. Never retain a D3D9 producer lease, borrowed CPU
    // pointer or native reference just to keep presentation metadata alive.
    o.device_id=f.device_id;o.generation=f.generation;o.capture_sequence=f.capture_sequence;
    o.transport_sequence=f.transport_sequence;o.render_pose_sequence=f.render_pose_sequence;
    o.render_hmd_pose=f.render_hmd_pose;o.gameplay_reticle=f.gameplay_reticle;o.hud_text=f.hud_text;
    o.capture_time=f.capture_time;o.presentation_mode=f.presentation_mode;o.transport=f.transport;
    o.shared_eyes=f.shared_eyes;o.flat_reference_width=f.flat_reference_width;
    o.flat_reference_height=f.flat_reference_height;o.flat_reference_owner=f.flat_reference_owner;
    for(unsigned i=0;i<2;++i){o.eyes[i].width=f.eyes[i].width;o.eyes[i].height=f.eyes[i].height;
        o.eyes[i].stride=f.eyes[i].stride;o.eyes[i].format=f.eyes[i].format;}
}
}
struct Presenter::Impl {
    Options options;Session session;ImageProcessor images;ContextFence context;
    bool requested=false,enabled=false,attempted=false,warmed=false;
    bool drain_resolve=false;
    unsigned failures=0;std::uint64_t epoch=1,revision=1,applied=0;
    Header packet{},previous{};bool have_previous=false;
    d3d9::StereoCpuFrame retained;
    Phase phase=Phase::idle;const char* status="off";
    std::vector<std::uint8_t> input;
    explicit Impl(Options o):options(std::move(o)){}
};
Options LoadGameOptions() noexcept {try{
    wchar_t path[32768]{};const auto n=GetModuleFileNameW(nullptr,path,32768);
    if(!n||n>=32768)return {};
    return LoadOptions(std::filesystem::path(path).parent_path()/L"cojvr-neural.ini");
}catch(...){return {};}}
Presenter::Presenter(Options options):impl_(std::make_unique<Impl>(std::move(options))){}
Presenter::~Presenter()=default;
const char* Presenter::status()const noexcept{return impl_->status;}
bool Presenter::enabled()const noexcept{return impl_->enabled;}
bool Presenter::busy()const noexcept{return impl_->phase==Phase::capture||impl_->phase==Phase::inference||impl_->phase==Phase::resolve;}
std::uint64_t Presenter::applied_pairs()const noexcept{return impl_->applied;}
d3d9::StereoCpuFrame& Presenter::RetainedFrame()noexcept{return impl_->retained;}
void Presenter::Invalidate()noexcept {
    auto& p=*impl_;p.drain_resolve=p.phase==Phase::resolve||(p.phase==Phase::draining&&p.drain_resolve);
    ++p.epoch;p.have_previous=false;p.session.Cancel();p.images.Discard();
    p.phase=p.images.busy()?Phase::draining:Phase::idle;
}
void Presenter::ObserveContext(bool available)noexcept {if(impl_->context.Observe(available))Invalidate();}
void Presenter::SetRequested(bool requested,std::uint64_t revision)noexcept {
    auto& p=*impl_;revision=revision?revision:1;
    if(p.requested==requested&&p.revision==revision)return;
    Invalidate();p.revision=revision;p.requested=requested;
    const bool changed=p.enabled!=(requested&&p.options.enabled);
    p.enabled=requested&&p.options.enabled;
    if(changed){p.session.Stop();p.attempted=p.warmed=false;p.failures=0;}
    p.status=p.enabled?"starting":requested?"unavailable":"off";
}
void Presenter::Toggle()noexcept {SetRequested(!impl_->requested,impl_->revision+1);}
bool Presenter::Process(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,const d3d9::StereoCpuFrame& frame,
    const std::array<runtime::EyeView,2>& optics,std::uint64_t revision,bool allowed)noexcept {try{
    auto& p=*impl_;
    if(!p.enabled){p.status=p.requested?"unavailable":"off";return false;}
    if(busy()||p.images.busy()){p.status="draining";return false;}
    Header h;h.request=1;const auto now=std::chrono::steady_clock::now();
    if(!allowed||frame.presentation_mode!=d3d9::FramePresentationMode::native_stereo||
       !frame.capture_sequence||!frame.render_pose_sequence||!frame.device_id||!frame.generation||
       now<frame.capture_time||now-frame.capture_time>std::chrono::milliseconds(100)||
       !BuildViews(frame.render_hmd_pose,optics,h.views)){
        Invalidate();p.status="context_bypass";return false;
    }
    if(p.failures>=8){p.status="disabled_after_failures";return false;}
    if(!p.attempted){p.attempted=true;
        if(!p.session.Start(p.options.helper,p.options.runtime,L"model")){p.failures=8;p.status="helper_unavailable";return false;}
    }
    const auto state=p.session.Poll();
    if(state==State::starting){p.status="starting";return false;}
    if(state==State::warming){p.status="warming";return false;}
    if(state==State::recovering){p.status="recovering";return false;}
    if(state==State::failed){p.failures=8;p.status="helper_failed";return false;}
    if(state!=State::ready){p.status="unavailable";return false;}
    if(p.have_previous&&frame.device_id==p.previous.input.device&&frame.generation==p.previous.input.generation&&
       (frame.capture_sequence<=p.previous.input.frame||frame.render_pose_sequence<=p.previous.input.pose)){
        p.status="repeated_pose";return false;
    }
    unsigned sw=0,sh=0;
    if(!p.images.BeginCapture(device,context,eyes,p.options.stage_height,sw,sh)){
        p.status="capture_failed";++p.failures;return false;
    }
    if(p.have_previous&&(frame.capture_sequence!=p.previous.input.frame+1||frame.device_id!=p.previous.input.device||
       frame.generation!=p.previous.input.generation||revision!=p.previous.input.revision||sw!=p.previous.width||sh!=p.previous.height))++p.epoch;
    h.width=sw;h.height=sh;
    h.input={frame.capture_sequence,frame.render_pose_sequence,frame.device_id,frame.generation,revision?revision:1,p.epoch};
    if(!Valid(h)){Invalidate();p.status="invalid_metadata";return false;}
    Snapshot(frame,p.retained);p.packet=h;p.phase=Phase::capture;p.status="capture_pending";
    return false; // Native pair is displayed first; completion is polled later.
}catch(...){Invalidate();impl_->status="exception_bypass";impl_->failures=8;impl_->session.Stop();return false;}}
bool Presenter::Pump(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& eyes,std::uint64_t active_frame,
    std::uint64_t device_id,std::uint64_t generation,bool allowed)noexcept {try{
    auto& p=*impl_;const auto session_state=p.session.Poll();
    if(p.phase==Phase::draining){
        if(p.drain_resolve)(void)p.images.PollResolve(device,context,{});
        else (void)p.images.PollCapture(device,context,p.input);
        if(!p.images.busy())p.phase=Phase::idle;
        return false;
    }
    if(!busy())return false;
    const auto now=std::chrono::steady_clock::now();
    if(!p.enabled||!allowed||active_frame!=p.packet.input.frame||device_id!=p.packet.input.device||
       generation!=p.packet.input.generation||p.revision!=p.packet.input.revision||
       now<p.retained.capture_time||now-p.retained.capture_time>std::chrono::milliseconds(100)){
        Invalidate();p.status="expired_or_context_bypass";return false;
    }
    if(session_state==State::failed){Invalidate();p.status="helper_failed";p.failures=8;return false;}
    if(p.phase==Phase::capture){
        if(!p.images.PollCapture(device,context,p.input)){
            if(p.images.last_result()==E_PENDING){p.status="capture_pending";return false;}
            Invalidate();p.status="capture_failed";++p.failures;return false;
        }
        p.previous=p.packet;p.have_previous=true;
        if(!p.warmed){
            if(!p.session.Warmup(p.packet,p.input)){Invalidate();p.status="warmup_failed";p.failures=8;return false;}
            p.warmed=true;p.images.Discard();p.phase=p.images.busy()?Phase::draining:Phase::idle;
            ++p.epoch;p.status="warming";return false;
        }
        if(!p.session.Submit(p.packet,p.input)){Invalidate();p.status="submit_failed";++p.failures;return false;}
        p.phase=Phase::inference;p.status="processing";return false;
    }
    if(p.phase==Phase::inference){
        if(session_state==State::pending){p.status="processing";return false;}
        if(session_state!=State::ready||p.session.Output().empty()){
            Invalidate();p.status="response_bypass";++p.failures;return false;
        }
        if(!p.images.BeginResolve(device,context,eyes,p.input,p.session.Output(),p.packet.width,p.packet.height)){
            Invalidate();p.status="resolve_failed";++p.failures;return false;
        }
        p.phase=Phase::resolve;p.status="resolve_pending";return false;
    }
    if(p.phase==Phase::resolve){
        if(!p.images.PollResolve(device,context,eyes)){
            if(p.images.last_result()==E_PENDING){p.status="resolve_pending";return false;}
            Invalidate();p.status="resolve_failed";++p.failures;return false;
        }
        p.phase=Phase::idle;p.failures=0;++p.applied;p.status="applied";return true;
    }
    return false;
}catch(...){Invalidate();impl_->status="exception_bypass";impl_->failures=8;impl_->session.Stop();return false;}}
}
