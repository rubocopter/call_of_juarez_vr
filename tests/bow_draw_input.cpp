#include "games/call_of_juarez/bow_draw_input.hpp"
#include "games/call_of_juarez/bow_draw_geometry.hpp"
#include "games/call_of_juarez/bow_draw_session.hpp"
#include "runtime/gameplay_utility.hpp"
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
struct SessionFixture {
    CoJBowDrawSession session;
    cojvr::runtime::GameplayControlMapper mapper;
    cojvr::runtime::GameplayInputState raw{},fire{};
    CoJBowDrawSessionInput s{};
    SessionFixture(){
        raw.active=true;raw.input_context_generation=4;
        s.selected=s.allowed=s.counter_valid=true;s.player=2;s.context=3;
        s.native.valid=true;s.native.weapon_id=5;s.native.current_state=1;
        s.native.root.valid=s.native.arrow.valid=s.native.holder.valid=true;
        s.head_world_m={0,1.6F,0};s.left_world_m={-.05F,1.3F,-.4F};s.right_world_m={0,1.3F,-.4F};
        s.native.root.position=s.native.arrow.position={0,130,-40};
        s.native.root.forward={0,0,-1};s.native.root.right={-1,0,0};
    }
    CoJBowDrawSessionOutput Tick(bool fresh=true){
        if(fresh){++s.sequence;s.now_ms+=20;}
        fire=mapper.Update(raw);
        s.input_generation=raw.input_context_generation;
        s.right_available=raw.active&&(raw.digital_available&2U)!=0;s.right_held=raw.fire_right;
        auto input=s;input.allowed&=CoJBowDrawControlsAvailable(raw,fire);
        const auto out=session.Update(input);
        // Actual camera caller: begin with mapped gameplay fire, replacing it
        // only while the bow session claims the native channels.
        if(out.dispatch.claimed){fire.fire_right=out.dispatch.right;fire.fire_left=out.dispatch.left;}
        return out;
    }
    void Grab(){
        Tick();raw.fire_right=true;
        Check(Tick().gesture.grab&&fire.fire_right,"composed session must begin a fresh physical grab");
    }
};
int main(){
    for(int boundary=0;boundary<4;++boundary){
        SessionFixture f;f.Grab();f.s.selected=false;f.s.sequence=500;f.Tick();
        const auto old_context=f.s.context,old_player=f.s.player,old_generation=f.raw.input_context_generation;
        if(boundary==0)++f.raw.input_context_generation;
        if(boundary==1)f.s.context+=2;
        if(boundary==2){ // Presenter Stop/Start advances native context and restarts both counters.
            f.s.context+=2;f.raw.input_context_generation=1;f.mapper={};f.session.Reset();
        }
        if(boundary==3)++f.s.player;
        f.s.sequence=0;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"new source baseline relinquished held R2");
        const auto current_context=f.s.context,current_player=f.s.player,current_generation=f.raw.input_context_generation;
        // A returned retired source is not an available release from the current source.
        f.s.context=old_context;f.s.player=old_player;f.raw.input_context_generation=old_generation;
        f.raw.fire_right=false;f.s.sequence=900;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"retired source sample must fail closed");
        f.s.context=current_context;f.s.player=current_player;f.raw.input_context_generation=current_generation;
        f.s.sequence=1;f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"retired source release discharged current R2 claim");
        // A duplicate cannot release, or move the watermark's time so a later
        // genuinely fresh sample would be rejected against the duplicate's clock.
        const auto time=f.s.now_ms;
        f.raw.fire_right=false;f.s.now_ms+=10000;
        Check(f.Tick(false).dispatch.claimed,"duplicate source sample discharged physical claim");
        f.s.now_ms=time;f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"duplicate timestamp renewed the release watermark");
        f.raw.digital_available&=~2U;f.raw.fire_right=false;f.Tick();
        f.raw.digital_available|=2U;f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"new source unavailable false discharged claim");
        f.raw.fire_right=false;const auto released=f.Tick();
        Check(released.dispatch.claimed&&!f.fire.fire_right&&!released.dispatch.shot_requested,
            "fresh new source release sample must itself remain consumed");
        Check(!f.Tick().dispatch.claimed,"restarted source could not release below old sequence watermark");
        f.raw.fire_right=true;
        Check(!f.Tick().dispatch.claimed&&f.fire.fire_right,"new source fresh press failed to recover ordinary fire");
        // A retired source also cannot reacquire native bow dispatch after release.
        f.s.context=old_context;f.s.player=old_player;f.raw.input_context_generation=old_generation;
        f.s.selected=true;f.s.sequence=1000;f.raw.fire_right=false;f.Tick();f.raw.fire_right=true;
        const auto stale=f.Tick();
        Check(stale.dispatch.claimed&&!stale.gesture.grab&&!f.fire.fire_right&&!stale.owner_token,
            "retired source returned to live bow dispatch");
    }
    {
        SessionFixture f;f.Grab();f.s.selected=false;f.Tick();
        ++f.raw.input_context_generation;f.raw.fire_right=false;
        const auto baseline=f.Tick();
        Check(baseline.dispatch.claimed,"new source baseline release must stay consumed");
        f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"source transition borrowed old source release");
        f.raw.fire_right=false;f.Tick();
        Check(!f.Tick().dispatch.claimed,"new source fresh release did not recover");
    }
    {
        SessionFixture f;f.Grab();
        ++f.raw.input_context_generation;f.s.sequence=0;f.raw.fire_right=false;
        const auto baseline=f.Tick();
        Check(baseline.dispatch.claimed&&!baseline.dispatch.shot_requested,
            "new bow source baseline cannot release old draw");
        f.raw.fire_right=true;
        Check(!f.Tick().gesture.grab&&!f.fire.fire_right,
            "new bow source borrowed baseline release to reacquire consumed R2");
        f.raw.fire_right=false;f.Tick();f.raw.fire_right=true;
        Check(f.Tick().gesture.grab&&f.fire.fire_right,
            "fresh release after source baseline must rearm physical grab");
    }
    for(int fault=0;fault<4;++fault){
        SessionFixture f;f.Grab();
        if(fault==0)f.s.selected=false; // Failed owner proof and nonbow share this result.
        if(fault==1){f.s.selected=false;f.session.Reset();}
        if(fault==2){f.s.selected=false;++f.s.player;f.s.context+=2;}
        if(fault==3){f.s.allowed=false;f.Tick();f.s.selected=false;}
        for(int i=0;i<3;++i){const auto cancelled=f.Tick();
            Check(cancelled.dispatch.claimed&&!f.fire.fire_right&&!f.fire.fire_left&&
                !cancelled.dispatch.shot_requested&&!cancelled.owner_token,
                "repeated owner proof loss must retain consumed R2 through caller fallback");}
        f.s.allowed=true;
        // An unavailable/neutralized trigger cannot discharge a physical claim.
        f.raw.digital_available&=~2U;f.raw.fire_right=false;f.Tick();
        f.raw.digital_available|=2U;f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"unavailable release lost the bow's consumed hold");
        // Neither an inactive nor a duplicated sample is an observed release.
        f.raw.active=false;f.raw.fire_right=false;f.Tick();
        f.raw.active=true;f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"inactive release lost the bow's consumed hold");
        f.raw.fire_right=false;f.Tick(false);f.raw.fire_right=true;
        Check(f.Tick().dispatch.claimed&&!f.fire.fire_right,"duplicate release lost the bow's consumed hold");
        f.raw.fire_right=false;const auto release=f.Tick();
        Check(!f.fire.fire_right&&!release.dispatch.left&&!release.dispatch.shot_requested,
            "releasing cancelled bow hold must not shoot");
        Check(!f.Tick().dispatch.claimed,"available nonbow release must retire retained claim");
        f.raw.fire_right=true;
        Check(!f.Tick().dispatch.claimed&&f.fire.fire_right,"fresh nonbow press must preserve ordinary fire");
    }
    {
        SessionFixture f;f.Grab();f.s.selected=false;f.Tick();f.Tick();
        f.s.selected=true;f.s.native.weapon_id++;
        Check(!f.Tick().gesture.grab&&!f.fire.fire_right,"replacement bow cannot inherit held grab");
        f.raw.fire_right=false;
        Check(!f.Tick().dispatch.shot_requested,"reacquired bow cannot fire old cancelled release");
        f.raw.fire_right=true;
        Check(f.Tick().gesture.grab&&f.fire.fire_right,"available release must permit a fresh reacquired bow grab");
        f.s.native.current_state=129;f.s.native.can_attack=f.s.native.held_right=f.s.native.release_ready=true;
        f.s.right_world_m.z+=.2F;f.Tick();f.raw.fire_right=false;
        const auto shot=f.Tick();
        Check(shot.dispatch.shot_requested&&f.fire.fire_left&&f.fire.fire_right,
            "normal qualified bow release must still request its native shot");
        Check(!f.Tick().dispatch.left,"normal release cannot repeat native shot");
    }
    {
        SessionFixture f;f.s.selected=false;f.Tick();f.raw.fire_right=true;
        Check(!f.Tick().dispatch.claimed&&f.fire.fire_right,"startup nonbow input must not be claimed");
        f.s.selected=true;
        Check(!f.Tick().gesture.grab&&!f.fire.fire_right,"startup held bow R2 still needs a release");
        f.raw.fire_right=false;f.Tick();f.raw.fire_right=true;
        Check(f.Tick().gesture.grab,"startup release and fresh contact must still grab");
    }
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
