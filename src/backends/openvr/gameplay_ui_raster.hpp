#pragma once
#include "backends/openvr/hud_text_overlay.hpp"
#include "runtime/gameplay_ui.hpp"
#include "runtime/action_prompts.hpp"
#include <string>

namespace cojvr::backends::openvr {
// Renderer-owned cached pixels. No native objects, device or tracking queries.
class GameplayUiRaster final {
public:
    // Optional captured control row. Caller supplies explicit action semantics,
    // current binding epoch and monotonic time. Native hint/interaction priority
    // suppresses the whole row. Caller must retire composed pixels on expiry or
    // epoch change even on repeated frames, using the existing backing protocol.
    [[nodiscard]] const HudTextPanel& ControlHint(const runtime::ActionPromptSnapshot&,
        runtime::PromptAction,std::uint64_t current_generation,std::uint64_t now_ms,
        bool native_text_visible,bool spanish=false) noexcept;
    [[nodiscard]] std::uint64_t control_hint_revision() const noexcept { return control_hint_revision_; }
    // Production coaching keeps the full existing text pixels. Optional art is
    // an 80px margin; on failure/expiry/priority return the original panel.
    [[nodiscard]] const HudTextPanel& DecorateControlHint(const HudTextPanel& instruction,
        std::uint64_t instruction_revision,const runtime::ActionPromptSnapshot&,
        runtime::PromptAction,std::uint64_t current_generation,std::uint64_t now_ms,
        bool admitted) noexcept;
    [[nodiscard]] std::uint64_t control_decoration_revision() const noexcept { return control_decoration_revision_; }
    [[nodiscard]] const HudTextPanel& InteractionGaze() noexcept;
    [[nodiscard]] const HudTextPanel& Threat(bool damage) noexcept;
    [[nodiscard]] const HudTextPanel& Cartridge() noexcept;
    [[nodiscard]] const HudTextPanel& TexturedCartridge() noexcept;
    [[nodiscard]] const HudTextPanel& Wheel(const runtime::EquipmentWheelSnapshot&) noexcept;
    [[nodiscard]] const HudTextPanel& Compass(const runtime::WristCompassSnapshot&,
        std::uint64_t monotonic_ms = 0) noexcept;
    [[nodiscard]] const HudTextPanel& Status(const runtime::WristStatusSnapshot&) noexcept;
    [[nodiscard]] std::uint64_t wheel_revision() const noexcept { return wheel_revision_; }
    [[nodiscard]] std::uint64_t compass_revision() const noexcept { return compass_revision_; }
    [[nodiscard]] std::uint64_t status_revision() const noexcept { return status_revision_; }
private:
    HudTextPanel control_hint_pixels_{};
    std::u16string control_hint_label_{};
    runtime::PromptGlyph control_hint_glyph_{};
    std::uint64_t control_hint_revision_=0;
    HudTextPanel control_decoration_pixels_{};
    runtime::PromptGlyph control_decoration_glyph_{};
    std::uint64_t control_decoration_source_revision_=0,control_decoration_revision_=0;
    HudTextPanel interaction_gaze_pixels_{};
    std::array<HudTextPanel,2> threat_pixels_{};
    HudTextPanel cartridge_pixels_{};
    HudTextPanel textured_cartridge_pixels_{};
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
