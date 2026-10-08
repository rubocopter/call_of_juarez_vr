// Execute the real JNI dispatcher against a controller that rejects one-shot
// actions while LockApplyControllerState is held, just like shipped CoJ.
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <iostream>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "runtime/gameplay_utility.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"

namespace {
using namespace cojvr::games::call_of_juarez;
enum Id { actions_field=1, targets_field, target_field, next_field, lock_method,
    unlock_method, apply_method, eligible_method, direct_method, translate_method,
    device_field, button_field, sign_field, origin_field, direction_field, x_field, y_field, z_field, rotate_method };
void* Ptr(int value) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value)); }
struct Action { int id; };
std::array<Action,48> actions{};
int controller, targets, item, player, digital_type, analog_type, game_type, settings_type, exception;
void* env_table[174]{};
void** env_holder = env_table;
void* vm_table[8]{};
void** vm_holder = vm_table;
bool locked = false, pending = false, fail_analog = false, analog_outside_lock = false;
int locks = 0, commits = 0;
int fail_digital_action=-1;
bool mount_on_interact=false, native_crouched=false;
std::array<int,48> releases{};
struct Vector { float x=0,y=0,z=0; } origin_vector, direction_vector;
void* published_origin=nullptr;
void* published_direction=nullptr;
int construction_count=0;
bool vector_write_fault=false;
int observed_weapon;
bool shot_field_fault=false, shot_read_fault=false, shot_owner_fault=false;
bool mounted_value=false, mounted_lookup_fault=false, mounted_call_fault=false;
int mounted_calls=0;
void* __stdcall MountedMethod(void*,void*,const char* name,const char* signature) {
    if (mounted_lookup_fault || std::strcmp(name,"IsRidingHorse") || std::strcmp(signature,"()Z")) {
        pending=true;return nullptr;
    }
    return Ptr(303);
}
std::uint8_t __stdcall MountedBoolean(void*,void* owner,void* method,const JValue* args) {
    if (owner!=&player || method!=Ptr(303) || args || mounted_call_fault) { pending=true;return 0; }
    ++mounted_calls;return mounted_value ? 1 : 0;
}
constexpr const char* shot_fields[]{"cojvrShotSerial","cojvrHitSerial","cojvrFxSerial",
    "cojvrFxStatus","cojvrShotSuppressEffects","cojvrCombHandle","cojvrSmokeHandle"};
constexpr int shot_values[]{12,9,12,63,0,101,202};
std::array<int,48> accepted{}, rejected{};
std::array<int,2> native_hand_attacks{};
std::vector<float> native_turns;

