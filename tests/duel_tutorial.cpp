#include "games/call_of_juarez/duel_tutorial.hpp"
#include <iostream>
#include <limits>
#include <string_view>

using namespace cojvr::games::call_of_juarez;
using cojvr::runtime::PromptAction;
namespace {
cojvr::runtime::Pose Aim(bool low) {
    cojvr::runtime::Pose p;
    p.position_valid=p.orientation_valid=true;
    if(low)p.orientation={-.5F,0,0,.8660254F};
    return p;
}
struct Fixture {
    CoJDuelTutorial guide;
    CoJDuelTutorialSample s;
    cojvr::runtime::ActionPromptSnapshot prompts;
    cojvr::runtime::StereoHudTextOverlay hud;
    Fixture() {
        s.owner_known=s.can_process_inputs=s.hud_observed=true;
        s.native_phase=3;s.hands_empty=true;
        s.owner={1,0x80000042U,1};s.raw.active=true;s.raw.input_context_generation=4;
        s.right_aim=Aim(false);s.pose_sequence=1;s.monotonic_us=1000;s.frame_sequence=1;
        s.presentation_time_ms=1000;
    }
    CoJDuelTutorialInstruction Tick(bool fresh=true) {
        if(fresh){++s.pose_sequence;s.monotonic_us+=1000;s.owner.game_time+=.01F;++s.frame_sequence;++s.presentation_time_ms;}
        hud={};hud.frame_sequence=s.frame_sequence;
        return guide.Append(s,prompts,CoJUiLanguage::english,true,hud);
    }
    void Baseline() {s.right_aim=Aim(true);s.raw.turn={};(void)Tick();}
    void Occupied() {s.hands_empty=false;s.delivered_axes={};}
};
}
int main() {
    using I=CoJDuelTutorialInstruction;
    int failures=0,checks=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){std::cerr<<why<<'\n';++failures;}};
    {
        Fixture f;
        check(f.Tick()==I::lower_and_center,"pre-aimed empty hand skipped physical preparation");
        f.s.right_aim=Aim(true);f.s.raw.turn.y=-1;
        check(f.Tick()==I::center_stick,"held down was mistaken for lowered/centered baseline");
        f.Baseline();
        check(f.Tick()==I::draw,"lowered centered hand did not receive draw guidance");
        f.s.raw.turn.y=-1;f.s.delivered_axes={0,-20,true};
        check(f.Tick()==I::draw,"delivered draw request fabricated native occupancy");
        f.Occupied();
        check(f.Tick()==I::raise_hand,"occupied low hand skipped real physical raise");
        f.s.right_aim=Aim(false);f.s.raw.turn={};
        check(f.Tick()==I::raise_stick,"physical hand raise fabricated native up delivery");
        f.s.raw.turn.y=1;f.s.delivered_axes={0,10,false};
        check(f.Tick()==I::raise_stick,"undelivered upward axes advanced the tutorial");
        f.s.delivered_axes.dispatch=true;
        check(f.Tick()==I::aim_fire,"delivered native raise did not advance to aim/fire guidance");
        f.s.delivered_axes={};f.s.raw.turn={};
        check(f.Tick()==I::aim_fire,"centered aim lost acknowledged raise");
        f.s.right_aim=Aim(true);
        check(f.Tick()==I::raise_hand,"lowering again retained raise coaching progress");
        f.s.right_aim=Aim(false);
        check(f.Tick()==I::raise_stick,"re-raised posture reused an old native up acknowledgement");
    }
    {
        Fixture f;f.Baseline();
        for(int phase:{-2,-1,0,4,99}){f.s.native_phase=phase;check(f.Tick()==I::none,"unknown/inited/finished phase showed instructions");}
        f.s.native_phase=1;f.s.right_aim=Aim(false);
        check(f.Tick()==I::lower_and_center,"prepared phase lost posture preparation");
        f.Baseline();check(f.Tick()==I::wait_for_start,"prepared phase encouraged drawing");
        f.s.native_phase=2;
        check(f.Tick()==I::wait_for_start,"countdown encouraged early draw");
        f.s.native_phase=3;check(f.Tick()==I::draw,"native start was inferred from a timer instead of phase");
        f.Occupied();f.s.native_phase=2;f.s.right_aim=Aim(false);f.s.delivered_axes={0,10,true};
        check(f.Tick()==I::wait_for_start,"native early draw advertised firing before start");
    }
    // Each boundary must revoke guidance progress, including a held UP on return.
    for(int boundary=0;boundary<10;++boundary) {
        Fixture f;f.Baseline();f.Occupied();f.s.right_aim=Aim(false);
        f.s.raw.turn.y=1;f.s.delivered_axes={0,10,true};
        check(f.Tick()==I::aim_fire,"recovery fixture never reached native raise");
        f.s.delivered_axes={};
        switch(boundary) {
        case 0:f.s.raw.active=false;(void)f.Tick();f.s.raw.active=true;break;
        case 1:f.s.owner_known=false;(void)f.Tick();f.s.owner_known=true;break;
        case 2:f.s.can_process_inputs=false;(void)f.Tick();f.s.can_process_inputs=true;break;
        case 3:f.s.recentered=true;(void)f.Tick();f.s.recentered=false;break;
        case 4:f.s.raw.weapon_radial=true;(void)f.Tick();f.s.raw.weapon_radial=false;break;
        case 5:f.s.right_aim.orientation_valid=false;(void)f.Tick();f.s.right_aim.orientation_valid=true;break;
        case 6:++f.s.owner.player_generation;break;
        case 7:++f.s.owner.action_identity;break;
        case 8:++f.s.raw.input_context_generation;break;
        case 9:f.s.monotonic_us+=100001;break;
        }
        const auto recovered=f.Tick();
        check(recovered==I::none||recovered==I::center_stick,"owner/focus/tracking recovery reused old raise");
        check(f.Tick()==I::center_stick,"held stick after recovery did not request center");
        f.s.raw.turn={};check(f.Tick()==I::raise_stick,"fresh centered recovery could not resume occupied guidance");
        f.s.hands_empty=true;check(f.Tick()==I::lower_and_center,"retry inherited occupied raise progress");
        f.Baseline();check(f.Tick()==I::draw,"retry could not establish new lowered baseline");
    }
    {
        Fixture f;f.Baseline();f.s.raw.turn.y=-1;
        f.s.owner.action_identity=0xffffffffU;
        check(f.Tick()==I::none,"invalid native action identity was accepted");
        f.s.owner.action_identity=42;f.s.right_aim=Aim(false);
        check(f.Tick()==I::lower_and_center,"invalid owner left stale baseline");
        f.s.raw.turn.y=std::numeric_limits<float>::quiet_NaN();
        check(f.Tick()==I::none,"nonfinite raw input was admitted");
        f.s.raw.turn={};f.s.right_aim=Aim(true);f.s.right_aim.position.x=std::numeric_limits<float>::infinity();
        check(f.Tick()==I::none,"invalid tracking position was admitted");
    }
    {
        Fixture f;f.Baseline();f.Occupied();f.s.right_aim=Aim(false);f.s.raw.turn.y=1;
        f.s.delivered_axes={0,10,true};
        check(f.Tick(false)!=I::aim_fire,"duplicate pose advanced acknowledgement");
        f.s.delivered_axes={};check(f.Tick()==I::raise_stick,"duplicate up acknowledgement survived fresh pose");
        f.s.pose_sequence-=2;check(f.Tick(false)==I::none,"reversed pose sequence retained guidance");
        f.s.pose_sequence+=3;check(f.Tick()==I::center_stick,"sequence reversal retained center");
        f.s.raw.turn={};(void)f.Tick();
        f.s.owner.game_time-=1;check(f.Tick()==I::none,"native clock rewind did not revoke guidance");
    }
    {
        Fixture f;f.s.right_aim=Aim(true);f.s.raw.turn={.3F,-.3F};
        check(f.Tick()==I::draw,"native center boundary rejected inclusive 0.3 axes");
        f.s.raw.turn={.31F,0};++f.s.raw.input_context_generation;
        check(f.Tick()==I::center_stick,"horizontal held input impersonated real center");
    }
    {
        Fixture f;f.Baseline();
        // A repeated pose must age from its first observation, not each render.
        f.s.monotonic_us+=50000;
        check(f.Tick(false)==I::draw,"fresh retained capture lost unchanged guidance");
        f.s.monotonic_us+=50001;
        check(f.Tick(false)==I::none,"repeated pose refreshed its age indefinitely");
        f.s.raw.turn.y=-1;
        check(f.Tick()==I::center_stick,"stale pose preserved empty-hand baseline");
        f.s.raw.turn={};check(f.Tick()==I::draw,"fresh low center could not recover from stale pose");
        f.guide.Reset();
        check(f.Tick(false)==I::none,"explicit reset reused the consumed pose");
    }
    {
        Fixture f;f.Baseline();f.s.native_phase=2;
        check(f.Tick()==I::wait_for_start,"phase rollback did not return to native waiting policy");
        f.s.native_phase=3;f.s.raw.turn.y=-1;
        check(f.Tick()==I::draw,"same duel phase restart lost fresh lowered baseline");
        f.s.owner.game_time-=1;
        check(f.Tick()==I::none,"retry clock rewind showed old guidance");
        f.s.raw.turn={};check(f.Tick()==I::none,"old clock fence was cleared on rewind");
    }
    {
        Fixture f;
        // Scaled valid quaternion must preserve the input converter's pitch.
        f.s.right_aim=Aim(true);
        f.s.right_aim.orientation.x*=3;f.s.right_aim.orientation.w*=3;
        check(f.Tick()==I::draw,"nonunit valid quaternion changed physical preparation");
        f.s.right_aim.orientation={0,0,0,0};
        check(f.Tick()==I::none,"zero quaternion was treated as tracked posture");
    }
    {
        Fixture f;
        const auto native=[](cojvr::runtime::HudText& t){t.characters[0]=u'X';t.length=1;};
        for(int owner=0;owner<6;++owner) {
            f.hud={};f.hud.frame_sequence=f.s.frame_sequence;
            switch(owner){
            case 0:native(f.hud.text.hint);break;
            case 1:native(f.hud.text.interaction);break;
            case 2:native(f.hud.mission_notices);break;
            case 3:f.hud.ui.wheel.active=true;break;
            case 4:f.hud.reload_instruction=true;break;
            case 5:f.hud.frame_sequence++;break;
            }
            const auto before=f.hud;
            check(f.guide.Append(f.s,f.prompts,CoJUiLanguage::english,true,f.hud)==I::none,"native/foreign HUD owner was overwritten");
            check(f.hud.text.hint==before.text.hint&&f.hud.text.interaction==before.text.interaction&&
                f.hud.text.subtitle==before.text.subtitle&&f.hud.mission_notices==before.mission_notices&&
                f.hud.reload_instruction==before.reload_instruction,"suppression mutated native HUD text or flags");
        }
        f.s.hud_observed=false;check(f.Tick()==I::none,"failed native HUD observation was treated as empty");
        f.s.hud_observed=true;f.hud={};f.hud.frame_sequence=f.s.frame_sequence;
        check(f.guide.Append(f.s,f.prompts,CoJUiLanguage::english,false,f.hud)==I::none,"disabled preference emitted guidance");
    }
    {
        Fixture f;f.Baseline();f.Occupied();f.s.right_aim=Aim(false);f.s.raw.turn.y=1;f.s.delivered_axes={0,10,true};
        f.prompts.active=true;f.prompts.binding_generation=9;
        check(cojvr::runtime::SetActionPromptLabel(f.prompts,PromptAction::right_fire,"Custom Fire"),"prompt setup failed");
        check(f.Tick()==I::aim_fire&&f.hud.text.interaction.view().find(u"Custom Fire")!=std::u16string_view::npos,
            "fire guidance ignored runtime binding label");
        check(f.hud.text.interaction.view().find(u"right stick left/right")!=std::u16string_view::npos,
            "duel aim guidance did not teach native horizontal stick aim");
        check(f.hud.text.controller_prompt_generation==9&&!f.hud.reload_instruction&&
            f.hud.control_hint_action==PromptAction::count,"duel text impersonated reload glyph ownership");
        f.s.delivered_axes={};f.s.raw.turn={};f.prompts.binding_generation=10;
        check(cojvr::runtime::SetActionPromptLabel(f.prompts,PromptAction::right_fire,"Rebound"),"rebind setup failed");
        (void)f.Tick();
        check(f.hud.text.interaction.view().find(u"Custom Fire")==std::u16string_view::npos,"rebind retained old label");
        f.hud={};++f.s.pose_sequence;++f.s.frame_sequence;f.s.monotonic_us+=1000;f.s.owner.game_time+=.01F;
        f.hud.frame_sequence=f.s.frame_sequence;
        const auto spanish=f.guide.Append(f.s,f.prompts,CoJUiLanguage::spanish,true,f.hud);
        check(spanish!=I::none&&f.hud.text.interaction.view().find(u"stick derecho a izquierda/derecha")!=std::u16string_view::npos&&
            f.hud.text.interaction.view().find(u"Rebound")!=std::u16string_view::npos,"Spanish guidance lost language or binding");
        f.prompts.active=false;
        check(f.Tick()==I::aim_fire&&f.hud.text.interaction.view().find(u"Right fire")!=std::u16string_view::npos&&
            f.hud.text.interaction.view().find(u"Rebound")==std::u16string_view::npos,"inactive binding failed to use semantic fallback");
        f.s.hands_empty=true;f.s.right_aim=Aim(true);f.s.raw.turn={};f.prompts.active=true;
        check(cojvr::runtime::SetActionPromptLabel(f.prompts,PromptAction::turn,"Custom Axis"),"axis binding setup failed");
        check(f.Tick()==I::draw&&f.hud.text.interaction.view().find(u"Custom Axis")!=std::u16string_view::npos,
            "direction guidance ignored runtime axis binding");
        f.Occupied();f.s.right_aim=Aim(false);f.s.raw.turn.y=1;f.s.delivered_axes={0,10,true};
        check(f.Tick()==I::aim_fire&&f.hud.text.interaction.view().find(u"Custom Axis left/right")!=std::u16string_view::npos,
            "duel aim guidance lost rebound horizontal axis label");
    }
    {
        Fixture f;f.Baseline();const auto first=f.hud.tutorial_owner_token;
        check(first&&first==f.guide.owner_token()&&f.hud.tutorial_until_ms==f.s.presentation_time_ms+500,
            "tutorial capture lacks independent owner and bounded deadline");
        (void)f.Tick();check(f.hud.tutorial_owner_token==first,"stable instruction churned its owner");
        f.s.native_phase=2;(void)f.Tick();const auto changed=f.guide.owner_token();
        check(changed&&changed!=first,"native phase replacement reused tutorial authority");
        f.s.owner.action_identity++;(void)f.Tick();
        check(f.guide.owner_token()&&f.guide.owner_token()!=changed,"new duel reused tutorial authority");
        f.s.recentered=true;check(f.Tick()==I::none&&!f.guide.owner_token()&&!f.hud.tutorial_owner_token,
            "recenter retained tutorial presentation authority");
        f.s.recentered=false;f.Baseline();const auto recovered=f.guide.owner_token();
        check(recovered&&recovered!=changed&&recovered!=first,"reacquisition reused a retired token");
        f.s.presentation_time_ms=0;check(f.Tick(false)==I::none&&!f.guide.owner_token(),
            "unbound presentation clock retained tutorial authority");
    }
    if(!failures)std::cout<<"duel tutorial: "<<checks<<" checks passed\n";
    return failures?1:0;
}
