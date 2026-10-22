#pragma once
#include "games/call_of_juarez/manual_reload_session.hpp"
#include "games/call_of_juarez/holster_guides.hpp"

namespace cojvr::games::call_of_juarez {
// Show the gesture's already-admitted source, without reimplementing its zone,
// opening a session, reserving ammunition or requesting a native transfer.
inline bool AppendCoJAmmunitionPickupGuide(const CoJManualReloadInput& s,
    const CoJManualReloadOutput& result,bool enabled,runtime::StereoHudTextOverlay& out) noexcept {
    const auto& hint=result.gesture.pickup_hint;
    if(!enabled||!s.enabled||!s.allowed||!s.raw.active||s.recentered||!result.tracked||result.cancel||
        !s.native.valid||!s.native.probe_valid||s.native.probe_status!=2||s.native.reserve_readback<=0||
        !s.player||!s.context||s.native.armed_hand<0||s.native.armed_hand>1||
        !hint.visible||hint.hand!=1-s.native.armed_hand||result.gesture.cartridge_held||
        !CoJVirtualHolsterPoseValid(s.head)||out.holster_guides.count>=out.holster_guides.zones.size())return false;
    const auto q=runtime::NormalizeQuaternion(s.head.orientation);
    const auto centre=runtime::RotateVector({-q.x,-q.y,-q.z,q.w},
        {hint.position.x-s.head.position.x,hint.position.y-s.head.position.y,hint.position.z-s.head.position.z});
    if(!std::isfinite(centre.x)||!std::isfinite(centre.y)||!std::isfinite(centre.z)||centre.z>=-.05F)return false;
    runtime::HolsterGuideSurface guide{};guide.kind=runtime::GestureGuideKind::ammunition_pickup;
    guide.state=!hint.ready?2:hint.in_range?1:0;
    constexpr float half=.035F; // Small 7 cm locator; the pickup radius remains gesture-owned.
    for(unsigned j=0;j<4;++j)guide.head_corners[j]={centre.x+((j&1)?half:-half),
        centre.y+((j&2)?-half:half),centre.z};
    out.holster_guides.zones[out.holster_guides.count++]=guide;
    return true;
}
}
