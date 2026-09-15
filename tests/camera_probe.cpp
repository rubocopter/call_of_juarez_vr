#include "games/call_of_juarez/camera_probe.hpp"
#include "runtime/vr_math.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool Near(float lhs, float rhs, float epsilon = 0.001F) {
    return std::fabs(lhs - rhs) <= epsilon;
}

float Dot(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(const cojvr::games::call_of_juarez::CameraProbeVector& value) {
    return std::sqrt(Dot(value, value));
}

cojvr::runtime::Quaternion AxisAngle(
    float x,
    float y,
    float z,
    float degrees) {
    constexpr float kDegreesToRadians = 0.01745329251994329577F;
    const float half = degrees * kDegreesToRadians * 0.5F;
    const float sine = std::sin(half);
    return cojvr::runtime::NormalizeQuaternion({x * sine, y * sine, z * sine, std::cos(half)});
}

} // namespace

int main() {
    using namespace cojvr::games::call_of_juarez;

    CameraProbeCommand command{};
    std::string error;
    if (!ParseCameraProbeCommand(
            R"({"enabled":true,"fovDegrees":110,"yawDegrees":30,"pitchDegrees":-10})",
            command, &error) ||
        !command.enabled || !command.override_fov || !Near(command.fov_degrees, 110.0F) ||
        !Near(command.yaw_degrees, 30.0F) || !Near(command.pitch_degrees, -10.0F)) {
        std::cerr << "valid control command was not parsed: " << error << '\n';
        return 1;
    }

    if (ParseCameraProbeCommand(R"({"enabled":true,"fovDegrees":200})", command, &error) ||
        ParseCameraProbeCommand(R"({"yawDegrees":10})", command, &error) ||
        ParseCameraProbeCommand(R"({"enabled":true,"pitchDegrees":nan})", command, &error) ||
        ParseCameraProbeCommand(R"({"enabled":false,"trackingEnabled":1})", command, &error)) {
        std::cerr << "invalid control command was accepted\n";
        return 1;
    }

    if (!ParseCameraProbeCommand(
            R"({"enabled":false,"trackingEnabled":true,"recenter":true})",
            command, &error) ||
        command.enabled || !command.tracking_enabled || !command.recenter) {
        std::cerr << "tracking control command was not parsed: " << error << '\n';
        return 1;
    }

    if (!IsSupportedChromeEngineHash(kInspectedChromeEngine3Sha256) ||
        !IsSupportedChromeEngineHash(
            "db69bc35919fe57187766771a2452aca11090474f6d63df1a85a80eded131ec8") ||
        IsSupportedChromeEngineHash(
            "AB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8")) {
        std::cerr << "ChromeEngine exact-build hash gate failed\n";
        return 1;
    }

    const CameraProbeBasis yawed = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, 30.0F, -10.0F);
    if (!Near(Length(yawed.forward), 1.0F) || !Near(Length(yawed.up), 1.0F) ||
        !Near(Dot(yawed.forward, yawed.up), 0.0F) || yawed.forward.x <= 0.4F) {
        std::cerr << "camera orientation transform did not preserve an orthonormal basis\n";
        return 1;
    }


    cojvr::runtime::RelativePoseTracker tracker;
    tracker.SetEnabled(true);
    cojvr::runtime::PoseSample base{};
    base.sequence = 1;
    base.pose.orientation = {};
    base.pose.orientation_valid = true;
    if (!tracker.Update(base) || tracker.last_recenter_sequence() != 1) {
        std::cerr << "initial HMD pose was not captured as recenter base\n";
        return 1;
    }

    cojvr::runtime::Pose relative{};
    if (!tracker.CurrentPose(relative)) {
        std::cerr << "recentered pose was not available\n";
        return 1;
    }
    const CameraProbeBasis centered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(centered.forward.x, 0.0F) || !Near(centered.forward.y, 0.0F) ||
        !Near(centered.forward.z, 1.0F)) {
        std::cerr << "recenter did not produce identity camera orientation\n";
        return 1;
    }

    cojvr::runtime::PoseSample yaw_sample{};
    yaw_sample.sequence = 2;
    yaw_sample.pose.orientation = AxisAngle(0.0F, 1.0F, 0.0F, -20.0F);
    yaw_sample.pose.orientation_valid = true;
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "yaw HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_yaw = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    constexpr float kSin20 = 0.3420201433F;
    constexpr float kCos20 = 0.9396926208F;
    if (!Near(hmd_yaw.forward.x, kSin20) || !Near(hmd_yaw.forward.z, kCos20) ||
        !Near(Length(hmd_yaw.forward), 1.0F) || !Near(Dot(hmd_yaw.forward, hmd_yaw.up), 0.0F)) {
        std::cerr << "HMD yaw did not map 1:1 into the CoJ camera basis\n";
        return 1;
    }

    cojvr::runtime::PoseSample pitch_sample{};
    pitch_sample.sequence = 3;
    pitch_sample.pose.orientation = AxisAngle(1.0F, 0.0F, 0.0F, 10.0F);
    pitch_sample.pose.orientation_valid = true;
    if (!tracker.Update(pitch_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "pitch HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_pitch = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (hmd_pitch.forward.y <= 0.17F || hmd_pitch.forward.z <= 0.98F) {
        std::cerr << "HMD pitch direction or scale is incorrect\n";
        return 1;
    }

    cojvr::runtime::PoseSample roll_sample{};
    roll_sample.sequence = 4;
    roll_sample.pose.orientation = AxisAngle(0.0F, 0.0F, -1.0F, 15.0F);
    roll_sample.pose.orientation_valid = true;
    if (!tracker.Update(roll_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis hmd_roll = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(hmd_roll.forward.x, 0.0F) || !Near(hmd_roll.forward.y, 0.0F) ||
        !Near(hmd_roll.forward.z, 1.0F) || hmd_roll.up.x <= 0.25F ||
        !Near(Dot(hmd_roll.forward, hmd_roll.up), 0.0F)) {
        std::cerr << "HMD roll did not preserve the camera basis contract\n";
        return 1;
    }

    cojvr::runtime::PoseSample origin_again = base;
    origin_again.sequence = 5;
    if (!tracker.Update(origin_again) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis returned = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(returned.forward.x, 0.0F) || !Near(returned.forward.y, 0.0F) ||
        !Near(returned.forward.z, 1.0F)) {
        std::cerr << "returning to the physical origin accumulated camera rotation\n";
        return 1;
    }

    tracker.RequestRecenter();
    yaw_sample.sequence = 6;
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis recentered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(recentered.forward.x, 0.0F) || !Near(recentered.forward.z, 1.0F) ||
        tracker.last_recenter_sequence() != 6) {
        std::cerr << "explicit recenter did not reset relative orientation\n";
        return 1;
    }

    cojvr::runtime::PoseSample invalid{};
    invalid.sequence = 7;
    if (tracker.Update(invalid) || tracker.CurrentPose(relative)) {
        std::cerr << "invalid tracking pose did not force passthrough\n";
        return 1;
    }
    tracker.SetEnabled(false);
    if (tracker.CurrentPose(relative)) {
        std::cerr << "disabled tracking did not clear relative orientation\n";
        return 1;
    }

    std::cout << "PASS - camera controls, exact-engine gate, HMD recenter and orientation mapping\n";
    return 0;
}
