#pragma once

#include "runtime/body_ik.hpp"

namespace cojvr::games::call_of_juarez {

// CONFLICT NOTE: BodyAdapter::Apply() writes absolute poses to upper arm, forearm,
// and hand bones simultaneously via BoneTransformWriter. Meanwhile, the IK system
// in camera_probe.cpp uses relative render-element rotations on the same bones.
// These two systems CONFLICT if both are enabled:
//   - BodyAdapter::Apply() overwrites the hierarchical render-element overlay
//   - RotateElementWithChildren expects to be the sole writer to these elements
// Only ONE of these systems should be active at a time. The render-element path
// is preferred for VR body tracking because it preserves animation twist/roll and
// respects the native hierarchy. BodyAdapter::Apply() is kept for
// potential future use with different game integrations that lack BoneRotate.

struct BodySkeletonState {
    bool available = false;
    void* actor = nullptr;
};

// Chrome Engine specific discovery stays behind this boundary. The runtime
// supplies IK targets; this layer owns the mapping from those targets to the
// game's actor joints once the native skeleton layout is identified.
struct SkeletonBinding {
    int pelvis = -1;
    int spine = -1;
    int spine1 = -1;
    int chest = -1;
    int neck = -1;
    int head = -1;
    int left_upper_arm = -1;
    int left_forearm = -1;
    int left_foretwist = -1;
    int left_hand = -1;
    int right_upper_arm = -1;
    int right_forearm = -1;
    int right_foretwist = -1;
    int right_hand = -1;
    int left_thigh = -1;
    int left_shin = -1;
    int left_foot = -1;
    int right_thigh = -1;
    int right_shin = -1;
    int right_foot = -1;
};

// HMD-relative offsets expressed in the angle convention consumed by
// ArmedPlayerBeing.UpdateBodyRotation. The game owns the final distribution
// across its animated elements; these values only describe the temporary
// field offsets needed to feed that native body path.
struct UpperBodyTrackingOffsets {
    float head_horizontal_degrees = 0.0F;
    float spine_horizontal_degrees = 0.0F;
    float head_vertical_degrees = 0.0F;
    bool valid = false;
};

struct PlayerSpaceReconciliation {
    cojvr::runtime::Vec3 desired_actor_position{};
    cojvr::runtime::Vec3 applied_world_offset{};
    cojvr::runtime::Vec3 applied_tracking_offset{};
    cojvr::runtime::Vec3 render_head_position{};
    bool valid = false;
};

struct ArmGeometrySample {
    cojvr::runtime::Vec3 shoulder{};
    cojvr::runtime::Vec3 elbow{};
    cojvr::runtime::Vec3 wrist{};
    cojvr::runtime::Vec3 upper_element_position{};
    cojvr::runtime::Vec3 upper_element_up{};
    cojvr::runtime::Vec3 upper_element_forward{};
    cojvr::runtime::Vec3 forearm_element_position{};
    cojvr::runtime::Vec3 forearm_element_up{};
    cojvr::runtime::Vec3 forearm_element_forward{};
    cojvr::runtime::Vec3 foretwist_element_position{};
    cojvr::runtime::Vec3 foretwist_element_up{};
    cojvr::runtime::Vec3 foretwist_element_forward{};
    cojvr::runtime::Vec3 hand_element_position{};
    cojvr::runtime::Vec3 hand_element_up{};
    cojvr::runtime::Vec3 hand_element_forward{};
};

struct ArmGeometryRestoreCheck {
    float max_joint_position_error = 0.0F;
    float max_element_position_error = 0.0F;
    float max_axis_error = 0.0F;
    bool matches = false;
};

// Native element transforms use single-precision world coordinates around
// 40,000 game units in the inspected campaign. At that magnitude one float
// ULP is already about 0.0039 game units, so exact/inverse composition cannot
// be judged with the much smaller mutation threshold. This restore-specific
// comparison accepts only sub-millimetre positional drift and a small unit-axis
// round-off while still rejecting a visible residual arm rotation.
[[nodiscard]] ArmGeometryRestoreCheck CheckArmGeometryRestored(
    const ArmGeometrySample& natural,
    const ArmGeometrySample& restored) noexcept;

// A recenter is an explicit recovery boundary for the transient arm overlay.
// Never clear a fail-closed writer latch while a render transaction is active;
// a latched fault additionally requires freshly readable/valid natural arm
// geometry before writes may resume.
[[nodiscard]] bool CanRecoverArmWriterAfterRecenter(
    bool previous_fault,
    bool transaction_active,
    bool natural_geometry_verified) noexcept;

struct PelvisLocomotionAnchor {
    cojvr::runtime::Vec3 actor_position{};
    cojvr::runtime::Vec3 pelvis_offset{};
    cojvr::runtime::Vec3 world_target{};
    bool valid = false;
};

struct LegGeometrySample {
    cojvr::runtime::Vec3 hip{};
    cojvr::runtime::Vec3 knee{};
    cojvr::runtime::Vec3 ankle{};
    cojvr::runtime::Vec3 thigh_up{};
    cojvr::runtime::Vec3 thigh_forward{};
    cojvr::runtime::Vec3 shin_up{};
    cojvr::runtime::Vec3 shin_forward{};
    cojvr::runtime::Vec3 foot_up{};
    cojvr::runtime::Vec3 foot_forward{};
};

struct ElementWorldBasisTarget {
    cojvr::runtime::Vec3 position{};
    cojvr::runtime::Vec3 up{};
    cojvr::runtime::Vec3 forward{};
    bool valid = false;
};

struct ArmIkPlan {
    ElementWorldBasisTarget upper_arm{};
    ElementWorldBasisTarget forearm{};
    cojvr::runtime::Vec3 elbow_target{};
    cojvr::runtime::Vec3 wrist_target{};
    float upper_length = 0.0F;
    float lower_length = 0.0F;
    float raw_target_distance = 0.0F;
    float effective_target_distance = 0.0F;
    float reach_adjustment = 0.0F;
    bool reach_adjusted = false;
    bool target_clamped = false;
    bool valid = false;
};

struct BoneRotationDelta {
    cojvr::runtime::Vec3 axis{};
    float angle_degrees = 0.0F;
    bool no_op = false;
    bool valid = false;
};

// The Sense model/grip axes are runtime/device details and do not line up with
// the shipped hand mesh axes. Capture their relationship from one live frame
// and only apply subsequent controller-orientation deltas. The hand basis is
// stored in the natural camera frame so game yaw can continue to own world
// orientation independently from HMD/controller tracking.
struct HandOrientationReference {
    cojvr::runtime::Quaternion controller_orientation{};
    cojvr::runtime::Vec3 hand_up_camera{};
    cojvr::runtime::Vec3 hand_forward_camera{};
    bool valid = false;
};

struct HandOrientationTarget {
    cojvr::runtime::Vec3 up{};
    cojvr::runtime::Vec3 forward{};
    bool valid = false;
};

struct HandOrientationRotationPlan {
    BoneRotationDelta forearm_twist{};
    BoneRotationDelta hand{};
    bool valid = false;
};

struct ArmBoneRotationPlan {
    BoneRotationDelta upper_arm{};
    BoneRotationDelta forearm{};
    bool valid = false;
};

// Live frame replay proves FORETWIST follows upper arm only, while hand follows
// forearm independently of FORETWIST. EBones ordinals are not parent indices.
struct ArmSkinningPlan {
    HandOrientationTarget foretwist{};
    HandOrientationTarget hand{};
    bool valid = false;
};

[[nodiscard]] ArmSkinningPlan BuildArmSkinningPlan(
    const ArmGeometrySample& natural,
    const ArmBoneRotationPlan& rotations,
    const BoneRotationDelta& world_twist,
    cojvr::runtime::Vec3 post_ik_hand_up,
    cojvr::runtime::Vec3 post_ik_hand_forward) noexcept;

struct LegIkPlan {
    ElementWorldBasisTarget thigh{};
    ElementWorldBasisTarget shin{};
    ElementWorldBasisTarget foot{};
    cojvr::runtime::Vec3 ankle_target{};
    float thigh_length = 0.0F;
    float shin_length = 0.0F;
    bool target_clamped = false;
    bool knee_plane_valid = false;
    bool valid = false;
};

// Builds a world-space arm overlay from the current animated skeleton. The
// native element frames preserve mesh/bind pivots and twist/roll; the solver
// rotates those frames around the measured shoulder/elbow joints.
[[nodiscard]] ArmIkPlan BuildArmIkPlan(
    const ArmGeometrySample& geometry,
    cojvr::runtime::Vec3 controller_target) noexcept;

// Converts the solved world-space arm chain into the relative hierarchy
// rotations consumed by Call of Juarez's render-element writer. The forearm
// delta is computed after applying the upper-arm delta to the natural lower
// segment so parent motion is not applied twice.
[[nodiscard]] ArmBoneRotationPlan BuildArmBoneRotationPlan(
    const ArmGeometrySample& geometry,
    const ArmIkPlan& plan) noexcept;

// RotateElementWithChildren post-multiplies the element world matrix, so the
// Java axis argument is expressed in the element's current local frame. Convert
// the solver's world-space shortest-arc axis using the exact live element basis.
[[nodiscard]] BoneRotationDelta ConvertWorldRotationToElementLocal(
    const BoneRotationDelta& world_rotation,
    cojvr::runtime::Vec3 element_up,
    cojvr::runtime::Vec3 element_forward) noexcept;

// Calibrate the current animated hand against a recentered controller pose
// without assuming any PS VR2 Sense local-axis convention.
[[nodiscard]] HandOrientationReference BuildHandOrientationReference(
    cojvr::runtime::Quaternion controller_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 hand_up_world,
    cojvr::runtime::Vec3 hand_forward_world) noexcept;

// Apply the controller delta relative to the calibration frame and map the
// resulting hand basis through the current natural CoJ camera basis.
[[nodiscard]] HandOrientationTarget BuildTrackedHandOrientationTarget(
    const HandOrientationReference& reference,
    cojvr::runtime::Quaternion controller_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward) noexcept;

// Split hand orientation into a forearm roll/twist around the solved lower-arm
// axis plus a residual hand-element rotation. This keeps controller roll from
// leaving the forearm mesh corkscrewed while still allowing the wrist/hand to
// reach the complete calibrated Sense orientation.
[[nodiscard]] HandOrientationRotationPlan BuildHandOrientationRotationPlan(
    cojvr::runtime::Vec3 lower_arm_axis,
    cojvr::runtime::Vec3 current_hand_up,
    cojvr::runtime::Vec3 current_hand_forward,
    const HandOrientationTarget& target) noexcept;

// Recompute the final hand residual from the hand basis that actually exists
// after the native FORETWIST writer has propagated through the mesh hierarchy.
// The real hierarchy can differ slightly from the ideal world-space twist used
// to choose FORETWIST, so this observed-basis correction is the authority for
// the final hand-element rotation.
[[nodiscard]] BoneRotationDelta BuildHandResidualRotationDelta(
    cojvr::runtime::Vec3 current_hand_up,
    cojvr::runtime::Vec3 current_hand_forward,
    const HandOrientationTarget& target) noexcept;

// Bounds a controller-requested anatomical roll without changing its axis.
// Used for the CoJ forearm/hand overlay where unrestricted controller roll can
// force the native wrist mesh well beyond the range seen in the shipped pose.
[[nodiscard]] BoneRotationDelta LimitRotationMagnitude(
    BoneRotationDelta rotation,
    float max_degrees) noexcept;

// Maps a recentered tracked hand around the native animated head joint. The
// controller and HMD positions are in the same tracking space; subtracting the
// tracked head removes the recenter-space origin before the offset is mapped
// through the exact CoJ camera basis into the skeleton's world space.
[[nodiscard]] cojvr::runtime::Vec3 BuildTrackedHandTarget(
    cojvr::runtime::Vec3 head_world_target,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 tracked_head,
    cojvr::runtime::Vec3 tracked_hand,
    float game_units_per_meter,
    bool& valid) noexcept;

// Maps the neutral tracking-space -Z axis of a controller /pose/tip into the
// exact CoJ camera basis. The returned value is a normalized game-world look
// direction suitable for m_avLookDirDevForHand.
[[nodiscard]] cojvr::runtime::Vec3 BuildTrackedAimDirection(
    cojvr::runtime::Quaternion tracked_orientation,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    bool& valid) noexcept;

// Keeps the pelvis rooted in the native locomotion owner while exposing the
// animated pelvis offset separately. The first lower-body pass is observation
// only; this anchor does not write the skeleton.
[[nodiscard]] PelvisLocomotionAnchor BuildPelvisLocomotionAnchor(
    cojvr::runtime::Vec3 actor_position,
    cojvr::runtime::Vec3 pelvis_joint) noexcept;

// Maps an estimated tracking-space foot anchor around the locomotion-rooted
// native pelvis. Tracking uses +X right/+Y up/-Z forward in metres; the exact
// CoJ camera basis is right/up/+forward in game units.
[[nodiscard]] cojvr::runtime::Vec3 BuildTrackedFootTarget(
    cojvr::runtime::Vec3 pelvis_world_target,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 tracked_pelvis,
    cojvr::runtime::Vec3 tracked_foot,
    float game_units_per_meter,
    bool& valid) noexcept;

// Builds a measured two-bone leg plan from the live animated chain. The knee
// bend plane comes from the current animation and the target is clamped to the
// measured thigh+shin reach. Foot orientation remains the current native basis
// until controller-free foot-placement evidence justifies a stronger policy.
[[nodiscard]] LegIkPlan BuildLegIkPlan(
    const LegGeometrySample& geometry,
    cojvr::runtime::Vec3 foot_target) noexcept;

// Reconciles room-scale HMD translation with game locomotion. The actor absorbs
// horizontal tracking displacement while the current render keeps only the
// portion not already represented by the actor on the previous game frame.
// World positions/offsets use game units; tracking inputs use metres.
[[nodiscard]] PlayerSpaceReconciliation ReconcilePlayerSpace(
    cojvr::runtime::Vec3 current_actor_position,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 relative_head_position,
    cojvr::runtime::Vec3 player_space_position,
    cojvr::runtime::Vec3 previous_world_offset,
    cojvr::runtime::Vec3 previous_tracking_offset,
    bool previous_applied,
    bool recentered,
    float game_units_per_meter) noexcept;

// Exact IDs recovered from the shipped code.pak EBones.class. Keep this data
// game-specific: these ordinals are evidence for Call of Juarez (2006), not a
// reusable Chrome Engine skeleton contract.
[[nodiscard]] SkeletonBinding ExactGameSkeletonBinding() noexcept;

// Maps the neutral XR head pose onto the exact Call of Juarez look/body angle
// convention. Live camera evidence established that CoJ needs the physical HMD
// yaw sign inverted while pitch keeps its tracking-space sign. Shipped
// ArmedPlayerBeing bytecode drives the spine toward 2/3 of horizontal look
// angle and leaves the remaining 1/3 to the neck/head chain.
[[nodiscard]] UpperBodyTrackingOffsets UpperBodyOffsetsFromHeadPose(
    const cojvr::runtime::Pose& relative_head_pose) noexcept;

// Discovery remains engine-specific. The adapter can start without a known
// skeleton and later accept bindings discovered from the live actor.
using SkeletonDiscovery = bool(*)(void* actor, SkeletonBinding& binding) noexcept;

using BoneTransformWriter = void(*)(void* actor, int bone, const cojvr::runtime::Pose& pose) noexcept;
using PlayerCapsuleWriter = void(*)(void* actor, const cojvr::runtime::Pose& player_space) noexcept;

class BodyAdapter final {
public:
    void SetSkeletonState(const BodySkeletonState& state) noexcept;
    void SetSkeletonBinding(const SkeletonBinding& binding) noexcept;
    void SetTransformWriter(BoneTransformWriter writer) noexcept;
    void SetPlayerCapsuleWriter(PlayerCapsuleWriter writer) noexcept;
    void SetSkeletonDiscovery(SkeletonDiscovery discovery) noexcept;
    [[nodiscard]] bool DiscoverSkeleton() noexcept;
    void Apply(const cojvr::runtime::IKBodyPose& pose) noexcept;
    void ApplyPlayerSpace(const cojvr::runtime::Pose& player_space) noexcept;

    [[nodiscard]] const SkeletonBinding& Binding() const noexcept { return binding_; }

private:
    BodySkeletonState state_{};
    SkeletonBinding binding_ = ExactGameSkeletonBinding();
    BoneTransformWriter writer_ = nullptr;
    PlayerCapsuleWriter player_capsule_writer_ = nullptr;
    SkeletonDiscovery discovery_ = nullptr;
};

} // namespace cojvr::games::call_of_juarez
