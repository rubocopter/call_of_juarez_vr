#pragma once

#include "games/call_of_juarez/bow_draw_session.hpp"
#include "games/call_of_juarez/holster_guides.hpp"

namespace cojvr::games::call_of_juarez {
// The native DArrow nock and the head-world reference were captured with the
// same camera basis. Invert exactly that mapping; do not approximate the
// loading/string point from the animated hand or an authored bow length.
struct CoJBowStringGuideSample {
    CoJBowDrawSessionOutput bow{};
    runtime::Vec3 head_world_m{},camera_right{},camera_up{},camera_forward{};
    runtime::Pose head{},right_hand{};
    std::uint64_t pose_sequence=0;
    bool allowed=false;
};

inline bool AppendCoJBowStringGuide(const CoJBowStringGuideSample& s,bool enabled,
    runtime::StereoHudTextOverlay& out) noexcept {
    if(!enabled||!s.allowed||!s.bow.nock_reference_valid||!s.bow.owner_token||
        !s.bow.nock_weapon_id||!s.pose_sequence||s.bow.nock_pose_sequence!=s.pose_sequence||
        s.bow.gesture.cancelled||s.bow.dispatch.cancelled||s.bow.dispatch.shot_requested||
        !CoJVirtualHolsterPoseValid(s.head)||!CoJVirtualHolsterPoseValid(s.right_hand)||
        out.holster_guides.count>=out.holster_guides.zones.size())return false;
    const auto finite=[](runtime::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto dot=[](runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const auto basis_ok=[&](runtime::Vec3 v){return finite(v)&&std::fabs(dot(v,v)-1.F)<=.01F;};
    if(!finite(s.head_world_m)||!finite(s.bow.nock_world_m)||
        !basis_ok(s.camera_right)||!basis_ok(s.camera_up)||!basis_ok(s.camera_forward)||
        std::fabs(dot(s.camera_right,s.camera_up))>.01F||
        std::fabs(dot(s.camera_right,s.camera_forward))>.01F||
        std::fabs(dot(s.camera_up,s.camera_forward))>.01F)return false;
    const runtime::Vec3 offset{s.bow.nock_world_m.x-s.head_world_m.x,
        s.bow.nock_world_m.y-s.head_world_m.y,s.bow.nock_world_m.z-s.head_world_m.z};
    const runtime::Vec3 tracking{s.head.position.x+dot(offset,s.camera_right),
        s.head.position.y+dot(offset,s.camera_up),
        s.head.position.z+dot(offset,s.camera_forward)};
    const auto q=runtime::NormalizeQuaternion(s.head.orientation);
    const auto centre=runtime::RotateVector({-q.x,-q.y,-q.z,q.w},
        {tracking.x-s.head.position.x,tracking.y-s.head.position.y,tracking.z-s.head.position.z});
    // Captured HUD has no scene depth. Cull points behind the face/too far away
    // and keep a small, translucent locator at the observed nock only.
    if(!finite(tracking)||!finite(centre)||centre.z>=-.05F||centre.z< -2.F)return false;
    runtime::HolsterGuideSurface guide{};
    guide.kind=runtime::GestureGuideKind::bow_string;
    const runtime::Vec3 reach{tracking.x-s.right_hand.position.x,
        tracking.y-s.right_hand.position.y,tracking.z-s.right_hand.position.z};
    guide.state=s.bow.gesture.drawing||dot(reach,reach)<=.12F*.12F?1:0;
    constexpr float half=.018F; // 3.6 cm locator; native draw admission is unchanged.
    for(unsigned j=0;j<4;++j)guide.head_corners[j]={centre.x+((j&1)?half:-half),
        centre.y+((j&2)?-half:half),centre.z};
    out.holster_guides.zones[out.holster_guides.count++]=guide;
    return true;
}
}
