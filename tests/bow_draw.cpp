#include "runtime/bow_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::runtime;
namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { ++failures; std::cerr << message << '\n'; }
}
bool Near(float actual, float expected) { return std::fabs(actual - expected) < .0001F; }
struct Fixture {
    BowDrawPolicy policy;
    BowDrawInput in{};
    Fixture() {
        in.eligible = in.right_trigger_available = true;
        in.owner = 3; in.player = 5; in.native_context = 7; in.input_generation = 9;
        in.head_position_m = {0, 1.6F, 0};
        in.left_position_m = {-.2F, 1.3F, -.4F};
        in.string_point_m = {0, 1.3F, -.4F};
        in.right_position_m = in.string_point_m;
        in.aim_forward = {0, 0, -1};
    }
    BowDrawResult Step() { ++in.sequence; in.now_ms += 20; return policy.Update(in); }
    BowDrawResult Grab() { Step(); in.right_trigger_held = true; return Step(); }
};
// Catches missing physical capture, signed draw, minimum draw and one-shot release.
void SuccessfulPhysicalShot() {
    Fixture f;
    const auto grab = f.Grab();
    Check(grab.grab && grab.drawing && grab.haptics.grabbed, "fresh near R2 grabs without any left-trigger input");
    f.in.right_position_m.z += .2F;
    const auto draw = f.Step();
    Check(draw.drawing && !draw.fire && Near(draw.normalized_draw, .36363636F), "20 cm retraction gives current 20/55 tension");
    Check(draw.haptics.tension_step, "physical retraction emits a tension step");
    f.in.right_trigger_held = false;
    const auto shot = f.Step();
    Check(shot.release && shot.fire && !shot.drawing && shot.haptics.released, "R2 release emits one fire intent");
    Check(Near(shot.normalized_draw, .36363636F), "terminal result retains the actual released draw");
    Check(!f.policy.Update(f.in).fire && !f.Step().fire, "consumed terminal cannot retry on duplicate or next pose");
}

// Catches startup-held shortcuts, unavailable-as-release and grab from afar.
void FreshPhysicalCapture() {
    Fixture f;
    f.in.right_trigger_held = true;
    Check(!f.Step().grab && !f.Step().grab, "startup held R2 cannot grab");
    f.in.right_trigger_available = false; f.in.right_trigger_held = false;
    Check(!f.Step().grab, "unavailable is not an observed release");
    f.in.right_trigger_available = f.in.right_trigger_held = true;
    Check(!f.Step().grab, "unavailable false did not establish a baseline");
    f.in.right_trigger_held = false; f.Step();
    f.in.right_position_m.x += .121F; f.in.right_trigger_held = true;
    Check(!f.Step().grab, "R2 must be physically near the authored string point");
    f.in.right_position_m = f.in.string_point_m;
    Check(!f.Step().grab, "moving into capture while held cannot retry a missed press");
    f.in.right_trigger_held = false; f.Step(); f.in.right_trigger_held = true;
    Check(f.Step().grab, "available release then near fresh press recovers capture");
}

// Catches historical maximum tension and initial capture-offset tension.
void CurrentDrawAndThresholds() {
    Fixture f;
    f.in.right_position_m.z += .10F;
    Check(f.Grab().grab, "capture tolerance admits a 10 cm initial offset");
    Check(Near(f.Step().normalized_draw, 0), "capture offset creates no draw");
    f.in.right_position_m.z += .20F;
    Check(Near(f.Step().normalized_draw, .36363636F), "draw is relative to grab offset");
    f.in.right_position_m.z -= .10F;
    Check(Near(f.Step().normalized_draw, .18181818F), "relaxation reduces current draw");
    f.in.right_trigger_held = false;
    auto out = f.Step();
    Check(out.release && !out.fire && out.haptics.released, "short release never fires even after a longer draw");
    for (const float distance : {.119F, .12F, .55F}) {
        Fixture g; g.Grab();
        g.in.right_position_m.z += distance / 2; g.Step();
        g.in.right_position_m.z += distance / 2; g.Step();
        g.in.right_trigger_held = false; out = g.Step();
        Check(out.release && out.fire == (distance >= .12F), "physical min/max draw boundary has deterministic fire admission");
    }
    Fixture g; g.Grab();
    g.in.right_position_m.z -= .10F;
    Check(Near(g.Step().normalized_draw, 0), "forward movement cannot tension the bow");
}

