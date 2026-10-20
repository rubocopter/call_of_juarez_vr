#include "games/call_of_juarez/bow_draw_input.hpp"
#include "games/call_of_juarez/bow_draw_geometry.hpp"
#include "games/call_of_juarez/bow_draw_session.hpp"
#include <cstdlib>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct Fixture {
    CoJBowDrawInput adapter;
    CoJBowDrawDispatchSample s{};
    Fixture(){s.selected=s.eligible=s.counter_valid=true;s.weapon=0x80000021U;
        s.player=2;s.context=3;s.input_generation=4;}
    CoJBowDrawDispatch Step(){++s.sequence;s.now_ms+=20;return adapter.Update(s);}
    void Draw(){Step();s.drawing=true;Check(Step().right,"physical grab must request native right tension");
        s.alternate_ready=s.can_attack=s.native_right_held=true;}
    CoJBowDrawDispatch Release(){s.drawing=false;s.release_fire=true;return Step();}
};
int main(){
    {
        cojvr::runtime::GameplayInputState raw{},filtered{};raw.active=filtered.active=true;
        Check(CoJBowDrawControlsAvailable(raw,filtered),"ordinary available bow input remains eligible");
        raw.weapon_radial=true;
        Check(!CoJBowDrawControlsAvailable(raw,filtered),"wheel ownership must cancel physical bow before raw R2 can bypass filtering");
        raw.weapon_radial=false;raw.fire_right=true;
        Check(!CoJBowDrawControlsAvailable(raw,filtered),"mapper release barrier must precede raw right trigger claim");
        raw.fire_right=false;Check(CoJBowDrawControlsAvailable(raw,filtered),"real available release can establish fresh baseline");
        raw.fire_right=filtered.fire_right=true;Check(CoJBowDrawControlsAvailable(raw,filtered),"fresh admitted R2 may begin draw");
        raw.interact=true;Check(!CoJBowDrawControlsAvailable(raw,filtered),"interaction owns its trigger context");
    }
    {
        CoJBowDrawSession session;CoJBowDrawSessionInput s{};
        s.selected=s.allowed=s.right_available=s.counter_valid=true;
        s.player=2;s.context=3;s.input_generation=4;
        s.native.valid=true;s.native.weapon_id=5;s.native.current_state=1;
        s.native.root.valid=s.native.arrow.valid=s.native.holder.valid=true;
        s.head_world_m={0,1.6F,0};s.left_world_m={-.05F,1.3F,-.4F};
        s.right_world_m={0,1.3F,-.4F};
        s.native.root.position={0,130,-40};s.native.arrow.position={0,130,-40};
        s.native.root.forward={0,0,-1};s.native.root.right={-1,0,0};
        auto tick=[&]{++s.sequence;s.now_ms+=20;return session.Update(s);};
        tick();s.right_held=true;const auto grabbed=tick();
        Check(grabbed.dispatch.claimed&&grabbed.dispatch.right&&grabbed.gesture.grab,
            "full session physical contact+R2 starts native tension without L2");
        Check(grabbed.feedback.kind==cojvr::runtime::BowDrawFeedbackKind::grab,"grab feedback follows same owner");
        s.native.current_state=129;s.native.can_attack=s.native.held_right=s.native.release_ready=true;
        s.right_world_m.z+=.2F;
        const auto drawn=tick();Check(drawn.gesture.drawing&&!drawn.dispatch.left,"hand retraction does not fire while held");
        s.right_world_m.x=.21F;
        const auto sideways=tick();
        Check(sideways.gesture.drawing&&!sideways.gesture.cancelled&&sideways.dispatch.right&&
            !sideways.dispatch.left&&sideways.gesture.normalized_draw==0,
            "soft alignment loss keeps native tension channel held and suppresses shot tension");
        s.right_world_m.x=0;Check(tick().dispatch.right,"realignment does not require another R2 press");
        s.right_held=false;const auto shot=tick();
        Check(shot.dispatch.shot_requested&&shot.dispatch.left&&shot.dispatch.right,
            "full physical release pulses native shot with stable owner and ready native state");
        Check(shot.feedback.kind==cojvr::runtime::BowDrawFeedbackKind::release,"release feedback is emitted only for admitted request");
        const auto epoch=shot.owner_token;
        Check(!tick().dispatch.left,"full session never repeats held native attack");
        ++s.attack_entries;Check(!tick().dispatch.right,"native attempt retires protocol hold");
        s.allowed=false;Check(!tick().owner_token,"context loss immediately invalidates haptic owner");
        s.allowed=true;const auto recovered=tick();
        Check(recovered.owner_token&&recovered.owner_token!=epoch,"reacquisition uses distinct feedback epoch");
        Check(!recovered.dispatch.left,"context recovery cannot replay release");
        s.native.current_state=1;s.native.held_right=false;s.native.release_ready=false;
        s.right_world_m={0,1.3F,-.4F};tick();s.right_held=true;
        Check(tick().gesture.grab,"fresh recovery press can establish a new physical grab");
        cojvr::runtime::GameplayInputState raw{},filtered{};raw.active=filtered.active=true;
        raw.fire_right=filtered.fire_right=true;raw.weapon_radial=true;
        s.allowed=CoJBowDrawControlsAvailable(raw,filtered);
        const auto wheel=tick();Check(!wheel.dispatch.right&&!wheel.dispatch.left&&!wheel.owner_token,
            "opening wheel during draw cancels native hold and feedback owner");
        raw.weapon_radial=false;raw.fire_right=filtered.fire_right=false;s.right_held=false;
        s.allowed=CoJBowDrawControlsAvailable(raw,filtered);
        Check(!tick().dispatch.left,"release at wheel close cannot fire previous draw");
        raw.fire_right=true;s.right_held=true;
        s.allowed=CoJBowDrawControlsAvailable(raw,filtered);
        Check(!tick().gesture.grab,"mapper-suppressed held trigger cannot bypass fresh-release barrier");
    }
    {
        CoJBowNockReference nock;CoJBowDrawNativeObservation n{};
        n.valid=true;n.weapon_id=9;n.root.valid=n.arrow.valid=true;
        n.root.position={10,20,30};n.arrow.position={10,20,28};
        cojvr::runtime::Vec3 point{};
        Check(nock.Observe(n,2,3,false,point)&&point.z==28,"authored DArrow establishes rest nock point");
        n.root.position[0]=30;n.arrow.position={1000,1000,1000};
        Check(nock.Observe(n,2,3,true,point)&&point.x==30&&point.z==28,
            "native arrow tension animation cannot move frozen gesture origin");
        n.weapon_id=10;
        Check(!nock.Observe(n,2,3,true,point),"owner replacement cannot inherit retained nock");
        Check(!nock.Observe(n,2,3,true,point),"replacement while held requires a new idle reference");
        Check(nock.Observe(n,2,3,false,point),"new idle authoring reference can establish new owner");
        n.root.right[0]=2;
        Check(!nock.Observe(n,2,3,false,point),"non-rigid nock source cannot repair itself");
    }
    {
        Fixture f;f.Draw();const auto shot=f.Release();
        Check(shot.claimed&&shot.left&&shot.right&&shot.shot_requested&&shot.pending,
            "physical R2 release must pulse native left attack while preserving right prerequisite");
        Check(!f.adapter.Update(f.s).left,"duplicate release must not repeat native shot request");
        f.s.release_fire=false;const auto waiting=f.Step();
        Check(waiting.right&&!waiting.left&&waiting.pending,"scheduled attack keeps native tension until entry observation");
        ++f.s.attack_entries;const auto completed=f.Step();
        Check(!completed.right&&!completed.left&&!completed.pending,"native attack entry retires tension without another request");
        Check(!f.Step().left,"completed request never retries");
    }
    for(int unavailable=0;unavailable<4;++unavailable){
        Fixture f;f.Draw();
        if(unavailable==0)f.s.alternate_ready=false;
        if(unavailable==1)f.s.can_attack=false;
        if(unavailable==2)f.s.native_right_held=false;
        if(unavailable==3)f.s.counter_valid=false;
        const auto rejected=f.Release();Check(!rejected.left&&!rejected.shot_requested,"unready release must not attack");
        f.s.alternate_ready=f.s.can_attack=f.s.native_right_held=f.s.counter_valid=true;
        f.s.release_fire=false;Check(!f.Step().left,"native recovery must not queue rejected release");
    }
    {
        Fixture f;f.Draw();f.Release();f.s.release_fire=false;++f.s.weapon;
        const auto changed=f.Step();Check(!changed.left&&!changed.right&&!changed.pending,"replacement aborts old pending attack ownership");
    }
    {
        Fixture f;f.Draw();f.Release();f.s.release_fire=false;
        for(int i=0;i<51;++i)f.Step();
        Check(!f.adapter.pending()&&!f.Step().right,"bounded pending timeout aborts without retry");
    }
    {
        Fixture f;f.Draw();f.Release();f.s.release_fire=false;
        f.s.now_ms+=110;
        const auto stale=f.adapter.Update(f.s);
        Check(!stale.right&&!stale.left&&!stale.pending,"duplicate pose cannot keep native tension alive past freshness watchdog");
    }
    {
        Fixture f;f.Draw();f.s.eligible=false;
        const auto lost=f.Step();Check(lost.claimed&&!lost.left&&!lost.right,"selected but unavailable bow fails closed");
        f.s.selected=false;Check(!f.Step().claimed,"ordinary weapons retain original input owner");
    }
    std::cout<<"bow physical release/native channel admission passed\n";
}
