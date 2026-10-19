// Exercise the production finger gate; JNI supplies passive native ownership.
#include <windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"

namespace {
using namespace cojvr::games::call_of_juarez;
int player, rifle, replacement, player_class, rifle_class, exception;
void* table[174]{}; void** env=table;
void* vm_table[8]{}; void** vm=vm_table;
constexpr const char* names[]{"GetActiveWeapon", "GetHandStateMashineState",
    "HasSomethingInHand", "IsNotAlive", "GetHandStateMashineDestinyState",
    "GetDesiredWeaponState", "GetDesiredWeapon", "IsDesiredWeaponOperatedTwoHand",
    "GetActualWeaponNotEmpty", "GetActualWeaponState", "IsActualWeaponOperatedTwoHand",
    "IsHandStateMashineOperatedTwoHand", "IsCarrying", "IsWeaponReloading"};
struct Hand {
    void* active=nullptr; void* actual=nullptr; void* desired=nullptr;
    int current=0, destination=0, actual_state=0, desired_state=0;
    bool occupied=false, actual_two=false, desired_two=false, machine_two=false;
};
std::array<Hand,2> hands{};
bool pending=false, dead=false, carrying=false, reloading=false, is_rifle=true;
const char* failed_lookup=nullptr;
int failed_call=-1, active_reads=0, change_at_read=0, locals=0;
bool fail_same=false, fail_find=false, fail_instance=false;
void* Id(int i) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(i+1)); }
int Index(void* id) { return static_cast<int>(reinterpret_cast<std::uintptr_t>(id))-1; }
void* Local(void* p) { if(p)++locals; return p; }
std::int32_t __stdcall GetEnv(void*,void** out,std::int32_t) { *out=&env; return 0; }
void* __stdcall Exception(void*) { return pending?Local(&exception):nullptr; }
void __stdcall Clear(void*) { pending=false; }
void __stdcall Delete(void*,void* p) { if(p)--locals; }
void* __stdcall Class(void*,void*) { return Local(&player_class); }
void* __stdcall Find(void*,const char* name) {
    if(fail_find || std::strcmp(name,"WeaponRifle")!=0){pending=true;return nullptr;}
    return Local(&rifle_class);
}
std::uint8_t __stdcall Same(void*,void* a,void* b) {
    if(fail_same){pending=true;return 0;} return a==b;
}
std::uint8_t __stdcall Instance(void*,void*,void*) {
    if(fail_instance){pending=true;return 0;} return is_rifle;
}
void* __stdcall Method(void*,void*,const char* name,const char*) {
    if(failed_lookup && std::strcmp(name,failed_lookup)==0){pending=true;return nullptr;}
    for(int i=0;i<std::size(names);++i)if(std::strcmp(name,names[i])==0)return Id(i);
    pending=true;return nullptr;
}
bool Fault(void* id) { if(Index(id)!=failed_call)return false;pending=true;return true; }
void* __stdcall Object(void*,void*,void* id,const JValue* args) {
    if(Fault(id))return nullptr;
    auto& h=hands[args[0].i];
    if(Index(id)==0 && args[0].i==0 && ++active_reads==change_at_read)h.desired=&replacement;
    return Local(Index(id)==0?h.active:Index(id)==6?h.desired:Index(id)==8?h.actual:nullptr);
}
int __stdcall Integer(void*,void*,void* id,const JValue* args) {
    if(Fault(id))return 0;const auto& h=hands[args[0].i];
    switch(Index(id)){case 1:return h.current;case 4:return h.destination;
        case 5:return h.desired_state;case 9:return h.actual_state;default:return -1;}
}
std::uint8_t __stdcall Boolean(void*,void*,void* id,const JValue* args) {
    if(Fault(id))return 0;
    if(Index(id)==3)return dead;if(Index(id)==12)return carrying;if(Index(id)==13)return reloading;
    const auto& h=hands[args[0].i];
    switch(Index(id)){case 2:return h.occupied;case 7:return h.desired_two;
        case 10:return h.actual_two;case 11:return h.machine_two;default:pending=true;return 0;}
}
bool Check(bool ok,const char* message) { if(!ok)std::cerr<<message<<'\n';return ok; }
void IdleRifle(JavaPlayerBridge& b) {
    hands={}; hands[0]={&rifle,&rifle,&rifle,1,1,1,1,true,true,true,true};
    hands[1].occupied=true; // Native HasWeaponInHand's opposite-hand fallback.
    dead=carrying=reloading=pending=false;is_rifle=true;locals=0;
    failed_lookup=nullptr;failed_call=-1;fail_same=fail_find=fail_instance=false;
    active_reads=change_at_read=0;
    b.weapon_overlays_={};b.weapon_overlay_faulted_=false;
    b.weapon_overlays_[0].weapon=&rifle;b.weapon_overlays_[0].active=true;
    b.weapon_overlays_[0].applied=true;b.weapon_overlays_[0].plan.valid=true;
}
}

