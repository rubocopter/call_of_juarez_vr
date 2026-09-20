#include "games/call_of_juarez/camera_probe.hpp"
#include "runtime/vr_math.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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

cojvr::games::call_of_juarez::CameraProbeVector Cross(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

bool NearVector(
    const cojvr::games::call_of_juarez::CameraProbeVector& lhs,
    const cojvr::games::call_of_juarez::CameraProbeVector& rhs,
    float epsilon = 0.001F) {
    return Near(lhs.x, rhs.x, epsilon) && Near(lhs.y, rhs.y, epsilon) &&
        Near(lhs.z, rhs.z, epsilon);
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

cojvr::runtime::Quaternion MultiplyQuaternion(
    const cojvr::runtime::Quaternion lhs,
    const cojvr::runtime::Quaternion rhs) {
    return cojvr::runtime::NormalizeQuaternion({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    });
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
            R"({"enabled":false,"trackingEnabled":true,"bodyIkEnabled":true,"recenter":true})",
            command, &error) ||
        command.enabled || !command.tracking_enabled || !command.body_ik_enabled ||
        !command.recenter) {
        std::cerr << "tracking control command was not parsed: " << error << '\n';
        return 1;
    }

    if (ParseCameraProbeCommand(
            R"({"enabled":false,"trackingEnabled":true,"bodyIkEnabled":1})",
            command, &error)) {
        std::cerr << "invalid body IK control value was accepted\n";
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
        !Near(Length(yawed.right), 1.0F) || !Near(Dot(yawed.forward, yawed.up), 0.0F) ||
        !Near(Dot(yawed.forward, yawed.right), 0.0F) || !Near(Dot(yawed.up, yawed.right), 0.0F) ||
        !Near(CameraProbeBasisDeterminant(yawed), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(yawed) || yawed.forward.x <= 0.4F) {
        std::cerr << "camera orientation transform did not preserve an orthonormal basis\n";
        return 1;
    }

    const CameraProbeBasis native_identity{
        {0.0F, 0.0F, 1.0F},
        {0.0F, 1.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
    };
    const CameraProbeBasis reflected_identity{
        native_identity.forward,
        native_identity.up,
        {-1.0F, 0.0F, 0.0F},
    };
    if (!Near(CameraProbeBasisDeterminant(native_identity), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(native_identity) ||
        !Near(CameraProbeBasisDeterminant(reflected_identity), -1.0F) ||
        IsCameraProbeBasisRigidRightHanded(reflected_identity)) {
        std::cerr << "camera handedness guard did not distinguish native and reflected bases\n";
        return 1;
    }

    const CameraProbeBasis scaled_identity{
        native_identity.forward,
        native_identity.up,
        {1.1F, 0.0F, 0.0F},
    };
    const CameraProbeBasis skewed_identity{
        native_identity.forward,
        {0.1F, 1.0F, 0.0F},
        native_identity.right,
    };
    CameraProbeBasis nonfinite_identity = native_identity;
    nonfinite_identity.forward.x = std::numeric_limits<float>::quiet_NaN();
    if (IsCameraProbeBasisRigidRightHanded(scaled_identity) ||
        IsCameraProbeBasisRigidRightHanded(skewed_identity) ||
        IsCameraProbeBasisRigidRightHanded(nonfinite_identity)) {
        std::cerr << "camera rigidity guard accepted scaled, skewed or non-finite basis\n";
        return 1;
    }

    CameraProbeFrustum stereo_frustum{};
    const cojvr::runtime::EyeFov asymmetric_fov{
        std::atan(-1.2F), std::atan(0.8F), std::atan(1.1F), std::atan(-0.9F)};
    if (!BuildCameraProbeFrustum(asymmetric_fov, 0.25F, 500.0F, stereo_frustum) ||
        !Near(stereo_frustum.left, -0.30F) ||
        !Near(stereo_frustum.right, 0.20F) ||
        !Near(stereo_frustum.bottom, -0.225F) ||
        !Near(stereo_frustum.top, 0.275F) ||
        !Near(stereo_frustum.near_plane, 0.25F) ||
        !Near(stereo_frustum.far_plane, 500.0F)) {
        std::cerr << "asymmetric OpenVR FOV did not map to the native off-center frustum\n";
        return 1;
    }
    cojvr::runtime::EyeFov invalid_fov = asymmetric_fov;
    invalid_fov.angle_left = std::numeric_limits<float>::quiet_NaN();
    if (BuildCameraProbeFrustum(invalid_fov, 0.25F, 500.0F, stereo_frustum) ||
        BuildCameraProbeFrustum(asymmetric_fov, 0.0F, 500.0F, stereo_frustum) ||
        BuildCameraProbeFrustum(asymmetric_fov, 1.0F, 0.5F, stereo_frustum)) {
        std::cerr << "invalid stereo frustum input was accepted\n";
        return 1;
    }

    const CameraProbeVector head_position{10.0F, 20.0F, 30.0F};
    const CameraProbeVector left_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {-0.032F, 0.0F, 0.0F});
    const CameraProbeVector right_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {0.032F, 0.0F, 0.0F});
    const CameraProbeVector forward_eye = ApplyCameraEyeOffset(
        head_position, native_identity, {0.0F, 0.0F, -0.01F});
    const CameraProbeVector tracked_head = ApplyCameraTrackingOffset(
        head_position, native_identity, {0.10F, 0.20F, -0.30F});
    if (!Near(kGameUnitsPerMeter, 100.0F) ||
        !NearVector(left_eye, {6.8F, 20.0F, 30.0F}) ||
        !NearVector(right_eye, {13.2F, 20.0F, 30.0F}) ||
        !NearVector(forward_eye, {10.0F, 20.0F, 31.0F}) ||
        !NearVector(tracked_head, {20.0F, 40.0F, 60.0F})) {
        std::cerr << "metre eye/head translation did not map into CoJ centimetres\n";
        return 1;
    }

    cojvr::runtime::Pose unresolved_actor_pose{};
    unresolved_actor_pose.orientation = AxisAngle(0.0F, 1.0F, 0.0F, 25.0F);
    unresolved_actor_pose.orientation_valid = true;
    unresolved_actor_pose.position = {0.12F, -0.04F, 0.33F};
    unresolved_actor_pose.position_valid = true;
    const auto rotation_only_pose = SuppressPhysicalTrackingTranslation(unresolved_actor_pose);
    if (!rotation_only_pose.orientation_valid || !rotation_only_pose.position_valid ||
        !Near(rotation_only_pose.orientation.x, unresolved_actor_pose.orientation.x) ||
        !Near(rotation_only_pose.orientation.y, unresolved_actor_pose.orientation.y) ||
        !Near(rotation_only_pose.orientation.z, unresolved_actor_pose.orientation.z) ||
        !Near(rotation_only_pose.orientation.w, unresolved_actor_pose.orientation.w) ||
        !Near(rotation_only_pose.position.x, 0.0F) ||
        !Near(rotation_only_pose.position.y, 0.0F) ||
        !Near(rotation_only_pose.position.z, 0.0F)) {
        std::cerr << "rotation-only fallback did not suppress physical translation cleanly\n";
        return 1;
    }

    for (int yaw = -60; yaw <= 60; yaw += 10) {
        for (int pitch = -45; pitch <= 45; pitch += 5) {
            const CameraProbeBasis swept = ApplyCameraProbeOrientation(
                native_identity.forward,
                native_identity.up,
                static_cast<float>(yaw),
                static_cast<float>(pitch));
            const CameraProbeVector native_right = Cross(swept.up, swept.forward);
            if (!IsCameraProbeBasisRigidRightHanded(swept) ||
                !Near(CameraProbeBasisDeterminant(swept), 1.0F, 0.002F) ||
                !NearVector(swept.right, native_right, 0.002F)) {
                std::cerr << "camera orientation sweep violated native right/up/forward contract\n";
                return 1;
            }
        }
    }


    cojvr::runtime::RelativePoseTracker tracker;
    tracker.SetEnabled(true);
    cojvr::runtime::PoseSample base{};
    base.sequence = 1;
    base.pose.orientation = {};
    base.pose.orientation_valid = true;
    base.pose.position = {1.0F, 2.0F, 3.0F};
    base.pose.position_valid = true;
    if (!tracker.Update(base) || tracker.last_recenter_sequence() != 1) {
        std::cerr << "initial HMD pose was not captured as recenter base\n";
        return 1;
    }

    cojvr::runtime::Pose relative{};
    if (!tracker.CurrentPose(relative) || !relative.position_valid ||
        !Near(relative.position.x, 0.0F) || !Near(relative.position.y, 0.0F) ||
        !Near(relative.position.z, 0.0F)) {
        std::cerr << "recentered pose was not available\n";
        return 1;
    }

    // Recenter is a standing-space calibration. Pitch/roll at the instant the
    // user presses recenter must not tilt the tracking coordinate system away
    // from gravity. Returning the head to level at the same yaw should therefore
    // produce a level relative orientation and keep vertical motion vertical.
    cojvr::runtime::RelativePoseTracker tilted_recenter_tracker;
    tilted_recenter_tracker.SetEnabled(true);
    cojvr::runtime::PoseSample tilted_base{};
    tilted_base.sequence = 10;
    const auto yaw_30 = AxisAngle(0.0F, 1.0F, 0.0F, 30.0F);
    tilted_base.pose.orientation = MultiplyQuaternion(
        yaw_30,
        MultiplyQuaternion(
            AxisAngle(1.0F, 0.0F, 0.0F, -25.0F),
            AxisAngle(0.0F, 0.0F, 1.0F, 15.0F)));
    tilted_base.pose.orientation_valid = true;
    tilted_base.pose.position = {4.0F, 1.5F, -2.0F};
    tilted_base.pose.position_valid = true;
    if (!tilted_recenter_tracker.Update(tilted_base)) {
        std::cerr << "tilted recenter pose was rejected\n";
        return 1;
    }
    cojvr::runtime::PoseSample leveled_after_recenter = tilted_base;
    leveled_after_recenter.sequence = 11;
    leveled_after_recenter.pose.orientation = yaw_30;
    leveled_after_recenter.pose.position = {4.0F, 1.7F, -2.0F};
    if (!tilted_recenter_tracker.Update(leveled_after_recenter) ||
        !tilted_recenter_tracker.CurrentPose(relative) ||
        !relative.orientation_valid || !relative.position_valid) {
        std::cerr << "leveled pose after tilted recenter was rejected\n";
        return 1;
    }
    const CameraProbeBasis level_after_tilted_recenter = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!NearVector(level_after_tilted_recenter.forward, {0.0F, 0.0F, 1.0F}, 0.002F) ||
        !NearVector(level_after_tilted_recenter.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !Near(relative.position.x, 0.0F, 0.002F) ||
        !Near(relative.position.y, 0.20F, 0.002F) ||
        !Near(relative.position.z, 0.0F, 0.002F)) {
        std::cerr << "tilted recenter leaked pitch/roll into relative orientation or gravity axes\n";
        return 1;
    }
    cojvr::runtime::Pose absolute_controller{};
    absolute_controller.orientation = {};
    absolute_controller.orientation_valid = true;
    absolute_controller.position = {1.30F, 1.80F, 2.60F};
    absolute_controller.position_valid = true;
    cojvr::runtime::Pose relative_controller{};
    if (!tracker.TransformPose(absolute_controller, relative_controller) ||
        !relative_controller.orientation_valid || !relative_controller.position_valid ||
        !Near(relative_controller.position.x, 0.30F) ||
        !Near(relative_controller.position.y, -0.20F) ||
        !Near(relative_controller.position.z, -0.40F)) {
        std::cerr << "controller pose did not share the HMD recenter space\n";
        return 1;
    }
    const CameraProbeBasis centered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(centered.forward.x, 0.0F) || !Near(centered.forward.y, 0.0F) ||
        !Near(centered.forward.z, 1.0F) || !Near(centered.right.x, 1.0F) ||
        !Near(centered.right.y, 0.0F) || !Near(centered.right.z, 0.0F) ||
        !Near(CameraProbeBasisDeterminant(centered), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(centered)) {
        std::cerr << "recenter did not produce identity camera orientation\n";
        return 1;
    }

    const CameraProbeBasis naturally_tilted = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, 25.0F, 20.0F);
    const CameraProbeBasis vr_leveled = ApplyCameraPoseOrientation(
        naturally_tilted.forward, naturally_tilted.up, {});
    const float expected_yaw_x = std::sin(25.0F * 3.14159265358979323846F / 180.0F);
    const float expected_yaw_z = std::cos(25.0F * 3.14159265358979323846F / 180.0F);
    if (!Near(vr_leveled.forward.x, expected_yaw_x, 0.002F) ||
        !Near(vr_leveled.forward.y, 0.0F, 0.002F) ||
        !Near(vr_leveled.forward.z, expected_yaw_z, 0.002F) ||
        !NearVector(vr_leveled.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(vr_leveled)) {
        std::cerr << "VR recenter inherited natural camera pitch/roll instead of a level yaw base\n";
        return 1;
    }

    cojvr::runtime::PoseSample translated_sample = base;
    translated_sample.sequence = 2;
    translated_sample.pose.position = {1.10F, 2.20F, 2.70F};
    if (!tracker.Update(translated_sample) || !tracker.CurrentPose(relative) ||
        !relative.position_valid || !Near(relative.position.x, 0.10F) ||
        !Near(relative.position.y, 0.20F) || !Near(relative.position.z, -0.30F)) {
        std::cerr << "relative HMD position did not preserve recentered metre axes\n";
        return 1;
    }

    cojvr::runtime::PoseSample yaw_sample = base;
    yaw_sample.sequence = 3;
    yaw_sample.pose.orientation = AxisAngle(0.0F, 1.0F, 0.0F, -20.0F);
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "yaw HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_yaw = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    constexpr float kSin20 = 0.3420201433F;
    constexpr float kCos20 = 0.9396926208F;
    if (!Near(hmd_yaw.forward.x, -kSin20) || !Near(hmd_yaw.forward.z, kCos20) ||
        !Near(Length(hmd_yaw.forward), 1.0F) || !Near(Dot(hmd_yaw.forward, hmd_yaw.up), 0.0F) ||
        !Near(CameraProbeBasisDeterminant(hmd_yaw), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_yaw) || hmd_yaw.right.x <= 0.9F) {
        std::cerr << "HMD yaw did not preserve the live-observed CoJ horizontal direction mapping\n";
        return 1;
    }

    const CameraProbeBasis actor_owned_yaw = ApplyCameraProbeOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, -20.0F, 0.0F);
    const CameraProbeBasis stable_tracking_basis = BuildTrackingReferenceBasis(
        actor_owned_yaw.forward,
        actor_owned_yaw.up,
        -20.0F);
    if (!NearVector(stable_tracking_basis.forward, {0.0F, 0.0F, 1.0F}, 0.002F) ||
        !NearVector(stable_tracking_basis.up, {0.0F, 1.0F, 0.0F}, 0.002F) ||
        !NearVector(stable_tracking_basis.right, {1.0F, 0.0F, 0.0F}, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(stable_tracking_basis)) {
        std::cerr << "actor-owned yaw leaked into the stable controller/body reference\n";
        return 1;
    }
    const CameraProbeBasis compensated_hmd_yaw = ApplyCameraPoseOrientation(
        actor_owned_yaw.forward,
        actor_owned_yaw.up,
        relative.orientation,
        -20.0F);
    if (!NearVector(compensated_hmd_yaw.forward, actor_owned_yaw.forward, 0.002F) ||
        !NearVector(compensated_hmd_yaw.up, actor_owned_yaw.up, 0.002F) ||
        !IsCameraProbeBasisRigidRightHanded(compensated_hmd_yaw)) {
        std::cerr << "actor-owned HMD yaw was applied to the VR camera a second time\n";
        return 1;
    }

    cojvr::runtime::PoseSample pitch_sample = base;
    pitch_sample.sequence = 4;
    pitch_sample.pose.orientation = AxisAngle(1.0F, 0.0F, 0.0F, 10.0F);
    if (!tracker.Update(pitch_sample) || !tracker.CurrentPose(relative)) {
        std::cerr << "pitch HMD pose was rejected\n";
        return 1;
    }
    const CameraProbeBasis hmd_pitch = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (hmd_pitch.forward.y >= -0.17F || hmd_pitch.forward.z <= 0.98F ||
        !Near(CameraProbeBasisDeterminant(hmd_pitch), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_pitch)) {
        std::cerr << "HMD pitch direction or scale is incorrect\n";
        return 1;
    }

    cojvr::runtime::PoseSample roll_sample = base;
    roll_sample.sequence = 5;
    roll_sample.pose.orientation = AxisAngle(0.0F, 0.0F, -1.0F, 15.0F);
    if (!tracker.Update(roll_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis hmd_roll = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(hmd_roll.forward.x, 0.0F) || !Near(hmd_roll.forward.y, 0.0F) ||
        !Near(hmd_roll.forward.z, 1.0F) || hmd_roll.up.x < 0.25F ||
        !Near(hmd_roll.up.y, 0.9659258F) ||
        !Near(hmd_roll.right.x, 0.9659258F) || hmd_roll.right.y > -0.25F ||
        !Near(CameraProbeBasisDeterminant(hmd_roll), 1.0F) ||
        !IsCameraProbeBasisRigidRightHanded(hmd_roll)) {
        std::cerr << "HMD roll did not rotate the native camera basis consistently\n";
        return 1;
    }

    cojvr::runtime::PoseSample origin_again = base;
    origin_again.sequence = 6;
    if (!tracker.Update(origin_again) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis returned = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(returned.forward.x, 0.0F) || !Near(returned.forward.y, 0.0F) ||
        !Near(returned.forward.z, 1.0F)) {
        std::cerr << "returning to the physical origin accumulated camera rotation\n";
        return 1;
    }

    tracker.RequestRecenter();
    yaw_sample.sequence = 7;
    if (!tracker.Update(yaw_sample) || !tracker.CurrentPose(relative)) return 1;
    const CameraProbeBasis recentered = ApplyCameraPoseOrientation(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, relative.orientation);
    if (!Near(recentered.forward.x, 0.0F) || !Near(recentered.forward.z, 1.0F) ||
        !relative.position_valid || tracker.last_recenter_sequence() != 7) {
        std::cerr << "explicit recenter did not reset relative orientation\n";
        return 1;
    }

    cojvr::runtime::PoseSample invalid{};
    invalid.sequence = 8;
    if (tracker.Update(invalid) || tracker.CurrentPose(relative)) {
        std::cerr << "invalid tracking pose did not force passthrough\n";
        return 1;
    }
    tracker.SetEnabled(false);
    if (tracker.CurrentPose(relative)) {
        std::cerr << "disabled tracking did not clear relative orientation\n";
        return 1;
    }

    std::cout << "PASS - camera controls, stereo frustum/eye mapping, HMD 6DOF and recenter\n";
    return 0;
}
