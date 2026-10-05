#include "games/call_of_juarez/gameplay_ui_adapter.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
void Require(bool v,const char* s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
    CoJNoShootSnapshot no{};no.valid=no.warning_visible=true;no.hand=0;no.reason=1;no.age_seconds=.02F;no.trace_end_cm={0,0,1000};
    Require(CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"fresh native protected trace must match tracked ray");
    Require(!CoJNoShootMatchesRay(no,1,{0,0,0},{0,0,1}),"opposite-hand warning leaked");
    Require(!CoJNoShootMatchesRay(no,0,{4,0,0},{0,0,1}),"stale translated trace accepted");
    Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{.1F,0,1}),"stale rotated trace accepted");
    no.age_seconds=.26F;Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"native trace older than bound accepted");no.age_seconds=.02F;
    no.trace_end_cm={};Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"empty collision segment accepted");no.trace_end_cm={0,0,1000};
    no.warning_visible=false;Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"hidden warning exposed");
    CoJWheelSelectionGate gate;
    GameplayInputState input{};input.active=input.utility_modifier=true;
    input.equipment_select[1]=input.equipment_select[2]=input.hands=input.discard_weapon=true;
    EquipmentWheelSnapshot inventory{};inventory.valid=true;inventory.available_mask=2;
    gate.Update(input,inventory);
    Require(input.equipment_select[1]&&!input.equipment_select[2]&&!input.hands&&!input.discard_weapon,
        "unowned/denied wheel sectors must not reach native action delivery");
    input.equipment_select[1]=true;inventory.valid=false;gate.Update(input,inventory);
    Require(!input.equipment_select[1],"missing inventory must fail closed for wheel mutation");
    input.equipment_select[1]=true;inventory.valid=true;inventory.available_mask=2;
    gate.Update(input,inventory);
    Require(!input.equipment_select[1],
        "permission recovery must not execute a previously denied held wheel gesture");
    input.equipment_select[1]=false;gate.Update(input,inventory);
    input.equipment_select[1]=true;gate.Update(input,inventory);
    Require(input.equipment_select[1],"neutral then a fresh gesture must rearm a permitted sector");
    // Exercise every sector, including hands/discard and a permission loss
    // after a previously accepted selection. Requests are fresh raw samples.
    for(int sector=0;sector<8;++sector){
        CoJWheelSelectionGate held_gate;
        GameplayInputState raw{};raw.active=raw.utility_modifier=true;
        if(sector<6)raw.equipment_select[sector]=true;
        else if(sector==6)raw.hands=true;else raw.discard_weapon=true;
        const auto requested=[&](const GameplayInputState& v){return sector<6?v.equipment_select[sector]:sector==6?v.hands:v.discard_weapon;};
        inventory.valid=true;inventory.available_mask=static_cast<std::uint8_t>(1U<<sector);
        auto delivered=raw;held_gate.Update(delivered,inventory);
        Require(requested(delivered),"initially permitted gesture must be delivered");
        inventory.available_mask=0;delivered=raw;held_gate.Update(delivered,inventory);
        Require(!requested(delivered),"native permission loss must suppress a held sector");
        inventory.available_mask=static_cast<std::uint8_t>(1U<<sector);delivered=raw;held_gate.Update(delivered,inventory);
        Require(!requested(delivered),"permission recovery cannot rearm a held sector");
        delivered=raw;delivered.active=false;held_gate.Update(delivered,inventory);
        Require(!requested(delivered),"inactive native context must clear wheel intents");
        delivered=raw;held_gate.Update(delivered,inventory);
        Require(requested(delivered),"native context exit must reset denial state");
        inventory.available_mask=0;delivered=raw;held_gate.Update(delivered,inventory);
        delivered=raw;delivered.utility_modifier=false;held_gate.Update(delivered,inventory);
        Require(requested(delivered),"direct custom input outside utility keeps its native route");
        inventory.available_mask=static_cast<std::uint8_t>(1U<<sector);delivered=raw;held_gate.Update(delivered,inventory);
        Require(requested(delivered),"utility exit must reset denial state");
    }
    Pose head{},hand{};head.orientation_valid=head.position_valid=true;
    hand=head;hand.position={-.2F,-.3F,-.5F};
    CoJGameplayUiSnapshot native{};native.inventory.valid=true;native.inventory.available_mask=2;
    native.compass_valid=native.compass_visible=true;native.player_forward={0,0,-1};native.player_left={-1,0,0};
    native.waypoint_count=1;native.waypoints[0].visible=true;native.waypoints[0].angle_degrees=90;
    StereoHudTextOverlay overlay{};input.equipment_select[1]=true;
    native.status.active=true;native.status.line_count=1;native.status.lines[0].length=1;native.status.lines[0].characters[0]=u'7';
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(overlay.ui.wheel.active&&overlay.ui.wheel.selected==1&&overlay.ui.compass.active&&overlay.compass_surface_valid,
        "native owned wheel and visible compass must reach a captured surface");
    Require(overlay.ui.status.active&&overlay.status_surface_valid,"owned health/ammo missing from tracked wrist");
    const auto status_before=overlay.status_head_corners;
    Require(std::fabs(overlay.ui.compass.markers[0].direction.x)<.001F&&
        overlay.ui.compass.markers[0].direction.y>.99F,"native forward waypoint must point along the wrist dial top");
    const auto before=overlay.compass_head_corners;
    head.position.x=.1F;
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(std::fabs(overlay.compass_head_corners[0].x-before[0].x+.1F)<.001F,
        "wrist geometry must remain attached in world while head translates");
    Require(std::fabs(overlay.status_head_corners[0].x-status_before[0].x+.1F)<.001F,"status detached after head translation");
    native.compass_valid=false;
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(overlay.ui.status.active&&!overlay.ui.compass.active,"health/ammo must survive missing compass objectives");native.compass_valid=true;
    hand.orientation={0,.70710678F,0,.70710678F};
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(overlay.ui.compass.markers[0].direction.x>.99F,
        "turning wrist must rotate native world guidance on the dial");
    input.active=false;
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(!overlay.ui.wheel.active&&!overlay.ui.compass.active&&!overlay.compass_surface_valid,
        "native context loss must remove both surfaces");
    Require(!overlay.ui.status.active&&!overlay.status_surface_valid,"context loss retained status");
    input.active=true;hand.orientation.x=std::numeric_limits<float>::quiet_NaN();
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(!overlay.ui.compass.active,"invalid wrist tracking must hide compass");
    Require(!overlay.ui.status.active,"invalid wrist tracking retained status");
    hand.orientation={};hand.position={-.2F,-.3F,-.5F};
    native.status.line_count=11;
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(!overlay.ui.status.active&&!overlay.status_surface_valid,"oversized status retained geometry");
    native.status.line_count=1;hand.position={0,-2,0};
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(!overlay.ui.status.active,"far wrist status must hide");
    hand.position={-.2F,-.3F,-.5F};hand.orientation={1,0,0,0};
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(!overlay.ui.status.active,"back-facing wrist status must hide");
    std::cout<<"Native wheel filtering and tracked objective geometry passed\n";
}
