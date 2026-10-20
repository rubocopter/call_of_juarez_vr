#pragma once
#include "games/call_of_juarez/bow_aim.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Literal world frame in native centimetres, before any overlay. The callback
// must preserve the source axes; this reader never repairs/normalizes them.
struct CoJBowDrawFrame {
    std::array<float,3> position{},right{1,0,0},up{0,1,0},forward{0,0,1};
    bool valid=false;
};
enum class CoJBowDrawNativeGate { unavailable,alt_start,not_alt_move,native_denied,right_released,ready };
inline const char* CoJBowDrawNativeGateName(CoJBowDrawNativeGate gate) noexcept {
    switch(gate){
    case CoJBowDrawNativeGate::alt_start:return "alt_start";
    case CoJBowDrawNativeGate::not_alt_move:return "not_alt_move";
    case CoJBowDrawNativeGate::native_denied:return "native_denied";
    case CoJBowDrawNativeGate::right_released:return "right_released";
    case CoJBowDrawNativeGate::ready:return "ready";
    default:return "unavailable";
    }
}
struct CoJBowDrawNativeObservation {
    bool valid=false;
    std::uint32_t weapon_id=0;
    int current_state=-1,holder_element=-1,arrow_element=-1;
    // held_right reads HumanBeing.GetAttackState(0), the native
    // m_abAttackHandState[0] value, never the bridge's last requested input.
    // release_ready is a conservative observation, not a shot permission token.
    bool can_attack=false,held_right=false,release_ready=false;
    CoJBowDrawFrame root{},holder{},arrow{};
    CoJBowDrawNativeGate gate=CoJBowDrawNativeGate::unavailable;
};
using CoJBowDrawReadFrame=bool(*)(void* context,void* env,void* weapon,int element,
    CoJBowDrawFrame& out) noexcept;
