// Independent JNI fixture derived from motion_reload_owner.cpp; real bridge ABI.
// Exercises the real bridge implementation against an independent JNI fixture.
// Native-owned active tickets are outside bridge cancellation ownership.
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include "games/call_of_juarez/focus_zoom.hpp"
#include "games/call_of_juarez/motion_reload_owner.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>

#if defined(_WIN32)
#define JNI_CALL __stdcall
#else
#define JNI_CALL
#endif
namespace {
using namespace cojvr::games::call_of_juarez;
union Value { std::int64_t alignment; void* object; std::int32_t integer; };
int player, weapon, replacement, player_class, pistol_class, other_class, exception;
int schofield_a_class, schofield_b_class;
int peacemaker_class, cogswell_class;
int weapon_alias;
void* table[174]{};
void** ticket_env_holder = table;
int refs = 0, step = 0, fail_step = -1;
bool pending = false, null_failure = false, dead = false, reload = false;
bool two[3][2]{}, carry[2]{}, attack[2]{};
bool replace_on_id = false;
std::array<void*, 2> actual{}, active{}, desired{};
std::array<int, 2> current{}, destiny{}, wanted{}, actual_state{};
void* weapon_class = &pistol_class;
int weapon_id = 123;
const char* fail_lookup = nullptr;
bool geometry_mode = false;
std::vector<int> optional_geometry_steps;
void Require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
bool Fail() {
    Require(!pending, "JNI operation after pending exception");
    if (++step != fail_step) return false;
    pending = !null_failure;
    return true;
}
void* Ref(void* object) { if (object) ++refs; return object; }
void* JNI_CALL Exception(void*) { return pending ? Ref(&exception) : nullptr; }
void JNI_CALL Clear(void*) { pending = false; }
void JNI_CALL Delete(void*, void* object) { if (object) --refs; }
void* JNI_CALL Class(void*, void* object) {
    if (Fail()) return nullptr;
    return Ref(object == &player ? &player_class : weapon_class);
}
void* JNI_CALL Find(void*, const char* name) {
    if (Fail() || (fail_lookup && !std::strcmp(name, fail_lookup))) {
        if (!null_failure) pending = true;
        return nullptr;
    }
    if (geometry_mode) {
        const char* exact = weapon_class == &peacemaker_class ? "WeaponPistolPeacemaker" :
            "WeaponPistolFrontier1878_Regular";
        if (std::strcmp(name, exact)) optional_geometry_steps.push_back(step);
    }
    if (!std::strcmp(name, "WeaponPistolPeacemaker")) return Ref(&peacemaker_class);
    if (!std::strcmp(name, "WeaponPistolCogswell")) return Ref(&cogswell_class);
    const bool whitelisted = !std::strcmp(name, "WeaponPistolFrontier1878_Regular") ||
        !std::strcmp(name, "WeaponPistolSchofield_A") ||
        !std::strcmp(name, "WeaponPistolSchofield_B");
    if (!whitelisted) {
        pending = true;
        return nullptr;
    }
    return Ref(!std::strcmp(name, "WeaponPistolFrontier1878_Regular") ? &pistol_class :
        !std::strcmp(name, "WeaponPistolSchofield_A") ? &schofield_a_class : &schofield_b_class);
}
std::uint8_t JNI_CALL Same(void*, void* a, void* b) {
    if (Fail()) return 0;
    if (geometry_mode && a == weapon_class && b != weapon_class)
        optional_geometry_steps.push_back(step);
    if (a == &weapon_alias) a = &weapon;
    if (b == &weapon_alias) b = &weapon;
    return a == b;
}
void* JNI_CALL Lookup(void*, void*, const char* name, const char* signature) {
    if (Fail() || (fail_lookup && !std::strcmp(name, fail_lookup))) {
        if (!null_failure) pending = true;
        return nullptr;
    }
    const bool object = !std::strcmp(name, "GetActualWeaponNotEmpty") ||
        !std::strcmp(name, "GetActiveWeapon") || !std::strcmp(name, "GetDesiredWeapon");
    const bool boolean = !std::strcmp(name, "HasSomethingInHand") ||
        !std::strcmp(name, "IsNotAlive") || !std::strcmp(name, "IsWeaponReloading") ||
        !std::strcmp(name, "GetAttackState") ||
        !std::strcmp(name, "IsActualWeaponOperatedTwoHand") ||
        !std::strcmp(name, "IsDesiredWeaponOperatedTwoHand") ||
        !std::strcmp(name, "IsHandStateMashineOperatedTwoHand");
    const bool integer = !std::strcmp(name, "GetHandStateMashineState") ||
        !std::strcmp(name, "GetHandStateMashineDestinyState") ||
        !std::strcmp(name, "GetDesiredWeaponState") ||
        !std::strcmp(name, "GetActualWeaponState") || !std::strcmp(name, "GetThisID");
    Require(object || boolean || integer, "unapproved helper lookup");
    Require(!std::strcmp(signature, object ? "(I)LWeapon;" :
        !std::strcmp(name, "GetThisID") ? "()I" :
        !std::strcmp(name, "IsNotAlive") || !std::strcmp(name, "IsWeaponReloading") ? "()Z" :
        boolean ? "(I)Z" : "(I)I"), "wrong method descriptor");
    return const_cast<char*>(name);
}
void* JNI_CALL Object(void*, void*, void* method, const Value* args) {
    if (Fail()) return nullptr;
    const auto name = static_cast<char*>(method); const int h = args[0].integer;
    Require(h == 0 || h == 1, "invalid hand");
    return Ref(!std::strcmp(name, "GetActualWeaponNotEmpty") ? actual[h] :
        !std::strcmp(name, "GetActiveWeapon") ? active[h] : desired[h]);
}
int JNI_CALL Integer(void*, void*, void* method, const Value* args) {
    if (Fail()) return 0;
    const auto name = static_cast<char*>(method);
    if (!std::strcmp(name, "GetThisID")) {
        if (replace_on_id) {
            for (int h = 0; h < 2; ++h) if (actual[h])
                actual[h] = active[h] = desired[h] = &replacement;
        }
        return weapon_id;
    }
    const int h = args[0].integer;
    return !std::strcmp(name, "GetHandStateMashineState") ? current[h] :
        !std::strcmp(name, "GetHandStateMashineDestinyState") ? destiny[h] :
        !std::strcmp(name, "GetActualWeaponState") ? actual_state[h] : wanted[h];
}
std::uint8_t JNI_CALL Boolean(void*, void*, void* method, const Value* args) {
    if (Fail()) return 0;
    const auto name = static_cast<char*>(method);
    if (!std::strcmp(name, "IsNotAlive")) return dead;
    if (!std::strcmp(name, "IsWeaponReloading")) return reload;
    return !std::strcmp(name, "HasSomethingInHand") ? carry[args[0].integer] :
        !std::strcmp(name, "GetAttackState") ? attack[args[0].integer] :
        two[!std::strcmp(name, "IsActualWeaponOperatedTwoHand") ? 0 :
            !std::strcmp(name, "IsDesiredWeaponOperatedTwoHand") ? 1 : 2][args[0].integer];
}
template<class T> void Set(int slot, T fn) { table[slot] = reinterpret_cast<void*>(fn); }
void Reset(int hand = 0) {
    actual = active = desired = {}; current = destiny = wanted = actual_state = {};
    actual[hand] = active[hand] = desired[hand] = &weapon;
    current[hand] = destiny[hand] = wanted[hand] = actual_state[hand] = 1;
    dead = reload = pending = null_failure = false;
    for (auto& row : two) row[0] = row[1] = false;
    carry[0] = carry[1] = replace_on_id = false;
    attack[0] = attack[1] = false;
    weapon_class = &pistol_class; weapon_id = 123;
    step = 0; fail_step = -1; fail_lookup = nullptr;
}
int Check(bool expected, int hand = 0) {
    CoJMotionReloadOwner out{1, 999, true};
    CoJMotionReloadDiagnostic diagnostic{};
    Require(ReadCoJMotionReloadOwner(&ticket_env_holder, &player, out, &diagnostic) == expected,
        "owner eligibility mismatch");
    Require(refs == 0 && !pending, "JNI exception/local reference leaked");
    Require(expected ? out.valid && out.armed_hand == hand &&
        out.weapon_id == static_cast<std::uint32_t>(weapon_id) :
        !out.valid && out.armed_hand == -1 && out.weapon_id == 0, "stale or wrong owner result");
    return step;
}
}
namespace {
void* vm_table[8]{};
void** vm_holder = vm_table;
void* pending_ticket = nullptr;
int active_ticket_object, ticket_field;
void* native_active_ticket = &active_ticket_object;
bool missing_field = false, fail_write = false, fail_clear = false;
int writes = 0, environment_calls = 0, global_creates = 0, global_deletes = 0;
std::int32_t JNI_CALL TicketGetEnv(void*, void** out, std::int32_t) {
    ++environment_calls;
    *out = &ticket_env_holder; return 0;
}
void JNI_CALL TicketDeleteGlobal(void*, void*) { ++global_deletes; }
void* JNI_CALL TicketGlobal(void*, void* object) { ++global_creates; return object; }
void* JNI_CALL TicketLookup(void* env, void* cls, const char* name, const char* signature) {
    if (!std::strcmp(name, "GetMeshElemFromBoneID") ||
        !std::strcmp(name, "GetPositionVectorVolatile") ||
        !std::strcmp(name, "SetPosition") || !std::strcmp(name, "RotateHorizontally")) {
        if (Fail()) return nullptr;
        return const_cast<char*>(name);
    }
    return Lookup(env, cls, name, signature);
}
void* JNI_CALL TicketField(void*, void*, const char* name, const char* signature) {
    if (Fail()) return nullptr;
    Require(!std::strcmp(name, "cojvrManualReloadWeapon") &&
        !std::strcmp(signature, "LWeapon;"), "bridge touched native-owned active ticket");
    if (missing_field) { pending = true; return nullptr; }
    return &ticket_field;
}
void JNI_CALL TicketWrite(void*, void* object, void* field, void* value) {
    Require(object == &player && field == &ticket_field, "wrong pending field owner");
    if (Fail() || (value ? fail_write : fail_clear)) { pending = true; return; }
    ++writes; pending_ticket = value;
}
void TicketReset(JavaPlayerBridge& bridge, int hand = 0) {
    Reset(hand);
    // NewGlobalRef is needed only by the explicit replacement/vector tests.
    // Leave unrelated ray/vector initialization unavailable during ticket reset.
    Set(21, static_cast<void*>(nullptr));
    bridge.vm_ = &vm_holder; bridge.being_ = &player;
    bridge.single_round_reload_pending_ = false;
    missing_field = fail_write = fail_clear = false;
    writes = environment_calls = global_creates = global_deletes = 0;
    pending_ticket = nullptr;
    native_active_ticket = &active_ticket_object;
}
int ArmCheck(JavaPlayerBridge& bridge, bool expected, int hand = 0) {
    const bool result = bridge.TryArmSingleRoundReload({hand, 123, true, true});
    Require(result == expected, "pending arm result mismatch");
    Require(refs == 0 && !pending, "pending arm leaked local reference or exception");
    Require(expected ? pending_ticket == &weapon : pending_ticket == nullptr,
        "pending field incorrectly published");
    Require(native_active_ticket == &active_ticket_object, "native active ticket modified");
    return step;
}
}
namespace {
// Independent Java-side storage: indices are deliberately sparse and not inferred
// from bridge targets. World X/up vectors are literal, with forward reconstructed
// by the production reader. No geometry helper computes our expected result.
int geometry_vector_class, geometry_constructor, geometry_x, geometry_y, geometry_z;
struct GeometryVector { float x, y, z; } geometry_vector{};
std::array<ElementWorldBasisTarget, 5> geometry_frames{};
std::vector<std::string> geometry_names;
int drum_index = 3, gate_index = 1, unavailable_element = -1;
int geometry_hand = 0;
bool replace_during_last_frame = false;
int replacement_reads = 0;
void* JNI_CALL GeometryString(void*, const char* text) {
    if (Fail()) return nullptr;
    geometry_names.emplace_back(text);
    return Ref(const_cast<char*>(text));
}
void* JNI_CALL GeometryLookup(void* env, void* cls, const char* name, const char* signature) {
    if (!std::strcmp(name,"GetElementID")) {
        Require(cls == weapon_class && !std::strcmp(signature,"(Ljava/lang/String;)I"),
            "geometry element lookup owner/descriptor mismatch");
        if (Fail()) return nullptr;
        return const_cast<char*>(name);
    }
    return TicketLookup(env,cls,name,signature);
}
int JNI_CALL GeometryInteger(void* env, void* object, void* method, const Value* args) {
    if (std::strcmp(static_cast<char*>(method),"GetElementID"))
        return Integer(env,object,method,args);
    Require(object == &weapon || object == &weapon_alias, "geometry element ID read wrong owner");
    if (Fail()) return -1;
    const auto text = static_cast<const char*>(args[0].object);
    const bool peacemaker = weapon_class == &peacemaker_class;
    if (!std::strcmp(text,peacemaker ? "ColtDrum" : "GunDrum")) return drum_index;
    if (!std::strcmp(text,peacemaker ? "ColtLock" : "GunLoader")) return gate_index;
    Require(false,"unexpected named geometry element"); return -1;
}
void* JNI_CALL GeometryNew(void*, void* cls, void* constructor, const Value*) {
    Require(cls == &geometry_vector_class && constructor == &geometry_constructor,
        "unexpected geometry vector construction");
    if (Fail()) return nullptr;
    geometry_vector = {}; return Ref(&geometry_vector);
}
std::uint8_t JNI_CALL GeometryBoolean(void* env, void* object, void* method, const Value* args) {
    const auto name = static_cast<char*>(method);
    if (std::strcmp(name,"GetElementPos") && std::strcmp(name,"GetElementLeftVector") &&
        std::strcmp(name,"GetElementUpVector")) return Boolean(env,object,method,args);
    Require(object == &weapon || object == &weapon_alias,"geometry frame read wrong owner");
    const int index = args[0].integer;
    Require(index >= 0 && index < 5 && args[1].object == &geometry_vector,
        "geometry frame read index/vector mismatch");
    if (Fail() || index == unavailable_element) return 0;
    const auto& frame = geometry_frames[index];
    const auto value = !std::strcmp(name,"GetElementPos") ? frame.position :
        !std::strcmp(name,"GetElementUpVector") ? frame.up :
        cojvr::runtime::Vec3{frame.up.y*frame.forward.z-frame.up.z*frame.forward.y,
            frame.up.z*frame.forward.x-frame.up.x*frame.forward.z,
            frame.up.x*frame.forward.y-frame.up.y*frame.forward.x};
    geometry_vector = {value.x,value.y,value.z};
    if (replace_during_last_frame && index == gate_index && !std::strcmp(name,"GetElementUpVector")) {
        actual[geometry_hand] = &replacement;
        ++replacement_reads;
    }
    return 1;
}
float JNI_CALL GeometryFloat(void*, void* object, void* field) {
    Require(object == &geometry_vector,"geometry read wrong vector");
    if (Fail()) return std::numeric_limits<float>::quiet_NaN();
    Require(field == &geometry_x || field == &geometry_y || field == &geometry_z,
        "geometry read unexpected field");
    return field == &geometry_x ? geometry_vector.x : field == &geometry_y ? geometry_vector.y : geometry_vector.z;
}
void GeometryReset(JavaPlayerBridge& bridge,int hand,bool peacemaker) {
    TicketReset(bridge,hand);
    geometry_mode = true; optional_geometry_steps.clear(); geometry_names.clear();
    weapon_class = peacemaker ? &peacemaker_class : &pistol_class;
    drum_index = 3; gate_index = 1; unavailable_element = -1;
    geometry_hand = hand; replace_during_last_frame = false; replacement_reads = 0;
    bridge.weapon_overlays_ = {};
    auto& overlay = bridge.weapon_overlays_[hand];
    overlay.weapon = &weapon; overlay.active = overlay.applied = true;
    geometry_frames = {};
    // Rotated drum: local X -> -Z, local +Z -> +X; translation is centimetres.
    geometry_frames[3] = {{100,200,300},{0,1,0},{1,0,0},true};
    geometry_frames[1] = {{110,210,310},{1,0,0},{0,0,1},true};
    overlay.targets.assign(geometry_frames.begin(),geometry_frames.end());
    bridge.element_world_read_lookup_attempted_ = true;
    bridge.get_element_position_method_ = const_cast<char*>("GetElementPos");
    bridge.get_element_left_method_ = const_cast<char*>("GetElementLeftVector");
    bridge.get_element_up_method_ = const_cast<char*>("GetElementUpVector");
    bridge.vector_class_ = &geometry_vector_class;
    bridge.vector_constructor_ = &geometry_constructor;
    bridge.vector_x_field_ = &geometry_x; bridge.vector_y_field_ = &geometry_y;
    bridge.vector_z_field_ = &geometry_z;
    Set(30,GeometryNew); Set(33,GeometryLookup); Set(39,GeometryBoolean);
    Set(51,GeometryInteger); Set(102,GeometryFloat); Set(167,GeometryString);
}
int GeometryCheck(JavaPlayerBridge& bridge,bool expected,int hand,bool peacemaker) {
    CoJReloadGeometrySnapshot result{};
    result.valid=true; result.model=CoJReloadModel::frontier;
    result.drum.valid=result.gate.valid=true;
    for(auto& mouth:result.mouths) mouth.valid=true;
    const bool observed=bridge.TryObserveReloadGeometry(hand,result);
    Require(observed==expected,"reload geometry acceptance mismatch");
    Require(refs==0 && !pending,"reload geometry leaked references/exception");
    Require(writes==0 && global_creates==0 && global_deletes==0 &&
        pending_ticket==nullptr && native_active_ticket==&active_ticket_object,
        "geometry observation mutated reload state/global ownership");
    if (!expected) {
        Require(!result.valid && result.model==CoJReloadModel::unknown &&
            !result.drum.valid && !result.gate.valid,"rejected geometry retained stale output");
        for(const auto& mouth:result.mouths) Require(!mouth.valid,"rejected geometry retained mouth");
    } else {
        Require(result.valid && result.model==(peacemaker?CoJReloadModel::peacemaker:CoJReloadModel::frontier),
            "geometry returned wrong exact model");
        Require(geometry_names==std::vector<std::string>{peacemaker?"ColtDrum":"GunDrum",
            peacemaker?"ColtLock":"GunLoader"},"geometry did not read exact named elements");
        Require(result.drum.position.x==100 && result.gate.position.x==110 &&
            result.drum.forward.x==1 && result.gate.up.x==1,"geometry lost displayed frames");
        const auto NearScalar=[](float a,float b){return std::abs(a-b)<.0001F;};
        // Literal independently transformed world coordinates from the mesh audit.
        constexpr std::array<cojvr::runtime::Vec3,6> expected_peacemaker{{
            {97.616722F,201.424893F,300.000764F},{97.616722F,200.712396F,301.234752F},
            {97.616722F,200.712472F,298.766768F},{97.616722F,199.287538F,301.234718F},
            {97.616722F,198.575109F,300.000696F},{97.616722F,199.287606F,298.766734F}}};
        constexpr std::array<cojvr::runtime::Vec3,6> expected_frontier{{
            {97.992788F,200.009727F,298.803870F},{97.991197F,201.036132F,299.408097F},
            {97.989515F,199.989578F,301.185826F},{97.992708F,198.973269F,299.390644F},
            {97.989576F,201.025625F,300.598805F},{97.991087F,198.963192F,300.581604F}}};
        const auto& expected_mouths=peacemaker?expected_peacemaker:expected_frontier;
        for(std::size_t i=0;i<result.mouths.size();++i) {
            const auto& frame=result.mouths[i]; const auto expected_position=expected_mouths[i];
            Require(frame.valid && frame.forward.x==1 && frame.up.y==1 &&
                NearScalar(frame.position.x,expected_position.x) && NearScalar(frame.position.y,expected_position.y) &&
                NearScalar(frame.position.z,expected_position.z),
                "geometry lost literal mouth positions or displayed drum rotation");
        }
    }
    return step;
}
void ReloadGeometryTests(JavaPlayerBridge& bridge) {
    for(int hand=0;hand<2;++hand) for(bool peacemaker:{false,true}) {
        GeometryReset(bridge,hand,peacemaker);
        const int calls=GeometryCheck(bridge,true,hand,peacemaker);
        const auto optional=optional_geometry_steps;
        for(bool null_only:{false,true}) for(int operation=1;operation<=calls;++operation) {
            GeometryReset(bridge,hand,peacemaker); null_failure=null_only; fail_step=operation;
            const bool benign=std::find(optional.begin(),optional.end(),operation)!=optional.end();
            GeometryCheck(bridge,benign,hand,peacemaker);
        }
        for(int fault=0;fault<23;++fault) {
            GeometryReset(bridge,hand,peacemaker);
            auto& overlay=bridge.weapon_overlays_[hand];
            switch(fault) {
            case 0: overlay.applied=false; break;
            case 1: overlay.active=false; break;
            case 2: overlay.weapon=nullptr; break;
            case 3: overlay.weapon=&replacement; break;
            case 4: actual[hand]=nullptr; break;
            case 5: weapon_class=&schofield_a_class; break;
            case 6: drum_index=-1; break;
            case 7: gate_index=-1; break;
            case 8: drum_index=5; break;
            case 9: gate_index=5; break;
            case 10: overlay.targets[3].valid=false; break;
            case 11: overlay.targets[1].valid=false; break;
            case 12: geometry_frames[3].position.x+=.101F; break;
            case 13: geometry_frames[1].position.z+=.101F; break;
            case 14: geometry_frames[3].up={0,0,1}; break;
            case 15: geometry_frames[1].forward={0,1,0}; break;
            case 16: unavailable_element=3; break;
            case 17: unavailable_element=1; break;
            case 18: geometry_frames[3].position.y=std::numeric_limits<float>::quiet_NaN(); break;
            case 19: overlay.targets.clear(); break;
            case 20: drum_index=2; break; // unmapped/invalid native element frame
            case 21: bridge.being_=nullptr; break;
            case 22: bridge.vm_=nullptr; break;
            }
            GeometryCheck(bridge,false,hand,peacemaker);
        }
        GeometryReset(bridge,hand,peacemaker);
        replace_during_last_frame=true;
        GeometryCheck(bridge,false,hand,peacemaker);
        Require(replacement_reads==1 && geometry_names.size()==2,
            "replacement regression did not reach final named frame read");
        GeometryReset(bridge,hand,peacemaker);
        bridge.weapon_overlays_[hand].weapon=&weapon_alias;
        GeometryCheck(bridge,true,hand,peacemaker);
        for(int slot:{6,15,17,23,24,30,31,33,36,39,51,102,167}) {
            GeometryReset(bridge,hand,peacemaker); auto saved=table[slot]; table[slot]=nullptr;
            GeometryCheck(bridge,false,hand,peacemaker); table[slot]=saved;
        }
        for(int invalid:{-1,2}) {
            GeometryReset(bridge,hand,peacemaker); GeometryCheck(bridge,false,invalid,peacemaker);
            Require(step==0,"invalid geometry hand performed JNI work");
        }
    }
    // Borrowed test handles are not bridge-owned global references/transactions.
    bridge.weapon_overlays_={}; bridge.vector_class_=nullptr;
    bridge.being_=nullptr; geometry_mode=false;
}
}

