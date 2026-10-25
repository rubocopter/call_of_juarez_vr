#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cojvr::runtime {

enum class PromptAction : std::uint8_t {
    left_fire, right_fire, reload, interact, kick, jump, alternate_fire,
    weapon_radial, objectives, focus, crouch, pause, recenter, turn, count,
};

struct ActionPromptSnapshot;

// Optional art qualified by the input owner, never by translated action text.
// Only the locally inspected Sense profile/components are admitted. These are
// owned values, not tracked-device pointers, textures or controller support claims.
enum class PromptGlyphKind : std::uint8_t { none, cross, circle, square, triangle, trigger, grip };
enum class PromptGlyphHand : std::uint8_t { unknown, left, right };
struct PromptGlyph {
    PromptGlyphKind kind=PromptGlyphKind::none;
    PromptGlyphHand hand=PromptGlyphHand::unknown;
    std::uint64_t origin=0;
    bool operator==(const PromptGlyph&) const = default;
};
inline constexpr std::uint64_t kPromptGlyphLifetimeMs=1500;

// Strict source tuple from GetOriginTrackedDeviceInfo + GetActionBindingInfo
// + Prop_ControllerType_String. Caller requires exactly one binding record and
// one distinct connected origin, and verifies the binding's device path handle.
[[nodiscard]] inline PromptGlyph QualifyPromptGlyph(std::string_view controller,
    std::string_view device, std::string_view component, std::string_view input,
    std::string_view mode, std::string_view slot, std::string_view source_type,
    std::uint64_t origin) noexcept {
    if(controller!="playstation_vr2_sense"||!origin||slot!="click")return {};
    const bool left=device=="/user/hand/left",right=device=="/user/hand/right";
    if(!left&&!right)return {};
    PromptGlyphKind kind=PromptGlyphKind::none;
    if(mode=="button"&&source_type=="button"){
        if(component=="button_a"&&input==(left?"/input/square":"/input/cross"))
            kind=left?PromptGlyphKind::square:PromptGlyphKind::cross;
        if(component=="button_b"&&input==(left?"/input/triangle":"/input/circle"))
            kind=left?PromptGlyphKind::triangle:PromptGlyphKind::circle;
    }else if(mode=="trigger"&&source_type=="trigger"){
        if(component=="trigger"&&input==(left?"/input/l2":"/input/r2"))kind=PromptGlyphKind::trigger;
        if(component=="button_grip"&&input==(left?"/input/l1":"/input/r1"))kind=PromptGlyphKind::grip;
    }
    if(kind==PromptGlyphKind::none)return {};
    return {kind,left?PromptGlyphHand::left:PromptGlyphHand::right,origin};
}

// Owned UTF-16 storage, including a reserved terminator: at most 95 code units.
// Only SetActionPromptLabel authors text, keeping every copied label well formed.
class PromptLabel final {
public:
    static constexpr std::size_t capacity = 96;
    [[nodiscard]] std::u16string_view view() const noexcept {
        return {characters_.data(), length_};
    }

private:
    std::array<char16_t, capacity> characters_{};
    std::uint32_t length_ = 0;
    friend bool SetActionPromptLabel(ActionPromptSnapshot&, PromptAction,
        std::string_view) noexcept;
};

// A single runtime owner supplies a complete fresh snapshot on rebinding.
// Generation is opaque metadata, not a device identity or a monotonic clock.
// Nonzero generation with inactive metadata is an invalidated runtime source:
// retain the epoch and semantic fallback, rather than assuming a legacy device.
// No independent label/origin cache survives complete snapshot replacement.
struct ActionPromptSnapshot {
    std::array<PromptLabel, static_cast<std::size_t>(PromptAction::count)> labels{};
    std::uint64_t binding_generation = 0;
    bool active = false;
    std::array<PromptGlyph, static_cast<std::size_t>(PromptAction::count)> glyphs{};
    std::uint64_t glyph_sample_ms=0,glyph_deadline_ms=0;
};

// Current epoch/clock checks also apply to retained captures and repeated frames.
// Expired or unsupported art returns none; ResolveActionPrompt remains the text
// fallback. Consumers must never replace native text with optional artwork.
[[nodiscard]] inline PromptGlyph ResolveActionPromptGlyph(const ActionPromptSnapshot& snapshot,
    PromptAction action,std::uint64_t current_generation,std::uint64_t now) noexcept {
    const auto index=static_cast<std::size_t>(action);
    if(!snapshot.active||!current_generation||snapshot.binding_generation!=current_generation||
        index>=snapshot.glyphs.size()||now<snapshot.glyph_sample_ms||
        now>=snapshot.glyph_deadline_ms||snapshot.glyph_deadline_ms<snapshot.glyph_sample_ms||
        snapshot.glyph_deadline_ms-snapshot.glyph_sample_ms>kPromptGlyphLifetimeMs)return {};
    const auto glyph=snapshot.glyphs[index];
    if(!glyph.origin||glyph.kind<PromptGlyphKind::cross||glyph.kind>PromptGlyphKind::grip||
        (glyph.hand!=PromptGlyphHand::left&&glyph.hand!=PromptGlyphHand::right))return {};
    return glyph;
}

// Reject empty, malformed, control-containing or oversized labels in full and
// clear the action's previous label. Return true only for a usable whole label.
// Does not activate the source or change its generation. Invalid actions fail.
[[nodiscard]] bool SetActionPromptLabel(ActionPromptSnapshot& snapshot,
    PromptAction action, std::string_view utf8) noexcept;

// Inactive sources and absent labels use semantic EN/ES action names, without
// guessing device/button names. Invalid actions return an empty view. A reported
// label's view borrows the supplied snapshot, which must outlive its use.
[[nodiscard]] std::u16string_view ResolveActionPrompt(const ActionPromptSnapshot& snapshot,
    PromptAction action, bool spanish = false) noexcept;

// Thread-safe complete value copies; consumers retain their own snapshot.
// Publication replaces all labels and metadata, including an inactive source.
void PublishActionPrompts(const ActionPromptSnapshot& snapshot) noexcept;
[[nodiscard]] ActionPromptSnapshot CurrentActionPrompts() noexcept;
void ClearActionPrompts() noexcept;

} // namespace cojvr::runtime
