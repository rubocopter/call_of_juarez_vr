#include "games/call_of_juarez/gameplay_ui_adapter.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
void Require(bool v,const char* s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
    // A confirmed release is outside the open-wheel context. It must still
    // enforce inventory ownership before reaching the native consumer.
    CoJWheelSelectionGate release_gate;
    GameplayInputState confirm{};confirm.active=confirm.radial_confirmed=true;
    confirm.radial_highlight=5;confirm.discard_weapon=true;
    EquipmentWheelSnapshot empty_inventory{};
    release_gate.Update(confirm,empty_inventory);
    Require(!confirm.discard_weapon,"closing wheel bypassed throw permission checks");
    CoJNoShootSnapshot no{};no.valid=no.warning_visible=true;no.hand=0;no.reason=1;no.age_seconds=.02F;no.trace_end_cm={0,0,1000};
    Require(CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"fresh native protected trace must match tracked ray");
    Require(AssessCoJNoShootRay(no,0,{0,0,0},{0,0,1}).reason==CoJNoShootRayReason::matched,"assessment accepts coherent warning");
    Require(AssessCoJNoShootRay(no,1,{0,0,0},{0,0,1}).reason==CoJNoShootRayReason::hand_mismatch,"assessment identifies wrong hand");
    const auto shifted=AssessCoJNoShootRay(no,0,{4,0,0},{0,0,1});
    Require(shifted.reason==CoJNoShootRayReason::origin_drift && std::abs(shifted.origin_drift_cm-4.F)<.001F,"assessment measures origin drift in game centimetres");
    Require(AssessCoJNoShootRay(no,0,{0,0,0},{.1F,0,1}).reason==CoJNoShootRayReason::direction_drift,"assessment identifies angular drift");
    auto old=no;old.age_seconds=.26F;
    Require(AssessCoJNoShootRay(old,0,{0,0,0},{0,0,1}).reason==CoJNoShootRayReason::stale_trace,"assessment identifies stale native trace");
    Require(!CoJNoShootMatchesRay(no,1,{0,0,0},{0,0,1}),"opposite-hand warning leaked");
    Require(!CoJNoShootMatchesRay(no,0,{4,0,0},{0,0,1}),"stale translated trace accepted");
    Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{.1F,0,1}),"stale rotated trace accepted");
    no.age_seconds=.26F;Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"native trace older than bound accepted");no.age_seconds=.02F;
    no.trace_end_cm={};Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"empty collision segment accepted");no.trace_end_cm={0,0,1000};
    no.warning_visible=false;Require(!CoJNoShootMatchesRay(no,0,{0,0,0},{0,0,1}),"hidden warning exposed");
    constexpr int channels[]{2,5,1,3,6,7,0,4};
    for(int sector=0;sector<8;++sector){
        CoJWheelSelectionGate gate;
        GameplayInputState highlight{};highlight.active=highlight.weapon_radial=true;
        highlight.radial_highlight=sector;
        EquipmentWheelSnapshot inventory{};inventory.valid=true;
        const auto requested=[&](const GameplayInputState& v){
            return channels[sector]<6?v.equipment_select[channels[sector]]:
                channels[sector]==6?v.hands:v.discard_weapon;
        };
        const auto confirmation=[&]{
            auto v=highlight;v.weapon_radial=false;v.radial_confirmed=true;
            if(channels[sector]<6)v.equipment_select[channels[sector]]=true;
            else if(channels[sector]==6)v.hands=true;else v.discard_weapon=true;
            return v;
        };
        inventory.available_mask=static_cast<std::uint8_t>(1U<<channels[sector]);
        gate.Update(highlight,inventory);
        Require(!requested(highlight),"highlight dispatched an equipment command");
        auto delivered=confirmation();gate.Update(delivered,inventory);
        Require(requested(delivered),"permitted confirmed sector was not delivered");
        // A sector denied while highlighted stays denied until that gesture
        // changes, even when the permission recovers immediately before close.
        inventory.available_mask=0;gate.Update(highlight,inventory);
        inventory.available_mask=static_cast<std::uint8_t>(1U<<channels[sector]);
        delivered=confirmation();gate.Update(delivered,inventory);
        Require(!requested(delivered),"recovered permission rearmed a denied gesture");
        highlight.radial_highlight=-1;gate.Update(highlight,inventory);
        highlight.radial_highlight=sector;gate.Update(highlight,inventory);
        delivered=confirmation();gate.Update(delivered,inventory);
        Require(requested(delivered),"center and a fresh highlight must rearm sector");
        delivered=confirmation();delivered.active=false;gate.Update(delivered,inventory);
        Require(!requested(delivered),"context loss retained an equipment command");
        delivered=confirmation();inventory.valid=false;gate.Update(delivered,inventory);
        Require(!requested(delivered),"unobserved inventory did not fail closed");
        delivered=confirmation();delivered.radial_confirmed=false;gate.Update(delivered,inventory);
        Require(requested(delivered),"direct custom action must retain native eligibility");
    }
    GameplayInputState input{};input.active=input.weapon_radial=true;input.radial_highlight=2;
    Pose head{},hand{};head.orientation_valid=head.position_valid=true;
    hand=head;hand.position={-.2F,-.3F,-.5F};
    CoJGameplayUiSnapshot native{};native.inventory.valid=true;native.inventory.available_mask=2;
    native.compass_valid=native.compass_visible=true;native.player_forward={0,0,-1};native.player_left={-1,0,0};
    native.waypoint_count=1;native.waypoints[0].visible=true;native.waypoints[0].angle_degrees=90;
    StereoHudTextOverlay overlay{};
    native.status.active=true;native.status.line_count=1;native.status.lines[0].length=1;native.status.lines[0].characters[0]=u'7';
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(overlay.ui.wheel.active&&overlay.ui.wheel.selected==2&&overlay.ui.compass.active&&overlay.compass_surface_valid,
        "native owned wheel and visible compass must reach a captured surface");
    Require(overlay.ui.status.active&&overlay.status_surface_valid,"owned health/ammo missing from tracked wrist");
    constexpr char16_t channel_labels[]{u'L',u'R',u'G',u'D',u'T',u'B',u'H',u'X'};
    constexpr char16_t sector_labels[]{u'G',u'B',u'R',u'D',u'H',u'X',u'L',u'T'};
    for(int channel=0;channel<8;++channel){
        native.inventory.labels[channel].length=1;
        native.inventory.labels[channel].characters[0]=channel_labels[channel];
    }
    for(int channel=0;channel<8;++channel){
        native.inventory.owned_mask=native.inventory.available_mask=static_cast<std::uint8_t>(1U<<channel);
        BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
        for(int sector=0;sector<8;++sector){
            Require(overlay.ui.wheel.labels[sector].view()==std::u16string_view(&sector_labels[sector],1),
                "radial UI label order disagrees with confirmed action order");
            Require(((overlay.ui.wheel.available_mask>>sector)&1)==(channels[sector]==channel),
                "radial UI permission mask disagrees with logical action");
            Require(((overlay.ui.wheel.owned_mask>>sector)&1)==(channels[sector]==channel),
                "radial UI ownership mask disagrees with logical action");
        }
    }
    native.inventory.valid=false;
    BuildCoJGameplayUiOverlay(native,input,head,hand,{1,0,0},{0,0,1},overlay);
    Require(overlay.ui.wheel.valid&&overlay.ui.wheel.active&&overlay.ui.wheel.available_mask==0,
        "unobserved inventory must show immediately as disabled wheel");
    native.inventory.valid=true;
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
