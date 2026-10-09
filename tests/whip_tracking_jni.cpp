// Real bridge exercised against an isolated JNI owner/cache fixture.
#include <windows.h>
#include <array>
#include <cmath>
#include <string>
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
enum Id { x=1,y,z,owner,hands,offset_left,offset_up,offset_forward,position,up,forward,
    right_origin,left_origin,right_direction,left_direction,right_fx_up,left_fx_up,right_fx_forward,left_fx_forward };
void* Ptr(int id) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)); }
int Number(void* id) { return static_cast<int>(reinterpret_cast<std::uintptr_t>(id)); }
struct Object { bool whip=false,in_hands=true; std::array<void*,20> fields{}; std::array<float,20> floats{}; };
Object player, weapon, replacement, foreign, firearm;
std::array<Object,12> vectors{};
Object* active=&weapon;
int player_type,whip_type,vector_type,exception;
void* env_table[174]{};void** env_holder=env_table;
void* vm_table[8]{};void** vm_holder=vm_table;
bool pending=false,fail_write=false,fail_read=false,ignore_clear=false,fail_boolean=false,fail_class=false;
bool has_item=false,dead=false;int hand_state=0;const char* fail_method=nullptr;
int destiny_state=0,desired_state=0;Object* desired=nullptr;bool desired_two_hand[2]{};
int fail_call=0;
int constructions=0,globals=0;
std::int32_t __stdcall GetEnv(void*,void** out,std::int32_t) { *out=&env_holder;return 0; }
void* __stdcall Exception(void*) { return pending ? &exception : nullptr; }
void __stdcall Clear(void*) { pending=false; }
void __stdcall Delete(void*,void*) {}
void* __stdcall Global(void*,void* object) { ++globals;return object; }
void __stdcall DeleteGlobal(void*,void*) { --globals; }
std::uint8_t __stdcall Same(void*,void* a,void* b) { return a==b; }
void* __stdcall Class(void*,void* obj) {
    if (fail_class && obj==active) { fail_class=false;pending=true;return nullptr; }
    return obj==&player ? &player_type : &whip_type;
}
void* __stdcall Find(void*,const char* name) { return std::strcmp(name,"WeaponWhip")==0 ? &whip_type : &vector_type; }
std::uint8_t __stdcall Instance(void*,void* obj,void*) { return static_cast<Object*>(obj)->whip; }
void* __stdcall Method(void*,void*,const char* name,const char*) {
    if(fail_method&&std::strcmp(fail_method,name)==0){pending=true;return nullptr;}
    const char* names[]{"GetActiveWeapon","GetHandStateMashineState","HasSomethingInHand","IsNotAlive",
        "GetHandStateMashineDestinyState","GetDesiredWeaponState","GetDesiredWeapon","IsDesiredWeaponOperatedTwoHand"};
    for(int i=0;i<8;++i)if(std::strcmp(name,names[i])==0)return Ptr(50+i);return nullptr;
}
int __stdcall CallInt(void*,void*,void* id,const JValue*){
    if(fail_call==Number(id)){pending=true;return 0;}
    return Number(id)==51?hand_state:Number(id)==54?destiny_state:Number(id)==55?desired_state:-1;
}
std::uint8_t __stdcall CallBool(void*,void*,void* id,const JValue* args){
    if(fail_call==Number(id)){pending=true;return 0;}
    if(fail_boolean){fail_boolean=false;pending=true;return 0;}
    return Number(id)==52?has_item:Number(id)==57?desired_two_hand[args[0].i]:dead;
}
void* __stdcall Call(void*,void*,void* id,const JValue* args) {
    if(fail_call==Number(id)){pending=true;return nullptr;}
    return Number(id)==56?desired:args[0].i==0 ? active : nullptr;
}
void* __stdcall Field(void*,void*,const char* name,const char*) {
    const char* names[]{"","fX","fY","fZ","cOwnerAPB","m_bInHands","m_fPositionLeft","m_fPositionUp","m_fPositionForward",
        "cojvrTrackedPosition","cojvrTrackedUp","cojvrTrackedForward","cojvrRightOrigin","cojvrLeftOrigin",
        "cojvrRightDirection","cojvrLeftDirection","cojvrRightFxUp","cojvrLeftFxUp","cojvrRightFxForward","cojvrLeftFxForward"};
    for (int i=1;i<20;++i) if (std::strcmp(name,names[i])==0) return Ptr(i);
    pending=true;return nullptr;
}
void* __stdcall Get(void*,void* obj,void* id) {
    if (fail_read) { fail_read=false;pending=true;return nullptr; }
    return static_cast<Object*>(obj)->fields[Number(id)];
}
std::uint8_t __stdcall Boolean(void*,void* obj,void*) {
    if (fail_boolean) { fail_boolean=false;pending=true;return 0; }
    return static_cast<Object*>(obj)->in_hands;
}
float __stdcall Float(void*,void* obj,void* id) { return static_cast<Object*>(obj)->floats[Number(id)]; }
void __stdcall Set(void*,void* obj,void* id,void* value) {
    if (ignore_clear && !value) return;
    static_cast<Object*>(obj)->fields[Number(id)]=value;
}
void __stdcall SetFloat(void*,void* obj,void* id,float value) {
    if (fail_write) { fail_write=false;pending=true;return; }
    static_cast<Object*>(obj)->floats[Number(id)]=value;
}
void* __stdcall New(void*,void*,void*,const JValue*) { return &vectors.at(constructions++); }
bool Check(bool value,const char* message) { if (!value) std::cerr<<message<<'\n';return value; }
}
int main() {
    env_table[6]=reinterpret_cast<void*>(Find);env_table[15]=reinterpret_cast<void*>(Exception);
    env_table[17]=reinterpret_cast<void*>(Clear);env_table[21]=reinterpret_cast<void*>(Global);
    env_table[22]=reinterpret_cast<void*>(DeleteGlobal);env_table[23]=reinterpret_cast<void*>(Delete);
    env_table[24]=reinterpret_cast<void*>(Same);env_table[30]=reinterpret_cast<void*>(New);
    env_table[31]=reinterpret_cast<void*>(Class);env_table[32]=reinterpret_cast<void*>(Instance);
    env_table[33]=reinterpret_cast<void*>(Method);env_table[36]=reinterpret_cast<void*>(Call);
    env_table[39]=reinterpret_cast<void*>(CallBool);env_table[51]=reinterpret_cast<void*>(CallInt);
    env_table[94]=reinterpret_cast<void*>(Field);env_table[95]=reinterpret_cast<void*>(Get);
    env_table[96]=reinterpret_cast<void*>(Boolean);env_table[102]=reinterpret_cast<void*>(Float);
    env_table[104]=reinterpret_cast<void*>(Set);env_table[111]=reinterpret_cast<void*>(SetFloat);
    vm_table[6]=reinterpret_cast<void*>(GetEnv);
    JavaPlayerBridge bridge;
    bridge.vm_=&vm_holder;bridge.being_=&player;bridge.vector_class_=&vector_type;bridge.vector_constructor_=Ptr(60);
    bridge.vector_x_field_=Ptr(x);bridge.vector_y_field_=Ptr(y);bridge.vector_z_field_=Ptr(z);
    weapon.whip=replacement.whip=true;weapon.fields[owner]=replacement.fields[owner]=&player;
    weapon.floats[offset_up]=replacement.floats[offset_up]=-2;
    weapon.floats[offset_forward]=replacement.floats[offset_forward]=10;
    ElementWorldBasisTarget socket{{30,40,50},{0,1,0},{0,0,1},true};
    bool ok=true,known=false;
    active=nullptr;
    ok &= Check(bridge.TryCanAnimateEmptyHand(0)&&bridge.TryCanAnimateEmptyHand(1),"fresh empty idle hands rejected");
    has_item=true;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"carry/shared secondary hand allowed finger animation");has_item=false;
    dead=true;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"dead hand allowed finger animation");dead=false;
    hand_state=1;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"non-idle hand allowed finger animation");hand_state=0;
    destiny_state=1;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"pending state-machine transition allowed fingers");destiny_state=0;
    desired_state=1;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"pending desired state allowed fingers");desired_state=0;
    desired=&weapon;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"pending weapon before state change allowed fingers");desired=nullptr;
    for(int side=0;side<2;++side){
        desired_two_hand[1-side]=true;
        ok &= Check(!bridge.TryCanAnimateEmptyHand(side),"other hand pending shared weapon allowed fingers");
        desired_two_hand[1-side]=false;
    }
    for(auto name:{"GetActiveWeapon","GetHandStateMashineState","HasSomethingInHand","IsNotAlive",
        "GetHandStateMashineDestinyState","GetDesiredWeaponState","GetDesiredWeapon","IsDesiredWeaponOperatedTwoHand"}){
        fail_method=name;ok &= Check(!bridge.TryCanAnimateEmptyHand(0)&&!pending,"eligibility lookup failure leaked/allowed fingers");fail_method=nullptr;
    }
    for(int id:{50,51,52,53,54,55,56,57}){
        fail_call=id;ok &= Check(!bridge.TryCanAnimateEmptyHand(0)&&!pending,"eligibility call failure leaked/allowed fingers");fail_call=0;
    }
    fail_boolean=true;ok &= Check(!bridge.TryCanAnimateEmptyHand(0)&&!pending,"eligibility read failure allowed fingers");
    ok &= Check(!bridge.TryCanAnimateEmptyHand(-1)&&!bridge.TryCanAnimateEmptyHand(2),"invalid finger hand accepted");
    bridge.manual_reload_owned_=true;
    ok &= Check(!bridge.TryCanAnimateEmptyHand(1)&&!pending&&globals==0,
        "unavailable native manual recovery must not fall through to ordinary idle finger admission");
    bridge.manual_reload_owned_=false;
    active=&weapon;ok &= Check(!bridge.TryCanAnimateEmptyHand(0),"tool weapon accepted as empty for fingers");
    weapon.fields[forward]=&foreign;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !weapon.fields[position] &&
        weapon.fields[forward]==&foreign && constructions==0 && globals==0,"first publication overwrote foreign cache");
    weapon.fields[forward]=nullptr;
    ok &= Check(bridge.TryGetActiveWhip(0,known) && known,"owned whip unavailable");
    for (int i=0;i<100;++i) {
        ok &= Check(bridge.TryPublishTrackedWhip(0,socket),"pose publication failed");
        auto* pose=static_cast<Object*>(weapon.fields[position]);
        ok &= Check(pose && pose->floats[x]==28 && pose->floats[y]==50 && pose->floats[z]==50,
            "native authored offset axes changed");
        ok &= Check(bridge.ClearTrackedWhip(false) && !weapon.fields[position] && !weapon.fields[forward],"deactivation failed");
    }
    ok &= Check(constructions==3 && globals==4,"frame updates allocated/unbounded pose objects");
    ok &= Check(bridge.TryPublishTrackedWhip(0,socket) &&
        bridge.TryPublishWeaponRay(0,{30,40,50},{0,0,1},true),"verified tool ray incorrectly requires firearm barrel");
    ok &= Check(player.fields[right_origin] && player.fields[right_direction],"tool ray not published");
    ok &= Check(bridge.TryPublishWeaponRay(0,{}, {},false) && !weapon.fields[position] &&
        !player.fields[right_origin],"ray invalidation retained stale tool pose");
    weapon.fields[owner]=&foreign;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !bridge.whip_frame_.valid,"foreign player accepted");
    weapon.fields[owner]=&player;weapon.in_hands=false;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket),"inventory whip accepted");weapon.in_hands=true;
    fail_boolean=true;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !pending,"failed boolean read leaked JNI exception");
    fail_class=true;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !pending,"failed object class read leaked JNI exception");
    active=&firearm;
    ok &= Check(bridge.TryGetActiveWhip(0,known) && !known && !bridge.TryPublishTrackedWhip(0,socket),"firearm treated as whip");
    active=nullptr;
    ok &= Check(bridge.TryGetActiveWhip(0,known) && !known,"empty hand treated as whip");active=&weapon;
    fail_write=true;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !weapon.fields[position] && !pending,
        "JNI write failure retained partial pose or exception");
    socket.valid=false;
    ok &= Check(!bridge.TryPublishTrackedWhip(0,socket) && !bridge.whip_frame_.valid,"invalid socket accepted");socket.valid=true;
    ok &= Check(bridge.TryPublishTrackedWhip(0,socket),"pose recovery failed");
    bool verified=false;
    ok &= Check(bridge.VerifyTrackedWhip(verified) && verified,"published cache not verified");
    weapon.fields[up]=&foreign;
    ok &= Check(!bridge.VerifyTrackedWhip(verified) && verified,"changed pose cache accepted");
    weapon.fields[up]=bridge.whip_vectors_[1];
    active=&replacement;
    ok &= Check(!bridge.VerifyTrackedWhip(verified),"stale active weapon accepted");active=&weapon;
    auto* own_position=weapon.fields[position];weapon.fields[forward]=&foreign;
    player.fields[right_origin]=&foreign;player.fields[right_direction]=&foreign;
    ok &= Check(!bridge.TryPublishWeaponRay(0,{}, {},false) && !player.fields[right_origin] &&
        !player.fields[right_direction] && weapon.fields[position]==own_position && weapon.fields[forward]==&foreign,
        "ambiguous tool clear retained a stale attack ray or overwrote foreign pose");
    bridge.Reset();
    ok &= Check(bridge.being_==&player && bridge.tracked_whip_==&weapon,"reset dropped ambiguous ownership/bindings");
    ok &= Check(!bridge.ClearTrackedWhip(true) && weapon.fields[position]==own_position &&
        weapon.fields[forward]==&foreign && bridge.tracked_whip_==&weapon,"ambiguous clear mutated inventory/dropped owner");
    weapon.fields[forward]=bridge.whip_vectors_[2];ignore_clear=true;
    ok &= Check(!bridge.ClearTrackedWhip(true) && bridge.tracked_whip_==&weapon,"failed readback erased restore identity");
    ignore_clear=false;fail_read=true;
    ok &= Check(!bridge.ClearTrackedWhip(true) && !pending && bridge.tracked_whip_==&weapon,"failed read preserved exception/lost owner");
    ok &= Check(bridge.ClearTrackedWhip(true) && globals==0,"owned cache release failed");
    active=&replacement;
    ok &= Check(bridge.TryPublishTrackedWhip(0,socket) && !weapon.fields[position] && replacement.fields[position],"new weapon reused stale cache");
    ok &= Check(!bridge.TryPublishTrackedWhip(1,socket) && !replacement.fields[position],"unsupported left tool retained pose");
    ok &= Check(bridge.ClearTrackedWhip(true) && bridge.ClearTrackedWhip(true) && globals==0,"restoration not idempotent");
    bridge.being_=nullptr;bridge.vector_class_=nullptr;bridge.vm_=nullptr;
    return ok ? 0 : 1;
}
