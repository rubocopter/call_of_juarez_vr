#pragma once

#include "runtime/vr_types.hpp"

namespace cojvr::runtime {

enum class BodyAnchor : unsigned char {
    head,
    left_hand,
    right_hand,
    pelvis,
    left_foot,
    right_foot,
};

struct BodyPose {
    Pose head{};
    // Generated torso anchor. It is derived from HMD/player constraints and
    // must remain separate from the authoritative HMD pose.
    Pose chest{};
    Pose left_hand{};
    Pose right_hand{};
    Pose pelvis{};
    Pose left_foot{};
    Pose right_foot{};
};

// Game-neutral body layer. The first consumer only needs HMD driven head and
// player-space reconciliation; controller and skeleton solving are added once
// the anchors have a game owner.
class BodyTracker final {
public:
    using PlayerSpaceWriter = void(*)(const Pose& player_space) noexcept;

    void SetHeadPose(const Pose& pose) noexcept;
    void SetHandPoses(const Pose& left, const Pose& right) noexcept;
    void SetPlayerSpaceWriter(PlayerSpaceWriter writer) noexcept;
    void UpdatePlayerSpace(const Pose& head) noexcept;
    void ConstrainHeadToBody(float minimum_height, float maximum_forward_offset) noexcept;
    [[nodiscard]] const Pose& PlayerSpace() const noexcept { return player_space_; }
    [[nodiscard]] const BodyPose& pose() const noexcept { return pose_; }

private:
    BodyPose pose_{};
    // Origin used by the game actor/capsule. Camera pose and body pose must not
    // share the same transform to avoid moving the whole world when the user
    // leans physically.
    Pose player_space_{};
    PlayerSpaceWriter player_space_writer_ = nullptr;
};

} // namespace cojvr::runtime
