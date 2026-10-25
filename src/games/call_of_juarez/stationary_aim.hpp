#pragma once
#include "games/call_of_juarez/stationary_weapon.hpp"
#include "runtime/gameplay_utility.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJStationaryAimRay {
    bool valid=false;
    std::uint32_t cannon=0;
    int element=-1;
    runtime::Vec3 origin{},direction{};
};
inline bool ReadCoJStationaryAim(void* env,void* player,CoJLongGunReadFrame read_frame,
    void* context,CoJStationaryAimRay& out) noexcept {
    using namespace long_gun_observer_detail;
    out={};CoJStationaryWeaponOwner owner;
    if(!read_frame||!ReadCoJStationaryWeaponOwner(env,player,owner)||!owner.active)return false;
    for(int slot:{94,95,100})if(!(*static_cast<void***>(env))[slot])return false;
    Reader r{env};auto player_class=r.Class(player);
    void* get_cannon=r.Method(player_class,"GetCannon","()LStationaryCannon;");
    const auto get=[&](void* object,void* method) noexcept -> Reader::Local {
        void* value=r.ok?r.Fn<Object>(36)(env,object,method,nullptr):nullptr;
        r.Check();return {r,value};
    };
    auto cannon=get(player,get_cannon);if(!r.ok||!cannon.value)return false;
    auto cannon_class=r.Class(cannon);
    void* get_controller=r.Method(cannon_class,"GetCannon","()LICannonController;");
    auto controller=get(cannon,get_controller);if(!r.ok||!controller.value)return false;
    auto controller_class=r.Class(controller);
    void* mesh_field=r.Field(controller_class,"m_OwnerMesh","LICannonMesh;");
    void* barrel_field=r.Field(controller_class,"m_nElementBarrelExit","I");
    auto mesh=r.ObjectField(controller,mesh_field);const int element=r.IntField(controller,barrel_field);
    if(!r.ok||!mesh.value||element<0)return false;
    ElementWorldBasisTarget frame{};
    if(!read_frame(context,mesh,element,frame)||!frame.valid||
        !IsCoJLongGunSourceFrameRigid(frame.position,frame.forward,frame.up))return false;
    auto cannon_again=get(player,get_cannon);auto controller_again=get(cannon,get_controller);
    auto mesh_again=r.ObjectField(controller,mesh_field);
    if(!r.ok||!r.Equal(cannon,cannon_again)||!r.Equal(controller,controller_again)||!r.Equal(mesh,mesh_again)||
        r.IntField(controller,barrel_field)!=element||!r.ok)return false;
    CoJStationaryWeaponOwner after;
    if(!ReadCoJStationaryWeaponOwner(env,player,after)||!after.active||after.cannon!=owner.cannon)return false;
    out={true,owner.cannon,element,frame.position,frame.forward};return true;
}
class CoJStationaryAimControl final {
public:
    runtime::Vec2 Update(bool known,CoJStationaryWeaponOwner owner,std::uint64_t player,
        std::uint64_t sequence,std::uint64_t now,const runtime::GameplayInputState& input) noexcept {
        if(!known||!owner.active||!owner.cannon||!player||!sequence||!now||!input.active||
            !input.input_context_generation||!input.turn_available||input.weapon_radial||
            !std::isfinite(input.turn.x)||!std::isfinite(input.turn.y)||
            std::fabs(input.turn.x)>1||std::fabs(input.turn.y)>1){Invalidate();return {};}
        if(player!=player_||owner.cannon!=cannon_||input.input_context_generation!=generation_)Invalidate();
        if((last_time_&&now<last_time_)||(sequence_&&sequence<sequence_)){Invalidate();return {};}
        if(sequence==sequence_)return {};
        player_=player;cannon_=owner.cannon;generation_=input.input_context_generation;
        sequence_=sequence;last_time_=now;
        auto vertical=input;vertical.turn={input.turn.y,input.turn.x};
        // Same proven continuous shaping/time/neutral policy as ordinary yaw.
        // Native mouse X is negative-right; positive native vertical is up.
        return {-horizontal_.Update(input,now),vertical_.Update(vertical,now)};
    }
    void Invalidate() noexcept {horizontal_={};vertical_={};player_=generation_=sequence_=last_time_=0;cannon_=0;}
private:
    runtime::SmoothTurnState horizontal_,vertical_;
    std::uint64_t player_=0,generation_=0,sequence_=0,last_time_=0;
    std::uint32_t cannon_=0;
};
namespace stationary_aim_detail {
union AxisValue {std::int64_t alignment;void* object;std::int32_t integer;float real;};
static_assert(sizeof(AxisValue)==8);
#if defined(_WIN32)
using StaticFloat=float(__stdcall*)(void*,void*,void*,const long_gun_observer_detail::Value*);
using CallVoid=void(__stdcall*)(void*,void*,void*,const AxisValue*);
#else
using StaticFloat=float(*)(void*,void*,void*,const long_gun_observer_detail::Value*);
using CallVoid=void(*)(void*,void*,void*,const AxisValue*);
#endif
}
inline bool DispatchCoJStationaryAim(void* env,void* player,void* controller,void* settings,
    CoJStationaryWeaponOwner expected,runtime::Vec2 degrees,bool& dispatched) noexcept {
    using namespace long_gun_observer_detail;
    using namespace stationary_aim_detail;
    dispatched=false;
    if(!env||!player||!controller||!settings||!expected.active||!expected.cannon||
        !std::isfinite(degrees.x)||!std::isfinite(degrees.y)||!*static_cast<void***>(env))return false;
    for(int slot:{63,113,137})if(!(*static_cast<void***>(env))[slot])return false;
    CoJStationaryWeaponOwner owner;
    if(!ReadCoJStationaryWeaponOwner(env,player,owner)||!owner.active||owner.cannon!=expected.cannon)return false;
    Reader r{env};auto controller_class=r.Class(controller);
    void* can_process=r.Method(controller_class,"CanProcessInputs","()Z");
    void* mouse=r.Method(controller_class,"OnInputMouseAxis","(IF)V");
    if(!r.ok)return false;
    if(!r.Bool(controller,can_process))return r.ok;
    void* sensitivity_method=r.Fn<Lookup>(113)(env,settings,"GetMouseSensitivity","()F");
    if(!r.Check()||!sensitivity_method)return false;
    const float sensitivity=r.Fn<StaticFloat>(137)(env,settings,sensitivity_method,nullptr);
    if(!r.Check()||!std::isfinite(sensitivity)||sensitivity<=0)return false;
    const float values[]{degrees.x/sensitivity,degrees.y/sensitivity};
    if(!std::isfinite(values[0])||!std::isfinite(values[1]))return false;
    // The native mouse path keeps CanProcessInputs, target eligibility,
    // Controller.CanApplyState, native inversion and Creature/Cannon limits.
    for(int axis=0;axis<2;++axis){
        if(!values[axis])continue;
        if(!ReadCoJStationaryWeaponOwner(env,player,owner)||!owner.active||owner.cannon!=expected.cannon)return false;
        const AxisValue args[]{{.integer=axis},{.real=values[axis]}};
        r.Fn<CallVoid>(63)(env,controller,mouse,args);
        if(!r.Check())return false;
    }
    if(!ReadCoJStationaryWeaponOwner(env,player,owner)||!owner.active||owner.cannon!=expected.cannon)return false;
    dispatched=values[0]!=0||values[1]!=0;return true;
}
class CoJStationaryAimFrame final {
    CoJStationaryAimControl& control_;bool reached_=false;
public:
    explicit CoJStationaryAimFrame(CoJStationaryAimControl& control) noexcept:control_(control){}
    CoJStationaryAimFrame(const CoJStationaryAimFrame&)=delete;
    ~CoJStationaryAimFrame(){if(!reached_)control_.Invalidate();}
    void ReachedFireBoundary() noexcept {reached_=true;}
};
}
