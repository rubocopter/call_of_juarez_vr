#pragma once

#include "runtime/body_ik.hpp"

namespace cojvr::games::call_of_juarez {

// CONFLICT NOTE: BodyAdapter::Apply() and the transient render-element overlay in
// camera_probe.cpp both write the same visible arm chain. Only one may own those
// elements at a time. The current CoJ overlay captures the animated element
// frames, computes complete absolute world-space frames, writes them through
// FromUpForwardPosElementWorld, and restores the captured frames after stereo
// capture. BodyAdapter::Apply() is kept for potential future integrations with a
// different native skeleton boundary.

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

struct BodyYawOwnershipUpdate {
    float actor_target_degrees = 0.0F;
    float actor_delta_degrees = 0.0F;
    float camera_compensation_degrees = 0.0F;
    float head_residual_degrees = 0.0F;
    float spine_residual_degrees = 0.0F;
    bool valid = false;
};

struct PlayerSpaceReconciliation {
    cojvr::runtime::Vec3 desired_actor_position{};
    cojvr::runtime::Vec3 applied_world_offset{};
    cojvr::runtime::Vec3 applied_tracking_offset{};
    cojvr::runtime::Vec3 render_head_position{};
    bool valid = false;
};

struct RoomScaleTranslationUpdate {
    cojvr::runtime::Vec3 render_head_position{};
    bool write_actor_position = false;
    bool valid = false;
};

struct TrackedAimPoseDiagnostics {
    cojvr::runtime::Vec3 grip_to_tip_direction{};
    float grip_to_tip_distance_m = 0.0F;
    float dot_positive_x = 0.0F;
    float dot_negative_x = 0.0F;
    float dot_positive_y = 0.0F;
    float dot_negative_y = 0.0F;
    float dot_positive_z = 0.0F;
    float dot_negative_z = 0.0F;
    bool valid = false;
};

// Render-only horizontal skeleton translation; actor/collision position and
// physical height remain owned by their existing native/camera paths.
[[nodiscard]] cojvr::runtime::Vec3 BuildCoJVisualBodyOffset(
    cojvr::runtime::Vec3 tracked_head,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_forward,
    float game_units_per_meter,
    bool& valid) noexcept;

// Physical room-scale crouch is detected from calibrated HMD height and merged
// with the explicit controller action so either source drives the game's native
// crouch pose/animation. Hysteresis keeps height noise from toggling the action.
class CoJPhysicalCrouchState final {
public:
    [[nodiscard]] bool Update(
        bool tracking_active,
        bool recentered,
        float relative_head_y_m) noexcept;
    void Reset() noexcept { crouched_ = false; }
    [[nodiscard]] bool crouched() const noexcept { return crouched_; }

private:
    bool crouched_ = false;
};

// Native crouch lowers the camera as well as the avatar. Physical HMD height
// already owns that descent, so compensate only the native camera drop while
// retaining actor vertical movement and explicit controller crouch.
class CoJPhysicalViewHeightState final {
public:
    [[nodiscard]] float Update(
        float native_camera_y, float actor_y, bool physical_crouch,
        bool controller_crouch, bool reset) noexcept;
    void Reset() noexcept { *this = {}; }
private:
    float reference_height_ = 0;
    bool reference_valid_ = false;
    bool compensating_ = false;
    bool controller_recovery_ = false;
};

[[nodiscard]] bool ResolveCoJCrouchAction(
    bool requested_native_crouch,
    bool physical_crouch_detected) noexcept;

// Rotate the neutral runtime stick into the actor-local movement frame using
// the current HMD yaw left over after body-yaw ownership. Positive head yaw is
// a physical turn to the player's right; shaping/deadzone remains owned by the
// exact-game InputAnalog path after this rotation.
[[nodiscard]] cojvr::runtime::Vec2 RotateCoJMoveForHeadRelativeYaw(
    cojvr::runtime::Vec2 move,
    float head_yaw_degrees) noexcept;

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

// Fit the render-only arm chain to a measured, stable bilateral T pose.
// Native bone ratios are retained; ordinary aiming/overreach never grows arms.
class CoJArmSpanCalibration final {
public:
    [[nodiscard]] float Update(cojvr::runtime::Vec3 head,
        cojvr::runtime::Vec3 left, cojvr::runtime::Vec3 right,
        float native_shoulder_span_cm, float native_arm_sum_cm, bool valid) noexcept;
    void Reset() noexcept { *this = {}; }
    [[nodiscard]] bool calibrated() const noexcept { return calibrated_; }
    [[nodiscard]] unsigned stable_samples() const noexcept { return stable_samples_; }
    [[nodiscard]] const char* observation() const noexcept { return observation_; }
private:
    float scale_ = 1;
    float candidate_span_cm_ = 0;
    unsigned stable_samples_ = 0;
    bool calibrated_ = false;
    const char* observation_ = "awaiting_pose";
};

struct ArmGeometryRestoreCheck {
    float max_joint_position_error = 0.0F;
    float max_element_position_error = 0.0F;
    float max_axis_error = 0.0F;
    bool matches = false;
};

// Absolute element writes are restored after stereo rendering. Native
// locomotion may move the player root during that transaction, so captured
// natural world positions must follow the actor translation before they are
// written back. Orientation bases remain those of the captured natural pose.
[[nodiscard]] ArmGeometrySample RebaseArmGeometryForActorTranslation(
    const ArmGeometrySample& natural,
    cojvr::runtime::Vec3 captured_actor_position,
    cojvr::runtime::Vec3 current_actor_position) noexcept;

struct ArmGeometryContinuityUpdate {
    ArmGeometrySample geometry{};
    bool valid = false;
    bool rebased_world_fixed = false;
};

// The exact game can occasionally leave the complete arm positional branch
// fixed in world space across adjacent frames while the actor root advances.
// Detect only that strong continuity break and translate the sampled natural
// geometry with the actor. Any native arm animation keeps ownership.
[[nodiscard]] ArmGeometryContinuityUpdate StabilizeArmGeometryAcrossActorMotion(
    const ArmGeometrySample& current,
    cojvr::runtime::Vec3 current_actor_position,
    const ArmGeometrySample& previous,
    cojvr::runtime::Vec3 previous_actor_position,
    bool previous_valid) noexcept;

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

struct TrackedWeaponFramePlan {
    ElementWorldBasisTarget root{};
    cojvr::runtime::Vec3 muzzle_origin{};
    cojvr::runtime::Vec3 muzzle_direction{};
    bool valid = false;
};

// Rigid mapping from the measured native barrel frame to the tracked tip.
// The native wrist-to-muzzle offset is retained; the tip is an axis, not a muzzle.
[[nodiscard]] TrackedWeaponFramePlan BuildTrackedWeaponFrame(
    const ElementWorldBasisTarget& natural_root,
    cojvr::runtime::Vec3 natural_wrist,
    cojvr::runtime::Vec3 natural_muzzle,
    cojvr::runtime::Vec3 natural_barrel_direction,
    cojvr::runtime::Vec3 tracked_grip,
    cojvr::runtime::Vec3 tracked_direction,
    cojvr::runtime::Vec3 tracked_up) noexcept;

[[nodiscard]] ElementWorldBasisTarget TransformWeaponElementFrame(
    const ElementWorldBasisTarget& source_root,
    const ElementWorldBasisTarget& target_root,
    const ElementWorldBasisTarget& element) noexcept;

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
    cojvr::runtime::Vec3 controller_target,
    cojvr::runtime::Vec3 elbow_pole = {},
    float measured_reach_scale = 1.0F) noexcept;

