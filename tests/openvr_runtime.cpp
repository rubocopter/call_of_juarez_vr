#include "runtime/openvr_runtime.hpp"

#include <cmath>
#include <iostream>
#include <utility>

namespace {

bool Near(const float a, const float b) {
    return std::fabs(a - b) <= 0.0001F;
}

} // namespace

int main() {
    using cojvr::runtime::OpenVrDigitalActionEdge;
    using cojvr::runtime::OpenVrProjectionRawToEyeFov;

    // OpenVR GetProjectionRaw convention observed on the PSVR2 runtime:
    // left/right retain their signs while top is negative and bottom positive.
    const auto fov = OpenVrProjectionRawToEyeFov(-1.84F, 0.95F, -1.33F, 1.33F);
    if (!(fov.angle_left < 0.0F && fov.angle_right > 0.0F &&
          fov.angle_up > 0.0F && fov.angle_down < 0.0F &&
          Near(fov.angle_up, -fov.angle_down))) {
        std::cerr << "OpenVR raw projection did not convert to the neutral EyeFov sign contract\n";
        return 1;
    }

    OpenVrDigitalActionEdge edge;
    if (edge.Update(false, false) || !edge.Update(true, true) ||
        edge.Update(true, true) || edge.Update(true, false) ||
        !edge.Update(true, true)) {
        std::cerr << "OpenVR digital action press-edge semantics are incorrect\n";
        return 1;
    }
    edge.Reset();
    if (!edge.Update(true, true)) {
        std::cerr << "OpenVR digital action edge reset did not allow a fresh press\n";
        return 1;
    }

    // Move construction/assignment must leave the source safely destructible
    // and queryable even when no live OpenVR session exists.
    cojvr::runtime::OpenVrRuntime first;
    cojvr::runtime::OpenVrRuntime second(std::move(first));
    if (first.initialized() || second.initialized()) {
        std::cerr << "OpenVR move construction invented initialized state\n";
        return 1;
    }
    cojvr::runtime::Pose moved_pose{};
    std::array<cojvr::runtime::EyeView, 2> moved_eyes{};
    if (first.ReadHmdPose(moved_pose) || first.ReadEyeConfiguration(moved_eyes)) {
        std::cerr << "moved-from OpenVR runtime accepted live-state reads\n";
        return 1;
    }
    cojvr::runtime::OpenVrRuntime third;
    third = std::move(second);
    if (second.initialized() || third.initialized() ||
        second.global_actions_initialized() || third.global_actions_initialized()) {
        std::cerr << "OpenVR move assignment retained invalid ownership state\n";
        return 1;
    }

    std::cout << "OpenVR projection/input contract tests passed\n";
    return 0;
}
