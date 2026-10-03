#include "runtime/gameplay_utility.hpp"
#include <cmath>

namespace cojvr::runtime {

GameplayInputState GameplayUtilityMapper::Update(const GameplayInputState& raw,
    const bool gameplay_context_allowed) noexcept {
    if (raw.active && raw.input_context_generation != input_context_generation_) {
        (void)Update({}, false);
        input_context_generation_ = raw.input_context_generation;
    }
    if (!raw.active || !gameplay_context_allowed) {
        release_required_ = 0xFFFFFFFFU;
        utility_consumed_ = 0;
        previous_held_ = 0;
        was_utility_ = false;
        utility_armed_ = false;
        focus_latched_ = false;
        focus_gesture_held_ = false;
        turn_release_required_ = true;
        radial_ready_ = false;
        radial_selection_ = -1;
        return {};
    }
    if (!raw.utility_available) utility_armed_ = false;
    else if (!raw.utility_modifier) utility_armed_ = true;
    const bool utility = raw.utility_modifier && utility_armed_;
    if (utility && !was_utility_) release_required_ |= previous_held_;
    was_utility_ = utility;
    auto result = raw;
    result.utility_modifier = utility;
    std::uint32_t held = 0;
    const auto filter = [&](const bool value, bool& output, const std::size_t index) {
        const std::uint32_t bit = 1U << index;
        if ((raw.digital_available & bit) == 0) {
            release_required_ |= bit;
            output = false;
        } else if (!value) {
            release_required_ &= ~bit;
            utility_consumed_ &= ~bit;
        } else {
            held |= bit;
            if ((release_required_ & bit) != 0 ||
                (!utility && (utility_consumed_ & bit) != 0)) output = false;
        }
    };
    for (std::size_t i = 0; i < kGameplayDigitalMembers.size(); ++i) {
        const auto member = kGameplayDigitalMembers[i];
        filter(raw.*member, result.*member, i);
    }
    for (std::size_t i = 0; i < result.equipment_select.size(); ++i)
        filter(raw.equipment_select[i], result.equipment_select[i], kGameplayDigitalMembers.size() + i);
    previous_held_ = held;

    if (!raw.move_available) result.move = {};
    const bool finite_turn = raw.turn_available &&
        std::isfinite(raw.turn.x) && std::isfinite(raw.turn.y);
    const float magnitude = finite_turn ? std::hypot(raw.turn.x, raw.turn.y) : 0.0F;
    const bool neutral = finite_turn && magnitude <= 0.25F;
    if (!finite_turn) turn_release_required_ = true;
    const bool focus_gesture = utility && result.reload;
    if (focus_gesture && !focus_gesture_held_) focus_latched_ = !focus_latched_;
    focus_gesture_held_ = focus_gesture;
    if (utility) {
        utility_consumed_ |= held;
        result.alternate_fire |= result.jump;
        result.hands |= result.kick;
        result.walk |= result.run;
        result.objectives |= result.crouch;
        result.discard_weapon |= result.interact;
        result.logs |= result.fire_right;
        result.weapon_previous |= result.weapon_next;
        result.fire_left = result.fire_right = result.jump = result.reload = false;
        result.run = result.crouch = result.interact = result.weapon_next = result.kick = false;
        result.turn = {};
        turn_release_required_ = true;
        if (neutral) {
            radial_ready_ = true;
            radial_selection_ = -1;
        } else if (!finite_turn) {
            radial_ready_ = false;
            radial_selection_ = -1;
        } else if (magnitude >= 0.65F && radial_ready_) {
            constexpr float kRadiansToDegrees = 57.295779513F;
            float angle = std::atan2(raw.turn.x, raw.turn.y) * kRadiansToDegrees;
            if (angle < 0) angle += 360.0F;
            radial_selection_ = static_cast<int>(std::floor((angle + 22.5F) / 45.0F)) % 8;
            radial_ready_ = false;
        }
        // Hold the selected intent through the gesture. A one-poll pulse could
        // be lost between presenter samples and a slower game render callback.
        if (radial_selection_ >= 0 && radial_selection_ < 6)
            result.equipment_select[radial_selection_] = true;
        else if (radial_selection_ == 6) result.hands = true;
        else if (radial_selection_ == 7) result.discard_weapon = true;
    } else {
        radial_ready_ = neutral;
        radial_selection_ = -1;
        if (neutral) turn_release_required_ = false;
        if (!finite_turn || turn_release_required_) result.turn = {};
    }
    result.focus |= focus_latched_;
    return result;
}

} // namespace cojvr::runtime
