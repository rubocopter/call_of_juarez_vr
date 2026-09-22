#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace cojvr::backends::d3d9 {

enum class CpuPixelFormat : std::uint8_t {
    bgrx8_unorm,
};

enum class FramePresentationMode : std::uint8_t {
    native_stereo,
    flat_theater,
};

enum class StereoFrameTransport : std::uint8_t {
    cpu_bgrx,
    classic_d3d9_locked_systemmem,
    d3d9ex_shared_texture,
};

struct CpuEyeFrame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    CpuPixelFormat format = CpuPixelFormat::bgrx8_unorm;
    std::vector<std::uint8_t> pixels;
    const std::uint8_t* borrowed_pixels = nullptr;
};

struct SharedTextureEyeFrame {
    std::uintptr_t shared_handle = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t d3d_format = 0;
};

struct ProducerFrameReleaseState {
    std::atomic_bool consumer_done{false};
};

// A producer ring slot remains inaccessible until the consumer destroys this
// lease. The lease contains no graphics API object, so it can cross the
// game/presenter thread boundary safely. The producer performs any required
// D3D9 unlock on its owner thread after observing consumer_done.
class ProducerFrameLease final {
public:
    ProducerFrameLease() = default;
    explicit ProducerFrameLease(
        std::shared_ptr<ProducerFrameReleaseState> state) noexcept
        : state_(std::move(state)) {}
    ~ProducerFrameLease() { Release(); }

    ProducerFrameLease(const ProducerFrameLease&) = delete;
    ProducerFrameLease& operator=(const ProducerFrameLease&) = delete;
    ProducerFrameLease(ProducerFrameLease&& other) noexcept
        : state_(std::move(other.state_)) {}
    ProducerFrameLease& operator=(ProducerFrameLease&& other) noexcept {
        if (this != &other) {
            Release();
            state_ = std::move(other.state_);
        }
        return *this;
    }

    [[nodiscard]] bool valid() const noexcept { return state_ != nullptr; }
    void Release() noexcept {
        if (state_) {
            state_->consumer_done.store(true, std::memory_order_release);
            state_.reset();
        }
    }

private:
    std::shared_ptr<ProducerFrameReleaseState> state_{};
};

struct StereoCpuFrame {
    StereoCpuFrame() = default;
    StereoCpuFrame(const StereoCpuFrame&) = delete;
    StereoCpuFrame& operator=(const StereoCpuFrame&) = delete;
    StereoCpuFrame(StereoCpuFrame&&) noexcept = default;
    StereoCpuFrame& operator=(StereoCpuFrame&&) noexcept = default;

    std::uintptr_t device_id = 0;
    std::uint64_t generation = 0;
    std::uint64_t capture_sequence = 0;
    // Optional owner-assigned ordering sequence. This lets two producer paths
    // (startup flat capture and native stereo) share the same latest-frame
    // mailbox without conflating their independent capture counters.
    std::uint64_t transport_sequence = 0;
    std::uint64_t render_pose_sequence = 0;
    runtime::Pose render_hmd_pose{};
    std::chrono::steady_clock::time_point capture_time{};
    FramePresentationMode presentation_mode = FramePresentationMode::native_stereo;
    StereoFrameTransport transport = StereoFrameTransport::cpu_bgrx;
    std::array<CpuEyeFrame, 2> eyes{};
    std::array<SharedTextureEyeFrame, 2> shared_eyes{};
    ProducerFrameLease producer_lease{};
};

} // namespace cojvr::backends::d3d9
