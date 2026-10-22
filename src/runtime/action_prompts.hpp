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
};

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
