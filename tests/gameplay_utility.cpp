#include "runtime/gameplay_utility.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
void Require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int Selections(const GameplayInputState& state) {
    int count = state.hands + state.discard_weapon;
    for (bool value : state.equipment_select) count += value;
    return count;
}
int main() {
    AuxiliaryButtonGesture create;
    (void)create.Update(true,false,0);
    Require(!create.Update(true,true,100).objectives,"Create must not dispatch objectives on press");
    auto tap=create.Update(true,false,400);
    Require(tap.objectives&&!tap.recenter,"short Create must emit only objectives on release");
    (void)create.Update(true,true,1000);
    auto early=create.Update(true,true,1799);
    Require(!early.objectives&&!early.recenter,"Create hold fired before 800 ms");
    auto hold=create.Update(true,true,1800);
    Require(hold.recenter&&!hold.objectives,"Create hold must emit only recenter");
    auto held=create.Update(true,true,2500);
    Require(!held.recenter&&!held.objectives,"Create hold repeated");
    tap=create.Update(true,false,2600);
    Require(!tap.objectives&&!tap.recenter,"Create hold also emitted tap");
    (void)create.Update(true,true,3000);(void)create.Update(false,false,3100);
    Require(!create.Update(true,true,5000).recenter,"context loss resumed interrupted Create hold");
    tap=create.Update(true,false,5100);
    Require(!tap.objectives&&!tap.recenter,"unavailable sample counted as Create release");
    (void)create.Update(true,true,5200);
    tap=create.Update(true,false,5250);Require(tap.objectives,"fresh Create gesture did not recover");
    (void)create.Update(true,true,6000);
    tap=create.Update(true,false,6800);
    Require(tap.recenter&&!tap.objectives,"unsampled hold threshold must still choose only recenter");
    (void)create.Update(true,true,7000);
    Require(!create.Update(true,true,6900).recenter,"regressing clock must cancel Create gesture");
    (void)create.Update(true,false,8000);
    (void)create.Update(true,true,8100,false); // starts inside menu/wheel
    tap=create.Update(true,false,8200,true);
    Require(!tap.objectives,"Create begun in menu/wheel leaked Objectives after closing context");
    (void)create.Update(true,true,8300,true);
    (void)create.Update(true,true,8350,false);
    tap=create.Update(true,false,8400,true);
    Require(!tap.objectives,"Create traversing higher context leaked tap after close");
    (void)create.Update(true,true,8500,false);
    hold=create.Update(true,true,9300,false);
    Require(hold.recenter&&!hold.objectives,"owned menu must retain global Recenter hold");
    AuxiliaryObjectivesPending pending_objectives;
    AuxiliaryButtonGesture pending_create;
    (void)pending_create.Update(true,false,0);
    (void)pending_create.Update(true,true,100);
    tap=pending_create.Update(true,false,200);
    pending_objectives.Observe(true,true,tap.objectives);
    pending_objectives.Observe(true,true,false); // presenter polls before game consumption
    Require(pending_objectives.Consume(),"valid pending Create tap was lost between polls");
    Require(!pending_objectives.Consume(),"pending Create tap was consumed twice");
    // The presenter shares the gesture's availability gate with pending taps:
    // invalid HMD orientation/position or unavailable Create must cancel both.
    for(const bool recover_before_consume:{false,true}) {
        const std::uint64_t tick=recover_before_consume?2000:1000;
        (void)pending_create.Update(true,false,tick);
        (void)pending_create.Update(true,true,tick+100);
        tap=pending_create.Update(true,false,tick+200);
        Require(tap.objectives,"pending-loss fixture did not produce a Create tap");
        pending_objectives.Observe(true,true,tap.objectives);
        (void)pending_create.Update(false,false,tick+300);
        pending_objectives.Observe(false,true,false);
        if(recover_before_consume) {
            (void)pending_create.Update(true,false,tick+400);
            pending_objectives.Observe(true,true,false);
        }
        Require(!pending_objectives.Consume(),"HMD/Create loss retained pending Objectives");
        pending_objectives.Observe(true,true,false);
        Require(!pending_objectives.Consume(),"recovery replayed a cancelled Create tap");
    }
    pending_objectives.Observe(true,true,true);
    pending_objectives.Observe(true,false,false); // menu or radial owns input
    pending_objectives.Observe(true,true,false);
    Require(!pending_objectives.Consume(),"higher-context cancellation replayed pending Objectives");
    pending_objectives.Observe(false,true,true);
    Require(!pending_objectives.Consume(),"invalid availability accepted a new pending tap");
    GameplayControlMapper mapper;
    GameplayInputState raw{}; raw.active = true;
    (void)mapper.Update(raw);
    Require(mapper.Update(raw,true,true).objectives,"retained Create tap did not reach gameplay mapper");
    Require(!mapper.Update(raw,false,true).objectives,"menu propagated Create tap to gameplay");
    (void)mapper.Update(raw);
    raw.weapon_radial=true;
    Require(!mapper.Update(raw,true,true).objectives,"open radial propagated Create tap");
    raw.weapon_radial=false;
    Require(!mapper.Update(raw,true,true).objectives,"closing radial propagated a consumed Create tap");
    (void)mapper.Update(raw);
    raw.weapon_radial = true; raw.turn = {0,1};
    auto out = mapper.Update(raw);
    Require(Selections(out) == 0, "radial must highlight without dispatch while Triangle is held");
    raw.reload = raw.jump = raw.kick = raw.interact = raw.run = raw.crouch = true;
    raw.fire_left = raw.fire_right = true;
    out = mapper.Update(raw);
    Require(!out.alternate_fire && !out.hands && !out.discard_weapon && !out.objectives &&
        !out.logs && !out.weapon_previous && !out.focus && !out.walk && Selections(out)==0,
        "Triangle must not derive any former secondary action");
    Require(!out.fire_left && !out.fire_right && !out.reload && !out.jump && !out.kick &&
        !out.run && !out.crouch && !out.interact && out.turn.x==0 && out.turn.y==0,
        "radial must consume conflicting gameplay buttons and turn");
    raw.weapon_radial = false;
    out = mapper.Update(raw);
    Require(Selections(out)==1 && out.equipment_select[2], "up must confirm only SelectRifle on release");
    Require(!out.fire_left && !out.fire_right && out.turn.x==0 && out.turn.y==0,
        "radial close must preserve held-button and turn release barriers");
    Require(Selections(mapper.Update(raw))==0, "radial release must dispatch exactly once");
    raw = {}; raw.active = true; (void)mapper.Update(raw);
    raw.fire_left = raw.fire_right = true;
    out = mapper.Update(raw);
    Require(out.fire_left && out.fire_right, "native LMB/RMB must work simultaneously");
    Require(mapper.Update(raw).fire_left && mapper.Update(raw).fire_right, "both triggers must remain held");
    raw.fire_left = false;
    Require(!mapper.Update(raw).fire_left && mapper.Update(raw).fire_right, "triggers must release independently");

    // Literal expectations independently encode the requested clockwise layout.
    constexpr Vec2 sticks[]{{0,1},{.71F,.71F},{1,0},{.71F,-.71F},{0,-1},{-.71F,-.71F},{-1,0},{-.71F,.71F}};
    constexpr int channels[]{2,5,1,3,6,7,0,4};
    for (int sector=0; sector<8; ++sector) {
        GameplayControlMapper wheel;
        raw={};raw.active=true;(void)wheel.Update(raw);
        raw.weapon_radial=true;raw.turn=sticks[sector];
        Require(Selections(wheel.Update(raw))==0,"traversing a sector must not select or throw");
        raw.turn=sticks[(sector+1)%8];(void)wheel.Update(raw);
        raw.turn=sticks[sector];
        Require(Selections(wheel.Update(raw))==0,"changing highlight must not emit commands");
        raw.weapon_radial=false;out=wheel.Update(raw);
        Require(Selections(out)==1,"each confirmed sector must emit exactly one command");
        Require(channels[sector]<6 ? out.equipment_select[channels[sector]] :
            channels[sector]==6 ? out.hands : out.discard_weapon,"wrong spatial radial action");
        Require(Selections(wheel.Update(raw))==0,"confirmed sector was replayed");
        raw.turn={1,0};Require(wheel.Update(raw).turn.x==0,"deflected close caused phantom snap");
        raw.turn={};(void)wheel.Update(raw);
        raw.turn={1,0};Require(wheel.Update(raw).turn.x==1,"center must rearm snap");
    }
    raw={};raw.active=true;(void)mapper.Update(raw);
    raw.weapon_radial=true;raw.turn={-1,-1};(void)mapper.Update(raw);
    raw.turn={};(void)mapper.Update(raw);raw.weapon_radial=false;
    Require(Selections(mapper.Update(raw))==0,"releasing Triangle at center must cancel Throw");
    raw.weapon_radial=true;raw.turn={std::numeric_limits<float>::quiet_NaN(),1};
    (void)mapper.Update(raw);raw.weapon_radial=false;
    Require(Selections(mapper.Update(raw))==0,"invalid stick must cancel selection");
    raw={};raw.active=true;(void)mapper.Update(raw);
    raw.focus=true;Require(mapper.Update(raw).focus,"R3 must enter Focus directly");
    raw.focus=false;Require(mapper.Update(raw).focus,"Focus must survive stick-button release");
    raw.focus=true;Require(!mapper.Update(raw).focus,"second R3 press must exit Focus");
    Require(!mapper.Update(raw).focus,"held R3 must not oscillate Focus");
    raw.focus=false;raw.crouch=true;
    Require(mapper.Update(raw).crouch,"L3 must enter crouch");
    raw.crouch=false;Require(mapper.Update(raw).crouch,"crouch must persist without holding L3");
    raw.crouch=true;Require(!mapper.Update(raw).crouch,"second L3 must stand");
    raw.crouch=false;raw.alternate_fire=true;raw.jump=true;
    out=mapper.Update(raw);Require(out.alternate_fire&&out.jump,"Circle/Cross must retain gameplay actions");
    Require(!mapper.Update(raw,false).active,"native menu must consume all gameplay");
    out=mapper.Update(raw);Require(!out.alternate_fire&&!out.jump&&!out.focus&&!out.crouch,
        "menu exit must clear latches and require shared buttons to release");
    raw.alternate_fire=raw.jump=false;(void)mapper.Update(raw);
    raw.alternate_fire=raw.jump=true;out=mapper.Update(raw);
    Require(out.alternate_fire&&out.jump,"fresh gameplay presses after menu must recover");
    raw.input_context_generation=1;raw.fire_left=true;
    Require(!mapper.Update(raw).fire_left,"missed context loss must still enforce release");
    raw.fire_left=false;raw.digital_available&=~1U;(void)mapper.Update(raw);
    raw.fire_left=true;raw.digital_available|=1U;
    Require(!mapper.Update(raw).fire_left,"unavailable action must not count as released");
    raw.fire_left=false;(void)mapper.Update(raw);raw.fire_left=true;
    Require(mapper.Update(raw).fire_left,"available released trigger must rearm");
    raw={};raw.active=true;(void)mapper.Update(raw);
    raw.move={.2F,.3F};out=mapper.Update(raw);
    Require(out.move.x==.2F&&out.move.y==.3F&&!out.walk,"native analog magnitude must remain intact");
    // Every old chord input, including optional/custom actions, is consumed
    // rather than acquiring a second Triangle meaning.
    for (const auto member:kGameplayDigitalMembers) {
        GameplayControlMapper chord;
        raw={};raw.active=true;(void)chord.Update(raw);
        raw.weapon_radial=true;raw.*member=true;out=chord.Update(raw);
        for (const auto action:kGameplayDigitalMembers)
            Require(!(out.*action),"Triangle plus a button generated an action");
        Require(Selections(out)==0,"Triangle plus a button selected equipment");
    }
    GameplayControlMapper hysteresis;
    raw={};raw.active=true;(void)hysteresis.Update(raw);
    raw.weapon_radial=true;raw.turn={0,.60F};
    Require(hysteresis.Update(raw).radial_highlight==-1,"radial engaged below its existing deliberate threshold");
    raw.turn={0,.66F};Require(hysteresis.Update(raw).radial_highlight==0,"up highlight missing");
    raw.turn={0,.60F};Require(hysteresis.Update(raw).radial_highlight==0,"radial hysteresis lost highlight");
    const auto angle=[](float degrees){const auto radians=degrees*.01745329252F;
        return Vec2{std::sin(radians),std::cos(radians)};};
    raw.turn=angle(24);Require(hysteresis.Update(raw).radial_highlight==0,"highlight jittered at sector border");
    raw.turn=angle(28);Require(hysteresis.Update(raw).radial_highlight==1,"deliberate sector crossing did not update highlight");
    raw.turn=angle(22);Require(hysteresis.Update(raw).radial_highlight==1,"reverse boundary jittered");
    raw.turn=angle(17);Require(hysteresis.Update(raw).radial_highlight==0,"deliberate reverse crossing missing");
    raw.turn={0,.54F};Require(hysteresis.Update(raw).radial_highlight==-1,"inner radial deadzone did not cancel");
    raw.weapon_radial=false;Require(Selections(hysteresis.Update(raw))==0,"inner-zone release dispatched selection");
    raw.weapon_radial=true;raw.turn={-1,-1};(void)hysteresis.Update(raw);
    raw.radial_available=false;raw.weapon_radial=false;
    Require(Selections(hysteresis.Update(raw))==0,"unavailable Triangle was treated as confirmation");
    std::cout << "Sense radial, direct actions, latches and context barriers passed\n";
}
