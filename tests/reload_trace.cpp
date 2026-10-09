#include "games/call_of_juarez/reload_trace.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#if defined(_WIN32)
#define JCALL __stdcall
#else
#define JCALL
#endif
namespace {
using namespace cojvr::games::call_of_juarez;
union Value { std::int64_t alignment; void* object; std::int32_t integer; };
int player, gun, foreign, player_class, gun_class, peacemaker_class, frontier_class;
int machine[2], machine_class, exception;
void* table[174]{}, **holder = table;
int refs = 0, steps = 0, fail_at = -1;
bool pending = false, without_exception = false, replace = false, bad_float = false;
bool owner_matches = true, coherent = true, known = true;
int armed = 0;
void Require(bool value, const char* why) {
    if (!value) { std::cerr << why << '\n'; std::exit(1); }
}
bool Fail() {
    Require(!pending, "JNI call after pending exception");
    if (++steps != fail_at) return false;
    pending = !without_exception; return true;
}
void* Ref(void* p) { if (p) ++refs; return p; }
void* JCALL Exception(void*) { return pending ? Ref(&exception) : nullptr; }
void JCALL Clear(void*) { pending = false; }
void JCALL Delete(void*, void* p) { if (p) --refs; }
void* JCALL Class(void*, void* p) {
    if (Fail()) return nullptr;
    return Ref(p == &player ? &player_class : p == &gun ?
        (known ? &peacemaker_class : &gun_class) : &machine_class);
}
void* JCALL Find(void*, const char* name) {
    if (Fail()) return nullptr;
    Require(!std::strcmp(name,"WeaponPistolPeacemaker") ||
        !std::strcmp(name,"WeaponPistolFrontier1878_Regular"), "unapproved class lookup");
    return Ref(!std::strcmp(name,"WeaponPistolPeacemaker") ? &peacemaker_class : &frontier_class);
}
std::uint8_t JCALL Same(void*, void* a, void* b) { return Fail() ? 0 : a == b; }
void* JCALL Method(void*, void*, const char* name, const char* desc) {
    if (Fail()) return nullptr;
    const bool weapon = !std::strcmp(name,"GetActualWeaponNotEmpty") ||
        !std::strcmp(name,"GetActiveWeapon") || !std::strcmp(name,"GetDesiredWeapon");
    const bool state = !std::strcmp(name,"GetHandStateMashineState") ||
        !std::strcmp(name,"GetHandStateMashineDestinyState") ||
        !std::strcmp(name,"GetDesiredWeaponState") || !std::strcmp(name,"GetActualWeaponState");
    const bool machine_getter = !std::strcmp(name,"GetHandStateMashine");
    const bool advance = !std::strcmp(name,"GetMinAnimAdvance") || !std::strcmp(name,"GetMaxAnimAdvance");
    Require(weapon || state || machine_getter || advance || !std::strcmp(name,"GetThisID") ||
        !std::strcmp(name,"GetAmmoCount") || !std::strcmp(name,"IsWeaponReloading"), "mutating/unknown helper");
    const char* expected = weapon ? "(I)LWeapon;" : state || !std::strcmp(name,"GetAmmoCount") ? "(I)I" :
        machine_getter ? "(I)LPlayerStateMashine;" : advance ? "()F" :
        !std::strcmp(name,"GetThisID") ? "()I" : "()Z";
    Require(!std::strcmp(desc, expected), "wrong method signature");
    return const_cast<char*>(name);
}
void* JCALL Object(void*, void*, void* id, const Value* args) {
    if (Fail()) return nullptr;
    const int h = args[0].integer; Require(h == 0 || h == 1, "bad hand");
    if (!std::strcmp(static_cast<char*>(id),"GetHandStateMashine")) return Ref(&machine[h]);
    return Ref(h != armed ? nullptr : !coherent &&
        !std::strcmp(static_cast<char*>(id),"GetDesiredWeapon") ? &foreign : replace ? &foreign : &gun);
}
int JCALL Integer(void*, void*, void* id, const Value* args) {
    if (Fail()) return 0;
    if (!std::strcmp(static_cast<char*>(id),"GetThisID")) { if (replace) return 456; return 123; }
    if (!std::strcmp(static_cast<char*>(id),"GetAmmoCount")) { if (fail_at == -2) replace = true; return 3; }
    return args[0].integer == armed ? 21 : 0;
}
std::uint8_t JCALL Boolean(void*, void*, void*, const Value*) { return Fail() ? 0 : 1; }
float JCALL Float(void*, void*, void* id, const Value*) {
    if (Fail()) return 0;
    return !std::strcmp(static_cast<char*>(id),"GetMinAnimAdvance") ? .25F : .5F;
}
void* JCALL Field(void*, void*, const char* name, const char* desc) {
    if (Fail()) return nullptr;
    const bool object = !std::strcmp(name,"cOwner");
    const bool integer = !std::strcmp(name,"m_iAnimID0") || !std::strcmp(name,"nAmmoPistol") ||
        !std::strcmp(name,"cojvrReloadProbeStatus");
    const bool real = !std::strcmp(name,"m_fCurrentTime") || !std::strcmp(name,"m_fTime0") ||
        !std::strcmp(name,"m_fPlayTime") || !std::strcmp(name,"m_fDuration") || !std::strcmp(name,"m_fRotate") ||
        !std::strcmp(name,"cojvrReloadProbeUntil");
    Require(object || integer || real, "unapproved field");
    Require(!std::strcmp(desc, object ? "LPawnInventory;" : integer ? "I" : "F"), "wrong field descriptor");
    return const_cast<char*>(name);
}
void* JCALL GetObject(void*, void*, void*) { if (Fail()) return nullptr; return Ref(owner_matches ? &player : &foreign); }
int JCALL GetInt(void*, void*, void* id) {
    if (Fail()) return 0;
    const char* name=static_cast<char*>(id);
    return !std::strcmp(name,"nAmmoPistol") ? 12 : !std::strcmp(name,"cojvrReloadProbeStatus") ? 2 : 77;
}
float JCALL GetFloat(void*, void*, void* id) {
    if (Fail()) return 0;
    if (bad_float) return std::numeric_limits<float>::quiet_NaN();
    const char* name = static_cast<char*>(id);
    return !std::strcmp(name,"m_fCurrentTime") ? 4.F : !std::strcmp(name,"m_fTime0") ? 3.F :
        !std::strcmp(name,"m_fPlayTime") || !std::strcmp(name,"m_fDuration") ? 2.F : 60.F;
}
template<class T> void Set(int i, T p) { table[i] = reinterpret_cast<void*>(p); }
void Reset() {
    steps = 0; fail_at = -1; pending = without_exception = replace = bad_float = false;
    owner_matches = coherent = known = true; armed = 0;
}
int Check(bool expected, bool probe=false) {
    CoJReloadTraceSnapshot out{}; out.valid = true; out.weapon_id = 999;
    Require(ReadCoJReloadTrace(&holder,&player,out,probe) == expected, "trace observation admission mismatch");
    Require(!pending && refs == 0, "trace exception/local reference leak");
    Require(expected ? out.valid && out.armed_hand == armed && out.weapon_id == 123 &&
        out.ammo_readback == 3 && out.pistol_reserve == 12 && out.animation_id[0] == 77 &&
        out.states[armed][0] == 21 && out.min_advance[armed] == .25F && out.max_advance[armed] == .5F &&
        out.current_time[armed] == 4.F && out.start_time[armed] == 3.F && out.drum_phase == 60.F :
        !out.valid && out.weapon_id == 0 && out.armed_hand == -1, "partial or wrong trace published");
    if (expected) Require(out.probe_valid==probe && (!probe || out.probe_status==2),"probe marker mismatch");
    return steps;
}
void EventTests() {
    CoJReloadTraceSnapshot before{},after{};
    before.valid=before.probe_valid=true;before.armed_hand=0;before.weapon_id=123;
    before.probe_status=2;before.ammo_readback=2;before.pistol_reserve=10;
    after=before;after.ammo_readback=3;after.pistol_reserve=9;
    CoJReloadFeedbackOwnerEpoch owners;
    const auto first_owner=owners.Update(1,3,before);
    Require(first_owner!=0&&owners.Update(1,3,after)==first_owner,"ammunition changes do not change reload feedback ownership");
    auto replaced=before;replaced.weapon_id=456;
    const auto next_owner=owners.Update(1,3,replaced);
    Require(next_owner!=0&&next_owner!=first_owner,"native weapon replacement invalidates feedback epoch");
    replaced.armed_hand=1;
    const auto mirrored_owner=owners.Update(1,3,replaced);
    Require(mirrored_owner!=next_owner,"armed hand replacement invalidates feedback epoch");
    const auto player_owner=owners.Update(2,3,replaced);
    Require(player_owner!=mirrored_owner,"player replacement invalidates feedback epoch");
    Require(owners.Update(2,7,replaced)!=player_owner,"native context replacement invalidates feedback epoch");
    Require(owners.Update(0,0,{})==0,"unobserved ownership invalidates feedback");
    Require(owners.Update(1,3,before)!=first_owner,"reacquiring the same owner cannot replay old epoch");
    Require(CoJUnitManualInsertionObserved(true,true,before,after),"native unit transfer qualifies feedback");
    after.probe_status=3;
    Require(CoJUnitManualInsertionObserved(true,true,before,after),"final round may enter closing");
    for(int fault=0;fault<10;++fault){
        auto b=before,a=after;
        if(fault==0)a.valid=false;
        if(fault==1)a.probe_valid=false;
        if(fault==2)a.weapon_id=456;
        if(fault==3)a.armed_hand=1;
        if(fault==4)a.probe_status=4;
        if(fault==5)b.probe_status=1;
        if(fault==6)a.ammo_readback=4;
        if(fault==7)a.pistol_reserve=10;
        if(fault==8)b.ammo_readback=-1;
        if(fault==9)b.weapon_id=0;
        Require(!CoJUnitManualInsertionObserved(true,true,b,a),"uncorrelated or non-unit insertion cannot buzz");
    }
    Require(!CoJUnitManualInsertionObserved(false,true,before,after),"failed native call cannot buzz");
    Require(!CoJUnitManualInsertionObserved(true,false,before,after),"rejected native insertion cannot buzz");
    CoJReloadTraceEvents events; CoJReloadTraceSample s{};
    s.player=1; s.context=3; s.sequence=1; s.allowed=true;
    s.native.valid=true; s.native.weapon_id=123; s.native.armed_hand=0;
    s.native.states[0]={1,1,1,1}; s.native.ammo_readback=3;
    Require(!events.Update(false,s).emit, "disabled trace emitted");
    auto event=events.Update(true,s);
    Require(event.emit && !event.comparable, "trace baseline missing");
    Require(!events.Update(true,s).emit, "duplicate emitted");
    ++s.sequence; s.native.current_time[0]=10.F; s.native.max_advance[0]=.5F;
    Require(!events.Update(true,s).emit, "continuous clock spam");
    ++s.sequence; s.native.probe_valid=true; s.native.probe_status=2;
    Require(events.Update(true,s).emit,"probe wait edge missing");
    ++s.sequence; s.native.probe_until=10.F;
    Require(!events.Update(true,s).emit,"probe clock spam");
    ++s.sequence;s.manual_session=true;s.vr_presentation_requested=true;
    Require(events.Update(true,s).emit,"manual ownership policy transition missing");
    ++s.sequence; s.native.states[0]={20,21,21,21};
    event=events.Update(true,s);
    Require(event.emit && event.comparable && event.before.native.states[0][0]==1, "opening edge missing");
    ++s.sequence; s.native.states[0]={21,21,21,21}; Require(events.Update(true,s).emit,"load edge missing");
    ++s.sequence; s.native.ammo_readback=4;
    event=events.Update(true,s);
    Require(event.emit && event.comparable && event.before.native.ammo_readback==3,"ammo edge missing");
    for (int next:{22,1}) { ++s.sequence; s.native.states[0]={next,next,next,next}; Require(events.Update(true,s).emit,"close/ready edge missing"); }
    ++s.sequence; s.cartridge=true; Require(events.Update(true,s).emit,"pickup missing");
    ++s.sequence; s.in_zone=true; Require(events.Update(true,s).emit,"zone entry missing");
    ++s.sequence; s.request=true; Require(events.Update(true,s).emit,"insertion intent missing");
    Require(!events.Update(true,s).emit,"request replay on duplicate");
    ++s.sequence; Require(!events.Update(true,s).emit,"pending intent spam across fresh frames");
    ++s.sequence; s.request=false; s.in_zone=false; s.cartridge=false; Require(events.Update(true,s).emit,"drop/exit missing");
    ++s.sequence; s.native.weapon_id=456;
    event=events.Update(true,s); Require(event.emit && !event.comparable,"cross-weapon delta allowed");
    ++s.sequence; s.player=2;
    Require(!events.Update(true,s).comparable,"cross-player delta allowed");
    ++s.sequence; s.allowed=false; s.reason="menu";
    Require(events.Update(true,s).emit,"context cancellation missing");
    ++s.sequence; Require(!events.Update(true,s).emit,"inactive context spam");
    ++s.sequence; s.allowed=true; s.native.valid=false; s.reason="observation_unavailable";
    Require(events.Update(true,s).emit,"tracking/native observation loss missing");
    ++s.sequence; s.native.valid=true; s.reason="none";
    Require(!events.Update(true,s).comparable,"recovery fabricated ammo delta");
    s.sequence=1; s.native.ammo_readback=99;
    Require(!events.Update(true,s).emit,"regressing sample emitted");
    Require(!events.Update(false,s).emit,"disable emitted gameplay event");
    s.sequence=100;
    Require(!events.Update(true,s).comparable,"re-enable kept stale delta");
    s.allowed=false; s.reason="post_dispatch_menu";
    Require(events.Update(true,s).emit,"same-frame UI acquisition lost cancellation");
    Require(!events.Update(true,s).emit,"same-frame cancellation spam");
}
}
int main() {
    Set(6,Find); Set(15,Exception); Set(17,Clear); Set(23,Delete); Set(24,Same);
    Set(31,Class); Set(33,Method); Set(36,Object); Set(39,Boolean); Set(51,Integer); Set(57,Float);
    Set(94,Field); Set(95,GetObject); Set(100,GetInt); Set(102,GetFloat);
    Reset(); const int count=Check(true);
    Reset(); const int probe_count=Check(true,true);
    for (int i=1;i<=probe_count;++i) { Reset(); fail_at=i; Check(false,true); }
    Reset(); armed=1; Check(true);
    for (int i=1;i<=count;++i) {
        Reset(); fail_at=i; Check(false);
    }
    for (int slot:{6,15,17,23,24,31,33,36,39,51,57,94,95,100,102}) {
        Reset(); auto p=table[slot]; table[slot]=nullptr; Check(false); table[slot]=p;
    }
    Reset(); known=false; Check(false);
    Reset(); owner_matches=false; Check(false);
    Reset(); coherent=false; Check(false);
    Reset(); fail_at=-2; Check(false);
    Reset(); bad_float=true; Check(false);
    Reset(); pending=true; Check(false);
    CoJReloadTraceSnapshot out{};
    Require(!ReadCoJReloadTrace(nullptr,&player,out) && !out.valid,"null environment accepted");
    EventTests();
}
