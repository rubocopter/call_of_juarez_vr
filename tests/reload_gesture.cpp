#include "runtime/reload_gesture.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::runtime;

namespace {
int failures = 0;
void Require(bool value, const char* message) {
    if (!value) {
        std::cerr << message << '\n';
        ++failures;
    }
}

Pose Tracked(Vec3 position) {
    Pose pose{};
    pose.position = position;
    pose.position_valid = pose.orientation_valid = true;
    return pose;
}

struct Fixture {
    MotionReloadGesture gesture;
    MotionReloadGestureInput input{};

    explicit Fixture(std::uint8_t armed_hand = 0) {
        input.valid = input.eligible = true;
        input.trigger_available = {true, true};
        input.player_identity = 11;
        input.weapon_identity = 29;
        input.armed_hand = armed_hand;
        input.input_generation = 7;
        input.head = Tracked({0.0F, 1.6F, 0.0F});
        input.left_grip = Tracked({-0.20F, 1.05F, -0.10F});
        input.right_grip = Tracked({0.20F, 1.35F, -0.35F});
        if (armed_hand == 1) {
            input.right_grip.position = {0.20F, 1.05F, -0.10F};
            input.left_grip.position = {-0.20F, 1.35F, -0.35F};
        }
    }

    Pose& Free() { return input.armed_hand == 0 ? input.left_grip : input.right_grip; }
    Pose& Armed() { return input.armed_hand == 0 ? input.right_grip : input.left_grip; }

    MotionReloadGestureOutput Sample(bool held, std::uint64_t ms) {
        input.trigger_held[1U - input.armed_hand] = held;
        input.monotonic_ms = ms;
        ++input.pose_sequence;
        return gesture.Update(input);
    }

    void Start() {
        const auto baseline = Sample(false, 1000);
        Require(!baseline.reload && !baseline.consume_free_trigger, "released baseline stays silent");
        const auto press = Sample(true, 1010);
        Require(!press.reload && press.consume_free_trigger, "source press claims only free trigger");
        Require(press.consumed_trigger_mask == (input.armed_hand == 0 ? 2 : 1), "claim mask identifies physical support hand");
        Require(gesture.stage() == MotionReloadGestureStage::carrying, "source press enters carrying");
    }

    void Contact() { Free().position = Armed().position; }

