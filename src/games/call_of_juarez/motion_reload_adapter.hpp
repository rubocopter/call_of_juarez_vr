#pragma once
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "runtime/reload_gesture.hpp"
#include <algorithm>

namespace cojvr::games::call_of_juarez {
// Exact-game gesture eligibility and physical-trigger consumption. This never
// bypasses native reload delivery, ammunition checks or animation ownership.
class CoJMotionReloadAdapter final {
public:
    [[nodiscard]] runtime::MotionReloadGestureOutput Update(
        runtime::GameplayInputState& input, const runtime::GameplayInputState& raw,
        const CoJMotionReloadOwner& owner, const CoJMotionReloadDiagnostic& diagnostic,
        const std::uint64_t player,
        const runtime::Pose& head, const runtime::Pose& left, const runtime::Pose& right,
        const bool context_allowed, const bool recentered, const std::uint64_t sequence,
        const std::uint64_t now, const std::uint64_t native_context,
        // Separate passive ongoing-reload observation, never initial admission.
        // The caller must verify actual/active/desired weapon JNI identity, exact
        // single-round class, empty support and native reload/recovery states on
        // this player. Admission's two_handed/armed_state reason alone proves
        // none of that. Default absence preserves fail-closed behavior.
        const CoJMotionReloadOwner& native_reload_owner = {}) noexcept {
        const bool native_changed = have_native_context_ && native_context != native_context_;
        have_native_context_ = true;
        native_context_ = native_context;
        const bool conflicting = input.reload || input.interact || input.objectives || input.logs ||
            input.quick_load || input.quick_save || input.weapon_next || input.weapon_previous ||
            input.hands || input.discard_weapon || input.alternate_fire ||
            std::any_of(input.equipment_select.begin(), input.equipment_select.end(),
                [](bool held) { return held; });
        const int effective_armed = owner.valid ? owner.armed_hand : continuation_.armed_hand;
        const bool armed_firing = effective_armed == 0 ? input.fire_right :
            effective_armed == 1 ? input.fire_left : false;
        const bool base_allowed = raw.active && context_allowed && input.active && !recentered &&
            !cancel_pending_ && !native_changed && (native_context & 1U) != 0 && !conflicting &&
            !armed_firing && !raw.weapon_radial && !input.weapon_radial;
        const bool regression = have_sample_ && (sequence < sequence_ || now < observed_ms_);
        const bool fresh = !have_sample_ || sequence > sequence_;
        const bool gap = have_sample_ && now >= fresh_ms_ && now - fresh_ms_ > 250;
        if (!regression) observed_ms_ = now;
        if (fresh && !regression) {
            have_sample_ = true;
            sequence_ = sequence;
            fresh_ms_ = now;
        }
        std::uint8_t native_consumed_trigger_mask = 0;
        if (native_trigger_hand_ < 2) {
            // An acquired physical claim survives owner/context loss, just like
            // gesture claims. Neutralized or unavailable input is not release.
            native_consumed_trigger_mask = static_cast<std::uint8_t>(1U << native_trigger_hand_);
            const auto binding = native_trigger_hand_ == 0 ? 2U : 1U;
            const bool held = native_trigger_hand_ == 0 ? raw.fire_right : raw.fire_left;
            if (fresh && !regression && !gap && raw.active &&
                (raw.digital_available & binding) != 0 && !held)
                native_trigger_hand_ = 2;
        }
        bool replenish_cartridge = false;
        if (continuation_.active) {
            const bool same_context = base_allowed && player == continuation_.player &&
                raw.input_context_generation == continuation_.input_generation &&
                native_context == continuation_.native_context;
            const bool native_owner_matches = native_reload_owner.valid &&
                native_reload_owner.single_round_supported &&
                native_reload_owner.armed_hand == continuation_.armed_hand &&
                native_reload_owner.weapon_id == continuation_.weapon;
            const bool native_recovery = !owner.valid && continuation_.saw_native_reload &&
                native_owner_matches &&
                (diagnostic.reason == CoJMotionReloadRejectReason::two_handed ||
                 diagnostic.reason == CoJMotionReloadRejectReason::armed_state);
            const bool native_fault = diagnostic.player_dead ||
                diagnostic.attacking[0] || diagnostic.attacking[1] ||
                (diagnostic.reason != CoJMotionReloadRejectReason::none &&
                 diagnostic.reason != CoJMotionReloadRejectReason::dead_or_reloading &&
                 !native_recovery);
            const auto support_binding = continuation_.armed_hand == 0 ? 1U : 2U;
            const bool tracking_available = ValidPose(head) && ValidPose(left) && ValidPose(right) &&
                (raw.digital_available & support_binding) != 0;
            if (!same_context || native_fault || !tracking_available || regression || gap) {
                ClearContinuation();
            } else if (!fresh) {
                // Duplicate render polls cannot establish native progress or
                // consume the one-sample replenishment before a fresh pose.
            } else {
                const bool support_held = continuation_.armed_hand == 0 ? raw.fire_left : raw.fire_right;
                if (native_owner_matches && (diagnostic.weapon_reloading || native_recovery) && support_held) {
                    // Tracked muzzle caches yield during native animation. An
                    // unclaimed support press could otherwise use the native
                    // cross-hand attack fallback and interrupt this reload.
                    native_trigger_hand_ = static_cast<std::uint8_t>(1 - continuation_.armed_hand);
                    native_consumed_trigger_mask |= static_cast<std::uint8_t>(1U << native_trigger_hand_);
                }
                if (diagnostic.weapon_reloading) {
                    continuation_.saw_native_reload = true;
                } else if (native_recovery) {
                    // Desired reload can end before the actual/destination states
                    // and native two-hand animation flags return to ordinary idle.
                    // Retain only the proven owner; no token or request is emitted
                    // until the original admission reader positively recovers it.
                } else if (owner.valid) {
                    const bool same_owner = owner.single_round_supported &&
                        owner.armed_hand == continuation_.armed_hand &&
                        owner.weapon_id == continuation_.weapon;
                    if (same_owner && continuation_.saw_native_reload) replenish_cartridge = true;
                    ClearContinuation();
                } else {
                    ClearContinuation();
                }
            }
        }
        runtime::MotionReloadGestureInput sample{};
        sample.valid = raw.active && owner.valid && player != 0;
        sample.eligible = base_allowed;
        sample.player_identity = player;
        sample.weapon_identity = owner.weapon_id;
        sample.armed_hand = static_cast<std::uint8_t>(owner.armed_hand);
        sample.head = head; sample.left_grip = left; sample.right_grip = right;
        // Inactive input is neutralized by the presenter, not an observed release.
        sample.trigger_available = {raw.active && (raw.digital_available & 2U) != 0,
            raw.active && (raw.digital_available & 1U) != 0};
        sample.trigger_held = {raw.fire_right, raw.fire_left};
        sample.input_generation = raw.input_context_generation;
        sample.pose_sequence = sequence; sample.monotonic_ms = now;
        sample.replenish_cartridge = replenish_cartridge;
        auto result = gesture_.Update(sample);
        result.consumed_trigger_mask |= native_consumed_trigger_mask;
        result.consume_free_trigger = result.consumed_trigger_mask != 0;
        if (result.claimed_hand >= 2 && native_consumed_trigger_mask != 0)
            result.claimed_hand = (native_consumed_trigger_mask & 1U) != 0 ? 0 : 1;
        request_candidate_.active = result.reload && owner.valid && owner.single_round_supported;
        if (request_candidate_.active) {
            request_candidate_.player = player;
            request_candidate_.weapon = owner.weapon_id;
            request_candidate_.armed_hand = owner.armed_hand;
            request_candidate_.input_generation = raw.input_context_generation;
            request_candidate_.native_context = native_context;
            request_candidate_.saw_native_reload = false;
        }
        cancel_pending_ = false;
        if ((result.consumed_trigger_mask & 1U) != 0) input.fire_right = false;
        if ((result.consumed_trigger_mask & 2U) != 0) input.fire_left = false;
        input.reload |= result.reload;
        return result;
    }
    void NoteSingleRoundDispatch(const bool accepted) noexcept {
        if (!request_candidate_.active) return;
        if (accepted) continuation_ = request_candidate_;
        else ClearContinuation();
        request_candidate_ = {};
    }
    void Cancel() noexcept {
        cancel_pending_ = true;
        request_candidate_ = {};
        ClearContinuation();
    }
    [[nodiscard]] runtime::MotionReloadGestureStage stage() const noexcept {
        return gesture_.stage();
    }
private:
    [[nodiscard]] static bool ValidPose(const runtime::Pose& pose) noexcept {
        const auto& p = pose.position;
        const auto& q = pose.orientation;
        return pose.position_valid && pose.orientation_valid &&
            std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
            static_cast<double>(q.x)*q.x + static_cast<double>(q.y)*q.y +
                static_cast<double>(q.z)*q.z + static_cast<double>(q.w)*q.w > 1.0e-12;
    }
    struct Continuation {
        bool active = false;
        bool saw_native_reload = false;
        std::uint64_t player = 0;
        std::uint64_t weapon = 0;
        std::uint64_t input_generation = 0;
        std::uint64_t native_context = 0;
        int armed_hand = -1;
    };
    void ClearContinuation() noexcept { continuation_ = {}; }
    runtime::MotionReloadGesture gesture_;
    Continuation request_candidate_{};
    Continuation continuation_{};
    std::uint8_t native_trigger_hand_ = 2;
    bool have_native_context_ = false, cancel_pending_ = false;
    bool have_sample_ = false;
    std::uint64_t sequence_ = 0, fresh_ms_ = 0, observed_ms_ = 0;
    std::uint64_t native_context_ = 0;
};
}