std::int32_t __stdcall GetEnv(void*, void** env, std::int32_t) { *env=&env_holder; return 0; }
void* __stdcall Exception(void*) { return pending ? &exception : nullptr; }
void __stdcall Clear(void*) { pending=false; }
void __stdcall Delete(void*, void*) {}
void* __stdcall StaticObject(void*, void*, void*) { return &controller; }
void* __stdcall ObjectField(void*, void* owner, void* field) {
    if (owner==&controller) return field==Ptr(actions_field) ? static_cast<void*>(actions.data()) : &targets;
    if (owner==&item) return field==Ptr(target_field) ? &player : nullptr;
    if (owner==&player) return field==Ptr(origin_field) ? published_origin : published_direction;
    return nullptr;
}
void* __stdcall Class(void*,void*) { return &game_type; }
void* __stdcall Field(void*,void*,const char* name,const char*) {
    for (int i=0;i<7;++i) if (std::strcmp(name,shot_fields[i])==0) {
        if (shot_field_fault && i==6) { pending=true; return nullptr; }
        return Ptr(200+i);
    }
    return Ptr(std::strcmp(name,"cojvrInteractionOrigin")==0 ? origin_field : direction_field);
}
void* __stdcall New(void*,void*,void*,const JValue*) {
    return (++construction_count % 2) ? &origin_vector : &direction_vector;
}
void __stdcall SetObject(void*,void*,void* field,void* value) {
    (field==Ptr(origin_field) ? published_origin : published_direction)=value;
}
void __stdcall SetFloat(void*,void* object,void* field,float value) {
    if (vector_write_fault) { pending=true; vector_write_fault=false; return; }
    auto& vector=*static_cast<Vector*>(object);
    (field==Ptr(x_field) ? vector.x : field==Ptr(y_field) ? vector.y : vector.z)=value;
}
template<class Bridge> bool PublishInteraction(Bridge& bridge,JavaPlayerPosition origin,
    JavaPlayerPosition direction,bool valid) {
    if constexpr (requires { bridge.TryPublishInteractionRay(origin,direction,valid); })
        return bridge.TryPublishInteractionRay(origin,direction,valid);
    return false;
}
std::int32_t __stdcall Length(void*, void* array) { return array==actions.data() ? 48 : 3; }
void* __stdcall Element(void*, void* array, std::int32_t index) {
    return array==actions.data() ? static_cast<void*>(&actions[index]) : &item;
}
std::uint8_t __stdcall Instance(void*, void* object, void* type) {
    return type==&digital_type || (object==&player && type==&game_type);
}
std::int32_t __stdcall Integer(void*, void* owner, void* field) {
    if (owner==&observed_weapon) {
        const auto index=reinterpret_cast<std::uintptr_t>(field)-200;
        if (shot_read_fault && index==6) { pending=true; return -1; }
        return shot_values[index];
    }
    return field==Ptr(button_field) ? static_cast<Action*>(owner)->id : 1;
}
void* __stdcall ObserveMethod(void* env,void* owner,const char* name,const char* signature) {
    if (!std::strcmp(name,"IsRidingHorse")) return MountedMethod(env,owner,name,signature);
    return Ptr(std::strcmp(name,"GetThisID")==0 ? 302 : 301);
}
std::int32_t __stdcall ObserveOwner(void*,void* owner,void* method,const JValue*) {
    if (owner!=&observed_weapon || method!=Ptr(302)) { pending=true; return 0; }
    if (shot_owner_fault) { pending=true; return 0; }
    return 0x30000000;
}
void* __stdcall ObserveObject(void*,void*,void*,const JValue* args) {
    return args[0].i==0 ? &observed_weapon : nullptr;
}
std::int32_t __stdcall TargetType(void*, void*, void*, const JValue*) { return 1; }
std::uint8_t __stdcall Boolean(void* env, void* owner, void* method, const JValue* args) {
    if (method==Ptr(303)) return MountedBoolean(env,owner,method,args);
    if (method==Ptr(eligible_method)) return 1;
    if (method==Ptr(translate_method)) {
        const int action=static_cast<Action*>(owner)->id;
        if (action==fail_digital_action) { fail_digital_action=-1; pending=true; return 0; }
        if (action==16) native_crouched=args[3].z!=0;
        if (!args[3].z) { ++releases[action]; return 1; }
        if (action==30 && mount_on_interact) { mounted_value=true; native_crouched=false; }
        ++(locked ? rejected[action] : accepted[action]);
        // Shipped PlayerController.ExecuteInput's tableswitch converts action
        // 10 to ApplyAttack(0/right), and action 9 to ApplyAttack(1/left).
        if (!locked && (action==9 || action==10))
            ++native_hand_attacks[action==10 ? 0 : 1];
        return !locked;
    }
    return 1;
}
void __stdcall Void(void*, void*, void* method, const JValue* args) {
    if(method==Ptr(rotate_method)){native_turns.push_back(args[0].f);return;}
    if (method==Ptr(lock_method)) { locked=true; ++locks; }
    else if (method==Ptr(unlock_method)) locked=false;
    else if (method==Ptr(apply_method)) ++commits;
    else if (method==Ptr(direct_method)) {
        analog_outside_lock |= !locked;
        if (fail_analog) { fail_analog=false; pending=true; }
    }
}
void Bind(JavaPlayerBridge& bridge) {
    bridge.vm_=&vm_holder;
    bridge.lawman_game_class_=&game_type;
    bridge.input_controller_field_=Ptr(99);
    bridge.controller_actions_field_=Ptr(actions_field);
    bridge.controller_targets_field_=Ptr(targets_field);
    bridge.controller_lock_apply_method_=Ptr(lock_method);
    bridge.controller_unlock_apply_method_=Ptr(unlock_method);
    bridge.controller_apply_method_=Ptr(apply_method);
    bridge.input_settings_class_=&settings_type;
    bridge.input_target_type_method_=Ptr(99);
    bridge.input_digital_class_=&digital_type;
    bridge.input_analog_class_=&analog_type;
    bridge.digital_device_field_=bridge.analog_device_field_=Ptr(device_field);
    bridge.digital_button_field_=bridge.analog_axis_field_=Ptr(button_field);
    bridge.analog_axis_sign_field_=Ptr(sign_field);
    bridge.digital_translate_method_=bridge.analog_translate_method_=Ptr(translate_method);
    bridge.input_target_item_target_field_=Ptr(target_field);
    bridge.input_target_item_next_field_=Ptr(next_field);
    bridge.input_target_can_execute_method_=Ptr(eligible_method);
    bridge.game_object_class_=&game_type;
    bridge.game_object_controller_input_method_=Ptr(direct_method);
}
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
int campaign_game,campaign_single,campaign_step,campaign_fail,campaign_globals,campaign_locals;
bool campaign_return_ref;
bool CampaignFault(){++campaign_step;if(campaign_step!=campaign_fail)return false;pending=true;return true;}
void* __stdcall CampaignFind(void*,const char* name){
    if(pending)std::abort();
    const bool fault=CampaignFault();
    if(fault&&!campaign_return_ref)return nullptr;
    ++campaign_locals;return !std::strcmp(name,"LawmanGame")?&campaign_game:&campaign_single;
}
void* __stdcall CampaignGlobal(void*,void* ref){
    if(pending)std::abort();
    const bool fault=CampaignFault();
    if(fault&&!campaign_return_ref)return nullptr;
    ++campaign_globals;return ref;
}
void* __stdcall CampaignField(void*,void*,const char*,const char*){
    if(pending)std::abort();if(CampaignFault())return nullptr;return Ptr(777);
}
void __stdcall CampaignDeleteLocal(void*,void* ref){if(ref==&campaign_game||ref==&campaign_single)--campaign_locals;}
void __stdcall CampaignDeleteGlobal(void*,void* ref){if(ref==&campaign_game||ref==&campaign_single)--campaign_globals;}
}
int main() {
    for (int i=0;i<48;++i) actions[i].id=i;
    env_table[15]=reinterpret_cast<void*>(&Exception);
    env_table[17]=reinterpret_cast<void*>(&Clear);
    env_table[22]=env_table[23]=reinterpret_cast<void*>(&Delete);
    env_table[30]=reinterpret_cast<void*>(&New);
    env_table[31]=reinterpret_cast<void*>(&Class);
    env_table[94]=reinterpret_cast<void*>(&Field);
    env_table[104]=reinterpret_cast<void*>(&SetObject);
    env_table[111]=reinterpret_cast<void*>(&SetFloat);
    env_table[32]=reinterpret_cast<void*>(&Instance);
    env_table[39]=reinterpret_cast<void*>(&Boolean);
    env_table[63]=reinterpret_cast<void*>(&Void);
    env_table[95]=reinterpret_cast<void*>(&ObjectField);
    env_table[100]=reinterpret_cast<void*>(&Integer);
    env_table[131]=reinterpret_cast<void*>(&TargetType);
    env_table[145]=reinterpret_cast<void*>(&StaticObject);
    env_table[171]=reinterpret_cast<void*>(&Length);
    env_table[173]=reinterpret_cast<void*>(&Element);
    vm_table[6]=reinterpret_cast<void*>(&GetEnv);
    JavaPlayerBridge bridge;
    Bind(bridge);
    cojvr::runtime::GameplayInputState state{};
    state.active=true; state.move={.3F,.8F};
    state.jump=state.interact=state.reload=state.weapon_next=state.kick=state.hands=true;
    std::string error;
    bool ok=Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::non_fire),
        "native gameplay dispatcher failed");
    for (const int id : {11,30,31,46,39,29})
        ok &= Check(accepted[id]==1 && rejected[id]==0,
            "one-shot gameplay action was consumed while locomotion held the native controller lock");
    ok &= Check(!locked && locks==1 && commits==1 && !analog_outside_lock,
        "locomotion lost its single native transaction");
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::non_fire) &&
        locks==1 && accepted[30]==1, "held one-shot was duplicated");
    state.move={}; state.jump=state.interact=state.reload=state.weapon_next=state.kick=state.hands=false;
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::non_fire), "release failed");
    state.move={.2F,.7F}; state.interact=true; fail_analog=true;
    ok &= Check(!bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::non_fire) &&
        !locked && accepted[30]==1, "failed analog transaction leaked or consumed F");
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::non_fire) &&
        accepted[30]==2, "failed transaction lost the pending F edge");
    const int previous_locks=locks;
    state.fire_right=true;
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::fire) &&
        locks==previous_locks && accepted[30]==2,
        "separate fire phase redispatched non-fire state");
    ok &= Check(native_hand_attacks[0]==1 && native_hand_attacks[1]==0,
        "right trigger reached native left-hand Attack through PlayerController action conversion");
    state.fire_right=false;
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::fire),
        "right trigger release failed");
    state.fire_left=true;
    ok &= Check(bridge.TryApplyGameplayInput(state,&error,CoJInputDispatchPhase::fire) &&
        native_hand_attacks[0]==1 && native_hand_attacks[1]==1,
        "left trigger did not reach native left-hand Attack");
    bridge.being_=&player;
    bridge.vector_class_=&game_type;
    bridge.vector_constructor_=Ptr(99);
    bridge.vector_x_field_=Ptr(x_field); bridge.vector_y_field_=Ptr(y_field); bridge.vector_z_field_=Ptr(z_field);
    ok &= Check(PublishInteraction(bridge,{10,20,30},{0,0,-1},true) &&
        published_origin && published_direction && origin_vector.x==10 && origin_vector.y==20 &&
        direction_vector.z==-1, "tracked interaction ray was not published as a complete pair");
    ok &= Check(PublishInteraction(bridge,{30,40,50},{1,0,0},true) && construction_count==2 &&
        origin_vector.x==30 && direction_vector.x==1, "interaction ray did not reuse player-owned vectors");
    vector_write_fault=true;
    ok &= Check(!PublishInteraction(bridge,{40,50,60},{0,1,0},true) &&
        !published_origin && !published_direction && !pending, "failed interaction write left a stale cache");
    ok &= Check(PublishInteraction(bridge,{10,20,30},{0,0,-1},true), "interaction ray failed to recover");
    ok &= Check(!PublishInteraction(bridge,{10,20,30},{0,0,0},true) &&
        !published_origin && !published_direction, "invalid interaction direction retained cache");
    ok &= Check(PublishInteraction(bridge,{10,20,30},{0,0,-1},true) &&
        PublishInteraction(bridge,{}, {},false) && !published_origin && !published_direction,
        "interaction context loss failed to clear the pair");
    env_table[33]=reinterpret_cast<void*>(&ObserveMethod);
    env_table[36]=reinterpret_cast<void*>(&ObserveObject);
    env_table[51]=reinterpret_cast<void*>(&ObserveOwner);
    CoJWeaponShotDiagnostics diagnostics{};
    ok &= Check(bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && diagnostics.valid &&
        diagnostics.values==std::array<int,5>{12,9,12,63,0}, "actual native shot counters were not read");
    ok &= Check(diagnostics.fx_handles==std::array<int,2>{101,202},
        "committed native FX handles were not observed");
    ok &= Check(diagnostics.owner_id==0x30000000,
        "FX diagnostic handle did not retain its freshly resolved active weapon owner");
    shot_owner_fault=true;
    ok &= Check(!bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && !diagnostics.valid &&
        !diagnostics.owner_id && diagnostics.values==std::array<int,5>{} && !pending,
        "owner failure retained stale emitter identity/counters or JNI exception");
    shot_owner_fault=false;
    shot_field_fault=true;
    ok &= Check(!bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && !diagnostics.valid &&
        diagnostics.values==std::array<int,5>{} && diagnostics.fx_handles==std::array<int,2>{} &&
        !pending, "failed shot read retained stale counters or JNI exception");
    shot_field_fault=false;
    ok &= Check(bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && diagnostics.valid,
        "shot observation did not recover after field failure");
    shot_read_fault=true;
    ok &= Check(!bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && !diagnostics.valid &&
        diagnostics.values==std::array<int,5>{} && diagnostics.fx_handles==std::array<int,2>{} && !pending,
        "GetIntField failure retained partial counters or JNI exception");
    shot_read_fault=false;
    ok &= Check(!bridge.TryObserveWeaponShotDiagnostics(1,diagnostics) && !diagnostics.valid,
        "empty hand acquired actual shot counters");
    CoJContextualShoulderState shoulder;
    cojvr::runtime::GameplayInputState r1{};r1.active=true;
    shoulder.Update(r1,true,true,1); // available released sample
    r1.kick=true;shoulder.Update(r1,true,true,1);
    ok &= Check(!r1.kick&&r1.run,"mounted R1 must hold gallop without horse kick");
    r1.run=false;r1.kick=true;shoulder.Update(r1,true,true,1);
    ok &= Check(!r1.kick&&r1.run,"held mounted R1 lost gallop");
    r1.run=false;r1.kick=true;shoulder.Update(r1,true,false,1);
    ok &= Check(!r1.kick&&!r1.run,"dismount reinterpreted held R1 as kick");
    r1.kick=false;shoulder.Update(r1,true,false,1);
    r1.kick=true;shoulder.Update(r1,true,false,1);
    ok &= Check(r1.kick&&!r1.run,"fresh on-foot R1 must pulse kick only");
    r1.kick=true;shoulder.Update(r1,true,false,1);
    ok &= Check(!r1.kick&&!r1.run,"held on-foot R1 repeated kick");
    r1.kick=true;shoulder.Update(r1,true,true,1);
    ok &= Check(!r1.kick&&!r1.run,"mount reinterpreted held kick as gallop");
    r1.kick=false;shoulder.Update(r1,true,true,1);
    r1.kick=true;shoulder.Update(r1,true,true,1);
    ok &= Check(r1.run&&!r1.kick,"fresh mounted R1 did not regain gallop");
    r1.kick=true;r1.run=false;shoulder.Update(r1,false,false,1);
    ok &= Check(!r1.run&&!r1.kick,"unknown mounted state did not release shoulder");
    r1.kick=true;shoulder.Update(r1,true,true,1);
    ok &= Check(!r1.run&&!r1.kick,"observation recovery rearmed held R1");
    r1.kick=false;shoulder.Update(r1,true,true,1);
    r1.kick=true;shoulder.Update(r1,true,true,2);
    ok &= Check(!r1.run&&!r1.kick,"player replacement rearmed held R1");
    r1.kick=false;shoulder.Update(r1,true,true,2);
    r1.kick=true;r1.digital_available&=~(1U<<9);shoulder.Update(r1,true,true,2);
    r1.digital_available|=1U<<9;r1.kick=true;shoulder.Update(r1,true,true,2);
    ok &= Check(!r1.run&&!r1.kick,"unavailable R1 counted as release");
    r1.kick=false;shoulder.Update(r1,true,true,2);
    r1.kick=true;shoulder.Update(r1,true,true,2);
    ok &= Check(r1.run&&!r1.kick,"mounted gesture did not recover after available release");
    r1.active=false;r1.kick=true;shoulder.Update(r1,true,true,2);
    ok &= Check(!r1.run&&!r1.kick,"menu loss left gallop held");
    const auto old_method=env_table[33],old_boolean=env_table[39];
    env_table[33]=reinterpret_cast<void*>(&MountedMethod);
    env_table[39]=reinterpret_cast<void*>(&MountedBoolean);
    bool mounted=true;
    mounted_value=false;
    ok &= Check(bridge.TryObserveMounted(mounted)&&!mounted,"native on-foot observation failed");
    mounted_value=true;
    ok &= Check(bridge.TryObserveMounted(mounted)&&mounted,"native mounted observation failed");
    mounted_lookup_fault=true;
    ok &= Check(!bridge.TryObserveMounted(mounted)&&!mounted&&!pending,"mounted lookup fault retained stale state");
    mounted_lookup_fault=false;mounted_call_fault=true;
    ok &= Check(!bridge.TryObserveMounted(mounted)&&!mounted&&!pending,"mounted call fault retained stale state");
    mounted_call_fault=false;pending=true;const auto calls_before=mounted_calls;
    ok &= Check(!bridge.TryObserveMounted(mounted)&&!mounted&&!pending&&mounted_calls==calls_before,
        "pending JNI exception allowed mounted observation");
    for(int api:{15,17,23,31,33,39}) {
        const auto saved=env_table[api];env_table[api]=nullptr;
        ok &= Check(!bridge.TryObserveMounted(mounted)&&!mounted,"missing mounted JNI function accepted");
        env_table[api]=saved;
    }
    ok &= Check(bridge.TryObserveMounted(mounted)&&mounted,"mounted observation did not recover");
    bridge.being_=nullptr;
    ok &= Check(!bridge.TryObserveMounted(mounted)&&!mounted,"missing local player accepted mounted state");
    bridge.being_=&player;env_table[33]=old_method;env_table[39]=old_boolean;
    JavaPlayerBridge snap_bridge;Bind(snap_bridge);snap_bridge.being_=&player;
    snap_bridge.rotate_horizontally_method_=Ptr(rotate_method);
    cojvr::runtime::GameplayInputState turn{};turn.active=true;turn.turn.x=.9F;
    ok &= Check(snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::non_fire)&&
        native_turns==std::vector<float>{-45},"right stick delivered leftward native yaw");
    ok &= Check(snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::fire)&&
        snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::non_fire)&&native_turns.size()==1,
        "held snap or fire phase duplicated rotation");
    turn.turn={};(void)snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::non_fire);
    turn.turn.x=-.9F;
    ok &= Check(snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::non_fire)&&
        native_turns==std::vector<float>{-45,45},"left stick delivered rightward native yaw");
    // Failed native reload release must retain the held action history so a
    // subsequent unqueued gesture cannot mistake it for a fresh press.
    JavaPlayerBridge reload_history_bridge; Bind(reload_history_bridge);
    cojvr::runtime::GameplayInputState reload_history{};
    reload_history.active=true; reload_history.reload=true;
    const int reload_before=accepted[31];
    ok &= Check(reload_history_bridge.TryApplyGameplayInput(reload_history,&error,CoJInputDispatchPhase::pre_shoulder)
        && reload_history_bridge.AppliedGameplayCommands().reload,"delivered reload missing from applied history");
    reload_history.reload=false; reload_history.move={.3F,.5F}; fail_analog=true;
    ok &= Check(!reload_history_bridge.TryApplyGameplayInput(reload_history,&error,CoJInputDispatchPhase::pre_shoulder)
        && reload_history_bridge.AppliedGameplayCommands().reload,"failed release forgot held native reload");
    ok &= Check(reload_history_bridge.TryApplyGameplayInput(reload_history,&error,CoJInputDispatchPhase::pre_shoulder)
        && !reload_history_bridge.AppliedGameplayCommands().reload,"successful release retained native reload");
    reload_history.reload=true;
    ok &= Check(reload_history_bridge.TryApplyGameplayInput(reload_history,&error,CoJInputDispatchPhase::pre_shoulder)
        && accepted[31]==reload_before+2,"fresh reload did not reach native action after release");

    // Generated release pulses survive an analog failure and are acknowledged
    // independently when a later digital transition fails in the same batch.
    JavaPlayerBridge pulse_bridge; Bind(pulse_bridge);
    CoJGameplayCommandQueue commands;
    cojvr::runtime::GameplayInputState pulse{};
    pulse.active=true; pulse.move={.3F,.5F}; pulse.radial_confirmed=true;
    pulse.radial_highlight=0; pulse.equipment_select[2]=true; pulse.objectives=true;
    const int rifle_before=accepted[21],objectives_before=accepted[32];
    commands.CaptureAndMerge(pulse); fail_analog=true;
    ok &= Check(!pulse_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder),
        "fault fixture did not reject pulse batch");
    commands.Acknowledge(pulse,pulse_bridge.AppliedGameplayCommands());
    pulse={}; pulse.active=true; pulse.move={.3F,.5F}; commands.CaptureAndMerge(pulse);
    ok &= Check(pulse.equipment_select[2] && pulse.objectives && pulse.radial_confirmed,
        "analog failure lost radial release/objectives pulses");
    fail_digital_action=32;
    ok &= Check(!pulse_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder) &&
        accepted[21]==rifle_before+1 && accepted[32]==objectives_before,
        "partial digital fault did not isolate delivered rifle from pending objectives");
    commands.Acknowledge(pulse,pulse_bridge.AppliedGameplayCommands());
    pulse={}; pulse.active=true; pulse.move={.3F,.5F}; commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.equipment_select[2] && pulse.objectives && !pulse.radial_confirmed,
        "acknowledgement replayed delivered equipment or lost pending objectives");
    ok &= Check(pulse_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder) &&
        accepted[21]==rifle_before+1 && accepted[32]==objectives_before+1,
        "retry did not deliver each generated command exactly once");
    commands.Acknowledge(pulse,pulse_bridge.AppliedGameplayCommands());
    pulse={};pulse.active=true;commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.objectives&&!pulse.equipment_select[2],"successful pulses stayed queued");
    pulse.kick=true;commands.CaptureShoulder(pulse);
    pulse={};pulse.active=true;commands.CaptureShoulder(pulse);
    ok &= Check(pulse.kick,"failed shoulder dispatch lost kick pulse");
    pulse={};commands.CaptureAndMerge(pulse);pulse.active=true;commands.CaptureShoulder(pulse);
    ok &= Check(!pulse.kick,"context cancellation retained a pending kick");
    pulse.radial_confirmed=true;pulse.radial_highlight=5;pulse.discard_weapon=true;
    commands.CaptureAndMerge(pulse);pulse.discard_weapon=false;
    commands.Acknowledge(pulse,{});pulse={};pulse.active=true;commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.discard_weapon,"inventory denial left a deferred Throw command");
    pulse.objectives=true;commands.CaptureAndMerge(pulse);
    pulse={};pulse.active=true;pulse.input_context_generation=7;commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.objectives,"missed inactive sample across presenter generation kept a pending command");

    // A second pulse cannot mistake an unreleased first pulse for delivery.
    JavaPlayerBridge repeat_bridge;Bind(repeat_bridge);CoJGameplayCommandQueue repeated;
    pulse={};pulse.active=true;pulse.radial_confirmed=true;pulse.radial_highlight=0;pulse.equipment_select[2]=true;
    repeated.CaptureAndMerge(pulse);
    repeated.PrepareDispatch(pulse,repeat_bridge.AppliedGameplayCommands());
    const int repeat_before=accepted[21];
    ok &= Check(repeat_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder),"first repeated pulse failed");
    repeated.Acknowledge(pulse,repeat_bridge.AppliedGameplayCommands());
    pulse={};pulse.active=true;pulse.weapon_radial=true;repeated.CaptureAndMerge(pulse);
    pulse.move={.9F,0};fail_analog=true;
    ok &= Check(!repeat_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder),"failed wheel release fixture did not fail");
    pulse={};pulse.active=true;pulse.radial_confirmed=true;pulse.radial_highlight=0;pulse.equipment_select[2]=true;
    repeated.CaptureAndMerge(pulse);repeated.PrepareDispatch(pulse,repeat_bridge.AppliedGameplayCommands());
    ok &= Check(!pulse.equipment_select[2],"old held native pulse was mistaken for a new confirmation");
    ok &= Check(repeat_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder),"deferred neutral release failed");
    repeated.Acknowledge(pulse,repeat_bridge.AppliedGameplayCommands());
    pulse={};pulse.active=true;repeated.CaptureAndMerge(pulse);
    repeated.PrepareDispatch(pulse,repeat_bridge.AppliedGameplayCommands());
    ok &= Check(pulse.equipment_select[2] && repeat_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder) &&
        accepted[21]==repeat_before+2,"new pulse was lost/replayed after deferred release");
    repeated.Acknowledge(pulse,repeat_bridge.AppliedGameplayCommands());

    pulse={};pulse.active=true;pulse.kick=true;commands.CaptureShoulder(pulse);
    pulse.kick=false;pulse.digital_available&=~(1U<<9);commands.CaptureShoulder(pulse);
    pulse.digital_available|=1U<<9;commands.CaptureShoulder(pulse);
    ok &= Check(!pulse.kick,"R1 availability loss replayed a queued kick after recovery");
    pulse.objectives=true;commands.CaptureAndMerge(pulse);
    pulse.objectives=false;pulse.objectives_event_available=false;commands.CaptureAndMerge(pulse);
    pulse.objectives_event_available=true;commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.objectives,"Create availability loss replayed a queued Objectives pulse");

    pulse={};pulse.active=true;pulse.radial_confirmed=true;pulse.radial_highlight=5;pulse.discard_weapon=true;
    commands.CaptureAndMerge(pulse);
    pulse={};pulse.active=true;pulse.radial_available=false;commands.CaptureAndMerge(pulse);
    pulse.radial_available=true;commands.CaptureAndMerge(pulse);
    ok &= Check(!pulse.discard_weapon && !pulse.radial_confirmed,
        "Triangle availability loss replayed queued Throw after recovery");

    for (const bool shoulder_command:{false,true}) {
        JavaPlayerBridge held_bridge;Bind(held_bridge);CoJGameplayCommandQueue held_commands;
        const auto phase=shoulder_command?CoJInputDispatchPhase::shoulder:CoJInputDispatchPhase::pre_shoulder;
        const int id=shoulder_command?39:32, before=accepted[id];
        for (int confirmation=0;confirmation<2;++confirmation) {
            if (confirmation==1 && !shoulder_command) {
                pulse={};pulse.active=true;held_commands.CaptureAndMerge(pulse); // physical release, native release still pending
            }
            pulse={};pulse.active=true;pulse.kick=shoulder_command;pulse.objectives=!shoulder_command;
            if (shoulder_command) held_commands.CaptureShoulder(pulse);
            else held_commands.CaptureAndMerge(pulse);
            held_commands.PrepareDispatch(pulse,held_bridge.AppliedGameplayCommands(),shoulder_command);
            ok &= Check(held_bridge.TryApplyGameplayInput(pulse,&error,phase),"held command dispatch failed");
            held_commands.Acknowledge(pulse,held_bridge.AppliedGameplayCommands(),shoulder_command);
            if (confirmation==1) {
                ok &= Check(!pulse.kick&&!pulse.objectives,"new pulse did not first release old held command");
                pulse={};pulse.active=true;
                if (shoulder_command) held_commands.CaptureShoulder(pulse);
                else held_commands.CaptureAndMerge(pulse);
                held_commands.PrepareDispatch(pulse,held_bridge.AppliedGameplayCommands(),shoulder_command);
                ok &= Check(held_bridge.TryApplyGameplayInput(pulse,&error,phase),"pulse after neutral release failed");
                held_commands.Acknowledge(pulse,held_bridge.AppliedGameplayCommands(),shoulder_command);
            }
        }
        ok &= Check(accepted[id]==before+2,"Objectives/Kick acknowledgement lost a second pulse");
    }

    JavaPlayerBridge held_objectives_bridge;Bind(held_objectives_bridge);
    CoJGameplayCommandQueue held_objectives;
    pulse={};pulse.active=true;pulse.objectives=true;
    const int held_objectives_before=accepted[32];
    for (int poll=0;poll<3;++poll) {
        held_objectives.CaptureAndMerge(pulse);
        held_objectives.PrepareDispatch(pulse,held_objectives_bridge.AppliedGameplayCommands());
        ok &= Check(held_objectives_bridge.TryApplyGameplayInput(pulse,&error,CoJInputDispatchPhase::pre_shoulder),
            "custom held Objectives dispatch failed");
        held_objectives.Acknowledge(pulse,held_objectives_bridge.AppliedGameplayCommands());
    }
    ok &= Check(accepted[32]==held_objectives_before+1,"custom held Objectives became repeated generated pulses");

    // Real JNI F synchronously mounts before the explicit shoulder phase. A
    // changed owner clears both logical crouch and native transition history.
    JavaPlayerBridge context_bridge;Bind(context_bridge);context_bridge.being_=&player;
    CoJGameplayOwnerState owner;CoJContextualShoulderState context_shoulder;
    cojvr::runtime::GameplayControlMapper mapper;
    cojvr::runtime::GameplayInputState physical{};physical.active=true;
    mounted_value=false;(void)owner.Update(true,false,1);context_shoulder.Update(physical,true,false,1);
    (void)mapper.Update(physical);physical.crouch=true;
    auto mapped=mapper.Update(physical);
    ok &= Check(mapped.crouch&&context_bridge.TryApplyGameplayInput(mapped,&error,CoJInputDispatchPhase::pre_shoulder)
        && native_crouched,"manual crouch did not reach native Duck before mount");
    physical.crouch=false;(void)mapper.Update(physical);
    mapped.active=true;mapped.interact=true;mapped.kick=true;mount_on_interact=true;
    const int kick_before=accepted[39],run_before=accepted[18];
    ok &= Check(context_bridge.TryApplyGameplayInput(mapped,&error,CoJInputDispatchPhase::pre_shoulder)
        && mounted_value&&!native_crouched&&accepted[39]==kick_before&&accepted[18]==run_before,
        "pre-interaction shoulder ran before native F mounted/reset the controller");
    mount_on_interact=false;
    ok &= Check(owner.Update(true,mounted_value,1),"mount transition did not invalidate owner");
    mapper={};context_shoulder={};
    ok &= Check(context_bridge.TryApplyGameplayInput({},&error),"owner reset did not release owned history");
    physical.kick=true;mapped=mapper.Update(physical);
    context_shoulder.Update(mapped,true,mounted_value,1);
    ok &= Check(!mapped.crouch&&!mapped.kick&&!mapped.run&&
        context_bridge.TryApplyGameplayInput(mapped,&error,CoJInputDispatchPhase::shoulder)&&accepted[39]==kick_before,
        "mount reset retained crouch or turned held R1 into a horse kick/gallop");
    ok &= Check(owner.Update(true,false,1),"dismount transition did not invalidate owner");
    mapper={};context_shoulder={};(void)context_bridge.TryApplyGameplayInput({},&error);
    physical.kick=false;(void)mapper.Update(physical);physical.crouch=true;mapped=mapper.Update(physical);
    ok &= Check(mapped.crouch&&context_bridge.TryApplyGameplayInput(mapped,&error,CoJInputDispatchPhase::pre_shoulder)
        &&native_crouched,"fresh crouch after dismount was suppressed by stale native history");
    ok &= Check(owner.Update(true,false,2)&&owner.Update(false,false,0)&&!owner.Update(false,false,0),
        "player replacement/observation loss did not invalidate ownership exactly once");
    // Resolve the rider's request before native dispatch: action 16 is shared
    // with HorseController and can otherwise block the horse's CanRun gate.
    mounted_value=true;
    bool crouch_mounted=false;
    bool crouch_owner_known=context_bridge.TryObserveMounted(crouch_mounted);
    auto rider_input=cojvr::runtime::GameplayInputState{};rider_input.active=true;
    rider_input.crouch=ResolveCoJCrouchAction(true,true,crouch_owner_known,crouch_mounted);
    const int crouch_before=accepted[16],crouch_releases_before=releases[16];
    ok &= Check(context_bridge.TryApplyGameplayInput(rider_input,&error,CoJInputDispatchPhase::pre_shoulder)
        && !native_crouched && accepted[16]==crouch_before && releases[16]>crouch_releases_before,
        "rider crouch leaked to native Duck or retained the previous on-foot request");
    mounted_call_fault=true;
    crouch_owner_known=context_bridge.TryObserveMounted(crouch_mounted);
    rider_input.crouch=ResolveCoJCrouchAction(true,true,crouch_owner_known,crouch_mounted);
    ok &= Check(context_bridge.TryApplyGameplayInput(rider_input,&error,CoJInputDispatchPhase::pre_shoulder)
        && !native_crouched && accepted[16]==crouch_before && !pending,
        "unobserved owner acquired a synthetic crouch request");
    mounted_call_fault=false;mounted_value=false;
    crouch_owner_known=context_bridge.TryObserveMounted(crouch_mounted);
    rider_input.crouch=ResolveCoJCrouchAction(false,true,crouch_owner_known,crouch_mounted);
    ok &= Check(context_bridge.TryApplyGameplayInput(rider_input,&error,CoJInputDispatchPhase::pre_shoulder)
        && native_crouched && accepted[16]==crouch_before+1,
        "on-foot physical crouch did not recover after mounted/unknown ownership");
    const int turn_count=static_cast<int>(native_turns.size());
    turn.turn.x=.9F;
    ok &= Check(snap_bridge.TryApplyGameplayInput(turn,&error,CoJInputDispatchPhase::shoulder)&&
        static_cast<int>(native_turns.size())==turn_count,"shoulder phase generated an extra snap turn");
    env_table[6]=reinterpret_cast<void*>(&CampaignFind);
    env_table[21]=reinterpret_cast<void*>(&CampaignGlobal);
    env_table[22]=reinterpret_cast<void*>(&CampaignDeleteGlobal);
    env_table[23]=reinterpret_cast<void*>(&CampaignDeleteLocal);
    env_table[144]=reinterpret_cast<void*>(&CampaignField);
    for(bool returned_ref:{false,true})for(int failure=1;failure<=5;++failure){
        JavaPlayerBridge admission;
        pending=false;campaign_step=campaign_globals=campaign_locals=0;
        campaign_fail=failure;campaign_return_ref=returned_ref;
        ok &= Check(!admission.EnsureCampaignAccess(&env_holder,&error)&&!pending&&
            campaign_globals==0&&campaign_locals==0&&!admission.lawman_game_class_&&
            !admission.lawman_module_single_class_,
            "campaign lookup failure leaked a pending JNI exception/reference or partial admission");
    }
    pending=false;campaign_step=campaign_globals=campaign_locals=0;campaign_fail=-1;
    {
        JavaPlayerBridge admission;
        ok &= Check(admission.EnsureCampaignAccess(&env_holder,&error)&&campaign_globals==2&&
            campaign_locals==0&&!pending,"campaign lookup did not recover after failed admission");
        admission.vm_=&vm_holder;
        admission.Reset();
    }
    ok &= Check(campaign_globals==0,"campaign global owners were not restored on shutdown");
    return ok ? 0 : 1;
}
