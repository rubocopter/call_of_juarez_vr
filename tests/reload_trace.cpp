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
int modern_frontier_class,walker_class,cogswell_class;
int winchester_class, scoped_class, volcanic_class;
int remington_class,sawoff_class,derringer_class,lemant_class,schofield_class,schofield_a_class,schofield_b_class;
std::array<int,2> fixture_loaded{3,0};
std::array<int,3> fixture_reserves{12,27,9};
void* exact_class=nullptr;
bool cogswell=false;
void* table[174]{}, **holder = table;
int refs = 0, steps = 0, fail_at = -1;
bool pending = false, without_exception = false, replace = false, bad_float = false;
bool owner_matches = true, coherent = true, known = true;
int armed = 0;
std::int32_t fixture_identity=123;
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
        (known ? (exact_class?exact_class:cogswell?&cogswell_class:&peacemaker_class) : &gun_class) : &machine_class);
}
void* JCALL Find(void*, const char* name) {
    if (Fail()) return nullptr;
    if(!std::strcmp(name,"WeaponPistolFrontier"))return Ref(&modern_frontier_class);
    if(!std::strcmp(name,"WeaponPistolWalker"))return Ref(&walker_class);
    if(!std::strcmp(name,"WeaponPistolCogswell"))return Ref(&cogswell_class);
    if(!std::strcmp(name,"WeaponRifleWinchester"))return Ref(&winchester_class);
    if(!std::strcmp(name,"WeaponRifleWinchesterScope"))return Ref(&scoped_class);
    if(!std::strcmp(name,"WeaponPistolVolcanic"))return Ref(&volcanic_class);
    if(!std::strcmp(name,"WeaponRifleRemington"))return Ref(&remington_class);
    if(!std::strcmp(name,"WeaponRifleRemingtonSawOff"))return Ref(&sawoff_class);
    if(!std::strcmp(name,"WeaponPistolDerringer"))return Ref(&derringer_class);
    if(!std::strcmp(name,"WeaponPistolLemant"))return Ref(&lemant_class);
    if(!std::strcmp(name,"WeaponPistolSchofield"))return Ref(&schofield_class);
    if(!std::strcmp(name,"WeaponPistolSchofield_A"))return Ref(&schofield_a_class);
    if(!std::strcmp(name,"WeaponPistolSchofield_B"))return Ref(&schofield_b_class);
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
    if (!std::strcmp(static_cast<char*>(id),"GetThisID")) { if (replace) return 456; return fixture_identity; }
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
    const bool integer = !std::strcmp(name,"m_iAnimID0") || !std::strcmp(name,"nAmmoPistol") || !std::strcmp(name,"nAmmoRifle") ||
        !std::strcmp(name,"cojvrReloadProbeStatus") || !std::strcmp(name,"nAmmoShotgun") ||
        !std::strcmp(name,"m_iAmmo0Count") || !std::strcmp(name,"m_iAmmo1Count");
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
    if(!std::strcmp(name,"m_iAmmo0Count")){if(fail_at==-2)replace=true;return fixture_loaded[0];}
    if(!std::strcmp(name,"m_iAmmo1Count"))return fixture_loaded[1];
    return !std::strcmp(name,"nAmmoRifle") ? fixture_reserves[1] :
        !std::strcmp(name,"nAmmoPistol") ? fixture_reserves[0] :
        !std::strcmp(name,"nAmmoShotgun") ? fixture_reserves[2] : !std::strcmp(name,"cojvrReloadProbeStatus") ? 2 : 77;
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
    exact_class=nullptr;
    fixture_loaded={3,0};fixture_reserves={12,27,9};
    fixture_identity=123;
    steps = 0; fail_at = -1; pending = without_exception = replace = bad_float = false;
    owner_matches = coherent = known = true; armed = 0;
}
int Check(bool expected, bool probe);
void IndexedAndClipReadback() {
    for(void* cls:{static_cast<void*>(&remington_class),static_cast<void*>(&sawoff_class),
            static_cast<void*>(&derringer_class),static_cast<void*>(&lemant_class),
            static_cast<void*>(&schofield_class),static_cast<void*>(&schofield_a_class),
            static_cast<void*>(&schofield_b_class)}) {
        const bool clip=cls==&schofield_class||cls==&schofield_a_class||cls==&schofield_b_class;
        const auto setup=[&](){Reset();exact_class=cls;fixture_loaded=clip?
            std::array<int,2>{2,0}:std::array<int,2>{1,0};};
        setup();fixture_identity=static_cast<std::int32_t>(0x86e8fca8U);
        CoJReloadTraceSnapshot out{};
        Require(ReadCoJReloadTrace(&holder,&player,out,true)&&out.valid&&
            out.weapon_id==0x86e8fca8U,"remaining exact family trace missing/high-bit ID lost");
        const char* expected_class=cls==&remington_class?"WeaponRifleRemington":
            cls==&sawoff_class?"WeaponRifleRemingtonSawOff":cls==&derringer_class?"WeaponPistolDerringer":
            cls==&lemant_class?"WeaponPistolLemant":cls==&schofield_class?"WeaponPistolSchofield":
            cls==&schofield_a_class?"WeaponPistolSchofield_A":"WeaponPistolSchofield_B";
        Require(out.weapon_class&&!std::strcmp(out.weapon_class,expected_class),"trace exported wrong exact-class metadata");
        const int pool=cls==&lemant_class||cls==&derringer_class||clip?0:2;
        Require(out.kind==(clip?CoJManualReloadKind::clip:CoJManualReloadKind::indexed)&&
            out.index_count==(clip?1:2)&&out.selected_index==(clip?0:1)&&
            out.loaded==fixture_loaded&&out.reserves==fixture_reserves&&
            out.capacities==(clip?std::array<int,2>{6,0}:cls==&lemant_class?
                std::array<int,2>{1,8}:std::array<int,2>{1,1})&&
            out.reserve_indices[out.selected_index]==pool&&out.ammo_readback==fixture_loaded[out.selected_index]&&
            out.reserve_readback==fixture_reserves[pool],"typed vector/capacity/second-slot proposal mismatch");
        Require(refs==0&&!pending,"remaining family trace leaked JNI state");
        const int calls=steps;
        for(int op=1;op<=calls;++op){setup();fail_at=op;Check(false,true);}
        setup();
        if(!clip) {
            Require(ReadCoJReloadTrace(&holder,&player,out,true,0)&&out.selected_index==0&&
                out.ammo_readback==1,"captured index override was replaced by eligibility proposal");
        }
        setup();fixture_loaded[0]=clip?7:2;Check(false,true);
        setup();fixture_reserves[2]=-1;Check(false,true);
    }
    Reset();
}
void NativeBodyProfileReadback() {
    for(void* cls:{static_cast<void*>(&winchester_class),static_cast<void*>(&scoped_class),
            static_cast<void*>(&volcanic_class)}) {
        for(int hand:{0,1}) {
            Reset();exact_class=cls;armed=hand;
            CoJReloadTraceSnapshot out{};
            Require(ReadCoJReloadTrace(&holder,&player,out,true)&&out.valid&&out.probe_valid,
                "exact native body-cycle class must expose native reload trace");
            Require(out.weapon_class&&!std::strcmp(out.weapon_class,cls==&winchester_class?"WeaponRifleWinchester":
                cls==&scoped_class?"WeaponRifleWinchesterScope":"WeaponPistolVolcanic"),
                "native body trace lost exact-class metadata");
            const bool rifle=cls!=&volcanic_class;
            Require(out.reserve_kind==(rifle?CoJReloadReserveKind::rifle:CoJReloadReserveKind::pistol)&&
                out.reserve_readback==(rifle?27:12)&&out.ammo_readback==3&&out.probe_status==2&&
                out.native_body_cycle,
                "new exact class read the wrong native reserve or ammo slot");
            Require(refs==0&&!pending,"new profile trace leaked JNI state");
            const int count=steps;
            for(int op=1;op<=count;++op) {
                Reset();exact_class=cls;armed=hand;fail_at=op;Check(false,true);
            }
        }
    }
    Reset();
}
int Check(bool expected, bool probe=false) {
    CoJReloadTraceSnapshot out{}; out.valid = true; out.weapon_id = 999;
    Require(ReadCoJReloadTrace(&holder,&player,out,probe) == expected, "trace observation admission mismatch");
    Require(!pending && refs == 0, "trace exception/local reference leak");
    Require(expected ? out.valid && out.armed_hand == armed && out.weapon_id == static_cast<std::uint32_t>(fixture_identity) &&
        out.ammo_readback == 3 && out.reserve_readback == 12 && out.animation_id[0] == 77 &&
        out.states[armed][0] == 21 && out.min_advance[armed] == .25F && out.max_advance[armed] == .5F &&
        out.current_time[armed] == 4.F && out.start_time[armed] == 3.F && out.drum_phase == 60.F &&
        !out.native_body_cycle :
        !out.valid && out.weapon_id == 0 && out.armed_hand == -1, "partial or wrong trace published");
    if (expected) Require(out.probe_valid==probe && (!probe || out.probe_status==2),"probe marker mismatch");
    return steps;
}
void EventTests() {

    cogswell=true;CoJReloadTraceSnapshot rifle{};
    Require(ReadCoJReloadTrace(&holder,&player,rifle,true)&&rifle.reserve_readback==27&&
        rifle.reserve_kind==CoJReloadReserveKind::rifle,"Cogswell must observe rifle reserve, not the unrelated pistol pool");
    Require(refs==0&&!pending,"rifle reserve observer leaked JNI state");cogswell=false;
    CoJReloadTraceSnapshot before{},after{};
    before.valid=before.probe_valid=true;before.armed_hand=0;before.weapon_id=123;
    before.probe_status=2;before.ammo_readback=2;before.reserve_readback=10;
    after=before;after.ammo_readback=3;after.reserve_readback=9;
    after.reserve_kind=CoJReloadReserveKind::rifle;
    Require(!CoJUnitManualInsertionObserved(true,true,before,after),"cross-reserve readbacks manufactured unit feedback");
    before.reserve_kind=after.reserve_kind=CoJReloadReserveKind::rifle;
    Require(CoJUnitManualInsertionObserved(true,true,before,after),"matching rifle unit transaction lost feedback");
    before.reserve_kind=after.reserve_kind=CoJReloadReserveKind::pistol;
    after.probe_status=1;
    Require(!CoJUnitManualInsertionObserved(true,true,before,after),
        "revolver insertion must not accept the native body-cycle status");
    before.native_body_cycle=after.native_body_cycle=true;
    Require(CoJUnitManualInsertionObserved(true,true,before,after),
        "native unit transfer into authored body replay must qualify feedback");
    for(int fault=0;fault<7;++fault) {
        auto b=before,a=after;
        if(fault==0)b.native_body_cycle=false;
        if(fault==1)a.native_body_cycle=false;
        if(fault==2)b.probe_status=1;
        if(fault==3)a.ammo_readback=4;
        if(fault==4)a.reserve_readback=10;
        if(fault==5)a.weapon_id=456;
        if(fault==6)a.reserve_kind=CoJReloadReserveKind::rifle;
        Require(!CoJUnitManualInsertionObserved(true,true,b,a),
            "unqualified body replay manufactured insertion feedback");
    }
    before.native_body_cycle=after.native_body_cycle=false;after.probe_status=2;

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
        if(fault==7)a.reserve_readback=10;
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
    s.allowed=true;s.reason="none";++s.sequence;(void)events.Update(true,s);
    ++s.sequence;s.native.kind=CoJManualReloadKind::indexed;s.native.index_count=2;
    auto mode_event=events.Update(true,s);
    Require(mode_event.emit&&!mode_event.comparable,"reload operation kind changed without a trace boundary");
    ++s.sequence;++s.native.loaded[1];
    Require(events.Update(true,s).emit,"unselected slot delta disappeared from native trace");
    ++s.sequence;--s.native.reserves[2];
    Require(events.Update(true,s).emit,"shared reserve vector delta disappeared from native trace");
}
void ModeFeedback() {
    CoJReloadTraceSnapshot b{},a{};
    b.valid=b.probe_valid=true;b.weapon_id=0x86e8fca8U;b.armed_hand=0;b.probe_status=2;
    b.kind=CoJManualReloadKind::clip;b.index_count=1;b.capacities={6,0};
    b.loaded={1,0};b.reserves={10,27,9};b.ammo_readback=1;b.reserve_readback=10;
    a=b;a.probe_status=3;a.loaded={6,0};a.reserves[0]=5;a.ammo_readback=6;a.reserve_readback=5;
    Require(CoJUnitManualInsertionObserved(true,true,b,a),"clip refill must qualify exact min(capacity-loaded,reserve)");
    for(int fault=0;fault<7;++fault){auto bad=a;
        if(fault==0)bad.loaded[0]=5;
        if(fault==1)bad.probe_status=2;
        if(fault==2)bad.reserves[1]--;
        if(fault==3)bad.capacities[0]=7;
        if(fault==4)bad.kind=CoJManualReloadKind::unit;
        if(fault==5)bad.loaded[1]=1;
        if(fault==6)bad.weapon_id++;
        Require(!CoJUnitManualInsertionObserved(true,true,b,bad),"invalid clip vector manufactured refill feedback");
    }
    b.reserves[0]=b.reserve_readback=2;a=b;a.probe_status=3;
    a.loaded[0]=a.ammo_readback=3;a.reserves[0]=a.reserve_readback=0;
    Require(CoJUnitManualInsertionObserved(true,true,b,a),"reserve-limited clip refill lost feedback");
    b.kind=CoJManualReloadKind::indexed;b.index_count=2;b.capacities={1,1};
    b.reserve_kinds={CoJReloadReserveKind::shotgun,CoJReloadReserveKind::shotgun};
    b.reserve_indices={2,2};b.reserve_kind=CoJReloadReserveKind::shotgun;
    b.loaded={0,0};b.reserves={12,27,1};b.selected_index=1;b.ammo_readback=0;b.reserve_readback=1;
    a=b;a.selected_index=0;a.probe_status=3;a.loaded[1]=1;a.reserves[2]=0;
    // These scalars follow the new proposal; captured-slot proof uses vectors.
    a.ammo_readback=0;a.reserve_readback=0;
    Require(CoJUnitManualInsertionObserved(true,true,b,a,1),"final shared reserve/second-slot insertion lost captured index");
    for(int fault=0;fault<9;++fault){auto bad=a;
        if(fault==0)bad.loaded[0]=1;
        if(fault==1)bad.reserves[0]--;
        if(fault==2)bad.reserves[2]=1;
        if(fault==3)bad.reserve_indices[1]=0;
        if(fault==4)bad.capacities[1]=2;
        if(fault==5)bad.index_count=1;
        if(fault==6)bad.kind=CoJManualReloadKind::unit;
        if(fault==7)bad.probe_status=1;
        if(fault==8)bad.loaded[1]=2;
        Require(!CoJUnitManualInsertionObserved(true,true,b,bad,1),"invalid indexed vector manufactured unit feedback");
    }
    Require(!CoJUnitManualInsertionObserved(true,true,b,a,0)&&
        !CoJUnitManualInsertionObserved(true,true,b,a,2),"wrong/out-of-range captured index qualified feedback");
    b.reserve_kinds={CoJReloadReserveKind::rifle,CoJReloadReserveKind::pistol};
    b.reserve_indices={1,0};b.capacities={1,8};b.loaded={1,3};b.reserves={1,27,9};
    b.reserve_kind=CoJReloadReserveKind::pistol;
    a=b;a.loaded[1]=4;a.reserves[0]=0;a.probe_status=2;
    Require(CoJUnitManualInsertionObserved(true,true,b,a,1),"Lemant pistol index debited/compared wrong pool");
}
// Real exact-profile acquisition supplies the typed vectors and cycle flag;
// qualification must prove one captured slot and its pool, even if the next
// eligibility proposal changes after the synchronous native operation.
void IndexedNativeCycleFeedback() {
    for(void* cls:{static_cast<void*>(&remington_class),static_cast<void*>(&sawoff_class),
            static_cast<void*>(&derringer_class),static_cast<void*>(&lemant_class)})
        for(int hand:{0,1})for(int index:{0,1}) {
        Reset();exact_class=cls;armed=hand;
        fixture_loaded=cls==&lemant_class?std::array<int,2>{0,2}:std::array<int,2>{0,0};
        fixture_reserves={5,7,9};
        CoJReloadTraceSnapshot b{};
        Require(ReadCoJReloadTrace(&holder,&player,b,true,index),"indexed cycle profile read failed");
        const int pool=cls==&lemant_class?(index==0?1:0):cls==&derringer_class?0:2;
        auto a=b;a.probe_status=1;++a.loaded[index];--a.reserves[pool];a.selected_index=1-index;
        Require(CoJUnitManualInsertionObserved(true,true,b,a,index)&&
            CoJUnitManualInsertionObserved(true,true,b,a),
            "exact indexed selected-unit replay failed native-cycle feedback qualification");
        Require(refs==0&&!pending,"indexed cycle acquisition leaked JNI state");
        for(int status:{2,3}) {
            auto wait=a;wait.probe_status=status;
            Require(CoJUnitManualInsertionObserved(true,true,b,wait,index),"cycle wait/close lost exact unit feedback");
            auto legacy=b;legacy.native_body_cycle=false;wait.native_body_cycle=false;
            Require(CoJUnitManualInsertionObserved(true,true,legacy,wait,index),"legacy indexed wait/close lost compatibility");
        }
        for(int fault=0;fault<30;++fault) {
            auto before=b,after=a;
            if(fault==0)before.native_body_cycle=false;
            if(fault==1)after.native_body_cycle=false;
            if(fault==2)before.native_body_cycle=after.native_body_cycle=false;
            if(fault==3)after.capacities[index]++;
            if(fault==4)after.reserve_indices[index]=(pool+1)%3;
            if(fault==5)after.reserve_kinds[index]=static_cast<CoJReloadReserveKind>(99);
            if(fault==6)after.loaded[1-index]++;
            if(fault==7)after.loaded[index]=before.loaded[index];
            if(fault==8)after.reserves[pool]=before.reserves[pool];
            if(fault==9)after.reserves[(pool+1)%3]--;
            if(fault==10)after.index_count=1;
            if(fault==11)after.weapon_id++;
            if(fault==12)after.armed_hand=1-hand;
            if(fault==13)before.probe_status=1;
            if(fault==14)after.probe_status=4;
            if(fault==15)after.valid=false;
            if(fault==16)after.probe_valid=false;
            if(fault==17)before.weapon_id=after.weapon_id=0;
            if(fault==18)before.loaded[index]=-1;
            if(fault==19)after.loaded[index]=after.capacities[index]+1;
            if(fault==20)before.capacities[index]=after.capacities[index]=0;
            if(fault==21)before.reserve_indices[index]=after.reserve_indices[index]=(pool+1)%3;
            if(fault==22)before.reserve_kinds[index]=after.reserve_kinds[index]=static_cast<CoJReloadReserveKind>(99);
            if(fault==23)before.reserves[(pool+1)%3]=after.reserves[(pool+1)%3]=-1;
            if(fault==24)after.kind=CoJManualReloadKind::unit;
            if(fault==25)before.index_count=after.index_count=3;
            if(fault==26)before.valid=false;
            if(fault==27)before.probe_valid=false;
            if(fault==28)after.loaded[index]++; // More than one unit, including Lemant's 8-round slot.
            if(fault==29){after.reserves[pool]++;after.reserves[(pool+1)%3]--;}
            Require(!CoJUnitManualInsertionObserved(true,true,before,after,index),
                "malformed indexed cycle/vector/owner manufactured insertion feedback");
        }
        Require(!CoJUnitManualInsertionObserved(false,true,b,a,index)&&
            !CoJUnitManualInsertionObserved(true,false,b,a,index)&&
            !CoJUnitManualInsertionObserved(true,true,b,a,1-index)&&
            !CoJUnitManualInsertionObserved(true,true,b,a,-2)&&
            !CoJUnitManualInsertionObserved(true,true,b,a,2),"unaccepted or wrong selected operation buzzed");
        for(int status:{2,3})for(bool before_cycle:{false,true}) {
            auto before=b,after=a;after.probe_status=status;
            before.native_body_cycle=before_cycle;after.native_body_cycle=!before_cycle;
            Require(!CoJUnitManualInsertionObserved(true,true,before,after,index),
                "wait/close accepted mismatched indexed cycle metadata");
        }
    }
    for(int status:{1,2,3})for(int flags:{1,2,3}) {
        CoJReloadTraceSnapshot b{};b.valid=b.probe_valid=true;b.weapon_id=123;b.armed_hand=0;
        b.probe_status=2;b.kind=CoJManualReloadKind::clip;b.capacities={6,0};
        b.loaded={2,0};b.reserves={4,7,9};
        auto a=b;a.probe_status=status;a.loaded[0]=6;a.reserves[0]=0;
        b.native_body_cycle=(flags&1)!=0;a.native_body_cycle=(flags&2)!=0;
        Require(!CoJUnitManualInsertionObserved(true,true,b,a),"clip cycle flag/replay manufactured feedback");
    }
    Reset();
}
}
int main() {
    Set(6,Find); Set(15,Exception); Set(17,Clear); Set(23,Delete); Set(24,Same);
    Set(31,Class); Set(33,Method); Set(36,Object); Set(39,Boolean); Set(51,Integer); Set(57,Float);
    Set(94,Field); Set(95,GetObject); Set(100,GetInt); Set(102,GetFloat);
    ModeFeedback();
    IndexedNativeCycleFeedback();
    IndexedAndClipReadback();
    NativeBodyProfileReadback();
    for(int hand=0;hand<2;++hand)for(const std::uint32_t id:{0x80000000U,0x86e8fca8U,0xfffe0000U}){
        Reset();armed=hand;fixture_identity=static_cast<std::int32_t>(id);Check(true,true);
        Reset();armed=hand;fixture_identity=static_cast<std::int32_t>(id);fail_at=-2;Check(false,true);
    }
    for(int invalid:{0,-1}){Reset();fixture_identity=invalid;Check(false,true);}
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
