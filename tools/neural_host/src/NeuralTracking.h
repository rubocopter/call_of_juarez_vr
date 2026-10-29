#pragma once
#include <cstdint>

// Companion-local view of the versioned Framework stereo IPC. This is not a
// second transport protocol and contains no game/controller input ownership.
namespace Transport {
struct Quaternion { float x, y, z, w; };
struct Vector { float x, y, z; };
struct Pose { Quaternion orientation; Vector position; };
struct Eye { Pose pose; float left, right, up, down; };
struct Tracking {
    std::uint64_t id{}, tick{};
    Pose head{};
    Eye eyes[2]{};
    std::uint32_t valid{};
};
}
