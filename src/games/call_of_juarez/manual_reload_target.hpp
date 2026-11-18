#pragma once
#include "games/call_of_juarez/manual_reload_session.hpp"
#include "games/call_of_juarez/holster_guides.hpp"
#include <cstring>

namespace cojvr::games::call_of_juarez {
// The active gesture receipt is the sole location source. Measured mechanical
// locators must not be drawn alongside this as competing insertion affordances.
// Readiness comes from gesture policy, never the visual marker's dimensions.
inline bool AppendCoJManualReloadTargetGuide(const CoJManualReloadInput& s,
    const CoJManualReloadOutput& result,std::uint64_t capture_sequence,
    bool enabled,runtime::StereoHudTextOverlay& out) noexcept {
    const auto& target=result.target;
    if(!enabled||!s.enabled||!s.allowed||!s.raw.active||s.recentered||!result.tracked||result.cancel||
        !target.valid||!target.player||!(target.context&1U)||!target.sequence||
        !target.input_generation||!target.observed_ms||!IsCoJNativeObjectIdentity(target.weapon)||
        !s.native.valid||!s.native.probe_valid||s.native.probe_status!=2||
        s.native.reserve_readback<=0||
        !s.native.weapon_class||!result.gesture.cartridge_held||
        target.armed_hand<0||target.armed_hand>1||
        result.gesture.cartridge_hand!=1-target.armed_hand||
        target.player!=s.player||target.context!=s.context||target.weapon!=s.native.weapon_id||
        target.armed_hand!=s.native.armed_hand||target.sequence!=s.sequence||
        target.observed_ms!=s.now||
        target.input_generation!=s.raw.input_context_generation||
        !capture_sequence||capture_sequence!=out.frame_sequence||!out.reload_feedback.owner_token||
        !CoJVirtualHolsterPoseValid(s.head)||!CoJVirtualHolsterPoseValid(s.left)||!CoJVirtualHolsterPoseValid(s.right)||
        out.holster_guides.count>=out.holster_guides.zones.size())return false;
    const bool exact=std::any_of(kCoJRevolvers.begin(),kCoJRevolvers.end(),[&](const auto& profile){
        return (profile.single_round||profile.manual_supported)&&profile.kind==s.native.kind&&
            (!profile.right_owned_only||target.armed_hand==0)&&
            std::strcmp(profile.class_name,s.native.weapon_class)==0;
    });
    if(!exact)return false;
    if(s.winchester_receiver&&!s.receiver.mouth_valid)return false;
    const auto armed=s.winchester_receiver?s.receiver.centre:
        target.armed_hand==0?s.right.position:s.left.position;
    // A modified or stale receipt must not select a different input target.
    if(!std::isfinite(target.position.x)||!std::isfinite(target.position.y)||!std::isfinite(target.position.z)||
        target.position.x!=armed.x||target.position.y!=armed.y||target.position.z!=armed.z)return false;
    const auto q=runtime::NormalizeQuaternion(s.head.orientation);
    const auto centre=runtime::RotateVector({-q.x,-q.y,-q.z,q.w},
        {target.position.x-s.head.position.x,target.position.y-s.head.position.y,
         target.position.z-s.head.position.z});
    if(!std::isfinite(centre.x)||!std::isfinite(centre.y)||!std::isfinite(centre.z)||
        centre.z>=-.05F||centre.z< -2.5F)return false;
    runtime::HolsterGuideSurface guide{};guide.kind=runtime::GestureGuideKind::loading_mouth;
    guide.state=result.gesture.insertion_ready?1:0;
    constexpr float half=.012F; // 2.4 cm visual centre marker, not an acceptance radius.
    for(unsigned i=0;i<4;++i)guide.head_corners[i]={centre.x+((i&1)?half:-half),
        centre.y+((i&2)?-half:half),centre.z};
    out.holster_guides.zones[out.holster_guides.count++]=guide;
    return true;
}
}
