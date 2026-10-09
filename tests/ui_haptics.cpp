#include "runtime/ui_haptics.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

using namespace cojvr::runtime;
namespace {
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
UiHapticTime Time(std::int64_t ms) { return UiHapticTime{} + std::chrono::milliseconds(ms); }
EquipmentWheelSnapshot Wheel(int sector = 0, std::uint8_t available = 255) {
    EquipmentWheelSnapshot wheel{};
    wheel.valid = wheel.active = true; wheel.selected = sector; wheel.available_mask = available;
    return wheel;
}
struct Fixture {
    EquipmentWheelHaptics policy;
    Fixture() { policy.UpdateContext(true, 1, Time(1000)); }
    std::optional<UiHapticPulse> Frame(int sector, std::uint64_t seq, std::int64_t ms,
        std::uint8_t available = 255, std::uint64_t generation = 1, std::uint64_t device = 1) {
        policy.UpdateContext(true, 1, Time(ms));
        return policy.Observe(Wheel(sector, available), seq, generation, device, Time(ms), Time(ms));
    }
};
}
int main() {
    GameplayUiFeedbackContextMailbox native_context;
    Require(!GameplayUiFeedbackContextMailbox::Available(native_context.Snapshot()), "native feedback initially unavailable");
    native_context.Publish(true);
    const auto active_token=native_context.Snapshot();
    Require(native_context.Matches(active_token), "native active token accepted");
    native_context.Publish(true);
    Require(native_context.Snapshot()==active_token, "unchanged native ownership keeps epoch stable");
    for (const char* reason : {"frozen", "unknown", "flat", "tracking", "finalization"}) {
        (void)reason;
        const auto capture_token = native_context.Snapshot();
        EquipmentWheelHaptics queued;
        queued.UpdateContext(true,1,Time(1000));
        queued.UpdateNativeContext(capture_token,Time(1000));
        Require(!queued.Observe(Wheel(0),1,1,1,Time(1000),Time(1000),capture_token), "native baseline silent");
        native_context.Publish(false);
        const auto lost_token=native_context.Snapshot();
        Require(!native_context.Matches(capture_token), "queued token rejected after native context loss");
        queued.UpdateNativeContext(lost_token,Time(1010));
        Require(!queued.Observe(Wheel(1),2,1,1,Time(1005),Time(1010),capture_token), "queued active frame cannot pulse after native loss");
        native_context.Publish(true);
        const auto resumed_token=native_context.Snapshot();
        Require(resumed_token!=capture_token && native_context.Matches(resumed_token), "loss/reacquisition changes native epoch");
        queued.UpdateNativeContext(resumed_token,Time(1020));
        Require(!queued.Observe(Wheel(2),3,1,1,Time(1021),Time(1021),capture_token), "old token rejected even with fresh capture time after reacquisition");
        Require(!queued.Observe(Wheel(2),4,1,1,Time(1022),Time(1022),resumed_token), "new token establishes silent baseline");
        Require(queued.Observe(Wheel(3),5,1,1,Time(1030),Time(1030),resumed_token).has_value(), "new token fresh highlight recovers");
    }
    EquipmentWheelHaptics missed_edges;
    missed_edges.UpdateContext(true,1,Time(1000));
    const auto before_edges = native_context.Snapshot();
    missed_edges.UpdateNativeContext(before_edges,Time(1000));
    Require(!missed_edges.Observe(Wheel(0),1,1,1,Time(1000),Time(1000),before_edges), "missed-edge baseline silent");
    native_context.Publish(false);
    native_context.Publish(true);
    const auto after_edges = native_context.Snapshot();
    missed_edges.UpdateNativeContext(after_edges,Time(1010));
    Require(!missed_edges.Observe(Wheel(1),2,1,1,Time(1011),Time(1011),before_edges), "missed native loss/reacquire rejects queued epoch");
    Require(!missed_edges.Observe(Wheel(1),3,1,1,Time(1009),Time(1011),after_edges), "capture before observed native boundary rejected");
    Require(!missed_edges.Observe(Wheel(1),4,1,1,Time(1012),Time(1012),after_edges), "fresh epoch baseline silent");
    Require(missed_edges.Observe(Wheel(2),5,1,1,Time(1013),Time(1013),after_edges).has_value(), "fresh epoch change recovers");
    native_context.Reset();
    const auto stopped_token = native_context.Snapshot();
    Require(!native_context.Matches(after_edges) && !GameplayUiFeedbackContextMailbox::Available(stopped_token), "Stop invalidates native token");
    native_context.Reset();
    Require(native_context.Snapshot()!=stopped_token, "Start/Stop advances even unavailable epoch");
    native_context.Publish(true);
    Require(native_context.Snapshot()!=after_edges, "restart cannot reuse captured native token");
    bool diagnostic_escaped=false;
    try { OptionalUiHapticDiagnostic([] { throw std::bad_alloc{}; }); }
    catch (...) { diagnostic_escaped=true; }
    Require(!diagnostic_escaped, "optional haptic diagnostic allocation failure escaped");
    int diagnostic_attempts=0;
    OptionalUiHapticDiagnostic([&] { ++diagnostic_attempts; });
    Require(diagnostic_attempts==1, "optional diagnostics continue after swallowed fault");
    Fixture normal;
    Require(!normal.Frame(0,1,1000), "opening establishes silent baseline");
    auto pulse = normal.Frame(1,2,1010);
    Require(pulse && pulse->hand == UiHapticHand::right, "fresh available highlight change pulses right hand");
    Require(pulse->duration_seconds > 0 && pulse->duration_seconds <= .02F &&
        pulse->amplitude > 0 && pulse->amplitude <= .25F && UiHapticPulseValid(*pulse), "gentle bounded feedback");
    Require(!normal.Frame(1,3,1020), "same highlight never repeats");
    Require(!normal.Frame(2,4,1030), "rapid change is dropped by cadence bound");
    Require(!normal.Frame(2,5,1090), "suppressed change is never queued for later");
    Require(normal.Frame(3,6,1090).has_value(), "next fresh change recovers at cadence limit");
    Require(!normal.Frame(4,6,1190), "repeated sequence cannot pulse");
    Require(!normal.Frame(4,5,1190), "regressing sequence cannot pulse");

    Fixture permissions;
    (void)permissions.Frame(0,1,1000);
    Require(!permissions.Frame(1,2,1010,1), "unavailable sector stays silent");
    Require(!permissions.Frame(1,3,1020), "permission recovery does not replay denied sector");
    Require(!permissions.Frame(-1,4,1030), "center stays silent");
    Require(permissions.Frame(1,5,1040).has_value(), "fresh departure from center pulses");
    for (int sector : {-2, 8, 100}) Require(!permissions.Frame(sector,6+sector+2,1100), "invalid sector silent");
    EquipmentWheelSnapshot closed = Wheel(); closed.active = false;
    Require(!permissions.policy.Observe(closed,200,1,1,Time(1200),Time(1200)), "close never pulses");
    closed = Wheel(); closed.valid = false;
    Require(!permissions.policy.Observe(closed,201,1,1,Time(1201),Time(1201)), "invalid native wheel silent");

    for (bool generation_change : {false,true}) {
        Fixture loss; (void)loss.Frame(0,1,1000);
        loss.policy.UpdateContext(false,1,Time(1010));
        Require(!loss.policy.Observe(Wheel(1),2,1,1,Time(1010),Time(1010)), "loss cancels feedback");
        loss.policy.UpdateContext(true,generation_change?2:1,Time(1100));
        Require(!loss.policy.Observe(Wheel(2),3,1,1,Time(1090),Time(1100)), "pre-reacquisition capture rejected");
        Require(!loss.policy.Observe(Wheel(2),4,1,1,Time(1100),Time(1100)), "first fresh reacquired sample baseline silent");
        Require(loss.policy.Observe(Wheel(3),5,1,1,Time(1110),Time(1110)).has_value(), "new change after reacquisition recovers");
    }
    Fixture epoch; (void)epoch.Frame(0,1,1000);
    epoch.policy.UpdateContext(true,2,Time(1010));
    Require(!epoch.policy.Observe(Wheel(1),2,1,1,Time(1009),Time(1010)), "context replacement rejects old frame");
    Require(!epoch.policy.Observe(Wheel(1),3,1,1,Time(1011),Time(1011)), "context replacement establishes baseline");
    for (bool device_change : {false,true}) {
        Fixture resource; (void)resource.Frame(0,1,1000);
        Require(!resource.Frame(1,2,1010,255,device_change?1:2,device_change?2:1), "resource replacement baseline silent");
        Require(resource.Frame(2,3,1020,255,device_change?1:2,device_change?2:1).has_value(), "fresh replacement change recovers");
    }
    for (bool device_change : {false,true}) {
        Fixture restart; (void)restart.Frame(0,1,1000);
        Require(restart.Frame(1,25,1010).has_value(), "original resource highlight delivers before sequence reset");
        const auto next_generation=device_change?1U:2U;
        const auto next_device=device_change?2U:1U;
        Require(!restart.Frame(2,1,1110,255,next_generation,next_device),
            "replacement with restarted capture sequence establishes silent baseline");
        Require(restart.Frame(3,2,1200,255,next_generation,next_device).has_value(),
            "replacement with restarted capture sequence recovers fresh highlight");
        Require(!restart.Frame(4,2,1210,255,next_generation,next_device),
            "replacement capture cannot repeat the same sequence");
    }
    Fixture pending_resource; (void)pending_resource.Frame(0,1,1000);
    Require(!pending_resource.policy.Observe(Wheel(1),2,2,1,Time(1005),Time(1010)), "replacement rejects already captured sample");
    Require(!pending_resource.policy.Observe(Wheel(2),3,2,1,Time(1006),Time(1011)), "pre-replacement queued highlight cannot replay");
    Require(!pending_resource.policy.Observe(Wheel(2),4,2,1,Time(1012),Time(1012)), "fresh resource baseline is silent");
    Require(pending_resource.policy.Observe(Wheel(3),5,2,1,Time(1013),Time(1013)).has_value(), "new resource highlight recovers");
    Fixture stale; (void)stale.Frame(0,1,1000);
    Require(!stale.policy.Observe(Wheel(1),2,1,1,Time(1001),Time(1102)), "old capture silent");
    Require(!stale.policy.Observe(Wheel(2),3,1,1,Time(1200),Time(1102)), "future capture silent");
    Require(!stale.policy.Observe(Wheel(2),4,1,1,{},Time(1102)), "missing capture clock silent");
    Require(!stale.Frame(2,5,1110), "fresh sample after stale capture baseline silent");
    Require(stale.Frame(3,6,1120).has_value(), "subsequent change after stale capture recovers");
    Fixture gap; (void)gap.Frame(0,1,1000);
    gap.policy.UpdateContext(true,1,Time(1101));
    Require(!gap.Frame(1,2,1110), "capture outage cannot replay old highlight");
    gap.policy.UpdateContext(true,1,Time(900));
    Require(!gap.policy.Observe(Wheel(2),3,1,1,Time(900),Time(900)), "clock regression silent");
    Require(!gap.policy.Observe(Wheel(2),0,1,1,Time(901),Time(901)), "zero sequence silent");

    GameplayInputState raw{}; raw.active = raw.weapon_radial = true;
    raw.turn = {0,1};
    Pose pose{}; pose.position_valid = pose.orientation_valid = true;
    Require(WheelHapticInputAvailable(raw,true,true,true,false,pose,pose), "owned current input valid");
    for (int fault=0; fault<13; ++fault) {
        auto input=raw; auto head=pose, hand=pose;
        bool native=true, polled=true, focus=true, dashboard=false;
        switch(fault) {
        case 0: input.active=false; break; case 1: input.weapon_radial=false; break;
        case 2: input.radial_available=false; break; case 3: input.turn_available=false; break;
        case 4: input.turn.x=std::numeric_limits<float>::quiet_NaN(); break;
        case 5: input.turn.y=std::numeric_limits<float>::infinity(); break;
        case 6: native=false; break; case 7: polled=false; break; case 8: focus=false; break;
        case 9: dashboard=true; break; case 10: head.position_valid=false; break;
        case 11: hand.orientation_valid=false; break; case 12: hand.position.x=std::numeric_limits<float>::infinity(); break;
        }
        Require(!WheelHapticInputAvailable(input,native,polled,focus,dashboard,head,hand), "input/context/focus/pose loss blocks feedback");
    }
    UiHapticPulse bounded{UiHapticHand::right,.012F,120,.18F};
    for(int fault=0;fault<7;++fault) {
        auto bad=bounded;
        switch(fault) {
        case 0: bad.duration_seconds=-1; break; case 1: bad.duration_seconds=.021F; break;
        case 2: bad.amplitude=.26F; break; case 3: bad.frequency_hz=161; break;
        case 4: bad.frequency_hz=std::numeric_limits<float>::quiet_NaN(); break;
        case 5: bad.amplitude=0; break; case 6: bad.hand=static_cast<UiHapticHand>(99); break;
        }
        Require(!UiHapticPulseValid(bad), "invalid or excessive pulses rejected");
    }
    UiHapticOutputGate output;
    Require(!output.Available(UiHapticHand::right), "optional output absent by default");
    output.Configure(UiHapticHand::right,true);
    Require(output.Available(UiHapticHand::right), "optional output enabled independently");
    output.Failed(UiHapticHand::right);
    Require(!output.Available(UiHapticHand::right), "device failure disables retries until reinitialization");
    Require(!output.Available(UiHapticHand::left), "right output cannot enable other hand");
    output.Configure(UiHapticHand::left,true);
    Require(output.Available(UiHapticHand::left), "independent left optional output");
    Require(!output.Available(static_cast<UiHapticHand>(99)), "invalid hand fails closed");
    int attempts=0;
    Require(!output.Dispatch(bounded,[&]{ ++attempts; return true; }) && attempts==0, "failed hand must never retry device");
    output.Configure(UiHapticHand::right,true);
    auto excessive=bounded; excessive.amplitude=.8F;
    Require(!output.Dispatch(excessive,[&]{ ++attempts; return true; }) && attempts==0, "excessive pulse never reaches device");
    Require(!output.Dispatch(bounded,[&]{ ++attempts; return false; }) && attempts==1, "device failure consumed once");
    Require(!output.Dispatch(bounded,[&]{ ++attempts; return true; }) && attempts==1, "device failure never replays");
    bounded.hand=UiHapticHand::left;
    Require(output.Dispatch(bounded,[&]{ ++attempts; return true; }) && attempts==2, "other hand survives optional device failure");
    std::cout << "UI haptics: fresh highlights, cadence, ownership, stale/replay rejection and optional failures passed\n";
}