// Catches treating the full-tension distance as a cancellation limit and
// retaining saturated historical tension when the physical hand relaxes.
void ContinuousFullDrawSaturates() {
    Fixture f; f.Grab();
    struct Sample { float distance, tension; };
    const Sample pull[] = {
        {.15F, .27272727F}, {.30F, .54545455F}, {.45F, .81818182F},
        {.55F, 1}, {.65F, 1}, {.75F, 1}, {.85F, 1}, {.90F, 1},
    };
    for (const auto sample : pull) {
        f.in.right_position_m.z = f.in.string_point_m.z + sample.distance;
        const auto out = f.Step();
        Check(out.drawing && !out.cancelled && !out.fire, "continuous full pull through 90 cm remains held without cancellation");
        Check(Near(out.normalized_draw, sample.tension), "current tension saturates at 55 cm, independent of the reach guard");
        if (sample.distance > .55F)
            Check(!out.haptics.tension_step, "pulling beyond saturation does not repeat the full-tension pulse");
    }
    f.in.right_trigger_held = false;
    const auto shot = f.Step();
    Check(shot.release && shot.fire && Near(shot.normalized_draw, 1), "release at exactly 90 cm fires once at saturated tension");
    Check(!f.policy.Update(f.in).fire && !f.Step().fire, "saturated shot is consumed before any native retry");

    Fixture relaxed; relaxed.Grab();
    for (const float distance : {.3F, .6F, .9F}) {
        relaxed.in.right_position_m.z = relaxed.in.string_point_m.z + distance;
        (void)relaxed.Step();
    }
    const Sample relaxation[] = {{.70F, 1}, {.45F, .81818182F}, {.20F, .36363636F}, {.10F, .18181818F}};
    for (const auto sample : relaxation) {
        relaxed.in.right_position_m.z = relaxed.in.string_point_m.z + sample.distance;
        const auto out = relaxed.Step();
        Check(out.drawing && Near(out.normalized_draw, sample.tension), "relaxing a long pull exposes current tension instead of the saturated peak");
    }
    relaxed.in.right_trigger_held = false;
    const auto short_release = relaxed.Step();
    Check(short_release.release && !short_release.fire, "relaxing below minimum after a 90 cm pull cannot fire");

    Fixture overreach; overreach.Grab();
    for (const float distance : {.3F, .6F, .9F}) {
        overreach.in.right_position_m.z = overreach.in.string_point_m.z + distance;
        (void)overreach.Step();
    }
    overreach.in.right_position_m.z = overreach.in.string_point_m.z + .901F;
    overreach.in.right_trigger_held = false;
    const auto cancelled = overreach.Step();
    Check(cancelled.cancelled && cancelled.release && !cancelled.fire,
          "raw reach beyond 90 cm cancels even when R2 release arrives in that sample");

    Fixture teleport; teleport.Grab();
    teleport.in.right_position_m.z += .7F;
    Check(teleport.Step().cancelled, "70 cm single-step teleport still cancels despite lying within total reach");

    Fixture custom;
    custom.policy = BowDrawPolicy({.12F, .12F, .55F, .20F, .70F});
    custom.Grab();
    for (const float distance : {.3F, .6F, .7F}) {
        custom.in.right_position_m.z = custom.in.string_point_m.z + distance;
        Check(custom.Step().drawing, "custom reach admits continuous pulls up to its configured boundary");
    }
    custom.in.right_position_m.z = custom.in.string_point_m.z + .701F;
    Check(custom.Step().cancelled, "configured reach guard operates before tension clamping");
}

// Catches frozen world-string geometry and tension manufactured by bow lean.
void MovingBowAndAxisContinuity() {
    Fixture f; f.Grab();
    f.in.left_position_m.z -= .2F; f.in.string_point_m.z -= .2F;
    Check(Near(f.Step().normalized_draw, 0), "moving only the bow cannot manufacture a right-hand draw");
    Fixture g; g.Grab();
    g.in.head_position_m.x += .6F;
    g.in.left_position_m.x += .6F; g.in.right_position_m.x += .6F; g.in.string_point_m.x += .6F;
    Check(g.Step().drawing && Near(g.Step().normalized_draw, 0), "common body lean preserves relative hand motion and zero tension");
    g.in.right_position_m.z += .2F;
    Check(Near(g.Step().normalized_draw, .36363636F), "draw follows the current translated string point");
    Fixture h; h.in.right_position_m.x += .10F; h.Grab();
    h.in.aim_forward = {-.6F, 0, -.8F};
    h.in.right_position_m = {.08F, 1.3F, -.46F};
    Check(Near(h.Step().normalized_draw, 0), "continuous aim rotation does not turn grab offset into tension");
    h.in.aim_forward = {.6F, 0, .8F};
    auto out = h.Step();
    Check(out.cancelled && out.release && !out.fire, "axis reversal cancels instead of manufacturing draw");
}