    // Observe actual intermediate poses, rather than turning a large endpoint
    // jump into evidence of a deliberate carry.
    MotionReloadGestureOutput Move(Pose& grip, Vec3 target, std::uint64_t end, bool held_on_arrival=true) {
        const auto start = grip.position;
        const auto begin = input.monotonic_ms;
        MotionReloadGestureOutput out{};
        for (auto ms = std::min(begin + 10,end);; ms = std::min(ms + 10,end)) {
            const float t = static_cast<float>(ms - begin) / static_cast<float>(end - begin);
            grip.position = {start.x + (target.x-start.x)*t,
                start.y + (target.y-start.y)*t, start.z + (target.z-start.z)*t};
            out = Sample(ms<end||held_on_arrival, ms);
            if(ms==end)break;
        }
        return out;
    }
};

void PrepareCarry(Fixture& f, bool continuation, float gap) {
    f.Start();
    if (continuation) {
        (void)f.Move(f.Free(), f.Armed().position, 1410);
        Require(f.Sample(false,1420).reload, "continuity fixture completes initial native intent");
        f.input.replenish_cartridge=true;
        (void)f.Sample(false,1430);
        f.input.replenish_cartridge=false;
        Require(f.Sample(true,1440).consume_free_trigger, "continuation claims without returning to waist");
    }
    auto target=f.Armed().position;
    target.x+=gap;
    const auto out=f.Move(f.Free(),target,f.input.monotonic_ms+400);
    Require(out.cartridge_held&&!out.reload, "continuity fixture arrives by a sampled held path");
}

bool Silent(const MotionReloadGestureOutput& out) {
    return !out.reload&&!out.insertion_ready&&!out.insertion_zone_entered&&!out.insertion_zone_exited;
}

void GripDiscontinuities() {
    for (const auto hand : {std::uint8_t{0},std::uint8_t{1}}) {
        for (const bool continuation : {false,true}) {
            for (const bool armed_jump : {false,true}) {
                for (const bool release_only : {false,true}) {
                    for (const bool speed_only : {false,true}) {
                        Fixture f(hand);
                        // A 20cm/100ms step violates only the step bound;
                        // a 4cm/10ms step violates only the speed bound. Both
                        // end inside the unchanged 14cm zone.
                        const float distance=speed_only?.04F:.20F;
                        PrepareCarry(f,continuation,speed_only?.16F:.20F);
                        if(armed_jump)f.Armed().position.x+=distance;
                        else f.Free().position.x-=distance;
                        const auto support=static_cast<std::uint8_t>(1-hand);
                        const auto mask=static_cast<std::uint8_t>(1U<<support);
                        auto out=f.Sample(!release_only,f.input.monotonic_ms+(speed_only?10:100));
                        Require(Silent(out)&&!out.cartridge_held&&out.consumed_trigger_mask==mask,
                            "grip discontinuity hides token and suppresses insertion/feedback while consuming physical claim");
                        if(!release_only) {
                            Require(f.gesture.stage()==MotionReloadGestureStage::cancel_held,
                                "discontinuous held carry waits for its physical release");
                            f.input.trigger_held[support]=false;
                            out=f.gesture.Update(f.input);
                            Require(Silent(out)&&out.consumed_trigger_mask==mask,
                                "duplicate release cannot discharge discontinuous claim");
                            f.input.trigger_available[support]=false;
                            out=f.Sample(false,f.input.monotonic_ms+10);
                            Require(Silent(out)&&out.consumed_trigger_mask==mask,
                                "unavailable false cannot discharge discontinuous claim");
                            f.input.trigger_available[support]=true;
                            out=f.Sample(true,f.input.monotonic_ms+10);
                            Require(Silent(out)&&!out.cartridge_held&&out.consumed_trigger_mask==mask,
                                "held reacquisition cannot revive discontinuous token");
                            out=f.Sample(false,f.input.monotonic_ms+10);
                            Require(Silent(out)&&out.consumed_trigger_mask==mask,
                                "available physical release is consumed without delayed insertion");
                        }
                        out=f.Sample(false,f.input.monotonic_ms+10);
                        Require(Silent(out)&&!out.cartridge_held&&out.consumed_trigger_mask==0,
                            "discontinuous claim ends only after an available release");
                    }
                }
            }
        }
    }
}

void GripDuplicateBaselines() {
    for (const auto hand : {std::uint8_t{0},std::uint8_t{1}}) {
        for (const bool continuation : {false,true}) {
            Fixture f(hand);
            PrepareCarry(f,continuation,.16F);
            const auto support=f.Free().position;
            const auto armed=f.Armed().position;
            const auto now=f.input.monotonic_ms;
            // Same sequence with completely different valid geometry cannot
            // move either spatial baseline or its timestamp, or cancel carry.
            for(const auto ms:{now+1,now+8}) {
                f.Free().position.x+=1.F;
                f.Armed().position.y+=1.F;
                f.input.monotonic_ms=ms;
                const auto out=f.gesture.Update(f.input);
                Require(Silent(out)&&out.cartridge_held&&out.cartridge_position.x==support.x,
                    "fresh-age duplicate geometry neither moves baseline nor cancels carry");
            }
            f.Free().position=support;f.Armed().position=armed;
            f.Free().position.x-=.02F;
            Require(f.Sample(true,now+10).cartridge_held,
                "2m/s fresh step is measured from last fresh sample, not duplicate timestamp/geometry");
            f.Free().position.x-=.02F;
            Require(f.Sample(false,now+20).reload,
                "valid carry survives duplicate geometry and inserts exactly once");
            Require(!f.Sample(false,now+30).reload,"duplicate-baseline journey cannot insert twice");
            for(const bool stale:{false,true}) {
                Fixture lost(hand);
                PrepareCarry(lost,continuation,.16F);
                const auto baseline=lost.input.monotonic_ms;
                lost.input.monotonic_ms=stale?baseline+251:baseline-1;
                const auto out=lost.gesture.Update(lost.input);
                const auto mask=static_cast<std::uint8_t>(1U<<(1-hand));
                Require(Silent(out)&&!out.cartridge_held&&out.consumed_trigger_mask==mask,
                    "duplicate stale/regressing clock cancels carry but preserves physical release barrier");
                Require(!lost.Sample(false,baseline+300).reload,
                    "fresh release after duplicate time fault cannot revive insertion");
            }
        }
    }
}

void SampledGripJourneys() {
    for(const auto rate:{90U,120U,144U}) {
        for(const auto hand:{std::uint8_t{0},std::uint8_t{1}}) {
            Fixture f(hand);
            std::uint64_t tick=0;
            const auto sample=[&](bool held) {
                return f.Sample(held,1000+(++tick*1000+rate/2)/rate);
            };
            (void)sample(false);
            Require(sample(true).consume_free_trigger,"sampled waist pickup acquires physical claim");
            const auto start=f.Free().position;
            const auto target=f.Armed().position;
            const unsigned steps=rate/2;
            for(unsigned i=1;i<=steps;++i) {
                const float t=static_cast<float>(i)/steps;
                f.Free().position={start.x+(target.x-start.x)*t,
                    start.y+(target.y-start.y)*t,start.z+(target.z-start.z)*t};
                // Grip-only translation policy must not inherit socket-axis limits.
                f.Free().orientation=i%2?Quaternion{0,1,0,0}:Quaternion{};
                f.Armed().orientation=i%2?Quaternion{1,0,0,0}:Quaternion{};
                const auto out=sample(true);
                Require(out.cartridge_held&&!out.reload,"90/120/144Hz sampled carry stays held without insertion");
            }
            Require(sample(false).reload,"90/120/144Hz initial release requests exactly one round");
            Require(!f.gesture.Update(f.input).reload,"same-sequence release never repeats insertion");
            f.input.replenish_cartridge=true;(void)sample(false);f.input.replenish_cartridge=false;
            Require(sample(true).consume_free_trigger,"sampled continuation claims at gun without a second waist route");
            for(unsigned i=1;i<=steps;++i) {
                const float t=static_cast<float>(i)/steps;
                f.Free().position.x=target.x+.2F*(t<=.5F?2*t:2*(1-t));
                const auto out=sample(true);
                Require(out.cartridge_held&&!out.reload,"sampled continuation excursion retains one claimed token");
            }
            Require(sample(false).reload,"90/120/144Hz continuation release requests exactly one round");
            Require(!sample(false).reload,"sampled continuation never repeats release intent");
        }
    }
}

// Removing release completion or using the wrong hand must break these literal journeys.
void InsertionReadiness(){
    for(const auto hand:{std::uint8_t{0},std::uint8_t{1}}){
        Fixture f(hand);f.Start();
        const auto start=f.Free().position,gun=f.Armed().position;
        const Vec3 delta{start.x-gun.x,start.y-gun.y,start.z-gun.z};
        const float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
        const auto at=[&](float distance){return Vec3{gun.x+delta.x*distance/length,
            gun.y+delta.y*distance/length,gun.z+delta.z*distance/length};};
        (void)f.Move(f.Free(),at(.15F),1410);
        f.Free().position=at(.13F);
        auto out=f.Sample(true,1420);
        Require(out.insertion_ready&&out.insertion_zone_entered&&!out.reload,
            "completed held approach reports ready zone without transferring ammunition");
        out=f.gesture.Update(f.input);
        Require(!out.insertion_zone_entered&&!out.reload,"duplicate poses cannot repeat zone-entry event");
        Require(!f.Sample(true,1430).insertion_zone_entered,"remaining in zone stays silent");
        f.Free().position=at(.15F);
        out=f.Sample(true,1440);
        Require(!out.insertion_ready&&!out.insertion_zone_entered,"outside insertion radius is not ready");
        f.Free().position=at(.13F);
        Require(!f.Sample(true,1450).insertion_zone_entered,"boundary jitter cannot repulse zone entry");
        f.Free().position=at(.20F);
        Require(f.Sample(true,1500).insertion_zone_exited,"deliberate withdrawal reports one zone exit");
        f.Free().position=at(.13F);
        Require(f.Sample(true,1550).insertion_zone_entered,"deliberate withdrawal rearms zone feedback");
        out=f.Sample(false,1560);
        Require(out.reload&&!out.insertion_ready&&!out.insertion_zone_entered,
            "release requests one round and never reports a held ready cartridge");
    }
    Fixture not_ready;not_ready.Armed().position={.09F,1.05F,-.10F};not_ready.Start();
    Require(!not_ready.Move(not_ready.Free(),not_ready.Armed().position,1109).insertion_ready,
        "too-fast pickup journey cannot advertise insertion");
    not_ready.input.eligible=false;
    Require(!not_ready.Sample(true,1110).insertion_ready,"cancelled cartridge cannot advertise insertion");
}

void Journeys() {
    for (const auto hand : {std::uint8_t{0}, std::uint8_t{1}}) {
        Fixture f(hand);
        f.Start();
        const auto carry = f.Move(f.Free(), f.Armed().position, 1410);
        Require(!carry.reload && carry.consume_free_trigger, "held contact never reloads");
        const auto release = f.Sample(false, 1420);
        Require(release.reload && release.consume_free_trigger, "literal mirrored journey reloads on release once");
        Require(!f.Sample(false, 1430).reload, "subsequent released sample cannot repeat reload");
    }
}

// Lost input/owner/pose must discard the journey, retaining a held release barrier.
void CancellationFaults() {
    for (int fault = 0; fault < 27; ++fault) {
        Fixture f;
        f.Start();
        (void)f.Move(f.Free(),f.Armed().position,1410);
        auto bad = f.input;
        ++bad.pose_sequence;
        bad.monotonic_ms += 10;
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        switch (fault) {
        case 0: bad.valid = false; break;
        case 1: bad.eligible = false; break;
        case 2: bad.trigger_available[1] = false; break;
        case 3: ++bad.player_identity; break;
        case 4: ++bad.weapon_identity; break;
        case 5: bad.armed_hand = 1; break;
        case 6: ++bad.input_generation; break;
        case 7: bad.head.position_valid = false; break;
        case 8: bad.head.orientation_valid = false; break;
        case 9: bad.left_grip.position_valid = false; break;
        case 10: bad.right_grip.orientation_valid = false; break;
        case 11: bad.head.position.x = nan; break;
        case 12: bad.left_grip.orientation.w = nan; break;
        case 13: bad.right_grip.position.z = std::numeric_limits<float>::infinity(); break;
        case 14: bad.head.orientation = {0, 0, 0, 0}; break;
        case 15: bad.pose_sequence = 1; break;
        case 16: bad.monotonic_ms = 1009; break;
        case 17: bad.monotonic_ms = f.input.monotonic_ms+251; break;
        case 18: bad.armed_hand = 2; break;
        case 19: bad.player_identity = 0; break;
        case 20: bad.weapon_identity = 0; break;
        case 21: bad.left_grip.orientation_valid = false; break;
        case 22: bad.right_grip.position_valid = false; break;
        case 23: bad.left_grip.orientation = {0, 0, 0, 0}; break;
        case 24: bad.right_grip.orientation = {0, 0, 0, 0}; break;
        case 25: bad.head.orientation.z = nan; break;
        case 26: bad.right_grip.orientation.x = nan; break;
        }
        const auto cancelled = f.gesture.Update(bad);
        Require(!cancelled.reload, "fault cancels without reload");
        Require(cancelled.consume_free_trigger, "cancelled claimed hold retains free-trigger barrier");
        f.input.pose_sequence = std::max(f.input.pose_sequence,bad.pose_sequence)+1;
        const auto recovery_at=std::max(f.input.monotonic_ms,bad.monotonic_ms)+50;
        Require(!f.Sample(true, recovery_at).reload, "reacquired held contact cannot finish old journey");
        Require(!f.Sample(false, recovery_at+10).reload, "release after loss cannot deliver delayed reload");
        f.Free().position = {-0.20F, 1.05F, -0.10F};
        Require(f.Sample(true, recovery_at+20).consume_free_trigger, "fresh press after available release rearms");
        Require(f.Move(f.Free(), f.Armed().position, recovery_at+420, false).reload,
            "fresh complete journey recovers after cancellation");
    }
}

void RearmingAndOwnership() {
    Fixture held;
    Require(!held.Sample(true, 1000).reload, "initial held source cannot start");
    Require(!held.Sample(true, 1100).consume_free_trigger, "unclaimed held input remains independent");
    (void)held.Sample(false, 1110);
    Require(held.Sample(true, 1120).consume_free_trigger, "released baseline permits next source press");

    Fixture outside;
    (void)outside.Sample(false, 1000);
    outside.Free().position = {0, 1.5F, -0.4F};
    Require(!outside.Sample(true, 1010).consume_free_trigger, "press outside source is not owned");
    outside.Free().position = {-0.20F, 1.05F, -0.10F};
    Require(!outside.Sample(true, 1110).consume_free_trigger, "held arrival at source is not a fresh press");

    Fixture unavailable;
    unavailable.input.trigger_available[1] = false;
    (void)unavailable.Sample(false, 1000);
    unavailable.input.trigger_available[1] = true;
    Require(!unavailable.Sample(true, 1010).consume_free_trigger, "unavailable release cannot establish baseline");

    Fixture cancel;
    cancel.Start();
    const auto miss = cancel.Move(cancel.Free(), {0, 1.5F, 0}, 1410, false);
    Require(!miss.reload && miss.consume_free_trigger, "claimed release away from armed grip cancels and is consumed");
    Require(!cancel.Sample(false, 1420).consume_free_trigger, "consumption ends after observed release");
}

void Freshness() {
    Fixture f;
    (void)f.Sample(false, 1000);
    f.input.trigger_held[1] = true;
    f.input.monotonic_ms = 1010;
    Require(!f.gesture.Update(f.input).consume_free_trigger, "duplicate sequence cannot create press edge");
    Require(f.Sample(true, 1020).consume_free_trigger, "fresh sequence observes source press");
    (void)f.Move(f.Free(),f.Armed().position,1420);
    f.input.trigger_held[1] = false;
    f.input.monotonic_ms = 1430;
    Require(!f.gesture.Update(f.input).reload, "duplicate sequence cannot complete release edge");
    Require(f.Sample(false, 1440).reload, "fresh sequence completes release edge");

    for (const auto time : {std::uint64_t{1009}, std::uint64_t{1261}}) {
        Fixture stale;
        stale.Start();
        stale.input.monotonic_ms = time;
        const auto cancelled=stale.gesture.Update(stale.input);
        Require(Silent(cancelled)&&!cancelled.cartridge_held&&cancelled.consume_free_trigger,
            "duplicate clock regression/gap cancels presentation and retains physical claim");
        stale.Contact();
        Require(!stale.Sample(false, 1300).reload, "duplicate time fault forbids delayed completion");
    }

    Fixture gap;
    gap.Start();
    auto target=gap.Armed().position;target.x+=.16F;
    (void)gap.Move(gap.Free(),target,1410);
    gap.Free().position.x-=.04F;
    Require(gap.Sample(false, 1660).reload, "250ms fresh sample gap is accepted within translation bounds");
}

void PhysicalClaimSurvivesOwnerLoss() {
    for (const auto original_hand : {std::uint8_t{0}, std::uint8_t{1}}) {
        for (const bool invalid_reader : {false, true}) {
            Fixture f(original_hand);
            f.Start();
            const auto claimed = static_cast<std::uint8_t>(1U - original_hand);
            const auto mask = static_cast<std::uint8_t>(1U << claimed);
            f.input.armed_hand = invalid_reader ? 0 : claimed;
            f.input.valid = f.input.eligible = !invalid_reader;
            ++f.input.weapon_identity;
            f.input.pose_sequence = 3;
            f.input.monotonic_ms = 1110;
            auto result = f.gesture.Update(f.input);
            Require(!result.reload && result.consumed_trigger_mask == mask, "hand switch/invalid default preserves original physical claim");
            Require(f.gesture.stage() == MotionReloadGestureStage::cancel_held, "lost owner enters physical release barrier");
            // Both held: the unclaimed input stays independent, even after switch.
            f.input.trigger_held = {true, true};
            f.input.trigger_available[claimed] = false;
            ++f.input.pose_sequence; f.input.monotonic_ms = 1120;
            result = f.gesture.Update(f.input);
            Require(result.consumed_trigger_mask == mask, "unavailable original trigger keeps only its physical claim");
            f.input.trigger_held[claimed] = false;
            ++f.input.pose_sequence; f.input.monotonic_ms = 1130;
            result = f.gesture.Update(f.input);
            Require(result.consumed_trigger_mask == mask, "unavailable false is not release of original trigger");
            f.input.valid = f.input.eligible = true;
            f.input.trigger_available[claimed] = true;
            ++f.input.pose_sequence; f.input.monotonic_ms = 1140;
            result = f.gesture.Update(f.input);
            Require(!result.reload && result.consumed_trigger_mask == mask, "original available release is consumed without delayed completion");
            ++f.input.pose_sequence; f.input.monotonic_ms = 1150;
            result = f.gesture.Update(f.input);
            Require(!result.reload && result.consumed_trigger_mask == 0, "physical claim ends after available release");
        }
    }
}

void GeometryAndTiming() {
    Fixture fast;
    fast.Armed().position={.09F,1.05F,-.10F};
    fast.Start();
    Require(!fast.Move(fast.Free(),fast.Armed().position,1109,false).reload,
        "journey shorter than 100ms rejected despite continuous translation");
    Fixture minimum;
    minimum.Armed().position={.09F,1.05F,-.10F};
    minimum.Start();
    Require(minimum.Move(minimum.Free(),minimum.Armed().position,1110,false).reload,
        "100ms journey accepted with continuous translation");

    Fixture timeout;
    timeout.Start();
    (void)timeout.Move(timeout.Free(),timeout.Armed().position,1410);
    for (std::uint64_t ms = 1660; ms < 6010; ms += 250) (void)timeout.Sample(true, ms);
    Require(!timeout.Sample(false, 6011).reload, "journey longer than five seconds rejected");

    Fixture maximum;
    maximum.Start();
    (void)maximum.Move(maximum.Free(),maximum.Armed().position,1410);
    for (std::uint64_t ms = 1660; ms < 6010; ms += 250) (void)maximum.Sample(true, ms);
    Require(maximum.Sample(false, 6010).reload, "five-second journey accepted with uninterrupted fresh samples");

    Fixture overlap;
    overlap.Armed().position = {-0.20F, 1.05F, -0.10F};
    overlap.Start();
    Require(!overlap.Sample(false, 1120).reload, "overlapping hands cannot skip departure and displacement");

    Fixture moved_gun;
    moved_gun.Start();
    Require(!moved_gun.Move(moved_gun.Armed(),moved_gun.Free().position,1410,false).reload,
        "gun motion is not actual support-hand displacement");

    Fixture head_motion;
    head_motion.Start();
    head_motion.input.head.position.x = 1.0F;
    Require(!head_motion.Move(head_motion.Armed(),head_motion.Free().position,1410,false).reload,
        "head motion alone cannot manufacture support displacement");

    Fixture too_short;
    too_short.Free().position = {-0.01F, 1.05F, -0.10F};
    too_short.Armed().position = {.18F, 1.05F, -.10F};
    too_short.Start();
    Require(!too_short.Move(too_short.Free(),too_short.Armed().position,1410,false).reload,
        "leaving source still requires 20cm actual support displacement");

    Fixture no_departure;
    no_departure.Armed().position = {.01F, 1.05F, -.10F};
    no_departure.Start();
    Require(!no_departure.Move(no_departure.Free(),no_departure.Armed().position,1410,false).reload,
        "20cm displacement and contact cannot bypass 28cm source exit");

    Fixture departure;
    departure.Armed().position = {.01F, 1.05F, -.10F};
    departure.Start();
    (void)departure.Move(departure.Free(),{.09F,1.05F,-.10F},1310);
    Require(departure.Move(departure.Free(),departure.Armed().position,1410,false).reload,
        "observed held departure remains latched on return toward source");

    Fixture miss;
    miss.Start();
    auto miss_target=miss.Armed().position;miss_target.x+=.141F;
    Require(!miss.Move(miss.Free(),miss_target,1410,false).reload, "release outside 14cm contact radius rejected");

    Fixture near;
    near.Start();
    auto near_target=near.Armed().position;near_target.x+=.139F;
    Require(near.Move(near.Free(),near_target,1410,false).reload, "release inside contact radius accepted");

    Fixture source_miss;
    (void)source_miss.Sample(false, 1000);
    source_miss.Free().position.x = -0.401F;
    Require(!source_miss.Sample(true, 1010).consume_free_trigger, "source outside 20cm radius rejected");

    Fixture source_edge;
    source_edge.Free().position.x = -0.399F;
    source_edge.Start();
    Require(source_edge.Move(source_edge.Free(),source_edge.Armed().position,1410,false).reload,
        "press inside source radius accepted");

    Fixture high;
    (void)high.Sample(false, 1000);
    high.Free().position = {-0.20F, 1.31F, -0.10F};
    Require(!high.Sample(true, 1010).consume_free_trigger, "source above minimum 30cm head drop rejected");

    // 90-degree yaw: forward -X, right -Z; left-side source is +Z.
    // Scaled quaternion verifies normalization instead of assuming unit input.
    Fixture yaw;
    yaw.input.head.orientation = {0, 1.41421356F, 0, 1.41421356F};
    yaw.Free().position = {-0.10F, 1.05F, 0.20F};
    yaw.Armed().position = {-0.35F, 1.35F, -0.20F};
    yaw.Start();
    Require(yaw.Move(yaw.Free(),yaw.Armed().position,1410,false).reload,
        "normalized head yaw rotates waist source");

    // yaw 90, pitch 60, roll 30: projected forward still -X.
    Fixture tilted;
    tilted.input.head.orientation = {0.5F, 0.5F, -0.1830127F, 0.6830127F};
    tilted.Free().position = {-0.10F, 1.05F, 0.20F};
    tilted.Armed().position = {-0.35F, 1.35F, -0.20F};
    tilted.Start();
    Require(tilted.Move(tilted.Free(),tilted.Armed().position,1410,false).reload,
        "pitch and roll do not tilt or shorten yaw-relative source");

    Fixture vertical;
    vertical.input.head.orientation = {0.70710678F, 0, 0, 0.70710678F};
    (void)vertical.Sample(false, 1000);
    Require(!vertical.Sample(true, 1010).consume_free_trigger, "vertical forward has no valid planar yaw");

    Fixture translated;
    translated.input.head.position = {2.0F, 2.0F, 3.0F};
    translated.Free().position = {1.80F, 1.45F, 2.90F};
    translated.Armed().position = {2.20F, 1.75F, 2.65F};
    translated.Start();
    Require(translated.Move(translated.Free(),translated.Armed().position,1410,false).reload,
        "source follows translated HMD instead of world origin");
}
// Explicit insertion uses the cartridge tip, not controller grip proximity.
void InsertionTargets() {
    for (const auto hand : {std::uint8_t{0}, std::uint8_t{1}}) {
        for (int fault = -1; fault < 12; ++fault) {
            Fixture f(hand);
            f.input.insertion_target = Tracked({0.0F, 1.45F, -0.50F});
            f.input.cartridge_tip = f.Free();
            f.Start();
            // Travel to a rear approach sample; the grip stays away from socket.
            for (int i = 1; i <= 8; ++i) {
                const float t = static_cast<float>(i) / 8.0F;
                f.Free().position = {hand == 0 ? -0.20F : 0.20F, 1.05F + .40F*t, -.10F - .30F*t};
                f.input.cartridge_tip->position = {(hand == 0 ? -.20F : .20F)*(1-t), 1.05F+.40F*t, -.10F-.45F*t};
                (void)f.Sample(true, 1010 + i*50);
            }
            if (fault == 0) f.input.cartridge_tip->position.x = .04F; // lateral miss
            if (fault == 1) f.input.cartridge_tip->orientation = {0,1,0,0}; // reversed axis
            if (fault == 2) f.input.cartridge_tip.reset();
            if (fault == 3) f.input.cartridge_tip->position_valid = false;
            if (fault == 4) f.input.cartridge_tip->position.z = -.20F; // teleport
            if (fault == 5) f.input.insertion_target.reset(); // cannot switch to fallback
            if (fault == 6) f.input.cartridge_tip->orientation = {0,0,0,0};
            if (fault == 7) f.input.cartridge_tip->position.x = std::numeric_limits<float>::quiet_NaN();
            if (fault == 8) f.input.cartridge_tip->orientation_valid = false;
            if (fault == 9) f.input.cartridge_tip->orientation = {0,.70710678F,0,.70710678F};
            if (fault == 10) f.input.insertion_target->position.z -= .30F; // socket teleport
            (void)f.Sample(true, 1460);
            if (f.input.cartridge_tip && fault != 4) f.input.cartridge_tip->position.z = -.49F;
            const auto entry = f.Sample(fault != 11, 1510);
            Require(!entry.reload, "entry must be held and never generates reload intent");
            Require(entry.insertion_ready==(fault==-1)&&entry.insertion_zone_entered==(fault==-1),
                "explicit-zone feedback requires the same aligned admitted trajectory as insertion");
            const auto release = f.Sample(false, 1520);
            Require(release.reload == (fault == -1), "only coherent aligned held tip entry then release inserts");
            Require(!f.Sample(false, 1530).reload, "explicit insertion cannot auto-repeat");
        }
        Fixture contact(hand);
        contact.input.insertion_target = contact.Armed();
        contact.input.cartridge_tip = contact.Armed();
        contact.Start(); contact.Contact();
        (void)contact.Sample(true,1110);
        Require(!contact.Sample(false,1120).reload, "tip already inside on press cannot manufacture entry");
    }

    for (const bool teleport : {false, true}) {
        Fixture f;
        f.input.insertion_target = Tracked({0,1.45F,-.50F});
        f.input.cartridge_tip = Tracked({0,1.45F,-.55F});
        (void)f.Sample(false,1000);
        f.input.replenish_cartridge = true;
        (void)f.Sample(false,1010);
        f.input.replenish_cartridge = false;
        (void)f.Sample(true,1020);
        f.input.cartridge_tip->position.z = teleport ? -.20F : -.49F;
        Require(!f.Sample(true,1070).reload, "replenished held entry stays silent");
        Require(f.Sample(false,1080).reload == !teleport, "replenished token requires same bounded tip entry");
        Require(!f.Sample(false,1090).reload, "replenished target insertion does not repeat");
    }
}

void TipTrajectoryDetails() {
    for (int scenario = 0; scenario < 8; ++scenario) {
        Fixture f;
        f.input.insertion_target = Tracked({0,1.45F,-.50F});
        f.input.cartridge_tip = Tracked({0,1.45F,-.55F});
        if (scenario == 6) {
            // Rotate both authored axes +90deg about Y: insertion now runs +X.
            f.input.insertion_target->orientation = {0,1.41421356F,0,1.41421356F};
            f.input.cartridge_tip->orientation = f.input.insertion_target->orientation;
            f.input.cartridge_tip->position = {-.05F,1.45F,-.50F};
        }
        (void)f.Sample(false,1000);
        f.input.replenish_cartridge = true;
        (void)f.Sample(false,1010);
        f.input.replenish_cartridge = false;
        (void)f.Sample(true,1020);
        for (int i = 1; i <= 12; ++i) {
            if (scenario == 1) f.input.insertion_target->position.z = -.50F-.005F*i;
            else if (scenario == 6) f.input.cartridge_tip->position.x = -.05F+.005F*i;
            else f.input.cartridge_tip->position.z = -.55F+.005F*i;
            (void)f.Sample(true,1020+i*10);
        }
        if (scenario == 2) f.input.cartridge_tip->position.x = .03F;
        if (scenario == 3) f.input.cartridge_tip->position.z = -.60F;
        if (scenario == 4) f.input.cartridge_tip->orientation = {0,1,0,0};
        if (scenario == 5) f.input.insertion_target->position.z = -.40F;
        if (scenario == 7) {
            f.input.cartridge_tip->position.z = -.55F;
            (void)f.Sample(true,1150);
            f.input.cartridge_tip->position.z = -.49F;
        }
        Require(f.Sample(false,1200).reload == (scenario == 0 || scenario == 6),
            "small held steps insert; socket-only motion and incoherent release do not");
    }
}

void InvalidInsertionTargets() {
    for (int fault = 0; fault < 10; ++fault) {
        Fixture f;
        f.input.insertion_target = f.Armed();
        f.input.cartridge_tip = f.Free();
        f.Start(); f.Contact();
        auto& target = *f.input.insertion_target;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        switch (fault) {
        case 0: target.position_valid = false; break;
        case 1: target.orientation_valid = false; break;
        case 2: target.position.x = nan; break;
        case 3: target.position.y = nan; break;
        case 4: target.position.z = std::numeric_limits<float>::infinity(); break;
        case 5: target.orientation.x = nan; break;
        case 6: target.orientation.y = nan; break;
        case 7: target.orientation.z = nan; break;
        case 8: target.orientation.w = std::numeric_limits<float>::infinity(); break;
        case 9: target.orientation = {0, 0, 0, 0}; break;
        }
        const auto invalid_target = target;
        const auto cancel = f.Sample(true, 1110);
        Require(!cancel.reload && !cancel.cartridge_held && cancel.claimed_hand == 1,
            "invalid supplied target cancels presentation but preserves physical hand");
        Require(cancel.consume_free_trigger && f.gesture.stage() == MotionReloadGestureStage::cancel_held,
            "invalid target fails closed without grip fallback");
        f.input.insertion_target = f.Armed();
        Require(!f.Sample(false, 1120).reload, "restored target cannot complete cancelled journey");

        Fixture initial;
        initial.input.insertion_target = invalid_target;
        (void)initial.Sample(false, 1000);
        Require(!initial.Sample(true, 1010).consume_free_trigger, "invalid target cannot start pickup");
    }
}

void CartridgePresentation() {
    // Appended metadata must not change the original aggregate field ordering.
    const MotionReloadGestureOutput aggregate{false, true, 2};
    Require(!aggregate.cartridge_held && aggregate.claimed_hand == 2,
        "legacy output aggregate retains default unclaimed presentation");
    for (const auto hand : {std::uint8_t{0}, std::uint8_t{1}}) {
        Fixture f(hand);
        const auto idle = f.Sample(false, 1000);
        Require(!idle.cartridge_held && idle.claimed_hand == 2, "idle has no presentation hand");
        auto output = f.Sample(true, 1010);
        const auto claimed = static_cast<std::uint8_t>(1U - hand);
        Require(output.cartridge_held && output.claimed_hand == claimed, "pickup exposes physical carrying hand");
        Require(output.cartridge_position.x == f.Free().position.x &&
            output.cartridge_position.y == f.Free().position.y &&
            output.cartridge_position.z == f.Free().position.z, "pickup position is exact XR support grip without offsets");
        output = f.Move(f.Free(),f.Armed().position,1410);
        Require(output.cartridge_held && output.claimed_hand == claimed &&
            output.cartridge_position.x == f.Free().position.x &&
            output.cartridge_position.y == f.Free().position.y &&
            output.cartridge_position.z == f.Free().position.z, "cartridge follows fresh physical hand during travel");
        f.Free().position.x += 1.0F;
        const auto duplicate = f.gesture.Update(f.input);
        Require(duplicate.cartridge_position.x == output.cartridge_position.x,
            "duplicate pose cannot move cartridge presentation");
        f.Contact();
        output = f.Sample(false, 1420);
        Require(output.reload && !output.cartridge_held && output.claimed_hand == claimed,
            "release ends presentation and consumes original physical hand once");
        Require(f.gesture.stage() == MotionReloadGestureStage::ready, "stage readout is ready after insertion");
        output = f.Sample(false, 1430);
        Require(!output.cartridge_held && output.claimed_hand == 2, "after release presentation is unclaimed");
    }
}

void ReplenishedCartridgeSession() {
    Fixture f;
    f.Start();
    const auto first = f.Move(f.Free(),f.Armed().position,1410,false);
    Require(first.reload && !first.cartridge_held,
        "first waist journey requests one native round and consumes its token");

    f.input.replenish_cartridge = true;
    auto output = f.Sample(false, 1420);
    f.input.replenish_cartridge = false;
    Require(!output.reload && output.cartridge_held && !output.consume_free_trigger,
        "accepted native continuation replenishes an unclaimed cartridge in support hand");
    Require(output.claimed_hand == 2 && output.cartridge_hand == 1,
        "replenished presentation does not claim trigger before a fresh press");

    output = f.Sample(true, 1430);
    Require(output.cartridge_held && output.consume_free_trigger && output.claimed_hand == 1,
        "fresh trigger press claims replenished cartridge");
    auto away=f.Free().position;away.x+=.20F;
    output = f.Move(f.Free(),away,1630,false);
    Require(!output.reload && output.cartridge_held && output.cartridge_hand == 1,
        "missed replenished insertion keeps cartridge available for another attempt");
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "missed release is consumed exactly once by its physical claim");

