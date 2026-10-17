#pragma once

#include <cstdint>
#include <atomic>
#include <limits>

namespace cojvr::backends::openvr {

// Capture authorization is sampled by the producer BEFORE capture, carried to
// Publish, and never replaced with a consumer's current epoch. This independent
// fence complements the producer queue's native owner/reset CaptureEpoch.
class FixedOutputFrameAuthority final {
public:
    [[nodiscard]] std::uint64_t Epoch() const noexcept { return epoch_.load(std::memory_order_acquire); }
    [[nodiscard]] bool Allows(std::uint64_t captured_epoch) const noexcept {
        return captured_epoch && captured_epoch != (std::numeric_limits<std::uint64_t>::max)() && captured_epoch == Epoch();
    }
    // Check immediately around a write/commit/Submit boundary. Invalidation
    // during the call rejects its result and all subsequent old-epoch calls;
    // it cannot retract a GPU command or eye already handed to an external API.
    template<class Operation>
    [[nodiscard]] bool Execute(std::uint64_t captured_epoch, Operation&& operation) const
        noexcept(noexcept(operation())) {
        return Allows(captured_epoch) && operation() && Allows(captured_epoch);
    }
    void Invalidate() noexcept {
        auto current = Epoch();
        while (current != (std::numeric_limits<std::uint64_t>::max)() &&
            !epoch_.compare_exchange_weak(current, current+1, std::memory_order_acq_rel, std::memory_order_acquire)) {}
    }
private:
    std::atomic_uint64_t epoch_{1}; // No reset/rearm API, including after failed Reset.
};

// Startup-only prototype. Disabled preserves the existing presenter allocator.
// This bounds output payload; native/driver/runtime memory admission is separate.
struct FixedOutputConfiguration {
    bool enabled = false;
    std::uint32_t native_width = 0;
    std::uint32_t native_height = 0;
    std::uint32_t desktop_width = 0;
    std::uint32_t desktop_height = 0;
    std::uint32_t native_d3d_format = 21; // D3DFMT_A8R8G8B8; 22 is X8R8G8B8.
    std::uint64_t max_output_bytes = 128ULL * 1024 * 1024;
};

enum class FixedOutputState : std::uint8_t {
    disabled, preparing, theater_ready, ready, failed, stopped,
};

enum class FixedOutputFailure : std::uint8_t {
    none, process_claimed, allocation, invalid_configuration, endpoints,
    texture, view, initialization_query, initialization_poll, initialization_timeout,
};

struct FixedOutputReadiness {
    bool native_content_failed = false;
    FixedOutputState state = FixedOutputState::disabled;
    FixedOutputFailure failure = FixedOutputFailure::none;
    std::uint32_t native_width = 0, native_height = 0;
    std::uint32_t desktop_width = 0, desktop_height = 0;
    std::uint32_t theater_width = 0, theater_height = 0;
    std::uint32_t retained_textures = 0;
    std::uint64_t payload_bytes = 0;
    // First complete natural theater upload; not continuing frame authority.
    bool theater_capture_initialized = false;
};

} // namespace cojvr::backends::openvr
