#pragma once
#include <cstdint>

namespace cojvr::runtime {
enum class UiPointerHand { none, left, right };

// Tracking alone never owns desktop input. Acquire on a deliberate button edge,
// keep that hand until release, and require rearming after focus/input/pose loss.
class UiPointerOwnership final {
public:
    UiPointerHand Update(bool available, bool input_valid, bool left_held,
                         bool right_held, bool left_valid, bool right_valid,
                         bool left_input_active = true, bool right_input_active = true) noexcept {
        if (!input_valid) { Suspend(); return owner_; }
        const bool left_pressed = left_input_active && left_held && !left_was_held_;
        const bool right_pressed = right_input_active && right_held && !right_was_held_;
        left_was_held_ = !left_input_active || left_held;
        right_was_held_ = !right_input_active || right_held;
        if (!available) { owner_ = UiPointerHand::none; return owner_; }
        if ((owner_ == UiPointerHand::left && (!left_held || !left_valid || !left_input_active)) ||
            (owner_ == UiPointerHand::right && (!right_held || !right_valid || !right_input_active)))
            owner_ = UiPointerHand::none;
        if (owner_ == UiPointerHand::none) {
            if (right_pressed && right_valid) { owner_ = UiPointerHand::right; ++claim_; }
            else if (left_pressed && left_valid) { owner_ = UiPointerHand::left; ++claim_; }
        }
        return owner_;
    }
    std::uint64_t claim() const noexcept { return claim_; }
    void Suspend() noexcept {
        owner_ = UiPointerHand::none;
        left_was_held_ = right_was_held_ = true;
    }
private:
    UiPointerHand owner_ = UiPointerHand::none;
    bool left_was_held_ = false;
    bool right_was_held_ = false;
    std::uint64_t claim_ = 0;
};

inline bool UiPointerSelect(UiPointerHand owner, bool left, bool right) noexcept {
    return owner == UiPointerHand::left ? left : owner == UiPointerHand::right && right;
}

struct UiPointerSelectionSnapshot {
    UiPointerHand hand = UiPointerHand::none;
    std::uint64_t claim = 0;
    bool held = false;
    bool pending = false;
    std::uint64_t click = 0;
};
// Caller serializes this mailbox. A stale dispatch completion must not consume
// a new claim's click, even when both claims use the same hand.
class UiPointerSelection final {
public:
    void Observe(UiPointerHand hand, std::uint64_t claim, bool held, bool pressed) noexcept {
        if (hand == UiPointerHand::none) { state_ = {}; return; }
        if (hand != state_.hand || claim != state_.claim) state_ = {hand, claim};
        state_.held = held;
        if (pressed) state_.click = ++next_click_;
        state_.pending = held && (state_.pending || pressed);
    }
    UiPointerSelectionSnapshot snapshot() const noexcept { return state_; }
    void Complete(const UiPointerSelectionSnapshot& expected, bool success) noexcept {
        if (success && expected.hand == state_.hand && expected.claim == state_.claim &&
            expected.click == state_.click) state_.pending = false;
    }
private:
    UiPointerSelectionSnapshot state_{};
    std::uint64_t next_click_ = 0;
};

// The same shoulder buttons are weapon selectors in gameplay. A menu-held
// button must be released before that gameplay mapping regains ownership.
class UiPointerGameplayGate final {
public:
    void Observe(bool menu, bool left_active, bool left_held,
                 bool right_active, bool right_held) noexcept {
        if (menu && left_active && left_held) left_blocked_ = true;
        if (menu && right_active && right_held) right_blocked_ = true;
        if (left_active && !left_held) left_blocked_ = false;
        if (right_active && !right_held) right_blocked_ = false;
    }
    bool left_blocked() const noexcept { return left_blocked_; }
    bool right_blocked() const noexcept { return right_blocked_; }
private:
    bool left_blocked_ = false;
    bool right_blocked_ = false;
};
}