    output = f.Sample(false, 1640);
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "replenished cartridge remains visible after missed release without retaining trigger claim");
    output = f.Sample(true, 1650);
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "second fresh press reclaims retained cartridge");
    (void)f.Move(f.Free(),f.Armed().position,1850);
    output = f.Sample(false, 1860);
    Require(output.reload && !output.cartridge_held,
        "replenished cartridge requests exactly one additional round on insertion release");
    Require(!f.Sample(false, 1870).reload,
        "replenished insertion never autonomously repeats native reload intent");

    Fixture held;
    held.Start();
    Require(held.Move(held.Free(),held.Armed().position,1410,false).reload,
        "held-during-reload fixture completes the initial cartridge");
    held.input.replenish_cartridge = true;
    output = held.Sample(true, 1420);
    held.input.replenish_cartridge = false;
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "replenishment while trigger is already held stays presentation-only");
    output = held.Sample(false, 1430);
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "fresh release after replenishment arms the next deliberate press");
    output = held.Sample(true, 1440);
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "only a fresh post-replenishment press claims the cartridge");
}
} // namespace

int main() {
    GripDiscontinuities();
    GripDuplicateBaselines();
    SampledGripJourneys();
    InsertionReadiness();
    Journeys();
    CancellationFaults();
    RearmingAndOwnership();
    Freshness();
    PhysicalClaimSurvivesOwnerLoss();
    GeometryAndTiming();
    InsertionTargets();
    TipTrajectoryDetails();
    InvalidInsertionTargets();
    CartridgePresentation();
    ReplenishedCartridgeSession();
    if (failures != 0) {
        std::cerr << failures << " reload gesture checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "reload gesture checks passed\n";
    return EXIT_SUCCESS;
}
