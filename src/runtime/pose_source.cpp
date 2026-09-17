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
        base_position_valid_ = false;
        recenter_pending_ = true;
        return;
    }
    recenter_pending_ = false;
    base_valid_ = false;
    base_position_valid_ = false;
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
        base_position_ = sample.pose.position;
        base_position_valid_ = sample.pose.position_valid;
        base_valid_ = true;
        recenter_pending_ = false;
        last_recenter_sequence_ = sample.sequence;
        relative_pose_ = {};
        relative_pose_.orientation = {};
        relative_pose_.orientation_valid = true;
        relative_pose_.position_valid = base_position_valid_;
        return true;
    }

    // If positional tracking becomes valid after an orientation-only sample,
    // adopt that first valid position as the positional origin. This avoids a
    // discontinuity while preserving the orientation recenter basis.
    if (!base_position_valid_ && sample.pose.position_valid) {
        base_position_ = sample.pose.position;
        base_position_valid_ = true;
    }

    const std::array<float, 12> relative_matrix{
        Dot(base_x_, current_x), Dot(base_x_, current_y), Dot(base_x_, current_z), 0.0F,
        Dot(base_y_, current_x), Dot(base_y_, current_y), Dot(base_y_, current_z), 0.0F,
        Dot(base_z_, current_x), Dot(base_z_, current_y), Dot(base_z_, current_z), 0.0F,
    };
    relative_pose_ = PoseFromRigidTransform3x4(relative_matrix);
    if (base_position_valid_ && sample.pose.position_valid) {
        const Vec3 delta{
            sample.pose.position.x - base_position_.x,
            sample.pose.position.y - base_position_.y,
            sample.pose.position.z - base_position_.z,
        };
        relative_pose_.position = {
            Dot(base_x_, delta),
            Dot(base_y_, delta),
            Dot(base_z_, delta),
        };
        relative_pose_.position_valid = true;
    } else {
        relative_pose_.position = {};
        relative_pose_.position_valid = false;
    }
    return relative_pose_.orientation_valid;
}

bool RelativePoseTracker::CurrentPose(Pose& pose) const noexcept {
    pose = {};
    if (!enabled_ || !relative_pose_.orientation_valid) return false;
    pose = relative_pose_;
    return true;
}

bool RelativePoseTracker::TransformPose(
    const Pose& absolute,
    Pose& relative) const noexcept {
    relative = {};
    if (!enabled_ || !base_valid_) return false;

    if (absolute.orientation_valid) {
        const Quaternion orientation = NormalizeQuaternion(absolute.orientation);
        const Vec3 current_x = RotateVector(orientation, {1.0F, 0.0F, 0.0F});
        const Vec3 current_y = RotateVector(orientation, {0.0F, 1.0F, 0.0F});
        const Vec3 current_z = RotateVector(orientation, {0.0F, 0.0F, 1.0F});
        const std::array<float, 12> relative_matrix{
            Dot(base_x_, current_x), Dot(base_x_, current_y), Dot(base_x_, current_z), 0.0F,
            Dot(base_y_, current_x), Dot(base_y_, current_y), Dot(base_y_, current_z), 0.0F,
            Dot(base_z_, current_x), Dot(base_z_, current_y), Dot(base_z_, current_z), 0.0F,
        };
        relative = PoseFromRigidTransform3x4(relative_matrix);
        relative.position = {};
        relative.position_valid = false;
    }

    if (base_position_valid_ && absolute.position_valid) {
        const Vec3 delta{
            absolute.position.x - base_position_.x,
            absolute.position.y - base_position_.y,
            absolute.position.z - base_position_.z,
        };
        relative.position = {
            Dot(base_x_, delta),
            Dot(base_y_, delta),
            Dot(base_z_, delta),
        };
        relative.position_valid = true;
    }
    return relative.orientation_valid || relative.position_valid;
}

} // namespace cojvr::runtime
