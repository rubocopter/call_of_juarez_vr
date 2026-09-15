#include "runtime/pose_source.hpp"

#include "runtime/vr_math.hpp"

#include <array>

namespace cojvr::runtime {
namespace {

float Dot(const Vec3 lhs, const Vec3 rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

} // namespace

void RelativePoseTracker::SetEnabled(const bool enabled) noexcept {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    relative_pose_ = {};
    last_sample_sequence_ = 0;
    if (enabled_) {
        base_valid_ = false;
        recenter_pending_ = true;
        return;
    }
    recenter_pending_ = false;
    base_valid_ = false;
    last_recenter_sequence_ = 0;
}

void RelativePoseTracker::RequestRecenter() noexcept {
    if (enabled_) recenter_pending_ = true;
}

bool RelativePoseTracker::Update(const PoseSample& sample) noexcept {
    if (!enabled_ || !sample.pose.orientation_valid) {
        relative_pose_ = {};
        return false;
    }

    const Quaternion orientation = NormalizeQuaternion(sample.pose.orientation);
    const Vec3 current_x = RotateVector(orientation, {1.0F, 0.0F, 0.0F});
    const Vec3 current_y = RotateVector(orientation, {0.0F, 1.0F, 0.0F});
    const Vec3 current_z = RotateVector(orientation, {0.0F, 0.0F, 1.0F});
    last_sample_sequence_ = sample.sequence;

    if (!base_valid_ || recenter_pending_) {
        base_x_ = current_x;
        base_y_ = current_y;
        base_z_ = current_z;
        base_valid_ = true;
        recenter_pending_ = false;
        last_recenter_sequence_ = sample.sequence;
        relative_pose_ = {};
        relative_pose_.orientation = {};
        relative_pose_.orientation_valid = true;
        return true;
    }

    const std::array<float, 12> relative_matrix{
        Dot(base_x_, current_x), Dot(base_x_, current_y), Dot(base_x_, current_z), 0.0F,
        Dot(base_y_, current_x), Dot(base_y_, current_y), Dot(base_y_, current_z), 0.0F,
        Dot(base_z_, current_x), Dot(base_z_, current_y), Dot(base_z_, current_z), 0.0F,
    };
    relative_pose_ = PoseFromRigidTransform3x4(relative_matrix);
    relative_pose_.position = {};
    relative_pose_.position_valid = false;
    return relative_pose_.orientation_valid;
}

bool RelativePoseTracker::CurrentPose(Pose& pose) const noexcept {
    pose = {};
    if (!enabled_ || !relative_pose_.orientation_valid) return false;
    pose = relative_pose_;
    return true;
}

} // namespace cojvr::runtime
