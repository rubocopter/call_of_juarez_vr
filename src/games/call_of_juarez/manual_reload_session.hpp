#pragma once
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
namespace cojvr::games::call_of_juarez {
enum class CoJManualReloadPhase { ready, opening, manual_load, closing };
struct CoJManualReloadInput {
    bool enabled=false, allowed=false, recentered=false;
    bool native_fallback_allowed=false; // Positive unsupported context; never a failed read.
    std::uint64_t player=0,context=0,sequence=0,now=0;
    runtime::GameplayInputState raw{};
    runtime::Pose head{},left{},right{};
    CoJMotionReloadOwner admission{},recovery{};
    CoJReloadTraceSnapshot native{};
};
struct CoJManualReloadOutput {
    bool start=false,cancel=false,insert=false,tracked=false,supervise=false;
    int selected_index=-1; // Captured at indexed pickup, never inferred at release.
    runtime::MotionReloadGestureOutput gesture{};
    const char* reason="none";
};
class CoJManualReloadSession {
public:
    // UI/native ownership and ammunition are observations, not state replicas.
    CoJManualReloadOutput Update(runtime::GameplayInputState& input,const CoJManualReloadInput& s) noexcept {
        CoJManualReloadOutput out{};
        const bool regression=have_sample_&&(s.sequence<sequence_||s.now<observed_);
        const bool fresh=!have_sample_||s.sequence>sequence_;
        const bool gap=have_sample_&&s.now>=fresh_at_&&s.now-fresh_at_>250;
        if(!regression)observed_=s.now;
        if(fresh&&!regression){have_sample_=true;sequence_=s.sequence;fresh_at_=s.now;}
        for(unsigned h=0;h<2;++h){
            const bool held=h==0?s.raw.fire_right:s.raw.fire_left;
            const auto binding=h==0?2U:1U;
            if(fresh&&!regression&&!gap&&s.raw.active&&(s.raw.digital_available&binding)!=0&&!held)
                trigger_claims_&=static_cast<std::uint8_t>(~(1U<<h));
        }
        const bool poses=Valid(s.head)&&Valid(s.left)&&Valid(s.right);
        const bool conflict=input.interact||input.quick_save||input.quick_load||input.weapon_next||
            input.weapon_previous||input.hands||input.discard_weapon||input.objectives||input.logs||
            input.alternate_fire||s.raw.weapon_radial||input.weapon_radial||
            std::any_of(input.equipment_select.begin(),input.equipment_select.end(),[](bool x){return x;});
        const bool allowed=s.enabled&&s.allowed&&input.active&&s.raw.active&&poses&&!s.recentered&&!conflict&&
            !regression&&!gap&&(s.context&1U)!=0&&(s.raw.digital_available&3U)==3U;
        const bool available=allowed&&(s.raw.digital_available&8U)!=0&&s.player!=0;
        const bool reload_owner_changed=s.player!=reload_player_||s.context!=reload_context_||
            s.raw.input_context_generation!=reload_generation_||s.native.weapon_id!=reload_weapon_||
            s.native.armed_hand!=reload_hand_;
        if(!available||reload_owner_changed)fire_ready_.fill(false);
        const int armed=s.native.armed_hand;
        const bool empty_fire_edge=available&&fresh&&armed>=0&&armed<2&&fire_ready_[armed]&&
            (armed==0?input.fire_right:input.fire_left)&&s.native.valid&&s.native.probe_valid&&
            s.native.kind!=CoJManualReloadKind::indexed&&
            !s.native.reloading&&s.native.ammo_readback==0&&s.native.reserve_readback>0;
        if(fresh)for(unsigned h=0;h<2;++h) {
            const bool held=h==0?s.raw.fire_right:s.raw.fire_left;
            if(held)fire_ready_[h]=false;
            else if(available)fire_ready_[h]=true;
        }
        // A release from before a menu/tracking/binding/owner loss cannot arm
        // a recovered Square press. Keep physical trigger claims separately.
        if(!available||!reload_available_||reload_owner_changed)reload_ready_=false;
        // Discharging an existing physical claim is independent of admitting a
        // new manual session, including after disabling the feature.
        if(fresh&&!regression&&!gap&&s.raw.active&&(s.raw.digital_available&8U)!=0&&!s.raw.reload)
            reload_claim_=false;
        const bool edge=fresh&&!regression&&!gap&&available&&s.raw.reload&&reload_ready_;
        if(fresh&&!regression){
            reload_available_=available;reload_player_=s.player;reload_context_=s.context;
            reload_generation_=s.raw.input_context_generation;reload_weapon_=s.native.weapon_id;
            reload_hand_=s.native.armed_hand;
            if(available&&!s.raw.reload){reload_ready_=true;reload_claim_=false;}
        }
        if(edge)reload_ready_=false;
        const bool bound=phase_!=CoJManualReloadPhase::ready;
        const bool same=bound&&s.player==player_&&s.context==context_&&
            s.raw.input_context_generation==generation_&&s.native.valid&&s.native.probe_valid&&
            s.native.weapon_id==weapon_&&s.native.armed_hand==hand_&&s.native.kind==kind_;
        const bool recovery=s.recovery.valid&&s.recovery.SupportsManual()&&
            s.recovery.weapon_id==weapon_&&s.recovery.armed_hand==hand_&&s.recovery.kind==kind_;
        const bool ordinary=s.admission.valid&&s.admission.SupportsManual()&&
            s.admission.weapon_id==weapon_&&s.admission.armed_hand==hand_&&s.admission.kind==kind_;
        if(bound&&phase_==CoJManualReloadPhase::closing&&(!same||s.native.probe_status==4||s.native.probe_status==0)){
            phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;
        }else if(bound&&phase_!=CoJManualReloadPhase::closing){
            // Opening on an empty trigger already consumed this physical hold.
            // Only a verified release can arm a subsequent trigger close; do
            // not let the opening press close its own session one frame later.
            const bool fire=(hand_==0?input.fire_right:input.fire_left)&&
                (trigger_claims_&(1U<<hand_))==0;
            if(!allowed||!same||(!recovery&&!ordinary)||edge||fire||cancel_pending_){
                const bool operation_failed=cancel_pending_; // Cancel consumes this pending flag.
                out.cancel=Cancel();out.reason=edge?"reload_button":fire?"fire_close":
                    operation_failed?"native_operation_failed":!poses?"tracking_lost":
                    (s.raw.digital_available&3U)!=3U?"trigger_unavailable":
                    !same?"owner_changed":!s.enabled?"disabled":s.recentered?"recentered":
                    regression?"sample_regression":gap?"sample_gap":conflict?"input_conflict":"context_lost";
                if(edge)reload_claim_=true;
                cancel_pending_=false;
            }else if(s.native.probe_status==3){phase_=CoJManualReloadPhase::closing;out.reason="native_closing";
            }else if(s.native.probe_status==4||s.native.probe_status==0){
                phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;out.reason="native_finished";
            }else if(s.native.probe_status==2){phase_=CoJManualReloadPhase::manual_load;
            }else if(s.native.probe_status==1){phase_=CoJManualReloadPhase::opening;
            }
        }
        if(phase_==CoJManualReloadPhase::ready&&allowed&&((edge&&input.reload)||empty_fire_edge)&&
            s.admission.valid&&s.admission.SupportsManual()&&s.player!=0&&
            s.native.valid&&s.native.probe_valid&&(s.native.probe_status==0||s.native.probe_status==4)&&
            s.native.armed_hand==s.admission.armed_hand&&s.native.weapon_id==s.admission.weapon_id&&
            s.native.kind==s.admission.kind){
            player_=s.player;context_=s.context;generation_=s.raw.input_context_generation;
            weapon_=s.admission.weapon_id;hand_=s.admission.armed_hand;
            kind_=s.native.kind;cartridge_index_=-1;
            replenish_=false;
            phase_=CoJManualReloadPhase::opening;reload_claim_=true;out.start=true;
            out.reason=empty_fire_edge?"empty_trigger":"reload_button";
            input.reload=true;
        }
        const bool owned=phase_!=CoJManualReloadPhase::ready;
        if(s.enabled&&!out.start&&s.raw.reload&&!s.native_fallback_allowed){
            input.reload=false;reload_claim_=true;reload_ready_=false;
            if(!owned)out.reason="manual_admission_rejected";
        }
        if(owned&&hand_>=0&&hand_<2){
            const unsigned support=1U-static_cast<unsigned>(hand_);
            const bool held=support==0?s.raw.fire_right:s.raw.fire_left;
            if(held)trigger_claims_|=static_cast<std::uint8_t>(1U<<support);
            const bool firing=hand_==0?s.raw.fire_right:s.raw.fire_left;
            if(firing)trigger_claims_|=static_cast<std::uint8_t>(1U<<hand_);
            if(support==0)input.fire_right=false;else input.fire_left=false;
        }
        out.tracked=owned&&allowed&&same&&recovery&&!out.cancel&&
            (s.native.probe_status==1||s.native.probe_status==2||s.native.probe_status==3);
        out.supervise=out.tracked&&phase_!=CoJManualReloadPhase::closing;
        runtime::MotionReloadGestureInput gesture{};
        // Reserve is a fresh native observation, never a VR ammunition replica.
        // Keep the native waiting/close route supervised, but withdraw any
        // cartridge offer/carry when no native round remains to transfer.
        const int offered_index=cartridge_index_>=0?cartridge_index_:s.native.selected_index;
        const bool round_available=s.native.valid&&(kind_==CoJManualReloadKind::indexed?
            IndexedAvailable(s.native,offered_index):s.native.reserve_readback>0);
        if(!round_available)replenish_=false;
        gesture.valid=gesture.eligible=out.tracked&&phase_==CoJManualReloadPhase::manual_load&&round_available;
        gesture.player_identity=player_;gesture.weapon_identity=weapon_;
        gesture.armed_hand=static_cast<std::uint8_t>(hand_);
        gesture.head=s.head;gesture.left_grip=s.left;gesture.right_grip=s.right;
        gesture.trigger_available={s.raw.active&&(s.raw.digital_available&2U)!=0,s.raw.active&&(s.raw.digital_available&1U)!=0};
        gesture.trigger_held={s.raw.fire_right,s.raw.fire_left};gesture.input_generation=s.raw.input_context_generation;
        gesture.pose_sequence=s.sequence;gesture.monotonic_ms=s.now;
        gesture.replenish_cartridge=replenish_&&fresh&&gesture.valid&&kind_==CoJManualReloadKind::unit;
        out.gesture=gesture_.Update(gesture);
        if(kind_==CoJManualReloadKind::indexed&&out.gesture.cartridge_held&&cartridge_index_<0)
            cartridge_index_=offered_index;
        if(gesture.replenish_cartridge)replenish_=false;
        out.insert=out.gesture.reload&&gesture.valid&&!out.cancel;
        if(kind_==CoJManualReloadKind::indexed){
            out.selected_index=cartridge_index_;
            if(!out.insert&&!out.gesture.cartridge_held&&out.gesture.claimed_hand>=2)cartridge_index_=-1;
        }
        trigger_claims_|=out.gesture.consumed_trigger_mask;
        if((trigger_claims_&1U)!=0)input.fire_right=false;
        if((trigger_claims_&2U)!=0)input.fire_left=false;
        if(!out.start&&(reload_claim_||owned||
            (s.enabled&&s.native.probe_valid&&s.native.probe_status>0&&s.native.probe_status<4)))input.reload=false;
        return out;
    }
    void NoteStart(bool accepted) noexcept {if(!accepted){phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;}}
    void NoteInsertion(bool accepted) noexcept {replenish_=accepted;cancel_pending_=!accepted;}
    // Resolve the synchronous native call before publishing this frame's
    // ownership/feedback. The insertion flag records intent, never ammo credit.
    void CompleteInsertion(bool completed,bool accepted,CoJManualReloadOutput& out) noexcept {
        NoteInsertion(completed&&accepted);
        if(completed&&accepted){
            cartridge_index_=-1;
            if(kind_!=CoJManualReloadKind::unit)replenish_=false;
            if(kind_==CoJManualReloadKind::clip){
                phase_=CoJManualReloadPhase::closing;out.supervise=false;
                out.gesture={};out.reason="native_clip_closed";
            }
        }
        if(!completed||!accepted){
            (void)Cancel();out.cancel=true;out.tracked=false;out.supervise=false;
            out.reason=completed?"native_insertion_rejected":"native_insertion_failed";
            out.gesture.insertion_ready=false;out.gesture.insertion_zone_entered=false;
            out.gesture.insertion_zone_exited=false;
        }
    }
    bool Cancel() noexcept {
        cancel_pending_=false;
        if(phase_==CoJManualReloadPhase::ready||phase_==CoJManualReloadPhase::closing)return false;
        phase_=CoJManualReloadPhase::closing;replenish_=false;cartridge_index_=-1;return true;
    }
    CoJManualReloadPhase phase() const noexcept {return phase_;}
    int selected_index() const noexcept {return cartridge_index_;}
    runtime::MotionReloadGestureStage gesture_stage() const noexcept {return gesture_.stage();}
private:
    static bool IndexedAvailable(const CoJReloadTraceSnapshot& n,int index) noexcept {
        if(n.index_count!=2||index<0||index>=2)return false;
        const int slot=CoJReloadReserveSlot(n.reserve_kinds[index]);
        return slot>=0&&slot<3&&n.loaded[index]>=0&&n.capacities[index]>0&&
            n.loaded[index]<n.capacities[index]&&n.reserves[slot]>0;
    }
    static bool Valid(const runtime::Pose& p) noexcept {
        const auto q=p.orientation;const auto v=p.position;
        return p.position_valid&&p.orientation_valid&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
            std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w)&&
            static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w>1.e-12;
    }
    runtime::MotionReloadGesture gesture_;
    CoJManualReloadPhase phase_=CoJManualReloadPhase::ready;
    std::uint64_t player_=0,weapon_=0,context_=0,generation_=0,sequence_=0,fresh_at_=0,observed_=0;
    std::uint64_t reload_player_=0,reload_context_=0,reload_generation_=0,reload_weapon_=0;
    int reload_hand_=-1;
    int hand_=-1;
    int cartridge_index_=-1;
    CoJManualReloadKind kind_=CoJManualReloadKind::unit;
    std::uint8_t trigger_claims_=0;
    std::array<bool,2> fire_ready_{};
    bool have_sample_=false,reload_available_=false,reload_ready_=false,reload_claim_=false,replenish_=false,cancel_pending_=false;
};
}
