#include <cstdlib>
#include <cmath>
#include <iostream>
#include "runtime/turn_preference.hpp"
using namespace cojvr::runtime;
void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    GameplayTurnState state;GameplayInputState input{};input.active=input.turn_available=true;
    Check(state.Update(input,1000000,TurnMode::smooth)==0,"baseline rotated");
    input.turn.x=1;Check(std::fabs(state.Update(input,1010000,TurnMode::smooth)-.9F)<.0001F,"smooth preference lost proportional timing");
    Check(state.Update(input,1020000,TurnMode::snap)==0,"switch to snap replayed held stick");
    Check(state.Update(input,1030000,TurnMode::snap)==0,"snap inherited old mode authority");
    input.turn={};Check(state.Update(input,1040000,TurnMode::snap)==0,"neutral snap baseline rotated");
    input.turn.x=1;Check(state.Update(input,1050000,TurnMode::snap)==45,"snap preference did not rotate right once");
    Check(state.Update(input,1050000,TurnMode::snap)==0,"duplicate snap sample replayed");
    Check(state.Update(input,1060000,TurnMode::snap)==0,"held snap repeated");
    input.turn={};(void)state.Update(input,1070000,TurnMode::snap);
    input.turn.x=-1;Check(state.Update(input,1080000,TurnMode::snap)==-45,"snap direction inverted");
    input.turn_available=false;Check(state.Update(input,1090000,TurnMode::snap)==0,"unavailable stick rotated");
    input.turn_available=true;Check(state.Update(input,1100000,TurnMode::snap)==0,"held reacquisition snapped");
    input.turn={};(void)state.Update(input,1110000,TurnMode::snap);
    input.turn.x=1;input.input_context_generation++;
    Check(state.Update(input,1120000,TurnMode::snap)==0,"new owner inherited snap authority");
    input.turn={};(void)state.Update(input,1130000,TurnMode::snap);
    input.turn.x=1;input.weapon_radial=true;Check(state.Update(input,1140000,TurnMode::snap)==0,"wheel triggered snap");
    input.weapon_radial=false;Check(state.Update(input,1150000,TurnMode::snap)==0,"wheel exit triggered held snap");
    input.turn={};(void)state.Update(input,1160000,TurnMode::snap);
    input.turn.x=1;Check(state.Update(input,1300000,TurnMode::snap)==0,"stale input triggered snap");
    input.turn={};(void)state.Update(input,1310000,TurnMode::snap);
    input.turn.x=1;Check(state.Update(input,1300000,TurnMode::snap)==0,"regressing clock triggered snap");
    Check(state.Update(input,1320000,TurnMode::smooth)==0,"switch to smooth jumped");
    Check(state.Update(input,1330000,TurnMode::smooth)==0,"smooth resumed held mode-switch stick");
    input.turn={};(void)state.Update(input,1340000,TurnMode::smooth);
    input.turn.x=1;Check(state.Update(input,1350000,TurnMode::smooth)>.89F,"smooth failed to recover");
    Check(state.Update(input,1360000,static_cast<TurnMode>(99))==0,"invalid preference rotated");
    std::cout<<"turn_preference: both modes and interrupted/mode-change neutral recovery passed\n";
}
