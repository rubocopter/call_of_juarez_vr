#include "runtime/body_haptics.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace cojvr::runtime;

static_assert(std::is_trivially_copyable_v<BodyHapticEvent>);
static_assert(std::is_trivially_copyable_v<BodyHapticContext>);
static_assert(std::is_trivially_copyable_v<BodyHapticCommand>);
#ifdef COJVR_REQUIRE_WIN32
static_assert(sizeof(void*) == 4, "This verification must run as Win32");
#endif

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* why) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << why << '\n'; }
}
BodyHapticTime Time(std::int64_t ms) {
    return BodyHapticTime{} + std::chrono::milliseconds(ms);
}
BodyHapticContext Context() { return {{1, 2, 3, 4}, true}; }
BodyHapticEvent Event(std::uint64_t sequence, std::int64_t at,
    BodyHapticIdentity identity = Context().identity) {
    return {identity, sequence, Time(at), BodyHapticPattern::vest_test, .5F};
}
struct Fixture {
    BodyHapticAdmission policy;
    BodyHapticContext context = Context();
    Fixture() {
        policy.UpdateContext(context, Time(1000));
        (void)policy.Observe(Event(1, 1000), context, Time(1000));
    }
    std::optional<BodyHapticCommand> Observe(std::uint64_t sequence, std::int64_t at,
        std::int64_t now) {
        return policy.Observe(Event(sequence, at, context.identity), context, Time(now));
    }
};

// A test output endpoint only. Admission/deduplication remain real policy code.
struct RecordingSink {
    std::vector<BodyHapticCommand> commands;
    int attempts = 0;
    bool fail_next = false, throw_next = false;
    bool TrySubmit(const BodyHapticCommand& command) {
        ++attempts;
        if (throw_next) { throw_next = false; throw std::runtime_error("output fault"); }
        if (fail_next) { fail_next = false; return false; }
        commands.push_back(command);
        return true;
    }
};
bool Record(Fixture& fixture, const BodyHapticEvent& event, std::int64_t now,
    RecordingSink& sink) {
    const auto command = fixture.policy.Observe(event, fixture.context, Time(now));
    if (!command) return false;
    try { return sink.TrySubmit(*command); } catch (...) { return false; }
}

void SilentBaselineAndFreshOnce() {
    BodyHapticAdmission policy;
    const auto context = Context();
    policy.UpdateContext(context, Time(1000));
    Check(!policy.Observe(Event(1, 999), context, Time(1000)),
        "a pre-context event cannot establish the baseline");
    Check(!policy.Observe(Event(2, 1000), context, Time(1000)),
        "the first eligible event establishes a silent baseline");
    const auto command = policy.Observe(Event(3, 1010), context, Time(1010));
    Check(command.has_value(), "a fresh event after baseline emits once");
    if (command) {
        Check(command->pattern == BodyHapticPattern::vest_test && command->intensity == .5F &&
            command->sequence == 3 && command->identity == context.identity,
            "the command preserves the QA pattern, normalized intensity and identity");
    }
    Check(!policy.Observe(Event(3, 1010), context, Time(1020)), "a repeat cannot replay");
    Check(!policy.Observe(Event(2, 1021), context, Time(1021)), "an older sequence cannot replay");
    Check(policy.Observe(Event(4, 1022), context, Time(1022)).has_value(),
        "duplicates do not suppress a genuinely new event");
}

void ConsumeBeforeOutputFault() {
    for (bool throws : {false, true}) {
        Fixture fixture;
        RecordingSink sink;
        sink.fail_next = !throws; sink.throw_next = throws;
        Check(!Record(fixture, Event(2, 1010), 1010, sink), "output fault is reported");
        Check(sink.attempts == 1 && sink.commands.empty(), "one eligible output attempt faults");
        Check(!Record(fixture, Event(2, 1010), 1020, sink), "a faulted event is already consumed");
        Check(sink.attempts == 1, "a duplicate never reaches the recovered output endpoint");
        Check(Record(fixture, Event(3, 1030), 1030, sink), "a new event can reach a healthy sink");
        Check(sink.commands.size() == 1 && sink.commands[0].sequence == 3,
            "the recorder contains only the new event, not fault backlog");
    }
}

