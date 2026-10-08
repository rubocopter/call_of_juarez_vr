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
};

// Removing release completion or using the wrong hand must break these literal journeys.
void Journeys() {
    for (const auto hand : {std::uint8_t{0}, std::uint8_t{1}}) {
        Fixture f(hand);
        f.Start();
        f.Contact();
        const auto carry = f.Sample(true, 1110);
        Require(!carry.reload && carry.consume_free_trigger, "held contact never reloads");
        const auto release = f.Sample(false, 1120);
        Require(release.reload && release.consume_free_trigger, "literal mirrored journey reloads on release once");
        Require(!f.Sample(false, 1130).reload, "subsequent released sample cannot repeat reload");
    }
}

// Lost input/owner/pose must discard the journey, retaining a held release barrier.
void CancellationFaults() {
    for (int fault = 0; fault < 27; ++fault) {
        Fixture f;
        f.Start();
        f.Contact();
        auto bad = f.input;
        bad.pose_sequence = 3;
        bad.monotonic_ms = 1110;
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
        case 17: bad.monotonic_ms = 1261; break;
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
        f.input.pose_sequence = 4;
        Require(!f.Sample(true, 1300).reload, "reacquired held contact cannot finish old journey");
        Require(!f.Sample(false, 1310).reload, "release after loss cannot deliver delayed reload");
        f.Free().position = {-0.20F, 1.05F, -0.10F};
        Require(f.Sample(true, 1320).consume_free_trigger, "fresh press after available release rearms");
        f.Contact();
        Require(f.Sample(false, 1430).reload, "fresh complete journey recovers after cancellation");
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
    cancel.Free().position = {0, 1.5F, 0};
    const auto miss = cancel.Sample(false, 1120);
    Require(!miss.reload && miss.consume_free_trigger, "claimed release away from armed grip cancels and is consumed");
    Require(!cancel.Sample(false, 1130).consume_free_trigger, "consumption ends after observed release");
}

void Freshness() {
    Fixture f;
    (void)f.Sample(false, 1000);
    f.input.trigger_held[1] = true;
    f.input.monotonic_ms = 1010;
    Require(!f.gesture.Update(f.input).consume_free_trigger, "duplicate sequence cannot create press edge");
    Require(f.Sample(true, 1020).consume_free_trigger, "fresh sequence observes source press");
    f.Contact();
    f.input.trigger_held[1] = false;
    f.input.monotonic_ms = 1120;
    Require(!f.gesture.Update(f.input).reload, "duplicate sequence cannot complete release edge");
    Require(f.Sample(false, 1130).reload, "fresh sequence completes release edge");

    for (const auto time : {std::uint64_t{1009}, std::uint64_t{1261}}) {
        Fixture stale;
        stale.Start();
        stale.input.monotonic_ms = time;
        Require(!stale.gesture.Update(stale.input).reload, "duplicate clock regression/gap cancels");
        stale.Contact();
        Require(!stale.Sample(false, 1300).reload, "duplicate time fault forbids delayed completion");
    }

    Fixture gap;
    gap.Start();
    gap.Contact();
    Require(gap.Sample(false, 1260).reload, "250ms fresh sample gap is accepted");
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
    fast.Start(); fast.Contact();
    Require(!fast.Sample(false, 1109).reload, "journey shorter than 100ms rejected");
    Fixture minimum;
    minimum.Start(); minimum.Contact();
    Require(minimum.Sample(false, 1110).reload, "100ms journey accepted");

    Fixture timeout;
    timeout.Start();
    for (std::uint64_t ms = 1260; ms <= 6010; ms += 250) (void)timeout.Sample(true, ms);
    timeout.Contact();
    Require(!timeout.Sample(false, 6011).reload, "journey longer than five seconds rejected");

    Fixture maximum;
    maximum.Start();
    for (std::uint64_t ms = 1260; ms < 6010; ms += 250) (void)maximum.Sample(true, ms);
    maximum.Contact();
    Require(maximum.Sample(false, 6010).reload, "five-second journey accepted with uninterrupted fresh samples");

    Fixture overlap;
    overlap.Armed().position = {-0.20F, 1.05F, -0.10F};
    overlap.Start();
    Require(!overlap.Sample(false, 1120).reload, "overlapping hands cannot skip departure and displacement");

    Fixture moved_gun;
    moved_gun.Start();
    moved_gun.Armed().position = moved_gun.Free().position;
    Require(!moved_gun.Sample(false, 1120).reload, "gun motion is not actual support-hand displacement");

    Fixture head_motion;
    head_motion.Start();
    head_motion.input.head.position.x = 1.0F;
    head_motion.Armed().position = head_motion.Free().position;
    Require(!head_motion.Sample(false, 1120).reload, "head motion alone cannot manufacture support displacement");

    Fixture too_short;
    too_short.Free().position = {-0.01F, 1.05F, -0.10F};
    too_short.Start();
    too_short.Free().position = {0.18F, 1.05F, -0.10F};
    too_short.Armed().position = too_short.Free().position;
    Require(!too_short.Sample(false, 1120).reload, "leaving source still requires 20cm actual support displacement");

    Fixture no_departure;
    no_departure.Start();
    no_departure.Free().position = {0.01F, 1.05F, -0.10F};
    no_departure.Armed().position = no_departure.Free().position;
    Require(!no_departure.Sample(false, 1120).reload, "20cm displacement and contact cannot bypass 28cm source exit");

    Fixture departure;
    departure.Start();
    departure.Free().position = {0.09F, 1.05F, -0.10F};
    (void)departure.Sample(true, 1110);
    departure.Free().position = {0.01F, 1.05F, -0.10F};
    departure.Armed().position = departure.Free().position;
    Require(departure.Sample(false, 1120).reload, "observed held departure remains latched on return toward source");

    Fixture miss;
    miss.Start(); miss.Contact();
    miss.Free().position.x += 0.141F;
    Require(!miss.Sample(false, 1120).reload, "release outside 14cm contact radius rejected");

    Fixture near;
    near.Start(); near.Contact();
    near.Free().position.x += 0.139F;
    Require(near.Sample(false, 1120).reload, "release inside contact radius accepted");

    Fixture source_miss;
    (void)source_miss.Sample(false, 1000);
    source_miss.Free().position.x = -0.401F;
    Require(!source_miss.Sample(true, 1010).consume_free_trigger, "source outside 20cm radius rejected");

    Fixture source_edge;
    source_edge.Free().position.x = -0.399F;
    source_edge.Start(); source_edge.Contact();
    Require(source_edge.Sample(false, 1120).reload, "press inside source radius accepted");

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
    yaw.Start(); yaw.Contact();
    Require(yaw.Sample(false, 1120).reload, "normalized head yaw rotates waist source");

    // yaw 90, pitch 60, roll 30: projected forward still -X.
    Fixture tilted;
    tilted.input.head.orientation = {0.5F, 0.5F, -0.1830127F, 0.6830127F};
    tilted.Free().position = {-0.10F, 1.05F, 0.20F};
    tilted.Armed().position = {-0.35F, 1.35F, -0.20F};
    tilted.Start(); tilted.Contact();
    Require(tilted.Sample(false, 1120).reload, "pitch and roll do not tilt or shorten yaw-relative source");

    Fixture vertical;
    vertical.input.head.orientation = {0.70710678F, 0, 0, 0.70710678F};
    (void)vertical.Sample(false, 1000);
    Require(!vertical.Sample(true, 1010).consume_free_trigger, "vertical forward has no valid planar yaw");

    Fixture translated;
    translated.input.head.position = {2.0F, 2.0F, 3.0F};
    translated.Free().position = {1.80F, 1.45F, 2.90F};
    translated.Armed().position = {2.20F, 1.75F, 2.65F};
    translated.Start(); translated.Contact();
    Require(translated.Sample(false, 1120).reload, "source follows translated HMD instead of world origin");
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
        f.Contact();
        output = f.Sample(true, 1110);
        Require(output.cartridge_held && output.claimed_hand == claimed &&
            output.cartridge_position.x == f.Free().position.x &&
            output.cartridge_position.y == f.Free().position.y &&
            output.cartridge_position.z == f.Free().position.z, "cartridge follows fresh physical hand during travel");
        f.Free().position.x += 1.0F;
        const auto duplicate = f.gesture.Update(f.input);
        Require(duplicate.cartridge_position.x == output.cartridge_position.x,
            "duplicate pose cannot move cartridge presentation");
        f.Contact();
        output = f.Sample(false, 1120);
        Require(output.reload && !output.cartridge_held && output.claimed_hand == claimed,
            "release ends presentation and consumes original physical hand once");
        Require(f.gesture.stage() == MotionReloadGestureStage::ready, "stage readout is ready after insertion");
        output = f.Sample(false, 1130);
        Require(!output.cartridge_held && output.claimed_hand == 2, "after release presentation is unclaimed");
    }
}

