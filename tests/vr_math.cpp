#include "runtime/vr_math.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool Near(const float a, const float b) {
    return std::fabs(a - b) <= 0.0001F;
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace cojvr::runtime;

    const Pose identity = PoseFromRigidTransform3x4({
        1.0F, 0.0F, 0.0F, 1.25F,
        0.0F, 1.0F, 0.0F, -2.5F,
        0.0F, 0.0F, 1.0F, 3.75F,
    });
    if (!Near(identity.position.x, 1.25F) ||
        !Near(identity.position.y, -2.5F) ||
        !Near(identity.position.z, 3.75F) ||
        !Near(identity.orientation.w, 1.0F) ||
        !identity.orientation_valid || !identity.position_valid) {
        return Fail("identity transform conversion failed");
    }

    const Pose yaw = PoseFromRigidTransform3x4({
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        -1.0F, 0.0F, 0.0F, 0.0F,
    });
    constexpr float kSqrtHalf = 0.70710678F;
    if (!Near(std::fabs(yaw.orientation.y), kSqrtHalf) ||
        !Near(std::fabs(yaw.orientation.w), kSqrtHalf) ||
        !yaw.orientation_valid || !yaw.position_valid) {
        return Fail("quaternion conversion failed");
    }

    std::array<float, 12> yaw_roundtrip{};
    if (!RigidTransform3x4FromPose(yaw, yaw_roundtrip)) {
        return Fail("pose to rigid-transform conversion failed");
    }
    const Pose yaw_roundtrip_pose = PoseFromRigidTransform3x4(yaw_roundtrip);
    const Vec3 yaw_forward = RotateVector(yaw_roundtrip_pose.orientation, {0.0F, 0.0F, -1.0F});
    if (!yaw_roundtrip_pose.orientation_valid || !yaw_roundtrip_pose.position_valid ||
        !Near(yaw_roundtrip_pose.position.x, yaw.position.x) ||
        !Near(yaw_roundtrip_pose.position.y, yaw.position.y) ||
        !Near(yaw_roundtrip_pose.position.z, yaw.position.z) ||
        !Near(yaw_forward.x, -1.0F) || !Near(yaw_forward.z, 0.0F)) {
        return Fail("pose rigid-transform roundtrip changed tracking-space semantics");
    }

    Pose invalid_pose{};
    invalid_pose.orientation_valid = true;
    std::array<float, 12> invalid_matrix{};
    if (RigidTransform3x4FromPose(invalid_pose, invalid_matrix)) {
        return Fail("pose without valid tracking position produced a compositor transform");
    }

    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    const Pose non_finite_rotation = PoseFromRigidTransform3x4({
        nan, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    });
    if (non_finite_rotation.orientation_valid || !non_finite_rotation.position_valid) {
        return Fail("non-finite rotation was accepted as a valid rigid transform");
    }

    const Pose non_finite_position = PoseFromRigidTransform3x4({
        1.0F, 0.0F, 0.0F, std::numeric_limits<float>::infinity(),
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    });
    if (!non_finite_position.orientation_valid || non_finite_position.position_valid) {
        return Fail("non-finite position validity was not rejected independently");
    }

    const Pose scaled_rotation = PoseFromRigidTransform3x4({
        2.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    });
    if (scaled_rotation.orientation_valid) {
        return Fail("scaled basis was accepted as a rigid rotation");
    }

    const Pose reflected_rotation = PoseFromRigidTransform3x4({
        -1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    });
    if (reflected_rotation.orientation_valid) {
        return Fail("reflected basis was accepted as a right-handed rigid rotation");
    }

    const EyeFov fov = FovFromTangents(-1.0F, 1.0F, 1.0F, -1.0F);
    constexpr float kQuarterTurn = 0.78539816F;
    if (!Near(fov.angle_left, -kQuarterTurn) ||
        !Near(fov.angle_right, kQuarterTurn) ||
        !Near(fov.angle_up, kQuarterTurn) ||
        !Near(fov.angle_down, -kQuarterTurn)) {
        return Fail("FOV tangent conversion failed");
    }

    std::cout << "VR math tests passed\n";
    return 0;
}
