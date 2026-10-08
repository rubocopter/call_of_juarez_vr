#include "games/call_of_juarez/motion_reload_adapter.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
namespace {
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
Pose At(Vec3 position) { Pose pose{}; pose.position=position;
    pose.orientation_valid=pose.position_valid=true; return pose; }
struct Fixture {
    CoJMotionReloadAdapter adapter;
    CoJMotionReloadOwner owner{0, 42, true};
    CoJMotionReloadOwner native_reload_owner{};
    CoJMotionReloadDiagnostic diagnostic{};
    GameplayInputState raw{}, mapped{};
    Pose head=At({0,1.6F,0}), left=At({-.2F,1.05F,-.1F}), right=At({.2F,1.35F,-.35F});
    std::uint64_t sequence=0, now=1000, player=7;
    std::uint64_t native_context=3;
    int armed_hand=0;
    bool context=true, recentered=false;
    Fixture(int hand=0) : owner{hand,42,true}, armed_hand(hand) {
        raw.active=mapped.active=true; raw.input_context_generation=4;
        if (hand==1) {
            left=At({-.2F,1.35F,-.35F}); right=At({.2F,1.05F,-.1F});
        }
    }
    MotionReloadGestureOutput Poll(bool held) {
        if (armed_hand==0) raw.fire_left=held; else raw.fire_right=held;
        mapped=raw;
        return Apply();
    }
    MotionReloadGestureOutput Apply() { ++sequence; now+=50;
        return adapter.Update(mapped,raw,owner,diagnostic,player,head,left,right,
            context,recentered,sequence,now,native_context,native_reload_owner); }
    void Start() { (void)Poll(false); const auto start=Poll(true);
        Require(start.consume_free_trigger && !(armed_hand==0 ? mapped.fire_left : mapped.fire_right) && !mapped.reload,
            "adapter must consume free trigger at source without early reload"); }
    void Contact() { if (armed_hand==0) left=right; else right=left; (void)Poll(true); }
    void RequestSingleRound() {
        owner.single_round_supported=true;
        Start(); Contact();
        Require(Poll(false).reload,"native recovery fixture must request its first round");
        adapter.NoteSingleRoundDispatch(true);
        owner={}; diagnostic={}; diagnostic.weapon_reloading=true;
        diagnostic.reason=CoJMotionReloadRejectReason::dead_or_reloading;
        Require(!Poll(false).cartridge_held,"native reload owns animation before replenishment");
    }
    void ObserveRecovery() {
        owner={}; diagnostic={};
        diagnostic.reason=CoJMotionReloadRejectReason::two_handed;
        // Admission exits before reporting hand states on the real two-hand
        // rejection. Only the separate passive native-session read proves owner.
        native_reload_owner={armed_hand,42,true,true};
    }
    MotionReloadGestureOutput Recover(bool held=false) {
        owner={armed_hand,42,true,true}; diagnostic={}; native_reload_owner={};
        return Poll(held);
    }
};

void NativeReloadRecovery() {
    // Replays the integration break: action31 -> observed native reload ->
    // admission two_handed -> ordinary owner. An unverified rejection cancels;
    // a separately verified same native owner permits waiting, never admission.
    Fixture unverified; unverified.RequestSingleRound(); unverified.ObserveRecovery();
    unverified.native_reload_owner={}; (void)unverified.Poll(false);
    Require(!unverified.Recover().cartridge_held,
        "two-handed rejection alone cannot authorize native continuation");

    Fixture session; session.RequestSingleRound(); session.ObserveRecovery();
    for (int frame=0; frame<12; ++frame) {
        const auto waiting=session.Poll(false);
        Require(!waiting.reload && !session.mapped.reload && !waiting.cartridge_held,
            "verified native recovery must wait without inserting or presenting a cartridge");
    }
    session.diagnostic.reason=CoJMotionReloadRejectReason::armed_state;
    Require(!session.Poll(true).cartridge_held,
        "native end-state recovery still cannot replenish before ordinary admission");
    const auto replenished=session.Recover(true);
    Require(replenished.cartridge_held && replenished.cartridge_hand==1 && !replenished.reload &&
        !session.Poll(true).reload && !session.Poll(false).reload,
        "held input across native recovery must require release and a fresh press");
    Require(session.Poll(true).consume_free_trigger,
        "fresh press must claim the recovered cartridge without another waist pickup");
    Require(session.Poll(false).reload,"recovered cartridge insertion must request exactly the next round");
    session.adapter.NoteSingleRoundDispatch(true);
    session.owner={}; session.diagnostic={}; session.diagnostic.weapon_reloading=true;
    session.diagnostic.reason=CoJMotionReloadRejectReason::dead_or_reloading;
    (void)session.Poll(false); session.ObserveRecovery(); (void)session.Poll(false);
    Require(session.Recover().cartridge_held && !session.Poll(false).reload,
        "each dispatched round must independently observe native reload and recovery");

    // Same integration timeline, interrupted at recovery: neither an explicit
    // native owner nor later ordinary admission may revive a cancelled session.
    for (int fault=0; fault<14; ++fault) {
        Fixture interrupted; interrupted.RequestSingleRound(); interrupted.ObserveRecovery();
        switch (fault) {
        case 0: interrupted.native_reload_owner.weapon_id=99; break;
        case 1: interrupted.native_reload_owner.armed_hand=1; break;
        case 2: interrupted.native_reload_owner.single_round_supported=false; break;
        case 3: interrupted.diagnostic.reason=CoJMotionReloadRejectReason::jni_failure; break;
        case 4: interrupted.diagnostic.player_dead=true; break;
        case 5: interrupted.diagnostic.attacking[0]=true; break;
        case 6: interrupted.raw.digital_available &= ~1U; break;
        case 7: interrupted.left.position_valid=false; break;
        case 8: interrupted.raw.input_context_generation++; break;
        case 9: interrupted.native_context+=4; break;
        case 10: interrupted.context=false; break;
        case 11: interrupted.recentered=true; break;
        case 12: interrupted.now+=300; break;
        case 13: interrupted.adapter.Cancel(); break;
        }
        (void)interrupted.Poll(false);
        interrupted.raw.digital_available |= 1U;
        interrupted.left=interrupted.right; interrupted.context=true; interrupted.recentered=false;
        Require(!interrupted.Recover().cartridge_held,
            "native recovery owner must not bypass owner/input/tracking/context/freshness cancellation");
    }

    Fixture unavailable; unavailable.RequestSingleRound(); unavailable.ObserveRecovery();
    // Equivalent to begin_frame/pose failure: HookRenderView calls Cancel and
    // does not poll Update. Real observation loss must not be a reload exception.
    unavailable.adapter.Cancel(); unavailable.now+=50;
    Require(!unavailable.Recover().cartridge_held,
        "missing camera sample during native reload must cancel replenishment");

    Fixture duplicates; duplicates.RequestSingleRound(); duplicates.ObserveRecovery();
    for (int poll=0; poll<6; ++poll) {
        duplicates.now+=50; duplicates.mapped=duplicates.raw;
        (void)duplicates.adapter.Update(duplicates.mapped,duplicates.raw,duplicates.owner,
            duplicates.diagnostic,duplicates.player,duplicates.head,duplicates.left,duplicates.right,
            duplicates.context,false,duplicates.sequence,duplicates.now,duplicates.native_context,
            duplicates.native_reload_owner);
    }
    Require(!duplicates.Recover().cartridge_held,
        "duplicate render polls cannot renew native reload pose freshness");
}
void NativeReloadSupportTrigger() {
    for (int hand=0; hand<2; ++hand) {
        Fixture session(hand); session.RequestSingleRound();
        session.native_reload_owner={hand,42,true,true};
        const auto mask=static_cast<std::uint8_t>(hand==0 ? 2U : 1U);
        const auto blocked=session.Poll(true);
        Require(blocked.consume_free_trigger && blocked.consumed_trigger_mask==mask &&
            blocked.claimed_hand==1-hand && !blocked.reload &&
            !(hand==0 ? session.mapped.fire_left : session.mapped.fire_right),
            "verified accepted native reload must suppress the original support trigger before native fallback");
        session.ObserveRecovery();
        Require(session.Poll(true).consumed_trigger_mask==mask,
            "native two-hand recovery must retain the same physical trigger claim");
        if (hand==0) session.raw.fire_left=false; else session.raw.fire_right=false;
        session.mapped=session.raw; session.now+=10;
        (void)session.adapter.Update(session.mapped,session.raw,session.owner,session.diagnostic,
            session.player,session.head,session.left,session.right,true,false,
            session.sequence,session.now,session.native_context,session.native_reload_owner);
        const auto recovered=session.Recover(true);
        Require(recovered.cartridge_held && recovered.consumed_trigger_mask==mask && !recovered.reload,
            "duplicate release cannot clear support suppression before held ordinary recovery");
        Require(!session.Poll(true).reload && session.Poll(false).consumed_trigger_mask==mask,
            "suppressed support trigger must observe its own fresh available release");
        Require(session.Poll(true).consume_free_trigger && session.Poll(false).reload,
            "fresh post-recovery press/release inserts the replenished cartridge once");

        Fixture unavailable_release(hand); unavailable_release.RequestSingleRound();
        unavailable_release.native_reload_owner={hand,42,true,true};
        (void)unavailable_release.Poll(true);
        unavailable_release.raw.digital_available &= ~(hand==0 ? 1U : 2U);
        unavailable_release.adapter.Cancel();
        Require(unavailable_release.Poll(false).consumed_trigger_mask==mask,
            "neutral/unavailable release cannot drop the prior native support trigger claim");
        unavailable_release.raw.digital_available |= hand==0 ? 1U : 2U;
        unavailable_release.owner={1-hand,99,true,true};
        // The formerly empty hand is now armed: its old physical claim still
        // owns this press, despite current owner cancellation/replacement.
        Require(unavailable_release.Poll(true).consumed_trigger_mask==mask &&
            !(hand==0 ? unavailable_release.mapped.fire_left : unavailable_release.mapped.fire_right),
            "owner replacement must preserve the original suppressed physical hand until release");
        (void)unavailable_release.Poll(false);

        for (int fault=0; fault<14; ++fault) {
            Fixture unverified(hand); unverified.RequestSingleRound();
            unverified.native_reload_owner={hand,42,true,true};
            switch (fault) {
            case 0: unverified.native_reload_owner={}; break;
            case 1: unverified.native_reload_owner.weapon_id=99; break;
            case 2: unverified.native_reload_owner.armed_hand=1-hand; break;
            case 3: unverified.player++; break;
            case 4: unverified.raw.input_context_generation++; break;
            case 5: unverified.native_context+=4; break;
            case 6: unverified.context=false; break;
            case 7: unverified.left.position_valid=false; break;
            case 8: unverified.now+=300; break;
            case 9: if (hand==0) unverified.raw.fire_right=true; else unverified.raw.fire_left=true; break;
            case 10: unverified.raw.digital_available &= ~(hand==0 ? 1U : 2U); break;
            case 11: unverified.native_reload_owner.single_round_supported=false; break;
            case 12: unverified.recentered=true; break;
            case 13: unverified.diagnostic.reason=CoJMotionReloadRejectReason::jni_failure; break;
            }
            const auto result=unverified.Poll(true);
            Require(result.consumed_trigger_mask==0 &&
                (hand==0 ? unverified.mapped.fire_left : unverified.mapped.fire_right),
                "unverified or interrupted reload must not newly suppress a support trigger");
            if (fault==9) Require(hand==0 ? unverified.mapped.fire_right : unverified.mapped.fire_left,
                "armed trigger must stay native-owned while cancelling reload continuation");
        }
    }
}
}
int main() {
    NativeReloadSupportTrigger();
    NativeReloadRecovery();
    Fixture f; f.Start(); f.Contact();
    const auto completed=f.Poll(false);
    Require(completed.reload && f.mapped.reload && f.mapped.active,
        "adapter must map completed journey to ordinary native reload intent");
    Require(!f.Poll(false).reload && !f.mapped.reload,"reload must be one sample only");
    for (int fault=0;fault<18;++fault) {
        Fixture blocked; blocked.Start(); blocked.Contact();
        blocked.raw.fire_left=false; blocked.mapped=blocked.raw;
        switch(fault) {
        case 0: blocked.context=false; break;
        case 1: blocked.recentered=true; break;
        case 2: blocked.owner.valid=false; break;
        case 3: blocked.mapped.interact=true; break;
        case 4: blocked.mapped.objectives=true; break;
        case 5: blocked.mapped.logs=true; break;
        case 6: blocked.mapped.quick_load=true; break;
        case 7: blocked.mapped.quick_save=true; break;
        case 8: blocked.mapped.weapon_next=true; break;
        case 9: blocked.mapped.weapon_previous=true; break;
        case 10: blocked.mapped.hands=true; break;
        case 11: blocked.mapped.discard_weapon=true; break;
        case 12: blocked.mapped.equipment_select[2]=true; break;
        case 13: blocked.raw.weapon_radial=true; break;
        case 14: blocked.mapped.weapon_radial=true; break;
        case 15: blocked.mapped.active=false; break;
        case 16: blocked.mapped.fire_right=true; break;
        case 17: blocked.mapped.alternate_fire=true; break;
        }
        Require(!blocked.Apply().reload,"native context/dispatch conflict must cancel completed gesture");
        if (fault==16) Require(blocked.mapped.fire_right,"armed trigger must remain independent");
    }
    Fixture square; square.Start(); square.Contact();
    square.raw.fire_left=false; square.mapped=square.raw; square.mapped.reload=true;
    Require(!square.Apply().reload && square.mapped.reload,
        "Square reload remains intact and cancels gesture");
    Fixture unavailable; unavailable.Start(); unavailable.Contact();
    unavailable.raw.fire_left=false; unavailable.raw.digital_available&=~1U;
    unavailable.mapped=unavailable.raw;
    Require(!unavailable.Apply().reload,"unavailable free binding is not release");
    Fixture flip; flip.Start(); flip.owner.armed_hand=1; flip.owner.weapon_id=90;
    flip.raw.fire_left=true; flip.raw.fire_right=true; flip.mapped=flip.raw;
    const auto cancelled=flip.Apply();
    Require(!cancelled.reload && !flip.mapped.fire_left && flip.mapped.fire_right &&
        (cancelled.consumed_trigger_mask & 2U),
        "owner flip must consume original physical trigger until released");
    Fixture invalid; invalid.Start(); invalid.owner={};
    invalid.raw.fire_left=true; invalid.mapped=invalid.raw;
    Require(invalid.Apply().consume_free_trigger && !invalid.mapped.fire_left,
        "lost native observation must retain original trigger claim");
    Fixture inactive; inactive.Start(); inactive.raw={}; inactive.mapped={};
    Require(inactive.Apply().consume_free_trigger,
        "neutralized inactive input must not pretend the claimed trigger was released");
    inactive.raw.active=true; inactive.raw.fire_left=true; inactive.mapped=inactive.raw;
    Require(inactive.Apply().consume_free_trigger && !inactive.mapped.fire_left,
        "context recovery held trigger remains consumed");
    Fixture epoch; epoch.Start(); epoch.Contact(); epoch.native_context=7;
    Require(!epoch.Poll(false).reload,"native loss/reacquire epoch cancels old journey");
    Fixture explicit_loss; explicit_loss.Start(); explicit_loss.Contact();
    explicit_loss.adapter.Cancel();
    Require(!explicit_loss.Poll(false).reload,"early render-context cancellation drops journey");

    Fixture chained; chained.owner.single_round_supported=true;
    chained.Start(); chained.Contact();
    Require(chained.Poll(false).reload,"first chained cartridge must request a round");
    chained.adapter.NoteSingleRoundDispatch(true);
    chained.owner={}; chained.diagnostic.weapon_reloading=true;
    auto continuation=chained.Poll(false);
    Require(!continuation.cartridge_held,"cartridge stays hidden during native reload interval");
    chained.owner={0,42,true,true}; chained.diagnostic={};
    continuation=chained.Poll(false);
    Require(continuation.cartridge_held && continuation.cartridge_hand==1 &&
        continuation.claimed_hand==2 && !continuation.consume_free_trigger,
        "same native owner returning eligible replenishes support hand without claiming trigger");
    Require(chained.Poll(true).consume_free_trigger,"fresh press claims replenished cartridge");
    chained.Contact(); (void)chained.Poll(true);
    Require(chained.Poll(false).reload,"replenished insertion requests the next native round");

    Fixture rejected; rejected.owner.single_round_supported=true;
    rejected.Start(); rejected.Contact(); Require(rejected.Poll(false).reload,"rejected fixture starts request");
    rejected.adapter.NoteSingleRoundDispatch(true);
    Require(!rejected.Poll(false).cartridge_held,
        "eligible owner without an observed native reload never replenishes a cartridge");
    rejected.owner={}; rejected.diagnostic.weapon_reloading=true;
    (void)rejected.Poll(false);
    rejected.owner={0,42,true,true}; rejected.diagnostic={};
    Require(!rejected.Poll(false).cartridge_held,
        "late unrelated native reload cannot revive a rejected continuation");

    Fixture replaced; replaced.owner.single_round_supported=true;
    replaced.Start(); replaced.Contact(); Require(replaced.Poll(false).reload,"replacement fixture starts request");
    replaced.adapter.NoteSingleRoundDispatch(true);
    replaced.owner={}; replaced.diagnostic.weapon_reloading=true; (void)replaced.Poll(false);
    replaced.owner={0,99,true,true}; replaced.diagnostic={};
    Require(!replaced.Poll(false).cartridge_held,
        "different weapon after native reload interval cancels continuation");
    for (int fault=0; fault<8; ++fault) {
        Fixture interrupted; interrupted.owner.single_round_supported=true;
        interrupted.Start(); interrupted.Contact();
        Require(interrupted.Poll(false).reload,"interruption fixture requests initial round");
        interrupted.adapter.NoteSingleRoundDispatch(true);
        interrupted.owner={}; interrupted.diagnostic.weapon_reloading=true;
        const auto prior_sequence=interrupted.sequence;
        const auto prior_now=interrupted.now;
        switch (fault) {
        case 0: interrupted.left.position_valid=false; break;
        case 1: interrupted.right.orientation_valid=false; break;
        case 2: interrupted.head.position.x=std::numeric_limits<float>::quiet_NaN(); break;
        case 3: interrupted.raw.digital_available &= ~1U; break;
        case 4: interrupted.now+=300; break;
        case 5: interrupted.sequence=0; break;
        case 6: interrupted.now=0; break;
        case 7: interrupted.diagnostic.reason=CoJMotionReloadRejectReason::jni_failure; break;
        }
        (void)interrupted.Poll(false);
        interrupted.left=interrupted.right=At({.2F,1.35F,-.35F});
        interrupted.head=At({0,1.6F,0}); interrupted.raw.digital_available |= 1U;
        interrupted.sequence=std::max(interrupted.sequence,prior_sequence);
        interrupted.now=std::max(interrupted.now,prior_now);
        interrupted.owner={0,42,true,true}; interrupted.diagnostic={};
        Require(!interrupted.Poll(false).cartridge_held,"tracking/input/freshness loss during native reload cancels replenishment");
    }
    std::cout<<"CoJ gesture/native intent and trigger ownership checks passed\n";
}
