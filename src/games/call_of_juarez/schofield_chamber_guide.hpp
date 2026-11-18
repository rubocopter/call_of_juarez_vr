#pragma once

#include "games/call_of_juarez/loading_mouth_guide.hpp"
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include <numbers>
#include <string_view>

namespace cojvr::games::call_of_juarez {
inline constexpr double kCoJSchofieldRearFaceCm=1.8852;
namespace schofield_chamber_guide_detail {
using winchester_loading_detail::Dot;
using winchester_loading_detail::Vector;

inline Vector Difference(Vector a, Vector b) noexcept {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}

inline bool Waiting(const CoJReloadTraceSnapshot& native,
    const CoJReloadGeometrySnapshot& geometry) noexcept {
    return native.valid && native.probe_valid && native.probe_status==2 && native.reloading &&
        native.weapon_class && std::string_view(native.weapon_class)=="WeaponPistolSchofield" &&
        native.kind==CoJManualReloadKind::clip && native.index_count==1 &&
        native.selected_index==0 && native.capacities==std::array<int,2>{6,0} &&
        native.loaded==std::array<int,2>{geometry.schofield_loaded,0} &&
        native.ammo_readback==geometry.schofield_loaded &&
        native.drum_phase==geometry.native_drum_phase_degrees;
}

inline bool RearView(Vector to_mouth, Vector inward) noexcept {
    // Keep the depthless compositor marker inside a 45-degree rear cone.
    const double axial=Dot(to_mouth,inward);
    return axial>0 && axial*axial>=.5*Dot(to_mouth,to_mouth);
}
}

// The caller supplies synchronous post-overlay geometry and the current native
// wait read from the same capture. Set frame.allowed only while the captured
// player and context still own publication, and frame.restored after restoration.
// The rear-face markers depict the aggregate count for the exact base model.
inline bool AppendCoJSchofieldChamberGuides(const CoJLoadingMouthGuideFrame& frame,
    const CoJReloadGeometrySnapshot& geometry, const CoJReloadTraceSnapshot& current,
    bool enabled, runtime::StereoHudTextOverlay& out, bool chambers_cleared=false) noexcept {
    using namespace winchester_loading_detail;
    using namespace schofield_chamber_guide_detail;
    const auto mask=CoJSchofieldEmptyMask(geometry.schofield_loaded);
    if (!mask || !geometry.valid || geometry.model!=CoJReloadModel::schofield ||
        geometry.source!=CoJReloadGeometrySource::post_overlay ||
        !std::isfinite(geometry.native_drum_phase_degrees) ||
        !enabled || !frame.allowed || !frame.restored || !frame.frame ||
        frame.frame!=out.frame_sequence || !frame.player || !(frame.context&1U) ||
        !out.reload_feedback.owner_token || !CoJVirtualHolsterPoseValid(frame.head) ||
        !IsCoJNativeObjectIdentity(frame.weapon) || frame.armed_hand<0 || frame.armed_hand>1 ||
        frame.weapon!=current.weapon_id || frame.armed_hand!=current.armed_hand ||
        !Waiting(current,geometry)) return false;

    const auto tracking_right=Widen(frame.right);
    const auto tracking_up=Widen(frame.up), tracking_forward=Widen(frame.forward);
    if (!Rigid({frame.origin_cm,frame.up,frame.forward,true}) || !Finite(tracking_right) ||
        std::abs(Dot(tracking_right,tracking_right)-1)>.001 ||
        std::abs(Dot(tracking_right,tracking_up))>.001 ||
        std::abs(Dot(tracking_right,tracking_forward))>.001) return false;

    const auto empty_count=static_cast<std::size_t>(6-geometry.schofield_loaded);
    const auto first=chambers_cleared?empty_count:0U;
    auto& guides=out.holster_guides;
    if (first==6 || guides.count>guides.zones.size()-(6-first) ||
        !Rigid(geometry.root) || !Rigid(geometry.barrel) || !Rigid(geometry.gate)) return false;
    std::array<ElementWorldBasisTarget,6> expected{};
    if (!BuildCoJReloadMouthFrames(geometry.model,geometry.drum,expected)) return false;

    // GunReleaser follows the drum axis and its up stays fixed across spins.
    // The negative-Z rings face the muzzle. Draw on the positive-Z drum face.
    const auto axis=Widen(geometry.drum.forward);
    const auto axis_length=std::sqrt(Dot(axis,axis));
    const Vector outward{axis.x/axis_length,axis.y/axis_length,axis.z/axis_length};
    const Vector inward{-outward.x,-outward.y,-outward.z};
    const auto gate_up=Widen(geometry.gate.up);
    if (Dot(outward,Widen(geometry.gate.forward))<.999 ||
        Dot(gate_up,Widen(geometry.barrel.up))<.999) return false;
    const auto vertical=Dot(gate_up,inward);
    const Vector projected_up{gate_up.x-vertical*inward.x,gate_up.y-vertical*inward.y,
        gate_up.z-vertical*inward.z};
    const auto up_length=std::sqrt(Dot(projected_up,projected_up));
    const Vector up{projected_up.x/up_length,projected_up.y/up_length,projected_up.z/up_length};
    // Looking inward with gate up held vertical, screen right is look × up.
    const auto right=Cross(inward,up);

    struct OrderedMouth { double angle; std::size_t index; };
    std::array<OrderedMouth,6> ordered{};
    for (std::size_t i=0;i<ordered.size();++i) {
        const auto& mouth=geometry.mouths[i];
        const auto error=Difference(Widen(mouth.position),Widen(expected[i].position));
        if (!Rigid(mouth) || Dot(error,error)>.0001 ||
            Dot(Widen(mouth.up),Widen(expected[i].up))<.999 ||
            Dot(Widen(mouth.forward),Widen(expected[i].forward))<.999) return false;
        const auto radial=Difference(Widen(mouth.position),Widen(geometry.drum.position));
        // Sweep clockwise from the boundary between top and upper right.
        constexpr double pi=std::numbers::pi_v<double>;
        double angle=pi/3+std::atan2(-Dot(radial,up),Dot(radial,right));
        if (angle<0) angle+=2*pi;
        ordered[i]={angle,i};
    }
    std::sort(ordered.begin(),ordered.end(),
        [](const OrderedMouth& a,const OrderedMouth& b){return a.angle<b.angle;});

    const auto q=runtime::NormalizeQuaternion(frame.head.orientation);
    const auto head_direction=[&](Vector world) {
        const auto v=Project(world,tracking_right,tracking_up,tracking_forward);
        return runtime::RotateVector({-q.x,-q.y,-q.z,q.w},
            {static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)});
    };
    const auto head_inward=Widen(head_direction(inward));
    const auto head_right=head_direction(right), head_up=head_direction(up);
    // The existing texture's stroke is at ~85% of its half-width. Fit it to
    // the measured 0.546 cm radius and keep the quad in the drum face plane.
    constexpr float half_m=.0064F;
    std::array<runtime::HolsterGuideSurface,6> pending{};
    for (std::size_t i=first;i<ordered.size();++i) {
        auto& surface=pending[i];
        const auto front=Widen(geometry.mouths[ordered[i].index].position);
        const auto offset=kCoJSchofieldRearFaceCm-Dot(Difference(front,Widen(geometry.drum.position)),outward);
        runtime::Vec3 rear{};
        if (!Store({front.x+offset*outward.x,front.y+offset*outward.y,front.z+offset*outward.z},rear) ||
            !loading_mouth_guide_detail::Surface(frame,rear,0.F,surface)) return false;
        surface.state=i<empty_count?0:2;
        const auto centre=surface.head_corners[0];
        if (!RearView(Widen(centre),head_inward)) return false;
        for (const auto& eye:out.eyes) {
            if (eye.eye_to_head.position_valid &&
                !RearView(Difference(Widen(centre),Widen(eye.eye_to_head.position)),head_inward))
                return false;
        }
        for (unsigned corner=0;corner<surface.head_corners.size();++corner) {
            const float x=(corner&1U)?half_m:-half_m;
            const float y=(corner&2U)?-half_m:half_m;
            surface.head_corners[corner]={centre.x+x*head_right.x+y*head_up.x,
                centre.y+x*head_right.y+y*head_up.y,centre.z+x*head_right.z+y*head_up.z};
        }
    }
    for (std::size_t i=first;i<ordered.size();++i) guides.zones[guides.count++]=pending[i];
    return true;
}
}
