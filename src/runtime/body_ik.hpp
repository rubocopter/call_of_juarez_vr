#pragma once

#include "runtime/body_tracking.hpp"

namespace cojvr::runtime {

struct IKBodyPose {
    Pose pelvis{};
    Pose spine{};
    Pose chest{};
    Pose head{};
    Pose left_arm{};
    Pose right_arm{};
    Pose left_leg{};
    Pose right_leg{};
    Pose left_foot{};
    Pose right_foot{};
};

struct TwoBoneIKResult {
    Vec3 root{};
    Vec3 joint{};
    Vec3 end{};
    float upper_length = 0.0F;
    float lower_length = 0.0F;
    bool target_clamped = false;
    bool valid = false;
};

// Solves a two-segment chain in a caller-owned coordinate space. Lengths and
// positions deliberately have no unit policy here; the game adapter supplies
// measured native geometry and the XR/game-space conversion.
[[nodiscard]] TwoBoneIKResult SolveTwoBoneIK(
    Vec3 root,
    Vec3 target,
    Vec3 pole_direction,
    float upper_length,
    float lower_length) noexcept;

// Small game-neutral IK seed. It provides stable intermediate anchors for a
// game adapter before a real skeleton solver is attached.
class BodyIKSolver final {
public:
    [[nodiscard]] IKBodyPose Solve(const BodyPose& body) const noexcept;
};

} // namespace cojvr::runtime
