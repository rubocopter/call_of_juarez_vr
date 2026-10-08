#pragma once

#include "runtime/vr_types.hpp"
#include "runtime/gameplay_ui.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace cojvr::runtime {

// Value-only, bounded UTF-16. Native references never leave the game owner.
struct HudText {
    static constexpr std::size_t capacity = 1024;
    std::array<char16_t, capacity> characters{};
    std::uint32_t length = 0;
    [[nodiscard]] std::u16string_view view() const noexcept {
        return length < capacity ? std::u16string_view(characters.data(), length)
                                 : std::u16string_view{};
    }
    bool operator==(const HudText&) const = default;
};

struct HudTextSnapshot {
    HudText hint{};
    HudText interaction{};
    HudText subtitle{};
};

// Bounded, value-only captured geometry; no scene depth is transported.
struct ReloadCartridgeSurface {
    std::array<Vec3,4> head_corners{};
    // Sample an opaque material patch in the immutable brass/copper raster.
    Vec2 material_uv{};
    float shade = 0.F;
};

struct StereoHudTextOverlay {
    std::uint64_t frame_sequence = 0;
    // Capture-time game-owner availability/epoch, independent of textures.
    std::uint64_t feedback_context_token = 0;
    std::array<EyeView, 2> eyes{};
    HudTextSnapshot text{};
    GameplayUiSnapshot ui{};
    bool compass_surface_valid = false;
    std::array<Vec3,4> compass_head_corners{};
    bool status_surface_valid = false;
    std::array<Vec3,4> status_head_corners{};
    // Controller-oriented presentation only: never native ammo/inventory.
    bool reload_cartridge_visible = false;
    std::array<Vec3,4> reload_cartridge_head_corners{};
    static constexpr std::size_t reload_cartridge_max_surfaces = 16;
    std::uint32_t reload_cartridge_surface_count = 0;
    std::array<ReloadCartridgeSurface,reload_cartridge_max_surfaces> reload_cartridge_surfaces{};
    // Captured HMD-forward feedback only; no selected target or collision claim.
    bool interaction_gaze_visible = false;
    std::array<Vec3,4> interaction_gaze_head_corners{};
};

} // namespace cojvr::runtime
