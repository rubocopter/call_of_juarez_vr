#include "games/call_of_juarez/holster_guides.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr;
using namespace games::call_of_juarez;
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    CoJVirtualHolsterSample s;s.allowed=s.native_ready=true;s.raw.active=true;
    s.head={{0,1.7F,0},{},true,true};
    s.grips={runtime::Pose{{.22F,1,0},{},true,true},runtime::Pose{{-.22F,1,0},{},true,true}};
    s.inventory.valid=true;s.inventory.owned_mask=s.inventory.available_mask=0x27;
    runtime::StereoHudTextOverlay out;
    Check(!BuildCoJHolsterGuides(s,false,out)&&out.holster_guides.count==0,"disabled guide must be empty");
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.count==4,"owned slots must have bounded guides");
    Check(out.holster_guides.zones[0].state==1&&out.holster_guides.zones[1].state==1,
        "hand inside a hip zone must highlight its guide");
    for(unsigned z=0;z<4;++z){
        const auto& guide=out.holster_guides.zones[z];
        runtime::Vec3 centre{};for(auto c:guide.head_corners){centre.x+=c.x*.25F;centre.y+=c.y*.25F;centre.z+=c.z*.25F;}
        const auto& zone=kCoJVirtualHolsterZones[z];
        Check(std::abs(centre.x-zone.offset.x)<.0001F&&std::abs(centre.y-zone.offset.y)<.0001F&&
            std::abs(centre.z-zone.offset.z)<.0001F,"guide centre diverged from active selection zone");
    }
    s.inventory.available_mask=0;
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.zones[0].state==2,"denied slot must not look selectable");
    s.inventory.available_mask=0x27;s.raw.fire_left=true;
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.zones[0].state==2,
        "trigger-owned hand advertised a selectable holster");
    s.raw.fire_left=false;s.inventory.available_mask=0;
    s.inventory.available_mask=0x27;s.raw.interact=true;s.grips[1].position={-.6F,1.4F,-.5F};
    CoJVirtualHolsterState dispatch;auto mapped=s.raw;
    (void)dispatch.Update(s,mapped);s.selection_blocked=dispatch.selection_blocked();
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.zones[1].state==2,
        "ordinary other-hand Action advertised a selectable right holster");
    s.raw.interact=false;s.grips[1].position={-.22F,1,0};
    mapped=s.raw;mapped.equipment_select[2]=true;
    (void)dispatch.Update(s,mapped);s.selection_blocked=dispatch.selection_blocked();
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.zones[1].state==2,
        "mapped equipment conflict advertised a selectable holster");
    mapped=s.raw;(void)dispatch.Update(s,mapped);s.selection_blocked=dispatch.selection_blocked();
    s.inventory.owned_mask=0;
    Check(!BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.count==0,"unowned weapons must not advertise zones");
    s.inventory.owned_mask=s.inventory.available_mask=0x27;
    s.head.orientation={-.70710678F,0,0,.70710678F};
    Check(BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.zones[0].head_corners[0].z<-.3F,
        "hip guides must remain visible when looking down");
    s.allowed=false;
    Check(!BuildCoJHolsterGuides(s,true,out)&&out.holster_guides.count==0,"lost gameplay owner retained guide geometry");
    s.allowed=true;s.head.orientation_valid=false;
    Check(!BuildCoJHolsterGuides(s,true,out),"invalid head tracking created a guide");
    std::cout<<"captured holster guides follow selection geometry and optional ownership\n";
}
