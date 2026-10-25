#include "games/call_of_juarez/virtual_holster.hpp"
#include "games/call_of_juarez/interaction_selection.hpp"
#include "runtime/gameplay_utility.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr;
using namespace games::call_of_juarez;
namespace {
void Check(bool value,const char* reason) {if(!value){std::cerr<<reason<<'\n';std::exit(1);}}
runtime::Pose Pose(runtime::Vec3 p) {return {p,{},true,true};}
template<class Sample> void NativeReady(Sample& s,bool ready){
    if constexpr(requires{s.native_ready=ready;})s.native_ready=ready;
}
void WorldSelection(CoJVirtualHolsterSample& s,const CoJInteractionSelection& selection){
    s.interaction=selection;
}
struct Fixture {
    CoJVirtualHolsterState gate;
    CoJVirtualHolsterSample s;
    runtime::GameplayInputState out;
    Fixture() {
        s.raw.active=true;s.raw.input_context_generation=3;
        s.player=7;s.context=9;s.allowed=true;
        NativeReady(s,true);
        s.head=Pose({0,1.7F,0});s.grips={Pose({.22F,1,0}),Pose({-.22F,1,0})};
        s.inventory.valid=true;s.inventory.owned_mask=s.inventory.available_mask=0x27;
    }
    int Step(){++s.sequence;s.now_us+=10000;out=s.raw;return gate.Update(s,out);}
    void Release(){s.raw.kick=s.raw.interact=false;Step();}
};
}
int main(){
    for(const auto& zone:kCoJVirtualHolsterZones){
        Fixture f;const auto layout=BuildCoJVirtualHolsterLayout(f.s.head);
        const auto index=static_cast<unsigned>(&zone-kCoJVirtualHolsterZones.data());
        for(int axis=0;axis<3;++axis)for(float sign:{-1.F,1.F})for(float fraction:{.99F,1.01F}){
            auto hand=Pose(layout.centres[index]);
            const float distance=zone.radius*sign*fraction;
            if(axis==0)hand.position.x+=distance;
            if(axis==1)hand.position.y+=distance;
            if(axis==2)hand.position.z+=distance;
            const bool inside=fraction<1&&(!zone.shoulder||hand.position.z-f.s.head.position.z>=.05F);
            Check(CoJVirtualHolsterZoneForHand(f.s.head,hand,zone.hand)==(inside?zone.channel:-1),
                "holster boundary does not match its three-dimensional radius");
            Check(CoJVirtualHolsterZoneForHand(f.s.head,hand,1-zone.hand)!=zone.channel,
                "opposite hand acquired a holster zone");
        }
    }
    // Ordinary 15 cm reaches near the hips retain their original Action.
    // Shoulder zones intentionally stay generous behind the head.
    for(int channel:{0,1}){
        Fixture f;const int hand=channel==0||channel==5?1:0;
        const auto layout=BuildCoJVirtualHolsterLayout(f.s.head);
        const int index=channel==0?0:channel==1?1:channel==2?2:3;
        f.s.grips[hand]=Pose(layout.centres[index]);
        f.s.grips[hand].position.x+=(channel<2?.15F:.18F)*(hand==0?1:-1);
        f.Step();if(hand==0)f.s.raw.kick=true;else f.s.raw.interact=true;
        Check(f.Step()==-1,"overbroad holster radius intercepted a world reach");
        Check(hand==0?f.out.kick:f.out.interact,"world reach lost its ordinary grip");
    }
    for(int channel:{2,5}){
        Fixture f;const int hand=channel==5?1:0;
        const auto layout=BuildCoJVirtualHolsterLayout(f.s.head);const int index=channel==2?2:3;
        Check(kCoJVirtualHolsterZones[index].radius>=.24F,"shoulder zone was shrunk with the hip zones");
        f.s.grips[hand]=Pose(layout.centres[index]);f.s.grips[hand].position.y+=.20F;
        CoJInteractionSelection selection{};selection.valid=true;selection.active_id=19;WorldSelection(f.s,selection);
        f.Step();if(hand==0)f.s.raw.kick=true;else f.s.raw.interact=true;
        Check(f.Step()==channel,"front world selection blocked the generous behind-shoulder gesture");
        Check(!f.out.kick&&!f.out.interact,"shoulder equipment leaked into Action/Kick");
    }
    // Cached native Action selection wins even at a holster centre. Clearing
    // the selection while held must not convert that Action into equipment.
    for(bool executing:{false,true}){
        Fixture f;CoJInteractionSelection selection{};selection.valid=true;
        if(executing)selection.executing_id=17;else selection.active_id=19;
        WorldSelection(f.s,selection);f.Step();f.s.raw.interact=true;
        Check(f.Step()==-1&&f.out.interact,"native world Action was consumed by a holster");
        WorldSelection(f.s,{});
        Check(f.Step()==-1&&f.out.interact,"lost world selection retargeted a held Action");
        f.Release();f.s.raw.interact=true;Check(f.Step()==0,"fresh release did not restore left holster");
    }
    {
        Fixture f;f.Step();f.s.raw.interact=true;Check(f.Step()==0,"claim fixture failed");
        CoJInteractionSelection selection{};selection.valid=true;selection.active_id=19;
        WorldSelection(f.s,selection);f.s.grips[1]=Pose({-1,1.7F,-1});
        Check(f.Step()==-1&&!f.out.interact,"world selection stole a previously claimed hold");
        f.Release();f.s.grips[1]=Pose({-.22F,1,0});f.s.raw.interact=true;
        Check(f.Step()==-1&&f.out.interact,"released holster hold did not yield to world Action");
    }
    for(int fault=0;fault<3;++fault){
        Fixture f;CoJInteractionSelection selection{};selection.active_id=19;
        if(fault==1){selection.valid=true;selection.active_id=0;}
        if(fault==2){selection.valid=true;selection.disabled_valid=true;selection.detection_disabled=true;}
        WorldSelection(f.s,selection);f.Step();f.s.raw.interact=true;
        Check(f.Step()==0,"invalid, empty or disabled native selection changed holster policy");
    }
    {
        Fixture f;CoJInteractionSelection selection{};selection.valid=true;selection.active_id=19;
        WorldSelection(f.s,selection);f.Step();f.s.raw.kick=true;
        Check(f.Step()==-1&&!f.out.kick,"world selection allowed equipment or a denied-zone kick");
        f.s.grips[0]=Pose({1,1.7F,-1});WorldSelection(f.s,{});
        Check(f.Step()==-1&&!f.out.kick,"world conflict changed a claimed hold into Kick");
    }
    {
        Fixture f;CoJInteractionSelection selection{};selection.valid=true;selection.active_id=19;
        WorldSelection(f.s,selection);f.Step();f.s.raw.interact=true;f.s.raw.reload=true;
        Check(f.Step()==-1&&!f.out.interact,"world priority bypassed a conflicting reload claim");
        f.s.raw.reload=false;WorldSelection(f.s,{});f.s.grips[1]=Pose({-1,1.7F,-1});
        Check(f.Step()==-1&&!f.out.interact,"conflict recovery leaked a claimed Action");
    }
    {
        Fixture f;CoJInteractionSelection selection{};selection.valid=true;selection.executing_id=17;
        WorldSelection(f.s,selection);f.Step();NativeReady(f.s,false);f.s.raw.interact=true;
        Check(f.Step()==-1&&f.out.interact,"busy/carrying native world owner lost Action");
    }
    for(int channel:{0,1,2,5}){
        Fixture f;Check(f.Step()==-1,"neutral acquisition selected a weapon");
        if(channel==0||channel==5)f.s.raw.interact=true;else f.s.raw.kick=true;
        if(channel==2)f.s.grips[0]=Pose({.22F,1.62F,.20F});
        if(channel==5)f.s.grips[1]=Pose({-.22F,1.62F,.20F});
        Check(f.Step()==channel&&f.out.equipment_select[channel],"zone did not request its native slot");
        Check(!f.out.interact&&!f.out.kick&&!f.out.run,"holster press leaked Action/Kick/Gallop");
        Check(f.Step()==-1,"held grip repeated selection");
        f.s.grips={Pose({1,1.7F,-1}),Pose({-1,1.7F,-1})};
        Check(f.Step()==-1&&!f.out.interact&&!f.out.kick,"claimed hold changed meaning outside zone");
        f.Release();f.s.grips={Pose({.22F,1,0}),Pose({-.22F,1,0})};
        if(channel==2)f.s.grips[0]=Pose({.22F,1.62F,.20F});
        if(channel==5)f.s.grips[1]=Pose({-.22F,1.62F,.20F});
        if(channel==0||channel==5)f.s.raw.interact=true;else f.s.raw.kick=true;
        Check(f.Step()==channel,"fresh grip did not issue native toggle again");
    }
    {
        Fixture f;f.s.grips[0]=Pose({.4F,1.6F,-.6F});f.Step();f.s.raw.kick=true;
        Check(f.Step()==-1&&f.out.kick,"ordinary Kick outside zones was lost");
        f.s.grips[0]=Pose({.22F,1,0});
        Check(f.Step()==-1,"moving an already held grip into a zone equipped a weapon");
    }
    for(int fault=0;fault<9;++fault){
        Fixture f;f.Step();f.s.raw.kick=true;Check(f.Step()==1,"fault fixture did not claim");
        switch(fault){case 0:f.s.allowed=false;break;case 1:f.s.player++;break;
        case 2:f.s.context++;break;case 3:f.s.raw.input_context_generation++;break;
        case 4:f.s.grips[0].position_valid=false;break;case 5:f.s.head.orientation_valid=false;break;
        case 6:f.s.now_us+=200000;break;case 7:f.s.raw.digital_available&=~(1U<<9);break;
        case 8:f.s.head.position.x=std::numeric_limits<float>::quiet_NaN();break;}
        Check(f.Step()==-1&&!f.out.kick,"fault replayed a claimed action");
        f.s.allowed=true;f.s.grips[0]=Pose({.22F,1,0});f.s.head=Pose({0,1.7F,0});
        f.s.raw.digital_available=0xffffffffU;
        Check(f.Step()==-1&&!f.out.kick,"held recovery re-equipped or kicked");
        f.Release();f.s.raw.kick=true;Check(f.Step()==1,"available release did not recover");
    }
    for(int conflict=0;conflict<5;++conflict){
        Fixture f;f.Step();f.s.raw.kick=true;
        switch(conflict){case 0:f.s.raw.weapon_radial=true;break;case 1:f.s.raw.reload=true;break;
        case 2:f.s.raw.fire_right=true;break;case 3:f.s.raw.fire_left=true;break;
        case 4:f.s.raw.equipment_select[5]=true;break;}
        Check(f.Step()==-1,"conflicting action allowed holster selection");
        Check(!f.out.kick&&!f.out.run,"conflicting holster-zone grip leaked a kick");
        f.s.grips[0]=Pose({1,1.7F,-1});
        f.s.raw.weapon_radial=f.s.raw.reload=f.s.raw.fire_right=f.s.raw.fire_left=false;
        f.s.raw.equipment_select={};Check(f.Step()==-1,"cleared conflict replayed held grip");
        Check(!f.out.kick,"denied holster hold changed into Kick after leaving the zone");
    }
    {
        Fixture f;f.s.raw.kick=true;Check(f.Step()==-1,"held startup grip selected a weapon");
        f.Release();f.s.inventory.available_mask=0;f.s.raw.kick=true;
        Check(f.Step()==-1&&!f.out.kick,"denied zone leaked a kick");
        f.s.inventory.available_mask=7;Check(f.Step()==-1,"permission recovery replayed denial");
    }
    {
        Fixture f;f.Step();NativeReady(f.s,false);f.s.raw.kick=true;
        Check(f.Step()==-1&&!f.out.kick,"busy native holster attempt leaked selection or Kick");
        NativeReady(f.s,true);Check(f.Step()==-1,"native readiness recovery replayed the held press");
    }
    {
        Fixture f;f.Step();f.s.raw.kick=true;
        f.out=f.s.raw;Check(f.gate.Update(f.s,f.out)==-1,"duplicate pose selected a weapon");
        Check(!f.out.kick,"duplicate-pose holster attempt leaked a kick");
        Check(f.Step()==-1,"duplicate pose button was deferred to a new frame");
    }
    {
        Fixture f;f.Step();f.s.raw.interact=f.s.raw.kick=true;
        Check(f.Step()==1&&!f.out.interact&&!f.out.kick,"simultaneous grips were not consumed/serialized");
        Check(f.Step()==-1,"losing simultaneous selection was queued");
    }
    {
        Fixture f;f.s.grips[1]=Pose({-.4F,1.5F,-.5F});f.Step();
        f.s.raw.interact=f.s.raw.kick=true;
        Check(f.Step()==-1&&f.out.interact&&!f.out.kick,
            "holster selection conflicted with the other hand's ordinary Action");
    }
    {
        Fixture f;f.s.head=Pose({3,1.7F,4});f.s.head.orientation={0,.70710678F,0,.70710678F};
        // +90 yaw rotates tracking +X to -Z; use the same head-relative zone.
        f.s.grips[0]=Pose({3,1,3.78F});f.Step();f.s.raw.kick=true;
        Check(f.Step()==1,"translated/rotated head-relative hip failed");
    }
    {
        Fixture f;f.s.head.orientation={-.70710678F,0,0,.70710678F};
        f.Step();f.s.raw.kick=true;
        Check(f.Step()==1,"looking straight down moved the hip zone");
    }
    {
        Fixture f;runtime::GameplayControlMapper mapper;
        auto mapped=[&]{++f.s.sequence;f.s.now_us+=10000;f.out=mapper.Update(f.s.raw);
            return f.gate.Update(f.s,f.out);};
        mapped();f.s.raw.weapon_radial=true;mapped();f.s.raw.kick=true;
        Check(mapped()==-1,"wheel leaked a holster action");
        f.s.raw.weapon_radial=false;Check(mapped()==-1,"wheel-close grip re-equipped");
        f.s.raw.kick=false;mapped();f.s.raw.kick=true;
        Check(mapped()==1,"mapper release barrier prevented fresh holster use");
    }
    std::cout<<"virtual hip/shoulder slots, one-shot claims and recovery passed\n";
}
