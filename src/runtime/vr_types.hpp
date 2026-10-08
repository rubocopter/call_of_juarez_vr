#pragma once

#include <cstdint>
#include <array>

namespace cojvr::runtime {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

struct Quaternion {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

// Neutral tracking-space convention: right-handed, +X right, +Y up,
// -Z forward, metres for position, quaternion stored as (x, y, z, w).
// Named transform fields use destination_from_source semantics. Composition is
// therefore destination_from_child = destination_from_parent * parent_from_child.
// A bare Pose is only a rigid transform value; producers/consumers must name the
// spaces at their API boundary rather than infer them from this storage type.
struct Pose {
    Vec3 position{};
    Quaternion orientation{};
    bool orientation_valid = false;
    bool position_valid = false;
};

// Skeletal animation fidelity reported by the runtime. Estimated curls are
// controller-derived poses, not measurements of every finger joint.
enum class FingerTrackingQuality : std::uint8_t { unavailable, estimated, partial, full };

struct FingerTrackingState {
    // Thumb, index, middle, ring, pinky; 0 straight, 1 fully curled.
    std::array<float, 5> curls{};
    FingerTrackingQuality quality = FingerTrackingQuality::unavailable;
    bool available = false;
};

// XR-backend-neutral gameplay intent. Physical controller paths belong to the
// runtime binding profile; numeric game action IDs belong to the game adapter.
// Keeping this semantic state between them lets a presenter zero all held
// inputs on focus/dashboard loss without synthesizing OS-global keyboard or
// mouse events.
struct GameplayInputState {
    Vec2 move{};
    Vec2 turn{};
    bool fire_left = false;
    bool fire_right = false;
    bool jump = false;
    bool reload = false;
    bool run = false;
    bool crouch = false;
    bool interact = false;
    bool weapon_next = false;
    bool weapon_previous = false;
    bool kick = false;
    bool walk = false;
    bool focus = false;
    bool alternate_fire = false;
    bool hands = false;
    bool discard_weapon = false;
    bool objectives = false;
    bool logs = false;
    bool quick_save = false;
    bool quick_load = false;
    bool lean_left = false;
    bool lean_right = false;
    // Logical equipment channels; each game adapter defines their inventory meaning.
    std::array<bool, 6> equipment_select{};
    bool weapon_radial = false;
    // UI highlight is metadata, never a held equipment action.
    int radial_highlight = -1;
    bool radial_confirmed = false;
    // Availability is distinct from an observed release. Mask order is
    // kGameplayDigitalMembers followed by the six equipment channels.
    std::uint32_t digital_available = 0xFFFFFFFFU;
    bool move_available = true;
    bool turn_available = true;
    bool radial_available = true;
    // Availability of the global gesture source behind generated Objectives.
    bool objectives_event_available = true;
    std::uint64_t input_context_generation = 0;
    bool active = false;
};

inline constexpr std::array<bool GameplayInputState::*, 21> kGameplayDigitalMembers{
    &GameplayInputState::fire_left, &GameplayInputState::fire_right,
    &GameplayInputState::jump, &GameplayInputState::reload, &GameplayInputState::run,
    &GameplayInputState::crouch, &GameplayInputState::interact,
    &GameplayInputState::weapon_next, &GameplayInputState::weapon_previous,
    &GameplayInputState::kick, &GameplayInputState::walk, &GameplayInputState::focus,
    &GameplayInputState::alternate_fire, &GameplayInputState::hands,
    &GameplayInputState::discard_weapon, &GameplayInputState::objectives,
    &GameplayInputState::logs, &GameplayInputState::quick_save,
    &GameplayInputState::quick_load, &GameplayInputState::lean_left,
    &GameplayInputState::lean_right,
};

struct EyeFov {
    // Radians from neutral -Z forward. A normal forward-looking eye has
    // left/down < 0 and right/up > 0; asymmetric magnitudes are expected.
    float angle_left = 0.0F;
    float angle_right = 0.0F;
    float angle_up = 0.0F;
    float angle_down = 0.0F;
};

enum class Eye : std::uint8_t { left, right };

// Recommended render-target extent only. This deliberately carries no pose or
// FOV so a runtime cannot accidentally use a size recommendation as EyeView
// optical data.
struct EyeRenderRecommendation {
    Eye eye = Eye::left;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct EyeView {
    Eye eye = Eye::left;
    // Static optical transform from the eye to the HMD/head origin.
    Pose eye_to_head{};
    EyeFov fov{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// A time-located eye pose in the runtime tracking/reference space. This is
// intentionally distinct from EyeView: eye-to-head optics and a located
// tracking-space pose have different ownership and lifetime semantics.
struct LocatedEyeView {
    Eye eye = Eye::left;
    Pose tracking_from_eye{};
    EyeFov fov{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

} // namespace cojvr::runtime