void StaleFutureAndMissingTime() {
    const std::array<std::int64_t, 3> occurred{1001, 1120, 0};
    for (const auto at : occurred) {
        Fixture fixture;
        Check(!fixture.Observe(2, at, 1102), "stale, future or absent event time rejects");
        Check(!fixture.Observe(2, 1103, 1103), "repairing a rejected sequence cannot replay it");
        Check(!fixture.Observe(3, 1104, 1104), "time rejection requires a fresh silent baseline");
        Check(fixture.Observe(4, 1105, 1105).has_value(), "fresh events recover after bad event time");
    }
    Fixture boundary;
    Check(boundary.Observe(2, 1000, 1100).has_value(), "an event at the 100 ms age boundary is fresh");
    Fixture expired;
    Check(!expired.Observe(2, 1000, 1101), "one millisecond beyond the age bound rejects");
    Fixture future;
    Check(!future.Observe(2, 1020, 1010), "a future event rejects before its time arrives");
    Check(!future.Observe(2, 1020, 1020), "a future event cannot wait and replay at its time");
}

void RegressedObservationClock() {
    Fixture fixture;
    Check(fixture.Observe(2, 1020, 1020).has_value(), "clock fixture first emits");
    Check(!fixture.Observe(3, 1010, 1010), "regressed observation time rejects");
    Check(!fixture.Observe(4, 1011, 1011), "multiple samples below the clock high-water mark reject");
    Check(!fixture.Observe(3, 1021, 1021), "clock rejection consumed its sequence");
    Check(!fixture.Observe(5, 1022, 1022), "clock recovery establishes a silent baseline");
    Check(fixture.Observe(6, 1023, 1023).has_value(), "a fresh event after clock recovery emits");
}

void RegressedEventClock() {
    Fixture fixture;
    Check(fixture.Observe(2, 1020, 1020).has_value(), "event-time fixture first emits");
    Check(!fixture.Observe(3, 1010, 1030), "increasing sequence cannot conceal regressed event time");
    Check(!fixture.Observe(3, 1031, 1031), "repairing regressed event time cannot replay");
    Check(!fixture.Observe(4, 1032, 1032), "event-time recovery is silent");
    Check(fixture.Observe(5, 1033, 1033).has_value(), "new ordered events recover");
}

void ContextIdentityChanges() {
    for (int field = 0; field < 4; ++field) {
        Fixture fixture;
        Check(fixture.Observe(2, 1010, 1010).has_value(), "old context first emits");
        const auto old_identity = fixture.context.identity;
        auto& identity = fixture.context.identity;
        if (field == 0) ++identity.session;
        if (field == 1) ++identity.owner;
        if (field == 2) ++identity.input_epoch;
        if (field == 3) ++identity.availability_epoch;
        fixture.policy.UpdateContext(fixture.context, Time(1020));
        Check(!fixture.policy.Observe(Event(3, 1021, old_identity), fixture.context, Time(1021)),
            "a queued old-context event cannot establish a new baseline");
        // Session is the sequence namespace; other changes retain its high-water mark.
        const std::uint64_t baseline = field == 0 ? 1 : 4;
        Check(!fixture.Observe(baseline, 1022, 1022), "context replacement establishes a silent baseline");
        Check(fixture.Observe(baseline + 1, 1023, 1023).has_value(), "fresh current-context event recovers");
    }
}

void AvailabilityAndReconnect() {
    Fixture fixture;
    fixture.context.available = false;
    fixture.policy.UpdateContext(fixture.context, Time(1010));
    Check(!fixture.Observe(2, 1011, 1011), "no availability means no output");
    fixture.context.available = true;
    fixture.policy.UpdateContext(fixture.context, Time(1020));
    Check(!fixture.Observe(2, 1011, 1021), "reconnect cannot replay an unavailable event");
    Check(!fixture.Observe(3, 1019, 1022), "an event from before reacquisition cannot arm");
    Check(!fixture.Observe(4, 1023, 1023), "reconnect is silent even with the same identity");
    Check(fixture.Observe(5, 1024, 1024).has_value(), "fresh post-reconnect event emits");

    Fixture missed_edge;
    const auto queued = Event(2, 1010);
    ++missed_edge.context.identity.availability_epoch;
    Check(!missed_edge.policy.Observe(queued, missed_edge.context, Time(1010)),
        "an availability epoch detects missed disconnect/reconnect polls");
    Check(!missed_edge.Observe(3, 1011, 1011), "missed-edge reacquisition establishes its own baseline");
    Check(missed_edge.Observe(4, 1012, 1012).has_value(), "missed-edge recovery accepts only new events");
}

