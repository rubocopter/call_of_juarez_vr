#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"

namespace cojvr::games::call_of_juarez {
namespace mounted_motion_detail {
#if defined(_WIN32)
using BooleanField=std::uint8_t(__stdcall*)(void*,void*,void*);
#else
using BooleanField=std::uint8_t(*)(void*,void*,void*);
#endif
}
// Read-only cached native state, sampled before the next simulation update.
// Input requests are dimensionless; cached ODE velocity is cm/s and the native
// UpdateTurnSpeed rate is degrees/s. Neither is a same-frame displacement proof.
struct CoJMountedMotionSnapshot {
    bool valid=false;
    std::uint32_t player_id=0,horse_id=0;
    std::uint64_t player_generation=0; // Assigned only by the bridge.
    float forward_request=0,side_request=0,turn_rate_degrees_s=0;
    runtime::Vec3 velocity_cm_s{},rider_velocity_cm_s{};
    int speed_key_state=0,speed_state=0;
    bool run=false,rider_run=false,can_move=false,tied=false,new_player_control=false;
};

// Exact-build admitted owner thread only. No global refs, gameplay callbacks,
// controller retention, writes, native update or scratch-vector getters.
inline bool ReadCoJMountedMotion(void* env,void* player,CoJMountedMotionSnapshot& out) noexcept {
    using namespace long_gun_observer_detail;
    out={};
    if(!env||!player||!*static_cast<void***>(env))return false;
    for(int slot:{15,17,23,24,31,33,51,94,95,96,100,102})
        if(!(*static_cast<void***>(env))[slot])return false;
    Reader r{env};if(!r.Check())return false;
    auto player_class=r.Class(player);
    void* horse_field=r.Field(player_class,"m_cHorse","LHorse;");
    void* player_id_method=r.Method(player_class,"GetThisID","()I");
    auto horse=r.ObjectField(player,horse_field);
    if(!r.ok||!horse.value)return false;
    auto horse_class=r.Class(horse);
    void* rider_field=r.Field(horse_class,"m_cHorseman","LHumanBeing;");
    void* horse_id_method=r.Method(horse_class,"GetThisID","()I");
    const auto owner_matches=[&]() noexcept {
        auto current_horse=r.ObjectField(player,horse_field);
        auto current_rider=r.ObjectField(horse,rider_field);
        return r.ok&&current_horse.value&&current_rider.value&&
            r.Equal(current_horse,horse)&&r.Equal(current_rider,player);
    };
    if(!r.ok||!owner_matches())return false;
    const auto player_id=static_cast<std::uint32_t>(r.Int(player,player_id_method));
    const auto horse_id=static_cast<std::uint32_t>(r.Int(horse,horse_id_method));
    if(!r.ok||!ValidId(player_id)||!ValidId(horse_id)||player_id==horse_id)return false;
    constexpr const char* floats[]{"m_fFwdLocalSpeed","m_fSideLocalSpeed","m_fCurrentTurnSpeed"};
    std::array<void*,3> scalar_fields{};
    for(unsigned i=0;i<3;++i)scalar_fields[i]=r.Field(horse_class,floats[i],"F");
    void* key_field=r.Field(horse_class,"m_iSpeedKeyState","I");
    void* state_field=r.Field(horse_class,"m_iSpeedState","I");
    constexpr const char* bools[]{"m_bRun","m_bCanMove","bTied","m_bNewPlayerControl"};
    std::array<void*,4> bool_fields{};
    for(unsigned i=0;i<4;++i)bool_fields[i]=r.Field(horse_class,bools[i],"Z");
    void* rider_run_field=r.Field(player_class,"m_bRun","Z");
    void* velocity_field=r.Field(horse_class,"m_vGlobalSpeed","LVector;");
    void* rider_velocity_field=r.Field(player_class,"m_vGlobalSpeed","LVector;");
    if(!r.ok)return false;
    // Keep each cached vector local until both reads finish; numeric agreement
    // alone must not hide a replaced vector or reciprocal owner.
    auto velocity=r.ObjectField(horse,velocity_field);
    auto rider_velocity=r.ObjectField(player,rider_velocity_field);
    if(!r.ok||!velocity.value||!rider_velocity.value)return false;
    auto vector_class=r.Class(velocity);
    std::array<void*,3> axes{};
    constexpr const char* axis_names[]{"fX","fY","fZ"};
    for(unsigned i=0;i<3;++i)axes[i]=r.Field(vector_class,axis_names[i],"F");
    if(!r.ok)return false;
    const auto boolean=[&](void* object,void* field) noexcept {
        const bool value=r.ok&&r.Fn<mounted_motion_detail::BooleanField>(96)(env,object,field)!=0;
        r.Check();return value;
    };
    const auto read=[&](CoJMountedMotionSnapshot& s) noexcept {
        s.player_id=static_cast<std::uint32_t>(r.Int(player,player_id_method));
        s.horse_id=static_cast<std::uint32_t>(r.Int(horse,horse_id_method));
        s.forward_request=r.FloatField(horse,scalar_fields[0]);
        s.side_request=r.FloatField(horse,scalar_fields[1]);
        s.turn_rate_degrees_s=r.FloatField(horse,scalar_fields[2]);
        s.speed_key_state=r.IntField(horse,key_field);s.speed_state=r.IntField(horse,state_field);
        s.run=boolean(horse,bool_fields[0]);s.can_move=boolean(horse,bool_fields[1]);
        s.tied=boolean(horse,bool_fields[2]);s.new_player_control=boolean(horse,bool_fields[3]);
        s.rider_run=boolean(player,rider_run_field);
        s.velocity_cm_s={r.FloatField(velocity,axes[0]),r.FloatField(velocity,axes[1]),r.FloatField(velocity,axes[2])};
        s.rider_velocity_cm_s={r.FloatField(rider_velocity,axes[0]),r.FloatField(rider_velocity,axes[1]),r.FloatField(rider_velocity,axes[2])};
        return r.ok;
    };
    CoJMountedMotionSnapshot a{},b{};
    if(!read(a)||!read(b))return false;
    const auto equal_vector=[](runtime::Vec3 x,runtime::Vec3 y) noexcept {
        return x.x==y.x&&x.y==y.y&&x.z==y.z;
    };
    if(a.player_id!=player_id||b.player_id!=player_id||a.horse_id!=horse_id||b.horse_id!=horse_id||
        a.forward_request!=b.forward_request||a.side_request!=b.side_request||
        a.turn_rate_degrees_s!=b.turn_rate_degrees_s||a.speed_key_state!=b.speed_key_state||
        a.speed_state!=b.speed_state||a.run!=b.run||a.rider_run!=b.rider_run||a.can_move!=b.can_move||
        a.tied!=b.tied||a.new_player_control!=b.new_player_control||
        !equal_vector(a.velocity_cm_s,b.velocity_cm_s)||!equal_vector(a.rider_velocity_cm_s,b.rider_velocity_cm_s))return false;
    auto velocity_after=r.ObjectField(horse,velocity_field);
    auto rider_velocity_after=r.ObjectField(player,rider_velocity_field);
    if(!r.ok||!r.Equal(velocity,velocity_after)||!r.Equal(rider_velocity,rider_velocity_after)||!owner_matches())return false;
    a.valid=true;out=a;return true;
}

// Attempt budget, so failed observations cannot turn into unbounded retries.
// Losing a mount does not refill it. Explicit disabling starts a new interval.
class CoJMountedTraceBudget {
    std::uint32_t attempts_=0;
    std::uint64_t frame_=0,time_=0;
public:
    [[nodiscard]] bool exhausted()const noexcept{return attempts_>=2048;}
    [[nodiscard]] std::uint32_t attempts()const noexcept{return attempts_;}
    // Process the command before menu/tracking/resource-loss early returns.
    void ObserveEnabled(bool enabled) noexcept {if(!enabled)*this={};}
    bool Admit(bool enabled,bool eligible,std::uint64_t frame,std::uint64_t now_us) noexcept {
        ObserveEnabled(enabled);
        if(!enabled)return false;
        if(!eligible||!frame||!now_us||exhausted()||frame<=frame_)return false;
        if(attempts_&&now_us<time_){time_=now_us;return false;}
        if(attempts_&&now_us-time_<100000)return false;
        ++attempts_;frame_=frame;time_=now_us;return true;
    }
};
} // namespace cojvr::games::call_of_juarez
