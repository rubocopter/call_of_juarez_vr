#include "games/call_of_juarez/duel_input.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <iomanip>
#include <cstdlib>
#include <string_view>

using namespace cojvr::games::call_of_juarez;
namespace {
cojvr::runtime::Pose HandAim(bool lowered) {
    cojvr::runtime::Pose pose;pose.position_valid=pose.orientation_valid=true;
    if(lowered)pose.orientation={-.5F,0,0,.8660254F}; // physical pitch -60 degrees
    return pose;
}
}
int main(int argc,char** argv) {
    if(argc==8&&std::string_view(argv[1])=="--draw-sample") {
        const auto value=[&](int i){return std::strtof(argv[i],nullptr);};
        CoJDuelInputState gate;
        cojvr::runtime::GameplayInputState input;input.active=true;
        CoJDuelPadParameters pad{value(6),value(6),value(7),value(7)};
        const CoJDuelDrawParameters draw{true,value(3),value(4),value(2),value(5)};
        (void)gate.UpdatePhysical(input,{1,42,1},pad,1,draw,HandAim(true),1);
        input.turn.y=-1;
        const auto sample=gate.UpdatePhysical(input,{1,42,2},pad,2,draw,HandAim(true),2);
        if(!sample.dispatch)return 2;
        std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)<<sample.y<<'\n';return 0;
    }
    int failed=0;
    const auto check=[&](bool value,const char* reason){if(!value){std::cerr<<reason<<'\n';++failed;}};
    CoJDuelInputState gate;
    cojvr::runtime::GameplayInputState input; input.active=true;input.input_context_generation=7;
    CoJDuelInputOwner owner{1,42,1.0F};
    CoJDuelPadParameters pad{10,10,.4F,.4F};
    input.turn={1,-1};
    check(!gate.Update(input,owner,pad,10).dispatch,"held input entered a new duel");
    input.turn={};owner.game_time=2;
    check(!gate.Update(input,owner,pad,20).dispatch,"center emitted a mouse event");
    input.turn={1,-1};owner.game_time=3;
    auto axes=gate.Update(input,owner,pad,30);
    check(axes.dispatch && axes.x==-10 && axes.y==-10,"native signed draw/aim input is missing");
    check(!gate.Update(input,owner,pad,30).dispatch,"duplicate monotonic sample emitted twice");
    check(!gate.Update(input,owner,pad,31).dispatch,"same native update emitted twice");
    owner.game_time=4;
    check(gate.Update(input,owner,pad,40).dispatch,"equal nonzero input was skipped on the next native update");
    input.turn={-.65F,.65F};owner.game_time=5;
    axes=gate.Update(input,owner,pad,50);
    check(axes.dispatch&&std::fabs(axes.x-5)<.001F&&std::fabs(axes.y-5)<.001F,"native pad shaping or up/left signs changed");
    input.active=false;owner.game_time=6;
    check(!gate.Update(input,owner,pad,60).dispatch,"inactive input was forwarded");
    input.active=true;owner.game_time=7;
    check(!gate.Update(input,owner,pad,70).dispatch,"focus return replayed held input");
    input.turn={};owner.game_time=8;(void)gate.Update(input,owner,pad,80);
    input.turn={1,-1};input.input_context_generation++;owner.game_time=9;
    check(!gate.Update(input,owner,pad,90).dispatch,"new input context replayed held input");
    input.turn={};owner.game_time=10;(void)gate.Update(input,owner,pad,100);
    input.turn={1,-1};owner.action_identity++;owner.game_time=11;
    check(!gate.Update(input,owner,pad,110).dispatch,"new duel action replayed held input");
    input.turn={};owner.game_time=12;(void)gate.Update(input,owner,pad,120);
    input.turn={1,-1};owner.player_generation++;owner.game_time=13;
    check(!gate.Update(input,owner,pad,130).dispatch,"new player replayed held input");
    input.turn={};owner.game_time=14;(void)gate.Update(input,owner,pad,140);
    input.turn={1,-1};input.turn_available=false;owner.game_time=15;
    check(!gate.Update(input,owner,pad,150).dispatch,"unavailable axes reached native input");
    input.turn_available=true;owner.game_time=16;
    check(!gate.Update(input,owner,pad,160).dispatch,"tracking return replayed held input");
    input.turn={};owner.game_time=17;(void)gate.Update(input,owner,pad,170);
    input.turn={1,-1};owner.game_time=16;
    check(!gate.Update(input,owner,pad,180).dispatch,"rewound native clock retained arming");
    input.turn={};owner.game_time=18;(void)gate.Update(input,owner,pad,190);
    input.turn={1,-1};owner.game_time=19;
    check(!gate.Update(input,owner,pad,180).dispatch,"reversed monotonic sample was delivered");
    input.turn={};owner.game_time=20;(void)gate.Update(input,owner,pad,200);
    input.turn={std::numeric_limits<float>::quiet_NaN(),0};owner.game_time=21;
    check(!gate.Update(input,owner,pad,210).dispatch,"nonfinite axes were delivered");
    input.turn={1,-1};owner.game_time=22;
    check(!gate.Update(input,owner,pad,220).dispatch,"invalid input retained arming");
    input.turn={};owner.game_time=23;(void)gate.Update(input,owner,pad,230);
    pad.sensitivity_y=std::numeric_limits<float>::infinity();input.turn={1,-1};owner.game_time=24;
    check(!gate.Update(input,owner,pad,240).dispatch,"nonfinite native gain was delivered");
    pad={20,30,.8F,1.2F};input.turn={};owner.game_time=25;(void)gate.Update(input,owner,pad,250);
    input.turn={.65F,-.65F};owner.game_time=26;
    axes=gate.Update(input,owner,pad,260);
    check(axes.dispatch&&std::fabs(axes.x+10)<.001F&&std::fabs(axes.y+15)<.001F,"native instance sensitivities were ignored");
    gate.Invalidate();owner.game_time=27;
    check(!gate.Update(input,owner,pad,270).dispatch,"explicit owner loss replayed held input");
    CoJDuelInputState drawing;
    input={};input.active=true;
    const CoJDuelDrawParameters draw{true,.275F,1,1.0F/144.0F,.1F};
    (void)drawing.Update(input,{1,42,1},{10,10,.4F,.4F},1,draw);
    input.turn.y=-1;
    axes=drawing.Update(input,{1,42,2},{10,10,.4F,.4F},2,draw);
    check(axes.dispatch&&((axes.y*.275F)*(1.0F/144.0F))<-.1F,
        "deliberate empty-hand draw sample did not cross the original native threshold at 144Hz");
    // Actual production entry: a pre-aimed hand plus down/up stick cannot draw.
    CoJDuelInputState physical;
    auto tracked=HandAim(false);
    auto physical_draw=draw;
    input.turn={};
    (void)physical.UpdatePhysical(input,{1,42,1},pad,1000,physical_draw,tracked,1);
    input.turn.y=-1;
    check(!physical.UpdatePhysical(input,{1,42,2},pad,2000,physical_draw,tracked,2).dispatch,
        "pre-aimed hand could still draw using only the stick");
    input.turn.y=1;
    check(!physical.UpdatePhysical(input,{1,42,3},pad,3000,physical_draw,tracked,3).dispatch,
        "empty-handed upward stick bypassed the physical draw");
    tracked=HandAim(true);input.turn.y=-1;
    check(!physical.UpdatePhysical(input,{1,42,4},pad,4000,physical_draw,tracked,4).dispatch,
        "lowering while holding down replayed the old draw request");
    input.turn={};
    (void)physical.UpdatePhysical(input,{1,42,5},pad,5000,physical_draw,tracked,5);
    input.turn.y=-1;
    check(physical.UpdatePhysical(input,{1,42,6},pad,6000,physical_draw,tracked,6).y<0,
        "fresh lowered-hand draw did not reach the native mapping");
    physical_draw.hands_empty=false;input.turn.y=1;
    check(!physical.UpdatePhysical(input,{1,42,7},pad,7000,physical_draw,tracked,7).dispatch,
        "stick raised the native gun while the tracked hand stayed lowered");
    tracked=HandAim(false);
    check(physical.UpdatePhysical(input,{1,42,8},pad,8000,physical_draw,tracked,8).y>0,
        "physical raise did not retain native upward input");
    check(!physical.UpdatePhysical(input,{1,42,9},pad,9000,physical_draw,tracked,8).dispatch,
        "a retained tracked pose was replayed at a new game tick");
    physical_draw.hands_empty=true;tracked=HandAim(true);input.turn={};
    (void)physical.UpdatePhysical(input,{1,42,10},pad,10000,physical_draw,tracked,10);
    tracked.orientation_valid=false;input.turn.y=-1;
    check(!physical.UpdatePhysical(input,{1,42,11},pad,11000,physical_draw,tracked,11).dispatch,
        "missing hand tracking retained draw authority");
    tracked=HandAim(true);
    check(!physical.UpdatePhysical(input,{1,42,12},pad,12000,physical_draw,tracked,12).dispatch,
        "tracking recovery replayed held down input");
    input.turn={};
    (void)physical.UpdatePhysical(input,{1,42,13},pad,13000,physical_draw,tracked,13);
    input.turn.y=-1;
    check(!physical.UpdatePhysical(input,{1,43,14},pad,14000,physical_draw,tracked,14).dispatch,
        "new native duel action inherited lowered-hand authority");
    input.turn={};
    (void)physical.UpdatePhysical(input,{1,43,15},pad,15000,physical_draw,tracked,15);
    input.turn.y=-1;
    check(!physical.UpdatePhysical(input,{1,43,16},pad,200000,physical_draw,tracked,16).dispatch,
        "stale hand stream retained physical draw authority");
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),-2.F,2.F}){
        CoJDuelInputState malformed;input.turn={};
        (void)malformed.UpdatePhysical(input,{1,42,1},pad,1000,draw,HandAim(true),1);
        input.turn={1,invalid};
        check(!malformed.UpdatePhysical(input,{1,42,2},pad,2000,draw,HandAim(false),2).dispatch,
            "posture filtering laundered a malformed raw axis into native input");
    }
    return failed?1:0;
}
