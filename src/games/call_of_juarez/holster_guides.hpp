#pragma once
#include "games/call_of_juarez/virtual_holster.hpp"
#include "runtime/hud_text.hpp"

namespace cojvr::games::call_of_juarez {
// Captured head-space billboards at the same centres/radii as gesture admission.
// No scene depth, anatomical socket claim, native call or selection side effect.
inline bool BuildCoJHolsterGuides(const CoJVirtualHolsterSample& s,bool enabled,
    runtime::StereoHudTextOverlay& out) noexcept {
    out.holster_guides={};
    if(!enabled||!s.allowed||!s.raw.active||!s.inventory.valid)return false;
    const auto layout=BuildCoJVirtualHolsterLayout(s.head);if(!layout.valid)return false;
    const auto q=runtime::NormalizeQuaternion(s.head.orientation);
    const runtime::Quaternion inverse{-q.x,-q.y,-q.z,q.w};
    const auto unit=[](runtime::Vec3 v){const float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        return n>.0001F?runtime::Vec3{v.x/n,v.y/n,v.z/n}:runtime::Vec3{};};
    const auto cross=[](runtime::Vec3 a,runtime::Vec3 b){return runtime::Vec3{
        a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
    for(unsigned i=0;i<kCoJVirtualHolsterZones.size();++i){
        const auto zone=kCoJVirtualHolsterZones[i];const auto bit=1U<<zone.channel;
        if(!(s.inventory.owned_mask&bit)||!CoJVirtualHolsterPoseValid(s.grips[zone.hand]))continue;
        const auto c=layout.centres[i];
        const auto centre=runtime::RotateVector(inverse,{c.x-s.head.position.x,c.y-s.head.position.y,c.z-s.head.position.z});
        const auto normal=unit({-centre.x,-centre.y,-centre.z});
        auto right=unit(cross({0,1,0},normal));
        if(right.x*right.x+right.y*right.y+right.z*right.z<.5F)right=unit(cross({0,0,1},normal));
        const auto up=cross(normal,right);
        auto& surface=out.holster_guides.zones[out.holster_guides.count++];
        const bool ready=s.native_ready&&(s.inventory.available_mask&bit)!=0&&
            !s.selection_blocked&&(zone.shoulder||!CoJVirtualHolsterWorldInteraction(s.interaction))&&
            !CoJVirtualHolsterInputConflict(s.raw,s.raw);
        surface.state=!ready?2:CoJVirtualHolsterZoneForHand(s.head,s.grips[zone.hand],zone.hand)==zone.channel?1:0;
        for(unsigned j=0;j<4;++j){const float x=(j&1)?zone.radius:-zone.radius,y=(j&2)?-zone.radius:zone.radius;
            surface.head_corners[j]={centre.x+x*right.x+y*up.x,centre.y+x*right.y+y*up.y,centre.z+x*right.z+y*up.z};}
    }
    return out.holster_guides.count!=0;
}
}