// Catches caching a rest/grab point when the exact adapter supplies current
// authored animation geometry, and firing from an earlier peak rather than now.
void CurrentAnimatedStringPoint() {
    Fixture f; f.Grab(); f.in.right_position_m.z += .2F; f.Step();
    f.in.string_point_m.z += .05F;
    auto out = f.Step();
    Check(out.drawing && Near(out.normalized_draw, .27272727F), "animated string motion changes current separation: 15/55");
    f.in.string_point_m.z += .05F;
    f.in.right_trigger_held = false;
    out = f.Step();
    Check(out.release && !out.fire && Near(out.normalized_draw, .18181818F), "release uses THIS animated string point and short current draw");

    Fixture g; g.Grab(); g.in.string_point_m.z -= .10F;
    out = g.Step();
    Check(out.drawing && Near(out.normalized_draw, 0) && !out.haptics.tension_step,
          "native string animation alone creates no physical tension or haptics");
    g.in.right_trigger_held = false;
    Check(!g.Step().fire, "native animation cannot fire with a stationary right hand");

    Fixture h; h.Grab(); h.in.right_position_m.z += .2F; h.Step();
    h.in.aim_forward = {-.6F, 0, -.8F};
    h.in.right_position_m = {.12F, 1.3F, -.24F};
    out = h.Step();
    Check(out.drawing && Near(out.normalized_draw, .36363636F), "continuous aim rotation preserves a physical current draw");
    h.in.string_point_m.x -= .26F; h.in.right_trigger_held = false; // .26 * .8 = .208 m lateral.
    out = h.Step();
    Check(!out.cancelled && out.release && !out.fire, "animated string lateral miss on release retires capture without a shot");
}

using Mutation = void (*)(BowDrawInput&);
void AdverseSamples() {
    const Mutation faults[] = {
        [](BowDrawInput& in) { in.eligible = false; },
        [](BowDrawInput& in) { in.paused = true; },
        [](BowDrawInput& in) { in.recentered = true; },
        [](BowDrawInput& in) { in.right_trigger_available = false; },
        [](BowDrawInput& in) { in.owner = 0; },
        [](BowDrawInput& in) { in.player = 0; },
        [](BowDrawInput& in) { in.native_context = 0; },
        [](BowDrawInput& in) { ++in.owner; },
        [](BowDrawInput& in) { ++in.player; },
        [](BowDrawInput& in) { ++in.native_context; },
        [](BowDrawInput& in) { ++in.input_generation; },
        [](BowDrawInput& in) { in.sequence -= 2; },
        [](BowDrawInput& in) { in.sequence = 0; },
        [](BowDrawInput& in) { in.now_ms -= 40; },
        [](BowDrawInput& in) { in.now_ms += 101; },
        [](BowDrawInput& in) { in.left_position_m.x += .351F; },
        [](BowDrawInput& in) { in.right_position_m.x += .351F; },
        [](BowDrawInput& in) { in.head_position_m.y += .351F; },
        [](BowDrawInput& in) { in.right_position_m.x = std::numeric_limits<float>::infinity(); },
        [](BowDrawInput& in) { in.left_position_m.y = std::numeric_limits<float>::quiet_NaN(); },
        [](BowDrawInput& in) { in.head_position_m.z = std::numeric_limits<float>::infinity(); },
        [](BowDrawInput& in) { in.string_point_m.x = std::numeric_limits<float>::quiet_NaN(); },
        [](BowDrawInput& in) { in.aim_forward = {}; },
        [](BowDrawInput& in) { in.aim_forward = {0, 0, -2}; },
        [](BowDrawInput& in) { in.aim_forward.x = std::numeric_limits<float>::infinity(); },
    };
    for (const auto fault : faults) {
        Fixture f; f.Grab(); f.in.right_position_m.z += .2F; f.Step();
        const auto good = f.in;
        fault(f.in);
        // Explicitly present a release at the bad observation: never fire it.
        f.in.right_trigger_held = false;
        const auto out = f.Step();
        Check(out.cancelled && out.release && !out.fire && !out.drawing, "ownership/tracking/geometry/time fault cancels a drawn bow");
        Check(std::string(out.reason)!="none","every hard cancellation reports a diagnostic cause");
        Check(!f.policy.Update(f.in).fire, "fault duplicate cannot retry a fire");
        const auto recovery_time = (std::max)(good.now_ms, f.in.now_ms) + 20;
        f.in = good; f.in.sequence += 5; f.in.now_ms = recovery_time; f.in.right_trigger_held = true;
        Check(!f.Step().grab, "fault release never clears the release barrier");
        f.in.right_trigger_held = false; f.Step();
        f.in.right_position_m = f.in.string_point_m; f.in.right_trigger_held = true;
        Check(f.Step().grab, "fresh release and press recover after a fault");
    }
}

