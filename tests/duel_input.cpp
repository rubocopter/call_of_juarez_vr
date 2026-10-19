#include "games/call_of_juarez/duel_input.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <iomanip>
#include <cstdlib>
#include <string_view>

using namespace cojvr::games::call_of_juarez;
int main(int argc,char** argv) {
    if(argc==8&&std::string_view(argv[1])=="--draw-sample") {
        const auto value=[&](int i){return std::strtof(argv[i],nullptr);};
        CoJDuelInputState gate;
        cojvr::runtime::GameplayInputState input;input.active=true;
        CoJDuelPadParameters pad{value(6),value(6),value(7),value(7)};
        const CoJDuelDrawParameters draw{true,value(3),value(4),value(2),value(5)};
        (void)gate.Update(input,{1,42,1},pad,1,draw);
        input.turn.y=-1;
        const auto sample=gate.Update(input,{1,42,2},pad,2,draw);
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
    return failed?1:0;
}
