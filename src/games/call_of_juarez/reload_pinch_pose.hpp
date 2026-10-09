#pragma once
#include "runtime/vr_types.hpp"
#include <cmath>

namespace cojvr::games::call_of_juarez {

// Presentation only. 0 = native right hand, 1 = native left hand. The native
// empty-hand eligibility gate still decides whether the finger pose is applied.
struct CoJReloadPinchInput {
    bool manual_load=false;
    bool cartridge_held=false;
    int cartridge_hand=2;
    int armed_hand=-1;
    runtime::Pose left{},right{};
};

[[nodiscard]] inline bool ApplyCoJReloadPinch(const CoJReloadPinchInput& input,
    runtime::FingerTrackingState& left, runtime::FingerTrackingState& right) noexcept {
    const auto tracked=[](const runtime::Pose& pose) noexcept {
        const auto& p=pose.position;
        const auto& q=pose.orientation;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        return pose.position_valid&&pose.orientation_valid&&
            std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
            std::isfinite(norm)&&std::fabs(norm-1.F)<.02F;
    };
    if(!input.manual_load||!input.cartridge_held||
       (input.cartridge_hand!=0&&input.cartridge_hand!=1)||
       (input.armed_hand!=0&&input.armed_hand!=1)||
       input.cartridge_hand==input.armed_hand||
       !tracked(input.left)||!tracked(input.right))return false;

    // Interpolate the established exact-game authored rest/fist quaternions.
    // These values are an approximate thumb/index pinch, pending visor tuning.
    runtime::FingerTrackingState pinch{};
    pinch.available=true;
    pinch.quality=runtime::FingerTrackingQuality::estimated;
    pinch.curls={.62F,.55F,.88F,.88F,.82F};
    (input.cartridge_hand==0?right:left)=pinch;
    return true;
}
}