void HeldCaptureAndReach() {
    {
        Fixture f; f.Grab();
        f.in.right_position_m.z += .3F; f.Step();
        f.in.right_position_m.z += .3F;
        Check(f.Step().drawing, "60 cm continuous pull remains valid before the reach limit");
        f.in.right_position_m.z += .301F;
        auto out = f.Step();
        Check(out.cancelled && out.release && !out.fire, "continuous overreach cancels");
        f.in.right_position_m = f.in.string_point_m;
        Check(!f.Step().grab, "reach cancellation requires a fresh R2 release");
        f.in.right_trigger_held = false;
        Check(!f.Step().fire, "release after cancellation is silent");
    }
    Fixture offset;
    offset.in.right_position_m.x = .10F;
    offset.Grab();
    offset.in.right_position_m.x = .201F;
    auto miss = offset.Step();
    Check(miss.drawing && !miss.cancelled && !miss.fire && Near(miss.normalized_draw,0),
        "leaving string axis relaxes tension without dropping held R2 capture");
    offset.in.right_trigger_held=false;miss=offset.Step();
    Check(miss.release&&!miss.fire,"off-axis release cannot shoot");
    for(int lateral=0;lateral<2;++lateral){
        Fixture f;f.Grab();f.in.right_position_m.z+=.2F;f.Step();
        if(lateral)f.in.right_position_m.x=.21F;
        else f.in.right_position_m.z=f.in.string_point_m.z-.13F;
        const auto relaxed=f.Step();
        Check(relaxed.drawing&&!relaxed.cancelled&&Near(relaxed.normalized_draw,0),
            "continuous slack or sideways motion retains capture while R2 is held");
        // Recover gradually to rule out a tracking jump, then make a real pull.
        f.in.right_position_m=f.in.string_point_m;
        Check(f.Step().drawing,"return to string needs no second trigger press");
        f.in.right_position_m.z+=.2F;Check(f.Step().drawing,"physical retraction can tension again");
        f.in.right_trigger_held=false;
        Check(f.Step().fire,"recovered continuous physical draw fires once on release");
        Check(!f.Step().fire,"recovered release never repeats");
    }
}

void DuplicateAndClockAdmission() {
    Fixture f;
    f.Step(); f.in.right_trigger_held = true;
    Check(!f.policy.Update(f.in).grab, "duplicate pose cannot acquire on changed trigger");
    Check(f.Step().grab, "next fresh pose can observe the fresh press");
    f.in.right_position_m.z += .2F; f.Step(); f.in.right_trigger_held = false;
    auto out = f.policy.Update(f.in);
    Check(out.drawing && !out.release && !out.fire && !out.haptics.released, "duplicate release emits no stale transition");
    Check(f.policy.drawing() && Near(out.normalized_draw, .36363636F), "duplicate result and getter retain current drawing state");
    Check(f.Step().fire, "one fresh release consumes the shot");
    Check(!f.policy.Update(f.in).release, "duplicate terminal has no release event either");
    Fixture g; g.Grab(); g.in.now_ms += 60;
    Check(g.policy.Update(g.in).drawing, "fresh duplicate does not cancel");
    g.in.now_ms += 41;
    auto stale=g.policy.Update(g.in);
    Check(stale.cancelled&&std::string(stale.reason)=="pose_stale", "duplicates cannot refresh the unique pose age; diagnostics name stale poses");
    Fixture h; h.Grab(); ++h.in.sequence;
    auto equal=h.policy.Update(h.in);
    Check(equal.cancelled&&std::string(equal.reason)=="pose_timestamp_repeated", "new pose with equal timestamp cannot advance; reason differs from stale pose");
    Fixture i; i.Grab(); ++i.in.sequence; i.in.now_ms += 100;
    Check(i.policy.Update(i.in).drawing, "exactly 100 ms is still admitted");
    i.in.now_ms += 5; (void)i.policy.Update(i.in); i.in.now_ms -= 1;
    Check(i.policy.Update(i.in).cancelled, "clock regression between duplicates also cancels");
    Fixture j; j.Grab(); j.in.left_position_m.y += .35F;
    Check(j.Step().drawing, "exactly 35 cm relative hand step is admitted despite float subtraction roundoff");
}

