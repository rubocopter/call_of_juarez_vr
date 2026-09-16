#pragma once

#include "backends/d3d9/frame_mailbox.hpp"
#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace cojvr::backends::openvr {

using PresenterLogCallback = void (*)(void* context, std::string_view line) noexcept;

struct OpenVrTrackingSample {
    runtime::Pose pose{};
    std::uint64_t sequence = 0;
    bool recenter_requested = false;
};

struct OpenVrPresenterStats {
    std::uint64_t pose_updates = 0;
    std::uint64_t frames_uploaded = 0;
    std::uint64_t new_frame_submissions = 0;
    std::uint64_t repeated_frame_submissions = 0;
    std::uint64_t rejected_frames = 0;
    std::uint64_t submit_failures = 0;
    d3d9::FrameMailboxStats mailbox{};
};

// Owns OpenVR and the D3D11 immediate context on one dedicated thread. The
// game/render thread only publishes owned CPU frames and reads the latest pose.
class OpenVrStereoPresenter final {
public:
    OpenVrStereoPresenter();
    ~OpenVrStereoPresenter();

    OpenVrStereoPresenter(const OpenVrStereoPresenter&) = delete;
    OpenVrStereoPresenter& operator=(const OpenVrStereoPresenter&) = delete;

    [[nodiscard]] bool Start(
        std::string action_manifest_path,
        PresenterLogCallback log_callback = nullptr,
        void* log_context = nullptr) noexcept;
    void Stop() noexcept;

    [[nodiscard]] bool Publish(d3d9::StereoCpuFrame frame) noexcept;
    [[nodiscard]] bool LatestTracking(OpenVrTrackingSample& sample) noexcept;
    [[nodiscard]] bool EyeViews(std::array<runtime::EyeView, 2>& eyes) const noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool input_ready() const noexcept;
    [[nodiscard]] OpenVrPresenterStats stats() const noexcept;
    [[nodiscard]] std::string last_error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openvr
