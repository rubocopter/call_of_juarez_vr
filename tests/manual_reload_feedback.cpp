// Compose the real persistent session and its production finger-feedback rule.
// Native transfer acceptance is supplied after an actual gesture release; no
// game, native finger writer or physical acceptance is implied by this test.
#include "games/call_of_juarez/manual_reload_session.hpp"
#include "games/call_of_juarez/reload_insertion_pose.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
namespace {
void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
Pose At(Vec3 p){Pose result;result.position=p;result.position_valid=result.orientation_valid=true;return result;}
struct Fixture {
    CoJManualReloadSession session;
    CoJReloadInsertionPose pulse;
    CoJManualReloadInput input{};
    GameplayInputState mapped{};
    FingerTrackingState left{},right{};
    CoJReloadInsertionPoseInput feedback{};
    unsigned inserts=0;
    explicit Fixture(int armed){
        input.enabled=input.allowed=true;input.player=7;input.context=5;
        input.raw.active=true;input.raw.input_context_generation=8;
        input.admission={armed,42,true,true};input.recovery=input.admission;
        input.head=At({0,1.6F,0});
        input.right=At(armed==0?Vec3{.2F,1.35F,-.35F}:Vec3{.2F,1.05F,-.1F});
        input.left=At(armed==1?Vec3{-.2F,1.35F,-.35F}:Vec3{-.2F,1.05F,-.1F});
        input.native.valid=input.native.probe_valid=true;input.native.weapon_id=42;
        input.native.reserve_readback=10;
        input.native.armed_hand=armed;input.native.states[armed]={1,1,1,1};
    }
    bool& Trigger(){return input.native.armed_hand==0?input.raw.fire_left:input.raw.fire_right;}
    Pose& Support(){return input.native.armed_hand==0?input.left:input.right;}
    Pose& Armed(){return input.native.armed_hand==0?input.right:input.left;}
    CoJManualReloadOutput Poll(unsigned dt=10){
        ++input.sequence;input.now+=dt;mapped=input.raw;
        auto out=session.Update(mapped,input);if(out.insert)++inserts;return out;
    }
    int PoseFeedback(const CoJManualReloadOutput& out,std::uint64_t event=0){
        feedback={};feedback.allowed=out.tracked&&input.native.probe_status==2;
        feedback.recentered=input.recentered;feedback.player=input.player;feedback.context=input.context;
        feedback.weapon=input.native.weapon_id;feedback.armed_hand=input.native.armed_hand;
        feedback.input_generation=input.raw.input_context_generation;feedback.sequence=input.sequence;
        feedback.now=input.now;feedback.insertion_event=event;
        feedback.support_trigger_held=Trigger();
        feedback.cartridge_held=CoJReloadInsertionHasCartridge(out.gesture,feedback.armed_hand,Trigger());
        feedback.left=input.left;feedback.right=input.right;
        left={};right={};return pulse.Apply(feedback,left,right);
    }
    void OpenAndInsert(){
        (void)Poll();input.raw.reload=true;Check(Poll().start,"manual session did not open");
        session.NoteStart(true);input.raw.reload=false;input.admission={};
        input.native.probe_status=2;input.native.states[input.native.armed_hand]={21,21,21,21};
        (void)Poll();(void)Poll();Trigger()=true;
        Check(Poll().gesture.claimed_hand==1-input.native.armed_hand,"fresh support pickup not claimed");
        const auto start=Support().position;const auto end=Armed().position;
        for(int step=1;step<=40;++step){
            const float t=float(step)/40;
            Support().position={start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t,start.z+(end.z-start.z)*t};
            Check(!Poll().insert,"held carry inserted before release");
        }
        Trigger()=false;auto out=Poll();Check(out.insert,"sampled release did not insert");
        session.CompleteInsertion(true,true,out);
        Check(PoseFeedback(out,1)==1-input.native.armed_hand,"native-confirmed thumb did not start");
    }
};
}
int main(){
    for(int armed=0;armed<2;++armed){
        Fixture f(armed);f.OpenAndInsert();
        std::uint64_t previous=0;
        for(unsigned age:{11,50,130,180}){
            auto out=f.Poll(static_cast<unsigned>(age-previous));previous=age;
            Check(out.gesture.cartridge_held&&out.gesture.claimed_hand==2,
                "continuation must remain an available unclaimed token");
            Check(!CoJReloadInsertionHasCartridge(out.gesture,armed,false),
                "available continuation token was mistaken for a physically held cartridge");
            const int result=f.PoseFeedback(out);
            Check(result==(age<180?1-armed:-1),"continuation availability cancelled thumb feedback");
            if(age==50)Check(std::fabs((armed==0?f.left:f.right).curls[0]-.75F)<.0001F,
                "native-confirmed thumb peak was preempted");
            Check(!out.insert&&f.inserts==1,"feedback/replenishment inserted another round");
        }
        Fixture pickup(armed);pickup.OpenAndInsert();(void)pickup.PoseFeedback(pickup.Poll(11));
        pickup.Trigger()=true;const auto held=pickup.Poll(10);
        Check(CoJReloadInsertionHasCartridge(held.gesture,armed,true),"fresh repickup not physical occupancy");
        Check(pickup.PoseFeedback(held)==-1,"fresh repickup did not take priority over the old thumb pulse");
        Check(!held.insert&&pickup.inserts==1,"repickup inserted without release");
        for(int fault=0;fault<7;++fault){
            Fixture loss(armed);loss.OpenAndInsert();
            auto out=loss.Poll(11);(void)loss.PoseFeedback(out);
            if(fault==0)loss.input.player++;
            if(fault==1)loss.input.raw.input_context_generation++;
            if(fault==2)loss.input.left.position_valid=false;
            if(fault==3)loss.input.recentered=true;
            if(fault==4)loss.input.native.probe_status=3;
            if(fault==5)loss.input.native.probe_status=4;
            if(fault==6)loss.pulse.Cancel(); // Native finger eligibility rejected the scoped write.
            out=loss.Poll(10);Check(loss.PoseFeedback(out)==-1,"lost native/presentation owner retained thumb");
        }
    }
}
