#pragma once
#include <cstdint>

namespace cojvr::games::call_of_juarez {
enum class CoJWinchesterWeaponClass : std::uint8_t {unknown,ordinary,scoped};
inline constexpr const char* CoJWinchesterClassName(CoJWinchesterWeaponClass value) noexcept {
    switch(value){
        case CoJWinchesterWeaponClass::ordinary:return "WeaponRifleWinchester";
        case CoJWinchesterWeaponClass::scoped:return "WeaponRifleWinchesterScope";
        default:return nullptr;
    }
}
struct CoJWinchesterProbeInput {
    std::uint64_t now_us=0,generation=0,context=0;
    bool square_down=false,fire_down=false;
};
struct CoJWinchesterProbeObservation {
    std::uint32_t weapon_id=0;
    CoJWinchesterWeaponClass weapon_class=CoJWinchesterWeaponClass::unknown;
    bool native_reloading=false;
    int ammo_readback=0,rifle_reserve=0;
};
struct CoJWinchesterProbeEnd {
    const char* reason=nullptr;
    std::uint64_t generation=0,context=0,sample_us=0,last_sample_us=0;
    std::uint32_t weapon_id=0;
    unsigned sample_index=0;
    CoJWinchesterWeaponClass weapon_class=CoJWinchesterWeaponClass::unknown;
};
struct CoJWinchesterProbeTick {bool poll=false;CoJWinchesterProbeEnd end{};};
struct CoJWinchesterProbeSample {
    bool emit=false,unavailable=false;
    unsigned sample_index=0;
    std::uint64_t sample_gap_us=0;
    int ammo_delta=0,reserve_delta=0;
    CoJWinchesterProbeEnd before{},after{};
};
// Exact-game, owner-thread diagnostics only. Begin decides whether to read;
// Record consumes one validated value-only read. Emission limits never stop
// 350 ms discovery. Intent is not acknowledgement of a native shot/reload.
// Emit tick.end before polling, sample.before before the observation, and
// sample.after after it. These ends delimit index/delta/displacement history.
class CoJWinchesterReloadProbe {
public:
    CoJWinchesterProbeTick Begin(const CoJWinchesterProbeInput& in) noexcept {
        CoJWinchesterProbeTick out{};
        if(in.generation!=generation_||in.context!=context_||in.now_us<now_){
            out.end=Close(in.now_us<now_?"clock_reset":"context_changed");
            *this={};generation_=in.generation;context_=in.context;
        }
        now_=in.now_us;
        pending_=false;intent_duration_=0;
        if(!now_||!generation_||!context_)return out;
        if(samples_&&until_&&now_>=until_)out.end=Close("expired");
        if(in.square_down&&!square_)intent_duration_=12000000;
        else if(in.fire_down&&!fire_)intent_duration_=4000000;
        square_=in.square_down;fire_=in.fire_down;
        if(intent_duration_){
            const auto end=Close("retriggered");if(end.reason)out.end=end;
            Open(intent_duration_);
        }
        const auto interval=!exhausted_&&now_<until_?50000ULL:350000ULL;
        pending_=!attempt_||now_-attempt_>=interval;
        if(pending_)attempt_=now_;
        out.poll=pending_;return out;
    }
    CoJWinchesterProbeSample Record(bool read,const CoJWinchesterProbeObservation& v) noexcept {
        if(!pending_)return {};pending_=false;
        CoJWinchesterProbeSample out{};
        if(!read||!v.weapon_id||v.weapon_id==0xffffffffu||!CoJWinchesterClassName(v.weapon_class)){
            out.unavailable=weapon_!=0;
            (void)Close("unavailable");weapon_=0;weapon_class_=CoJWinchesterWeaponClass::unknown;
            native_reloading_=false;
            return out;
        }
        if(weapon_!=v.weapon_id||weapon_class_!=v.weapon_class){
            out.before=Close("owner_changed");weapon_=v.weapon_id;weapon_class_=v.weapon_class;
            native_reloading_=false;exhausted_=false;idle_=0;
            // Only this tick's intent may cross an equipment boundary.
            until_=intent_duration_?now_+intent_duration_:0;
        }
        if(v.native_reloading&&!native_reloading_){
            const auto end=Close("native_reload_started");if(end.reason)out.before=end;
            Open(2000000);
        }
        native_reloading_=v.native_reloading;
        if(v.native_reloading&&!exhausted_&&until_<now_+2000000)until_=now_+2000000;
        const bool active=!exhausted_&&now_<until_;
        if(exhausted_||(!active&&idle_>=4))return out;
        out.emit=true;out.sample_gap_us=last_?now_-last_:0;
        out.ammo_delta=last_?v.ammo_readback-ammo_:0;
        out.reserve_delta=last_?v.rifle_reserve-reserve_:0;
        out.sample_index=++samples_;if(!active)++idle_;
        last_=now_;ammo_=v.ammo_readback;reserve_=v.rifle_reserve;
        if(samples_==220){out.after=Close("exhausted");exhausted_=true;}
        else if(!active&&idle_==4)out.after=Close("idle_limit");
        return out;
    }
    CoJWinchesterProbeEnd Reset() noexcept {
        const auto end=Close("disabled");*this={};return end;
    }
private:
    CoJWinchesterProbeEnd Close(const char* reason) noexcept {
        CoJWinchesterProbeEnd end{};
        if(samples_)end={reason,generation_,context_,now_,last_,weapon_,samples_,weapon_class_};
        samples_=0;last_=0;ammo_=reserve_=0;until_=0;
        return end;
    }
    void Open(std::uint64_t duration) noexcept {
        exhausted_=false;idle_=0;until_=now_+duration;
    }
    std::uint64_t generation_=0,context_=0,now_=0,attempt_=0,last_=0,until_=0;
    std::uint64_t intent_duration_=0;
    std::uint32_t weapon_=0;
    CoJWinchesterWeaponClass weapon_class_=CoJWinchesterWeaponClass::unknown;
    unsigned samples_=0,idle_=0;
    int ammo_=0,reserve_=0;
    bool square_=false,fire_=false,pending_=false,native_reloading_=false,exhausted_=false;
};
}
