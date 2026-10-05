#include "games/call_of_juarez/gameplay_ui_adapter.hpp"
#include "runtime/vr_math.hpp"
#include <cmath>
namespace cojvr::games::call_of_juarez {
namespace {
float Dot(runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
bool Finite(runtime::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool PoseValid(const runtime::Pose& p){
    const auto q=p.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return p.position_valid&&p.orientation_valid&&Finite(p.position)&&std::isfinite(norm)&&norm>.9F&&norm<1.1F;
}
}
bool CoJNoShootMatchesRay(const CoJNoShootSnapshot& native,int hand,runtime::Vec3 origin,runtime::Vec3 direction) noexcept {
    if(!native.valid||!native.warning_visible||(hand!=0&&hand!=1)||native.hand!=hand||
        (native.reason!=1&&native.reason!=3)||!std::isfinite(native.age_seconds)||native.age_seconds<0||native.age_seconds>.25F||
        !Finite(origin)||!Finite(direction)||!Finite(native.trace_start_cm)||!Finite(native.trace_end_cm))return false;
    const runtime::Vec3 delta{origin.x-native.trace_start_cm.x,origin.y-native.trace_start_cm.y,origin.z-native.trace_start_cm.z};
    const runtime::Vec3 segment{native.trace_end_cm.x-native.trace_start_cm.x,native.trace_end_cm.y-native.trace_start_cm.y,native.trace_end_cm.z-native.trace_start_cm.z};
    const float length=std::sqrt(Dot(segment,segment)),norm=std::sqrt(Dot(direction,direction));
    // Conservative stale-ray suppression, not a native target/reach decision:
    // at most 3 cm origin drift and 3 degrees angular drift from natural trace.
    return std::isfinite(length)&&std::isfinite(norm)&&length>.01F&&norm>.01F&&Dot(delta,delta)<=9.0F&&
        Dot(segment,direction)/(length*norm)>=.9986295F;
}
namespace {
runtime::Quaternion Inverse(runtime::Quaternion q){return runtime::NormalizeQuaternion({-q.x,-q.y,-q.z,q.w});}
bool DialDirection(runtime::Vec3 world,runtime::Vec3 right,runtime::Vec3 back,
    runtime::Quaternion wrist_inverse,runtime::Vec2& out){
    const auto local=runtime::RotateVector(wrist_inverse,{Dot(world,right),0,Dot(world,back)});
    const float norm=std::hypot(local.x,local.z);
    if(!std::isfinite(norm)||norm<.01F)return false;
    out={local.x/norm,-local.z/norm};return true;
}
}
void CoJWheelSelectionGate::Update(runtime::GameplayInputState& input,
    const runtime::EquipmentWheelSnapshot& inventory) noexcept {
    if(!input.active) {
        denied_mask_=0;input.equipment_select={};input.hands=input.discard_weapon=false;return;
    }
    if(!input.utility_modifier) { denied_mask_=0;return; }
    std::uint8_t requested=0;
    for(std::size_t i=0;i<6;++i)if(input.equipment_select[i])requested|=static_cast<std::uint8_t>(1U<<i);
    if(input.hands)requested|=0x40;
    if(input.discard_weapon)requested|=0x80;
    const auto available=inventory.valid?inventory.available_mask:std::uint8_t{0};
    // Observe the raw requests before filtering them. Only an actual release
    // clears a denial while the utility context remains active.
    denied_mask_&=requested;
    denied_mask_|=static_cast<std::uint8_t>(requested&~available);
    const auto allowed=static_cast<std::uint8_t>(requested&available&~denied_mask_);
    for(std::size_t i=0;i<6;++i)input.equipment_select[i]=(allowed&(1U<<i))!=0;
    input.hands=(allowed&0x40)!=0;input.discard_weapon=(allowed&0x80)!=0;
}
void BuildCoJGameplayUiOverlay(const CoJGameplayUiSnapshot& native,const runtime::GameplayInputState& input,
    const runtime::Pose& head,const runtime::Pose& hand,runtime::Vec3 right,runtime::Vec3 back,
    runtime::StereoHudTextOverlay& out) noexcept {
    out.ui={};out.compass_surface_valid=false;out.compass_head_corners={};
    out.status_surface_valid=false;out.status_head_corners={};
    if(!input.active)return;
    out.ui.wheel=native.inventory;out.ui.wheel.active=input.utility_modifier&&native.inventory.valid;
    for(int i=0;i<6;++i)if(input.equipment_select[i]){out.ui.wheel.selected=i;break;}
    if(input.hands)out.ui.wheel.selected=6;
    if(input.discard_weapon)out.ui.wheel.selected=7;
    if(!PoseValid(head)||!PoseValid(hand))return;
    const auto head_inverse=Inverse(head.orientation), wrist_inverse=Inverse(hand.orientation);
    const auto face_normal=runtime::RotateVector(hand.orientation,{0,1,0});
    const runtime::Vec3 hand_to_head{head.position.x-hand.position.x,head.position.y-hand.position.y,head.position.z-hand.position.z};
    if(Dot(face_normal,hand_to_head)<.015F||Dot(hand_to_head,hand_to_head)>2.25F)return;
    if(native.status.active&&native.status.line_count&&native.status.line_count<=native.status.lines.size()){
        const float half_width=.08F;
        const float half_height=half_width*native.status.pixel_height()/runtime::WristStatusSnapshot::pixel_width;
        for(std::size_t i=0;i<4;++i){
            const auto offset=runtime::RotateVector(hand.orientation,
                {(i&1)?half_width:-half_width,.045F,.205F+((i&2)?2*half_height:0)});
            out.status_head_corners[i]=runtime::RotateVector(head_inverse,
                {hand.position.x+offset.x-head.position.x,hand.position.y+offset.y-head.position.y,
                 hand.position.z+offset.z-head.position.z});
        }
        out.ui.status=native.status;out.status_surface_valid=true;
    }
    if(!native.compass_valid||!native.compass_visible||!Finite(right)||!Finite(back)||
        std::fabs(Dot(right,right)-1)>.02F||std::fabs(Dot(back,back)-1)>.02F||std::fabs(Dot(right,back))>.02F||
        !std::isfinite(native.map_angle_degrees)||!Finite(native.player_forward)||!Finite(native.player_left)||
        native.waypoint_count>native.waypoints.size())return;
    constexpr float radians=.017453292519943295F;
    const float angle=native.map_angle_degrees*radians;
    const auto f=native.player_forward,l=native.player_left;
    runtime::Vec3 north{l.x*std::sin(angle)+f.x*std::cos(angle),0,l.z*std::sin(angle)+f.z*std::cos(angle)};
    auto& compass=out.ui.compass;
    if(!DialDirection(north,right,back,wrist_inverse,compass.north_direction))return;
    for(std::size_t i=0;i<native.waypoint_count;++i){
        const auto& waypoint=native.waypoints[i];if(!waypoint.visible)continue;
        if(!std::isfinite(waypoint.angle_degrees)){compass={};return;}
        const float a=waypoint.angle_degrees*radians;
        const runtime::Vec3 world{f.x*std::sin(a)-l.x*std::cos(a),0,f.z*std::sin(a)-l.z*std::cos(a)};
        auto& marker=compass.markers[compass.marker_count];
        if(!DialDirection(world,right,back,wrist_inverse,marker.direction)){compass={};return;}
        marker.label=waypoint.label;++compass.marker_count;
    }
    // Compact face retains its mount center above the grip, towards the forearm (+Z).
    constexpr float half_extent=.048F, forearm_center=.11F;
    for(std::size_t i=0;i<4;++i){
        const auto offset=runtime::RotateVector(hand.orientation,
            {(i&1)?half_extent:-half_extent,.045F,forearm_center+((i&2)?half_extent:-half_extent)});
        out.compass_head_corners[i]=runtime::RotateVector(head_inverse,
            {hand.position.x+offset.x-head.position.x,hand.position.y+offset.y-head.position.y,
             hand.position.z+offset.z-head.position.z});
    }
    compass.active=true;out.compass_surface_valid=true;
}
}
