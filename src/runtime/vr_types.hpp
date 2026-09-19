#pragma once

#include <cstdint>

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
    bool active = false;
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
