#pragma once
#include "runtime/vr_types.hpp"
#include <cmath>
#include <cstdint>

namespace cojvr::runtime {
enum class UiPointerHand { none, left, right };

// Automatic laser, preferring the right tracked hand. A fresh trigger press
// chooses its own hand; tracking motion alone never switches between valid hands.
// Desktop coexistence is handled at the logical game-cursor delivery boundary.
class UiPointerOwnership final {
public:
    UiPointerHand Update(bool available, bool input_valid, bool left_pressed,
                         bool right_pressed, bool left_valid, bool right_valid,
                         bool left_input_active = true, bool right_input_active = true) noexcept {
        if (!available || !input_valid) { Suspend(); return owner_; }
        left_valid = left_valid && left_input_active;
        right_valid = right_valid && right_input_active;
        auto next = owner_;
        if (right_pressed && right_valid) next = UiPointerHand::right;
        else if (left_pressed && left_valid) next = UiPointerHand::left;
        else if (next == UiPointerHand::none ||
                 (next == UiPointerHand::right && !right_valid) ||
                 (next == UiPointerHand::left && !left_valid))
            next = right_valid ? UiPointerHand::right :
                   left_valid ? UiPointerHand::left : UiPointerHand::none;
        if (next != owner_ && next != UiPointerHand::none) ++claim_;
        owner_ = next;
        return owner_;
    }
    std::uint64_t claim() const noexcept { return claim_; }
    void Suspend() noexcept {
        owner_ = UiPointerHand::none;
    }
private:
    UiPointerHand owner_ = UiPointerHand::none;
    std::uint64_t claim_ = 0;
};

// Compare the game's cursor before applying VR with the last accepted target.
// Mouse motion/drag owns it for 1.5 s after the latest activity. VR writes update
// the reference, so ordinary ray movement does not masquerade as mouse input.
class UiPointerMousePriority final {
public:
    bool Observe(float x, float y, bool valid, bool button_held, std::uint64_t now_ms) noexcept {
        if (!valid || !std::isfinite(x) || !std::isfinite(y)) {
            have_reference_ = false;
            return false;
        }
        if (button_held || (have_reference_ &&
            (std::abs(x - reference_x_) > 0.5F || std::abs(y - reference_y_) > 0.5F)))
            override_until_ms_ = now_ms + 1500;
        Applied(x, y);
        return now_ms >= override_until_ms_;
    }
    void Applied(float x, float y) noexcept {
        reference_x_ = x;
        reference_y_ = y;
        have_reference_ = true;
    }
    std::uint64_t override_until_ms() const noexcept { return override_until_ms_; }
private:
    float reference_x_ = 0, reference_y_ = 0;
    bool have_reference_ = false;
    std::uint64_t override_until_ms_ = 0;
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
        if (pressed && held) {
            state_.click = ++next_click_;
            state_.pending = true;
        }
    }
    UiPointerSelectionSnapshot snapshot() const noexcept { return state_; }
    void Cancel(const UiPointerSelectionSnapshot& expected) noexcept {
        if (expected.hand == state_.hand && expected.claim == state_.claim &&
            expected.click == state_.click) state_.pending = false;
    }
    void Complete(const UiPointerSelectionSnapshot& expected, bool success) noexcept {
        if (success && expected.pending) Cancel(expected);
    }
private:
    UiPointerSelectionSnapshot state_{};
    std::uint64_t next_click_ = 0;
};

// A menu-held gameplay shoulder must be released before regaining ownership.
// Only that gameplay action's availability can prove its physical release;
// automatic UI pointing does not depend on the legacy global shoulder actions.
class UiPointerGameplayGate final {
public:
    GameplayInputState Filter(bool menu, const GameplayInputState& raw) noexcept {
        Observe(menu, raw.active && (raw.digital_available & (1U << 6)) != 0, raw.interact,
                raw.active && (raw.digital_available & (1U << 7)) != 0, raw.weapon_next);
        auto result = raw;
        if (left_blocked_) {
            result.interact = false;
            result.digital_available &= ~(1U << 6);
        }
        if (right_blocked_) {
            result.weapon_next = false;
            result.digital_available &= ~(1U << 7);
        }
        return result;
    }
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