namespace bow_draw_native_detail {
inline bool Rigid(const CoJBowDrawFrame& frame) noexcept {
    if(!frame.valid)return false;
    for(const auto* v:{&frame.position,&frame.right,&frame.up,&frame.forward})
        for(float x:*v)if(!std::isfinite(x))return false;
    const auto dot=[](const auto& a,const auto& b) noexcept {
        return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    constexpr float tolerance=.002F;
    if(std::abs(dot(frame.right,frame.right)-1.F)>=tolerance||
        std::abs(dot(frame.up,frame.up)-1.F)>=tolerance||
        std::abs(dot(frame.forward,frame.forward)-1.F)>=tolerance||
        std::abs(dot(frame.right,frame.up))>=tolerance||
        std::abs(dot(frame.right,frame.forward))>=tolerance||
        std::abs(dot(frame.up,frame.forward))>=tolerance)return false;
    const auto& u=frame.up;const auto& f=frame.forward;const auto& r=frame.right;
    const float determinant=r[0]*(u[1]*f[2]-u[2]*f[1])+
        r[1]*(u[2]*f[0]-u[0]*f[2])+r[2]*(u[0]*f[1]-u[1]*f[0]);
    return std::abs(determinant-1.F)<3.F*tolerance;
}
}
// Caller owns exact-build, game-thread, gameplay/context and pre-overlay
// admission. No JNI reference escapes. DArrow is only the original named frame:
// the caller freezes it relative to root at grab to avoid animated fake tension.
// Observation stays valid through unready/attack states; no timer, animation
// fraction, setters, native attack invocation or shot completion is inferred.
inline bool ReadCoJBowDrawNative(void* env,void* player,CoJBowDrawNativeObservation& out,
    CoJBowDrawReadFrame read_frame,void* context) noexcept {
    using namespace bow_aim_detail;
    out={};
    if(!env||!player||!read_frame||!*static_cast<void***>(env))return false;
    for(int i:{6,15,17,23,24,31,33,36,39,51,167})if(!(*static_cast<void***>(env))[i])return false;
    Reader r{env};if(!r.Check())return false;
    CoJBowAimOwner owner{};
    if(!ReadCoJBowAimOwner(env,player,owner)||owner.hand!=1)return false;
    Reader::Local cls{r,r.Fn<Class>(31)(env,player)};
    if(!r.Check()||!cls.value)return false;
    void* actual=r.Lookup(cls.value,"GetActualWeaponNotEmpty","(I)LWeapon;");
    void* state=r.Lookup(cls.value,"GetHandStateMashineState","(I)I");
    void* permission=r.Lookup(cls.value,"CanAttack","(I)Z");
    void* held=r.Lookup(cls.value,"GetAttackState","(I)Z");if(!r.ok)return false;
    auto bow=r.Get(player,actual,1);if(!r.ok||!bow.value)return false;
    CoJBowAimOwner bound{};
    if(!ReadCoJBowAimOwner(env,player,bound,bow.value)||bound.hand!=1||
        bound.holder_element!=owner.holder_element)return false;
    Reader::Local bow_class{r,r.Fn<Class>(31)(env,bow.value)};
    if(!r.Check()||!bow_class.value)return false;
    void* identity=r.Lookup(bow_class.value,"GetThisID","()I");
    void* element=r.Lookup(bow_class.value,"GetElementID","(Ljava/lang/String;)I");if(!r.ok)return false;
    Reader::Local arrow_name{r,r.Fn<Text>(167)(env,"DArrow")};
    if(!r.Check()||!arrow_name.value)return false;
    Value arrow_arg{.object=arrow_name.value},left{.integer=1},right{.integer=0};
    CoJBowDrawNativeObservation sample{};
    sample.weapon_id=static_cast<std::uint32_t>(r.Integer(bow.value,identity));
    sample.current_state=r.Integer(player,state,&left);
    sample.can_attack=r.Boolean(player,permission,&left);
    sample.held_right=r.Boolean(player,held,&right);
    sample.holder_element=bound.holder_element;
    sample.arrow_element=r.Integer(bow.value,element,&arrow_arg);
    if(!r.ok||sample.weapon_id==0||sample.weapon_id==0xffffffffU||
        sample.current_state<0||sample.current_state>255||sample.arrow_element<1||
        sample.arrow_element>255||sample.arrow_element==sample.holder_element)return false;
    const auto frame=[&](int index,CoJBowDrawFrame& target) noexcept {
        const bool read=read_frame(context,env,bow.value,index,target);
        return r.Check()&&read&&bow_draw_native_detail::Rigid(target);
    };
    if(!frame(0,sample.root)||!frame(sample.holder_element,sample.holder)||
        !frame(sample.arrow_element,sample.arrow))return false;
    CoJBowAimOwner after{};
    if(!ReadCoJBowAimOwner(env,player,after,bow.value)||after.hand!=1||
        after.holder_element!=sample.holder_element)return false;
    const auto id_after=static_cast<std::uint32_t>(r.Integer(bow.value,identity));
    const int state_after=r.Integer(player,state,&left);
    const bool can_after=r.Boolean(player,permission,&left);
    const bool held_after=r.Boolean(player,held,&right);
    const int arrow_after=r.Integer(bow.value,element,&arrow_arg);
    if(!r.ok||id_after!=sample.weapon_id||state_after!=sample.current_state||
        can_after!=sample.can_attack||held_after!=sample.held_right||arrow_after!=sample.arrow_element)return false;
    // Exact WeaponOperate.def: ALT_START=131, ALT_MOVE=129. The alt bit
    // alone also admits reload/wall/attack transitions and is insufficient.
    sample.gate=sample.current_state==131?CoJBowDrawNativeGate::alt_start:
        sample.current_state!=129?CoJBowDrawNativeGate::not_alt_move:
        !sample.can_attack?CoJBowDrawNativeGate::native_denied:
        !sample.held_right?CoJBowDrawNativeGate::right_released:CoJBowDrawNativeGate::ready;
    sample.release_ready=sample.gate==CoJBowDrawNativeGate::ready;
    sample.valid=true;out=sample;return true;
}
}
