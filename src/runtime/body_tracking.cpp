#include "runtime/body_tracking.hpp"

#include <cmath>

namespace cojvr::runtime {

void BodyTracker::SetHeadPose(const Pose& pose) noexcept {
    pose_.head = pose;
}

void BodyTracker::SetHandPoses(const Pose& left, const Pose& right) noexcept {
    pose_.left_hand = left;
    pose_.right_hand = right;
}

void BodyTracker::SetPlayerSpaceWriter(PlayerSpaceWriter writer) noexcept {
    player_space_writer_ = writer;
}

void BodyTracker::UpdatePlayerSpace(const Pose& head) noexcept {
    // Keep the actor origin independent from the render camera. Positional HMD
    // movement is a body-space input, not a camera/world transform.
    player_space_ = {};
    player_space_.orientation = head.orientation;
    player_space_.orientation_valid = head.orientation_valid;
    player_space_.position = head.position;
    // Horizontal HMD displacement becomes actor displacement. Vertical head
    // motion is retained by the camera/IK chain, not by the player capsule.
    player_space_.position.y = 0.0F;
    player_space_.position_valid = head.position_valid;

    if (player_space_writer_ != nullptr) {
        player_space_writer_(player_space_);
    }
    pose_.pelvis = {};
    pose_.pelvis.orientation = head.orientation;
    pose_.pelvis.orientation_valid = head.orientation_valid;
    // Initial pelvis estimate: keep actor origin under the HMD tracking space
    // instead of making the camera transform the character transform. A later
    // game adapter can replace this with locomotion/capsule data.
    pose_.pelvis.position = player_space_.position;
    pose_.pelvis.position.y = head.position.y - 0.9F;
    pose_.pelvis.position_valid = head.position_valid;

    // Initial foot anchors are body estimates. They give the IK layer stable
    // targets before the game adapter exposes the real locomotion/collision
    // state. Native foot placement remains owned by the game integration.
    pose_.left_foot = pose_.pelvis;
    pose_.right_foot = pose_.pelvis;
    pose_.left_foot.position.x -= 0.12F;
    pose_.right_foot.position.x += 0.12F;
    pose_.left_foot.position.y -= 0.9F;
    pose_.right_foot.position.y -= 0.9F;
    pose_.left_foot.position_valid = head.position_valid;
    pose_.right_foot.position_valid = head.position_valid;

    // Keep generated body targets inside plausible human proportions. The HMD
    // pose itself remains untouched because it is the render authority.
    ConstrainHeadToBody(0.45F, 0.35F);
}

void BodyTracker::ConstrainHeadToBody(float minimum_height, float maximum_forward_offset) noexcept {
    // The HMD remains the camera authority, but the generated body anchor is
    // prevented from following impossible torso penetrations. This is a
    // placeholder constraint until the game capsule is exposed by the adapter.
    if (!pose_.head.position_valid || !pose_.pelvis.position_valid) return;

    // Only constrain the generated torso target. Never clamp the HMD pose:
    // doing so would make physical head movement fight the tracking system.
    float head_x = pose_.head.position.x;
    float head_y = pose_.head.position.y;
    float head_z = pose_.head.position.z;
    if (head_y < pose_.pelvis.position.y + minimum_height) {
        head_y = pose_.pelvis.position.y + minimum_height;
    }

    const float dx = head_x - pose_.pelvis.position.x;
    const float dz = head_z - pose_.pelvis.position.z;
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq > maximum_forward_offset * maximum_forward_offset) {
        const float scale = maximum_forward_offset / std::sqrt(distance_sq);
        head_x = pose_.pelvis.position.x + dx * scale;
        head_z = pose_.pelvis.position.z + dz * scale;
    }

    pose_.chest.position.x = head_x;
    pose_.chest.position.y = head_y;
    pose_.chest.position.z = head_z;
}

} // namespace cojvr::runtime
