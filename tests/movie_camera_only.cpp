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
bool controller_available=false,fail_end=false;
int controller,lookups=0,native_calls=0,renders=0,captures=0,submits=0;
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
    eye.camera_applied=eye.projection_applied=eye.renderer_camera_match=true;
    // Model native camera writes and verify the actual transaction restores them.
    *reinterpret_cast<float*>(static_cast<std::byte*>(eye.camera)+kCameraNearOffset)=123.F;
}
bool Begin(void*,runtime::Eye,std::uint64_t) noexcept{return true;}
bool End(void*,runtime::Eye,std::uint64_t) noexcept{return !fail_end;}
bool Capture(void*,runtime::Eye,std::uint64_t,std::uint64_t*) noexcept{++captures;return true;}
bool Submit(void*,std::uint64_t,const runtime::PoseSample&,const CameraStereoReticleOverlay& reticle,
    const runtime::StereoHudTextOverlay&) noexcept{
    Check(!reticle.weapons[0].active&&!reticle.weapons[1].active,"movie published gameplay rays");
    ++submits;return true;
}
void Run(){
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
    CameraRenderStateSnapshot natural{};Check(CaptureCameraRenderState(camera.data(),natural),"camera fixture snapshot failed");
    CameraStereoFrameSample sample{};sample.cinematic_immersive=true;sample.cinematic_movie_epoch=9;
    sample.hmd_pose.pose.orientation_valid=sample.hmd_pose.pose.position_valid=true;
    sample.eyes[0].eye=runtime::Eye::left;sample.eyes[1].eye=runtime::Eye::right;
    g_stereo_callbacks={};g_stereo_callbacks.context=&controller;
    g_stereo_callbacks.begin_eye_render=&Begin;g_stereo_callbacks.end_eye_render=&End;
    g_stereo_callbacks.capture_eye=&Capture;g_stereo_callbacks.submit_frame=&Submit;
    for(unsigned frame=1;frame<=3;++frame){
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
            natural,sample,{}, {},{},{},{},frame,true,true);
        Check(!g_camera_only_restore.camera&&
            *reinterpret_cast<float*>(camera.data()+kCameraNearOffset)==0.F,
            "movie eye camera preimage was not restored");
    }
    Check(submits==3&&captures==6&&renders==6&&native_calls==0&&lookups==3,
        "passive movie must render both eyes without injecting gameplay or native skip");
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
    fail_end=true;
    view[kViewRenderedThisFrameOffset]=std::byte{};
    RenderCameraOnlyStereo(reinterpret_cast<RenderViewFn>(&Render),owner.data(),view.data(),camera.data(),
        natural,sample,{}, {},{},{},{},10,true,true);
    Check(submits==3,"failed eye resource restoration published a movie pair");
    g_eye_render_restore={};g_camera_only_restore={};g_stereo_callbacks={};
    // Dispose fixture tokens without pretending an unperformed release succeeded.
    bridge.vm_=nullptr;bridge.lawman_game_class_=nullptr;g_camera_vtable=nullptr;
    std::cout<<"passive movie stereo retains unavailable native releases and scoped restoration\n";
}
}
}
int main(){cojvr::games::call_of_juarez::Run();}
