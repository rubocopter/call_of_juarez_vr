#pragma once
#include "games/call_of_juarez/manual_reload_session.hpp"
#include "games/call_of_juarez/ui_language.hpp"
#include "runtime/action_prompts.hpp"
#include "runtime/hud_text.hpp"
namespace cojvr::games::call_of_juarez {
// Optional low-priority coaching, never an action, ammunition replica or retry.
class CoJReloadContextHints {
public:
    bool Append(const CoJManualReloadInput& s,const CoJManualReloadOutput& r,
        std::uint64_t owner,const runtime::ActionPromptSnapshot& prompts,
        CoJUiLanguage language,bool enabled,runtime::StereoHudTextOverlay& out) noexcept {
        if(!s.enabled||!s.allowed||!s.raw.active||s.recentered||!s.player||!s.context||!s.sequence||
            !r.tracked||r.cancel||r.insert||!owner||
            out.reload_feedback.owner_token!=owner||!s.native.valid||!s.native.probe_valid||
            s.native.probe_status!=2||s.native.reserve_readback<0||s.native.armed_hand<0||s.native.armed_hand>1){Reset();return false;}
        const bool regression=have_&&(s.sequence<sequence_||s.now<now_);
        const bool gap=have_&&s.now>=fresh_at_&&s.now-fresh_at_>250;
        const bool changed_owner=have_&&(owner!=owner_||s.player!=player_||
            s.context!=context_||s.raw.input_context_generation!=input_||s.native.weapon_id!=weapon_||s.native.armed_hand!=hand_);
        if(regression||gap){Reset();return false;}
        const bool fresh=!have_||s.sequence>sequence_;
        const bool changed_binding=have_&&prompts.binding_generation!=binding_;
        if(changed_owner||(changed_binding&&(!fresh||r.unload_refusal==CoJReloadRefusal::none))){
            Reset();Remember(s,owner,prompts.binding_generation);return false;
        }
        now_=s.now;
        if(fresh){
            if(r.unload_refusal!=CoJReloadRefusal::none){
                instruction_=r.unload_refusal==CoJReloadRefusal::reserve_full?
                    Instruction::reserve_full:Instruction::unload_refused;
                started_=s.now;
            }else if(!IsRefusal()||s.now-started_>=3000||r.unload||r.eject_empty){
                const auto next=enabled?InstructionFor(s,r):Instruction::none;
                if(!have_||next!=instruction_){instruction_=next;started_=s.now;}
            }
            Remember(s,owner,prompts.binding_generation);
        }
        if((!enabled&&!IsRefusal())||instruction_==Instruction::none||s.now<started_||s.now-started_>=3000||
            !out.text.hint.view().empty()||!out.text.interaction.view().empty())return false;
        runtime::HudText text{};const bool es=language==CoJUiLanguage::spanish;
        const auto add=[&](std::u16string_view part){
            if(text.length+part.size()>=text.capacity)return;
            for(auto c:part)text.characters[text.length++]=c;
        };
        const auto support=s.native.armed_hand==0?runtime::PromptAction::left_fire:runtime::PromptAction::right_fire;
        const auto label=runtime::ResolveActionPrompt(prompts,support,es);
        const bool chest_load=r.schofield_motion&&s.native.reserve_readback>0&&
            (s.native.schofield_spent==0||r.unload_refusal==CoJReloadRefusal::reserve_full)&&
            s.native.ammo_readback>=0&&s.native.ammo_readback<s.native.capacities[0];
        switch(instruction_){
        case Instruction::spent_cases:
        case Instruction::schofield_gestures:
            if(instruction_==Instruction::spent_cases)
                add(es?u"Inclina el revólver hacia atrás para expulsar las vainas.\n":
                    u"Tilt the revolver back to eject the spent cases.\n");
            else if(chest_load)add(es?u"Acerca el revólver al pecho para cargar.\n":
                u"Bring the revolver to your chest to load.\n");
            if(instruction_==Instruction::schofield_gestures&&s.native.ammo_readback>0)
                add(es?u"Para vaciar, inclina el revólver hacia atrás.\n":
                    u"To empty, tilt the revolver back.\n");
            if(s.native.support_weapon_id){
                add(es?u"Pulsa ":u"Press ");
                add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::reload,es));
                add(es?u" para cerrar.":u" to close.");break;
            }
            add(es?u"Vuelve a nivel y da un golpe de muñeca hacia abajo y delante, o pulsa ":
                u"Return to level, then flick down and forward or press ");
            add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::reload,es));
            add(es?u".":u" to close.");break;
        case Instruction::close:
            add(es?u"Pulsa ":u"Press ");
            add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::reload,es));
            add(es?u" para cerrar.":u" to close.");break;
        case Instruction::receiver_carry:
            add(es?u"Acerca la bala a la entrada lateral. Suelta ":
                u"Bring the round to the side port. Release ");
            add(label);add(es?u" cuando la guía esté verde.":u" when the guide turns green.");break;
        case Instruction::reserve_full:
        case Instruction::unload_refused:
            if(instruction_==Instruction::reserve_full&&chest_load){
                add(es?u"No caben todas las balas en la reserva. Acerca el revólver al pecho para completar la carga.":
                    u"Not all rounds fit in reserve. Bring the revolver to your chest to top up.");
                break;
            }
            if(instruction_==Instruction::reserve_full)
                add(es?u"No hay espacio en la reserva. Pulsa ":u"No room in reserve. Press ");
            else add(es?u"El tambor sigue cargado. Pulsa ":u"Cylinder still loaded. Press ");
            add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::reload,es));
            add(es?u" para cerrar.":u" to close.");break;
        case Instruction::empty_reserve:
            add(es?u"Sin munición de reserva. Pulsa ":u"No reserve ammunition. Press ");
            add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::reload,es));
            add(es?u" para cerrar.":u" to close.");break;
        case Instruction::pickup:
            add(es?u"Pulsa ":u"Press ");add(label);
            if(chest_load)
                add(es?u" en la cadera para coger balas o acerca el revólver al pecho.":
                    u" at your hip to take rounds, or bring the revolver to your chest.");
            else if(r.schofield_motion)
                add(es?u" junto a la cadera para coger la munición que falta.":u" at your hip to take the missing rounds.");
            else add(es?u" junto a la cadera para coger un cartucho.":u" at your hip to take a round.");
            break;
        case Instruction::release_first:add(es?u"Suelta ":u"Release ");add(label);add(es?u" antes de coger otro cartucho.":u" before taking a round.");break;
        case Instruction::carry:add(es?u"Lleva la munición al marcador. Cuando esté verde, suelta ":u"Bring the ammo to the marker. When green, release ");add(label);add(u".");break;
        case Instruction::insert:add(es?u"Suelta ":u"Release ");add(label);add(es?u" para cargar.":u" to load.");break;
        case Instruction::continuation:add(es?u"Pulsa ":u"Press ");add(label);add(es?u" para coger el siguiente cartucho.":u" to take the next round.");break;
        default:return false;
        }
        out.text.interaction=text;out.text.controller_prompt_generation=prompts.binding_generation;
        out.reload_instruction=true;
        out.reload_result_notice=IsRefusal();
        out.reload_instruction_until_ms=started_>UINT64_MAX-3000?UINT64_MAX:started_+3000;
        out.control_hint_action=instruction_==Instruction::empty_reserve||IsRefusal()||
            instruction_==Instruction::schofield_gestures||instruction_==Instruction::spent_cases||
            instruction_==Instruction::close?
            runtime::PromptAction::reload:support;
        out.control_hint_prompts=prompts;
        return true;
    }
    void Reset() noexcept {have_=false;instruction_=Instruction::none;}
