#pragma once
#include "backends/openvr/hud_text_overlay.hpp"
#include "runtime/gameplay_ui.hpp"

namespace cojvr::backends::openvr {
// Renderer-owned cached pixels. No native objects, device or tracking queries.
class GameplayUiRaster final {
public:
    [[nodiscard]] const HudTextPanel& Wheel(const runtime::EquipmentWheelSnapshot&) noexcept;
    [[nodiscard]] const HudTextPanel& Compass(const runtime::WristCompassSnapshot&,
        std::uint64_t monotonic_ms = 0) noexcept;
    [[nodiscard]] const HudTextPanel& Status(const runtime::WristStatusSnapshot&) noexcept;
    [[nodiscard]] std::uint64_t wheel_revision() const noexcept { return wheel_revision_; }
    [[nodiscard]] std::uint64_t compass_revision() const noexcept { return compass_revision_; }
    [[nodiscard]] std::uint64_t status_revision() const noexcept { return status_revision_; }
private:
    runtime::EquipmentWheelSnapshot wheel_{};
    runtime::WristCompassSnapshot compass_{};
    runtime::WristStatusSnapshot status_{};
    HudTextPanel status_pixels_{};
    std::uint64_t status_revision_=0;
    HudTextPanel wheel_pixels_{}, compass_pixels_{};
    std::uint64_t wheel_revision_ = 0, compass_revision_ = 0;
    std::uint64_t compass_update_ms_ = 0;
};
}
