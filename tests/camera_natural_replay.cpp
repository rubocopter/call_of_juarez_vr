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
       !cojvr::games::call_of_juarez::ReplayPreservesReference() ||
       cojvr::games::call_of_juarez::pass_events.size()!=12) {
        std::cerr<<"natural fallback must forward native update without disabling/recentering tracked hands\n";return 1;
    }
    std::cout<<"natural camera replay preserves VR reference and forwards native update once\n";
}