int main() {
    table[6]=reinterpret_cast<void*>(Find);table[15]=reinterpret_cast<void*>(Exception);
    table[17]=reinterpret_cast<void*>(Clear);table[23]=reinterpret_cast<void*>(Delete);
    table[24]=reinterpret_cast<void*>(Same);table[31]=reinterpret_cast<void*>(Class);
    table[32]=reinterpret_cast<void*>(Instance);table[33]=reinterpret_cast<void*>(Method);
    table[36]=reinterpret_cast<void*>(Object);table[39]=reinterpret_cast<void*>(Boolean);
    table[51]=reinterpret_cast<void*>(Integer);vm_table[6]=reinterpret_cast<void*>(GetEnv);
    JavaPlayerBridge b;b.vm_=&vm;b.being_=&player;
    bool ok=true;
    const auto rejects=[&](const char* why){return Check(!b.TryCanAnimateEmptyHand(1)&&!pending&&locals==0,why);};
    IdleRifle(b);
    ok &= Check(b.TryCanAnimateEmptyHand(1)&&!pending&&locals==0,
        "idle rifle's logically empty left support hand must use controller curls");
    ok &= Check(!b.TryCanAnimateEmptyHand(0),"armed right fingers must remain native");
    for(int h=0;h<2;++h)for(int field=0;field<4;++field)for(int state:{20,21,22,31,32,97,129}){
        IdleRifle(b);auto& x=hands[h];int* values[]{&x.current,&x.destination,&x.actual_state,&x.desired_state};
        *values[field]=state;ok &= rejects("reload/attack/alternate/pending state admitted support fingers");
    }
    for(int field=0;field<3;++field){
        IdleRifle(b);void** values[]{&hands[1].active,&hands[1].actual,&hands[1].desired};
        *values[field]=&rifle;ok &= rejects("shared active alias or occupied logical support admitted");
        IdleRifle(b);void** owner[]{&hands[0].active,&hands[0].actual,&hands[0].desired};
        *owner[field]=&replacement;ok &= rejects("replacement weapon admitted");
    }
    for(int flag=0;flag<3;++flag){
        IdleRifle(b);bool* flags[]{&hands[0].actual_two,&hands[0].desired_two,&hands[0].machine_two};
        *flags[flag]=false;ok &= rejects("incoherent two-hand ownership admitted");
    }
    IdleRifle(b);carrying=true;ok &= rejects("carried object admitted");
    IdleRifle(b);dead=true;ok &= rejects("dead player admitted");
    IdleRifle(b);reloading=true;ok &= rejects("native reload admitted");
    IdleRifle(b);is_rifle=false;ok &= rejects("pistol reload or special weapon admitted");
    IdleRifle(b);b.weapon_overlays_[0].applied=false;ok &= rejects("unapplied weapon presentation admitted");
    IdleRifle(b);b.weapon_overlays_[0].active=false;ok &= rejects("inactive weapon transaction admitted");
    IdleRifle(b);b.weapon_overlays_[0].plan.valid=false;ok &= rejects("invalid weapon mapping admitted");
    IdleRifle(b);b.weapon_overlay_faulted_=true;ok &= rejects("faulted restore admitted");
    IdleRifle(b);b.weapon_overlays_[1].weapon=&rifle;ok &= rejects("second visual owner admitted");
    IdleRifle(b);change_at_read=2;ok &= rejects("owner replacement during observation admitted");
    for(const auto* name:names){IdleRifle(b);failed_lookup=name;ok &= rejects("JNI lookup fault admitted or leaked");}
    for(int i=0;i<std::size(names);++i){IdleRifle(b);failed_call=i;ok &= rejects("JNI read fault admitted or leaked");}
    IdleRifle(b);fail_same=true;ok &= rejects("JNI identity fault admitted or leaked");
    IdleRifle(b);fail_find=true;ok &= rejects("rifle class lookup fault admitted or leaked");
    IdleRifle(b);fail_instance=true;ok &= rejects("rifle class read fault admitted or leaked");
    IdleRifle(b);b.manual_reload_owned_=true;ok &= rejects("manual recovery failure fell through to rifle idle");
    b.manual_reload_owned_=false;
    b.weapon_overlays_={};b.being_=nullptr;b.vm_=nullptr;
    return ok?0:1;
}