int main() {
    Set(6, Find); Set(15, Exception); Set(17, Clear); Set(23, Delete);
    Set(24, Same); Set(31, Class); Set(33, TicketLookup);
    Set(36, Object); Set(39, Boolean); Set(51, Integer);
    Set(94, TicketField); Set(104, TicketWrite); Set(22, TicketDeleteGlobal);
    vm_table[6] = reinterpret_cast<void*>(TicketGetEnv);
    JavaPlayerBridge bridge;
    TicketReset(bridge);
    Set(21, TicketGlobal);
    // FindClass(Vector) in this focused fixture returns null WITH an exception.
    // It must be cleared before Reset can attempt its next JNI cleanup route.
    Require(!bridge.EnsureVectorAccess(&ticket_env_holder, nullptr) && !pending && refs == 0,
        "null Vector lookup short-circuited exception cleanup");
    const int vector_failure_step = step;
    Require(Class(&ticket_env_holder, &player) != nullptr && step == vector_failure_step + 1,
        "JNI could not continue after failed Vector lookup");
    Delete(&ticket_env_holder, &player_class);
    Require(refs == 0, "Vector lookup regression leaked local references");
    TicketReset(bridge);
    missing_field = true;
    std::string error = "old error";
    for (int frame = 0; frame < 100; ++frame)
        Require(bridge.ClearSingleRoundReload(&error), "unowned clear failed");
    Require(environment_calls == 0 && step == 0 && writes == 0 && error.empty(),
        "unowned per-frame clear performed JNI work on unpatched profile");
    bridge.vm_ = nullptr; bridge.being_ = nullptr;
    Require(bridge.ClearSingleRoundReload() && environment_calls == 0,
        "unowned clear required VM/player");
    for (int hand = 0; hand < 2; ++hand) {
        TicketReset(bridge, hand); const int calls = ArmCheck(bridge, true, hand);
        for (int operation = 1; operation <= calls; ++operation) {
            TicketReset(bridge, hand); fail_step = operation; ArmCheck(bridge, false, hand);
        }
        TicketReset(bridge, hand); weapon_class = &peacemaker_class; ArmCheck(bridge, true, hand);
        for (void* cls : {static_cast<void*>(&schofield_a_class),
            static_cast<void*>(&schofield_b_class), static_cast<void*>(&other_class)}) {
            TicketReset(bridge, hand); weapon_class = cls; ArmCheck(bridge, false, hand);
        }
        TicketReset(bridge, hand); active[hand] = desired[hand] = &weapon_alias;
        ArmCheck(bridge, true, hand); // jobject handles differ; identity matches.
        TicketReset(bridge, hand); active[hand] = &replacement; ArmCheck(bridge, false, hand);
        TicketReset(bridge, hand); desired[hand] = &replacement; ArmCheck(bridge, false, hand);
        TicketReset(bridge, hand); replace_on_id = true; ArmCheck(bridge, false, hand);
        TicketReset(bridge, hand); weapon_id = 124; ArmCheck(bridge, false, hand);
        TicketReset(bridge, hand); missing_field = true; ArmCheck(bridge, false, hand);
        Require(writes == 0, "missing patch field caused action");
        TicketReset(bridge, hand); fail_write = true; ArmCheck(bridge, false, hand);
        TicketReset(bridge, hand); fail_write = fail_clear = true;
        ArmCheck(bridge, false, hand);
        Require(bridge.single_round_reload_pending_, "failed publication lost cleanup ownership");
        const auto generation = bridge.being_generation_;
        void* retained_vm = bridge.vm_;
        bridge.Reset();
        Require(bridge.being_ == &player && bridge.vm_ == retained_vm &&
            bridge.being_generation_ == generation && bridge.single_round_reload_pending_,
            "failed-publication reset discarded owner/VM/generation");
        Set(21, TicketGlobal);
        Require(!bridge.ResolveBeingMethods(&ticket_env_holder, &player, nullptr) &&
            bridge.being_ == &player && bridge.being_generation_ == generation &&
            bridge.single_round_reload_pending_ && global_creates == global_deletes,
            "replacement advanced generation or leaked candidate after failed cancellation");
        Set(21, static_cast<void*>(nullptr));
        Require(refs == 0 && !pending, "failed replacement leaked JNI state");
        fail_clear = false;
        Require(bridge.ClearSingleRoundReload() && !bridge.single_round_reload_pending_,
            "failed publication could not retry cancellation");
        const int clear_step = step, clear_env_calls = environment_calls;
        missing_field = true;
        Require(bridge.ClearSingleRoundReload() && step == clear_step &&
            environment_calls == clear_env_calls, "cleared ticket repeated JNI work");
        TicketReset(bridge, hand); ArmCheck(bridge, true, hand);
        reload = true;
        CoJMotionReloadOwner observation{};
        Require(!bridge.TryObserveMotionReloadOwner(observation) && pending_ticket == &weapon,
            "ordinary reload observation canceled pending ticket");
        Require(bridge.ClearSingleRoundReload() && !pending_ticket,
            "post-dispatch cancellation did not clear pending");
        Require(native_active_ticket == &active_ticket_object,
            "post-dispatch clear canceled native completion");
        TicketReset(bridge, hand); ArmCheck(bridge, true, hand);
        // Model Java admission consuming pending; completion now owns active.
        native_active_ticket = pending_ticket; pending_ticket = nullptr;
        Require(bridge.ClearSingleRoundReload() && native_active_ticket == &weapon,
            "pending clear after accepted admission canceled active reload");
        bridge.Reset();
        Require(native_active_ticket == &weapon,
            "reset after admission canceled native-owned active reload");
        TicketReset(bridge, hand); ArmCheck(bridge, true, hand);
        fail_clear = true;
        bridge.Reset();
        Require(bridge.being_ == &player && bridge.single_round_reload_pending_,
            "failed cancellation released pending owner");
        fail_clear = false;
        bridge.Reset();
        Require(!bridge.being_ && !pending_ticket && !bridge.single_round_reload_pending_,
            "reset did not clear pending before releasing owner");
        Require(native_active_ticket == &active_ticket_object && refs == 0 && !pending,
            "reset damaged native completion or JNI lifetime");
        TicketReset(bridge, hand); ArmCheck(bridge, true, hand);
        bridge.ClearBeing(&ticket_env_holder);
        Require(!bridge.being_ && !pending_ticket, "owner release retained pending ticket");
        TicketReset(bridge, hand); ArmCheck(bridge, true, hand);
        Require(bridge.TryApplyGameplayInput({}) && !pending_ticket,
            "inactive context did not cancel pending ticket");
    }
    for (int slot : {6, 15, 17, 23, 24, 31, 33, 36, 39, 51, 94, 104}) {
        TicketReset(bridge); auto saved = table[slot]; table[slot] = nullptr;
        ArmCheck(bridge, false); table[slot] = saved;
    }
    TicketReset(bridge);
    Require(!bridge.TryArmSingleRoundReload({-1, 123, true, true}) && writes == 0,
        "invalid hand accepted");
    Require(!bridge.TryArmSingleRoundReload({1, 123, true, true}) && writes == 0,
        "stale armed hand accepted");
    Require(!bridge.TryArmSingleRoundReload({0, 0, true, true}) && writes == 0,
        "invalid ID accepted");
    Require(!bridge.TryArmSingleRoundReload({0, 123, false, true}) && writes == 0,
        "invalid owner accepted");
    Require(!bridge.TryArmSingleRoundReload({0, 123, true}) && writes == 0,
        "owner without per-round support accepted");
    ReloadGeometryTests(bridge);
}
