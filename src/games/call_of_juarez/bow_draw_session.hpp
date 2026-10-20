#pragma once
#include "runtime/bow_draw.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/bow_draw_input.hpp"
#include "games/call_of_juarez/bow_draw_geometry.hpp"

namespace cojvr::games::call_of_juarez {
inline bool CoJBowDrawControlsAvailable(const runtime::GameplayInputState& raw,
    const runtime::GameplayInputState& filtered) noexcept {
    return raw.active&&filtered.active&&(raw.digital_available&2U)!=0&&
        !(raw.fire_right&&!filtered.fire_right)&&
        !raw.weapon_radial&&!filtered.weapon_radial&&!raw.interact&&!raw.reload&&
        !raw.alternate_fire&&!raw.quick_save&&!raw.quick_load&&!raw.weapon_next&&
        !raw.weapon_previous&&!raw.hands&&!raw.discard_weapon&&
        !std::any_of(raw.equipment_select.begin(),raw.equipment_select.end(),[](bool selected){return selected;});
}
struct CoJBowDrawSessionInput {
    bool selected=false,allowed=false,recentered=false,right_available=false,right_held=false;
    bool counter_valid=false;
    int attack_entries=0;
    std::uint64_t player=0,context=0,input_generation=0,sequence=0,now_ms=0;
    runtime::Vec3 head_world_m{},left_world_m{},right_world_m{};
    CoJBowDrawNativeObservation native{};
};
struct CoJBowDrawSessionOutput {
    CoJBowDrawDispatch dispatch{};
    runtime::BowDrawResult gesture{};
    runtime::BowDrawFeedback feedback{};
    std::uint64_t owner_token=0;
    const char* reason="none";
};
class CoJBowDrawSession final {
public:
    CoJBowDrawSessionOutput Update(const CoJBowDrawSessionInput& s) noexcept {
        CoJBowDrawSessionOutput out{};
        if(!s.selected||!s.allowed||!s.native.valid||!s.player||!s.context||!s.input_generation||s.recentered){
            out.dispatch.claimed=s.selected||selected_;out.dispatch.cancelled=policy_.drawing()||dispatch_.pending();
            out.reason=!s.selected?"bow_deselected":!s.allowed?"adapter_gate":!s.native.valid?
                "native_observation":s.recentered?"recentered":"session_identity";
            Reset();return out;
        }
        selected_=true;
        if(weapon_!=s.native.weapon_id||player_!=s.player||context_!=s.context||input_!=s.input_generation){
            out.dispatch.cancelled=active();out.reason=active()?"session_owner_changed":"none";
            Reset();selected_=true;weapon_=s.native.weapon_id;player_=s.player;context_=s.context;input_=s.input_generation;
            owner_=++epoch_;if(!owner_)owner_=++epoch_;
        }
        runtime::Vec3 nock_cm{};
        const bool geometry=nock_.Observe(s.native,s.player,s.context,policy_.drawing()||dispatch_.pending(),nock_cm);
        runtime::BowDrawInput input{};
        input.eligible=geometry&&!dispatch_.pending()&&(policy_.drawing()||s.native.current_state==1);
        input.right_trigger_available=s.right_available;input.right_trigger_held=s.right_held;
        input.owner=s.native.weapon_id;input.player=s.player;input.native_context=s.context;
        input.input_generation=s.input_generation;input.sequence=s.sequence;input.now_ms=s.now_ms;
        input.head_position_m=s.head_world_m;input.left_position_m=s.left_world_m;input.right_position_m=s.right_world_m;
        input.string_point_m={nock_cm.x*.01F,nock_cm.y*.01F,nock_cm.z*.01F};
        input.aim_forward={s.native.root.forward[0],s.native.root.forward[1],s.native.root.forward[2]};
        out.gesture=policy_.Update(input);
        CoJBowDrawDispatchSample native{};
        native.selected=true;native.eligible=geometry&&s.right_available;
        native.drawing=out.gesture.drawing;native.release_fire=out.gesture.fire;
        native.alternate_ready=s.native.release_ready;native.can_attack=s.native.can_attack;
        native.native_right_held=s.native.held_right;native.counter_valid=s.counter_valid;
        native.attack_entries=s.attack_entries;native.weapon=s.native.weapon_id;
        native.player=s.player;native.context=s.context;native.input_generation=s.input_generation;
        native.sequence=s.sequence;native.now_ms=s.now_ms;
        const bool replaced=out.dispatch.cancelled;
        out.dispatch=dispatch_.Update(native);out.dispatch.cancelled|=replaced;out.owner_token=owner_;
        if(out.gesture.cancelled)out.reason=out.gesture.reason;
        else if(!replaced)out.reason=out.dispatch.cancelled?out.dispatch.reason:out.gesture.reason;
        out.feedback.owner_token=owner_;out.feedback.input_generation=s.input_generation;
        out.feedback.event_sequence=s.sequence;out.feedback.tension=out.gesture.normalized_draw;
        if(out.dispatch.shot_requested)out.feedback.kind=runtime::BowDrawFeedbackKind::release;
        else if(out.gesture.haptics.grabbed)out.feedback.kind=runtime::BowDrawFeedbackKind::grab;
        else if(out.gesture.haptics.tension_step)out.feedback.kind=runtime::BowDrawFeedbackKind::tension;
        return out;
    }
    void Reset() noexcept {
        policy_.Reset();dispatch_.Reset();nock_.Reset();
        owner_=player_=context_=input_=0;weapon_=0;selected_=false;
    }
    std::uint64_t owner_token() const noexcept {return owner_;}
    bool active() const noexcept {return policy_.drawing()||dispatch_.pending();}
private:
    runtime::BowDrawPolicy policy_;
    CoJBowDrawInput dispatch_;
    CoJBowNockReference nock_;
    std::uint64_t epoch_=0,owner_=0,player_=0,context_=0,input_=0;
    std::uint32_t weapon_=0;
    bool selected_=false;
};
}
