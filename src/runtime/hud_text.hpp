#pragma once

#include "runtime/vr_types.hpp"
#include "runtime/gameplay_ui.hpp"
#include "runtime/action_prompts.hpp"
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
    std::uint64_t controller_prompt_generation = 0; // Copied binding labels; 0 means no runtime-specific text.
};

// Native presentation observations only; no damage, deadline or fade clock.
struct ThreatMarker {
    Vec2 direction{}; // +X right, +Y up on the view's bearing ring.
    float alpha = 0;
    bool damage = false;
    bool operator==(const ThreatMarker& other) const noexcept {
        return direction.x==other.direction.x && direction.y==other.direction.y &&
            alpha==other.alpha && damage==other.damage;
    }
};
struct ThreatMarkers {
    std::array<ThreatMarker,12> markers{};
    std::uint32_t count = 0;
    bool operator==(const ThreatMarkers&) const = default;
};

// Bounded, value-only captured geometry; no scene depth is transported.
struct ReloadCartridgeSurface {
    std::array<Vec3,4> head_corners{};
    // Sample an opaque material patch in the immutable brass/copper raster.
    Vec2 material_uv{};
    float shade = 0.F;
    std::array<Vec2,4> corner_uv{};
    std::array<float,4> corner_shade{};
    std::array<Vec3,4> corner_normal{};
};

// One capture's native-confirmed insertion or qualified zone-entry event.
// Zone entry grants no ammunition. Neither event has a retry queue.
struct ReloadInsertionFeedback {
    bool accepted = false;
    std::uint8_t hand = 2; // Native convention: 0 right, 1 left, 2 absent.
    std::uint64_t event_sequence = 0;
    std::uint64_t input_generation = 0;
    bool diagnostics = false;
    std::uint64_t owner_token = 0; // Opaque game-owned epoch; 0 is unobserved.
    bool zone_entered = false;
};

enum class BowDrawFeedbackKind : std::uint8_t { none, grab, tension, release };
// Physical string gesture only. Release is a native request, never arrow acknowledgement.
// Opaque ownership is separate from reload; the producer fills the capture sequence.
struct BowDrawFeedback {
    std::uint64_t owner_token = 0;
    std::uint64_t input_generation = 0;
    std::uint64_t event_sequence = 0;
    BowDrawFeedbackKind kind = BowDrawFeedbackKind::none;
    float tension = 0.F; // Normalized physical draw progress, not native animation time.
};

// Optional captured gesture hints. State: 0 available, 1 near hand, 2 unavailable.
// Value-only presentation; these surfaces never grant selection or collision.
enum class GestureGuideKind : std::uint8_t { holster, support_grip, ammunition_pickup };
struct HolsterGuideSurface {
    std::array<Vec3,4> head_corners{};
    std::uint8_t state=0;
    GestureGuideKind kind=GestureGuideKind::holster;
};
struct HolsterGuides {
    std::array<HolsterGuideSurface,6> zones{}; // Four holsters, support contact, ammunition source.
    std::uint32_t count=0;
};

struct StereoHudTextOverlay {
    std::uint64_t frame_sequence = 0;
    // Capture-time game-owner availability/epoch, independent of textures.
    std::uint64_t feedback_context_token = 0;
    ReloadInsertionFeedback reload_feedback{};
    BowDrawFeedback bow_feedback{};
    bool reload_instruction = false; // Low-priority interaction text, bound to reload_feedback.owner_token.
    std::uint64_t reload_instruction_until_ms = 0;
    // Source-owned reload coaching ONLY. Never infer an action from copied
    // native text. Complete owned binding metadata travels with this capture.
    PromptAction control_hint_action = PromptAction::count;
    ActionPromptSnapshot control_hint_prompts{};
    HolsterGuides holster_guides{};
    std::array<EyeView, 2> eyes{};
    HudTextSnapshot text{};
    GameplayUiSnapshot ui{};
    HudText mission_timer{};
    HudText mission_notices{};
    ThreatMarkers threats{};
    bool compass_surface_valid = false;
    std::array<Vec3,4> compass_head_corners{};
    bool status_surface_valid = false;
    std::array<Vec3,4> status_head_corners{};
    // Controller-oriented presentation only: never native ammo/inventory.
    bool reload_cartridge_visible = false;
    bool reload_cartridge_textured = false;
    // Repeated copies of one captured mesh. Keep the per-draw constant buffer
    // below D3D11's 64 KiB limit instead of multiplying the capture surfaces.
    static constexpr std::size_t reload_cartridge_max_instances = 6;
    std::uint32_t reload_cartridge_instance_count = 1;
    std::array<Vec3,reload_cartridge_max_instances> reload_cartridge_instance_offsets{};
    std::array<Vec3,4> reload_cartridge_head_corners{};
    static constexpr std::size_t reload_cartridge_max_surfaces = 224;
    std::uint32_t reload_cartridge_surface_count = 0;
    std::array<ReloadCartridgeSurface,reload_cartridge_max_surfaces> reload_cartridge_surfaces{};
    // Captured HMD-forward feedback only; no selected target or collision claim.
    bool interaction_gaze_visible = false;
    std::array<Vec3,4> interaction_gaze_head_corners{};
};

// Admission independent of renderer pixels. Owner/preference fences are checked
// by the existing reload-instruction compositor path before using this proof.
[[nodiscard]] inline PromptGlyph ResolveReloadInstructionGlyph(const StereoHudTextOverlay& overlay,
    std::uint64_t current_generation,std::uint64_t now) noexcept {
    if(!overlay.reload_instruction||!overlay.reload_instruction_until_ms||
        now>=overlay.reload_instruction_until_ms||!overlay.text.hint.view().empty()||
        overlay.text.interaction.view().empty()||
        overlay.text.controller_prompt_generation!=overlay.control_hint_prompts.binding_generation||
        (overlay.control_hint_action!=PromptAction::left_fire&&
         overlay.control_hint_action!=PromptAction::right_fire&&
         overlay.control_hint_action!=PromptAction::reload))return {};
    return ResolveActionPromptGlyph(overlay.control_hint_prompts,overlay.control_hint_action,current_generation,now);
}

} // namespace cojvr::runtime
