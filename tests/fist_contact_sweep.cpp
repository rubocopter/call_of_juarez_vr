#include "games/call_of_juarez/fist_contact_sweep.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
int main() {
    using P=cojvr::runtime::Vec3;
    const std::array<P,4> previous{{{0,0,0},{0,2,0},{0,4,0},{0,6,0}}};
    auto current=previous;
    for(auto& point:current)point.z-=1;
    auto sweep=BuildCoJFistContactSweep(previous,current,true);
    if(!sweep.valid||sweep.count!=7||sweep.segments[0][0].z!=0||sweep.segments[0][1].z!=-1) return 1;
    // Independent traces preserve only current adjacent poses, with no chord
    // connecting one finger's past position to another finger's future position.
    for(unsigned i=0;i<4;++i)
        if(sweep.segments[i][0].y!=previous[i].y||sweep.segments[i][1].y!=current[i].y) return 2;
    sweep=BuildCoJFistContactSweep(current,current,true);
    if(!sweep.valid||sweep.count!=3) return 3; // Static patch still reports clear/contact.
    sweep=BuildCoJFistContactSweep(previous,current,false);
    if(!sweep.valid||sweep.count!=3) return 4; // First pose cannot replay missing movement.
    auto invalid=current;invalid[2].x=std::numeric_limits<float>::quiet_NaN();
    if(BuildCoJFistContactSweep(previous,invalid,true).valid) return 5;
    invalid=current;invalid[2].z-=30;
    if(BuildCoJFistContactSweep(previous,invalid,true).valid) return 6;
    // A frame-position jump must be discarded, not clamped into a long hit ray.
    invalid=current;for(auto& p:invalid)p.x+=40;
    if(BuildCoJFistContactSweep(previous,invalid,true).valid) return 7;
    const CoJFistContactIdentity prior{10,1,2,3,4,0,true};
    auto next=prior;next.pose=11;
    if(!CoJFistContactContinuous(prior,next,10))return 8;
    next.native_yaw=30;
    // Body yaw can change while the compensated reference remains the same.
    if(!CoJFistContactContinuous(prior,next,10))return 9;
    next.native_yaw=std::numeric_limits<float>::quiet_NaN();
    if(CoJFistContactContinuous(prior,next,10))return 10;
    next=prior;next.pose=11;
    // A 30-degree reference change at 30 cm radius moves only 15.5 cm.
    // Body compensation alone is unchanged, yet this chord is untravelled.
    next.reference_basis={P{.8660254F,0,.5F},P{0,1,0},P{-.5F,0,.8660254F}};
    if(CoJFistContactContinuous(prior,next,10))return 11;
    auto within_new=next;within_new.pose=12;
    if(!CoJFistContactContinuous(next,within_new,11))return 12;
    within_new.reference_basis[0].x+=std::numeric_limits<float>::epsilon();
    if(!CoJFistContactContinuous(next,within_new,11))return 13;
    within_new.reference_basis[1].y=std::numeric_limits<float>::infinity();
    if(CoJFistContactContinuous(next,within_new,11))return 14;
    std::cout<<"current measured fist rails, static patch and discontinuity rejection passed\n";
}
