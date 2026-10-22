#pragma once
#include "games/call_of_juarez/long_gun_grip.hpp"
#include "games/call_of_juarez/holster_guides.hpp"
namespace cojvr::games::call_of_juarez {
inline bool AppendCoJLongGunGripGuide(const CoJLongGunGripInput& input,const CoJLongGunGripGeometry& g,
    const runtime::Pose& head,const runtime::Pose& right,bool enabled,bool attached,
    runtime::StereoHudTextOverlay& out) noexcept {
    const auto& c=input.controls;
    if(!enabled||attached||!c.enabled||!c.eligible||!c.tracking_valid||c.recentered||
        !c.grip_available||!c.support_trigger_available||!c.sequence||!c.input_generation||
        c.now_ms<c.sample_time_ms||c.now_ms-c.sample_time_ms>100||g.native.firing||
        !CoJVirtualHolsterPoseValid(head)||!CoJVirtualHolsterPoseValid(right)||
        out.holster_guides.count>=out.holster_guides.zones.size())return false;
    const auto acquisition=BuildCoJLongGunAcquisitionFrame(g);
    if(!acquisition.contact.valid)return false;
    const auto& x=g.world_from_tracking[0];const auto& u=g.world_from_tracking[1];const auto& f=g.world_from_tracking[2];
    const auto basis=runtime::PoseFromRigidTransform3x4({x.x,u.x,f.x,0,x.y,u.y,f.y,0,x.z,u.z,f.z,0});
    if(!basis.orientation_valid)return false;
    const auto delta=runtime::two_hand_detail::Sub(acquisition.contact.position,g.dominant_grip_cm);
    const runtime::Vec3 tracking{right.position.x+float(runtime::two_hand_detail::Dot(delta,x))*.01F,
        right.position.y+float(runtime::two_hand_detail::Dot(delta,u))*.01F,
        right.position.z+float(runtime::two_hand_detail::Dot(delta,f))*.01F};
    const auto q=runtime::NormalizeQuaternion(head.orientation);
    const auto centre=runtime::RotateVector({-q.x,-q.y,-q.z,q.w},runtime::two_hand_detail::Sub(tracking,head.position));
    if(!runtime::two_hand_detail::Finite(centre)||centre.z>=-.05F)return false;
    const auto candidate=BuildCoJLongGunGripFrame(g);
    const bool near=IsCoJLongGunGripFrameAdmitted(candidate)&&std::isfinite(c.acquisition_radius_m)&&
        c.acquisition_radius_m>0&&candidate.contact_distance_m<=c.acquisition_radius_m;
    runtime::HolsterGuideSurface guide{};guide.kind=runtime::GestureGuideKind::support_grip;
    guide.state=c.grip_held?2:near?1:0;
    constexpr float half=.0225F; // 4.5 cm marker; the actual capture radius remains separate.
    for(unsigned j=0;j<4;++j)guide.head_corners[j]={centre.x+((j&1)?half:-half),
        centre.y+((j&2)?-half:half),centre.z};
    out.holster_guides.zones[out.holster_guides.count++]=guide;
    return true;
}
}
