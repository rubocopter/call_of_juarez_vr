#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace cojvr::backends::d3d9 {

enum class CpuPixelFormat : std::uint8_t {
    bgrx8_unorm,
};

struct CpuEyeFrame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    CpuPixelFormat format = CpuPixelFormat::bgrx8_unorm;
    std::vector<std::uint8_t> pixels;
};

struct StereoCpuFrame {
    std::uintptr_t device_id = 0;
    std::uint64_t generation = 0;
    std::uint64_t capture_sequence = 0;
    std::uint64_t render_pose_sequence = 0;
    runtime::Pose render_hmd_pose{};
    std::chrono::steady_clock::time_point capture_time{};
    std::array<CpuEyeFrame, 2> eyes{};
};

} // namespace cojvr::backends::d3d9