// Keep ownership of every valid positional IK plan, including unreachable
// controller targets already hard-clamped by SolveTwoBoneIK to the measured
// native chain length. Rotation and hand-residual safety gates remain separate.
[[nodiscard]] bool ShouldApplyCoJArmIk(const ArmIkPlan& plan) noexcept;

// Converts the solved world-space arm chain into the relative hierarchy
// rotations consumed by Call of Juarez's render-element writer. The forearm
// delta is computed after applying the upper-arm delta to the natural lower
// segment so parent motion is not applied twice.
[[nodiscard]] ArmBoneRotationPlan BuildArmBoneRotationPlan(
    const ArmGeometrySample& geometry,
    const ArmIkPlan& plan) noexcept;

// A controller target can be positionally reachable while still requiring an
// anatomically absurd takeover from the current shipped animation.  Bound the
// single-frame upper/forearm overlay independently from reach and hand-roll
// safety so the native pose wins when the solve would flip a joint.
[[nodiscard]] bool ShouldApplyCoJArmRotationPlan(
    const ArmBoneRotationPlan& plan) noexcept;

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

// Compute the final hand residual from the hand basis after the preceding arm
// transforms. The caller owns that basis source: the absolute-frame Body IK
// planner uses its precomputed skinning basis, while diagnostics may use an
// observed basis when validating a live hierarchy.
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
[[nodiscard]] float CoJArmControllerTwistLimitDegrees() noexcept;
[[nodiscard]] float CoJHandControllerResidualLimitDegrees() noexcept;
[[nodiscard]] BoneRotationDelta BuildSafeCoJHandResidual(
    BoneRotationDelta residual) noexcept;

