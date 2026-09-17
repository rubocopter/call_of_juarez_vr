#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace cojvr::runtime {

enum class OpenXrStatus : std::uint8_t {
    idle,
    system_ready,
    session_ready,
    running,
    exit_requested,
    failed,
};

struct OpenXrSystemInfo {
    std::string name;
    std::uint32_t vendor_id = 0;
    std::uint32_t max_swapchain_width = 0;
    std::uint32_t max_swapchain_height = 0;
    bool orientation_tracking = false;
    bool position_tracking = false;
};

struct OpenXrInitOptions {
    std::span<const char* const> required_extensions{};
};

// Renderer backends own the concrete OpenXR graphics binding structure. The
// runtime only threads that opaque structure into xrCreateSession.
struct OpenXrGraphicsBinding {
    const void* native = nullptr;
};

struct OpenXrFrameState {
    std::int64_t predicted_display_time = 0;
    std::int64_t predicted_display_period = 0;
    bool should_render = false;
};

struct OpenXrCompositionLayer {
    const void* native = nullptr;
};

struct OpenXrSwapchainHandle {
    void* native = nullptr;
};

class OpenXrRuntime final {
public:
    OpenXrRuntime();
    ~OpenXrRuntime();

    OpenXrRuntime(const OpenXrRuntime&) = delete;
    OpenXrRuntime& operator=(const OpenXrRuntime&) = delete;
    OpenXrRuntime(OpenXrRuntime&&) noexcept;
    OpenXrRuntime& operator=(OpenXrRuntime&&) noexcept;

    bool Initialize(const OpenXrInitOptions& options = {}) noexcept;
    bool CreateSession(OpenXrGraphicsBinding graphics_binding) noexcept;
    void DestroySession() noexcept;
    bool PollEvents() noexcept;
    bool EnumerateSwapchainFormats(
        std::span<std::int64_t> formats, std::uint32_t& format_count) noexcept;
    bool CreateColorSwapchain(
        std::int64_t format, std::uint32_t width, std::uint32_t height,
        std::uint32_t sample_count, OpenXrSwapchainHandle& swapchain) noexcept;
    bool DestroySwapchain(OpenXrSwapchainHandle& swapchain) noexcept;
    bool EnumerateSwapchainImages(
        OpenXrSwapchainHandle swapchain, std::uint32_t capacity,
        std::uint32_t& image_count, void* images) noexcept;
    bool AcquireSwapchainImage(
        OpenXrSwapchainHandle swapchain, std::uint32_t& image_index) noexcept;
    bool WaitSwapchainImage(OpenXrSwapchainHandle swapchain) noexcept;
    bool ReleaseSwapchainImage(OpenXrSwapchainHandle swapchain) noexcept;
    bool WaitBeginFrame(OpenXrFrameState& frame) noexcept;
    bool LocateStereoViews(
        const OpenXrFrameState& frame, std::array<LocatedEyeView, 2>& views) noexcept;
    bool EndFrame(
        const OpenXrFrameState& frame,
        std::span<const OpenXrCompositionLayer> layers = {}) noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] OpenXrStatus status() const noexcept;
    [[nodiscard]] const OpenXrSystemInfo& system_info() const noexcept;
    [[nodiscard]] const std::array<EyeView, 2>& recommended_views() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;

    // Native handles are exposed only for renderer adapters that must query a
    // graphics-binding extension. Shared game/runtime policy should not use them.
    [[nodiscard]] void* native_instance() const noexcept;
    [[nodiscard]] void* native_session() const noexcept;
    [[nodiscard]] void* native_reference_space() const noexcept;
    [[nodiscard]] std::uint64_t system_id() const noexcept;
    [[nodiscard]] void* GetProcAddress(const char* name) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::runtime
