// Real neutral-input JNI bookkeeping + real scoped movie-eye render/restore.
// No JVM, game, OpenVR runtime or installed staging is used.
#include <array>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <mutex>
#include <optional>
#include <memory>
#include <iostream>
#include "runtime/hud_text.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/camera_probe.cpp"

namespace cojvr::games::call_of_juarez {
namespace {
void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
void* table[174]{};void** environment=table;
void* vm_table[8]{};void** vm=vm_table;
bool controller_available=false,fail_end=false,fail_capture=false;
int controller,lookups=0,native_calls=0,renders=0,captures=0,submits=0;
std::vector<std::string> camera_records;
runtime::Vec3 expected_movie_position{};
void CameraEvent(const char* event,const char*,const char* detail) noexcept {
    if(std::string_view(event)=="camera_only_stereo")camera_records.emplace_back(detail);
}
void* __stdcall Exception(void*){return nullptr;}
void __stdcall Clear(void*){}
void __stdcall Delete(void*,void*){}
std::int32_t __stdcall Env(void*,void** out,std::int32_t){*out=&environment;return 0;}
void* __stdcall StaticObject(void*,void*,void* field){
    if(field!=&controller)return nullptr;
    ++lookups;return controller_available?&controller:nullptr;
}
void* __stdcall ReadFixtureObject(void*,void*,void*){return nullptr;}
std::int32_t __stdcall FixtureArrayLength(void*,void*){return 0;}
void* __stdcall Element(void*,void*,std::int32_t){return nullptr;}
std::int32_t __stdcall Integer(void*,void*,void*){return 0;}
std::uint8_t __stdcall Instance(void*,void*,void*){return 0;}
std::int32_t __stdcall StaticInteger(void*,void*,void*,const void*){return 0;}
std::uint8_t __stdcall Boolean(void*,void*,void*,const void*){++native_calls;return 0;}
void __stdcall Void(void*,void*,void*,const void*){++native_calls;}
void Bind(JavaPlayerBridge& b){
    b.vm_=&vm;void* token=&controller;
    b.lawman_game_class_=b.input_controller_field_=b.controller_actions_field_=b.controller_targets_field_=token;
    b.controller_lock_apply_method_=b.controller_unlock_apply_method_=b.controller_apply_method_=token;
    b.input_settings_class_=b.input_target_type_method_=b.input_digital_class_=b.input_analog_class_=token;
    b.digital_device_field_=b.digital_button_field_=b.digital_translate_method_=token;
    b.analog_device_field_=b.analog_axis_field_=b.analog_axis_sign_field_=b.analog_translate_method_=token;
    b.input_target_item_target_field_=b.input_target_item_next_field_=b.input_target_can_execute_method_=token;
    b.game_object_class_=b.game_object_controller_input_method_=token;
}
bool Admit(const CoJPresentationOwner& owner,bool observed,bool immersive,
    bool settings,bool restored,bool pending,std::uint64_t expected_epoch=9){
    return CoJCameraOnlyStereoAllowed(owner,observed,immersive,settings,restored,pending,expected_epoch);
}
void __fastcall Render(void*,void*,void* view){
    ++renders;*(static_cast<unsigned char*>(view)+kViewRenderedThisFrameOffset)=1;
    auto& eye=g_stereo_eye_override;
    if(!eye.active)return; // Invalid movie tracking forwards one natural pass.
    Check(std::fabs(eye.relative_head_pose.position.x-expected_movie_position.x)<1.e-5F&&
        std::fabs(eye.relative_head_pose.position.y-expected_movie_position.y)<1.e-5F&&
        std::fabs(eye.relative_head_pose.position.z-expected_movie_position.z)<1.e-5F,
        "movie inherited gameplay translation instead of its admitted entry origin");
    eye.camera_applied=eye.projection_applied=eye.renderer_camera_match=true;
    eye.applied_position=eye.eye->eye==runtime::Eye::left
        ? CameraProbeVector{101.F,102.F,103.F} : CameraProbeVector{201.F,202.F,203.F};
    // Model native camera writes and verify the actual transaction restores them.
    *reinterpret_cast<float*>(static_cast<std::byte*>(eye.camera)+kCameraNearOffset)=123.F;
}
bool Begin(void*,runtime::Eye,std::uint64_t) noexcept{return true;}
bool End(void*,runtime::Eye,std::uint64_t) noexcept{return !fail_end;}
bool Capture(void*,runtime::Eye,std::uint64_t,std::uint64_t*) noexcept{++captures;return !fail_capture;}
bool Submit(void*,std::uint64_t,const runtime::PoseSample&,const CameraStereoReticleOverlay& reticle,
    const runtime::StereoHudTextOverlay&) noexcept{
    Check(!reticle.weapons[0].active&&!reticle.weapons[1].active,"movie published gameplay rays");
    ++submits;return true;
}
void ReferenceBoundaries(){
    MovieCameraPositionReference reference;
    CameraStereoFrameSample sample{};sample.cinematic_movie_epoch=7;
    sample.render_device_identity=1;sample.render_device_generation=1;
    sample.hmd_pose.pose.orientation_valid=sample.hmd_pose.pose.position_valid=true;
    runtime::Pose input{};input.orientation_valid=input.position_valid=true;
    input.position={-3.F,2.F,8.F};input.orientation={.1F,.2F,.3F,.9F};
    std::uintptr_t renderer=11,view=12,camera=13;std::uint64_t recenter=14;
    const auto apply=[&](runtime::Pose& output){
        ++sample.hmd_pose.sequence;
        return reference.Apply(input,sample,reinterpret_cast<void*>(renderer),
            reinterpret_cast<void*>(view),reinterpret_cast<void*>(camera),recenter,output);
    };
    runtime::Pose output{};
    for(unsigned boundary=0;boundary<5;++boundary){
        if(boundary==1)++renderer;if(boundary==2)++view;if(boundary==3)++camera;if(boundary==4)++recenter;
        Check(apply(output)&&output.position_valid&&output.position.x==0.F&&output.position.y==0.F&&output.position.z==0.F,
            "native camera/view/renderer/recenter replacement inherited a retired movie origin");
        input.position.y+=.125F;
        Check(apply(output)&&output.position.y==.125F&&output.orientation.x==input.orientation.x&&
            output.orientation.y==input.orientation.y&&output.orientation.z==input.orientation.z&&
            output.orientation.w==input.orientation.w,"movie reference changed orientation or lost post-entry motion");
    }
    input.position_valid=false;
    Check(!apply(output)&&!output.position_valid&&!reference.entry_pose,"invalid relative tracking retained origin");
    input.position_valid=true;Check(apply(output)&&output.position.y==0.F,"tracking recovery did not capture fresh origin");
    input.position.x=std::numeric_limits<float>::quiet_NaN();
    Check(!apply(output)&&!reference.entry_pose,"nonfinite movie tracking retained origin");
    input.position.x=2.F;sample.render_device_generation=0;
    Check(!apply(output)&&!reference.entry_pose,"unknown renderer generation admitted a movie reference");
}
void Run(){
    ReferenceBoundaries();
    table[15]=reinterpret_cast<void*>(&Exception);table[17]=reinterpret_cast<void*>(&Clear);
    table[23]=reinterpret_cast<void*>(&Delete);table[145]=reinterpret_cast<void*>(&StaticObject);
    table[95]=reinterpret_cast<void*>(&ReadFixtureObject);table[171]=reinterpret_cast<void*>(&FixtureArrayLength);
    table[173]=reinterpret_cast<void*>(&Element);table[100]=reinterpret_cast<void*>(&Integer);
    table[32]=reinterpret_cast<void*>(&Instance);table[131]=reinterpret_cast<void*>(&StaticInteger);
    table[39]=reinterpret_cast<void*>(&Boolean);table[63]=reinterpret_cast<void*>(&Void);
    vm_table[6]=reinterpret_cast<void*>(&Env);
    auto& bridge=g_java_player_bridge;Bind(bridge);
    bridge.gameplay_input_applied_=true;bridge.gameplay_analog_state_uncertain_=true;
    bridge.last_gameplay_action_values_[2]=1.F;
    bridge.last_gameplay_action_values_[0]=1.F;
    const auto held=bridge.last_gameplay_action_values_;
    CoJPresentationOwner movie{};movie.valid=movie.player_alive=movie.movie_playing=true;movie.movie_input_epoch=9;
    alignas(16) std::array<std::byte,0x500> camera{},owner{};
    std::array<std::byte,0x100> view{};
    void* vtable=&controller;g_camera_vtable=&vtable;
    *reinterpret_cast<void***>(camera.data())=g_camera_vtable;
    *reinterpret_cast<void**>(camera.data()+kCameraViewOwnerOffset)=view.data();
    *reinterpret_cast<void**>(owner.data()+kRenderOwnerActiveViewOffset)=view.data();
    *reinterpret_cast<void**>(view.data()+kViewCameraOffset)=camera.data();
    *reinterpret_cast<CameraProbeVector*>(camera.data()+kCameraSourceRightOffset)={1.F,0.F,0.F};
    *reinterpret_cast<CameraProbeVector*>(camera.data()+kCameraSourceUpOffset)={0.F,1.F,0.F};
    *reinterpret_cast<CameraProbeVector*>(camera.data()+kCameraSourceForwardOffset)={0.F,0.F,1.F};
    *reinterpret_cast<CameraProbeVector*>(camera.data()+kCameraSourcePositionOffset)={10.F,20.F,30.F};
    CameraRenderStateSnapshot natural{};Check(CaptureCameraRenderState(camera.data(),natural),"camera fixture snapshot failed");
    CameraStereoFrameSample sample{};sample.cinematic_immersive=true;sample.cinematic_movie_epoch=9;
    sample.render_device_identity=101;sample.render_device_generation=2;
    sample.hmd_pose.pose.orientation_valid=sample.hmd_pose.pose.position_valid=true;
    sample.hmd_pose.pose.position={2.F,3.F,4.F};
    runtime::Pose relative{};relative.orientation_valid=relative.position_valid=true;
    relative.position={-.401425F,.009402F,.896102F};
    g_event_callback=&CameraEvent;
    g_stereo_logged_count.store(0);
    sample.eyes[0].eye=runtime::Eye::left;sample.eyes[1].eye=runtime::Eye::right;
    g_stereo_callbacks={};g_stereo_callbacks.context=&controller;
    g_stereo_callbacks.begin_eye_render=&Begin;g_stereo_callbacks.end_eye_render=&End;
    g_stereo_callbacks.capture_eye=&Capture;g_stereo_callbacks.submit_frame=&Submit;
    for(unsigned frame=1;frame<=3;++frame){
        auto moved=relative;moved.position.x+=.1F*(frame-1);moved.position.y-=.05F*(frame-1);
        expected_movie_position={.1F*(frame-1),-.05F*(frame-1),0.F};
        view[kViewRenderedThisFrameOffset]=std::byte{};
        g_gameplay_owner_release_pending=true;
        std::string error;
        if(bridge.TryApplyGameplayInput({},&error))g_gameplay_owner_release_pending=false;
        Check(error=="game input controller is unavailable"&&g_gameplay_owner_release_pending,
            "fixture must retain failed native cleanup when the controller disappears");
        Check(bridge.last_gameplay_action_values_==held&&bridge.gameplay_analog_state_uncertain_,
            "failed release erased acknowledged native keys or analog uncertainty");
        Check(Admit(movie,true,true,false,true,g_gameplay_owner_release_pending),
            "qualified passive movie was blocked by unavailable gameplay cleanup");
        sample.hmd_pose.sequence=frame;
        RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
            natural,sample,moved, {},{},{},{},frame,true,true);
        Check(!g_camera_only_restore.camera&&
            *reinterpret_cast<float*>(camera.data()+kCameraNearOffset)==0.F,
            "movie eye camera preimage was not restored");
    }
    Check(submits==3&&captures==6&&renders==6&&native_calls==0&&lookups==3,
        "passive movie must render both eyes without injecting gameplay or native skip");
    Check(camera_records.size()==3,"movie provenance must use existing bounded stereo cadence");
    for(const auto& record:camera_records) {
        for(const char* field:{";movie_epoch=9;recenter_sequence=0;",
            ";natural_camera_valid=1;natural_position=(10.0000,20.0000,30.0000)",
            ";raw_head_position=(2.000000,3.000000,4.000000)",
            ";left_camera_applied=1;left_position=(101.0000,102.0000,103.0000)",
            ";right_camera_applied=1;right_position=(201.0000,202.0000,203.0000)"})
            Check(record.find(field)!=std::string::npos,"movie record lost camera/reference/eye provenance");
    }
    Check(camera_records.front().find(";movie_head_position=(0.000000,0.000000,0.000000)")!=std::string::npos,
        "movie telemetry did not distinguish its entry origin from global tracking");
    // Exact collected pose 21510: the previous gameplay reference offsets the
    // camera by almost a metre, before the few-centimetre physical eye offset.
    const CameraProbeVector native_position{145563.375F,2288.5762F,130155.5859F};
    const auto reference=BuildTrackingReferenceBasis({.6829F,-.5133F,.5199F},{.4084F,.8582F,.3109F},0.F);
    const auto displaced=ApplyCameraTrackingOffset(native_position,reference,relative.position);
    Check(std::fabs(displaced.x-145610.3594F)<.03F&&
        std::fabs(displaced.y-2289.5164F)<.01F&&std::fabs(displaced.z-130241.8047F)<.03F,
        "collected movie displacement no longer matches the exact adapter mapping");
    // Ring pressure can reject a capture and replay natural output. It must
    // retain the admitted movie origin, including sequence-owned movies.
    auto after_pressure=relative;after_pressure.position.x+=.3F;
    expected_movie_position={.3F,0.F,0.F};sample.hmd_pose.sequence=4;fail_capture=true;
    view[kViewRenderedThisFrameOffset]=std::byte{};
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,after_pressure,{},{},{},{},4,true,true);
    Check(submits==3&&g_movie_camera_position_reference.entry_pose==1,
        "ordinary backpressure retired the movie translation origin");
    fail_capture=false;after_pressure.position.x+=.1F;
    expected_movie_position={.4F,0.F,0.F};sample.hmd_pose.sequence=5;
    view[kViewRenderedThisFrameOffset]=std::byte{};
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,after_pressure,{},{},{},{},5,true,true);
    Check(submits==4,"movie did not recover after ordinary backpressure");
    Check(g_gameplay_owner_release_pending&&bridge.last_gameplay_action_values_==held,
        "passive stereo acknowledged pending native releases");
    for(unsigned mask=0;mask<64;++mask){
        auto current=movie;current.valid=!(mask&1);current.timer_frozen=mask&2;
        current.player_alive=!(mask&4);current.mission_end_visible=mask&8;
        current.movie_playing=!(mask&16);current.movie_target_missing=mask&32;
        Check(Admit(current,true,true,false,true,true)==(mask==0),"movie bypassed native owner veto");
    }
    Check(!Admit(movie,false,true,false,true,true)&&!Admit(movie,true,true,false,false,true),
        "unknown owner or failed native restoration admitted movie stereo");
    Check(!Admit(movie,true,true,false,true,true,10)&&!Admit(movie,true,true,false,true,true,0),
        "movie camera-only admission reused a stale sample epoch");
    CoJMovieSkipResult skip{};
    g_installed.store(true);
    Check(!DispatchCameraMovieSkipPress(30,1,9,&skip)&&!skip.attempted&&
        std::string_view(skip.reason)=="gameplay_release_pending",
        "shared movie skip entry attempted native input during pending cleanup");
    g_gameplay_owner_release_pending=false;
    Check(!DispatchCameraMovieSkipPress(30,1,9,&skip)&&!skip.attempted&&
        std::string_view(skip.reason)=="input_edge_rejected",
        "skip edge rejected during cleanup reached native input on replay");
    g_installed.store(false);
    auto gameplay=movie;gameplay.movie_playing=false;
    Check(!Admit(gameplay,true,false,true,true,true)&&Admit(gameplay,true,false,true,true,false),
        "settings hands bypassed pending native cleanup");
    // Every owner/recenter/device boundary captures a fresh movie origin. The
    // full render function uses that pose for both eyes, never the global one.
    auto input=relative;
    for(unsigned boundary=0;boundary<4;++boundary) {
        if(boundary==0)++sample.cinematic_movie_epoch;
        if(boundary==1)++sample.native_gameplay_context_token;
        if(boundary==2)++sample.render_device_identity;
        if(boundary==3)++sample.render_device_generation;
        sample.hmd_pose.sequence=20+boundary;
        input.position.x+=.25F;expected_movie_position={};
        view[kViewRenderedThisFrameOffset]=std::byte{};
        RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
            natural,sample,input,{},{},{},{},20+boundary,true,true);
    }
    // Raw tracking loss must not publish a pair or retain an origin, even if
    // a caller accidentally supplies an old relative pose as still valid.
    const auto before_tracking_loss=submits;
    sample.hmd_pose.pose.position_valid=false;
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,input,{},{},{},{},30,true,true);
    Check(submits==before_tracking_loss&&!g_movie_camera_position_reference.entry_pose,
        "invalid raw movie tracking retained its origin or published eyes");
    sample.hmd_pose.pose.position_valid=true;sample.hmd_pose.sequence=31;
    view[kViewRenderedThisFrameOffset]=std::byte{};input.position.z+=.3F;
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,input,{},{},{},{},31,true,true);
    // Scope exit before admission (the actual hook's tracking/owner/restore
    // failure path) releases the movie origin without touching global recenter.
    { MovieCameraReferenceFrame failed_frame; }
    Check(!g_movie_camera_position_reference.entry_pose,"unadmitted render retained movie reference");
    const auto before_failure=submits;
    fail_end=true;
    expected_movie_position={};
    sample.hmd_pose.sequence=32;
    view[kViewRenderedThisFrameOffset]=std::byte{};
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,input, {},{},{},{},32,true,true);
    Check(submits==before_failure&&!g_movie_camera_position_reference.entry_pose,
        "failed eye resource restoration published a movie pair or retained its reference");
    g_eye_render_restore={};g_camera_only_restore={};g_stereo_callbacks={};g_event_callback=nullptr;
    // Dispose fixture tokens without pretending an unperformed release succeeded.
    bridge.vm_=nullptr;bridge.lawman_game_class_=nullptr;g_camera_vtable=nullptr;
    std::cout<<"passive movie stereo retains unavailable native releases and scoped restoration\n";
}
}
}
int main(){cojvr::games::call_of_juarez::Run();}
