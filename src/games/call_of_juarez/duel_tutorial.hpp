#pragma once
#include "games/call_of_juarez/duel_input.hpp"
#include "games/call_of_juarez/ui_language.hpp"
#include "runtime/hud_text.hpp"

namespace cojvr::games::call_of_juarez {
enum class CoJDuelTutorialInstruction {
    none, lower_and_center, center_stick, wait_for_start, draw, raise_hand, raise_stick, aim_fire
};
struct CoJDuelTutorialSample {
    // Same game-thread capture as UpdatePhysical and the copied HUD. The bridge
    // must verify its exact-build/module/player/duel/action owner and native
    // CanProcessInputs before setting these flags. A failed read is NOT empty.
    bool owner_known=false, can_process_inputs=false, hud_observed=false, recentered=false;
    // Original Duel.m_iState: -1 undefined/finished, 0 inited, 1 prepared,
    // 2 ready (countdown, early draw possible), 3 started. Not a VR phase enum.
    int native_phase=-1;
    // Original HasSomethingInHands result; never infer from delivered draw.
    bool hands_empty=true;
    CoJDuelInputOwner owner{};
    runtime::GameplayInputState raw{};
    runtime::Pose right_aim{};
    std::uint64_t pose_sequence=0, monotonic_us=0, frame_sequence=0;
    std::uint64_t presentation_time_ms=0; // Presenter's GetTickCount64 clock.
    // Copy ONLY after all original OnInputMouseAxis calls succeeded. This is
    // acknowledgement of input delivery, NOT a successful equip/raise/shot.
    CoJDuelMouseAxes delivered_axes{};
};
// Presentation only. This object never invokes input, equips, fires, changes
// countdowns, dismisses hints, or writes native strings. Hip wording describes
// the existing pitch gesture UX; it does not claim measured holster contact.
class CoJDuelTutorial {
public:
    // Retain consumed sample fences across loss; an old pose cannot rearm.
    void Reset() noexcept {
        known_=centered_=lowered_=raised_=false;
        instruction_=CoJDuelTutorialInstruction::none;
        token_=0;
    }
    [[nodiscard]] std::uint64_t owner_token() const noexcept {return token_;}
    // Call once per freshly constructed capture, AFTER native HUD observation.
    // Native hints (including pausing E03_H08_Duel), interactions and mission
    // notices win. Countdown and subtitles stay intact in their existing rows.
    // On none the entire overlay remains unchanged. The caller discards any
    // prior copied guide with its capture; do not Append into a retained packet.
    [[nodiscard]] CoJDuelTutorialInstruction Append(const CoJDuelTutorialSample& s,
        const runtime::ActionPromptSnapshot& prompts,CoJUiLanguage language,bool enabled,
        runtime::StereoHudTextOverlay& out) noexcept {
        using I=CoJDuelTutorialInstruction;
        if(!enabled||!s.hud_observed||!s.presentation_time_ms||
            s.presentation_time_ms>UINT64_MAX-500||!s.frame_sequence||out.frame_sequence!=s.frame_sequence||
            out.text.hint.length||out.text.interaction.length||out.mission_notices.length||
            out.ui.wheel.active||out.reload_instruction) {Reset();return I::none;}
        const auto next=Observe(s);
        if(next==I::none){token_=0;return next;}
        if(!token_||token_instruction_!=next||token_phase_!=s.native_phase){
            if(next_token_==UINT64_MAX){Reset();return I::none;}
            token_=++next_token_;token_instruction_=next;token_phase_=s.native_phase;
        }
        const bool es=language==CoJUiLanguage::spanish;
        runtime::HudText text{};
        const auto add=[&](std::u16string_view part){
            if(text.length+part.size()>=text.capacity)return;
            for(const auto c:part)text.characters[text.length++]=c;
        };
        const auto turn_index=static_cast<std::size_t>(runtime::PromptAction::turn);
        const auto stick=prompts.active&&!prompts.labels[turn_index].view().empty()?
            runtime::ResolveActionPrompt(prompts,runtime::PromptAction::turn,es):
            (es?std::u16string_view(u"stick derecho"):std::u16string_view(u"right stick"));
        switch(next){
        case I::lower_and_center:
            add(es?u"Baja la mano derecha a la cadera y apunta hacia abajo. Centra el ":
                u"Lower your right hand to your hip, pointing down. Center the ");add(stick);add(u".");break;
        case I::center_stick:
            add(es?u"Centra el ":u"Center the ");add(stick);add(es?u" antes de continuar.":u" before continuing.");break;
        case I::wait_for_start:
            add(es?u"Espera a que termine la cuenta atr\u00e1s; no desenfundes antes.":
                u"Wait for the countdown to end; do not draw early.");break;
        case I::draw:
            add(es?u"Mant\u00e9n la mano baja. Mueve el ":u"Keep your hand lowered. Pull the ");
            add(stick);add(es?u" hacia abajo para desenfundar.":u" down to draw.");break;
        case I::raise_hand:
            add(es?u"Levanta la mano derecha de verdad antes de mover el ":
                u"Physically raise your right hand before pushing the ");
            add(stick);add(es?u" hacia arriba.":u" up.");break;
        case I::raise_stick:
            add(es?u"Con la mano levantada, mueve el ":u"With your hand raised, push the ");
            add(stick);add(es?u" hacia arriba para alzar el arma.":u" up to raise the gun.");break;
        case I::aim_fire:
            // The duel mouse-axis route owns aim; tracked tip pitch gates the
            // draw/raise gesture but does not replace native horizontal aim.
            add(es?u"Mueve el ":u"Move the ");add(stick);
            add(es?u" a izquierda/derecha para apuntar. Cuando el arma est\u00e9 lista, pulsa ":
                u" left/right to aim. When the gun is ready, press ");
            add(runtime::ResolveActionPrompt(prompts,runtime::PromptAction::right_fire,es));
            add(es?u" para disparar.":u" to fire.");break;
        default:return I::none;
        }
        out.text.interaction=text;
        out.text.controller_prompt_generation=prompts.binding_generation;
        out.tutorial_owner_token=token_;
        out.tutorial_until_ms=s.presentation_time_ms+500;
        // Do not borrow reload's owner token, deadline, or glyph channel.
        return next;
    }
private:
    CoJDuelTutorialInstruction Observe(const CoJDuelTutorialSample& s) noexcept {
        using I=CoJDuelTutorialInstruction;
        const auto& p=s.right_aim.position;const auto& q=s.right_aim.orientation;
        const double norm=static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+
            static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w;
        if(!s.owner_known||!s.can_process_inputs||s.recentered||s.native_phase<1||s.native_phase>3||
            !s.owner.player_generation||!s.owner.action_identity||s.owner.action_identity==0xffffffffU||
            !std::isfinite(s.owner.game_time)||s.owner.game_time<0||!s.raw.active||
            !s.raw.turn_available||s.raw.weapon_radial||!s.pose_sequence||!s.monotonic_us||
            !std::isfinite(s.raw.turn.x)||!std::isfinite(s.raw.turn.y)||
            std::fabs(s.raw.turn.x)>1||std::fabs(s.raw.turn.y)>1||
            !s.right_aim.position_valid||!s.right_aim.orientation_valid||
            !std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
            !std::isfinite(norm)||norm<1.e-12||
            !std::isfinite(s.delivered_axes.x)||!std::isfinite(s.delivered_axes.y)) {Reset();return I::none;}
        if(sample_known_&&(s.pose_sequence<sequence_||s.monotonic_us<time_||
            (s.owner.player_generation==clock_player_&&s.owner.game_time<game_time_))) {Reset();return I::none;}
        if(sample_known_&&s.monotonic_us-time_>100000)Reset();
        const bool changed=!known_||s.owner.player_generation!=player_||s.owner.action_identity!=action_||
            s.raw.input_context_generation!=context_;
        if(changed)Reset();
        // Duplicate/retained poses may repeat an unchanged guide but never
        // supply center or delivered-UP evidence. Do not refresh their age.
        if(sample_known_&&(s.pose_sequence==sequence_||s.monotonic_us==time_||
            (s.owner.player_generation==clock_player_&&s.owner.game_time==game_time_)))
            return known_&&s.native_phase==phase_&&s.hands_empty==empty_?instruction_:I::none;
        if(known_&&(s.native_phase<phase_||s.hands_empty!=empty_)) {
            lowered_=raised_=false;
            if(s.native_phase<phase_)centered_=false;
        }
        known_=sample_known_=true;player_=clock_player_=s.owner.player_generation;
        action_=s.owner.action_identity;context_=s.raw.input_context_generation;
        sequence_=s.pose_sequence;time_=s.monotonic_us;game_time_=s.owner.game_time;
        phase_=s.native_phase;empty_=s.hands_empty;
        const bool center=std::fabs(s.raw.turn.x)<=.3F&&std::fabs(s.raw.turn.y)<=.3F;
        if(center)centered_=true;
        // Match UpdatePhysical's exact normalized tip/pitch and inclusive
        // raw-center boundaries. No position/contact or new native threshold.
        const double forward_y=2.0*(static_cast<double>(q.w)*q.x-static_cast<double>(q.y)*q.z)/norm;
        if(s.hands_empty){
            raised_=false;
            if(forward_y>-.7071067811865475){lowered_=false;return instruction_=I::lower_and_center;}
            if(center)lowered_=true;
            if(!lowered_||!centered_)return instruction_=I::center_stick;
            return instruction_=s.native_phase<3?I::wait_for_start:I::draw;
        }
        if(!centered_)return instruction_=I::center_stick;
        // Native early drawing remains possible in phase 2. Never describe
        // its occurrence as completion or permission to shoot before start.
        if(s.native_phase<3){raised_=false;return instruction_=I::wait_for_start;}
        if(forward_y<-.5){raised_=false;return instruction_=I::raise_hand;}
        if(s.delivered_axes.dispatch&&s.delivered_axes.y>0&&s.raw.turn.y>0)raised_=true;
        return instruction_=raised_?I::aim_fire:I::raise_stick;
    }
    bool known_=false,sample_known_=false,centered_=false,lowered_=false,raised_=false,empty_=true;
    int phase_=-1;
    std::uint64_t player_=0,context_=0,clock_player_=0,sequence_=0,time_=0;
    std::uint32_t action_=0;
    float game_time_=0;
    CoJDuelTutorialInstruction instruction_=CoJDuelTutorialInstruction::none;
    CoJDuelTutorialInstruction token_instruction_=CoJDuelTutorialInstruction::none;
    std::uint64_t token_=0,next_token_=0;
    int token_phase_=-1;
};
}
