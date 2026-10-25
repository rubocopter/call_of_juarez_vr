// Value-only observer coverage. Intentionally authored without compiling this batch.
#include "games/call_of_juarez/input_dispatch_diagnostics.hpp"
#include "runtime/turn_preference.hpp"
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
int main() {
    bool ok = true;
    auto check = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ok = false; }
    };
    CoJTurnDispatchObserver observer;
    GameplayTurnState policy;
    GameplayInputState input{};
    input.active = true;
    input.input_context_generation = 900;
    auto sample = [&](std::uint64_t time, TurnMode mode) {
        const float native = -policy.Update(input, time, mode);
        return observer.Observe(input, time, mode, native);
    };
    auto row = sample(10000, TurnMode::smooth);
    check(row.rearmed && row.armed && row.reason == CoJTurnInhibitReason::neutral,
        "first neutral did not record smooth rearm");
    input.turn = {1, 0};
    row = sample(20000, TurnMode::smooth);
    check(row.reason == CoJTurnInhibitReason::none && row.delta_us == 10000,
        "normal smooth dispatch was inhibited");
    row = sample(210000, TurnMode::smooth);
    check(row.gap && !row.armed && row.reason == CoJTurnInhibitReason::awaiting_neutral &&
        row.delta_us == 190000, "gap did not explain held-stick inhibition");
    check(sample(220000, TurnMode::smooth).reason == CoJTurnInhibitReason::awaiting_neutral,
        "held stick falsely rearmed");
    input.turn = {};
    check(sample(230000, TurnMode::smooth).rearmed, "fresh neutral did not rearm");
    input.turn = {1, 0};
    check(sample(240000, TurnMode::smooth).reason == CoJTurnInhibitReason::none,
        "smooth did not recover");
    row = sample(240000, TurnMode::smooth);
    check(row.duplicate && row.reason == CoJTurnInhibitReason::duplicate_time,
        "duplicate time was mislabelled");
    row = sample(239999, TurnMode::smooth);
    check(row.clock_reversed && !row.delta_valid && !row.armed, "reversed clock underflowed");
    input.turn = {};
    row = sample(250000, TurnMode::snap);
    check(row.mode_changed && row.rearmed, "snap mode switch lost neutral rearm");
    input.turn = {1, 0};
    check(sample(260000, TurnMode::snap).reason == CoJTurnInhibitReason::none,
        "fresh snap was labelled inhibited");
    check(sample(270000, TurnMode::snap).reason == CoJTurnInhibitReason::snap_latched,
        "held snap did not retain its latch");
    input.input_context_generation++;
    row = sample(280000, TurnMode::snap);
    check(row.context_changed && !row.armed, "context replacement retained turn ownership");
    input.turn_available = false;
    check(sample(290000, TurnMode::snap).reason == CoJTurnInhibitReason::unavailable,
        "availability loss was hidden");
    input.turn_available = true; input.weapon_radial = true;
    check(sample(300000, TurnMode::snap).reason == CoJTurnInhibitReason::radial,
        "wheel ownership was hidden");
    input.weapon_radial = false; input.turn.x = std::numeric_limits<float>::quiet_NaN();
    check(sample(310000, TurnMode::snap).reason == CoJTurnInhibitReason::nonfinite,
        "nonfinite turn was hidden");
    input.active = false;
    check(sample(320000, TurnMode::snap).reason == CoJTurnInhibitReason::inactive,
        "inactive release was hidden");
    input = {}; input.active = true;
    check(sample(330000, static_cast<TurnMode>(255)).reason == CoJTurnInhibitReason::invalid_mode,
        "invalid turn mode was hidden");
    CoJInputDispatchDiagnostic formatted{};
    formatted.identity.frame_sequence = 11; formatted.identity.sample_sequence = 22;
    formatted.identity.pose_sequence = 33; formatted.turn_time_us = 44;
    formatted.dispatch_monotonic_us = 55; formatted.turn.reason = CoJTurnInhibitReason::awaiting_neutral;
    formatted.move_callbacks_attempted[0] = 1; formatted.analog_uncertain = true;
    const auto text = FormatCoJInputDispatchDiagnostic(formatted);
    check(text.find("frame_sequence=11;sample_sequence=22;pose_sequence=33") != std::string::npos &&
        text.find("dispatch_monotonic_us=55") != std::string::npos &&
        text.find("turn_time_us=44") != std::string::npos &&
        text.find("turn_reason=awaiting_neutral") != std::string::npos &&
        text.find("move4_callbacks_attempted=1;move4_callbacks_completed=0") != std::string::npos &&
        text.find("analog_uncertain=true") != std::string::npos,
        "sink formatting lost identity/clock separation or partial delivery");

    // Probe a COPY of the real policy with a subsequent full deflection.
    // Observer arming/latch state must predict this independently produced
    // response. Comparing reason==none to the yaw passed into Observe would
    // merely repeat the observer's own direct selection and miss state drift.
    observer = {}; policy = {}; input = {}; input.active = true;
    for (auto mode : {TurnMode::smooth, TurnMode::snap}) {
        for (std::uint64_t i = 1; i <= 400; ++i) {
            input.turn = {i % 7 == 0 ? 0.F : i % 2 ? .8F : -.8F, 0};
            input.turn_available = i % 19 != 0;
            input.weapon_radial = i % 23 == 0;
            input.input_context_generation = i / 31;
            const auto time = i * 10000 + (i / 37) * 190000;
            const float yaw = -policy.Update(input, time, mode);
            row = observer.Observe(input, time, mode, yaw);
            auto probe = policy;
            auto deflected = input;
            deflected.turn = {1, 0}; deflected.turn_available = true; deflected.weapon_radial = false;
            const float next_yaw = probe.Update(deflected, time + 10000, mode);
            const bool predicts_next_turn = row.available && row.armed &&
                (mode == TurnMode::smooth || !row.latched);
            check(predicts_next_turn == (next_yaw != 0),
                "observer armed/latch state disagrees with real policy's next response");
        }
    }
    return ok ? 0 : 1;
}