void ReplenishedCartridgeSession() {
    Fixture f;
    f.Start();
    f.Contact();
    const auto first = f.Sample(false, 1120);
    Require(first.reload && !first.cartridge_held,
        "first waist journey requests one native round and consumes its token");

    f.input.replenish_cartridge = true;
    auto output = f.Sample(false, 1130);
    f.input.replenish_cartridge = false;
    Require(!output.reload && output.cartridge_held && !output.consume_free_trigger,
        "accepted native continuation replenishes an unclaimed cartridge in support hand");
    Require(output.claimed_hand == 2 && output.cartridge_hand == 1,
        "replenished presentation does not claim trigger before a fresh press");

    output = f.Sample(true, 1140);
    Require(output.cartridge_held && output.consume_free_trigger && output.claimed_hand == 1,
        "fresh trigger press claims replenished cartridge");
    f.Free().position.x += 0.20F;
    output = f.Sample(false, 1150);
    Require(!output.reload && output.cartridge_held && output.cartridge_hand == 1,
        "missed replenished insertion keeps cartridge available for another attempt");
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "missed release is consumed exactly once by its physical claim");

    output = f.Sample(false, 1160);
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "replenished cartridge remains visible after missed release without retaining trigger claim");
    output = f.Sample(true, 1170);
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "second fresh press reclaims retained cartridge");
    f.Contact();
    (void)f.Sample(true, 1180);
    output = f.Sample(false, 1190);
    Require(output.reload && !output.cartridge_held,
        "replenished cartridge requests exactly one additional round on insertion release");
    Require(!f.Sample(false, 1200).reload,
        "replenished insertion never autonomously repeats native reload intent");

    Fixture held;
    held.Start();
    held.Contact();
    Require(held.Sample(false, 1120).reload,
        "held-during-reload fixture completes the initial cartridge");
    held.input.replenish_cartridge = true;
    output = held.Sample(true, 1130);
    held.input.replenish_cartridge = false;
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "replenishment while trigger is already held stays presentation-only");
    output = held.Sample(false, 1140);
    Require(output.cartridge_held && !output.consume_free_trigger && output.claimed_hand == 2,
        "fresh release after replenishment arms the next deliberate press");
    output = held.Sample(true, 1150);
    Require(output.consume_free_trigger && output.claimed_hand == 1,
        "only a fresh post-replenishment press claims the cartridge");
}
} // namespace

int main() {
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