void ResetAndHaptics() {
    Fixture f; auto out = f.Grab();
    Check(f.policy.drawing(), "public drawing state follows a grab");
    Check(out.haptics.pulse_strength > 0 && out.haptics.pulse_strength <= .25F, "grab pulse is modest");
    f.in.right_position_m.z += .2F; out = f.Step();
    Check(out.haptics.tension_step && out.haptics.pulse_strength <= .25F, "tension pulse is modest");
    Check(!f.Step().haptics.tension_step, "held tension does not pulse every frame");
    Check(!f.policy.Update(f.in).haptics.tension_step, "duplicate tension cannot pulse again");
    f.policy.Reset();
    Check(!f.Step().grab && !f.Step().fire, "explicit reset while held cannot reacquire");
    f.in.right_trigger_held = false; f.Step(); f.in.right_position_m = f.in.string_point_m;
    f.in.right_trigger_held = true;
    Check(f.Step().grab, "explicit reset recovers through available release");
    Fixture g; g.in.owner = 0xFEDCBA98ULL; g.in.player = 0x87654321ULL;
    Check(g.Grab().grab, "opaque native high-bit identities are compared without signed positivity");
    g.in.eligible = false;
    out = g.Step();
    Check(out.cancelled && out.release && !g.policy.drawing(), "cancellation retires public drawing state immediately");
    out = g.policy.Update(g.in);
    Check(!out.cancelled && !out.release && !out.haptics.released, "duplicate invalid sample cannot retry a cancellation event");
    out = g.Step();
    Check(!out.cancelled && !out.release, "sustained ineligibility is not another terminal event");
}

void InvalidParametersFailClosed() {
    const BowDrawParameters invalid[] = {
        {0, .12F, .55F, .2F}, {.12F, -.1F, .55F, .2F},
        {.12F, .6F, .55F, .2F}, {.12F, .12F, .55F, 0},
        {.12F, .12F, std::numeric_limits<float>::infinity(), .2F},
        {std::numeric_limits<float>::quiet_NaN(), .12F, .55F, .2F},
        {.12F, .12F, .55F, .2F, 0}, {.12F, .12F, .55F, .2F, .50F},
        {.12F, .12F, .55F, .2F, std::numeric_limits<float>::quiet_NaN()},
        {.12F, .12F, .55F, .2F, std::numeric_limits<float>::infinity()},
    };
    for (const auto parameters : invalid) {
        Fixture f; f.policy = BowDrawPolicy(parameters);
        Check(!f.Grab().grab && !f.policy.drawing(), "invalid ergonomic configuration cannot admit a grab");
    }
    Fixture f;
    f.in.sequence = 1; f.in.now_ms = (std::numeric_limits<std::uint64_t>::max)() - 2;
    (void)f.policy.Update(f.in);
    ++f.in.sequence; ++f.in.now_ms; f.in.right_trigger_held = true;
    Check(f.policy.Update(f.in).grab, "monotonic clock can approach its unsigned limit");
    ++f.in.sequence; f.in.now_ms = 1;
    Check(f.policy.Update(f.in).cancelled, "clock wrap is a discontinuity, never a huge valid delta");
}
}
int main() {
    SuccessfulPhysicalShot();
    FreshPhysicalCapture();
    CurrentDrawAndThresholds();
    ContinuousFullDrawSaturates();
    MovingBowAndAxisContinuity();
    CurrentAnimatedStringPoint();
    AdverseSamples();
    HeldCaptureAndReach();
    DuplicateAndClockAdmission();
    ResetAndHaptics();
    InvalidParametersFailClosed();
    std::cout << checks << " bow draw checks, " << failures << " failures\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