private:
    enum class Instruction {none,pickup,release_first,carry,insert,continuation,empty_reserve,reserve_full,
        unload_refused,schofield_gestures,spent_cases,close,receiver_carry};
    bool IsRefusal() const noexcept {
        return instruction_==Instruction::reserve_full||instruction_==Instruction::unload_refused;
    }
    static Instruction InstructionFor(const CoJManualReloadInput& s,const CoJManualReloadOutput& r) noexcept {
        if(r.schofield_motion&&s.native.schofield_spent>0)return Instruction::spent_cases;
        if(s.native.reserve_readback==0)
            return r.schofield_motion?Instruction::schofield_gestures:Instruction::empty_reserve;
        const auto& g=r.gesture;
        if(g.pickup_hint.visible)return g.pickup_hint.ready?Instruction::pickup:Instruction::release_first;
        if(!g.cartridge_held||g.cartridge_hand!=1-s.native.armed_hand)
            return r.schofield_motion?Instruction::schofield_gestures:
                s.winchester_receiver?Instruction::close:Instruction::none;
        if(g.claimed_hand!=g.cartridge_hand){
            const bool held=s.native.armed_hand==0?s.raw.fire_left:s.raw.fire_right;
            return held?Instruction::release_first:Instruction::continuation;
        }
        if(s.winchester_receiver)return Instruction::receiver_carry;
        return g.insertion_ready?Instruction::insert:Instruction::carry;
    }
    void Remember(const CoJManualReloadInput& s,std::uint64_t owner,std::uint64_t binding) noexcept {
        have_=true;owner_=owner;binding_=binding;player_=s.player;context_=s.context;weapon_=s.native.weapon_id;
        hand_=s.native.armed_hand;input_=s.raw.input_context_generation;sequence_=s.sequence;now_=fresh_at_=s.now;
    }
    bool have_=false;int hand_=-1;Instruction instruction_=Instruction::none;
    std::uint64_t owner_=0,binding_=0,player_=0,context_=0,weapon_=0,input_=0,sequence_=0,now_=0,fresh_at_=0,started_=0;
};
}
