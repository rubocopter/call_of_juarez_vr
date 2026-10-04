// Execute the real JNI dispatcher against a controller that rejects one-shot
// actions while LockApplyControllerState is held, just like shipped CoJ.
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <iostream>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"

namespace {
using namespace cojvr::games::call_of_juarez;
enum Id { actions_field=1, targets_field, target_field, next_field, lock_method,
    unlock_method, apply_method, eligible_method, direct_method, translate_method,
    device_field, button_field, sign_field, origin_field, direction_field, x_field, y_field, z_field };
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
struct Vector { float x=0,y=0,z=0; } origin_vector, direction_vector;
void* published_origin=nullptr;
void* published_direction=nullptr;
int construction_count=0;
bool vector_write_fault=false;
int observed_weapon;
bool shot_field_fault=false, shot_read_fault=false;
constexpr const char* shot_fields[]{"cojvrShotSerial","cojvrHitSerial","cojvrFxSerial",
    "cojvrFxStatus","cojvrShotSuppressEffects","cojvrCombHandle","cojvrSmokeHandle"};
constexpr int shot_values[]{12,9,12,63,0,101,202};
std::array<int,48> accepted{}, rejected{};
std::array<int,2> native_hand_attacks{};

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
void* __stdcall ObserveMethod(void*,void*,const char*,const char*) { return Ptr(301); }
void* __stdcall ObserveObject(void*,void*,void*,const JValue* args) {
    return args[0].i==0 ? &observed_weapon : nullptr;
}
std::int32_t __stdcall TargetType(void*, void*, void*, const JValue*) { return 1; }
std::uint8_t __stdcall Boolean(void*, void* owner, void* method, const JValue* args) {
    if (method==Ptr(eligible_method)) return 1;
    if (method==Ptr(translate_method) && args[3].z) {
        const int action=static_cast<Action*>(owner)->id;
        ++(locked ? rejected[action] : accepted[action]);
        // Shipped PlayerController.ExecuteInput's tableswitch converts action
        // 10 to ApplyAttack(0/right), and action 9 to ApplyAttack(1/left).
        if (!locked && (action==9 || action==10))
            ++native_hand_attacks[action==10 ? 0 : 1];
        return !locked;
    }
    return 1;
}
void __stdcall Void(void*, void*, void* method, const JValue*) {
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
    CoJWeaponShotDiagnostics diagnostics{};
    ok &= Check(bridge.TryObserveWeaponShotDiagnostics(0,diagnostics) && diagnostics.valid &&
        diagnostics.values==std::array<int,5>{12,9,12,63,0}, "actual native shot counters were not read");
    ok &= Check(diagnostics.fx_handles==std::array<int,2>{101,202},
        "committed native FX handles were not observed");
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
    return ok ? 0 : 1;
}
