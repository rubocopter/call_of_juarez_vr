#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/motion_reload_adapter.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

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
void** holder = table;
int refs = 0, step = 0, fail_step = -1;
bool pending = false, null_failure = false, dead = false, reload = false;
bool two[3][2]{}, carry[2]{}, attack[2]{};
bool unexplained_occupied[2]{};
bool replace_on_id = false;
enum class RecoveryMutation { none, attack, death, carry, attack_state, alternate_state,
    support_state, active_weapon, desired_weapon, identity, unexplained_occupation };
RecoveryMutation recovery_mutation = RecoveryMutation::none;
std::array<void*, 2> actual{}, active{}, desired{};
std::array<int, 2> current{}, destiny{}, wanted{}, actual_state{};
void* weapon_class = &pistol_class;
int weapon_id = 123;
const char* fail_lookup = nullptr;
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
        !std::strcmp(name, "IsCarrying") ||
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
        !std::strcmp(name, "IsNotAlive") || !std::strcmp(name, "IsWeaponReloading") ||
        !std::strcmp(name, "IsCarrying") ? "()Z" :
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
        const int id = weapon_id;
        if (replace_on_id) {
            for (int h = 0; h < 2; ++h) if (actual[h])
                actual[h] = active[h] = desired[h] = &replacement;
        }
        const int armed = actual[0] ? 0 : 1;
        switch (recovery_mutation) {
        case RecoveryMutation::none: break;
        case RecoveryMutation::attack: attack[1-armed]=true; break;
        case RecoveryMutation::death: dead=true; break;
        case RecoveryMutation::carry: carry[1-armed]=true; break;
        case RecoveryMutation::attack_state: current[armed]=32; break;
        case RecoveryMutation::alternate_state: wanted[armed]=149; break;
        case RecoveryMutation::support_state: current[1-armed]=20; break;
        case RecoveryMutation::active_weapon: active[armed]=&replacement; break;
        case RecoveryMutation::desired_weapon: desired[armed]=&replacement; break;
        case RecoveryMutation::identity: ++weapon_id; break;
        case RecoveryMutation::unexplained_occupation:
            two[2][armed]=false;unexplained_occupied[1-armed]=true;break;
        }
        recovery_mutation=RecoveryMutation::none;
        return id;
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
    if (!std::strcmp(name, "IsCarrying")) return carry[0] || carry[1];
    if (!std::strcmp(name, "HasSomethingInHand")) {
        const int h=args[0].integer, other=1-h;
        // Shipped ArmedPlayerBeing.HasWeaponInHand includes an opposite-hand
        // two-hand state even when this hand's current state is empty. The
        // inherited HumanBeing.IsCarrying is a global carried-object check.
        return current[h]!=0 || (current[other]!=0 && two[2][other]) ||
            carry[0] || carry[1] || unexplained_occupied[h];
    }
    return
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
    unexplained_occupied[0]=unexplained_occupied[1]=false;
    recovery_mutation=RecoveryMutation::none;
    attack[0] = attack[1] = false;
    weapon_class = &pistol_class; weapon_id = 123;
    step = 0; fail_step = -1; fail_lookup = nullptr;
}
int Check(bool expected, int hand = 0) {
    CoJMotionReloadOwner out{1, 999, true};
    CoJMotionReloadDiagnostic diagnostic{};
    Require(ReadCoJMotionReloadOwner(&holder, &player, out, &diagnostic) == expected,
        "owner eligibility mismatch");
    Require(refs == 0 && !pending, "JNI exception/local reference leaked");
    Require(expected ? out.valid && out.armed_hand == hand &&
        out.weapon_id == static_cast<std::uint32_t>(weapon_id) :
        !out.valid && out.armed_hand == -1 && out.weapon_id == 0, "stale or wrong owner result");
    return step;
}
void ReloadRecovery(int hand) {
    Reset(hand);
    current[hand]=22; destiny[hand]=wanted[hand]=actual_state[hand]=1;
    for (auto& flags : two) flags[0]=flags[1]=true;
}
int CheckRecovery(bool expected, int hand=0) {
    CoJMotionReloadOwner out{1,999,true,true};
    CoJMotionReloadDiagnostic diagnostic{};
    Require(ReadCoJNativeReloadRecoveryOwner(&holder,&player,out,&diagnostic)==expected,
        "passive native reload recovery eligibility mismatch");
    Require(refs==0 && !pending,"recovery reader leaked exception/local reference");
    Require(expected ? out.valid && out.single_round_supported && out.armed_hand==hand &&
        out.weapon_id==123 && diagnostic.reason==CoJMotionReloadRejectReason::none :
        !out.valid && !out.single_round_supported && out.armed_hand==-1 && out.weapon_id==0,
        "recovery reader published stale or wrong ownership");
    return step;
}
void EmptySupportNativeAnimationOccupancy() {
    // Fresh run:20/21/21/21 armed, all support states0 and no support weapon.
    // Treating HasSomethingInHand as a carried-object predicate cancels an
    // accepted native reload before its next token can be replenished.
    for(int hand=0;hand<2;++hand) {
        ReloadRecovery(hand);
        current[hand]=20;destiny[hand]=wanted[hand]=actual_state[hand]=21;reload=true;
        Check(false,hand); // Ordinary admission remains ineligible.
        CoJMotionReloadOwner owner{}; CoJMotionReloadDiagnostic diagnostic{};
        Require(ReadCoJNativeReloadRecoveryOwner(&holder,&player,owner,&diagnostic) &&
            owner.valid && owner.single_round_supported && owner.armed_hand==hand &&
            owner.weapon_id==123 && diagnostic.occupied[1-hand],
            "empty support native two-hand reload must retain recovery ownership");
        Require(refs==0 && !pending,"native animation occupancy leaked JNI state");
        for(int carried_hand=0;carried_hand<2;++carried_hand) {
            ReloadRecovery(hand);carry[carried_hand]=true;CheckRecovery(false,hand);
        }
        ReloadRecovery(hand);two[2][hand]=false;
        unexplained_occupied[1-hand]=true;CheckRecovery(false,hand);
        Reset(hand);unexplained_occupied[1-hand]=true;Check(false,hand);
        Reset(hand);fail_lookup="IsCarrying";Check(true,hand);
        // Neither an active/destination two-hand flag nor an unknown occupied
        // hand proves the state-machine fallback that caused the live failure.
        ReloadRecovery(hand);two[2][hand]=false;
        CheckRecovery(true,hand);
        for(const char* name:{"IsCarrying"}) {
            for(bool without_exception:{false,true}) {
                ReloadRecovery(hand);fail_lookup=name;null_failure=without_exception;
                CheckRecovery(false,hand);
            }
        }
    }
}
void NativeReloadRecovery() {
    // Native desired state returns to1 while the current reload animation and
    // its two-hand flags still own rendering. Admission must stay ineligible;
    // the passive reader validates only the existing exact native owner.
    for (int hand=0; hand<2; ++hand) {
        ReloadRecovery(hand); Check(false);
        ReloadRecovery(hand); const int calls=CheckRecovery(true,hand);
        for (void* cls : {static_cast<void*>(&pistol_class),static_cast<void*>(&peacemaker_class)}) {
            for (const auto& states : {std::array<int,4>{20,21,21,21},
                    std::array<int,4>{21,22,21,21},std::array<int,4>{22,1,1,1},
                    std::array<int,4>{1,22,1,1},std::array<int,4>{1,1,1,22}}) {
                ReloadRecovery(hand); weapon_class=cls;
                current[hand]=states[0]; destiny[hand]=states[1];
                wanted[hand]=states[2]; actual_state[hand]=states[3];
                reload=states[2]!=1; CheckRecovery(true,hand);
            }
        }
        ReloadRecovery(hand); active[hand]=desired[hand]=&weapon_alias; CheckRecovery(true,hand);
        // Both observations must validate the same context, not only the first.
        for (auto mutation : {RecoveryMutation::attack,RecoveryMutation::death,
                RecoveryMutation::carry,RecoveryMutation::attack_state,RecoveryMutation::alternate_state,
                RecoveryMutation::support_state,RecoveryMutation::active_weapon,
                RecoveryMutation::desired_weapon,RecoveryMutation::identity,
                RecoveryMutation::unexplained_occupation}) {
            ReloadRecovery(hand); recovery_mutation=mutation; CheckRecovery(false);
        }
        ReloadRecovery(hand); replace_on_id=true; CheckRecovery(false);
        for (int operation=1; operation<=calls; ++operation) {
            ReloadRecovery(hand); fail_step=operation; CheckRecovery(false);
        }
        for (auto states : {&current,&destiny,&wanted,&actual_state}) {
            for (int value : {-1,0,3,12,31,32,129,148,149,150}) {
                ReloadRecovery(hand); (*states)[hand]=value; CheckRecovery(false);
            }
            for (int value : {1,20,21,22,32}) {
                ReloadRecovery(hand); (*states)[1-hand]=value; CheckRecovery(false);
            }
        }
        ReloadRecovery(hand); current[hand]=1; CheckRecovery(false);
        for (auto weapons : {&actual,&active,&desired}) {
            ReloadRecovery(hand); (*weapons)[hand]=nullptr; CheckRecovery(false);
            ReloadRecovery(hand); (*weapons)[hand]=&replacement; CheckRecovery(false);
            ReloadRecovery(hand); (*weapons)[1-hand]=&weapon; CheckRecovery(false);
        }
        for (void* cls : {static_cast<void*>(&schofield_a_class),static_cast<void*>(&schofield_b_class),
                static_cast<void*>(&other_class),static_cast<void*>(&cogswell_class)}) {
            ReloadRecovery(hand); weapon_class=cls; CheckRecovery(false);
        }
        ReloadRecovery(hand); dead=true; CheckRecovery(false);
        ReloadRecovery(hand); carry[1-hand]=true; CheckRecovery(false);
        for (int h=0; h<2; ++h) {
            ReloadRecovery(hand); attack[h]=true; CheckRecovery(false);
        }
        for (int id : {0,-1}) { ReloadRecovery(hand); weapon_id=id; CheckRecovery(false); }
    }
    for (const char* name : {"WeaponPistolFrontier1878_Regular","WeaponPistolPeacemaker",
            "GetActualWeaponNotEmpty","GetActiveWeapon","GetDesiredWeapon","GetHandStateMashineState",
            "GetHandStateMashineDestinyState","GetDesiredWeaponState","GetActualWeaponState",
            "HasSomethingInHand","IsCarrying","IsNotAlive","IsWeaponReloading","GetThisID",
            "IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand",
            "IsHandStateMashineOperatedTwoHand","GetAttackState"}) {
        for (bool without_exception : {false,true}) {
            ReloadRecovery(0); fail_lookup=name; null_failure=without_exception; CheckRecovery(false);
        }
    }
    ReloadRecovery(0); pending=true; CheckRecovery(false);
    CoJMotionReloadOwner out{0,123,true,true};
    Require(!ReadCoJNativeReloadRecoveryOwner(nullptr,&player,out) && !out.valid,
        "recovery reader accepted null JNI environment");
    Require(!ReadCoJNativeReloadRecoveryOwner(&holder,nullptr,out) && !out.valid,
        "recovery reader accepted null player");
    for (int slot : {6,15,17,23,24,31,33,36,39,51}) {
        ReloadRecovery(0); auto saved=table[slot]; table[slot]=nullptr;
        CheckRecovery(false); table[slot]=saved;
    }
}
void NativeReloadContinuationIntegration() {
    using namespace cojvr::runtime;
    CoJMotionReloadAdapter adapter;
    GameplayInputState raw{}, mapped{};
    raw.active=true; raw.input_context_generation=4;
    const auto pose=[](Vec3 p) { Pose result{}; result.position=p;
        result.position_valid=result.orientation_valid=true; return result; };
    const auto head=pose({0,1.6F,0});
    auto left=pose({-.2F,1.05F,-.1F}); const auto right=pose({.2F,1.35F,-.35F});
    std::uint64_t sequence=0, now=1000;
    const auto poll=[&]() {
        CoJMotionReloadOwner owner{}, recovery{};
        CoJMotionReloadDiagnostic diagnostic{};
        if (!ReadCoJMotionReloadOwner(&holder,&player,owner,&diagnostic))
            (void)ReadCoJNativeReloadRecoveryOwner(&holder,&player,recovery);
        Require(refs==0 && !pending,"integrated reload poll leaked JNI observation state");
        mapped=raw; ++sequence; now+=50;
        return adapter.Update(mapped,raw,owner,diagnostic,7,head,left,right,
            true,false,sequence,now,3,recovery);
    };
    Reset(); (void)poll(); raw.fire_left=true;
    Require(poll().consume_free_trigger,"integrated pickup must claim the empty hand");
    left=right; (void)poll(); raw.fire_left=false;
    Require(poll().reload,"integrated insertion must request one native round");
    adapter.NoteSingleRoundDispatch(true);
    current[0]=20; destiny[0]=wanted[0]=actual_state[0]=21; reload=true;
    for (auto& flags : two) flags[0]=flags[1]=true;
    raw.fire_left=true;
    const auto native=poll();
    Require(!native.cartridge_held && native.consumed_trigger_mask==2 && !mapped.fire_left,
        "actual recovery reader must protect support press while native animation owns rendering");
    current[0]=22; destiny[0]=wanted[0]=actual_state[0]=1; reload=false;
    Require(poll().consumed_trigger_mask==2 && !mapped.fire_left,
        "actual reader must preserve continuation through native two-hand end animation");
    Reset();
    Require(poll().cartridge_held && !mapped.fire_left,
        "actual same ordinary owner must replenish without releasing a held trigger into native fire");
    raw.fire_left=false; Require(!poll().reload,"integrated release only rearms the replenished cartridge");
    raw.fire_left=true; (void)poll(); raw.fire_left=false;
    Require(poll().reload,"fresh integrated insertion must request exactly the next round");
    adapter.NoteSingleRoundDispatch(true);
    current[0]=wanted[0]=actual_state[0]=21; destiny[0]=22; reload=true; (void)poll();
    current[0]=32; destiny[0]=wanted[0]=actual_state[0]=1; reload=false;
    (void)poll(); Reset();
    Require(!poll().cartridge_held,"actual state32 rejection must irreversibly cancel the continuation");
}
}
int main() {
    Set(6, Find); Set(15, Exception); Set(17, Clear); Set(23, Delete);
    Set(24, Same); Set(31, Class); Set(33, Lookup);
    Set(36, Object); Set(39, Boolean); Set(51, Integer);
    EmptySupportNativeAnimationOccupancy();
    NativeReloadRecovery();
    NativeReloadContinuationIntegration();
    for (int hand = 0; hand < 2; ++hand) {
        Reset(hand); const int calls = Check(true, hand); // RED against the inert stub.
        {
            CoJMotionReloadOwner out{};
            Require(ReadCoJMotionReloadOwner(&holder, &player, out) && out.single_round_supported,
                "Frontier must admit cartridge-paced native completion");
            for (void* cls : {static_cast<void*>(&schofield_a_class), static_cast<void*>(&schofield_b_class)}) {
                Reset(hand); weapon_class = cls;
                Require(ReadCoJMotionReloadOwner(&holder, &player, out) && !out.single_round_supported,
                    "Schofield must retain whole-clip hybrid ownership");
            }
        }
        for (int operation = 1; operation <= calls; ++operation) {
            Reset(hand); fail_step = operation; Check(false);
        }
        for (auto states : {&current, &destiny, &wanted, &actual_state})
            for (int h = 0; h < 2; ++h) for (int state : {-1, 3, 12, 20, 21, 22, 31, 129, 149}) {
                Reset(hand); (*states)[h] = state; Check(false);
            }
        for (auto weapons : {&actual, &active, &desired}) {
            Reset(hand); (*weapons)[hand] = nullptr; Check(false);
            Reset(hand); (*weapons)[hand] = &replacement; Check(false);
            Reset(hand); (*weapons)[1-hand] = &weapon; Check(false);
        }
        Reset(hand); active[hand] = desired[hand] = &weapon_alias; Check(true, hand);
        Reset(hand); weapon_class = &other_class; Check(false);
        Reset(hand); weapon_class = &peacemaker_class; Check(true, hand);
        {
            CoJMotionReloadOwner out{};
            Require(ReadCoJMotionReloadOwner(&holder, &player, out) && out.single_round_supported,
                "Peacemaker must admit cartridge-paced native completion");
        }
        {
            Reset(hand); weapon_class = &peacemaker_class;
            const char* class_name = nullptr;
            Require(ReadCoJMotionReloadWeaponClassName(&holder, &player, hand, class_name) &&
                class_name && !std::strcmp(class_name, "WeaponPistolPeacemaker"),
                "diagnostic class lookup did not identify exact peacemaker class");
            Require(refs == 0 && !pending, "diagnostic class lookup leaked JNI state");
        }
        Reset(hand); weapon_class = &cogswell_class;
        {
            CoJMotionReloadOwner out{};
            CoJMotionReloadDiagnostic diagnostic{};
            Require(!ReadCoJMotionReloadOwner(&holder, &player, out, &diagnostic),
                "known unsupported pistol unexpectedly accepted");
            Require(diagnostic.reason == CoJMotionReloadRejectReason::unsupported_weapon,
                "known unsupported pistol lost strict rejection");
            const char* class_name = nullptr;
            Require(ReadCoJMotionReloadWeaponClassName(&holder, &player, hand, class_name) &&
                class_name && !std::strcmp(class_name, "WeaponPistolCogswell"),
                "diagnostic class lookup did not identify exact unsupported pistol class");
            Require(refs == 0 && !pending, "diagnostic class lookup leaked JNI state");
        }
        for (void* cls : {static_cast<void*>(&schofield_a_class), static_cast<void*>(&schofield_b_class)}) {
            Reset(hand); weapon_class = cls; Check(true, hand);
        }
        for (int id : {0, -1}) { Reset(hand); weapon_id = id; Check(false); }
        Reset(hand); weapon_id = 2147483647; Check(true, hand);
        Reset(hand); replace_on_id = true; Check(false);
        Reset(hand); dead = true; Check(false);
        Reset(hand); reload = true; Check(false);
        Reset(hand); carry[1-hand] = true; Check(false);
        Reset(hand); current[1-hand] = 20;
        {
            CoJMotionReloadOwner out{};
            CoJMotionReloadDiagnostic diagnostic{};
            Require(!ReadCoJMotionReloadOwner(&holder, &player, out, &diagnostic),
                "support-state diagnostic sample unexpectedly accepted");
            // Shipped HasSomethingInHand reports current state20 as occupied;
            // unchanged initial admission rejects that before support_state.
            Require(diagnostic.reason == CoJMotionReloadRejectReason::support_occupied &&
                diagnostic.armed_hand == hand && diagnostic.states[1-hand][0] == 20 &&
                diagnostic.occupied[1-hand] && !out.valid,
                "current support-state occupation diagnostic missing observed values");
        }
        for(int state_index=1;state_index<4;++state_index) {
            Reset(hand);
            auto& support_states = state_index==1 ? destiny :
                state_index==2 ? wanted : actual_state;
            support_states[1-hand]=20;
            CoJMotionReloadOwner out{};
            CoJMotionReloadDiagnostic diagnostic{};
            Require(!ReadCoJMotionReloadOwner(&holder,&player,out,&diagnostic) &&
                !out.valid && diagnostic.reason==CoJMotionReloadRejectReason::support_state &&
                diagnostic.armed_hand==hand && diagnostic.states[1-hand][state_index]==20 &&
                diagnostic.states[1-hand][0]==0 && !diagnostic.occupied[1-hand],
                "noncurrent support-state rejection diagnostic missing observed values");
            Require(refs==0 && !pending,"support-state diagnostic leaked JNI state");
        }
        Reset(hand); current[hand] = 20;
        {
            CoJMotionReloadOwner out{};
            CoJMotionReloadDiagnostic diagnostic{};
            Require(!ReadCoJMotionReloadOwner(&holder, &player, out, &diagnostic),
                "armed-state diagnostic sample unexpectedly accepted");
            Require(diagnostic.reason == CoJMotionReloadRejectReason::armed_state &&
                diagnostic.armed_hand == hand && diagnostic.states[hand][0] == 20,
                "armed-state rejection diagnostic missing observed values");
        }
        for (int h = 0; h < 2; ++h) {
            for (int kind = 0; kind < 3; ++kind) {
                Reset(hand); two[kind][h] = true; Check(false);
            }
            Reset(hand); attack[h] = true; Check(false);
        }
    }
    for (const char* name : {"WeaponPistolFrontier1878_Regular", "WeaponPistolSchofield_A", "WeaponPistolSchofield_B", "WeaponPistolPeacemaker", "GetActualWeaponNotEmpty", "GetActiveWeapon",
        "GetDesiredWeapon", "GetHandStateMashineState", "GetHandStateMashineDestinyState",
        "GetDesiredWeaponState", "GetActualWeaponState", "HasSomethingInHand",
        "IsNotAlive", "IsWeaponReloading", "GetThisID", "IsActualWeaponOperatedTwoHand",
        "IsDesiredWeaponOperatedTwoHand", "IsHandStateMashineOperatedTwoHand", "GetAttackState"}) {
        for (bool without_exception : {false, true}) {
            Reset(); fail_lookup = name; null_failure = without_exception; Check(false);
        }
    }
    Reset(); pending = true; Check(false);
    CoJMotionReloadOwner out{0, 1, true};
    Require(!ReadCoJMotionReloadOwner(nullptr, &player, out) && !out.valid, "null env accepted");
    Require(!ReadCoJMotionReloadOwner(&holder, nullptr, out) && !out.valid, "null player accepted");
    for (int slot : {6, 15, 17, 23, 24, 31, 33, 36, 39, 51}) {
        Reset(); auto saved = table[slot]; table[slot] = nullptr;
        Check(false); table[slot] = saved;
    }
}
