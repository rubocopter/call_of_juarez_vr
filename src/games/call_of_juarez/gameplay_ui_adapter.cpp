#include "games/call_of_juarez/gameplay_ui_adapter.hpp"
#include "runtime/vr_math.hpp"
#include "runtime/equipment_actions.hpp"
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
const char* CoJNoShootRayReasonName(CoJNoShootRayReason reason) noexcept {
    switch(reason) {
    case CoJNoShootRayReason::matched: return "matched";
    case CoJNoShootRayReason::unavailable: return "unavailable";
    case CoJNoShootRayReason::hidden: return "hidden";
    case CoJNoShootRayReason::hand_mismatch: return "hand_mismatch";
    case CoJNoShootRayReason::unsupported_reason: return "unsupported_reason";
    case CoJNoShootRayReason::stale_trace: return "stale_trace";
    case CoJNoShootRayReason::invalid_geometry: return "invalid_geometry";
    case CoJNoShootRayReason::origin_drift: return "origin_drift";
    case CoJNoShootRayReason::direction_drift: return "direction_drift";
    }
    return "unknown";
}
CoJNoShootRayAssessment AssessCoJNoShootRay(const CoJNoShootSnapshot& native,int hand,
    runtime::Vec3 origin,runtime::Vec3 direction) noexcept {
    CoJNoShootRayAssessment result{};
    const auto reject=[&](CoJNoShootRayReason reason) { result.reason=reason;return result; };
    if(!native.valid)return reject(CoJNoShootRayReason::unavailable);
    if(!native.warning_visible)return reject(CoJNoShootRayReason::hidden);
    if((hand!=0&&hand!=1)||native.hand!=hand)return reject(CoJNoShootRayReason::hand_mismatch);
    if(native.reason!=1&&native.reason!=3)return reject(CoJNoShootRayReason::unsupported_reason);
    if(!std::isfinite(native.age_seconds)||native.age_seconds<0||native.age_seconds>.25F)
        return reject(CoJNoShootRayReason::stale_trace);
    if(!Finite(origin)||!Finite(direction)||!Finite(native.trace_start_cm)||!Finite(native.trace_end_cm))
        return reject(CoJNoShootRayReason::invalid_geometry);
    const runtime::Vec3 delta{origin.x-native.trace_start_cm.x,origin.y-native.trace_start_cm.y,origin.z-native.trace_start_cm.z};
    const runtime::Vec3 segment{native.trace_end_cm.x-native.trace_start_cm.x,native.trace_end_cm.y-native.trace_start_cm.y,native.trace_end_cm.z-native.trace_start_cm.z};
    const float length=std::sqrt(Dot(segment,segment)),norm=std::sqrt(Dot(direction,direction));
    const float drift_squared=Dot(delta,delta);
    if(!std::isfinite(length)||!std::isfinite(norm)||!std::isfinite(drift_squared)||length<=.01F||norm<=.01F)
        return reject(CoJNoShootRayReason::invalid_geometry);
    result.origin_drift_cm=std::sqrt(drift_squared);
    result.direction_cosine=Dot(segment,direction)/(length*norm);
    if(!std::isfinite(result.direction_cosine))return reject(CoJNoShootRayReason::invalid_geometry);
    // Preserve the demonstrated 3 cm / 3 degree stale-ray suppression.
    if(drift_squared>9.0F)return reject(CoJNoShootRayReason::origin_drift);
    if(result.direction_cosine<.9986295F)return reject(CoJNoShootRayReason::direction_drift);
    return reject(CoJNoShootRayReason::matched);
}
bool CoJNoShootMatchesRay(const CoJNoShootSnapshot& native,int hand,runtime::Vec3 origin,runtime::Vec3 direction) noexcept {
    return AssessCoJNoShootRay(native,hand,origin,direction).reason==CoJNoShootRayReason::matched;
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
    if(!input.weapon_radial && !input.radial_confirmed) { denied_mask_=0;return; }
    std::uint8_t requested=0;
    if(input.radial_highlight>=0 && input.radial_highlight<8)
        requested=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(
            runtime::kWeaponRadialActions[input.radial_highlight]));
    const auto available=inventory.valid?inventory.available_mask:std::uint8_t{0};
    // Observe highlight without dispatch. Permission recovery cannot authorize
    // the same denied gesture; center or another sector starts a fresh gesture.
    denied_mask_&=requested;
    denied_mask_|=static_cast<std::uint8_t>(requested&~available);
    const auto allowed=input.radial_confirmed
        ? static_cast<std::uint8_t>(requested&available&~denied_mask_) : std::uint8_t{0};
    for(std::size_t i=0;i<6;++i)input.equipment_select[i]=(allowed&(1U<<i))!=0;
    input.hands=(allowed&0x40)!=0;input.discard_weapon=(allowed&0x80)!=0;
}
void BuildCoJGameplayUiOverlay(const CoJGameplayUiSnapshot& native,const runtime::GameplayInputState& input,
    const runtime::Pose& head,const runtime::Pose& hand,runtime::Vec3 right,runtime::Vec3 back,
    runtime::StereoHudTextOverlay& out) noexcept {
    out.ui={};out.compass_surface_valid=false;out.compass_head_corners={};
    out.status_surface_valid=false;out.status_head_corners={};
    out.mission_timer={};out.mission_notices={};out.threats={};
    if(!input.active)return;
    if(PoseValid(head)){
        out.mission_timer=native.mission_timer;out.mission_notices=native.mission_notices;
        if(native.threat_count<=native.threats.size()&&Finite(right)&&Finite(back)&&
            std::fabs(Dot(right,right)-1)<.02F&&std::fabs(Dot(back,back)-1)<.02F&&std::fabs(Dot(right,back))<.02F){
            for(std::size_t i=0;i<native.threat_count;++i){
                const auto& source=native.threats[i];
                if(!std::isfinite(source.world_angle_degrees)||!std::isfinite(source.alpha)||source.alpha<=0||source.alpha>1)continue;
                // PlayerBeing.AddIndicatedDamage/Direction encode the native
                // vector against Vector.Backward (-Z), signed by its X. Native
                // HUD subtracts atan2(camera_back.z,camera_back.x)+90 before
                // placing the icon at (sin(relative), -cos(relative)). Camera
                // "forward" is the view's BACK axis: the matching source vector
                // is the opposite of (sin(a),0,-cos(a)), not that vector itself.
                const float a=source.world_angle_degrees*.017453292519943295F;
                const runtime::Vec3 world{-std::sin(a),0,std::cos(a)};
                const auto local=runtime::RotateVector(Inverse(head.orientation),{Dot(world,right),0,Dot(world,back)});
                const float n=std::hypot(local.x,local.z);if(!std::isfinite(n)||n<.01F)continue;
                auto& marker=out.threats.markers[out.threats.count++];
                marker.direction={local.x/n,-local.z/n};marker.alpha=source.alpha;marker.damage=source.damage;
            }
        }
    }
    // Inventory is stored in semantic channel order; presentation is clockwise
    // spatial order. Keep both labels and permissions on the same action map.
    constexpr std::u16string_view fallback[]{u"Left pistol",u"Right pistol",u"Long weapon",
        u"Dynamite",u"Bible / whip",u"Bow",u"Hands",u"Throw weapon"};
    out.ui.wheel.valid=true;out.ui.wheel.active=input.weapon_radial;
    out.ui.wheel.selected=input.radial_highlight;
    for(std::size_t sector=0;sector<runtime::kWeaponRadialActions.size();++sector){
        const auto channel=static_cast<std::size_t>(runtime::kWeaponRadialActions[sector]);
        auto& label=out.ui.wheel.labels[sector];
        if(native.inventory.valid)label=native.inventory.labels[channel];
        else{
            const auto text=fallback[channel];label.length=static_cast<std::uint32_t>(text.size());
            std::copy(text.begin(),text.end(),label.characters.begin());
        }
        if(native.inventory.valid && (native.inventory.owned_mask&(1U<<channel)))
            out.ui.wheel.owned_mask|=static_cast<std::uint8_t>(1U<<sector);
        if(native.inventory.valid && (native.inventory.available_mask&(1U<<channel)))
            out.ui.wheel.available_mask|=static_cast<std::uint8_t>(1U<<sector);
    }
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
