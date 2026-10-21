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
bool RifleVerificationStopsLaterEyes(){
    // A supported transaction expects both hands and its weapon to remain
    // active. The real bridge reports inactive when those overlays disappear.
    CoJLongGunGripGeometry geometry{};
    auto& n=geometry.native;n.valid=true;n.object_epoch=1;n.weapon_class="WeaponRifleWinchester";
    n.player_id=10;n.weapon_id=20;n.holder_element=4;n.support_element=10;
    n.states[0]={1,1,1,1};n.two_handed[0]={true,true,true};
    n.root={{0,0,0},{0,1,0},{0,0,1},true};n.holder=n.root;
    n.support={{0,0,-30},{0,1,0},{0,0,1},true};
    const auto contact=CoJLongGunContact(n.weapon_class).point_cm;
    n.contact_element=8;
    n.contact_anchor={{-contact.x,-contact.y,-30-contact.z},{0,1,0},{0,0,1},true};
    if(!BuildCoJReloadLocalFrame(n.root,n.support,n.support_in_weapon))return false;
    geometry.muzzle_cm={0,0,-60};geometry.muzzle_direction=geometry.direction={0,0,-1};
    geometry.up={0,1,0};geometry.support_grip_cm=n.support.position;
    CoJLongGunGripInput input{};input.player_generation=7;input.context=9;
    auto& controls=input.controls;
    controls.enabled=controls.eligible=controls.tracking_valid=true;
    controls.grip_available=controls.support_trigger_available=true;
    controls.acquisition_radius_m=.1F;controls.sequence=1;
    controls.sample_time_ms=controls.now_ms=1000;
    g_long_gun_grip={};(void)g_long_gun_grip.Update(input,geometry);
    controls.grip_held=true;++controls.sequence;controls.sample_time_ms=controls.now_ms=1010;
    if(!g_long_gun_grip.Update(input,geometry).attached){
        std::cerr<<"supported stereo fixture failed initial mesh-contact acquisition\n";return false;}
    EyeScopeProbe probe{};g_stereo_callbacks={};g_stereo_callbacks.context=&probe;
    g_stereo_callbacks.begin_eye_render=&BeginEye;g_stereo_callbacks.end_eye_render=&EndEye;
    g_stereo_callbacks.mirror_eye_pair=&MirrorPair;g_stereo_callbacks.invalidate_frame=&InvalidatePair;
    bool lo=true,ro=true,lm=true,rm=true,whip=false;
    ObserveIndependentHandRenderState(731,"left_eye_complete",lo,ro,lm,rm,whip);
    if(g_long_gun_grip.attached()||g_long_gun_failed_sequence!=731||rm||ro){
        std::cerr<<"inactive supported hand/weapon did not fail the frame\n";return false;}
    {ScopedStereoEyeRender right(runtime::Eye::right,731);
        if(right.begun()){std::cerr<<"failed rifle admitted another eye\n";return false;}}
    if(MirrorStereoEyePair(731)||!probe.events.empty()){
        std::cerr<<"failed rifle reached stereo mirror\n";return false;}
    if(GameplayStereoPairReady(731,true,true,true,true,true,true)){
        std::cerr<<"restoration laundered a failed rifle pair into publication\n";return false;}
    // The legacy route without independent-eye callbacks must enforce the
    // same supported native-path and frame-verification gates.
    g_stereo_callbacks={};
    if(GameplayStereoPairReady(731,true,true,true,true,true,true))return false;
    g_long_gun_expected_sequence=732;
    if(GameplayStereoPairReady(732,true,true,true,true,true,false)||
        !GameplayStereoPairReady(732,true,true,true,true,true,true))return false;
    // Final post-eye verification must still expect both hands after an
    // earlier cancellation; inactive=false-success cannot discharge the latch.
    ObserveIndependentHandRenderState(732,"stereo_complete",lo,ro,lm,rm,whip);
    if(!LongGunFrameFailed(732)||GameplayStereoPairReady(732,true,true,true,true,true,true))return false;
    g_long_gun_grip={};g_long_gun_expected_sequence=g_long_gun_failed_sequence=0;return true;
}
std::uint64_t bow_source_sequence=0,bow_source_time_ms=0;
struct BowCaptureFixture {
    CoJBowDrawSessionInput input{};
    BowCaptureFixture(){
        g_bow_draw_session.Reset();
        // Reset cancels logical drawing, not ownership of an already consumed
        // physical trigger. This fixture is the same controller source across
        // cases: keep its watermark and supply the real release in Grab().
        input.sequence=bow_source_sequence;input.now_ms=bow_source_time_ms;
        input.selected=input.allowed=input.right_available=input.counter_valid=true;
        input.player=2;input.context=3;input.input_generation=4;
        input.native.valid=true;input.native.weapon_id=5;input.native.current_state=1;
        input.native.root.valid=input.native.arrow.valid=input.native.holder.valid=true;
        input.head_world_m={0,1.6F,0};input.left_world_m={-.05F,1.3F,-.4F};input.right_world_m={0,1.3F,-.4F};
        input.native.root.position=input.native.arrow.position={0,130,-40};
        input.native.root.forward={0,0,-1};input.native.root.right={-1,0,0};
    }
    CoJBowDrawSessionOutput Tick(){input.sequence=++bow_source_sequence;input.now_ms+=20;
        bow_source_time_ms=input.now_ms;return g_bow_draw_session.Update(input);}
    bool Grab(){Tick();input.right_held=true;return Tick().gesture.grab;}
    void Ready(){input.native.current_state=129;input.native.can_attack=input.native.held_right=input.native.release_ready=true;}
    ~BowCaptureFixture(){g_bow_draw_session.Reset();}
};
bool CaptureDropPreservesBow(){
    g_stereo_callbacks={};
    BowCaptureFixture bow;if(!bow.Grab()){std::cerr<<"capture fixture failed initial grab\n";return false;}bow.Ready();
    bow.input.right_world_m.z+=.2F;
    if(!bow.Tick().dispatch.right)return false;
    const auto owner=g_bow_draw_session.owner_token();
    // The physical report contains 31 left capture-ring misses. No right eye
    // was attempted on those frames; all attempted native resources restored.
    for(int drop=0;drop<31;++drop){
        if(CompletePhysicalBowFrame(false,true,true,true,true)||
            !g_bow_draw_session.active()||g_bow_draw_session.owner_token()!=owner)return false;
        const auto held=bow.Tick();
        if(!held.gesture.drawing||!held.dispatch.right||held.dispatch.left||held.dispatch.shot_requested)return false;
    }
    if(!CompletePhysicalBowFrame(true,true,true,true,true))return false;
    bow.input.right_held=false;const auto released=bow.Tick();
    if(!released.dispatch.shot_requested||!released.dispatch.left||!released.dispatch.pending)return false;
    if(bow.Tick().dispatch.left)return false;
    ++bow.input.attack_entries;
    return !bow.Tick().dispatch.right;
}
bool CaptureDropRetainsBowSafety(){
    g_stereo_callbacks={};
    for(int fault=0;fault<4;++fault){
        BowCaptureFixture bow;if(!bow.Grab()){std::cerr<<"safety fixture failed grab: "<<fault<<'\n';return false;}bow.Ready();
        if(CompletePhysicalBowFrame(true,fault!=0,fault!=1,fault!=2,fault!=3)||
            g_bow_draw_session.active()||g_bow_draw_session.owner_token())return false;
        if(bow.Tick().gesture.grab)return false;
        bow.input.right_held=false;
        if(bow.Tick().dispatch.shot_requested)return false;
    }
    BowCaptureFixture bow;if(!bow.Grab()){std::cerr<<"stale fixture failed grab\n";return false;}bow.Ready();
    bow.input.now_ms+=101;
    const auto stale=bow.Tick();
    if(stale.gesture.drawing||stale.dispatch.right||stale.dispatch.shot_requested)return false;
    if(CompletePhysicalBowFrame(false,true,true,true,true)||g_bow_draw_session.active())return false;
    return !bow.Tick().gesture.grab;
}
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
    BowCaptureFixture bow;
    if(!bow.Grab())return false;
    g_original_render_view=reinterpret_cast<RenderViewFn>(&NativeRender);
    HookRenderViewImpl(camera.data(),nullptr,view.data());
    if(g_bow_draw_session.active()||g_bow_draw_session.owner_token())return false;
    // Repeated quarantined frames must not revive the cancelled native owner.
    HookRenderViewImpl(camera.data(),nullptr,view.data());
    if(g_bow_draw_session.active()||natural_renders!=1)return false;
    g_original_render_view=nullptr;
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
struct InputLevelProbe {
    std::vector<std::byte> engine=std::vector<std::byte>(0x542f58+sizeof(void*));
    std::array<std::byte,0x700> game{};
    std::array<std::byte,0x100> view{};
    std::array<void*,2> levels{};
    void* owner=reinterpret_cast<void*>(0x8f120000);
    void* foreign=reinterpret_cast<void*>(0x12340000);
    int calls=0,renders=0,credits=0;
    bool blocking_ui=false,native_context_valid=false;
    void*& current() { return *reinterpret_cast<void**>(game.data()+0x668); }
    void*& singleton() { return *reinterpret_cast<void**>(engine.data()+0x542f58); }
    void Bind() {
        singleton()=game.data();current()=owner;levels={owner,foreign};view={};
        *reinterpret_cast<void***>(game.data()+0x65c)=levels.data();
        *reinterpret_cast<void***>(game.data()+0x660)=levels.data()+2;
        calls=renders=credits=0;blocking_ui=native_context_valid=false;
    }
};
InputLevelProbe* input_level_probe=nullptr;
void __fastcall NativeRenderAfterInput(void* owner,void*,void* view) {
    auto& p=*input_level_probe;++p.renders;p.native_context_valid=p.current()==owner;
    *(static_cast<std::uint8_t*>(view)+kViewRenderedThisFrameOffset)=1;
}
bool InputRestoresNativeRenderContext() {
    InputLevelProbe p;input_level_probe=&p;
    auto* previous_engine=g_engine_base;g_engine_base=p.engine.data();
    g_stereo_callbacks={};g_stereo_callbacks.observe_natural_render=&NaturalSource;
    bool passed=true;
    for(int scenario=0;scenario<7;++scenario) {
        p.Bind();bool restored=true;
        std::array<void*,3> relocated{p.foreign,p.owner,p.foreign};
        const bool delivered=DispatchWithRenderLevelContext(p.owner,[&]() noexcept {
            ++p.calls;++p.credits;p.blocking_ui=true;p.current()=nullptr;
            if(scenario==1)p.current()=p.foreign;
            if(scenario==2)p.levels[0]=p.foreign;
            if(scenario==3)p.singleton()=nullptr;
            if(scenario==4) {
                *reinterpret_cast<void***>(p.game.data()+0x65c)=relocated.data();
                *reinterpret_cast<void***>(p.game.data()+0x660)=relocated.data()+3;
            }
            if(scenario==6)*reinterpret_cast<std::uintptr_t*>(p.game.data()+0x660)=
                reinterpret_cast<std::uintptr_t>(p.levels.data())+257*sizeof(void*);
            return scenario!=5;
        },restored);
        const bool expected=scenario==0||scenario==4||scenario==5;
        // A rejected dispatch must not authorize a native continuation.
        if(restored)ForwardNaturalRender(reinterpret_cast<RenderViewFn>(&NativeRenderAfterInput),p.owner,p.view.data());
        passed &= restored==expected&&delivered==(expected&&scenario!=5)&&
            p.calls==1&&p.credits==1&&p.blocking_ui&&p.renders==(expected?1:0);
        if(expected)passed &= p.native_context_valid&&p.current()==p.owner;
        else passed &= p.current()==(scenario==1?p.foreign:nullptr);
    }
    for(void* entry:std::array<void*,2>{nullptr,p.foreign}) {
        p.Bind();p.current()=entry;bool restored=true;
        passed &= !DispatchWithRenderLevelContext(p.owner,[&]() noexcept {++p.calls;return true;},restored)&&
            !restored&&p.calls==0&&p.current()==entry;
    }
    p.Bind();bool outer_restored=false;
    passed &= DispatchWithRenderLevelContext(p.owner,[&]() noexcept {
        bool inner_restored=false;
        const bool inner=DispatchWithRenderLevelContext(p.owner,[&]() noexcept {
            ++p.calls;p.current()=nullptr;return true;
        },inner_restored);
        ++p.calls;p.current()=nullptr;return inner&&inner_restored;
    },outer_restored)&&outer_restored&&p.calls==2&&p.current()==p.owner;
    // The unchanged read-only slot succeeds without a write. A cleared read-only
    // slot rejects without changing protection or reviving a retired context.
    auto* page=static_cast<std::byte*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!page)passed=false;
    else {
        for(bool clear:{false,true}) {
            p.Bind();std::memcpy(page,p.game.data(),p.game.size());p.singleton()=page;
            RenderInputLevelContext context(p.owner);
            auto** slot=reinterpret_cast<void**>(page+0x668);
            if(clear)*slot=nullptr;
            DWORD old=0;
            if(!VirtualProtect(page,4096,PAGE_READONLY,&old))passed=false;
            else {
                passed &= context.Restore()==!clear&&*slot==(clear?nullptr:p.owner);
                DWORD ignored=0;if(!VirtualProtect(page,4096,old,&ignored))passed=false;
            }
            // Each scope has one completion; a later null cannot rearm it.
            *slot=nullptr;passed &= !context.Restore()&&*slot==nullptr;
        }
        VirtualFree(page,0,MEM_RELEASE);
    }
    g_engine_base=previous_engine;g_stereo_callbacks={};input_level_probe=nullptr;
    return passed;
}
}
}
int main() {
    if(!cojvr::games::call_of_juarez::RifleVerificationStopsLaterEyes())return 1;
    if(!cojvr::games::call_of_juarez::CaptureDropPreservesBow()){
        std::cerr<<"capture backpressure cancelled physical bow\n";return 1;}
    if(!cojvr::games::call_of_juarez::CaptureDropRetainsBowSafety()){
        std::cerr<<"capture drop bypassed native/tracking safety\n";return 1;}
    if(!cojvr::games::call_of_juarez::InputRestoresNativeRenderContext()) {
        std::cerr<<"injected input lost native render level or changed native action ownership\n";return 1;
    }
    if(!cojvr::games::call_of_juarez::PassScopes() ||
       !cojvr::games::call_of_juarez::EyeResourceScopes() ||
       !cojvr::games::call_of_juarez::PendingQueryAndNaturalFallback() ||
       !cojvr::games::call_of_juarez::ReplayPreservesReference() ||
       cojvr::games::call_of_juarez::pass_events.size()!=12) {
        std::cerr<<"eye resources must bracket capture, block pending rollback and preserve natural replay tracking\n";return 1;
    }
    std::cout<<"eye resource scopes fail closed; natural replay preserves VR reference and forwards native update once\n";
}
