#pragma once
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
#include "runtime/vr_math.hpp"
#include <string_view>
namespace cojvr::games::call_of_juarez {
inline constexpr double kCoJManualGripInsertionRadiusM = 0.18;
inline bool CoJReloadChestCentre(const runtime::Pose& head,runtime::Vec3& centre) noexcept {
    constexpr float chest_forward=.21F, chest_below_head=.34F;
    const auto right=runtime::RotateVector(runtime::NormalizeQuaternion(head.orientation),{1,0,0});
    const float width=std::hypot(right.x,right.z);
    if(!head.position_valid||!head.orientation_valid||!std::isfinite(width)||width<.5F)return false;
    centre={head.position.x+chest_forward*right.z/width,
        head.position.y-chest_below_head,head.position.z-chest_forward*right.x/width};
    return std::isfinite(centre.x)&&std::isfinite(centre.y)&&std::isfinite(centre.z);
}
inline int SelectCoJDualSchofieldHand(const CoJReloadTraceSnapshot& right,
    const runtime::Pose& head,const runtime::Pose& right_grip,const runtime::Pose& left_grip) noexcept {
    runtime::Vec3 chest{};
    if(!right.valid||right.armed_hand!=0||!right.support_weapon_id||
        right.ammo_readback<0||right.ammo_readback>6||right.support_loaded<0||right.support_loaded>6||
        !right_grip.position_valid||!left_grip.position_valid||!CoJReloadChestCentre(head,chest))return -1;
    const auto distance=[&](const runtime::Pose& grip){
        return std::hypot(grip.position.x-chest.x,grip.position.y-chest.y,grip.position.z-chest.z);
    };
    const float r=distance(right_grip),l=distance(left_grip);
    if(!std::isfinite(r)||!std::isfinite(l))return -1;
    return l<r?1:0;
}
enum class CoJManualReloadPhase { ready, opening, manual_load, closing };
enum class CoJReloadRefusal { none, reserve_full, native_guard };
struct CoJSchofieldMotionSample {
    const char* state="inactive";
    std::uint64_t interval_ms=0;
    float down_m=0,back_m=0,aim_y=0,pitch_delta=0,step_m=0,progress_m=0,peak_mps=0;
    bool classified=false;
};
struct CoJWinchesterReceiverInput {
    runtime::Vec3 centre{}; // Current tracking-space capture.
    float outside_cm=0, alignment=0;
    bool mouth_valid=false, tip_valid=false, within_capture=false;
};
struct CoJManualReloadInput {
    bool enabled=false, allowed=false, recentered=false;
    bool schofield_motion=false;
    bool winchester_receiver=false;
    bool native_cleanup_pending=false;
    bool native_fallback_allowed=false; // Positive unsupported context; never a failed read.
    bool native_reload_held=false; // Last applied action31, independent of physical Square.
    std::uint64_t player=0,context=0,sequence=0,now=0;
    runtime::GameplayInputState raw{};
    runtime::Pose head{},left{},right{},armed_aim{};
    CoJWinchesterReceiverInput receiver{};
    CoJMotionReloadOwner admission{},recovery{},after_shot{};
    CoJReloadTraceSnapshot native{};
    bool AllowsNativeFallback() const noexcept {
        return native_fallback_allowed&&
            !(native.valid&&native.weapon_class&&IsCoJSchofield(native.weapon_class));
    }
};
// Receipt for this sample's active gesture centre, in tracking metres. This
// is the current grip or the measured Winchester exterior receiver reference.
// Presentation must consume the receipt instead of choosing another locator.
struct CoJManualReloadGestureTarget {
    runtime::Vec3 position{};
    std::uint64_t player=0,context=0,weapon=0,input_generation=0,sequence=0,observed_ms=0;
    int armed_hand=-1;
    bool valid=false;
};
struct CoJManualReloadOutput {
    bool start=false,cancel=false,insert=false,tracked=false,supervise=false;
    bool schofield_motion=false,unload=false,eject_empty=false,chest_insert=false;
    std::uint8_t dry_fire_mask=0; // 0 right, 1 left, one pulse per fresh empty press.
    CoJSchofieldMotionSample schofield_sample{};
    CoJReloadRefusal unload_refusal=CoJReloadRefusal::none;
    std::uint32_t transition_controls=0; // Gameplay digital bits held by the preference transition.
    int selected_index=-1; // Captured at indexed pickup, never inferred at release.
    runtime::MotionReloadGestureOutput gesture{};
    CoJManualReloadGestureTarget target{};
    const char* reason="none";
};
class CoJManualReloadSession {
public:
    // UI/native ownership and ammunition are observations, not state replicas.
    CoJManualReloadOutput Update(runtime::GameplayInputState& input,const CoJManualReloadInput& s) noexcept {
        CoJManualReloadOutput out{};
        constexpr std::uint32_t reload_controls=1U|2U|8U;
        native_cleanup_pending_=s.native_cleanup_pending;
        if(s.native_cleanup_pending)transition_release_|=known_controls_;
        const bool source_valid=s.context&&s.raw.input_context_generation&&s.player&&s.sequence&&s.now;
        const std::array<std::uint64_t,3> source{s.context,s.raw.input_context_generation,s.player};
        if(source_valid&&source<trigger_source_){
            out.cancel=Cancel();out.reason="retired_trigger_source";
            out.transition_controls=reload_controls;
            input.fire_left=input.fire_right=input.reload=false;
            out.gesture.consumed_trigger_mask=trigger_claims_;
            out.gesture.consume_free_trigger=trigger_claims_!=0;
            return out;
        }
        const bool source_changed=source_valid&&source!=trigger_source_;
        const bool replacement=source_changed&&trigger_source_!=std::array<std::uint64_t,3>{};
        if(source_changed){
            trigger_source_=source;have_sample_=false;
            // Retire old gesture clocks/authority, but keep physical holds.
            // The replacement baseline cannot release them or rearm Square.
            gesture_={};replenish_=false;cartridge_index_=-1;
            reload_ready_=reload_available_=false;fire_ready_.fill(false);
        }
        const bool regression=source_valid&&have_sample_&&(s.sequence<sequence_||s.now<observed_);
        const bool fresh=source_valid&&(!have_sample_||s.sequence>sequence_);
        const auto sample_elapsed=source_valid&&have_sample_&&s.now>=fresh_at_?s.now-fresh_at_:0;
        const bool gap=sample_elapsed>250;
        if(fresh&&!regression)observed_=s.now;
        if(fresh&&!regression){have_sample_=true;sequence_=s.sequence;fresh_at_=s.now;}
        const auto prior_trigger_claims=trigger_claims_;
        std::uint8_t released_triggers=0;
        for(unsigned h=0;h<2;++h){
            const bool held=h==0?s.raw.fire_right:s.raw.fire_left;
            const auto binding=h==0?2U:1U;
            if(!source_changed&&fresh&&!regression&&!gap&&s.raw.active&&(s.raw.digital_available&binding)!=0&&!held){
                trigger_claims_&=static_cast<std::uint8_t>(~(1U<<h));
                released_triggers|=static_cast<std::uint8_t>(1U<<h);
            }
        }
        const bool poses=Valid(s.head)&&Valid(s.left)&&Valid(s.right);
        const std::uint32_t pressed=(s.raw.fire_left?1U:0U)|(s.raw.fire_right?2U:0U)|(s.raw.reload?8U:0U);
        const auto observed=s.raw.active?s.raw.digital_available&reload_controls:0U;
        if(source_valid&&fresh&&!regression&&!gap)known_controls_|=observed;
        const bool disabled=enabled_&&!s.enabled;
        enabled_=s.enabled;
        if(disabled||s.native_cleanup_pending)transition_release_|=known_controls_|pressed;
        out.transition_controls=s.native_cleanup_pending?reload_controls:transition_release_;
        if(transition_release_) {
            // Each physical channel recovers on its own observed release.
            if(!s.native_cleanup_pending&&!source_changed&&fresh&&!regression&&!gap&&
                s.raw.active&&input.active&&poses&&!s.recentered&&(s.context&1U)!=0)
                transition_release_&=~(observed&~pressed);
        }
        const bool conflict=input.interact||input.quick_save||input.quick_load||input.weapon_next||
            input.weapon_previous||input.hands||input.discard_weapon||input.objectives||input.logs||
            input.alternate_fire||s.raw.weapon_radial||input.weapon_radial||
            std::any_of(input.equipment_select.begin(),input.equipment_select.end(),[](bool x){return x;});
        const bool allowed=s.enabled&&!out.transition_controls&&s.allowed&&input.active&&s.raw.active&&poses&&!s.recentered&&!conflict&&
            source_valid&&!replacement&&!regression&&!gap&&(s.context&1U)!=0&&(s.raw.digital_available&3U)==3U;
        const bool available=allowed&&(s.raw.digital_available&8U)!=0&&s.player!=0;
        const bool same_hand=s.native.weapon_id==reload_weapon_&&s.native.armed_hand==reload_hand_&&
            s.native.support_weapon_id==reload_support_weapon_;
        const bool same_pair=s.native.support_weapon_id&&reload_support_weapon_&&s.native.weapon_id==reload_support_weapon_&&
            s.native.support_weapon_id==reload_weapon_&&s.native.armed_hand==1-reload_hand_;
        const bool reload_owner_changed=s.player!=reload_player_||s.context!=reload_context_||
            s.raw.input_context_generation!=reload_generation_||(!same_hand&&!same_pair);
        if(!available||reload_owner_changed)fire_ready_.fill(false);
        const int armed=s.native.armed_hand;
        const bool idle_dry=allowed&&phase_==CoJManualReloadPhase::ready&&armed>=0&&armed<2&&
            s.native.valid&&s.native.probe_valid&&(s.native.probe_status==0||s.native.probe_status==4)&&
            !s.native.reloading&&(s.native.weapon_class||MatchesIdleProbe(s.admission,s.native));
        if(idle_dry)for(unsigned h=0;h<2;++h){
            const bool primary=h==static_cast<unsigned>(armed);
            const bool empty=primary?(s.native.ammo_readback==0&&
                (s.native.kind!=CoJManualReloadKind::indexed||s.native.loaded==std::array<int,2>{})):
                s.native.support_weapon_id&&s.native.support_loaded==0;
            if(!empty||!(h==0?input.fire_right:input.fire_left))continue;
            const auto mask=static_cast<std::uint8_t>(1U<<h);
            if(fresh&&fire_ready_[h]&&!s.raw.reload)out.dry_fire_mask|=mask;
            trigger_claims_|=mask;
        }
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
        if(!source_changed&&fresh&&!regression&&!gap&&s.raw.active&&(s.raw.digital_available&8U)!=0&&!s.raw.reload)
            reload_claim_=false;
        const bool edge=fresh&&!regression&&!gap&&available&&s.raw.reload&&reload_ready_;
        if(fresh&&!regression){
            reload_available_=available;reload_player_=s.player;reload_context_=s.context;
            reload_generation_=s.raw.input_context_generation;reload_weapon_=s.native.weapon_id;
            reload_support_weapon_=s.native.support_weapon_id;
            reload_hand_=s.native.armed_hand;
            if(available&&!s.raw.reload){reload_ready_=true;reload_claim_=false;}
        }
        if(edge)reload_ready_=false;
        const bool bound=phase_!=CoJManualReloadPhase::ready;
        const bool same=bound&&s.player==player_&&s.context==context_&&
            s.raw.input_context_generation==generation_&&s.native.valid&&s.native.probe_valid&&
            s.native.weapon_id==weapon_&&s.native.armed_hand==hand_&&s.native.kind==kind_&&
            s.native.support_weapon_id==support_weapon_;
        const bool recovery=s.recovery.valid&&s.recovery.SupportsManual()&&
            s.recovery.weapon_id==weapon_&&s.recovery.armed_hand==hand_&&s.recovery.kind==kind_&&
            s.recovery.support_weapon_id==support_weapon_;
        const bool ordinary=s.admission.valid&&s.admission.SupportsManual()&&
            s.admission.weapon_id==weapon_&&s.admission.armed_hand==hand_&&s.admission.kind==kind_&&
            s.admission.support_weapon_id==support_weapon_;
        const bool schofield=s.schofield_motion&&s.native.kind==CoJManualReloadKind::clip&&
            s.native.weapon_class&&IsCoJSchofield(s.native.weapon_class);
        if(bound&&phase_==CoJManualReloadPhase::closing&&(!same||s.native.probe_status==4||s.native.probe_status==0)){
            phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;
        }else if(bound&&phase_!=CoJManualReloadPhase::closing){
            // Only the native waiting phase accepts a trigger close. Opening
            // and per-cartridge replay use phase 1, so a fresh press there is
            // consumed through release even when the next sample is waiting.
            const bool fire=!support_weapon_&&s.native.probe_status==2&&(hand_==0?input.fire_right:input.fire_left)&&
                (trigger_claims_&(1U<<hand_))==0;
            if(!allowed||!same||(!recovery&&!ordinary)||edge||fire||cancel_pending_||
                (schofield_motion_&&!schofield)){
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
        const bool manual_idle=MatchesIdleProbe(s.admission,s.native);
        const bool shot_tail=MatchesIdleProbe(s.after_shot,s.native)&&!s.native.reloading;
        const bool armed_fire=armed==0?s.raw.fire_right:s.raw.fire_left;
        constexpr std::uint64_t kShotTailReloadWindowMs=450;
        if(after_shot_at_&&(!available||reload_owner_changed||armed_fire||
            (manual_idle&&s.native_reload_held)||
            s.now<after_shot_at_||s.now-after_shot_at_>kShotTailReloadWindowMs||
            (!shot_tail&&(!manual_idle||s.native.reloading))))after_shot_at_=0;
        if(phase_==CoJManualReloadPhase::ready&&edge&&input.reload&&shot_tail&&!armed_fire&&!s.native_reload_held)
            after_shot_at_=s.now;
        const bool deferred_start=after_shot_at_&&fresh&&manual_idle;
        if(phase_==CoJManualReloadPhase::ready&&allowed&&manual_idle&&
            !s.native_reload_held&&((edge&&input.reload)||deferred_start)){
            after_shot_at_=0;
            player_=s.player;context_=s.context;generation_=s.raw.input_context_generation;
            weapon_=s.admission.weapon_id;hand_=s.admission.armed_hand;
            support_weapon_=s.admission.support_weapon_id;
            kind_=s.native.kind;cartridge_index_=-1;
            schofield_motion_=schofield;schofield_chambers_cleared_=false;
            schofield_sweep_={};
            replenish_=false;
            phase_=CoJManualReloadPhase::opening;reload_claim_=true;out.start=true;
            out.reason=deferred_start?"reload_after_shot":"reload_button";
            input.reload=true;
        }
        const bool owned=phase_!=CoJManualReloadPhase::ready;
        if(s.enabled&&!out.start&&s.raw.reload&&!s.AllowsNativeFallback()){
            input.reload=false;reload_claim_=true;reload_ready_=false;
            if(!owned)out.reason=after_shot_at_?"reload_waiting_for_shot":"manual_admission_rejected";
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
        out.schofield_motion=owned&&schofield_motion_;
        const bool waiting=out.tracked&&phase_==CoJManualReloadPhase::manual_load;
        const bool carried=gesture_.stage()==runtime::MotionReloadGestureStage::carrying||
            gesture_.stage()==runtime::MotionReloadGestureStage::replenished_carrying;
        out.schofield_sample.interval_ms=sample_elapsed;
        if(!waiting||!out.schofield_motion||s.raw.fire_left||s.raw.fire_right||carried){
            if(out.schofield_motion)out.schofield_sample.state=!waiting?
                (out.cancel?out.reason:phase_==CoJManualReloadPhase::opening?"opening":"not_waiting"):
                carried?"cartridge_carried":"trigger_held";
            schofield_sweep_={};
        }else if(fresh){
            const auto sweep=UpdateSchofieldSweep(s,out.schofield_sample);
            if(sweep==SchofieldFlick::close){
                out.cancel=Cancel();out.supervise=false;out.tracked=false;out.reason="schofield_wrist_close";
            }else if(sweep==SchofieldFlick::eject){
                if(s.native.ammo_readback>0||s.native.schofield_spent>0){
                    out.unload=true;out.reason="schofield_eject";
                }
                else if(s.native.ammo_readback==0&&!schofield_chambers_cleared_){
                    out.eject_empty=true;
                    out.reason="schofield_eject";
                    schofield_chambers_cleared_=true;
                }
            }
        }else out.schofield_sample.state="duplicate_sample";
        runtime::MotionReloadGestureInput gesture{};
        // Reserve is a fresh native observation, never a VR ammunition replica.
        // Keep the native waiting/close route supervised, but withdraw any
        // cartridge offer/carry when no native round remains to transfer.
        const int offered_index=cartridge_index_>=0?cartridge_index_:s.native.selected_index;
        const bool round_available=s.native.valid&&(kind_==CoJManualReloadKind::indexed?
            IndexedAvailable(s.native,offered_index):s.native.reserve_readback>0);
        const bool chambers_available=!out.schofield_motion||s.native.schofield_spent==0;
        if(!round_available)replenish_=false;
        const bool chest_eligible=out.tracked&&out.schofield_motion&&
            phase_==CoJManualReloadPhase::manual_load&&round_available&&chambers_available&&s.native.ammo_readback>=0&&
            s.native.ammo_readback<s.native.capacities[0]&&!out.cancel&&!out.unload&&!out.eject_empty&&
            !carried&&!s.raw.fire_left&&!s.raw.fire_right;
        if(!chest_eligible)chest_={};
        else if(fresh)out.chest_insert=UpdateChestEntry(s);
        gesture.valid=gesture.eligible=!support_weapon_&&out.tracked&&phase_==CoJManualReloadPhase::manual_load&&
            round_available&&chambers_available&&!out.unload&&!out.eject_empty&&
            (!out.schofield_motion||(s.native.ammo_readback>=0&&s.native.ammo_readback<s.native.capacities[0]));
        gesture.player_identity=player_;gesture.weapon_identity=weapon_;
        gesture.armed_hand=static_cast<std::uint8_t>(hand_);
        gesture.head=s.head;gesture.left_grip=s.left;gesture.right_grip=s.right;
        gesture.trigger_available={s.raw.active&&(s.raw.digital_available&2U)!=0,s.raw.active&&(s.raw.digital_available&1U)!=0};
        gesture.trigger_held={s.raw.fire_right,s.raw.fire_left};gesture.input_generation=s.raw.input_context_generation;
        gesture.pose_sequence=s.sequence;gesture.monotonic_ms=s.now;
        gesture.replenish_cartridge=replenish_&&fresh&&gesture.valid&&kind_==CoJManualReloadKind::unit;
        gesture.grip_insertion_radius_m=kCoJManualGripInsertionRadiusM;
        if(s.winchester_receiver)
            gesture.adapter_insertion_contact=gesture.valid&&carried&&fresh&&s.receiver.mouth_valid&&
                s.receiver.tip_valid&&s.receiver.within_capture;
        out.gesture=gesture_.Update(gesture);
        if(gesture.valid&&fresh&&!regression&&!gap&&out.gesture.cartridge_held&&
            (!s.winchester_receiver||s.receiver.mouth_valid)){
            out.target.position=s.winchester_receiver?s.receiver.centre:
                gesture.armed_hand==0?gesture.right_grip.position:gesture.left_grip.position;
            out.target.player=gesture.player_identity;out.target.context=s.context;
            out.target.weapon=gesture.weapon_identity;out.target.input_generation=gesture.input_generation;
            out.target.sequence=gesture.pose_sequence;out.target.armed_hand=gesture.armed_hand;
            out.target.observed_ms=s.now;
            out.target.valid=true;
        }
        if(kind_==CoJManualReloadKind::indexed&&out.gesture.cartridge_held&&cartridge_index_<0)
            cartridge_index_=offered_index;
        if(gesture.replenish_cartridge)replenish_=false;
        out.insert=out.chest_insert||(out.gesture.reload&&gesture.valid&&!out.cancel);
        if(kind_==CoJManualReloadKind::indexed){
            // The release sample can still consume the trigger after it ends
            // the carry. Keep the slot only for a carried or submitted round.
            if(!out.insert&&!out.gesture.cartridge_held)cartridge_index_=-1;
            out.selected_index=cartridge_index_;
        }
        out.gesture.consumed_trigger_mask|=prior_trigger_claims&released_triggers;
        out.gesture.consume_free_trigger=out.gesture.consumed_trigger_mask!=0;
        // Gesture output consumes its final release sample, but that verified
        // release must not recreate a persistent hold after cancellation.
        trigger_claims_|=out.gesture.consumed_trigger_mask&static_cast<std::uint8_t>(~released_triggers);
        if((trigger_claims_&1U)!=0)input.fire_right=false;
        if((trigger_claims_&2U)!=0)input.fire_left=false;
        if(!out.start&&(reload_claim_||owned||
            (s.enabled&&s.native.probe_valid&&s.native.probe_status>0&&s.native.probe_status<4)))input.reload=false;
        if(out.transition_controls&1U)input.fire_left=false;
        if(out.transition_controls&2U)input.fire_right=false;
        if(out.transition_controls&8U)input.reload=false;
        return out;
    }
    void NoteStart(bool accepted) noexcept {if(!accepted){phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;}}
    void NoteInsertion(bool accepted) noexcept {replenish_=accepted;cancel_pending_=!accepted;}
    void NoteSchofieldUnloadConfirmed() noexcept {schofield_chambers_cleared_=true;}
    // Resolve the synchronous native call before publishing this frame's
    // ownership/feedback. The insertion flag records intent, never ammo credit.
    void CompleteInsertion(bool completed,bool accepted,CoJManualReloadOutput& out) noexcept {
        out.target={};
        NoteInsertion(completed&&accepted);
        if(completed&&accepted){
            cartridge_index_=-1;
            if(kind_!=CoJManualReloadKind::unit)replenish_=false;
            if(kind_==CoJManualReloadKind::clip&&schofield_motion_){
                if(!out.chest_insert||!schofield_sweep_.locked)schofield_sweep_={};
                out.gesture={};out.reason="schofield_bundle_inserted";
            }else if(kind_==CoJManualReloadKind::clip){
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
        after_shot_at_=0;
        cancel_pending_=false;
        if(phase_==CoJManualReloadPhase::ready||phase_==CoJManualReloadPhase::closing)return false;
        phase_=CoJManualReloadPhase::closing;replenish_=false;cartridge_index_=-1;return true;
    }
    CoJManualReloadPhase phase() const noexcept {return phase_;}
    bool native_cleanup_pending() const noexcept {return native_cleanup_pending_;}
    bool waiting_for_shot_reload() const noexcept {return after_shot_at_!=0;}
    int selected_index() const noexcept {return cartridge_index_;}
    int paired_hand() const noexcept {return phase_!=CoJManualReloadPhase::ready&&support_weapon_?hand_:-1;}
    runtime::MotionReloadGestureStage gesture_stage() const noexcept {return gesture_.stage();}
private:
    bool UpdateChestEntry(const CoJManualReloadInput& s) noexcept {
        constexpr float enter_radius=.11F;
        constexpr float exit_radius=.23F;
        const auto grip=hand_==0?s.right.position:s.left.position;
        // Use the headset's horizontal right axis so looking down does not
        // pull the target upward or toward the controller.
        runtime::Vec3 centre{};
        if(!CoJReloadChestCentre(s.head,centre)){chest_={};return false;}
        const runtime::Vec3 offset{grip.x-centre.x,grip.y-centre.y,grip.z-centre.z};
        const float distance=std::hypot(offset.x,offset.y,offset.z);
        const auto prior=chest_;
        chest_={.grip=grip,.offset=offset,.at=s.now};
        if(!prior.at||s.now<=prior.at||s.now-prior.at>90)return false;
        const runtime::Vec3 step{grip.x-prior.grip.x,grip.y-prior.grip.y,grip.z-prior.grip.z};
        const float grip_step=std::hypot(step.x,step.y,step.z);
        const float relative_step=std::hypot(offset.x-prior.offset.x,offset.y-prior.offset.y,
            offset.z-prior.offset.z);
        const float max_step=std::min(.12F,.003F*static_cast<float>(s.now-prior.at));
        if(grip_step>max_step||relative_step>max_step)return false;
        if(distance>=exit_radius){chest_.arm_grip=grip;chest_.outside=true;return false;}
        if(!prior.outside)return false;
        if(distance>enter_radius){chest_.arm_grip=prior.arm_grip;chest_.outside=true;return false;}
        const float prior_distance=std::hypot(prior.offset.x,prior.offset.y,prior.offset.z);
        const float against_prior_centre=std::hypot(prior.offset.x+step.x,
            prior.offset.y+step.y,prior.offset.z+step.z);
        // Require motion of the gun hand as well as a relative zone crossing.
        // This permits slow approaches and rejects a stationary gun during a lean.
        const float total_inward=-((grip.x-prior.arm_grip.x)*prior.offset.x+
            (grip.y-prior.arm_grip.y)*prior.offset.y+(grip.z-prior.arm_grip.z)*prior.offset.z);
        return prior_distance>enter_radius&&against_prior_centre<=enter_radius&&total_inward>=.03F*prior_distance;
    }
    enum class SchofieldFlick { none, eject, close };
    SchofieldFlick UpdateSchofieldSweep(const CoJManualReloadInput& s,CoJSchofieldMotionSample& sample) noexcept {
        const auto& grip=hand_==0?s.right:s.left;
        const auto position=runtime::Vec3{grip.position.x-s.head.position.x,
            grip.position.y-s.head.position.y,grip.position.z-s.head.position.z};
        if(!Valid(s.armed_aim)){
            sample.state="aim_unavailable";
            schofield_sweep_={.locked=true};return SchofieldFlick::none;
        }
        const auto aim=runtime::RotateVector(runtime::NormalizeQuaternion(s.armed_aim.orientation),{0,0,-1});
        const float aim_pitch=std::asin(std::clamp(aim.y,-1.F,1.F));
        const auto prior=schofield_sweep_;
        // Rearward tilt uses a positional gate with hysteresis. Closure
        // uses a deliberate down-forward stroke only after returning to rest.
        constexpr float eject_tilt_y=.57F; // about 35 degrees above level
        constexpr float neutral_tilt_y=.26F; // about 15 degrees above level
        const SchofieldSweep baseline{.position=position,.last=s.now,.pitch=aim_pitch,
            .tilt_ready=aim.y<=neutral_tilt_y};
        schofield_sweep_.last=s.now;schofield_sweep_.position=position;schofield_sweep_.pitch=aim_pitch;
        const auto dt=s.now-prior.last;
        sample.aim_y=aim.y;
        sample.interval_ms=prior.last?dt:0;
        const auto lock_until_rest=[&]{
            schofield_sweep_=baseline;schofield_sweep_.locked=true;
        };
        if(!prior.last||!dt||dt>90){
            sample.state=prior.last?"sweep_gap":"baseline";
            schofield_sweep_=baseline;
            // A raised baseline has no observed outward stroke. Lowering it
            // to level must first re-arm the detector, even below eject tilt.
            if(!baseline.tilt_ready||prior.locked||prior.start||prior.raised_samples)lock_until_rest();
            return SchofieldFlick::none;
        }
        const runtime::Vec3 step{position.x-prior.position.x,position.y-prior.position.y,position.z-prior.position.z};
        const float distance=std::hypot(step.x,step.y,step.z);
        const float velocity_scale=1000.F/static_cast<float>(dt);
        const auto speed=distance*velocity_scale;
        const float pitch=aim_pitch-prior.pitch;
        // Use the hand's horizontal reach from the player, independent of
        // looking down or turning the head to inspect the open cylinder.
        const float reach=std::hypot(prior.position.x,prior.position.z);
        const float back=reach>=.12F?-(step.x*prior.position.x+step.z*prior.position.z)/reach:0.F;
        sample.down_m=-step.y;sample.back_m=back;sample.pitch_delta=pitch;sample.step_m=distance;
        if(distance>.12F||speed>4.F){
            sample.state="pose_jump";lock_until_rest();return SchofieldFlick::none;
        }
        // Small returns below the speed floor still reduce net travel.
        constexpr float wrist_radius=.14F;
        const float reverse=std::max({pitch*wrist_radius,step.y,back});
        if(prior.start&&reverse>0.F)
            schofield_sweep_.distance=std::max(0.F,prior.distance-reverse);
        sample.progress_m=schofield_sweep_.distance;
        const bool settled=speed<=.20F&&std::abs(pitch)*velocity_scale<=.9F;
        if(prior.locked){
            const float rest_travel=std::hypot(position.x-prior.rest_position.x,
                position.y-prior.rest_position.y,position.z-prior.rest_position.z);
            const bool inside_rest=prior.rest_since&&rest_travel<=.035F&&std::abs(aim_pitch-prior.rest_pitch)<=.055F;
            if(aim.y<=neutral_tilt_y&&(inside_rest||settled)){
                sample.state="settling";
                if(!inside_rest){
                    schofield_sweep_.rest_since=s.now;
                    schofield_sweep_.rest_position=position;
                    schofield_sweep_.rest_pitch=aim_pitch;
                }else if(settled&&s.now-prior.rest_since>=110)schofield_sweep_=baseline;
            }else{
                sample.state="locked_until_rest";
                schofield_sweep_.rest_since=0;
            }
            return SchofieldFlick::none;
        }
        if(aim.y>=eject_tilt_y){
            if(!prior.tilt_ready){
                sample.state="tilt_needs_neutral";lock_until_rest();return SchofieldFlick::none;
            }
            sample.state="tilt_back";
            // Require two distinct fresh samples above the threshold.
            if(++schofield_sweep_.raised_samples<2)return SchofieldFlick::none;
            sample.classified=true;lock_until_rest();return SchofieldFlick::eject;
        }
        if(prior.raised_samples){
            sample.state="tilt_interrupted";lock_until_rest();return SchofieldFlick::none;
        }
        if(aim.y<=neutral_tilt_y)schofield_sweep_.tilt_ready=true;
        if(settled){
            sample.state="rest";
            if(!prior.rest_since)schofield_sweep_.rest_since=prior.last;
            else if(s.now-prior.rest_since>=110){
                const bool tilt_ready=schofield_sweep_.tilt_ready;
                schofield_sweep_=baseline;schofield_sweep_.tilt_ready=tilt_ready;
            }
            return SchofieldFlick::none;
        }
        schofield_sweep_.rest_since=0;
        // Accept a downward wrist flick at the same position or while the
        // hand moves down and away.
        const float down=-step.y,away=-back;
        constexpr float inverse_sqrt2=.70710678F;
        const float diagonal=(down+away)*inverse_sqrt2;
        const bool closing_wrist=speed<=.20F||(back<=0.F&&down>=0.F&&diagonal>=.65F*distance);
        const float progress=closing_wrist?-pitch*wrist_radius:0.F;
        const float axial_speed=progress*velocity_scale;
        if(prior.start){
            // Apply the speed floor to opposition even when it cannot issue
            // either command on its own.
            if(reverse*velocity_scale>=.22F){
                sample.state="rebound";
                schofield_sweep_=baseline;
                if(prior.distance>=.025F)lock_until_rest();
                return SchofieldFlick::none;
            }
            if(s.now-prior.start>180){
                sample.state="stroke_timeout";schofield_sweep_=baseline;return SchofieldFlick::none;
            }
        }
        if(axial_speed<.22F){sample.state="below_axis_speed";return SchofieldFlick::none;}
        if(!prior.start){
            schofield_sweep_.start=prior.last;schofield_sweep_.distance=0;
            schofield_sweep_.peak_speed=0;
        }
        schofield_sweep_.distance+=progress;
        schofield_sweep_.peak_speed=std::max(axial_speed,schofield_sweep_.peak_speed);
        sample.progress_m=schofield_sweep_.distance;
        sample.peak_mps=schofield_sweep_.peak_speed;
        sample.state="down_forward";
        // The finished Sense trace included coherent wrist strokes near
        // 0.029–0.033 m of equivalent arc at 0.38–0.44 m/s.
        if(schofield_sweep_.distance<.028F||
            schofield_sweep_.peak_speed<.36F)return SchofieldFlick::none;
        sample.classified=true;lock_until_rest();return SchofieldFlick::close;
    }
    static bool MatchesIdleProbe(const CoJMotionReloadOwner& owner,const CoJReloadTraceSnapshot& native) noexcept {
        return owner.valid&&owner.SupportsManual()&&native.valid&&native.probe_valid&&
            (native.probe_status==0||native.probe_status==4)&&native.weapon_id==owner.weapon_id&&
            native.armed_hand==owner.armed_hand&&native.kind==owner.kind&&native.support_weapon_id==owner.support_weapon_id;
    }
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
    std::uint64_t reload_player_=0,reload_context_=0,reload_generation_=0,reload_weapon_=0,reload_support_weapon_=0;
    std::uint64_t support_weapon_=0;
    std::uint64_t after_shot_at_=0;
    int reload_hand_=-1;
    int hand_=-1;
    int cartridge_index_=-1;
    CoJManualReloadKind kind_=CoJManualReloadKind::unit;
    struct ChestEntry {
        runtime::Vec3 grip{},offset{},arm_grip{};
        std::uint64_t at=0;
        bool outside=false;
    } chest_{};
    struct SchofieldSweep {
        runtime::Vec3 position{},rest_position{};
        std::uint64_t last=0,start=0,rest_since=0;
        float pitch=0,rest_pitch=0,distance=0,peak_speed=0;
        unsigned raised_samples=0;
        bool tilt_ready=false,locked=false;
    } schofield_sweep_{};
    bool schofield_motion_=false,schofield_chambers_cleared_=false;
    std::uint8_t trigger_claims_=0;
    std::uint32_t known_controls_=0,transition_release_=0;
    std::array<std::uint64_t,3> trigger_source_{};
    std::array<bool,2> fire_ready_{};
    bool have_sample_=false,reload_available_=false,reload_ready_=false,reload_claim_=false,replenish_=false,cancel_pending_=false;
    bool enabled_=false,native_cleanup_pending_=false;
};
}
