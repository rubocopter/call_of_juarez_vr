#include "games/call_of_juarez/long_gun_grip.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
namespace {
void Check(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(Vec3 a,Vec3 b){return std::abs(a.x-b.x)<.002F&&std::abs(a.y-b.y)<.002F&&std::abs(a.z-b.z)<.002F;}
CoJLongGunSupportSnapshot Native(){
    CoJLongGunSupportSnapshot n{};n.valid=true;n.weapon_class="WeaponRifleWinchester";
    n.player_id=0x81234567;n.weapon_id=0xf1234567;n.holder_element=4;n.support_element=10;
    n.states[0]={1,1,1,1};n.two_handed[0]={true,true,true};
    n.root={{100,200,300},{0,1,0},{0,0,1},true};
    n.holder={{110,200,300},{0,1,0},{0,0,1},true};
    n.support={{130,210,260},{0,1,0},{0,0,1},true};
    Check(BuildCoJReloadLocalFrame(n.root,n.support,n.support_in_weapon),"fixture local");return n;
}
CoJLongGunGripGeometry Geometry(){
    CoJLongGunGripGeometry g{};g.native=Native();g.dominant_socket_cm={110,200,300};
    g.muzzle_cm={115,207,220};g.muzzle_direction={0,0,-1};
    g.dominant_grip_cm={400,500,600};g.direction={0,0,-1};g.up={0,1,0};
    g.support_grip_cm={420,510,560};return g;
}
void GeometryAndSinglePlan(){
    auto g=Geometry();auto out=BuildCoJLongGunGripFrame(g);
    Check(out.valid,"measured support plan missing");
    Check(Near(out.mapped_support_frame.position,g.support_grip_cm),"measured support point missed");
    Check(Near(out.plan.muzzle_origin,{405,507,520}),"muzzle offset replaced by support ray");
    Check(out.contact_distance_m<.00001F,"ordinary contact map incorrect");
    // A different support direction rotates the WHOLE map about the right socket.
    g.support_grip_cm={440,510,620};out=BuildCoJLongGunGripFrame(g);
    Check(out.valid&&Near(out.mapped_support_frame.position,g.support_grip_cm),"rotated support missed");
    auto replay=BuildTrackedWeaponFrame(g.native.root,g.dominant_socket_cm,g.muzzle_cm,
        g.muzzle_direction,g.dominant_grip_cm,out.plan.muzzle_direction,out.plan.root.up);
    Check(replay.valid&&Near(replay.root.position,out.plan.root.position)&&
        Near(replay.root.up,out.plan.root.up)&&Near(replay.root.forward,out.plan.root.forward)&&
        Near(replay.muzzle_origin,out.plan.muzzle_origin),"existing capture cannot reproduce solver map");
    const auto anchor=TransformWeaponElementFrame(g.native.root,out.plan.root,
        {g.dominant_socket_cm,{0,1,0},{0,0,1},true});
    Check(anchor.valid&&Near(anchor.position,g.dominant_grip_cm),"dominant socket drifted");
    g.support_grip_cm={480,520,640};out=BuildCoJLongGunGripFrame(g);
    Check(out.valid&&out.support_error_m>.4F,"unequal spacing hidden");
    Check(std::abs(out.solution.source_span_m-std::sqrt(.21F))<.0001F,"cm/metre conversion incorrect");
}
struct Fixture {
    CoJLongGunGripState state;CoJLongGunGripGeometry g=Geometry();CoJLongGunGripInput in{};
    Fixture(){in.player_generation=3;in.context=5;in.controls.input_generation=7;
        in.controls.enabled=in.controls.eligible=in.controls.tracking_valid=true;
        in.controls.grip_available=in.controls.support_trigger_available=true;
        in.controls.acquisition_radius_m=.1F;in.controls.sequence=1;
        in.controls.sample_time_ms=in.controls.now_ms=1000;}
    CoJLongGunGripOutput Step(){++in.controls.sequence;in.controls.now_ms+=10;
        in.controls.sample_time_ms=in.controls.now_ms;return state.Update(in,g);}
    CoJLongGunGripOutput Attach(){(void)Step();in.controls.grip_held=true;auto o=Step();
        Check(o.claim.acquired&&o.attached&&o.frame.valid,"fresh idle support claim failed");return o;}
};
void OwnershipAndRelease(){
    Fixture f;auto o=f.Attach();
    GameplayInputState mapped{};mapped.active=mapped.interact=mapped.fire_left=mapped.fire_right=true;
    FilterCoJLongGunGripInput(o.claim,mapped);
    Check(!mapped.interact&&!mapped.fire_left&&mapped.fire_right&&!(mapped.digital_available&65),"support controls leaked");
    // Ordinary native cycling changes the live support pose, never the acquired contact.
    f.g.native.firing=true;f.g.native.states[0]={30,31,38,1};
    f.g.native.support.position={999,888,777};
    Check(BuildCoJReloadLocalFrame(f.g.native.root,f.g.native.support,f.g.native.support_in_weapon),"cycle fixture");
    o=f.Step();Check(o.attached&&Near(o.frame.mapped_support_frame.position,{420,510,560}),"cycle reseeded support contact");
    f.in.controls.support_trigger_held=true;f.g.native.valid=false;o=f.Step();
    Check(o.claim.cancelled&&!o.attached&&o.claim.consume_grip&&o.claim.consume_support_trigger,"owner loss leaked hold");
    f.g.native=Native();o=f.Step();Check(!o.attached,"held owner recovery attached");
    f.in.controls.grip_held=false;o=f.Step();Check(o.claim.consume_grip&&o.claim.consume_support_trigger,"release leaked");
    o=f.Step();Check(!o.claim.consume_grip&&o.claim.consume_support_trigger,"L2 barrier lost");
    f.in.controls.support_trigger_available=false;f.in.controls.support_trigger_held=false;
    Check(f.Step().claim.consume_support_trigger,"missing L2 counted as release");
    f.in.controls.support_trigger_available=true;Check(f.Step().claim.consume_support_trigger,"L2 release leaked");
    Check(!f.Step().claim.consume_support_trigger,"L2 failed to recover");
}
void RejectionsAndRefresh(){
    for(int why=0;why<14;++why){Fixture f;(void)f.Attach();
        if(why==0)++f.g.native.weapon_id;if(why==1)++f.g.native.player_id;
        if(why==2)++f.in.player_generation;if(why==3)++f.in.context;
        if(why==4)f.g.native.weapon_class="WeaponRifleWinchesterScope";
        if(why==5)f.g.native.states[0][1]=21;if(why==6)f.g.native.two_handed[1][0]=true;
        if(why==7)++f.g.native.support_element;if(why==8)f.in.controls.recentered=true;
        if(why==9)f.g.native.root.up={0,2,0};if(why==10)f.in.controls.eligible=false;
        if(why==11)f.g.support_grip_cm.x=std::numeric_limits<float>::quiet_NaN();
        if(why==12)f.g.support_grip_cm={1200,1000,600};
        if(why==13)f.g.support_grip_cm={400.1F,500,600};
        auto o=f.Step();Check(!o.attached&&o.claim.cancelled&&o.claim.consume_grip,"changed/invalid owner retained");}
    Fixture f;f.g.native.firing=true;f.g.native.states[0][0]=30;
    (void)f.Step();f.in.controls.grip_held=true;Check(!f.Step().attached,"fire acquired support");
    Fixture h;(void)h.Attach();Check(h.state.Refresh(h.in.player_generation,h.in.context,h.g).valid,"same owner refresh rejected");
    auto distant=h.g;distant.support_grip_cm.x+=500;
    Check(!h.state.Refresh(h.in.player_generation,h.in.context,distant).valid,"refresh bypassed separation bounds");
    ++h.g.native.weapon_id;Check(!h.state.Refresh(h.in.player_generation,h.in.context,h.g).valid,"new owner got old map");
    Fixture q;(void)q.Attach();q.state.Cancel();Check(!q.state.attached(),"cancel kept attachment");
    auto o=q.Step();Check(o.claim.consume_grip&&!o.attached,"cancel forgot physical hold");
}
void RepeatedRenderSamples(){
    Fixture f;CoJLongGunGripSampleClock clock;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    (void)f.state.Update(f.in,f.g);
    ++f.in.controls.sequence;f.in.controls.now_ms+=10;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    f.in.controls.grip_held=true;Check(f.state.Update(f.in,f.g).attached,"dated fresh grip failed");
    f.in.controls.now_ms+=10;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    Check(f.state.Update(f.in,f.g).attached,"duplicate render cancelled attachment");
    f.in.controls.now_ms+=101;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    Check(!f.state.Update(f.in,f.g).attached,"duplicate render renewed stale tracking");
    Check(clock.Observe(8,1,2000)==2000,"replacement source failed to restart dating");
}
}
int main(){GeometryAndSinglePlan();OwnershipAndRelease();RejectionsAndRefresh();RepeatedRenderSamples();
    std::cout<<"longgun measured geometry, single-map replay, native cycle/ownership and physical releases passed\n";}
