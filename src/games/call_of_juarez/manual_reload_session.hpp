#pragma once
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
#include "runtime/vr_math.hpp"
#include <string_view>
namespace cojvr::games::call_of_juarez {
inline constexpr double kCoJManualGripInsertionRadiusM = 0.18;
enum class CoJManualReloadPhase { ready, opening, manual_load, closing };
enum class CoJReloadRefusal { none, reserve_full, native_guard };
struct CoJSchofieldMotionSample {
    const char* state="inactive";
    std::uint64_t interval_ms=0;
    float down_m=0,back_m=0,aim_y=0,pitch_delta=0,step_m=0,progress_m=0,peak_mps=0;
    bool classified=false;
};
struct CoJWinchesterReceiverInput {
    runtime::Vec3 centre{}, lever{}; // Current tracking-space capture.
    float outside_cm=0, alignment=0;
    bool mouth_valid=false, tip_valid=false, within_capture=false, lever_valid=false;
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
    bool schofield_motion=false,unload=false,eject_empty=false;
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
        if(!source_changed&&fresh&&!regression&&!gap&&s.raw.active&&(s.raw.digital_available&8U)!=0&&!s.raw.reload)
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
        const bool schofield=s.schofield_motion&&s.native.kind==CoJManualReloadKind::clip&&
            s.native.weapon_class&&std::string_view(s.native.weapon_class)=="WeaponPistolSchofield";
        if(bound&&phase_==CoJManualReloadPhase::closing&&(!same||s.native.probe_status==4||s.native.probe_status==0)){
            phase_=CoJManualReloadPhase::ready;weapon_=0;hand_=-1;replenish_=false;
        }else if(bound&&phase_!=CoJManualReloadPhase::closing){
            // Opening on an empty trigger already consumed this physical hold.
            // Only a verified release can arm a subsequent trigger close; do
            // not let the opening press close its own session one frame later.
            const bool fire=(hand_==0?input.fire_right:input.fire_left)&&
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
        if(phase_==CoJManualReloadPhase::ready&&manual_idle&&s.native_reload_held&&empty_fire_edge)
            trigger_claims_|=static_cast<std::uint8_t>(1U<<armed);
        if(phase_==CoJManualReloadPhase::ready&&allowed&&manual_idle&&
            !s.native_reload_held&&((edge&&input.reload)||empty_fire_edge||deferred_start)){
            after_shot_at_=0;
            player_=s.player;context_=s.context;generation_=s.raw.input_context_generation;
            weapon_=s.admission.weapon_id;hand_=s.admission.armed_hand;
            kind_=s.native.kind;cartridge_index_=-1;
            schofield_motion_=schofield;schofield_chambers_cleared_=false;
            schofield_sweep_={};
            replenish_=false;
            phase_=CoJManualReloadPhase::opening;reload_claim_=true;out.start=true;
            out.reason=deferred_start?"reload_after_shot":empty_fire_edge?"empty_trigger":"reload_button";
            input.reload=true;
        }
        const bool owned=phase_!=CoJManualReloadPhase::ready;
        if(s.enabled&&!out.start&&s.raw.reload&&!s.native_fallback_allowed){
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
                out.cancel=Cancel();out.supervise=false;out.tracked=false;out.reason="schofield_rearward_close";
            }else if(sweep==SchofieldFlick::eject){
                if(s.native.ammo_readback>0){out.unload=true;out.reason="schofield_downward_eject";}
                else if(s.native.ammo_readback==0&&!schofield_chambers_cleared_){
                    out.eject_empty=true;
                    out.reason="schofield_downward_eject";
                    schofield_chambers_cleared_=true;
                }
            }
        }else out.schofield_sample.state="duplicate_sample";
        if(!waiting||!s.winchester_receiver||carried||s.raw.fire_left||s.raw.fire_right||
            !s.receiver.lever_valid)lever_stroke_={};
        else if(fresh&&UpdateWinchesterLever(s)){
            out.cancel=Cancel();out.supervise=false;out.tracked=false;out.reason="winchester_lever_close";
        }
        runtime::MotionReloadGestureInput gesture{};
        // Reserve is a fresh native observation, never a VR ammunition replica.
        // Keep the native waiting/close route supervised, but withdraw any
        // cartridge offer/carry when no native round remains to transfer.
        const int offered_index=cartridge_index_>=0?cartridge_index_:s.native.selected_index;
        const bool round_available=s.native.valid&&(kind_==CoJManualReloadKind::indexed?
            IndexedAvailable(s.native,offered_index):s.native.reserve_readback>0);
        if(!round_available)replenish_=false;
        gesture.valid=gesture.eligible=out.tracked&&phase_==CoJManualReloadPhase::manual_load&&round_available&&
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
        out.insert=out.gesture.reload&&gesture.valid&&!out.cancel;
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
    bool schofield_chambers_cleared() const noexcept {return schofield_chambers_cleared_;}
    // Resolve the synchronous native call before publishing this frame's
    // ownership/feedback. The insertion flag records intent, never ammo credit.
    void CompleteInsertion(bool completed,bool accepted,CoJManualReloadOutput& out) noexcept {
        out.target={};
        NoteInsertion(completed&&accepted);
        if(completed&&accepted){
            cartridge_index_=-1;
            if(kind_!=CoJManualReloadKind::unit)replenish_=false;
            if(kind_==CoJManualReloadKind::clip&&schofield_motion_){
                schofield_sweep_={};
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
    runtime::MotionReloadGestureStage gesture_stage() const noexcept {return gesture_.stage();}
private:
    bool UpdateWinchesterLever(const CoJManualReloadInput& s) noexcept {
        const auto lever=s.receiver.lever;
        const runtime::Vec3 rel{s.left.position.x-lever.x,s.left.position.y-lever.y,s.left.position.z-lever.z};
        const auto previous=lever_stroke_;
        lever_stroke_.last=s.now;lever_stroke_.position=rel;
        lever_stroke_.hand_y=s.left.position.y;
        const auto elapsed=s.now-previous.last;
        if(!previous.last||!elapsed||elapsed>90||(previous.start&&s.now-previous.start>350)||
            std::hypot(rel.x,rel.y,rel.z)>.18F){lever_stroke_.start=0;lever_stroke_.depth=0;return false;}
        if(!previous.start&&std::hypot(previous.position.x,previous.position.y,previous.position.z)>.12F)return false;
        const float down=previous.position.y-rel.y;
        const float side=std::hypot(rel.x-previous.position.x,rel.z-previous.position.z);
        const float minimum=(previous.start?.00008F:.0002F)*static_cast<float>(elapsed);
        const float hand_down=previous.hand_y-s.left.position.y;
        if(down<minimum||hand_down<minimum||std::max(down,hand_down)>.004F*static_cast<float>(elapsed)||side>down+.003F){
            lever_stroke_.start=0;lever_stroke_.depth=0;return false;
        }
        if(!previous.start)lever_stroke_.start=previous.last;
        lever_stroke_.depth+=std::min(down,hand_down);
        return lever_stroke_.depth>=.04F;
    }
    enum class SchofieldFlick { none, eject, close };
    SchofieldFlick UpdateSchofieldSweep(const CoJManualReloadInput& s,CoJSchofieldMotionSample& sample) noexcept {
        const auto& grip=hand_==0?s.right:s.left;
        const auto position=runtime::Vec3{grip.position.x-s.head.position.x,
            grip.position.y-s.head.position.y,grip.position.z-s.head.position.z};
        if(!Valid(s.armed_aim)){
            sample.state="aim_unavailable";
            schofield_sweep_={};return SchofieldFlick::none;
        }
        const auto aim=runtime::RotateVector(runtime::NormalizeQuaternion(s.armed_aim.orientation),{0,0,-1});
        const auto head_rear=runtime::RotateVector(runtime::NormalizeQuaternion(s.head.orientation),{0,0,1});
        const float horizontal=std::hypot(head_rear.x,head_rear.z);
        const auto prior=schofield_sweep_;
        schofield_sweep_.last=s.now;schofield_sweep_.position=position;schofield_sweep_.aim=aim;
        const auto dt=s.now-prior.last;
        sample.aim_y=aim.y;
        sample.interval_ms=prior.last?dt:0;
        const auto lock_until_rest=[&]{
            schofield_sweep_={.position=position,.aim=aim,.last=s.now,.locked=true};
        };
        if(!prior.last||!dt||dt>90){
            sample.state=prior.last?"sweep_gap":"baseline";
            schofield_sweep_={.position=position,.aim=aim,.last=s.now};
            if(prior.locked||prior.direction!=SchofieldFlick::none)lock_until_rest();
            return SchofieldFlick::none;
        }
        const runtime::Vec3 step{position.x-prior.position.x,position.y-prior.position.y,position.z-prior.position.z};
        const float distance=std::hypot(step.x,step.y,step.z);
        const float velocity_scale=1000.F/static_cast<float>(dt);
        const auto speed=distance*velocity_scale;
        const float pitch=aim.y-prior.aim.y;
        const float back=horizontal>=.12F?(step.x*head_rear.x+step.z*head_rear.z)/horizontal:0.F;
        sample.down_m=-step.y;sample.back_m=back;sample.pitch_delta=pitch;sample.step_m=distance;
        if(distance>.12F||speed>4.F){
            sample.state="pose_jump";lock_until_rest();return SchofieldFlick::none;
        }
        // Small returns below the speed floor still reduce net travel.
        const bool wrist_only=speed<=.55F;
        const float reverse=prior.direction==SchofieldFlick::eject?
            std::max(step.y,wrist_only?pitch*.14F:0.F):
            std::max(-back,wrist_only?-pitch*.14F:0.F);
        if(prior.start&&reverse>0.F)
            schofield_sweep_.distance=std::max(0.F,prior.distance-reverse);
        sample.progress_m=schofield_sweep_.distance;
        if(speed<=.20F&&std::abs(pitch)*velocity_scale<=.9F){
            sample.state=prior.locked?"settling":"rest";
            if(!prior.rest_since)schofield_sweep_.rest_since=prior.last;
            else if(s.now-prior.rest_since>=110)schofield_sweep_={.position=position,.aim=aim,.last=s.now};
            return SchofieldFlick::none;
        }
        schofield_sweep_.rest_since=0;
        if(prior.locked){sample.state="locked_until_rest";return SchofieldFlick::none;}
        // A tilted downward shake ejects. Pulling back or pitching the muzzle
        // up in place closes. Large translation owns the sample over pitch.
        const float eject=aim.y<=-.28F?std::max(-step.y>=.65F*distance?-step.y:0.F,
            wrist_only?-pitch*.14F:0.F):0.F;
        const float close=std::max(back>=.72F*distance?back:0.F,wrist_only?pitch*.14F:0.F);
        const auto direction=eject>=close?SchofieldFlick::eject:SchofieldFlick::close;
        const float progress=std::max(eject,close);
        const float axial_speed=progress*velocity_scale;
        if(prior.start){
            // Apply the speed floor to opposition even when it cannot issue
            // either command on its own.
            if(reverse*velocity_scale>=.22F||(axial_speed>=.22F&&prior.direction!=direction)){
                sample.state="rebound";lock_until_rest();return SchofieldFlick::none;
            }
            if(s.now-prior.start>180){
                sample.state="stroke_timeout";lock_until_rest();return SchofieldFlick::none;
            }
        }
        if(axial_speed<.22F){sample.state="below_axis_speed";return SchofieldFlick::none;}
        if(!prior.start){
            schofield_sweep_.start=prior.last;schofield_sweep_.distance=0;
            schofield_sweep_.peak_speed=0;
        }
        schofield_sweep_.direction=direction;
        schofield_sweep_.distance+=progress;
        schofield_sweep_.peak_speed=std::max(axial_speed,schofield_sweep_.peak_speed);
        sample.progress_m=schofield_sweep_.distance;
        sample.peak_mps=schofield_sweep_.peak_speed;
        sample.state=direction==SchofieldFlick::close?"rearward":"downward";
        if(schofield_sweep_.distance<(direction==SchofieldFlick::close?.08F:.065F)||
            schofield_sweep_.peak_speed<.43F)return SchofieldFlick::none;
        sample.classified=true;lock_until_rest();return direction;
    }
    static bool MatchesIdleProbe(const CoJMotionReloadOwner& owner,const CoJReloadTraceSnapshot& native) noexcept {
        return owner.valid&&owner.SupportsManual()&&native.valid&&native.probe_valid&&
            (native.probe_status==0||native.probe_status==4)&&native.weapon_id==owner.weapon_id&&
            native.armed_hand==owner.armed_hand&&native.kind==owner.kind;
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
    std::uint64_t reload_player_=0,reload_context_=0,reload_generation_=0,reload_weapon_=0;
    std::uint64_t after_shot_at_=0;
    int reload_hand_=-1;
    int hand_=-1;
    int cartridge_index_=-1;
    CoJManualReloadKind kind_=CoJManualReloadKind::unit;
    struct WinchesterStroke {
        runtime::Vec3 position{};
        std::uint64_t last=0,start=0;
        float depth=0,hand_y=0;
    } lever_stroke_{};
    struct SchofieldSweep {
        runtime::Vec3 position{},aim{};
        std::uint64_t last=0,start=0,rest_since=0;
        float distance=0,peak_speed=0;
        SchofieldFlick direction=SchofieldFlick::none;
        bool locked=false;
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
