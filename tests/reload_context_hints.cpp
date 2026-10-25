#include "games/call_of_juarez/reload_context_hints.hpp"
#include <cstdlib>
#include <iostream>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
struct Fixture {
    CoJReloadContextHints hints;CoJManualReloadInput s{};CoJManualReloadOutput r{};ActionPromptSnapshot labels{};
    Fixture(){s.enabled=s.allowed=s.raw.active=true;s.player=2;s.context=3;s.raw.input_context_generation=4;
        s.native.valid=s.native.probe_valid=true;s.native.weapon_id=9;s.native.armed_hand=0;s.native.probe_status=2;s.native.reserve_readback=6;
        s.sequence=1;s.now=100;r.tracked=true;r.gesture.pickup_hint.visible=r.gesture.pickup_hint.ready=true;
        r.gesture.pickup_hint.hand=1;labels.active=true;labels.binding_generation=5;
        (void)SetActionPromptLabel(labels,PromptAction::left_fire,"Left Trigger");}
    StereoHudTextOverlay Frame(){StereoHudTextOverlay out{};out.reload_feedback.owner_token=17;return out;}
    bool Draw(StereoHudTextOverlay& out,bool enabled=true){return hints.Append(s,r,17,labels,CoJUiLanguage::english,enabled,out);}
    void Step(){++s.sequence;s.now+=20;}
};
}
int main(){
    Fixture empty;empty.s.native.reserve_readback=0;
    (void)SetActionPromptLabel(empty.labels,PromptAction::reload,"Reload Button");
    auto empty_out=empty.Frame();
    Check(empty.Draw(empty_out)&&empty_out.reload_instruction&&
        empty_out.text.interaction.view()==u"No reserve ammunition. Press Reload Button to close.",
        "empty reserve needs a bounded instruction using the reload binding");
    Check(empty_out.control_hint_action==PromptAction::reload&&
        empty_out.control_hint_prompts.binding_generation==empty.labels.binding_generation,
        "empty reserve must capture semantic reload and its owned binding epoch");
    Check(!empty.r.insert&&!empty.r.cancel&&!empty.r.gesture.cartridge_held,
        "empty reserve instruction changed native or gesture authority");
    empty.Step();auto empty_es=empty.Frame();
    Check(empty.hints.Append(empty.s,empty.r,17,empty.labels,CoJUiLanguage::spanish,true,empty_es)&&
        empty_es.text.interaction.view()==u"Sin munición de reserva. Pulsa Reload Button para cerrar.",
        "empty reserve closing instruction was not localized");
    empty.s.native.reserve_readback=2;empty.Step();auto replenished=empty.Frame();
    Check(empty.Draw(replenished)&&replenished.text.interaction.view().find(u"Left Trigger")!=std::u16string_view::npos,
        "reserve recovery retained the empty closing instruction");
    Fixture f;auto out=f.Frame();Check(f.Draw(out)&&out.reload_instruction&&out.text.interaction.view().find(u"Left Trigger")!=std::u16string_view::npos,"pickup instruction needs actual reported support binding");
    Check(out.text.controller_prompt_generation==5&&!f.r.insert&&!f.r.gesture.cartridge_held,"instruction cannot grant a cartridge or insertion");
    Check(out.control_hint_action==PromptAction::left_fire&&out.control_hint_prompts.binding_generation==5,
        "coaching did not capture the actual support action and binding epoch");
    Fixture graphical;graphical.labels.glyph_sample_ms=100;graphical.labels.glyph_deadline_ms=1600;
    graphical.labels.glyphs[0]=QualifyPromptGlyph("playstation_vr2_sense","/user/hand/left",
        "trigger","/input/l2","trigger","click","trigger",33);
    auto decorated=graphical.Frame();Check(graphical.Draw(decorated),"glyph coaching missing");
    Check(ResolveReloadInstructionGlyph(decorated,5,101).origin==33,
        "semantic support coaching did not admit the captured actual binding");
    graphical.labels.glyphs={};
    Check(ResolveReloadInstructionGlyph(decorated,5,101).origin==33,
        "captured hint borrowed mutable producer metadata");
    for(const auto now:{99ULL,1600ULL,3100ULL})
        Check(ResolveReloadInstructionGlyph(decorated,5,now).kind==PromptGlyphKind::none,
            "queued semantic glyph survived clock regression or deadline");
    Check(ResolveReloadInstructionGlyph(decorated,6,101).kind==PromptGlyphKind::none,
        "queued semantic glyph survived rebinding");
    auto priority=decorated;priority.text.hint.length=1;priority.text.hint.characters[0]=u'N';
    Check(ResolveReloadInstructionGlyph(priority,5,101).kind==PromptGlyphKind::none,
        "native hint priority admitted optional art");
    priority=decorated;priority.reload_instruction=false;
    Check(ResolveReloadInstructionGlyph(priority,5,101).kind==PromptGlyphKind::none,
        "ordinary native interaction text was decorated without semantic ownership");
    priority=decorated;priority.control_hint_action=PromptAction::interact;
    Check(ResolveReloadInstructionGlyph(priority,5,101).kind==PromptGlyphKind::none,
        "non-reload action was admitted by coaching-only glyph path");
    for(int i=0;i<151;++i){f.Step();out=f.Frame();(void)f.Draw(out);}
    out=f.Frame();Check(!f.Draw(out),"stationary stage replayed an expired instruction");
    f.r.gesture.pickup_hint={};f.r.gesture.cartridge_held=true;f.r.gesture.cartridge_hand=f.r.gesture.claimed_hand=1;f.Step();out=f.Frame();
    Check(f.Draw(out)&&out.text.interaction.view().find(u"Bring")!=std::u16string_view::npos,"carrying needs a new bounded instruction");
    f.r.gesture.insertion_ready=true;f.Step();out=f.Frame();Check(f.Draw(out)&&out.text.interaction.view().find(u"Release")!=std::u16string_view::npos,"qualified insertion needs release instruction");
    f.r.gesture.insertion_ready=false;f.r.gesture.claimed_hand=2;f.Step();out=f.Frame();Check(f.Draw(out)&&out.text.interaction.view().find(u"next")!=std::u16string_view::npos,"unclaimed continuation needs deliberate press instruction");
    f.s.raw.fire_left=true;f.Step();out=f.Frame();Check(f.Draw(out)&&out.text.interaction.view().find(u"Release")!=std::u16string_view::npos,"unreleased continuation must not suggest a fresh press");
    for(int reason=0;reason<9;++reason){Fixture x;out=x.Frame();
        if(reason==0)x.r.tracked=false;if(reason==1)x.r.cancel=true;if(reason==2)x.r.insert=true;
        if(reason==3)x.s.native.probe_status=3;if(reason==4)x.s.native.reserve_readback=-1;
        if(reason==5)out.reload_feedback.owner_token=18;
        if(reason==6){out.text.hint.length=1;out.text.hint.characters[0]=u'N';}
        if(reason==7){out.text.interaction.length=1;out.text.interaction.characters[0]=u'N';}
        Check(!x.Draw(out,reason!=8)&&!out.reload_instruction,"unavailable instructions overwrote native HUD or survived invalid ownership");}
    Fixture interrupted;out=interrupted.Frame();Check(interrupted.Draw(out),"initial coaching missing");
    interrupted.s.sequence=0;out=interrupted.Frame();Check(!interrupted.Draw(out),"regressing sample retained coaching");
    Fixture rebound;out=rebound.Frame();Check(rebound.Draw(out),"binding baseline missing");
    rebound.labels.binding_generation=8;out=rebound.Frame();Check(!rebound.Draw(out),"rebinding reused an old sample's button name");
    rebound.Step();out=rebound.Frame();Check(rebound.Draw(out),"fresh post-rebind sample did not recover coaching");
    Fixture invalidated;invalidated.labels.active=false;out=invalidated.Frame();
    Check(invalidated.Draw(out)&&out.text.controller_prompt_generation==5&&
        out.text.interaction.view().find(u"Left fire")!=std::u16string_view::npos,
        "inactive runtime metadata lost its invalidation epoch or semantic fallback");
    Fixture mirrored;mirrored.s.native.armed_hand=1;mirrored.r.gesture.pickup_hint.hand=0;
    (void)SetActionPromptLabel(mirrored.labels,PromptAction::right_fire,"Right B");out=mirrored.Frame();
    Check(mirrored.hints.Append(mirrored.s,mirrored.r,17,mirrored.labels,CoJUiLanguage::spanish,true,out)&&
        out.text.interaction.view().find(u"Right B")!=std::u16string_view::npos&&
        out.text.interaction.view().find(u"cartucho")!=std::u16string_view::npos,"mirrored Spanish coaching used the wrong hand or language");
    Check(out.control_hint_action==PromptAction::right_fire&&out.control_hint_prompts.binding_generation==5,
        "mirrored coaching captured the wrong semantic action");
    std::cout<<"reload context coaching checks passed\n";
}