// Builds the complete transient arm overlay as absolute world-space element
// frames. This is the non-hierarchical write contract used by the CoJ adapter:
// every affected element receives its final frame explicitly, so applying or
// restoring Body IK does not require RotateElementWithChildren propagation.
struct ArmElementFramePlan {
    ElementWorldBasisTarget upper_arm{};
    ElementWorldBasisTarget forearm{};
    ElementWorldBasisTarget foretwist{};
    ElementWorldBasisTarget hand{};
    BoneRotationDelta forearm_twist{};
    BoneRotationDelta diagnostic_hand_residual{};
    BoneRotationDelta hand_residual{};
    float requested_forearm_twist_degrees = 0.0F;
    bool forearm_twist_limited = false;
    bool valid = false;
};

[[nodiscard]] ArmElementFramePlan BuildArmElementFramePlan(
    const ArmGeometrySample& natural,
    const ArmIkPlan& ik,
    const ArmBoneRotationPlan& rotations,
    const HandOrientationTarget& hand_target) noexcept;

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

// The render pelvis already owns horizontal HMD displacement. Physical height
// remains in the camera, so retain the controller's vertical descent as well
// as the native crouch animation when anchoring the arm to the live head joint.
[[nodiscard]] cojvr::runtime::Vec3 BuildTrackedArmTarget(
    cojvr::runtime::Vec3 head_world_target,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 tracked_head,
    cojvr::runtime::Vec3 tracked_hand,
    float game_units_per_meter,
    bool& valid) noexcept;

// The native camera has no physical tracking offset yet. Map the full tip
// position around it; subtracting HMD translation here would apply room-scale
// movement twice. Physical crouch compensation is in game units.
[[nodiscard]] cojvr::runtime::Vec3 BuildTrackedAimOrigin(
    cojvr::runtime::Vec3 native_camera_position,
    cojvr::runtime::Vec3 camera_right,
    cojvr::runtime::Vec3 camera_up,
    cojvr::runtime::Vec3 camera_forward,
    cojvr::runtime::Vec3 tracked_head,
    cojvr::runtime::Vec3 tracked_tip,
    float physical_view_correction,
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

// Compares the runtime's /pose/tip orientation axes against the physical
// displacement from /pose/handgrip to /pose/tip. This is diagnostic only: it
// does not choose or modify the gameplay aim axis.
[[nodiscard]] TrackedAimPoseDiagnostics BuildTrackedAimPoseDiagnostics(
    cojvr::runtime::Quaternion tip_orientation,
    cojvr::runtime::Vec3 grip_position,
    cojvr::runtime::Vec3 tip_position) noexcept;

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

// The actor absorbs physical HMD yaw after the current stereo frame. The
// current frame therefore keeps only the yaw not already represented by the
// actor, while the camera compensates the previously committed actor yaw.
// Recenter promotes the actor's current orientation to the new baseline.
[[nodiscard]] BodyYawOwnershipUpdate BuildBodyYawOwnershipUpdate(
    const cojvr::runtime::Pose& relative_head_pose,
    float previously_owned_degrees,
    bool previous_owned_valid,
    bool recentered) noexcept;

// Room-scale HMD translation is rendered by the camera. Directly writing the
// Java actor position bypasses native collision/grounding and, in physical
// evidence, fought locomotion with millimetre-scale teleports each frame.
[[nodiscard]] RoomScaleTranslationUpdate BuildCollisionSafeRoomScaleTranslation(
    cojvr::runtime::Vec3 relative_head_position) noexcept;

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
