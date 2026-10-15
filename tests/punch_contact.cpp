#include "runtime/punch_contact.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
namespace {
int checks=0,failures=0;
void Check(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<message<<'\n';}}
bool Near(float a,float b){return std::abs(a-b)<.00001F;}
bool Same(Vec3 a,Vec3 b){return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z);}
bool Same(Quaternion a,Quaternion b){return Near(a.x,b.x)&&Near(a.y,b.y)&&Near(a.z,b.z)&&Near(a.w,b.w);}
Pose Tracked(Vec3 p){Pose pose{};pose.position=p;pose.position_valid=pose.orientation_valid=true;return pose;}
struct Fixture {
    MotionPunchContact policy;
    MotionPunchInput in{};
    Fixture(){in.allowed=true;in.owner=3;in.context=7;in.now_ms=1000;
        in.head=Tracked({0,1.6F,0});
        in.grip={Tracked({.2F,1.4F,-.2F}),Tracked({-.2F,1.4F,-.2F})};
        in.claim_available={true,true};}
    MotionPunchContactOutput Step(unsigned dt=11){++in.sequence;in.now_ms+=dt;return policy.Update(in);}
    void Ack(const MotionPunchContactOutput& out,int result=0){for(std::size_t h=0;h<2;++h)
        if(out.request[h])Check(policy.Complete(h,out.request_sequence[h],result),"current synchronous acknowledgment accepted");}
    void Arm(){Ack(Step());in.closed={true,true};Ack(Step());}
};
void RealTimeMotion(){
    for(int hand=0;hand<2;++hand)for(Vec3 step:{Vec3{0,0,-.009F},Vec3{.009F,0,0},Vec3{0,.009F,0}}){
        Fixture f;f.Arm();
        for(int n=0;n<4;++n){const auto previous=f.in.grip[hand];
            f.in.grip[hand].position.x+=step.x;f.in.grip[hand].position.y+=step.y;f.in.grip[hand].position.z+=step.z;
            const auto out=f.Step();
            Check(out.request[hand]&&out.allow_attack[hand],"sub-centimetre jab/hook/uppercut attacks immediately at 90Hz");
            Check(Same(out.previous_grip[hand].position,previous.position)&&
                Same(out.current_grip[hand].position,f.in.grip[hand].position),"request contains only latest measured segment");
            Check(out.dt_ms==11&&out.diagnostic[hand].world_speed_mps>.8F&&!out.diagnostic[hand].noise,
                "unique pose diagnostics use measured translation and time");f.Ack(out);
        }
    }
    {Fixture f;f.Step();f.in.closed[0]=true;f.in.grip[0].position.z-=.009F;
        const auto out=f.Step();Check(out.allow_attack[0],"close and move attacks without guard or launch travel");f.Ack(out);}
    {Fixture f;f.Arm();
        for(int n=0;n<240;++n){f.in.grip[0].position.x+=(n/20)%2?-.015F:.015F;
            const auto out=f.Step();Check(out.request[0]&&out.allow_attack[0],
                "sustained motion beyond old point/path/time bounds has no fence");f.Ack(out);}}
    {Fixture f;f.Arm();f.in.grip[0].position.z=-1.2F;f.Ack(f.Step());
        f.in.grip[0].position.z-=.009F;const auto out=f.Step();
        Check(out.request[0]&&out.allow_attack[0],"long reach has no arbitrary 1.1m lockout");f.Ack(out);}
    {Fixture f;f.Arm();f.in.grip[0].position.z-=.12F;const auto out=f.Step();
        Check(out.allow_attack[0]&&out.diagnostic[0].world_speed_mps>8.F,
            "human fast punch below segment continuity bound has no 8m/s rejection");f.Ack(out);}
}
void ContactBout(){
    for(int result:{1,2})for(int hand=0;hand<2;++hand){Fixture f;f.Arm();
        f.in.grip[hand].position.z-=.02F;auto out=f.Step();
        Check(out.allow_attack[hand],"fresh contact may request native attack");
        for(std::size_t h=0;h<2;++h)if(out.request[h])
            Check(f.policy.Complete(h,out.request_sequence[h],h==static_cast<std::size_t>(hand)?result:0),"native contact result acknowledged");
        for(int n=0;n<12;++n){if(n>3)f.in.grip[hand].position.x+=.02F;
            out=f.Step();Check(out.request[hand]&&!out.allow_attack[hand]&&out.diagnostic[hand].contact_latched,
                "static or moving persistent contact never retries delayed native attack");f.Ack(out,result);}
        // A stationary front-patch miss can clear the bout.
        out=f.Step();Check(out.request[hand]&&!out.allow_attack[hand],"static contact-clear probe cannot attack");f.Ack(out,0);
        out=f.Step();Check(out.request[hand]&&!out.allow_attack[hand]&&!out.diagnostic[hand].contact_latched,
            "miss clears latch but static fist still cannot attack");f.Ack(out);
        f.in.grip[hand].position.y+=.009F;out=f.Step();
        Check(out.allow_attack[hand],"post-clear next punch needs no return or trigger edge");f.Ack(out);
    }
    {Fixture f;f.Arm();f.in.grip[0].position.z-=.001F;auto out=f.Step();
        Check(out.request[0]&&!out.allow_attack[0],"slow motion probes without native attack permission");f.Ack(out,1);
        f.in.grip[0].position.z-=.02F;out=f.Step();
        Check(!out.allow_attack[0]&&out.diagnostic[0].contact_latched,"slow contact blocks acceleration while interpenetrating");f.Ack(out);}
    {Fixture f;f.Arm();for(int n=0;n<8;++n){const int h=n%2;
        f.in.grip[h].position.x+=(n/2)%2?-.02F:.02F;auto out=f.Step();
        Check(out.allow_attack[h]&&!out.allow_attack[1-h],"alternating held hands use independent motion admission");f.Ack(out,2);
        out=f.Step();Check(!out.allow_attack[0]&&!out.allow_attack[1],"contact probes never synthesize combo attacks");f.Ack(out,0);}}
}
void NoiseAndGeometry(){
    Fixture f;f.Arm();auto out=f.Step();
    Check(out.request[0]&&out.request[1]&&!out.allow_attack[0]&&!out.allow_attack[1],"fresh static closed poses query without attacking");f.Ack(out);
    f.in.grip[0].position.x+=.00005F;out=f.Step();
    Check(out.request[0]&&!out.allow_attack[0]&&out.diagnostic[0].noise,"sub-precision jitter still probes but cannot attack");f.Ack(out);
    for(int n=0;n<4;++n){f.in.head.position.z-=.02F;for(auto& p:f.in.grip)p.position.z-=.02F;
        out=f.Step();Check(!out.allow_attack[0]&&!out.allow_attack[1],"shared body translation cannot attack");f.Ack(out);}
    for(int n=0;n<4;++n){f.in.head.position.z+=.02F;out=f.Step();
        Check(!out.allow_attack[0]&&!out.allow_attack[1],"head recoil with static grips cannot attack");f.Ack(out);}
    const auto previous=f.in.grip[0];f.in.grip[0].orientation={0,.6F,0,.8F};out=f.Step();
    Check(out.request[0]&&!out.allow_attack[0]&&Same(out.previous_grip[0].orientation,previous.orientation)&&
        Same(out.current_grip[0].orientation,f.in.grip[0].orientation),"full grip orientations retained but rotation alone cannot attack");f.Ack(out);
    {Fixture g;g.Arm();const auto initial=g.in.grip;
        for(int n=1;n<5;++n){const float a=n*.2F;g.in.head.orientation={0,std::sin(a/2),0,std::cos(a/2)};
            for(int h=0;h<2;++h){const auto p=initial[h].position;
                g.in.grip[h].position={std::cos(a)*p.x+std::sin(a)*p.z,p.y,-std::sin(a)*p.x+std::cos(a)*p.z};}
            out=g.Step();Check(!out.allow_attack[0]&&!out.allow_attack[1],"shared head/body rotation cannot attack");g.Ack(out);}}
    {Fixture g;g.Arm();g.in.grip[0].position.z-=.3F;out=g.Step();
        Check(!out.request[0]&&out.geometry_reset[0]&&out.diagnostic[0].reason==MotionPunchContactReason::motion_discontinuity,
            "tracking jump discards only discontinuous segment");g.Ack(out);
        const auto base=g.in.grip[0];g.in.grip[0].position.z-=.009F;out=g.Step();
        Check(out.allow_attack[0]&&!out.geometry_reset[0]&&Same(out.previous_grip[0].position,base.position),
            "continuity rebaseline permits later closed movement without permanent release fence");g.Ack(out);}
}
void LossBarriers(){
    for(int fault=0;fault<17;++fault){Fixture f;f.Arm();
        f.in.grip[0].position.z-=.02F;auto pending=f.Step();
        Check(pending.request[0],"loss fixture has a current request");
        // A synchronous completion is required except in the missing-ack case.
        if(fault!=16)f.Ack(pending,2);
        if(fault==0)f.in.allowed=false;
        if(fault==1)++f.in.owner;
        if(fault==2)++f.in.context;
        if(fault==3)++f.in.player_generation;
        if(fault==4)++f.in.input_generation;
        if(fault==5)f.in.recentered=true;
        if(fault==6)f.in.head.position_valid=false;
        if(fault==7)f.in.head.orientation={0,0,0,0};
        if(fault==8)f.in.grip[0].position_valid=false;
        if(fault==9)f.in.grip[0].orientation_valid=false;
        if(fault==10)f.in.claim_available[0]=false;
        if(fault==11)f.in.grip[0].orientation.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==12)f.in.grip[0].position.x=std::numeric_limits<float>::infinity();
        if(fault==13)f.in.now_ms+=81;
        if(fault==14)f.in.now_ms-=20;
        if(fault==15)f.in.sequence-=2;
        auto out=f.Step();Check(!out.request[0]&&out.geometry_reset[0],"loss or missing completion cancels request and retires geometry");f.Ack(out);
        Check(!f.policy.Complete(0,pending.request_sequence[0],0),"pre-loss completion cannot clear current barriers");
        f.in.allowed=true;f.in.recentered=false;f.in.claim_available[0]=true;
        f.in.head=Tracked({0,1.6F,0});f.in.grip[0]=Tracked({.2F,1.4F,-.2F});
        f.Ack(f.Step());f.in.grip[0].position.z-=.009F;out=f.Step();
        Check(!out.request[0],"held trigger cannot resume after real source/context loss");f.Ack(out);
        f.in.closed[0]=false;f.Ack(f.Step());f.in.closed[0]=true;f.in.grip[0].position.z-=.009F;out=f.Step();
        Check(out.request[0]&&out.allow_attack[0],"fresh release restores immediate closed motion after loss");f.Ack(out);
    }
    for(int result:{-1,9}){Fixture f;f.Arm();auto out=f.Step();
        Check(f.policy.Complete(0,out.request_sequence[0],result),"current native failure consumed");
        if(out.request[1])f.policy.Complete(1,out.request_sequence[1],0);
        f.in.grip[0].position.z-=.009F;out=f.Step();
        Check(!out.request[0]&&out.request[1],"native failure fences only affected hand");f.Ack(out);}
}
void DuplicateAndAcknowledgment(){
    Fixture f;f.Arm();f.in.grip[0].position.z-=.009F;auto out=f.Step();const auto token=out.request_sequence[0];
    Check(!f.policy.Complete(2,token,2)&&!f.policy.Complete(0,token+100,2),"invalid hand and unmatched completion ignored");
    f.in.now_ms+=5;auto duplicate=f.policy.Update(f.in);
    Check(!duplicate.request[0]&&!duplicate.request[1],"duplicate pose never replays contact query");
    Check(f.policy.Complete(0,token,2)&&!f.policy.Complete(0,token,0),"fresh duplicate preserves one completion, duplicate ack cannot clear latch");
    if(out.request[1])f.policy.Complete(1,out.request_sequence[1],0);
    f.in.grip[0].position.z-=.009F;out=f.Step(6);
    Check(out.dt_ms==11&&!out.allow_attack[0]&&out.diagnostic[0].contact_latched,"duplicates retain unique-pose clock and contact latch");
    Check(!f.policy.Complete(0,token,0),"old acknowledgment cannot clear new pending contact");f.Ack(out);
    auto old=out.request_sequence[0];f.policy.Reset();f.in.sequence=0;f.in.closed={false,false};f.Ack(f.Step());
    f.in.closed={true,true};out=f.Step();
    Check(out.request_sequence[0]!=old&&!f.policy.Complete(0,old,2),"reset and reused producer sequence cannot revive old completion");f.Ack(out);
    for(int fault=0;fault<4;++fault){Fixture g;g.Arm();out=g.Step();
        g.in.now_ms+=5;
        if(fault==0)g.in.closed[0]=false;
        if(fault==1)g.in.claim_available[0]=false;
        if(fault==2)g.in.grip[0].orientation_valid=false;
        if(fault==3)g.in.now_ms+=81;
        g.policy.Update(g.in);
        Check(!g.policy.Complete(0,out.request_sequence[0],2),"duplicate release/loss/staleness cancels in-flight completion");
        g.in.closed[0]=true;g.in.claim_available[0]=true;g.in.grip[0].orientation_valid=true;
        // Complete the unaffected hand unless the global clock expired.
        if(fault!=3)g.policy.Complete(1,out.request_sequence[1],0);
        g.in.grip[0].position.z-=.009F;out=g.Step();
        Check(out.request[0]==(fault==0),"only valid available duplicate release establishes new baseline");g.Ack(out);
    }
}
void ProducerGameCadence(){
    for(auto phase:{0ULL,3700ULL,11000ULL}){Fixture f;unsigned requests=0,attacks=0;
        for(std::uint64_t tick=0;tick<100;++tick){const auto elapsed=tick*1000000/180;
            const auto pose=elapsed*90/1000000;f.in.sequence=pose+1;f.in.now_ms=(1000000+phase+elapsed)/1000;
            // Integer pose publication plus delivery rounding can produce a
            // 17ms unique-pose interval; 15mm still exceeds .75m/s there.
            f.in.closed[0]=pose>=2;f.in.grip[0].position.z=-.2F-.015F*static_cast<float>(pose);
            const auto out=f.policy.Update(f.in);requests+=out.request[0];attacks+=out.allow_attack[0];f.Ack(out);}
        Check(requests>40&&requests<51&&attacks==requests,"90Hz producer/180Hz consumer emits once per unique closed pose without travel fence");}
}
void ContactContinuity(){
    {Fixture f;f.Arm();auto out=f.Step();
        Check(f.policy.Complete(0,out.request_sequence[0],MotionPunchContactResult::attack_accepted),
            "typed native result supports synchronous completion");
        f.policy.Complete(1,out.request_sequence[1],0);
        f.in.grip[0].position.z-=.3F;out=f.Step();
        Check(!out.request[0]&&out.geometry_reset[0]&&out.diagnostic[0].contact_latched,
            "skipped jump retires geometry without clearing genuine contact");f.Ack(out);
        f.in.grip[0].position.z-=.02F;out=f.Step();
        Check(out.request[0]&&!out.allow_attack[0]&&out.diagnostic[0].contact_latched,
            "post-jump continuous probe preserves latch until actual miss");f.Ack(out,0);
        f.in.grip[0].position.z-=.02F;out=f.Step();
        Check(out.allow_attack[0],"post-jump actual contact clear allows next held punch");f.Ack(out);}
    {Fixture f;f.Arm();f.in.grip[0].position.z-=.014F;auto out=f.Step(20);
        Check(out.request[0]&&!out.allow_attack[0]&&!out.diagnostic[0].noise,
            "measurable motion below .75m/s probes without attacking");f.Ack(out);
        f.in.grip[0].position.z-=.016F;out=f.Step(20);
        Check(out.allow_attack[0],"motion above .75m/s allows native attack without accumulated travel");f.Ack(out);}
    {Fixture f;f.Arm();const auto out=f.Step();
        f.in.now_ms+=40;f.policy.Update(f.in);f.in.now_ms+=40;f.policy.Update(f.in);
        f.in.now_ms+=1;const auto stale=f.policy.Update(f.in);
        Check(stale.geometry_reset[0]&&!stale.request[0]&&
            !f.policy.Complete(0,out.request_sequence[0],0),"duplicate delivery cannot renew original freshness or pending request");}
    {Fixture f;f.in.closed={true,true};auto out=f.Step();
        Check(!out.request[0]&&!out.request[1],"startup held fists require a released baseline");
        f.in.grip[0].position.z-=.02F;out=f.Step();
        Check(!out.request[0],"startup motion cannot bypass released baseline");}
    {Fixture f;f.Arm();auto out=f.Step();f.Ack(out,2);
        f.in.closed[0]=false;out=f.Step();Check(out.geometry_reset[0]&&!out.request[0],"release retires cached contact geometry");f.Ack(out);
        f.in.closed[0]=true;f.in.grip[0].position.z-=.02F;out=f.Step();
        Check(out.allow_attack[0]&&!out.diagnostic[0].contact_latched,
            "deliberate release and reclose permits immediate fresh attack");f.Ack(out);}
}
void SkippedGeometry(){
    for(int hand=0;hand<2;++hand)for(int contact:{0,1,2}){Fixture f;f.Arm();
        auto out=f.Step();f.Ack(out,contact);
        f.in.grip[hand].position.z-=.02F;out=f.Step();const auto token=out.request_sequence[hand];
        Check(out.request[hand]&&out.allow_attack[hand]==(contact==0),"geometry skip fixture retains native contact state");
        Check(!f.policy.SkipGeometry(2,token)&&!f.policy.SkipGeometry(hand,token+100),
            "geometry skip rejects invalid hand and unmatched token");
        Check(f.policy.SkipGeometry(hand,token),"geometry skip consumes current pending request");
        Check(!f.policy.SkipGeometry(hand,token)&&!f.policy.Complete(hand,token,0),
            "skipped token cannot be skipped twice or acknowledged as native clear");
        f.policy.Complete(1-hand,out.request_sequence[1-hand],0);
        f.in.grip[hand].position.z-=.02F;out=f.Step();
        Check(out.request[hand]&&out.diagnostic[hand].released&&
            out.diagnostic[hand].contact_latched==(contact!=0)&&out.allow_attack[hand]==(contact==0),
            "next continuous request preserves release and latch after geometry skip");
        Check(!f.policy.SkipGeometry(hand,token)&&!f.policy.Complete(hand,token,2),
            "old skipped token cannot consume next native request");f.Ack(out,0);
        f.in.grip[hand].position.z-=.02F;out=f.Step();
        Check(out.allow_attack[hand],"actual native clear after geometry skip restores attack admission");f.Ack(out);
    }
}
void RepeatedOpenGeometryCache(){
    for(int hand=0;hand<2;++hand){Fixture f;
        // Mirror the adapter: a fresh valid open pose measures and retains
        // skin geometry, a repeated delivery reads none, and reset retires it.
        auto out=f.Step();const auto open_sequence=f.in.sequence;
        const auto open_grip=f.in.grip[hand];
        bool cached=true;std::uint64_t cached_sequence=open_sequence;
        f.in.now_ms+=5;out=f.policy.Update(f.in);
        if(out.geometry_reset[hand]){cached=false;cached_sequence=0;}
        Check(!out.request[hand]&&!out.geometry_reset[hand]&&out.diagnostic[hand].released,
            "valid repeated open cancels input state without retiring unique skin geometry");
        f.in.closed[hand]=true;f.in.grip[hand].position.z-=.009F;
        out=f.Step(6);
        const bool continuous=cached&&cached_sequence==out.previous_pose_sequence[hand];
        Check(out.request[hand]&&out.allow_attack[hand]&&continuous&&!out.geometry_reset[hand]&&
            out.previous_pose_sequence[hand]==open_sequence&&Same(out.previous_grip[hand].position,open_grip.position),
            "first moving close after repeated open retains continuous skin segment and attack permission");f.Ack(out,2);
        // Release on a repeated closed delivery must still cancel pending
        // native work and the contact latch, while preserving measured poses.
        out=f.Step();const auto pending=out.request_sequence[hand];
        const auto released_sequence=f.in.sequence;f.in.closed[hand]=false;f.in.now_ms+=5;
        out=f.policy.Update(f.in);
        Check(!out.geometry_reset[hand]&&out.diagnostic[hand].released&&!out.diagnostic[hand].contact_latched&&
            !f.policy.Complete(hand,pending,2),"valid duplicate release cancels pending contact but preserves geometry");
        f.in.closed[hand]=true;f.in.grip[hand].position.z-=.009F;out=f.Step(6);
        Check(out.request[hand]&&out.allow_attack[hand]&&out.previous_pose_sequence[hand]==released_sequence,
            "duplicate release clears old latch and admits fresh moving close");f.Ack(out);
    }
}
}
int main(){RealTimeMotion();ContactBout();NoiseAndGeometry();LossBarriers();DuplicateAndAcknowledgment();ProducerGameCadence();ContactContinuity();SkippedGeometry();RepeatedOpenGeometryCache();
    std::cout<<"punch_contact: "<<checks<<" checks, "<<failures<<" failures\n";return failures?EXIT_FAILURE:EXIT_SUCCESS;}
