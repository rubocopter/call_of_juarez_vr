#include "runtime/punch_gesture.hpp"
#include "runtime/gameplay_utility.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
namespace {
int failures=0,checks=0;
void Check(bool value,const char* name){++checks;if(!value){std::cerr<<name<<'\n';++failures;}}
Pose Tracked(Vec3 p){Pose v{};v.position=p;v.position_valid=v.orientation_valid=true;return v;}
// The scratch boundary replay substitutes the adapter's actual clock expression
// here, with deterministic GetTickCount64/MonotonicMicroseconds clock values.
std::uint64_t CadenceTimestamp(std::uint64_t delivery_us){return delivery_us/1000;}
void CheckProducerGameCadence(){
    for(int hand=0;hand<2;++hand)for(const auto phase_us:{0ULL,3700ULL,11000ULL}){
        MotionPunchGesture gesture;MotionPunchInput in{};
        in.allowed=true;in.owner=3;in.context=7;in.head=Tracked({0,1.6F,0});
        in.grip={Tracked({.2F,1.4F,-.2F}),Tracked({-.2F,1.4F,-.2F})};
        in.claim_available={true,true};
        unsigned strikes=0,discontinuities=0;bool armed=false,collected=false;
        // Presenter publishes at 90 Hz; a 180 Hz game sometimes consumes the
        // same pose twice. Close after a real release, settle, extend, then stop.
        for(std::uint64_t tick=0;tick<100;++tick){
            const auto elapsed_us=tick*1000000/180;
            const auto pose=elapsed_us*90/1000000;
            in.sequence=pose+1;in.now_ms=CadenceTimestamp(1000000+phase_us+elapsed_us);
            in.closed[hand]=pose>=3;
            in.grip[hand].position.z=pose<30?-.2F:pose==30?-.24F:pose==31?-.28F:-.32F;
            const auto out=gesture.Update(in);
            strikes+=out.strike[hand];
            discontinuities+=out.diagnostic[hand].reason==MotionPunchReason::time_discontinuity;
            armed|=out.diagnostic[hand].stage==MotionPunchStage::armed;
            collected|=out.diagnostic[hand].stage==MotionPunchStage::collecting;
        }
        Check(discontinuities==0,"90Hz producer/180Hz game: unique poses keep valid timing");
        Check(armed&&collected,"90Hz producer/180Hz game: release and guard survive delivery cadence");
        Check(strikes==1,"90Hz producer/180Hz game: one measured stroke, no duplicate or held replay");
    }
}
struct Fixture {
    MotionPunchGesture gesture;
    MotionPunchInput in{};
    Fixture(){in.allowed=true;in.owner=3;in.context=7;in.head=Tracked({0,1.6F,0});
        in.grip={Tracked({.2F,1.4F,-.2F}),Tracked({-.2F,1.4F,-.2F})};
        in.claim_available={true,true};}
    MotionPunchOutput Step(float z=0){++in.sequence;in.now_ms+=20;in.grip[0].position.z+=z;return gesture.Update(in);}
    void Arm(){Step();in.closed[0]=true;for(int n=0;n<7;++n)Step();}
};
void NaturalHeldCombos(){
    // These regressions separate motion admission from button edges/settling.
    {Fixture f;f.Step();f.in.closed[0]=true;f.Step(-.09F);
        Check(f.Step().strike[0],"close and immediately move: no mandatory still guard");}
    for(int hand=0;hand<2;++hand)for(bool alternating:{false,true}){
        Fixture f;f.in.grip[0].position.z=f.in.grip[1].position.z=-.35F;
        f.Step();f.in.closed={true,true};f.Step();
        unsigned strikes[2]{};unsigned return_hits=0,curves=0;
        for(int stroke=0;stroke<4;++stroke){
            const int h=alternating?(hand+stroke)%2:hand;
            const float sign=h==0?-1.F:1.F;
            const std::array<Vec3,3> arc={Vec3{sign*.04F,0,.01F},Vec3{sign*.06F,0,.03F},Vec3{sign*.05F,0,.02F}};
            const auto start=f.in.grip[h].position;
            for(int n=0;n<3;++n){f.in.grip[h].position.x+=arc[n].x;f.in.grip[h].position.z+=arc[n].z;
                auto o=f.Step();strikes[0]+=o.strike[0];strikes[1]+=o.strike[1];}
            // First return sample completes the outgoing curve, excludes recoil.
            f.in.grip[h].position.x-=arc[2].x;f.in.grip[h].position.z-=arc[2].z;
            auto o=f.Step();strikes[0]+=o.strike[0];strikes[1]+=o.strike[1];
            curves+=o.strike[h]&&o.sweep[h].count==4&&
                std::abs(o.end[h].z+.29F)<.0001F&&
                std::abs(o.sweep[h].points[0].x-start.x)<.0001F&&
                std::abs(o.sweep[h].points[1].x-(start.x+arc[0].x))<.0001F&&
                std::abs(o.sweep[h].points[1].z+.34F)<.0001F&&
                std::abs(o.sweep[h].points[2].z+.31F)<.0001F;
            for(int n=1;n>=0;--n){f.in.grip[h].position.x-=arc[n].x;f.in.grip[h].position.z-=arc[n].z;
                o=f.Step();return_hits+=o.strike[0]+o.strike[1];}
            // No stop, trigger release, or radial extension between hooks.
        }
        Check(strikes[hand]==(alternating?2U:4U)&&strikes[1-hand]==(alternating?2U:0U),
            "held natural hooks: every repeated/alternating stroke emits exactly once");
        Check(curves==4,"positive-Z hook retains all measured corners through terminal point");
        Check(return_hits==0,"held hook return is recovery, never a second attack");
        for(int n=0;n<30;++n){auto o=f.Step();Check(!o.strike[0]&&!o.strike[1],"stationary held fists never repeat");}
    }
    {Fixture f;f.Step();f.in.closed[0]=true;unsigned hits=0;
        for(int stroke=0;stroke<4;++stroke){
            for(int n=0;n<3;++n){auto o=f.Step(-.04F);hits+=o.strike[0];}
            auto o=f.Step(.04F);hits+=o.strike[0];
            for(int n=0;n<2;++n)Check(!f.Step(.04F).strike[0],"jab recoil cannot attack");
        }
        Check(hits==4,"held jabs rearm at velocity turn without a stationary guard");}
}

void FineCadenceCombos(){
    // 90 Hz strokes can meet the existing .75m/s threshold with sub-centimetre
    // segments. Turning must depend on direction, not a 1cm frame-size floor.
    for(int hand=0;hand<2;++hand){Fixture f;
        f.in.grip[hand].position.z=-.35F;f.Step();f.in.closed[hand]=true;
        unsigned hits=0,return_hits=0;
        const float dx=hand==0?-.009F:.009F;
        for(int stroke=0;stroke<4;++stroke){
            for(int n=0;n<12;++n){f.in.now_ms-=9;
                f.in.grip[hand].position.x+=dx;f.in.grip[hand].position.z+=.002F;
                hits+=f.Step().strike[hand];}
            f.in.now_ms-=9;f.in.grip[hand].position.x-=dx;f.in.grip[hand].position.z-=.002F;
            hits+=f.Step().strike[hand];
            for(int n=0;n<11;++n){f.in.now_ms-=9;
                f.in.grip[hand].position.x-=dx;f.in.grip[hand].position.z-=.002F;
                return_hits+=f.Step().strike[hand];}
        }
        Check(hits==4,"90Hz sub-centimetre velocity turns complete each held hook");
        Check(return_hits==0,"90Hz fine-cadence recoil cannot attack");
    }
}

void RecoveryTransitions(){
    for(int hand=0;hand<2;++hand){
        const float sign=hand==0?-1.F:1.F;
        // A paused return remains recovery even if it resumes at punch speed.
        for(int pause:{1,7}){Fixture f;f.in.grip[hand].position.z=-.35F;
            f.Step();f.in.closed[hand]=true;
            for(int n=0;n<6;++n){f.in.grip[hand].position.x+=sign*.04F;f.Step();}
            Check(f.Step().strike[hand],"paused return fixture completes outgoing hook");
            for(int n=0;n<2;++n){f.in.grip[hand].position.x-=sign*.05F;
                Check(!f.Step().strike[hand],"initial hook return cannot attack");}
            for(int n=0;n<pause;++n){const auto o=f.Step();
                Check(!o.strike[hand]&&o.diagnostic[hand].stage==MotionPunchStage::recovery,
                    "return stop preserves recovery direction without a timed gate");}
            for(int n=0;n<3;++n){f.in.grip[hand].position.x-=sign*.04F;
                Check(!f.Step().strike[hand],"continued paused return cannot launch an attack");}
            Check(!f.Step().strike[hand],"continued paused return cannot finish an attack");
            const auto apex=f.in.grip[hand].position;
            for(int n=0;n<3;++n){f.in.grip[hand].position.x+=sign*.04F;f.Step();}
            const auto next=f.Step();
            Check(next.strike[hand]&&std::abs(next.start[hand].x-apex.x)<.0001F,
                "new hook after paused return starts at actual return apex");
        }
        // Jab recovery can flow directly into a perpendicular hook/uppercut.
        for(bool ascending:{false,true}){Fixture f;f.Step();f.in.closed[hand]=true;
            for(int n=0;n<3;++n){f.in.grip[hand].position.z-=.04F;f.Step();}
            f.in.grip[hand].position.z+=.04F;
            Check(f.Step().strike[hand],"mixed combo jab completes at first recoil");
            for(int n=0;n<2;++n){f.in.grip[hand].position.z+=.04F;
                Check(!f.Step().strike[hand],"mixed combo jab return cannot attack");}
            const auto apex=f.in.grip[hand].position;
            for(int n=0;n<3;++n){
                if(ascending)f.in.grip[hand].position.y+=.04F;
                else f.in.grip[hand].position.x+=sign*.04F;
                Check(!f.Step().strike[hand],"perpendicular next stroke collects until stop");}
            const auto next=f.Step();
            Check(next.strike[hand]&&next.sweep[hand].count==4&&
                std::abs(next.start[hand].x-apex.x)<.0001F&&
                std::abs(next.start[hand].y-apex.y)<.0001F&&
                std::abs(next.start[hand].z-apex.z)<.0001F,
                "perpendicular next punch retains every new segment without recovery replay");
        }
        {Fixture f;f.Step();f.in.closed[hand]=true;
            const auto start=f.in.grip[hand].position;
            f.in.grip[hand].position.z-=.09F;f.Step();
            f.in.grip[hand].position.z+=.09F;const auto first=f.Step();
            Check(first.strike[hand]&&first.sweep[hand].count==2&&
                std::abs(first.end[hand].z-(start.z-.09F))<.0001F,
                "single recoil qualifies recovery but remains excluded from completed sweep");
            f.in.grip[hand].position.z-=.09F;f.Step();
            const auto next=f.Step();
            Check(next.strike[hand]&&next.sweep[hand].count==2&&
                std::abs(next.start[hand].z-start.z)<.0001F,
                "single-sample full return permits immediate next jab without extra return");
        }
    }
}

void NaturalStrokeBounds(){
    {Fixture f;f.Step();f.in.closed[0]=true;unsigned hits=0;
        for(int n=0;n<3;++n)hits+=f.Step(-.04F).strike[0];
        for(int n=0;n<24;++n)hits+=f.Step(-.012F).strike[0];
        Check(hits==0,"moving stroke beyond 240ms cannot emit a truncated curve");
        auto o=f.Step();
        Check(o.strike[0]&&o.sweep[0].count==28&&std::abs(o.end[0].z+.608F)<.0001F,
            "long slower stroke retains its complete physical path until stop");}
    for(bool point_limit:{false,true}){
        Fixture f;f.Step();f.in.closed[0]=true;f.Step(-.09F);
        unsigned hits=0,limits=0;
        for(int n=0;n<70;++n){if(point_limit)f.in.now_ms-=15;
            const auto o=f.Step(point_limit?-.003F:-.04F);
            hits+=o.strike[0];limits+=o.diagnostic[0].reason==MotionPunchReason::path_limit;}
        const auto o=f.Step();
        Check(hits==0&&!o.strike[0]&&limits==1,
            "64 point/0.8m bounds cancel whole stroke rather than truncate or replay");
        Check(o.diagnostic[0].stage==MotionPunchStage::awaiting_release,
            "bounded path cancellation preserves fresh release barrier");}
    {Fixture f;f.Step();f.in.closed[0]=true;
        for(int n=0;n<6;++n)f.Step(-.04F);
        auto o=f.Step(.05F);Check(o.strike[0],"extended jab completes on first recoil sample");
        Check(!f.Step(.05F).strike[0],"partial recoil remains recovery");
        for(int n=0;n<3;++n)Check(!f.Step(-.04F).strike[0],"next jab collects after partial return turn");
        o=f.Step();Check(o.strike[0]&&std::abs(o.start[0].z+.34F)<.0001F,
            "partial return rearms without returning to original guard or full extension");}
    for(int hand=0;hand<2;++hand){Fixture f;f.Step();f.in.closed[hand]=true;
        for(int n=0;n<3;++n){f.in.grip[hand].position.y+=.04F;f.Step();}
        auto o=f.Step();Check(o.strike[hand]&&o.sweep[hand].count==4,
            "immediate-close uppercut retains ascending measured trajectory");}
    {Fixture f;f.Step();f.in.closed[0]=true;
        for(int n=0;n<3;++n){f.in.grip[0].position.x-=.01F;
            Check(!f.Step(.04F).strike[0],"predominantly backward diagonal is retraction, not hook");}
        Check(!f.Step().strike[0],"stopped retraction cannot attack");}
}

}
int main(){
    CheckProducerGameCadence();
    NaturalHeldCombos();
    NaturalStrokeBounds();
    FineCadenceCombos();
    RecoveryTransitions();
    for(int hand=0;hand<2;++hand)for(const auto delay:{0U,10U}){
        Fixture f;f.Step();f.in.now_ms+=delay;
        const auto open=f.gesture.Update(f.in);
        Check(open.diagnostic[hand].stage==MotionPunchStage::open,
            "repeated open pose preserves confirmed release");
        f.in.closed[hand]=true;const auto close=f.Step();
        Check(!close.strike[hand]&&close.diagnostic[hand].stage==MotionPunchStage::armed,
            "unique close after repeated open arms immediately");
        for(int n=0;n<7;++n)f.Step();f.in.grip[hand].position.z-=.09F;f.Step();
        Check(f.Step().strike[hand],"deliberate punch follows repeated open baseline");
    }
    for(int hand=0;hand<2;++hand){Fixture f;f.Step();f.in.closed[hand]=true;
        for(int n=0;n<7;++n)f.Step();f.in.grip[hand].position.z-=.09F;f.Step();
        f.in.closed[hand]=false;f.in.now_ms+=10;const auto released=f.gesture.Update(f.in);
        Check(!released.strike[hand]&&released.diagnostic[hand].stage==MotionPunchStage::open&&
            released.diagnostic[hand].points==0,"duplicate release discards stroke and records open baseline");
        f.in.closed[hand]=true;const auto close=f.Step();
        Check(!close.strike[hand]&&close.diagnostic[hand].stage==MotionPunchStage::armed,
            "deliberate reclose after duplicate release starts a fresh armed path");
        for(int n=0;n<7;++n)f.Step();f.in.grip[hand].position.z-=.09F;f.Step();
        const auto fresh=f.Step();Check(fresh.strike[hand]&&
            std::abs(fresh.start[hand].z+.29F)<.0001F,"new stroke excludes canceled duplicate-release path");
    }
    for(int fault=0;fault<5;++fault){Fixture f;f.Arm();f.Step(-.09F);
        f.in.closed[0]=false;f.in.now_ms+=10;
        if(fault==0)f.in.claim_available[0]=false;
        if(fault==1)f.in.grip[0].position_valid=false;
        if(fault==2)f.in.grip[0].orientation_valid=false;
        if(fault==3)f.in.grip[0].position.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==4)f.in.now_ms+=81;
        const auto invalid=f.gesture.Update(f.in);
        Check(!invalid.strike[0]&&invalid.diagnostic[0].stage==MotionPunchStage::awaiting_release,
            "invalid duplicate release never completes stroke or confirms release");
        f.in.closed[0]=true;f.in.claim_available[0]=true;f.in.grip[0]=Tracked({.2F,1.4F,-.29F});
        for(int n=0;n<7;++n)f.Step();f.Step(-.09F);
        Check(!f.Step().strike[0],"unavailable/invalid/stale duplicate release cannot establish open baseline");
    }
    {Fixture f;f.Arm();f.Step(-.09F);
        f.in.now_ms+=10;
        const auto repeated=f.gesture.Update(f.in);
        Check(!repeated.strike[0],"later delivery of same pose cannot finish a stroke");
        Check(repeated.diagnostic[0].stage==MotionPunchStage::collecting&&
            repeated.diagnostic[0].reason==MotionPunchReason::repeated_pose&&repeated.dt_ms==10&&
            repeated.diagnostic[0].points==2,"diagnostics distinguish fresh repeat from cancellation");
        const auto finished=f.Step();
        Check(finished.strike[0],"fresh repeated pose preserves collected stroke until next pose");
        Check(finished.dt_ms==30,"next unique pose measures from original timestamp, not repeated delivery");}
    {Fixture f;f.Step();f.in.closed[0]=true;
        for(int n=0;n<7;++n){f.Step();f.in.now_ms+=10;f.gesture.Update(f.in);}
        f.Step(-.09F);Check(f.Step().strike[0],"fresh repeated poses preserve release and stable guard");}
    for(int fault=0;fault<3;++fault){Fixture f;f.Arm();f.Step(-.09F);
        if(fault==0)f.in.now_ms+=81;
        if(fault==1)--f.in.now_ms;
        if(fault==2)--f.in.sequence;
        Check(!f.gesture.Update(f.in).strike[0],"stale/backwards repeated delivery cannot finish stroke");
        for(int n=0;n<7;++n)f.Step();f.Step(-.09F);
        Check(!f.Step().strike[0],"stale/backwards repeated delivery still requires release");}
    {Fixture f;f.Arm();f.Step(-.09F);
        f.in.now_ms+=40;f.gesture.Update(f.in);f.in.now_ms+=40;f.gesture.Update(f.in);
        f.in.now_ms+=1;const auto expired=f.gesture.Update(f.in);
        Check(!expired.strike[0]&&expired.diagnostic[0].reason==MotionPunchReason::time_discontinuity,
            "duplicates cannot refresh original pose timestamp indefinitely");
        Check(!f.Step().strike[0],"cumulative duplicate expiry cancels stroke");}
    {Fixture f;f.in.closed[0]=true;auto o=f.Step();
        Check(o.diagnostic[0].stage==MotionPunchStage::awaiting_release,"held startup explains release barrier");
        f.in.closed[0]=false;o=f.Step();Check(o.diagnostic[0].stage==MotionPunchStage::open,"released fist observed");
        f.in.closed[0]=true;o=f.Step();Check(o.diagnostic[0].stage==MotionPunchStage::armed,"close immediately arms motion admission");
        for(int n=0;n<7;++n)o=f.Step();Check(o.diagnostic[0].stage==MotionPunchStage::armed,"stable guard observed");
        o=f.Step(-.09F);Check(o.diagnostic[0].stage==MotionPunchStage::collecting&&
            o.diagnostic[0].world_speed_mps>4&&o.diagnostic[0].world_travel_m>.08F,"physical trajectory observed");
        o=f.Step();Check(o.strike[0]&&o.diagnostic[0].stage==MotionPunchStage::recovery,"spent stroke observed");
        f.in.allowed=false;o=f.Step();Check(o.diagnostic[0].stage==MotionPunchStage::blocked&&
            o.diagnostic[0].reason==MotionPunchReason::not_allowed,"admission rejection observed");}
    {Fixture f;f.Arm();auto a=f.Step(-.04F);Check(!a.strike[0],"short movement");
        a=f.Step(-.05F);Check(!a.strike[0],"early threshold waits for full jab");
        Check(!f.Step(-.04F).strike[0],"outward motion collects contact reach");
        a=f.Step();Check(a.strike[0]&&!a.strike[1],"right swing emits once at extension");
        Check(a.start[0].z>a.end[0].z+.12F,"sweep includes later contact in stroke");
        for(int n=0;n<8;++n)f.Step();
        Check(!f.Step(-.09F).strike[0],"rest at extension cannot rearm");
        f.Step(.08F);f.Step(.08F);f.Step(.06F);for(int n=0;n<7;++n)f.Step();
        f.Step(-.09F);Check(f.Step().strike[0],"retracted stable fist rearms");}
    {Fixture f;f.Step();f.in.closed[1]=true;for(int n=0;n<7;++n)f.Step();
        f.in.grip[1].position.z-=.09F;f.Step();auto o=f.Step();Check(o.strike[1]&&!o.strike[0],"independent left swing");}
    {Fixture f;f.Arm();f.in.head.position.z-=.09F;
        f.in.grip[0].position.z-=.09F;f.in.grip[1].position.z-=.09F;
        Check(!f.Step().strike[0],"whole body translation is not a punch");}
    {Fixture f;f.Arm();Check(!f.Step(.09F).strike[0],"pulling toward body is not a punch");}
    for(int fault=0;fault<10;++fault){Fixture f;f.Arm();
        if(fault==0)f.in.allowed=false;
        if(fault==1)f.in.context++;
        if(fault==2)f.in.owner++;
        if(fault==3)f.in.grip[0].position_valid=false;
        if(fault==4)f.in.claim_available[0]=false;
        if(fault==5)f.in.now_ms+=120;
        if(fault==6)f.in.recentered=true;
        if(fault==7)f.in.grip[0].position.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==8)f.in.player_generation++;
        if(fault==9)f.in.input_generation++;
        Check(!f.Step(-.09F).strike[0],"loss/change cancels swing");
        f.in.allowed=true;f.in.recentered=false;f.in.claim_available[0]=true;
        f.in.grip[0]=Tracked({.2F,1.4F,-.2F});
        for(int n=0;n<7;++n)f.Step();Check(!f.Step(-.09F).strike[0],"held claim cannot resume after loss");
        f.in.closed[0]=false;f.Step();f.in.closed[0]=true;f.Step(-.09F);
        Check(f.Step().strike[0],"fresh available release after loss permits immediate close and motion");}
    {Fixture f;f.Arm();f.in.grip[0].position.z-=.09F;
        Check(!f.gesture.Update(f.in).strike[0],"duplicate pose cannot trigger");}
    for(int fault=0;fault<2;++fault){Fixture f;f.Arm();f.Step(-.09F);
        if(fault==0)f.in.claim_available[0]=false;
        if(fault==1)f.in.grip[0].position_valid=false;
        f.in.now_ms+=10;
        f.gesture.Update(f.in);
        f.in.claim_available[0]=true;f.in.grip[0].position_valid=true;f.in.closed[0]=true;
        Check(!f.Step().strike[0],"duplicate-pose loss still cancels collected stroke");
        for(int n=0;n<7;++n)f.Step();f.Step(-.09F);
        Check(!f.Step().strike[0],"held claim cannot replay after duplicate-pose interruption");}
    {Fixture f;f.Arm();Check(!f.Step(-.4F).strike[0],"tracking teleport is rejected");}
    {Fixture f;f.Arm();f.Step(-.04F);f.in.grip[0].position.x-=.04F;f.Step(-.04F);
        f.in.grip[0].position.x-=.06F;f.Step(-.02F);
        f.in.grip[0].position.x-=.05F;f.Step(.03F);
        const auto o=f.Step();Check(o.strike[0]&&o.sweep[0].count==5,"curved hook retains every physical segment");
        Check(std::abs(o.sweep[0].points[1].x-.2F)<.0001F&&
            std::abs(o.sweep[0].points[1].z+.24F)<.0001F,"prelaunch corner retained, no chord through arc interior");
        Check(!f.Step().strike[0],"curve does not repeat damage");
        f.in.grip[0]=Tracked({.125F,1.4F,-.235F});f.Step();
        f.in.grip[0]=Tracked({.2F,1.4F,-.2F});f.Step();for(int n=0;n<7;++n)f.Step();
        f.Step(-.09F);Check(f.Step().strike[0],"hook rearms after returning to guard");}
    {Fixture f;f.Arm();f.in.head.position.z+=.04F;f.Step();f.in.head.position.z+=.05F;f.Step();
        f.in.head.position.z+=.04F;f.Step();Check(!f.Step().strike[0],"head recoil with stationary fist cannot punch");}
    {Fixture f;f.Arm();f.Step(-.09F);f.in.head.position.z+=.05F;auto o=f.Step();
        Check(o.strike[0]&&std::abs(o.start[0].z+.2F)<.0001F&&std::abs(o.end[0].z+.29F)<.0001F,
            "head motion cannot translate historical physical sweep");}
    {Fixture f;f.Arm();f.in.grip[0].position.x-=.09F;f.Step();
        Check(f.Step().strike[0],"sideways forward hook does not require radial extension");}
    {Fixture f;f.Arm();f.in.grip[0].position.y+=.09F;f.Step();
        Check(f.Step().strike[0],"front uppercut keeps physical path");}
    {Fixture f;f.Arm();const auto initial=f.in.grip;
        for(int n=1;n<=4;++n){const float a=.2F*n;f.in.head.orientation={0,std::sin(a*.5F),0,std::cos(a*.5F)};
            for(int h=0;h<2;++h){const auto p=initial[h].position;
                f.in.grip[h].position={std::cos(a)*p.x+std::sin(a)*p.z,p.y,-std::sin(a)*p.x+std::cos(a)*p.z};}
            Check(!f.Step().strike[0],"shared body/head rotation does not become a hook");}
        Check(!f.Step().strike[0],"shared rotation stopping cannot hit");}
    {Fixture f;GameplayControlMapper mapper;GameplayInputState raw{};raw.active=true;
        const auto tick=[&](float dz=0.F){const auto admitted=mapper.Update(raw);
            f.in.allowed=true;f.in.closed[0]=raw.fire_right;
            ApplyMotionPunchClaims(f.in,admitted);return f.Step(dz);};
        tick();raw.fire_right=true;for(int n=0;n<7;++n)tick();
        raw.weapon_radial=true;Check(!tick(-.09F).strike[0]&&!tick().strike[0],"wheel owns closed-fist input");
        raw.weapon_radial=false;for(int n=0;n<7;++n)tick();
        Check(!tick(-.09F).strike[0]&&!tick().strike[0],"wheel close keeps held-trigger release barrier");
        raw.fire_right=false;tick(.09F);raw.fire_right=true;for(int n=0;n<7;++n)tick();
        tick(-.09F);Check(tick().strike[0],"fresh post-wheel claim can punch");}
    std::cout<<"punch_gesture: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
