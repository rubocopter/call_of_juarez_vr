#pragma once

#include "runtime/vr_types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cojvr::runtime {

struct OpenVrSystemInfo {
    std::uint32_t recommended_width = 0;
    std::uint32_t recommended_height = 0;
    std::int32_t dxgi_adapter_index = -1;
};

class OpenVrRuntime final {
public:
    OpenVrRuntime();
    ~OpenVrRuntime();

    OpenVrRuntime(const OpenVrRuntime&) = delete;
    OpenVrRuntime& operator=(const OpenVrRuntime&) = delete;
    OpenVrRuntime(OpenVrRuntime&&) noexcept;
    OpenVrRuntime& operator=(OpenVrRuntime&&) noexcept;

    [[nodiscard]] bool Initialize(
        std::string_view application_name = "Call of Juarez VR") noexcept;
    void Shutdown() noexcept;
    [[nodiscard]] bool ReadEyeConfiguration(std::array<EyeView, 2>& eyes) noexcept;
    [[nodiscard]] bool WaitForHmdPose(Pose& pose) noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] const OpenVrSystemInfo& system_info() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::int32_t last_result_code() const noexcept;

    // Native handles are exposed only to renderer adapters that must bridge
    // runtime-owned tracking/session state to API-specific texture submission.
    [[nodiscard]] void* native_system() const noexcept;
    [[nodiscard]] void* native_compositor() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::runtime
