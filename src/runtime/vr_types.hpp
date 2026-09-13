#pragma once

#include <cstdint>

namespace cojvr::runtime {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Quaternion {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

struct Pose {
    Vec3 position{};
    Quaternion orientation{};
    bool valid = false;
};

struct EyeFov {
    float angle_left = 0.0F;
    float angle_right = 0.0F;
    float angle_up = 0.0F;
    float angle_down = 0.0F;
};

enum class Eye : std::uint8_t { left, right };

struct EyeView {
    Eye eye = Eye::left;
    Pose pose{};
    EyeFov fov{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

} // namespace cojvr::runtime
