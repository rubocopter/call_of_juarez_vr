#pragma once
#include "runtime/vr_types.hpp"
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
#include <cmath>

namespace cojvr::games::call_of_juarez {

// Presentation only. 0 = native right hand, 1 = native left hand. The native
// empty-hand eligibility gate still decides whether the finger pose is applied.
struct CoJReloadPinchInput {
    bool manual_load=false;
    bool cartridge_held=false;
    int cartridge_hand=2;
    int armed_hand=-1;
    int claimed_hand=2;
    bool trigger_held=false;
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
    if(!input.manual_load||
       (input.armed_hand!=0&&input.armed_hand!=1)||
       !tracked(input.left)||!tracked(input.right))return false;

    const int support=1-input.armed_hand;
    const bool carrying=input.cartridge_held&&input.cartridge_hand==support&&
        input.claimed_hand==support&&input.trigger_held;
    // Native reload holds a two-hand loading pose even with no VR round. Keep
    // ordinary estimated/rest fingers while waiting, rather than inheriting it.
    auto& target=support==0?right:left;
    if(!carrying){
        if(!target.available){target={};target.available=true;
            target.quality=runtime::FingerTrackingQuality::estimated;}
        return false;
    }

    // Interpolate the established exact-game authored rest/fist quaternions.
    // These values are an approximate thumb/index pinch, pending visor tuning.
    runtime::FingerTrackingState pinch{};
    pinch.available=true;
    pinch.quality=runtime::FingerTrackingQuality::estimated;
    pinch.curls={.62F,.55F,.88F,.88F,.82F};
    (input.cartridge_hand==0?right:left)=pinch;
    return true;
}

// Recovery already distinguishes an actual carried object from temporary
// two-hand reload occupancy. Only a bridge-owned same-weapon manual interval
// may use this exception to the ordinary empty-idle finger gate.
[[nodiscard]] inline bool CoJManualSupportFingersAllowed(bool owned,int hand,
    const CoJMotionReloadOwner& recovery,const CoJReloadTraceSnapshot& native) noexcept {
    return owned&&hand>=0&&hand<2&&recovery.valid&&recovery.single_round_supported&&
        native.valid&&native.probe_valid&&native.weapon_id!=0&&
        recovery.weapon_id==native.weapon_id&&recovery.armed_hand==native.armed_hand&&
        native.armed_hand==1-hand&&(native.probe_status==1||native.probe_status==2);
}
}
