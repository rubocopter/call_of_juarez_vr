#pragma once
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Native channels differ from physical hands: right enters alternate/tension;
// left requests the actual bow attack. The operator only uses physical R2.
struct CoJBowDrawDispatchSample {
    bool selected = false, eligible = false, drawing = false, release_fire = false;
    bool alternate_ready = false, can_attack = false, native_right_held = false;
    bool counter_valid = false;
    std::uint32_t weapon = 0;
    std::uint64_t player = 0, context = 0, input_generation = 0, sequence = 0, now_ms = 0;
    int attack_entries = 0;
};
struct CoJBowDrawDispatch {
    bool claimed = false, right = false, left = false, shot_requested = false;
    bool pending = false, cancelled = false;
    const char* reason = "none";
};
class CoJBowDrawInput final {
public:
    CoJBowDrawDispatch Update(const CoJBowDrawDispatchSample& s) noexcept {
        CoJBowDrawDispatch out{};out.claimed=s.selected;
        if(!s.selected){Reset();return out;}
        const bool usable=s.eligible&&s.weapon&&s.weapon!=0xFFFFFFFFU&&s.player&&s.context&&
            s.input_generation&&s.sequence&&s.now_ms;
        if(!usable){out.cancelled=holding_||pending_;out.reason="dispatch_unavailable";Reset();return out;}
        const bool same=weapon_==s.weapon&&player_==s.player&&context_==s.context&&input_==s.input_generation;
        if(!same){const bool existed=weapon_!=0;Reset();
            weapon_=s.weapon;player_=s.player;context_=s.context;input_=s.input_generation;
            sequence_=s.sequence;last_ms_=s.now_ms;
            out.cancelled=existed;out.reason=existed?"dispatch_owner_changed":"none";return out;}
        if(s.sequence<sequence_||s.now_ms<last_ms_||s.now_ms-last_ms_>100){
            out.cancelled=holding_||pending_;holding_=pending_=false;
            out.reason="dispatch_pose_time_fault";
            sequence_=s.sequence;last_ms_=s.now_ms;return out;}
        if(s.sequence==sequence_){out.right=holding_||pending_;out.pending=pending_;return out;}
        sequence_=s.sequence;last_ms_=s.now_ms;
        if(pending_){
            // Entry is an observed native attempt, not an arrow/hit confirmation.
            // A counter fault/regression or watchdog expiry aborts; no retry.
            if(!s.counter_valid||s.attack_entries!=entries_||s.now_ms-request_ms_>=1000){
                out.cancelled=!s.counter_valid||s.attack_entries<entries_||s.now_ms-request_ms_>=1000;
                out.reason=!s.counter_valid?"attack_counter_unavailable":s.attack_entries<entries_?
                    "attack_counter_regressed":s.now_ms-request_ms_>=1000?"attack_timeout":"attack_entry_observed";
                pending_=holding_=false;return out;}
            out.right=out.pending=true;return out;
        }
        if(s.release_fire){
            if(holding_&&s.alternate_ready&&s.can_attack&&s.native_right_held&&s.counter_valid&&s.attack_entries>=0){
                pending_=true;holding_=false;entries_=s.attack_entries;request_ms_=s.now_ms;
                out.right=out.left=out.shot_requested=out.pending=true;return out;
            }
            holding_=false;out.cancelled=true;out.reason="native_release_gate";return out;
        }
        holding_=s.drawing;out.right=holding_;return out;
    }
    void Reset() noexcept {
        weapon_=0;player_=context_=input_=sequence_=last_ms_=request_ms_=0;
        entries_=0;holding_=pending_=false;
    }
    bool pending() const noexcept { return pending_; }
private:
    std::uint32_t weapon_=0;
    std::uint64_t player_=0,context_=0,input_=0,sequence_=0,last_ms_=0,request_ms_=0;
    int entries_=0;
    bool holding_=false,pending_=false;
};
}
