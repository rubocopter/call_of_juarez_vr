// Exercise the real hook and tracker without installing hooks or a game/VR VM.
#include "../src/games/call_of_juarez/camera_probe.cpp"
#include <iostream>

namespace cojvr::games::call_of_juarez {
namespace {
int native_updates=0;
void __fastcall NativeUpdate(void*,void*) { ++native_updates; }
struct PassEvent { runtime::Eye eye; std::uint64_t frame; std::uintptr_t owner; bool begin,complete; };
std::vector<PassEvent> pass_events;
void ObservePass(void*,runtime::Eye eye,std::uint64_t frame,std::uintptr_t owner,
    bool begin,bool complete,const char*) noexcept {
    pass_events.push_back({eye,frame,owner,begin,complete});
}
bool PassScopes() {
    g_stereo_callbacks.observe_render_pass=&ObservePass;
    for(const char* route:{"gameplay","settings","movie"}) {
        { ScopedRenderResourceObservation pass(runtime::Eye::left,9,reinterpret_cast<void*>(20),route);pass.Complete(); }
        { ScopedRenderResourceObservation cancelled(runtime::Eye::right,9,reinterpret_cast<void*>(20),route); }
    }
    if(pass_events.size()!=12)return false;
    for(std::size_t i=0;i<pass_events.size();i+=4) {
        if(!pass_events[i].begin||pass_events[i+1].begin||!pass_events[i+1].complete||
            !pass_events[i+2].begin||pass_events[i+3].begin||pass_events[i+3].complete||
            pass_events[i].eye!=runtime::Eye::left||pass_events[i+2].eye!=runtime::Eye::right||
            pass_events[i].frame!=9||pass_events[i].owner!=20)return false;
    }
    return true;
}
struct EyeScopeProbe {
    std::string events;
    bool reject_begin=false,reject_end=false,reject_mirror=false;
    bool pending=false,pending_on_begin=false,clear_pending_on_invalidate=false;
    int invalidations=0;
    runtime::Eye ended_eye=runtime::Eye::left;
    std::uint64_t ended_frame=0;
};
bool BeginEye(void* context,runtime::Eye,std::uint64_t) noexcept {
    auto& probe=*static_cast<EyeScopeProbe*>(context);
    probe.events+='b';
    if(probe.pending_on_begin)probe.pending=true;
    return !probe.reject_begin;
}
bool EndEye(void* context,runtime::Eye eye,std::uint64_t frame) noexcept {
    auto& probe=*static_cast<EyeScopeProbe*>(context);
    probe.events+='e';probe.ended_eye=eye;probe.ended_frame=frame;
    return !probe.reject_end;
}
bool MirrorPair(void* context,std::uint64_t) noexcept {
    auto& probe=*static_cast<EyeScopeProbe*>(context);
    probe.events+='m';return !probe.reject_mirror;
}
void InvalidatePair(void* context) noexcept {
    auto& probe=*static_cast<EyeScopeProbe*>(context);
    ++probe.invalidations;
    if(probe.clear_pending_on_invalidate)probe.pending=false;
}
bool EyeRestorationPending(void* context) noexcept {
    return static_cast<EyeScopeProbe*>(context)->pending;
}
void ObserveEye(void* context,runtime::Eye,std::uint64_t,std::uintptr_t,
    bool begin,bool,const char*) noexcept {
    static_cast<EyeScopeProbe*>(context)->events+=begin?'o':'p';
}
// Exercise the real resource scope with a synthetic wrapper and capture. The
// read-only observation must end before capture; the writer must end after it.
bool ScopedEyeAttempt(EyeScopeProbe& probe,bool projection=true,bool capture=true,
    runtime::Eye eye=runtime::Eye::left,std::uint64_t frame=17) {
    ScopedStereoEyeRender resources(eye,frame);
    bool captured=false;
    if(resources.begun()) {
        { ScopedRenderResourceObservation observation(eye,frame,nullptr,"gameplay");
          probe.events+='r';observation.Complete(); }
        if(projection) { probe.events+='c';captured=capture; }
    }
    return resources.Finish() && captured;
}
bool EyeResourceScopes() {
    g_stereo_callbacks={};
    { ScopedStereoEyeRender disabled(runtime::Eye::left,1);
      if(!disabled.begun()||!disabled.Finish())return false; }
    if(!MirrorStereoEyePair(1)||!NaturalStereoReplayResourcesReady())return false;
    EyeScopeProbe probe{};
    g_stereo_callbacks.context=&probe;
    g_stereo_callbacks.invalidate_frame=&InvalidatePair;
    g_stereo_callbacks.begin_eye_render=&BeginEye;
    g_stereo_callbacks.end_eye_render=&EndEye;
    g_stereo_callbacks.mirror_eye_pair=&MirrorPair;
    g_stereo_callbacks.observe_render_pass=&ObserveEye;
    if(!ScopedEyeAttempt(probe)||probe.events!="borpce"||probe.invalidations)return false;
    if(!ScopedEyeAttempt(probe,true,true,runtime::Eye::right,17))return false;
    if(!MirrorStereoEyePair(17)||probe.events!="borpceborpcem"||
        probe.ended_eye!=runtime::Eye::right||probe.ended_frame!=17)return false;
    probe.events.clear();
    if(ScopedEyeAttempt(probe,false)||probe.events!="borpe"||g_eye_render_restore.end)return false;
    probe.events.clear();
    if(ScopedEyeAttempt(probe,true,false)||probe.events!="borpce"||g_eye_render_restore.end)return false;
    probe.events.clear();probe.reject_begin=true;
    if(ScopedEyeAttempt(probe)||probe.events!="be"||probe.invalidations!=1||
        !NaturalStereoReplayResourcesReady())return false;
    probe.events.clear();probe.reject_end=true;
    if(ScopedEyeAttempt(probe)||probe.events!="be"||!g_eye_render_restore.end||
        NaturalStereoReplayResourcesReady())return false;
    // No next eye, mirror or fallback while rollback from a failed begin is pending.
    probe.events.clear();
    if(ScopedEyeAttempt(probe,true,true,runtime::Eye::right,18)||
        MirrorStereoEyePair(18)||!probe.events.empty())return false;
    probe.reject_begin=false;probe.reject_end=false;
    if(!RestorePendingEyeRenderResources()||probe.events!="e"||
        probe.ended_eye!=runtime::Eye::left||probe.ended_frame!=17||
        !NaturalStereoReplayResourcesReady())return false;
    // Successful entry/capture followed by failed end retains the same barrier.
    probe.events.clear();probe.reject_end=true;
    if(ScopedEyeAttempt(probe,true,true,runtime::Eye::right,19)||
        probe.events!="borpce"||NaturalStereoReplayResourcesReady())return false;
    probe.reject_end=false;
    if(!RestorePendingEyeRenderResources()||probe.ended_eye!=runtime::Eye::right||
        probe.ended_frame!=19)return false;
    probe.events.clear();
    { ScopedStereoEyeRender early_exit(runtime::Eye::left,20); }
    if(probe.events!="be"||g_eye_render_restore.end)return false;
    probe.events.clear();probe.reject_mirror=true;
    const auto invalidations=probe.invalidations;
    if(MirrorStereoEyePair(20)||probe.events!="me"||probe.invalidations!=invalidations+1)return false;
    probe.events.clear();probe.reject_end=true;
    if(MirrorStereoEyePair(21)||probe.events!="me"||NaturalStereoReplayResourcesReady())return false;
    probe.reject_end=false;
    if(!RestorePendingEyeRenderResources()||probe.ended_eye!=runtime::Eye::right||
        probe.ended_frame!=21||!NaturalStereoReplayResourcesReady())return false;
    // Incomplete callback installation must never admit a native eye/capture.
    probe.events.clear();g_stereo_callbacks.end_eye_render=nullptr;
    if(ScopedEyeAttempt(probe)||!probe.events.empty())return false;
    g_camera_only_restore.camera=reinterpret_cast<void*>(1);
    if(NaturalStereoReplayResourcesReady())return false;
    g_camera_only_restore={};g_stereo_callbacks={};
    return true;
}
int natural_renders=0;
bool natural_render_scoped=false;
bool natural_source_ready=false;
void NaturalSource(void*,bool complete) noexcept {natural_source_ready=complete;}
void __fastcall NativeRender(void*,void*,void* view) {
    ++natural_renders;natural_render_scoped=g_camera_only_natural_replay;
    *(static_cast<std::uint8_t*>(view)+kViewRenderedThisFrameOffset)=1;
}
bool PendingQueryAndNaturalFallback() {
    EyeScopeProbe probe{};
    g_stereo_callbacks.context=&probe;
    g_stereo_callbacks.invalidate_frame=&InvalidatePair;
    g_stereo_callbacks.begin_eye_render=&BeginEye;
    g_stereo_callbacks.end_eye_render=&EndEye;
    g_stereo_callbacks.eye_render_restoration_pending=&EyeRestorationPending;
    g_stereo_callbacks.observe_natural_render=&NaturalSource;
    alignas(16) std::array<std::byte,0x1000> camera{};
    std::array<std::byte,0x100> view{};
    CameraRenderStateSnapshot natural{};
    if(!CaptureCameraRenderState(camera.data(),natural))return false;
    bool guard=false,state=false;
    const auto replay=[&] {
        return ReplayNaturalStereoView(reinterpret_cast<RenderViewFn>(&NativeRender),
            camera.data(),view.data(),camera.data(),natural,guard,state);
    };
    // Begin and End can both reject, yet fully restored native state is safe.
    probe.reject_begin=probe.reject_end=true;
    if(ScopedEyeAttempt(probe)||probe.events!="be"||g_eye_render_restore.end||
        !NaturalStereoReplayResourcesReady())return false;
    if(!replay()||!natural_source_ready||natural_renders!=1||!natural_render_scoped||!guard||!state||
        g_camera_only_natural_replay||g_camera_only_restore.camera||probe.events!="be")return false;
    // Conversely, even successful End cannot override native/GPU quarantine.
    probe.events.clear();probe.reject_end=false;probe.pending_on_begin=true;
    ObserveNaturalRender(false); // A newer world/private attempt revokes the old source.
    if(ScopedEyeAttempt(probe)||probe.events!="be"||!g_eye_render_restore.end||replay()||
        natural_renders!=1||natural_source_ready||NaturalStereoReplayResourcesReady())return false;
    probe.reject_end=true;
    if(RestorePendingEyeRenderResources()||replay()||natural_renders!=1)return false;
    // Pending can outlive the camera's local End record (e.g. failed mirror).
    g_eye_render_restore={};
    if(replay()||natural_renders!=1||RestorePendingEyeRenderResources())return false;
    probe.events.clear();
    if(ScopedEyeAttempt(probe)||!probe.events.empty()||MirrorStereoEyePair(32))return false;
    // Invalidation may retry the native owner's End; requery before rendering.
    probe.clear_pending_on_invalidate=true;
    if(!RestorePendingEyeRenderResources()||!NaturalStereoReplayResourcesReady())return false;
    if(!replay()||natural_renders!=2||!natural_source_ready)return false;
    g_stereo_callbacks={};return true;
}
bool ReplayPreservesReference() {
    g_original_render_camera_update=reinterpret_cast<RenderCameraUpdateFn>(&NativeUpdate);
    g_pose_source=nullptr;
    g_pose_tracker={};g_pose_tracker.SetEnabled(true);
    runtime::PoseSample sample{};
    sample.sequence=1;sample.pose.position_valid=sample.pose.orientation_valid=true;
    sample.pose.position={2,1.6F,3};
    if(!g_pose_tracker.Update(sample))return false;
    sample.sequence=2;sample.pose.position.x+=.2F;
    if(!g_pose_tracker.Update(sample))return false;
    runtime::Pose before{},after{};
    if(!g_pose_tracker.CurrentPose(before))return false;
    const auto recenter=g_pose_tracker.last_recenter_sequence();
    alignas(16) std::array<std::byte,0x1000> camera{};
    {
        ScopedNaturalCameraReplay native_only;
        { ScopedNaturalCameraReplay nested; }
        if(!g_camera_only_natural_replay)return false;
        HookRenderCameraUpdate(camera.data(),nullptr);
    }
    if(g_camera_only_natural_replay||native_updates!=1||!g_pose_tracker.enabled()||
        g_pose_tracker.last_sample_sequence()!=2||g_pose_tracker.last_recenter_sequence()!=recenter||
        !g_pose_tracker.CurrentPose(after)||std::abs(after.position.x-before.position.x)>.00001F)
        return false;
    sample.sequence=3;sample.pose.position.x+=.1F;
    return g_pose_tracker.Update(sample)&&g_pose_tracker.CurrentPose(after)&&
        std::abs(after.position.x-.3F)<.00001F&&g_pose_tracker.last_recenter_sequence()==recenter;
}
}
}
int main() {
    if(!cojvr::games::call_of_juarez::PassScopes() ||
       !cojvr::games::call_of_juarez::EyeResourceScopes() ||
       !cojvr::games::call_of_juarez::PendingQueryAndNaturalFallback() ||
       !cojvr::games::call_of_juarez::ReplayPreservesReference() ||
       cojvr::games::call_of_juarez::pass_events.size()!=12) {
        std::cerr<<"eye resources must bracket capture, block pending rollback and preserve natural replay tracking\n";return 1;
    }
    std::cout<<"eye resource scopes fail closed; natural replay preserves VR reference and forwards native update once\n";
}
