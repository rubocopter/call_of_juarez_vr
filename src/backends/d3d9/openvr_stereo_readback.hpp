#pragma once

#include "runtime/vr_types.hpp"

#include <d3d9.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::runtime {
class OpenVrRuntime;
}

namespace cojvr::backends::d3d9 {

// First-proof transport for native Chrome Engine stereo. The engine renders each
// eye separately; this class snapshots those two D3D9 results and submits two
// distinct D3D11 textures to OpenVR. CPU readback is intentionally temporary.
class OpenVrStereoReadback final {
public:
    OpenVrStereoReadback();
    ~OpenVrStereoReadback();

    OpenVrStereoReadback(const OpenVrStereoReadback&) = delete;
    OpenVrStereoReadback& operator=(const OpenVrStereoReadback&) = delete;

    [[nodiscard]] bool Initialize(runtime::OpenVrRuntime& runtime) noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] bool CaptureEye(
        IDirect3DDevice9* device,
        runtime::Eye eye,
        std::uint64_t frame_sequence,
        std::uint64_t& content_hash) noexcept;
    [[nodiscard]] bool Submit(
        runtime::OpenVrRuntime& runtime,
        std::uint64_t frame_sequence) noexcept;

    [[nodiscard]] std::uint64_t submitted_frames() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::string_view description() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::d3d9
