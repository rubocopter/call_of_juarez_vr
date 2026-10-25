#pragma once
#include "runtime/vr_types.hpp"
#include "runtime/vr_settings.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>

namespace cojvr::games::call_of_juarez {
enum class CoJInputDispatchPhase { all, non_fire, pre_shoulder, shoulder, fire };
enum class CoJTurnInhibitReason {
    not_updated, none, inactive, unavailable, radial, nonfinite, invalid_mode,
    awaiting_neutral, neutral, duplicate_time, no_elapsed_time, axis_deadzone, snap_latched
};
struct CoJTurnDispatchObservation {
    CoJTurnInhibitReason reason = CoJTurnInhibitReason::not_updated;
    runtime::TurnMode mode = runtime::TurnMode::smooth;
    std::uint64_t delta_us = 0;
    bool delta_valid = false, first = false, gap = false, clock_reversed = false;
    bool context_changed = false, mode_changed = false, duplicate = false;
    bool available = false, interrupted = false, armed = false, rearmed = false, latched = false;
};

// Read-only companion to GameplayTurnState, never a source of gameplay decisions.
// Track every actual Update (even with logging off) so enabling a trace mid-hold
// doesn't invent a fresh neutral barrier. The differential fixture guards this
// exact policy description; changes to the runtime policy must update it too.
class CoJTurnDispatchObserver final {
public:
    [[nodiscard]] CoJTurnDispatchObservation Observe(const runtime::GameplayInputState& input,
        std::uint64_t time, runtime::TurnMode mode, float requested_native_yaw) noexcept {
        CoJTurnDispatchObservation out{};
        out.mode = mode; out.first = !seen_;
        out.clock_reversed = seen_ && time < previous_;
        out.delta_valid = seen_ && !out.clock_reversed;
        if (out.delta_valid) out.delta_us = time - previous_;
        out.gap = out.delta_valid && out.delta_us > 100000;
        out.context_changed = seen_ && context_ != input.input_context_generation;
        out.mode_changed = seen_ && mode_ != mode;
        out.duplicate = seen_ && time == previous_;
        const bool valid_mode = mode == runtime::TurnMode::smooth || mode == runtime::TurnMode::snap;
        const bool finite = std::isfinite(input.turn.x) && std::isfinite(input.turn.y);
        out.available = input.active && input.turn_available && !input.weapon_radial && valid_mode && finite;
        out.interrupted = !known_ || !out.available || out.mode_changed || out.context_changed ||
            out.clock_reversed || out.gap;
        if (out.interrupted) armed_ = latched_ = false;
        const bool was_armed = armed_;
        // Snap deliberately ignores a duplicate sample before checking neutral.
        const bool duplicate_snap = mode == runtime::TurnMode::snap && out.duplicate && !out.interrupted;
        const bool neutral = finite && std::hypot(input.turn.x, input.turn.y) <=
            (mode == runtime::TurnMode::snap ? .35F : .25F);
        if (out.available && neutral && !duplicate_snap) { armed_ = true; latched_ = false; }
        out.rearmed = !was_armed && armed_;
        if (!input.active) out.reason = CoJTurnInhibitReason::inactive;
        else if (!input.turn_available) out.reason = CoJTurnInhibitReason::unavailable;
        else if (input.weapon_radial) out.reason = CoJTurnInhibitReason::radial;
        else if (!finite) out.reason = CoJTurnInhibitReason::nonfinite;
        else if (!valid_mode) out.reason = CoJTurnInhibitReason::invalid_mode;
        else if (requested_native_yaw != 0) out.reason = CoJTurnInhibitReason::none;
        else if (duplicate_snap) out.reason = CoJTurnInhibitReason::duplicate_time;
        else if (neutral) out.reason = CoJTurnInhibitReason::neutral;
        else if (!armed_) out.reason = CoJTurnInhibitReason::awaiting_neutral;
        else if (out.duplicate) out.reason = CoJTurnInhibitReason::duplicate_time;
        else if (out.interrupted) out.reason = CoJTurnInhibitReason::no_elapsed_time;
        else if (mode == runtime::TurnMode::snap && latched_) out.reason = CoJTurnInhibitReason::snap_latched;
        else out.reason = CoJTurnInhibitReason::axis_deadzone;
        if (mode == runtime::TurnMode::snap && requested_native_yaw != 0) latched_ = true;
        out.armed = armed_; out.latched = latched_;
        known_ = out.available; seen_ = true; previous_ = time;
        context_ = input.input_context_generation; mode_ = mode;
        return out;
    }
private:
    std::uint64_t previous_ = 0, context_ = 0;
    runtime::TurnMode mode_ = runtime::TurnMode::smooth;
    bool seen_ = false, known_ = false, armed_ = false, latched_ = false;
};

struct CoJInputDispatchDiagnostic;
struct CoJInputDispatchTraceContext {
    // Copy the existing CameraProbeCommand::movement_trace_enabled. No new
    // environment variable, file polling, JNI observation or separate opt-in.
    bool movement_trace_enabled = false;
    std::uint64_t frame_sequence = 0, sample_sequence = 0, pose_sequence = 0, native_context = 0;
    std::uint32_t process_id = 0;
    std::uintptr_t device_identity = 0;
    // Hard ceiling remains 4096; zero suppresses records. Disable/re-enable
    // starts a fresh budget without resetting either gameplay or its observer.
    std::uint32_t max_records = 4096;
    void* sink_context = nullptr;
    void (*sink)(void*, const CoJInputDispatchDiagnostic&) noexcept = nullptr;
};
struct CoJInputDispatchDiagnostic {
    CoJInputDispatchTraceContext identity{};
    CoJInputDispatchPhase phase = CoJInputDispatchPhase::all;
    runtime::GameplayInputState input{}; // Actual phase input, after upstream policy.
    std::uint64_t dispatch_monotonic_us = 0, dispatch_delta_us = 0, duration_us = 0;
    // Caller-supplied policy clock is separate from the measured entry clock.
    // In particular zero in an untimed fire/shoulder phase is not a pose ID.
    std::uint64_t turn_time_us = 0;
    std::uint64_t player_generation_before = 0, player_generation_after = 0;
    std::uintptr_t player_reference_before = 0, player_reference_after = 0; // JNI refs, NOT native IDs.
    std::uint32_t record_index = 0, record_limit = 0;
    bool dispatch_delta_valid = false, success = false, budget_exhausted = false;
    bool turn_updated = false, turn_attempted = false, turn_completed = false;
    bool turn_native_epsilon_suppressed = false;
    float turn_requested_degrees = 0, turn_delivered_degrees = 0;
    CoJTurnDispatchObservation turn{};
    float head_relative_move_yaw_degrees = 0;
    // Native action order 4,5,6,7. Completed callbacks are acknowledgements,
    // not proof of displacement. A throwing callback may already have mutated.
    std::array<float, 4> requested_move{}, acknowledged_move{};
    bool move_values_built = false;
    std::array<std::uint32_t, 4> move_callbacks_attempted{}, move_callbacks_completed{};
    bool analog_lock_attempted = false, analog_lock_completed = false;
    bool analog_unlock_attempted = false, analog_unlock_completed = false;
    bool analog_apply_attempted = false, analog_apply_completed = false, analog_uncertain = false;
    // Successful digital phase decisions, including unchanged cached actions.
    // These are not counts of native transitions; callbacks are never replayed.
    std::uint64_t actions_acknowledged_mask = 0;
    int failed_action = -1;
    const char* stage = "entry"; // Last boundary reached; inspect success alongside it.
};

[[nodiscard]] inline const char* CoJTurnInhibitReasonName(CoJTurnInhibitReason reason) noexcept {
    switch (reason) {
#define COJVR_REASON(value) case CoJTurnInhibitReason::value: return #value
        COJVR_REASON(not_updated); COJVR_REASON(none); COJVR_REASON(inactive);
        COJVR_REASON(unavailable); COJVR_REASON(radial); COJVR_REASON(nonfinite);
        COJVR_REASON(invalid_mode); COJVR_REASON(awaiting_neutral); COJVR_REASON(neutral);
        COJVR_REASON(duplicate_time); COJVR_REASON(no_elapsed_time);
        COJVR_REASON(axis_deadzone); COJVR_REASON(snap_latched);
#undef COJVR_REASON
    }
    return "unknown";
}

// Optional sink-side formatting, invoked only for admitted diagnostic rows.
// No JNI, native callback, retry or retained object is involved.
[[nodiscard]] inline std::string FormatCoJInputDispatchDiagnostic(const CoJInputDispatchDiagnostic& row) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << std::boolalpha << "frame_sequence=" << row.identity.frame_sequence
        << ";sample_sequence=" << row.identity.sample_sequence << ";pose_sequence=" << row.identity.pose_sequence
        << ";native_context=" << row.identity.native_context << ";process_id=" << row.identity.process_id
        << ";device_identity=" << row.identity.device_identity << ";phase=" << static_cast<int>(row.phase)
        << ";dispatch_clock=steady_clock_us;dispatch_monotonic_us=" << row.dispatch_monotonic_us
        << ";dispatch_delta_valid=" << row.dispatch_delta_valid << ";dispatch_delta_us=" << row.dispatch_delta_us
        << ";duration_us=" << row.duration_us << ";turn_time_us=" << row.turn_time_us
        << ";player_generation_before=" << row.player_generation_before
        << ";player_generation_after=" << row.player_generation_after
        << ";player_jni_ref_before=" << row.player_reference_before << ";player_jni_ref_after=" << row.player_reference_after
        << ";active=" << row.input.active << ";move_available=" << row.input.move_available
        << ";turn_available=" << row.input.turn_available << ";radial_available=" << row.input.radial_available
        << ";digital_available=" << row.input.digital_available << ";weapon_radial=" << row.input.weapon_radial
        << ";input_context_generation=" << row.input.input_context_generation
        << ";move=(" << row.input.move.x << ',' << row.input.move.y << ");turn=(" << row.input.turn.x << ',' << row.input.turn.y << ')'
        << ";head_relative_move_yaw_degrees=" << row.head_relative_move_yaw_degrees
        << ";turn_updated=" << row.turn_updated << ";turn_mode=" << static_cast<int>(row.turn.mode)
        << ";turn_reason=" << CoJTurnInhibitReasonName(row.turn.reason)
        << ";turn_delta_valid=" << row.turn.delta_valid << ";turn_delta_us=" << row.turn.delta_us
        << ";turn_first=" << row.turn.first << ";turn_available_effective=" << row.turn.available
        << ";turn_interrupted=" << row.turn.interrupted << ";turn_gap=" << row.turn.gap
        << ";turn_clock_reversed=" << row.turn.clock_reversed << ";turn_duplicate=" << row.turn.duplicate
        << ";turn_context_changed=" << row.turn.context_changed << ";turn_mode_changed=" << row.turn.mode_changed
        << ";turn_armed=" << row.turn.armed << ";turn_rearmed=" << row.turn.rearmed << ";snap_latched=" << row.turn.latched
        << ";turn_requested_degrees=" << row.turn_requested_degrees << ";turn_attempted=" << row.turn_attempted
        << ";turn_completed=" << row.turn_completed << ";turn_delivered_degrees=" << row.turn_delivered_degrees;
    out << ";turn_native_epsilon_suppressed=" << row.turn_native_epsilon_suppressed;
    out << ";move_values_built=" << row.move_values_built;
    for (std::size_t i = 0; i < 4; ++i)
        out << ";move" << i + 4 << "_requested=" << row.requested_move[i]
            << ";move" << i + 4 << "_acknowledged=" << row.acknowledged_move[i]
            << ";move" << i + 4 << "_callbacks_attempted=" << row.move_callbacks_attempted[i]
            << ";move" << i + 4 << "_callbacks_completed=" << row.move_callbacks_completed[i];
    out << ";analog_lock_attempted=" << row.analog_lock_attempted << ";analog_lock_completed=" << row.analog_lock_completed
        << ";analog_unlock_attempted=" << row.analog_unlock_attempted << ";analog_unlock_completed=" << row.analog_unlock_completed
        << ";analog_apply_attempted=" << row.analog_apply_attempted << ";analog_apply_completed=" << row.analog_apply_completed
        << ";analog_uncertain=" << row.analog_uncertain << ";actions_acknowledged_mask=" << row.actions_acknowledged_mask
        << ";failed_action=" << row.failed_action << ";stage=" << row.stage << ";success=" << row.success
        << ";record_index=" << row.record_index << ";record_limit=" << row.record_limit
        << ";budget_exhausted=" << row.budget_exhausted << ";turn_reason_source=policy_observer;movement_displacement_evaluated=false";
    return out.str();
}
} // namespace cojvr::games::call_of_juarez
