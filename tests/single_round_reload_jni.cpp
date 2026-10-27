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
#include "games/call_of_juarez/winchester_loading_geometry.hpp"
#include "games/call_of_juarez/winchester_loading_probe.hpp"
#include "finger_fixture.hpp"
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
int peacemaker_class, derringer_class;
int frontier_class,walker_class,cogswell_class,schofield_class;
int winchester_class,scoped_class,volcanic_class;
int remington_class,sawoff_class,lemant_class;
bool net_game=false,foreign_owner=false;
JavaPlayerBridge* ticket_fault_bridge=nullptr;
bool ticket_generation_fault=false;
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
    if (!std::strcmp(name,"WeaponPistolFrontier"))return Ref(&frontier_class);
    if (!std::strcmp(name,"WeaponPistolWalker"))return Ref(&walker_class);
    if (!std::strcmp(name,"WeaponPistolCogswell"))return Ref(&cogswell_class);
    if (!std::strcmp(name,"WeaponPistolSchofield"))return Ref(&schofield_class);
    if (!std::strcmp(name,"WeaponRifleWinchester"))return Ref(&winchester_class);
    if (!std::strcmp(name,"WeaponRifleWinchesterScope"))return Ref(&scoped_class);
    if (!std::strcmp(name,"WeaponPistolVolcanic"))return Ref(&volcanic_class);
    if (!std::strcmp(name,"WeaponRifleRemington"))return Ref(&remington_class);
    if (!std::strcmp(name,"WeaponRifleRemingtonSawOff"))return Ref(&sawoff_class);
    if (!std::strcmp(name,"WeaponPistolLemant"))return Ref(&lemant_class);

    if (!std::strcmp(name, "WeaponPistolDerringer")) return Ref(&derringer_class);
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
        !std::strcmp(name,"IsCarrying") || !std::strcmp(name,"IsNetGame") ||
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
        !std::strcmp(name,"IsCarrying") || !std::strcmp(name,"IsNetGame") ? "()Z" :
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
        if(ticket_generation_fault&&ticket_fault_bridge){++ticket_fault_bridge->being_generation_;ticket_generation_fault=false;}
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
    if (!std::strcmp(name,"IsCarrying"))return carry[0]||carry[1];
    if (!std::strcmp(name,"IsNetGame"))return net_game;
    return !std::strcmp(name, "HasSomethingInHand") ? carry[args[0].integer] :
        !std::strcmp(name, "GetAttackState") ? attack[args[0].integer] :
        two[!std::strcmp(name, "IsActualWeaponOperatedTwoHand") ? 0 :
            !std::strcmp(name, "IsDesiredWeaponOperatedTwoHand") ? 1 : 2][args[0].integer];
}
template<class T> void Set(int slot, T fn) { table[slot] = reinterpret_cast<void*>(fn); }
void Reset(int hand = 0) {
    ticket_fault_bridge=nullptr;ticket_generation_fault=false;
    net_game=foreign_owner=false;
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
    if(!std::strcmp(name,"cOwner")){
        Require(!std::strcmp(signature,"LPawnInventory;"),"wrong native cOwner type");
        return const_cast<char*>(name);
    }
    Require(!std::strcmp(name, "cojvrManualReloadWeapon") &&
        !std::strcmp(signature, "LWeapon;"), "bridge touched native-owned active ticket");
    if (missing_field) { pending = true; return nullptr; }
    return &ticket_field;
}
void* JNI_CALL TicketOwner(void*,void*,void* field) {
    Require(!std::strcmp(static_cast<char*>(field),"cOwner"),"unapproved owner read");
    if(Fail())return nullptr;return Ref(foreign_owner?&replacement:&player);
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
int ArmCheck(JavaPlayerBridge& bridge, bool expected, int hand = 0, std::uint32_t expected_id = 123,
    CoJManualReloadKind kind=CoJManualReloadKind::unit) {
    const bool result = bridge.TryArmSingleRoundReload({hand, expected_id, true,
        kind==CoJManualReloadKind::unit,kind,kind!=CoJManualReloadKind::unit});
    Require(result == expected, "pending arm result mismatch");
    Require(refs == 0 && !pending, "pending arm leaked local reference or exception");
    Require(expected ? pending_ticket == &weapon : pending_ticket == nullptr,
        "pending field incorrectly published");
    Require(native_active_ticket == &active_ticket_object, "native active ticket modified");
    return step;
}
void RemainingFamilyTickets(JavaPlayerBridge& bridge) {
    for(void* cls:{static_cast<void*>(&remington_class),static_cast<void*>(&sawoff_class),
            static_cast<void*>(&derringer_class),static_cast<void*>(&lemant_class),
            static_cast<void*>(&schofield_class),static_cast<void*>(&schofield_a_class),
            static_cast<void*>(&schofield_b_class)})for(int hand:{0,1}) {
        const bool clip=cls==&schofield_class||cls==&schofield_a_class||cls==&schofield_b_class;
        const auto kind=clip?CoJManualReloadKind::clip:CoJManualReloadKind::indexed;
        const auto setup=[&](){TicketReset(bridge,hand);weapon_class=cls;
            if(cls==&remington_class)for(auto& flags:two)flags[hand]=true;};
        setup();const bool expected=cls!=&remington_class||hand==0;
        const int calls=ArmCheck(bridge,expected,hand,123,kind);
        if(!expected)continue;
        for(int op=1;op<=calls;++op){setup();fail_step=op;ArmCheck(bridge,false,hand,123,kind);}
        setup();ticket_fault_bridge=&bridge;ticket_generation_fault=true;
        Require(!bridge.TryArmSingleRoundReload({hand,123,true,false,kind,true})&&pending_ticket==nullptr,
            "ticket publication credited a changed player generation");
        for(std::uint32_t id:{0x80000000U,0x86e8fca8U}){
            setup();weapon_id=static_cast<std::int32_t>(id);ArmCheck(bridge,true,hand,id,kind);
        }
        for(int fault=0;fault<5;++fault){setup();
            if(fault==0)net_game=true;
            if(fault==1)foreign_owner=true;
            if(fault==2)carry[1-hand]=true;
            if(fault==3)active[hand]=&replacement;
            if(fault==4)actual[1-hand]=active[1-hand]=desired[1-hand]=&weapon;
            ArmCheck(bridge,false,hand,123,kind);
        }
        for(int slot:{95,113,119}){setup();auto saved=table[slot];table[slot]=nullptr;
            ArmCheck(bridge,false,hand,123,kind);table[slot]=saved;}
    }
    TicketReset(bridge);
}
}
namespace {
int fallback_scope_class;
JavaPlayerBridge* fallback_bridge=nullptr;
int fallback_mutation=0;
void* JNI_CALL FallbackFind(void* env,const char* name) {
    if(!std::strcmp(name,"WeaponRifleWinchesterScope"))return Ref(&fallback_scope_class);
    return Find(env,name);
}
void* JNI_CALL FallbackField(void*,void*,const char* name,const char* descriptor) {
    Require(!std::strcmp(name,"cOwner")&&!std::strcmp(descriptor,"LPawnInventory;"),"fallback read an unapproved field");
    return const_cast<char*>(name);
}
void* JNI_CALL FallbackOwner(void*,void*,void*) { return Ref(&player); }
int JNI_CALL FallbackInteger(void* env,void* object,void* method,const Value* args) {
    const int result=Integer(env,object,method,args);
    if(fallback_bridge&&!std::strcmp(static_cast<char*>(method),"GetThisID")) {
        if(fallback_mutation==1)fallback_bridge->being_=&replacement;
        if(fallback_mutation==2)++fallback_bridge->being_generation_;
    }
    return result;
}
void NativeFallbackBridgeTests(JavaPlayerBridge& bridge) {
    const auto reset=[&]() {
        TicketReset(bridge);weapon_class=&fallback_scope_class;
        Set(6,FallbackFind);Set(31,Class);Set(33,Lookup);Set(36,Object);Set(39,Boolean);
        Set(51,FallbackInteger);Set(94,FallbackField);Set(95,FallbackOwner);
        bridge.independent_hand_faulted_=bridge.weapon_overlay_faulted_=false;
        bridge.body_restore_owner_count_=0;bridge.hand_overlays_={};bridge.weapon_overlays_={};
        fallback_bridge=&bridge;fallback_mutation=0;
    };
    reset();Require(bridge.TryObserveNativeReloadFallback(),"bridge lost stable native fallback");
    for(int mutation:{1,2}) {
        reset();fallback_mutation=mutation;
        Require(!bridge.TryObserveNativeReloadFallback(),"fallback credited a replaced player or generation");
    }
    for(int guard=0;guard<7;++guard) {
        reset();
        if(guard==0)bridge.independent_hand_faulted_=true;
        if(guard==1)bridge.weapon_overlay_faulted_=true;
        if(guard==2)bridge.body_restore_owner_count_=1;
        if(guard==3)bridge.hand_overlays_[0].object=&player;
        if(guard==4)bridge.weapon_overlays_[0].weapon=&weapon;
        if(guard==5)bridge.weapon_overlays_[1].active=true;
        if(guard==6)bridge.weapon_overlays_[1].applied=true;
        Require(!bridge.TryObserveNativeReloadFallback()&&environment_calls==0,
            "fallback touched JNI during an unresolved body/hand/weapon transaction");
    }
    reset();Require(refs==0&&!pending&&writes==0,"fallback mutated native reload state or leaked JNI");
    fallback_bridge=nullptr;Set(6,Find);Set(51,Integer);Set(94,TicketField);Set(95,static_cast<void*>(nullptr));
}
bool manual_result=true;
int manual_calls=0, manual_cancels=0;
int manual_index=-1;
JavaPlayerBridge* manual_scoped_owner=nullptr;
JavaPlayerBridge* manual_fault_owner=nullptr;
int manual_callback_fault=0;
void* JNI_CALL ManualLookup(void* env,void* cls,const char* name,const char* signature){
    if(std::string_view(name).starts_with("cojvr")){
        if(Fail()||(fail_lookup&&!std::strcmp(name,fail_lookup))){pending=!null_failure;return nullptr;}
        Require(!std::strcmp(signature,!std::strcmp(name,"cojvrCancelReloadProbe")?"()V":
            !std::strcmp(name,"cojvrInsertManualIndexedRound")?"(I)Z":"()Z"),"wrong manual descriptor");
        if(manual_fault_owner&&manual_callback_fault==4)++manual_fault_owner->being_generation_;
        return const_cast<char*>(name);
    }
    return TicketLookup(env,cls,name,signature);
}
std::uint8_t JNI_CALL ManualBoolean(void* env,void* object,void* method,const Value* args){
    if(!std::string_view(static_cast<char*>(method)).starts_with("cojvr"))return Boolean(env,object,method,args);
    const bool indexed=!std::strcmp(static_cast<char*>(method),"cojvrInsertManualIndexedRound");
    Require(object==&player&&(indexed?args!=nullptr:args==nullptr),"manual helper has wrong owner/arguments");
    if(Fail())return 0;
    ++manual_calls;
    manual_index=indexed?args[0].integer:-1;
    if(manual_fault_owner&&manual_callback_fault==1)manual_fault_owner->being_=&replacement;
    if(manual_fault_owner&&manual_callback_fault==2)++manual_fault_owner->being_generation_;
    if(manual_callback_fault==3)pending=true;
    return manual_result;
}
void JNI_CALL ManualVoid(void*,void* object,void* method,const Value* args){
    Require(object==&player&&!args&&!std::strcmp(static_cast<char*>(method),"cojvrCancelReloadProbe"),"wrong manual cancel call");
    if(!Fail()){
        if(manual_scoped_owner)Require(!manual_scoped_owner->hand_overlays_[0].object&&
            !manual_scoped_owner->weapon_overlays_[0].weapon,"native close preceded scoped restoration");
        ++manual_cancels;
    }
}
void ManualReset(JavaPlayerBridge& bridge){TicketReset(bridge);bridge.manual_reload_owned_=false;
    manual_fault_owner=nullptr;manual_callback_fault=0;
    manual_result=true;manual_index=-1;manual_calls=manual_cancels=0;Set(33,ManualLookup);Set(39,ManualBoolean);Set(63,ManualVoid);}
void ManualReloadJniTests(JavaPlayerBridge& bridge){
    for(int index:{0,1}){
        ManualReset(bridge);bridge.NoteManualReloadRequest();bool inserted=false;
        Require(bridge.TryInsertManualRound(inserted,nullptr,index)&&inserted&&manual_index==index,
            "captured ammo index did not reach exact (I)Z native helper");
        for(int fault=1;fault<=4;++fault){
            ManualReset(bridge);bridge.NoteManualReloadRequest();manual_fault_owner=&bridge;manual_callback_fault=fault;
            Require(!bridge.TryInsertManualRound(inserted,nullptr,index)&&!inserted&&refs==0&&!pending,
                "indexed helper credited replaced owner/exception");
        }
    }
    for(int index:{-2,2}){
        ManualReset(bridge);bridge.NoteManualReloadRequest();bool inserted=true;
        Require(!bridge.TryInsertManualRound(inserted,nullptr,index)&&!inserted&&environment_calls==0,
            "invalid captured index reached JNI");
    }
    ManualReset(bridge);std::string error;
    Require(bridge.CancelManualReload()&&environment_calls==0,"unowned manual cancellation touched VM");
    bool accepted=true;
    Require(!bridge.TryInsertManualRound(accepted)&&!accepted&&environment_calls==0,"unowned insertion touched VM");
    Require(bridge.TryKeepManualReloadAlive()&&bridge.manual_reload_owned_&&refs==0&&!pending,"heartbeat lost ownership or refs");
    const int operations=step;
    Require(bridge.TryInsertManualRound(accepted)&&accepted&&bridge.manual_reload_owned_,"manual native result not propagated");
    manual_result=false;Require(bridge.TryInsertManualRound(accepted)&&!accepted,"native rejection reported as accepted");
    Require(bridge.TryApplyGameplayInput({})&&!bridge.manual_reload_owned_&&manual_cancels==1,"inactive input did not cancel manual owner");
    for(int fault=1;fault<=4;++fault){
        ManualReset(bridge);manual_fault_owner=&bridge;manual_callback_fault=fault;
        Require(!bridge.TryKeepManualReloadAlive()&&!bridge.manual_reload_owned_&&refs==0&&!pending,
            "manual heartbeat credited a replaced owner or an exception after native success");
        if(fault==4)Require(manual_calls==0,"manual callback ran after its lookup changed the owner generation");
        ManualReset(bridge);bridge.NoteManualReloadRequest();manual_fault_owner=&bridge;manual_callback_fault=fault;
        accepted=true;
        Require(!bridge.TryInsertManualRound(accepted)&&!accepted&&refs==0&&!pending,
            "manual insertion credited a replaced owner or retained success after a JNI exception");
    }
    for(int operation=1;operation<=operations;++operation){
        ManualReset(bridge);fail_step=operation;
        Require(!bridge.TryKeepManualReloadAlive()&&!bridge.manual_reload_owned_&&refs==0&&!pending,"manual heartbeat fault leaked/published ownership");
    }
    for(int slot:{15,17,23,31,33,39}){
        ManualReset(bridge);auto saved=table[slot];table[slot]=nullptr;
        Require(!bridge.TryKeepManualReloadAlive()&&!bridge.manual_reload_owned_&&refs==0,"missing JNI slot admitted manual ownership");
        table[slot]=saved;
    }
    for(int slot:{15,17,23,31,33,63}){
        ManualReset(bridge);bridge.NoteManualReloadRequest();auto saved=table[slot];table[slot]=nullptr;
        Require(!bridge.CancelManualReload()&&bridge.manual_reload_owned_&&bridge.being_==&player&&refs==0,
            "missing cancel JNI slot discarded manual owner");
        table[slot]=saved;Require(bridge.CancelManualReload()&&!bridge.manual_reload_owned_,"cancel slot recovery failed");
    }
    for(int operation=1;operation<=operations;++operation){
        ManualReset(bridge);bridge.NoteManualReloadRequest();fail_step=operation;
        Require(!bridge.CancelManualReload()&&bridge.manual_reload_owned_&&refs==0&&!pending,"cancel exception discarded owner or leaked");
        fail_step=0;Require(bridge.CancelManualReload()&&!bridge.manual_reload_owned_,"cancel exception retry failed");
    }
    ManualReset(bridge);bridge.NoteManualReloadRequest();fail_lookup="cojvrCancelReloadProbe";
    bridge.Reset();Require(bridge.being_==&player&&bridge.manual_reload_owned_&&refs==0&&!pending,"failed manual cancellation discarded actor");
    fail_lookup=nullptr;Require(bridge.CancelManualReload()&&!bridge.manual_reload_owned_,"manual cancel retry failed");
    ManualReset(bridge);bridge.NoteManualReloadRequest();bridge.ClearBeing(&ticket_env_holder);
    Require(!bridge.being_&&!bridge.manual_reload_owned_&&manual_cancels==1,"actor release did not cancel manual first");
    ManualReset(bridge);bridge.NoteManualReloadRequest();
    manual_scoped_owner=&bridge;
    bridge.hand_overlays_[0].object=&player;bridge.get_position_vector_method_=nullptr;
    bridge.weapon_overlays_[0].weapon=&weapon;bridge.weapon_overlays_[0].active=true;
    Require(!bridge.CancelManualReload()&&bridge.manual_reload_owned_&&manual_cancels==0&&
        bridge.hand_overlays_[0].object==&player,"native closing ran inside retained hand transaction");
    bridge.hand_overlays_[0].object=nullptr;
    Require(!bridge.CancelManualReload()&&bridge.manual_reload_owned_&&manual_cancels==0&&
        bridge.weapon_overlays_[0].weapon==&weapon,"native closing ran inside retained weapon transaction");
    bridge.weapon_overlays_[0].active=false;
    Require(bridge.CancelManualReload()&&!bridge.manual_reload_owned_&&manual_cancels==1,
        "restoration recovery did not permit closing");
    manual_scoped_owner=nullptr;
    ManualReset(bridge);bridge.being_=nullptr;bridge.vm_=nullptr;Set(33,TicketLookup);Set(39,Boolean);
}
}
namespace {
// Independent Java-side storage: indices are deliberately sparse and not inferred
// from bridge targets. World X/up vectors are literal, with forward reconstructed
// by the production reader. No geometry helper computes our expected result.
int geometry_vector_class, geometry_constructor, geometry_x, geometry_y, geometry_z;
struct GeometryVector { float x, y, z; } geometry_vector{};
std::array<ElementWorldBasisTarget, 13> geometry_frames{};
std::array<cojvr::runtime::Vec3,13> geometry_raw_x{};
bool geometry_raw_x_enabled=false;
std::vector<std::string> geometry_names;
int drum_index = 3, gate_index = 1, unavailable_element = -1, geometry_holder_index=4;
int geometry_hand = 0;
bool replace_during_last_frame = false;
int replacement_reads = 0;
int geometry_phase_field,geometry_phase_reads=0;
float geometry_phase=120;
bool change_phase_during_read=false;
void* JNI_CALL GeometryField(void*,void*,const char* name,const char* signature) {
    Require(!std::strcmp(name,"m_fRotate") && !std::strcmp(signature,"F"),"unexpected mechanical field");
    if(Fail())return nullptr;
    return &geometry_phase_field;
}
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
    if (weapon_class==&winchester_class || weapon_class==&scoped_class) {
        if(!std::strcmp(text,"GunBody"))return drum_index;
        if(!std::strcmp(text,"GunLoader"))return gate_index;
        if(!std::strcmp(text,"Holder"))return geometry_holder_index;
        Require(false,"unexpected rifle element name");return -1;
    }
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
    Require(index >= 0 && static_cast<std::size_t>(index) < geometry_frames.size() && args[1].object == &geometry_vector,
        "geometry frame read index/vector mismatch");
    if (Fail() || index == unavailable_element) return 0;
    const auto& frame = geometry_frames[index];
    const auto value = !std::strcmp(name,"GetElementPos") ? frame.position :
        !std::strcmp(name,"GetElementUpVector") ? frame.up :
        geometry_raw_x_enabled?geometry_raw_x[index]:cojvr::runtime::Vec3{frame.up.y*frame.forward.z-frame.up.z*frame.forward.y,
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
    if(field==&geometry_phase_field) {
        Require(object==&weapon || object==&weapon_alias,"phase read wrong native owner");
        if(Fail())return std::numeric_limits<float>::quiet_NaN();
        return geometry_phase+(change_phase_during_read && geometry_phase_reads++>0?60.F:0.F);
    }
    Require(object == &geometry_vector,"geometry read wrong vector");
    if (Fail()) return std::numeric_limits<float>::quiet_NaN();
    Require(field == &geometry_x || field == &geometry_y || field == &geometry_z,
        "geometry read unexpected field");
    return field == &geometry_x ? geometry_vector.x : field == &geometry_y ? geometry_vector.y : geometry_vector.z;
}
void GeometryReset(JavaPlayerBridge& bridge,int hand,bool peacemaker) {
    TicketReset(bridge,hand);
    geometry_raw_x_enabled=false;
    geometry_mode = true; optional_geometry_steps.clear(); geometry_names.clear();
    weapon_class = peacemaker ? &peacemaker_class : &pistol_class;
    drum_index = 3; gate_index = 1; unavailable_element = -1;
    geometry_hand = hand; replace_during_last_frame = false; replacement_reads = 0;
    geometry_phase=120;geometry_phase_reads=0;change_phase_during_read=false;
    bridge.weapon_overlays_ = {};
    auto& overlay = bridge.weapon_overlays_[hand];
    overlay.weapon = &weapon; overlay.active = overlay.applied = true;
    geometry_frames = {};
    geometry_frames[0] = {{90,190,290},{0,1,0},{0,0,1},true};
    geometry_frames[2] = {{95,195,295},{0,1,0},{0,0,1},true};
    // Rotated drum: local X -> -Z, local +Z -> +X; translation is centimetres.
    geometry_frames[3] = {{100,200,300},{0,1,0},{1,0,0},true};
    geometry_frames[1] = {{110,210,310},{1,0,0},{0,0,1},true};
    overlay.barrel_element = 2;
    overlay.natural.assign(geometry_frames.begin(),geometry_frames.end());
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
    Set(51,GeometryInteger); Set(94,GeometryField); Set(102,GeometryFloat); Set(167,GeometryString);
}
int GeometryCheck(JavaPlayerBridge& bridge,bool expected,int hand,bool peacemaker,
    CoJReloadGeometrySource source=CoJReloadGeometrySource::post_overlay) {
    CoJReloadGeometrySnapshot result{};
    result.valid=true; result.model=CoJReloadModel::frontier;
    result.drum.valid=result.gate.valid=true;
    for(auto& mouth:result.mouths) mouth.valid=true;
    const bool observed=bridge.TryObserveReloadGeometry(hand,result,source);
    if(observed!=expected)
        std::cerr << "geometry mismatch: observed=" << observed << " expected=" << expected
            << " hand=" << hand << " peacemaker=" << peacemaker
            << " source=" << static_cast<int>(source) << " step=" << step
            << " fail_step=" << fail_step << " names=" << geometry_names.size() << '\n';
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
        Require(result.native_drum_phase_degrees==geometry_phase,"geometry lost same-owner native drum phase");
        Require(result.valid && result.model==(peacemaker?CoJReloadModel::peacemaker:CoJReloadModel::frontier),
            "geometry returned wrong exact model");
        Require(result.source==source && result.root.valid && result.barrel.valid &&
            result.gate_root.valid && result.drum_root.valid &&
            result.gate_barrel.valid && result.drum_barrel.valid,
            "geometry missing provenance or stable reference frames");
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
// Full bridge path: recovery + trace + manual finger policy + retained curled
// overlay verification + JNI world-frame reads. No predicate/reader is mocked.
CoJHandFrames pinch_live{};
int pinch_machines[2]{},pinch_machine_class,pinch_status=2,pinch_frame_reads=0,pinch_writes=0;
int pinch_weapon_class_reads=0;
bool pinch_kind_mismatch=false,pinch_frame_failure=false;
bool PinchRead(void*,int element,ElementWorldBasisTarget& out) noexcept {
    if(element<0||element>=20)return false;out=pinch_live[element].frame;return true;
}
bool PinchWrite(void*,int element,const ElementWorldBasisTarget& value) noexcept {
    if(element<0||element>=20)return false;pinch_live[element].frame=value;++pinch_writes;return true;
}
void* JNI_CALL PinchClass(void* env,void* object) {
    if(object==&pinch_machines[0]||object==&pinch_machines[1]){
        if(Fail())return nullptr;return Ref(&pinch_machine_class);
    }
    if(object==&weapon&&++pinch_weapon_class_reads>1&&pinch_kind_mismatch){
        if(Fail())return nullptr;return Ref(weapon_class==&schofield_a_class?&lemant_class:&schofield_a_class);
    }
    return Class(env,object);
}
void* JNI_CALL PinchLookup(void* env,void* cls,const char* name,const char* desc) {
    const bool machine=!std::strcmp(name,"GetHandStateMashine");
    const bool advance=!std::strcmp(name,"GetMinAnimAdvance")||!std::strcmp(name,"GetMaxAnimAdvance");
    if(machine||advance){
        Require(!std::strcmp(desc,machine?"(I)LPlayerStateMashine;":"()F"),"wrong pinch trace descriptor");
        if(Fail())return nullptr;return const_cast<char*>(name);
    }
    return TicketLookup(env,cls,name,desc);
}
void* JNI_CALL PinchObject(void* env,void* object,void* method,const Value* args) {
    if(!std::strcmp(static_cast<char*>(method),"GetHandStateMashine")){
        if(Fail())return nullptr;
        Require(object==&player&&args&&(args[0].integer==0||args[0].integer==1),"wrong pinch machine owner/hand");
        return Ref(&pinch_machines[args[0].integer]);
    }
    return Object(env,object,method,args);
}
void* JNI_CALL PinchField(void*,void*,const char* name,const char* desc) {
    if(Fail())return nullptr;
    const bool owner=!std::strcmp(name,"cOwner");
    const bool integer=!std::strcmp(name,"m_iAnimID0")||!std::strcmp(name,"m_iAmmo0Count")||
        !std::strcmp(name,"m_iAmmo1Count")||!std::strcmp(name,"nAmmoPistol")||
        !std::strcmp(name,"nAmmoRifle")||!std::strcmp(name,"nAmmoShotgun")||!std::strcmp(name,"cojvrReloadProbeStatus");
    const bool real=!std::strcmp(name,"m_fCurrentTime")||!std::strcmp(name,"m_fTime0")||
        !std::strcmp(name,"m_fPlayTime")||!std::strcmp(name,"m_fDuration")||
        !std::strcmp(name,"m_fRotate")||!std::strcmp(name,"cojvrReloadProbeUntil");
    Require((owner||integer||real)&&!std::strcmp(desc,owner?"LPawnInventory;":integer?"I":"F"),
        "unexpected pinch native field/type");
    return const_cast<char*>(name);
}
int JNI_CALL PinchIntField(void*,void* object,void* field) {
    if(Fail())return 0;const char* name=static_cast<char*>(field);
    if(!std::strcmp(name,"cojvrReloadProbeStatus")){Require(object==&player,"wrong pinch probe owner");return pinch_status;}
    if(!std::strcmp(name,"m_iAnimID0")){Require(object==&pinch_machines[0]||object==&pinch_machines[1],"wrong pinch animation owner");return 77;}
    if(!std::strcmp(name,"m_iAmmo0Count")||!std::strcmp(name,"m_iAmmo1Count")){
        Require(object==&weapon,"wrong pinch loaded owner");return 0;
    }
    Require(object==&player,"wrong pinch reserve owner");return 10;
}
float JNI_CALL PinchFloat(void* env,void* object,void* field) {
    if(object==&geometry_vector)return GeometryFloat(env,object,field);
    if(Fail())return 0;const char* name=static_cast<char*>(field);
    return !std::strcmp(name,"m_fCurrentTime")?4.F:!std::strcmp(name,"m_fTime0")?3.F:
        !std::strcmp(name,"m_fRotate")?0.F:!std::strcmp(name,"cojvrReloadProbeUntil")?10.F:2.F;
}
float JNI_CALL PinchAdvance(void*,void*,void*,const Value*) {return Fail()?0.F:.5F;}
std::uint8_t JNI_CALL PinchBoolean(void* env,void* object,void* method,const Value* args) {
    const char* name=static_cast<char*>(method);
    if(!std::strcmp(name,"HasSomethingInHand")){
        if(Fail())return 0;const int h=args[0].integer;
        return carry[0]||carry[1]||current[h]!=0||(current[1-h]!=0&&two[2][1-h]);
    }
    if(std::strcmp(name,"GetElementPos")&&std::strcmp(name,"GetElementLeftVector")&&std::strcmp(name,"GetElementUpVector"))
        return Boolean(env,object,method,args);
    Require(object==&player&&args&&args[0].integer>=0&&args[0].integer<20&&args[1].object==&geometry_vector,
        "pinch JNI frame read wrong player/index/vector");
    if(Fail()||pinch_frame_failure)return 0;
    ++pinch_frame_reads;const auto& frame=pinch_live[args[0].integer].frame;
    const auto v=!std::strcmp(name,"GetElementPos")?frame.position:!std::strcmp(name,"GetElementUpVector")?frame.up:
        cojvr::runtime::Vec3{frame.up.y*frame.forward.z-frame.up.z*frame.forward.y,
            frame.up.z*frame.forward.x-frame.up.x*frame.forward.z,frame.up.x*frame.forward.y-frame.up.y*frame.forward.x};
    geometry_vector={v.x,v.y,v.z};return 1;
}
void ManualPinchBridgeTests(JavaPlayerBridge& bridge) {
    // Schofield_A is mirrored; Lemant and Winchester cover the other two modes.
    for(void* cls:{static_cast<void*>(&schofield_a_class),static_cast<void*>(&lemant_class),
            static_cast<void*>(&winchester_class)})for(int armed:{0,1}) {
        if(cls==&winchester_class&&armed==1)continue;
        const int support=1-armed;
        for(int status:{1,2})for(int fault=-1;fault<10;++fault){
            TicketReset(bridge,armed);weapon_class=cls;
            bridge.hand_overlays_={};bridge.weapon_overlays_={};bridge.independent_hand_faulted_=false;
            bridge.manual_reload_owned_=true;
            current[armed]=destiny[armed]=wanted[armed]=actual_state[armed]=21;reload=true;
            for(auto& flags:two)flags[armed]=true;
            pinch_status=status;pinch_kind_mismatch=pinch_frame_failure=false;
            pinch_weapon_class_reads=pinch_frame_reads=pinch_writes=0;
            const auto& rest=support==0?finger_fixture::R_rest:finger_fixture::L_rest;
            pinch_live=rest;
            cojvr::runtime::FingerTrackingState curls{};curls.available=true;
            curls.quality=cojvr::runtime::FingerTrackingQuality::estimated;curls.curls={.7F,.7F,.88F,.88F,.82F};
            auto& overlay=bridge.hand_overlays_[support];overlay.object=&player;
            Require(overlay.frames.Apply(rest,rest[3].frame,rest[3].frame,{},nullptr,PinchRead,PinchWrite,
                &curls,support)&&overlay.frames.fingers_active(),"manual pinch fixture failed to apply real curled overlay");
            bridge.element_world_read_lookup_attempted_=true;
            bridge.get_element_position_method_=const_cast<char*>("GetElementPos");
            bridge.get_element_left_method_=const_cast<char*>("GetElementLeftVector");
            bridge.get_element_up_method_=const_cast<char*>("GetElementUpVector");
            bridge.vector_class_=&geometry_vector_class;bridge.vector_constructor_=&geometry_constructor;
            bridge.vector_x_field_=&geometry_x;bridge.vector_y_field_=&geometry_y;bridge.vector_z_field_=&geometry_z;
            Set(30,GeometryNew);Set(31,PinchClass);Set(33,PinchLookup);Set(36,PinchObject);Set(39,PinchBoolean);
            Set(51,Integer);Set(57,PinchAdvance);Set(94,PinchField);Set(95,TicketOwner);
            Set(100,PinchIntField);Set(102,PinchFloat);Set(113,Lookup);Set(119,Boolean);
            if(fault==0)bridge.manual_reload_owned_=false;
            if(fault==1)foreign_owner=true;
            if(fault==2)overlay.object=nullptr;
            if(fault==3)Require(overlay.frames.Restore({},nullptr,PinchRead,PinchWrite),"pinch setup restore failed");
            if(fault==4)pinch_live[6].frame.position.x+=1.F;
            if(fault==5)pinch_frame_failure=true;
            if(fault==6)pinch_status=3;
            if(fault==7)pinch_kind_mismatch=true;
            if(fault==8)bridge.being_=nullptr;
            if(fault==9)desired[armed]=&replacement;
            const int setup_writes=pinch_writes;
            ElementWorldBasisTarget frame{{1,2,3},{0,1,0},{0,0,1},true};float gap=-1.F;
            const bool observed=bridge.TryReadReloadPinchFrame(support,frame,&gap);
            if(observed!=(fault==-1))std::cerr<<"pinch bridge mismatch mode="<<
                (cls==&schofield_a_class?"clip":cls==&lemant_class?"indexed":"cycle")<<
                " armed="<<armed<<" status="<<status<<" fault="<<fault<<'\n';
            Require(observed==(fault==-1),"manual-only real bridge pinch admission mismatch");
            Require(refs==0&&!pending&&pinch_writes==setup_writes&&writes==0,
                "pinch observation leaked JNI or wrote native frames/tickets");
            if(observed)Require(frame.valid&&std::isfinite(frame.position.x)&&std::isfinite(frame.position.y)&&
                std::isfinite(frame.position.z)&&std::isfinite(gap)&&gap>=.6F&&gap<=1.8F&&
                overlay.frames.fingers_active()&&pinch_frame_reads>0,"manual pinch omitted verified live finger frames");
            else Require(!frame.valid&&frame.position.x==0&&frame.position.y==0&&frame.position.z==0&&gap==0,
                "rejected manual pinch retained stale anchor/gap");
            // Explicit fixture ownership: restore through our backed frame IO,
            // then detach without invoking unrelated native actor teardown.
            if(overlay.frames.active())Require(overlay.frames.Restore({},nullptr,PinchRead,PinchWrite),"manual pinch cleanup failed");
            overlay={};bridge.manual_reload_owned_=false;bridge.being_=&player;
        }
    }
    geometry_mode=false;TicketReset(bridge);
    Set(30,static_cast<void*>(nullptr));Set(31,Class);Set(33,TicketLookup);Set(36,Object);Set(39,Boolean);
    Set(51,Integer);Set(57,static_cast<void*>(nullptr));Set(94,TicketField);Set(95,TicketOwner);
    Set(100,static_cast<void*>(nullptr));Set(102,static_cast<void*>(nullptr));
    bridge.vector_class_=bridge.vector_constructor_=nullptr;
}
JavaPlayerBridge* rifle_bridge=nullptr;
int rifle_fault=0,rifle_frame_calls=0,rifle_actual_reads=0;
std::vector<int> rifle_nonmatching_comparisons;
void* JNI_CALL RifleUnexpectedField(void*,void*,const char*,const char*) {
    Require(false,"rifle geometry attempted an extra field/phase lookup");return nullptr;
}
int JNI_CALL RifleInteger(void* env,void* object,void* method,const Value* args) {
    Require(!std::strcmp(static_cast<char*>(method),"GetElementID"),"rifle geometry read a gameplay counter");
    return GeometryInteger(env,object,method,args);
}
std::uint8_t JNI_CALL RifleSame(void* env,void* a,void* b) {
    const bool nonmatching=a==weapon_class && b!=weapon_class;
    const auto same=Same(env,a,b);
    if(nonmatching)rifle_nonmatching_comparisons.push_back(step);
    return same;
}
void RifleCallbackFault() {
    if(rifle_fault==1)actual[0]=&replacement;
    if(rifle_fault==2)active[0]=&replacement;
    if(rifle_fault==3)rifle_bridge->being_=&replacement;
    if(rifle_fault==4)++rifle_bridge->being_generation_;
    if(rifle_fault==5)rifle_bridge->weapon_overlays_[0].weapon=&replacement;
    if(rifle_fault==6)rifle_bridge->weapon_overlays_[0].applied=false;
}
std::uint8_t JNI_CALL RifleBoolean(void* env,void* object,void* method,const Value* args) {
    const auto result=GeometryBoolean(env,object,method,args);
    ++rifle_frame_calls;
    if(args[0].integer==4 && !std::strcmp(static_cast<char*>(method),"GetElementUpVector"))
        RifleCallbackFault(); // The last requested frame, before the final owner reads.
    return result;
}
void* JNI_CALL RifleObject(void* env,void* object,void* method,const Value* args) {
    auto* result=Object(env,object,method,args);
    if(!std::strcmp(static_cast<char*>(method),"GetActualWeaponNotEmpty")) {
        ++rifle_actual_reads;
        // Even a getter returning the old object cannot hide a bridge replacement.
        if(rifle_actual_reads==2 && rifle_fault==7)rifle_bridge->being_=&replacement;
        if(rifle_actual_reads==2 && rifle_fault==8)++rifle_bridge->being_generation_;
    }
    return result;
}
void RifleReset(JavaPlayerBridge& bridge,bool scoped,CoJReloadGeometrySource source) {
    GeometryReset(bridge,0,false);geometry_mode=false;
    bridge.weapon_overlay_faulted_=bridge.independent_hand_faulted_=false;
    Set(21,TicketGlobal);
    rifle_bridge=&bridge;rifle_fault=rifle_frame_calls=rifle_actual_reads=0;
    rifle_nonmatching_comparisons.clear();
    weapon_class=scoped?&scoped_class:&winchester_class;
    drum_index=7;gate_index=11;geometry_holder_index=4;
    geometry_frames={};
    geometry_frames[0]={{90,190,290},{0,1,0},{0,0,1},true};
    geometry_frames[9]={{95,195,295},{0,1,0},{0,0,1},true};
    geometry_frames[7]={{100,200,300},{0,1,0},{1,0,0},true};
    geometry_frames[11]={{110,210,310},{1,0,0},{0,0,1},true};
    geometry_frames[4]={{120,220,320},{0,0,1},{0,1,0},true};
    geometry_raw_x_enabled=true;geometry_raw_x={};
    geometry_raw_x[0]=geometry_raw_x[9]={1,0,0};geometry_raw_x[7]={0,0,-1};
    geometry_raw_x[11]={0,-1,0};geometry_raw_x[4]={-1,0,0};
    auto& overlay=bridge.weapon_overlays_[0];overlay.barrel_element=9;
    overlay.applied=source==CoJReloadGeometrySource::post_overlay;
    overlay.natural.assign(geometry_frames.begin(),geometry_frames.end());
    overlay.targets=overlay.natural;
    Set(24,RifleSame);Set(36,RifleObject);Set(39,RifleBoolean);
    // A rifle has no admitted m_fRotate field. Any field lookup fails this test.
    Set(51,RifleInteger);Set(94,RifleUnexpectedField);
}
int RifleCheck(JavaPlayerBridge& bridge,bool expected,bool scoped,
    CoJReloadGeometrySource source,int hand=0) {
    CoJWinchesterLoadingGeometry out{};out.valid=true;out.model=CoJWinchesterLoadingModel::scoped;
    out.root.valid=out.body.valid=out.loader.valid=out.holder.valid=out.barrel.valid=true;
    out.lip.fill({1,2,3});std::string error="stale error";
    const bool observed=bridge.TryObserveWinchesterLoadingGeometry(hand,out,source,&error);
    if(observed!=expected)std::cerr<<"rifle geometry mismatch scoped="<<scoped<<" source="<<
        static_cast<int>(source)<<" step="<<step<<" fail="<<fail_step<<" fault="<<rifle_fault<<
        " error="<<error<<'\n';
    Require(observed==expected,"rifle loading geometry admission mismatch");
    Require(refs==0&&!pending,"rifle geometry leaked locals or exception");
    Require(writes==0&&global_creates==0&&global_deletes==0&&!pending_ticket&&
        native_active_ticket==&active_ticket_object&&geometry_phase_reads==0,
        "rifle observer wrote state/refs or inferred mechanical phase");
    if(!expected) {
        Require(!error.empty()&&error!="stale error","rifle failure omitted a fresh diagnostic");
        Require(!out.valid&&out.model==CoJWinchesterLoadingModel::unknown&&out.source==CoJReloadGeometrySource::unknown&&
            !out.root.valid&&!out.body.valid&&!out.loader.valid&&!out.holder.valid&&!out.barrel.valid,
            "rifle rejection retained stale geometry");
        for(const auto& p:out.lip)Require(p.x==0&&p.y==0&&p.z==0,"rifle rejection retained lip");
    } else {
        Require(error.empty()&&out.valid&&out.source==source&&out.model==
            (scoped?CoJWinchesterLoadingModel::scoped:CoJWinchesterLoadingModel::ordinary),
            "rifle geometry lost provenance/exact class");
        Require(geometry_names==std::vector<std::string>{"GunBody","GunLoader","Holder"},
            "rifle observer did not resolve exact sparse element names");
        Require(out.root.position.x==90&&out.barrel.position.x==95&&out.body.position.x==100&&
            out.loader.position.x==110&&out.holder.position.x==120&&out.body.forward.x==1,
            "rifle geometry lost source or frame axes");
        // Literal GunBody-local polygon transformed independently: local X -> -Z,
        // local Y -> +Y and local Z -> +X, in native centimetres.
        constexpr std::array<cojvr::runtime::Vec3,8> expected_lip{{
            {94.743238F,185.176128F,298.765461F},{94.750906F,179.931233F,298.765461F},
            {96.577801F,179.931233F,298.788464F},{96.552880F,185.153124F,298.786547F},
            {96.207821F,185.155041F,298.765461F},{96.207821F,185.511602F,298.765461F},
            {95.094048F,185.503934F,298.755876F},{95.086380F,185.178045F,298.755876F}}};
        for(std::size_t i=0;i<out.lip.size();++i)Require(
            std::abs(out.lip[i].x-expected_lip[i].x)<.0001F&&
            std::abs(out.lip[i].y-expected_lip[i].y)<.0001F&&
            std::abs(out.lip[i].z-expected_lip[i].z)<.0001F,"rifle lip lost literal measured geometry");
        Require(rifle_frame_calls==15,"rifle observer omitted strict live native axis reads");
    }
    return step;
}
void WinchesterLoadingGeometryTests(JavaPlayerBridge& bridge) {
    for(bool scoped:{false,true})for(auto source:{CoJReloadGeometrySource::pre_native,CoJReloadGeometrySource::post_overlay}) {
        // The cached capture is deliberately still rigid/normalized. Only live
        // native +X/up reveal this scale or shear, including pre-native samples.
        for(auto raw:{cojvr::runtime::Vec3{0,0,-2},cojvr::runtime::Vec3{0,.2F,-1}}) {
            RifleReset(bridge,scoped,source);geometry_raw_x[7]=raw;
            RifleCheck(bridge,false,scoped,source);
        }
        RifleReset(bridge,scoped,source);const int calls=RifleCheck(bridge,true,scoped,source);
        const auto nonmatching=rifle_nonmatching_comparisons;
        for(bool null_only:{false,true})for(int op=1;op<=calls;++op) {
            RifleReset(bridge,scoped,source);null_failure=null_only;fail_step=op;
            // A false IsSameObject for an already nonmatching class is the
            // correct answer; only an exception can distinguish its failure.
            const bool benign=null_only&&std::find(nonmatching.begin(),nonmatching.end(),op)!=nonmatching.end();
            RifleCheck(bridge,benign,scoped,source);
        }
        for(int fault=0;fault<29;++fault) {
            RifleReset(bridge,scoped,source);auto& overlay=bridge.weapon_overlays_[0];
            switch(fault) {
            case 0:overlay.active=false;break;
            case 1:overlay.weapon=nullptr;break;
            case 2:overlay.weapon=&replacement;break;
            case 3:overlay.applied=!overlay.applied;break;
            case 4:actual[0]=nullptr;break;
            case 5:active[0]=nullptr;break;
            case 6:active[0]=&replacement;break;
            case 7:weapon_class=&other_class;break;
            case 8:weapon_class=&volcanic_class;break;
            case 9:drum_index=-1;break;
            case 10:gate_index=13;break;
            case 11:overlay.barrel_element=-1;break;
            case 12:bridge.being_=nullptr;break;
            case 13:bridge.vm_=nullptr;break;
            case 14:overlay.natural.clear();overlay.targets.clear();break;
            case 15:overlay.natural[7].valid=overlay.targets[7].valid=false;break;
            case 16:overlay.natural[11].up=overlay.targets[11].up={0,2,0};break;
            case 17:overlay.natural[4].position.x=overlay.targets[4].position.x=std::numeric_limits<float>::quiet_NaN();break;
            case 18:overlay.natural[0].forward=overlay.targets[0].forward={0,.2F,1};break;
            case 19:overlay.natural[9].forward=overlay.targets[9].forward={0,0,2};break;
            case 20:bridge.weapon_overlay_faulted_=true;break;
            case 21:unavailable_element=4;break;
            case 22:geometry_frames[7].position.x+=.101F;break;
            case 23:geometry_frames[11].up={0,2,0};break;
            case 24:geometry_frames[4].up={.2F,0,1};break;
            case 25:geometry_frames[0].position.z=std::numeric_limits<float>::infinity();break;
            case 26:geometry_frames[9].up={.2F,1,0};break;
            case 27:bridge.independent_hand_faulted_=true;break;
            case 28:geometry_holder_index=-1;break;
            }
            RifleCheck(bridge,false,scoped,source);
        }
        for(int hand:{-1,1,2}) {
            RifleReset(bridge,scoped,source);RifleCheck(bridge,false,scoped,source,hand);
            Require(step==0,"unsupported rifle hand reached JNI");
        }
        for(auto bad:{CoJReloadGeometrySource::unknown,static_cast<CoJReloadGeometrySource>(255)}) {
            RifleReset(bridge,scoped,source);RifleCheck(bridge,false,scoped,bad);
            Require(step==0,"invalid rifle provenance reached JNI");
        }
        RifleReset(bridge,scoped,source);bridge.weapon_overlays_[0].weapon=&weapon_alias;
        active[0]=&weapon_alias;RifleCheck(bridge,true,scoped,source);
        if(source==CoJReloadGeometrySource::pre_native) {
            RifleReset(bridge,scoped,source);
            for(auto& f:bridge.weapon_overlays_[0].targets)f={};
            RifleCheck(bridge,true,scoped,source);
        }
        for(int fault=(source==CoJReloadGeometrySource::pre_native?7:1);fault<=8;++fault) {
            RifleReset(bridge,scoped,source);rifle_fault=fault;RifleCheck(bridge,false,scoped,source);
        }
        for(int slot:{6,15,17,23,24,30,31,33,36,39,51,102,167}) {
            RifleReset(bridge,scoped,source);auto saved=table[slot];table[slot]=nullptr;
            RifleCheck(bridge,false,scoped,source);table[slot]=saved;
        }
    }
    bridge.weapon_overlays_={};bridge.weapon_overlay_faulted_=bridge.independent_hand_faulted_=false;
    bridge.vector_class_=nullptr;bridge.being_=nullptr;rifle_bridge=nullptr;
    Set(24,Same);Set(36,Object);Set(39,Boolean);
}
int sample_fault=0,sample_actual_reads=0,sample_swap_owner_reads=0,sample_replacement_counter_reads=0;
bool sample_swap_active=false;
void SampleReplace() {
    actual[0]=active[0]=desired[0]=&replacement;sample_swap_active=true;
}
void* JNI_CALL SampleLookup(void* env,void* cls,const char* name,const char* desc) {
    return !std::strcmp(name,"GetElementID")?GeometryLookup(env,cls,name,desc):PinchLookup(env,cls,name,desc);
}
int JNI_CALL SampleInteger(void* env,void* object,void* method,const Value* args) {
    if(object==&replacement)++sample_replacement_counter_reads;
    return !std::strcmp(static_cast<char*>(method),"GetElementID")?
        GeometryInteger(env,object,method,args):Integer(env,object,method,args);
}
int JNI_CALL SampleIntField(void* env,void* object,void* field) {
    if(object==&replacement)++sample_replacement_counter_reads;
    return PinchIntField(env,object==&replacement?&weapon:object,field);
}
void* JNI_CALL SampleObject(void* env,void* object,void* method,const Value* args) {
    if(!std::strcmp(static_cast<char*>(method),"GetActualWeaponNotEmpty")&&args[0].integer==0) {
        ++sample_actual_reads;
        // Same native ID, class, owner, states and counts, different Java object.
        // These are first/second *outer* trace acquisition points without pinch.
        if((sample_fault==1&&sample_actual_reads==2)||(sample_fault==2&&sample_actual_reads==6))SampleReplace();
    }
    return PinchObject(env,object,method,args);
}
void* JNI_CALL SampleOwner(void* env,void* object,void* field) {
    auto* owner=TicketOwner(env,object,field);
    if(object==&replacement&&sample_swap_active&&++sample_swap_owner_reads==2) {
        // Restore after the raw reader's internal owner/end checks. A caller
        // comparing only numeric snapshots would accept this temporary swap.
        actual[0]=active[0]=desired[0]=&weapon;sample_swap_active=false;
    }
    return owner;
}
std::uint8_t JNI_CALL SampleBoolean(void* env,void* object,void* method,const Value* args) {
    const bool player_frame=object==&player&&(!std::strcmp(static_cast<char*>(method),"GetElementPos")||
        !std::strcmp(static_cast<char*>(method),"GetElementLeftVector")||!std::strcmp(static_cast<char*>(method),"GetElementUpVector"));
    const auto value=player_frame?PinchBoolean(env,object,method,args):
        (object==&weapon||object==&weapon_alias)?RifleBoolean(env,object,method,args):PinchBoolean(env,object,method,args);
    if(player_frame&&sample_fault==3)SampleReplace();
    if(player_frame&&sample_fault==4)++rifle_bridge->being_generation_;
    if(player_frame&&sample_fault==5)rifle_bridge->being_=&replacement;
    return value;
}
void SampleReset(JavaPlayerBridge& bridge,bool scoped,CoJReloadGeometrySource source,bool physical) {
    RifleReset(bridge,scoped,source);bridge.being_generation_=17;bridge.manual_reload_owned_=true;
    current[0]=destiny[0]=wanted[0]=actual_state[0]=21;reload=true;
    for(auto& flags:two)flags[0]=true;
    pinch_status=2;pinch_kind_mismatch=pinch_frame_failure=false;
    pinch_weapon_class_reads=pinch_frame_reads=pinch_writes=0;bridge.hand_overlays_={};
    sample_fault=sample_actual_reads=sample_swap_owner_reads=sample_replacement_counter_reads=0;sample_swap_active=false;
    if(physical&&source==CoJReloadGeometrySource::post_overlay) {
        const auto& rest=finger_fixture::L_rest;pinch_live=rest;
        cojvr::runtime::FingerTrackingState curls{};curls.available=true;
        curls.quality=cojvr::runtime::FingerTrackingQuality::estimated;curls.curls={.7F,.7F,.88F,.88F,.82F};
        auto& fingers=bridge.hand_overlays_[1];fingers.object=&player;
        Require(fingers.frames.Apply(rest,rest[3].frame,rest[3].frame,{},nullptr,PinchRead,PinchWrite,&curls,1),
            "sample curled-finger setup failed");
    }
    Set(24,Same);Set(31,PinchClass);Set(33,SampleLookup);Set(36,SampleObject);Set(39,SampleBoolean);
    Set(51,SampleInteger);Set(57,PinchAdvance);Set(94,PinchField);Set(95,SampleOwner);
    Set(100,SampleIntField);Set(102,PinchFloat);Set(113,Lookup);Set(119,Boolean);
}
int SampleCheck(JavaPlayerBridge& bridge,bool expected,CoJReloadGeometrySource source,bool physical,
    std::uint64_t frame=42,std::uint64_t context=1) {
    CoJWinchesterLoadingSample out{};out.valid=true;out.tip.valid=true;out.native.valid=true;
    std::string error="old error";const int setup_writes=pinch_writes;
    const bool observed=bridge.TryObserveWinchesterLoadingSample(source,frame,context,physical,out,&error);
    if(observed!=expected)std::cerr<<"sample mismatch source="<<static_cast<int>(source)<<" fault="<<sample_fault<<
        " step="<<step<<" fail="<<fail_step<<" error="<<error<<'\n';
    Require(observed==expected,"aggregate loading sample admission mismatch");
    Require(refs==0&&!pending&&writes==0&&pinch_writes==setup_writes&&global_creates==0&&global_deletes==0,
        "aggregate loading sample mutated state or leaked JNI ownership");
    if(expected)Require(error.empty()&&out.valid&&out.geometry.valid&&out.native.valid&&out.frame_sequence==frame&&
        out.player_generation==17&&out.native_context==context&&out.tip.valid==(physical&&source==CoJReloadGeometrySource::post_overlay),
        "aggregate sample lost provenance or conditional displayed pinch");
    else Require(!out.valid&&!out.geometry.valid&&!out.native.valid&&!out.tip.valid&&!out.tip_observation.valid&&
        out.frame_sequence==0&&out.player_generation==0&&out.native_context==0&&!error.empty()&&error!="old error",
        "aggregate sample rejection retained stale output or error");
    if(bridge.hand_overlays_[1].frames.active())Require(bridge.hand_overlays_[1].frames.Restore({},nullptr,PinchRead,PinchWrite),
        "sample fixture finger restoration failed");
    bridge.hand_overlays_={};bridge.manual_reload_owned_=false;bridge.being_=&player;
    return step;
}
void WinchesterLoadingSampleTests(JavaPlayerBridge& bridge) {
    for(bool scoped:{false,true})for(auto source:{CoJReloadGeometrySource::pre_native,CoJReloadGeometrySource::post_overlay}) {
        for(bool physical:{false,true}) {
            SampleReset(bridge,scoped,source,physical);const int operations=SampleCheck(bridge,true,source,physical);
            Require((pinch_frame_reads>0)==(physical&&source==CoJReloadGeometrySource::post_overlay),
                "sample read pinch without displayed physical cartridge");
            for(int fault=1;fault<=2;++fault) {
                SampleReset(bridge,scoped,source,false);sample_fault=fault;SampleCheck(bridge,false,source,false);
                Require(sample_replacement_counter_reads==0,"raw trace sampled replacement before checking expected jobject");
            }
            for(int op=1;op<=operations;++op) {
                SampleReset(bridge,scoped,source,physical);fail_step=op;SampleCheck(bridge,false,source,physical);
            }
        }
        for(int fault=3;source==CoJReloadGeometrySource::post_overlay&&fault<=5;++fault) {
            SampleReset(bridge,scoped,source,true);sample_fault=fault;SampleCheck(bridge,false,source,true);
            if(fault==3)Require(sample_replacement_counter_reads==0,"post-pinch trace sampled a same-ID replacement");
        }
        for(int fault=0;fault<5;++fault) {
            SampleReset(bridge,scoped,source,false);
            if(fault==0)bridge.weapon_overlays_[0].active=false;
            if(fault==1)bridge.weapon_overlays_[0].weapon=&replacement;
            if(fault==2)bridge.weapon_overlay_faulted_=true;
            if(fault==3)bridge.being_generation_=0;
            if(fault==4)pinch_status=4;
            SampleCheck(bridge,false,source,false);
        }
        SampleReset(bridge,scoped,source,false);SampleCheck(bridge,false,source,false,0,1);
        SampleReset(bridge,scoped,source,false);SampleCheck(bridge,false,source,false,42,0);
    }
    bridge.weapon_overlays_={};bridge.hand_overlays_={};bridge.being_=nullptr;bridge.vector_class_=nullptr;
    bridge.manual_reload_owned_=false;Set(24,Same);Set(31,Class);Set(33,TicketLookup);Set(36,Object);Set(39,Boolean);
    Set(51,Integer);Set(94,TicketField);Set(95,TicketOwner);
}
void ReloadGeometryTests(JavaPlayerBridge& bridge) {
    for(const auto source_up:{cojvr::runtime::Vec3{0,2,0},cojvr::runtime::Vec3{.2F,1,0}}) {
        GeometryReset(bridge,0,true);
        geometry_frames[0].up=source_up;
        JavaPlayerPosition p{},u{},f{};
        Require(bridge.ReadObjectElementFrame(&weapon,0,p,u,f,nullptr),
            "ordinary element reader lost existing normalization contract");
        Require(!bridge.ReadObjectElementFrame(&weapon,0,p,u,f,nullptr,true) &&
            p.x==0 && p.y==0 && p.z==0 && u.x==0 && u.y==0 && u.z==0 &&
            f.x==0 && f.y==0 && f.z==0 && refs==0 && !pending,
            "strict natural source reader normalized a scaled/sheared native frame");
    }
    for(int hand=0;hand<2;++hand) for(bool peacemaker:{false,true}) {
        GeometryReset(bridge,hand,peacemaker);
        const int calls=GeometryCheck(bridge,true,hand,peacemaker);
        const auto optional=optional_geometry_steps;
        for(bool null_only:{false,true}) for(int operation=1;operation<=calls;++operation) {
            GeometryReset(bridge,hand,peacemaker); null_failure=null_only; fail_step=operation;
            const bool benign=std::find(optional.begin(),optional.end(),operation)!=optional.end();
            GeometryCheck(bridge,benign,hand,peacemaker);
        }
        for(int fault=0;fault<29;++fault) {
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
            case 20: drum_index=4; break; // mapped index, invalid native element frame
            case 21: bridge.being_=nullptr; break;
            case 22: bridge.vm_=nullptr; break;
            case 23: overlay.targets[0].valid=false; break;
            case 24: overlay.targets[2].valid=false; break;
            case 25: geometry_frames[0].position.x+=.101F; break;
            case 26: geometry_frames[2].position.x+=.101F; break;
            case 27: geometry_frames[0].up={0,0,1}; break;
            case 28: geometry_frames[2].position.y=std::numeric_limits<float>::quiet_NaN(); break;
            }
            GeometryCheck(bridge,false,hand,peacemaker);
        }
        GeometryReset(bridge,hand,peacemaker);
        auto& natural_only=bridge.weapon_overlays_[hand];
        natural_only.applied=false;
        for(int index:{0,1,2,3}) {
            natural_only.targets[index].position.x+=50;
            geometry_frames[index].position.x+=50;
        }
        GeometryCheck(bridge,true,hand,peacemaker,CoJReloadGeometrySource::pre_native);
        GeometryReset(bridge,hand,peacemaker);
        GeometryCheck(bridge,false,hand,peacemaker,CoJReloadGeometrySource::pre_native);
        Require(step==0,"pre-native geometry sampled an applied overlay");
        GeometryReset(bridge,hand,peacemaker);
        bridge.weapon_overlays_[hand].applied=false;
        GeometryCheck(bridge,false,hand,peacemaker,CoJReloadGeometrySource::post_overlay);
        Require(step==0,"post-overlay geometry sampled an uncommitted overlay");
        GeometryReset(bridge,hand,peacemaker);
        GeometryCheck(bridge,false,hand,peacemaker,CoJReloadGeometrySource::unknown);
        Require(step==0,"unknown geometry provenance reached JNI");
        GeometryReset(bridge,hand,peacemaker);
        replace_during_last_frame=true;
        GeometryCheck(bridge,false,hand,peacemaker);
        Require(replacement_reads==1 && geometry_names.size()==2,
            "replacement regression did not reach final named frame read");
        GeometryReset(bridge,hand,peacemaker);change_phase_during_read=true;
        GeometryCheck(bridge,false,hand,peacemaker);
        GeometryReset(bridge,hand,peacemaker);geometry_phase=std::numeric_limits<float>::quiet_NaN();
        GeometryCheck(bridge,false,hand,peacemaker);
        GeometryReset(bridge,hand,peacemaker);
        bridge.weapon_overlays_[hand].weapon=&weapon_alias;
        GeometryCheck(bridge,true,hand,peacemaker);
        for(int slot:{6,15,17,23,24,30,31,33,36,39,51,94,102,167}) {
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
    Set(95,TicketOwner);Set(113,Lookup);Set(119,Boolean);
    vm_table[6] = reinterpret_cast<void*>(TicketGetEnv);
    JavaPlayerBridge bridge;
    RemainingFamilyTickets(bridge);
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
        for(const std::uint32_t id:{0x80000000U,0x86e8fca8U,0xfffe0000U}){
            TicketReset(bridge,hand);weapon_id=static_cast<std::int32_t>(id);ArmCheck(bridge,true,hand,id);
            TicketReset(bridge,hand);weapon_id=static_cast<std::int32_t>(id);ArmCheck(bridge,false,hand,id+1);
        }
        for (int operation = 1; operation <= calls; ++operation) {
            TicketReset(bridge, hand); fail_step = operation; ArmCheck(bridge, false, hand);
        }
        TicketReset(bridge, hand); weapon_class = &peacemaker_class; ArmCheck(bridge, true, hand);
        for(void* cls:{static_cast<void*>(&frontier_class),static_cast<void*>(&walker_class),static_cast<void*>(&cogswell_class)}){
            TicketReset(bridge,hand);weapon_class=cls;ArmCheck(bridge,true,hand);
            active[hand]=&replacement;ArmCheck(bridge,false,hand);
        }

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
    ManualReloadJniTests(bridge);
    NativeFallbackBridgeTests(bridge);
    ReloadGeometryTests(bridge);
    WinchesterLoadingGeometryTests(bridge);
    WinchesterLoadingSampleTests(bridge);
    ManualPinchBridgeTests(bridge);
}