void InvalidIntensityAndPattern() {
    const std::array<float, 6> invalid{-.1F, 0.F, 1.01F,
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    for (const float intensity : invalid) {
        Fixture fixture;
        auto event = Event(2, 1010); event.intensity = intensity;
        Check(!fixture.policy.Observe(event, fixture.context, Time(1010)), "invalid normalized intensity rejects");
        Check(!fixture.Observe(2, 1011, 1011), "invalid intensity consumes the attempt before correction");
        Check(!fixture.Observe(3, 1012, 1012), "invalid intensity cannot establish an output baseline");
        Check(fixture.Observe(4, 1013, 1013).has_value(), "valid intensity recovers after baseline");
    }
    Fixture fixture;
    auto event = Event(2, 1010); event.pattern = static_cast<BodyHapticPattern>(255);
    Check(!fixture.policy.Observe(event, fixture.context, Time(1010)), "only QA vest_test is admitted");
    Check(!fixture.Observe(2, 1011, 1011), "correcting the pattern cannot replay its sequence");
    Check(!fixture.Observe(3, 1012, 1012), "invalid pattern resets the baseline");
    auto maximum = Event(4, 1013); maximum.intensity = 1.F;
    const auto command = fixture.policy.Observe(maximum, fixture.context, Time(1013));
    Check(command && command->intensity == 1.F, "the normalized upper endpoint is preserved");
}

void InvalidIdentityAndSequence() {
    for (int field = 0; field < 4; ++field) {
        Fixture fixture;
        auto& identity = fixture.context.identity;
        if (field == 0) identity.session = 0;
        if (field == 1) identity.owner = 0;
        if (field == 2) identity.input_epoch = 0;
        if (field == 3) identity.availability_epoch = 0;
        Check(!fixture.Observe(2, 1010, 1010), "an unobserved context identity cannot output");
        Check(!fixture.Observe(3, 1011, 1011), "an invalid context cannot establish a baseline");
    }
    Fixture fixture;
    Check(!fixture.Observe(0, 1010, 1010), "zero event sequence rejects");
    Check(fixture.Observe(2, 1011, 1011).has_value(), "a missing sequence cannot consume a new valid event");
    BodyHapticAdmission absent;
    Check(!absent.Observe(Event(1, 1000), {}, Time(1000)), "default context is unavailable");
    Check(!absent.Observe(Event(2, 1001), {}, Time(1001)), "default context remains silent");
}

void ShutdownIsTerminal() {
    Fixture fixture;
    Check(fixture.Observe(2, 1010, 1010).has_value(), "shutdown fixture first emits");
    fixture.policy.Shutdown();
    Check(!fixture.Observe(3, 1011, 1011), "shutdown immediately rejects fresh events");
    ++fixture.context.identity.session;
    fixture.policy.UpdateContext(fixture.context, Time(1020));
    Check(!fixture.Observe(1, 1020, 1020), "context updates cannot restart a shutdown policy");
    Check(!fixture.Observe(2, 1021, 1021), "a new session cannot bypass terminal shutdown");
    fixture.policy.Shutdown();
    Check(!fixture.Observe(3, 1022, 1022), "repeated shutdown remains terminal");
}
} // namespace

int main() {
    SilentBaselineAndFreshOnce();
    ConsumeBeforeOutputFault();
    StaleFutureAndMissingTime();
    RegressedObservationClock();
    RegressedEventClock();
    ContextIdentityChanges();
    AvailabilityAndReconnect();
    InvalidIntensityAndPattern();
    InvalidIdentityAndSequence();
    ShutdownIsTerminal();
    std::cout << "Body haptic QA admission: " << checks << " checks, " << failures << " failures; "
              << sizeof(void*) * 8 << "-bit\n";
    return failures == 0 ? 0 : 1;
}
