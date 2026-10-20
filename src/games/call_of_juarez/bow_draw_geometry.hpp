#pragma once
#include "games/call_of_juarez/bow_draw_native.hpp"
#include "runtime/vr_types.hpp"
#include <cmath>

namespace cojvr::games::call_of_juarez {
// Keep the undeformed authored DArrow point relative to the mapped root during
// a grab. Native tension animation must not move the gesture's measuring origin.
class CoJBowNockReference final {
public:
    bool Observe(const CoJBowDrawNativeObservation& native,std::uint64_t player,
        std::uint64_t context,bool grabbed,runtime::Vec3& world_cm) noexcept {
        world_cm={};
        if(!native.valid||!native.weapon_id||!player||!context||
            !bow_draw_native_detail::Rigid(native.root)||!bow_draw_native_detail::Rigid(native.arrow)){
            Reset();return false;}
        if(grabbed&&(!valid_||native.weapon_id!=weapon_||player!=player_||context!=context_)){
            Reset();return false;}
        if(!grabbed){
            const auto& r=native.root;const auto& a=native.arrow;
            const std::array<float,3> d{a.position[0]-r.position[0],a.position[1]-r.position[1],a.position[2]-r.position[2]};
            const auto dot=[&](const auto& axis){return d[0]*axis[0]+d[1]*axis[1]+d[2]*axis[2];};
            local_={dot(r.right),dot(r.up),dot(r.forward)};
            weapon_=native.weapon_id;player_=player;context_=context;valid_=true;
        }
        const auto& r=native.root;
        world_cm={r.position[0]+r.right[0]*local_[0]+r.up[0]*local_[1]+r.forward[0]*local_[2],
            r.position[1]+r.right[1]*local_[0]+r.up[1]*local_[1]+r.forward[1]*local_[2],
            r.position[2]+r.right[2]*local_[0]+r.up[2]*local_[1]+r.forward[2]*local_[2]};
        return std::isfinite(world_cm.x)&&std::isfinite(world_cm.y)&&std::isfinite(world_cm.z);
    }
    void Reset() noexcept {valid_=false;weapon_=0;player_=context_=0;local_={};}
private:
    bool valid_=false;
    std::uint32_t weapon_=0;
    std::uint64_t player_=0,context_=0;
    std::array<float,3> local_{};
};
}
