#pragma once

#include "runtime/hud_text.hpp"
#include "runtime/vr_math.hpp"

#include <cmath>

namespace cojvr::runtime {

namespace interaction_hand_detail {
[[nodiscard]] inline bool Valid(const Pose& pose) noexcept {
    const auto q=pose.orientation;
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return pose.position_valid&&pose.orientation_valid&&
        std::isfinite(pose.position.x)&&std::isfinite(pose.position.y)&&
        std::isfinite(pose.position.z)&&std::isfinite(norm)&&
        std::abs(norm-1.F)<=.001F;
}
}

// Capture-only feedback along the left aim's local -Z, in metres. Both poses
// must share tracking space. The game owner supplies visibility after its live
// hand-ray/context checks. Nominal presentation depth is not a collision hit or
// native interaction range. There is no head-ray fallback or target selection.
[[nodiscard]] inline bool BuildInteractionHandOverlay(
    const Pose& head, const Pose& left_aim, const bool visible,
    StereoHudTextOverlay& out) noexcept {
    out.interaction_gaze_visible = false;
    out.interaction_gaze_head_corners = {};
    if (!visible||!interaction_hand_detail::Valid(head)||
        !interaction_hand_detail::Valid(left_aim)) return false;
    // Small presentation footprint at finite binocular depth; not native range.
    constexpr float half_width = .012F, distance = 1.5F;
    constexpr std::array<Vec3,4> local_corners{
        Vec3{-half_width, half_width, -distance},
        Vec3{ half_width, half_width, -distance},
        Vec3{-half_width,-half_width, -distance},
        Vec3{ half_width,-half_width, -distance},
    };
    const auto q=head.orientation;
    const Quaternion inverse_head{-q.x,-q.y,-q.z,q.w};
    const Vec3 offset{left_aim.position.x-head.position.x,
        left_aim.position.y-head.position.y,left_aim.position.z-head.position.z};
    std::array<Vec3,4> captured{};
    for(std::size_t i=0;i<captured.size();++i) {
        const auto hand_corner=RotateVector(left_aim.orientation,local_corners[i]);
        captured[i]=RotateVector(inverse_head,{offset.x+hand_corner.x,
            offset.y+hand_corner.y,offset.z+hand_corner.z});
        const auto p=captured[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) return false;
    }
    out.interaction_gaze_head_corners = captured;
    out.interaction_gaze_visible = true;
    return true;
}

} // namespace cojvr::runtime
