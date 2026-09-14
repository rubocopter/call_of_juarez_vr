#pragma once

#include <d3d9.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::backends::d3d9 {

enum class OpenVrFlatBridgePhase {
    BeforeReadback,
    AfterReadback,
    BeforeUpload,
    AfterUpload,
    BeforeWaitForHmdPose,
    AfterWaitForHmdPose,
    BeforeSubmitStereo,
    AfterSubmitStereo,
};

using OpenVrFlatBridgePhaseCallback = void (*)(OpenVrFlatBridgePhase phase) noexcept;

// Diagnostic bridge used to prove the real game's classic-D3D9 backbuffer can
// reach the OpenVR compositor without replacing the game's native D3D9 device.
// The same flat image is submitted to both eyes; this is not stereo rendering.
class OpenVrFlatBridge final {
public:
    OpenVrFlatBridge();
    ~OpenVrFlatBridge();

    OpenVrFlatBridge(const OpenVrFlatBridge&) = delete;
    OpenVrFlatBridge& operator=(const OpenVrFlatBridge&) = delete;

    [[nodiscard]] bool CaptureAndSubmit(
        IDirect3DDevice9* device,
        OpenVrFlatBridgePhaseCallback phase_callback = nullptr) noexcept;
    void BeforeD3D9Reset() noexcept;

    [[nodiscard]] std::uint64_t submitted_frames() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::string_view description() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
