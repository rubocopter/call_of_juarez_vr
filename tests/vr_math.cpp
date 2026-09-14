#include "runtime/vr_math.hpp"

#include <cmath>
#include <iostream>

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
        !Near(identity.orientation.w, 1.0F)) {
        return Fail("identity transform conversion failed");
    }

    const Pose yaw = PoseFromRigidTransform3x4({
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        -1.0F, 0.0F, 0.0F, 0.0F,
    });
    constexpr float kSqrtHalf = 0.70710678F;
    if (!Near(std::fabs(yaw.orientation.y), kSqrtHalf) ||
        !Near(std::fabs(yaw.orientation.w), kSqrtHalf)) {
        return Fail("quaternion conversion failed");
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
