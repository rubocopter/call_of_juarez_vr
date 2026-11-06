#pragma once
#include "games/call_of_juarez/winchester_loading_probe.hpp"
#include "games/call_of_juarez/remington_loading_probe.hpp"
#include "games/call_of_juarez/holster_guides.hpp"

namespace cojvr::games::call_of_juarez {
// Same captured mapping as BuildTrackedAimOrigin: native camera with the world-Y
// physical height correction already included, and the exact tracking basis.
struct CoJLoadingMouthGuideFrame {
    runtime::Pose head{};
    runtime::Vec3 origin_cm{},right{},up{},forward{};
    std::uint64_t frame=0,player=0,context=0,weapon=0;
    int armed_hand=-1;
    bool allowed=false,restored=false;
};
namespace loading_mouth_guide_detail {
inline bool Current(const CoJLoadingMouthGuideFrame& f,const CoJReloadTraceSnapshot& n,
    std::uint64_t frame,std::uint64_t player,std::uint64_t context,bool enabled,
    const runtime::StereoHudTextOverlay& out) noexcept {
    using namespace winchester_loading_detail;
    if(!enabled||!f.allowed||!f.restored||!f.frame||!f.player||!(f.context&1U)||!f.weapon||
        f.frame!=frame||f.frame!=out.frame_sequence||f.player!=player||f.context!=context||
        f.weapon!=n.weapon_id||f.armed_hand!=n.armed_hand||!n.valid||!n.probe_valid||n.probe_status!=2||
        n.reserve_readback<=0||!out.reload_feedback.owner_token||!CoJVirtualHolsterPoseValid(f.head)||
        !Finite(Widen(f.origin_cm)))return false;
    const auto x=Widen(f.right),y=Widen(f.up),z=Widen(f.forward);
    return Finite(x)&&Finite(y)&&Finite(z)&&std::abs(Dot(x,x)-1)<=.001&&
        std::abs(Dot(y,y)-1)<=.001&&std::abs(Dot(z,z)-1)<=.001&&
        std::abs(Dot(x,y))<=.001&&std::abs(Dot(x,z))<=.001&&std::abs(Dot(y,z))<=.001;
}
inline bool Surface(const CoJLoadingMouthGuideFrame& f,runtime::Vec3 world_cm,float half_m,
    runtime::HolsterGuideSurface& out) noexcept {
    using namespace winchester_loading_detail;
    const auto p=Widen(world_cm),o=Widen(f.origin_cm);
    const Vector delta{(p.x-o.x)/100.,(p.y-o.y)/100.,(p.z-o.z)/100.};
    runtime::Vec3 tracking{};
    if(!Store(Project(delta,Widen(f.right),Widen(f.up),Widen(f.forward)),tracking))return false;
    const auto q=runtime::NormalizeQuaternion(f.head.orientation);
    const auto c=runtime::RotateVector({-q.x,-q.y,-q.z,q.w},
        {tracking.x-f.head.position.x,tracking.y-f.head.position.y,tracking.z-f.head.position.z});
    if(!Finite(Widen(c))||c.z>=-.05F||c.z< -2.5F)return false;
    out={};out.kind=runtime::GestureGuideKind::loading_mouth;
    // Cyan geometry locator only. Never assert aperture clearance, occupancy,
    // ammunition-slot mapping or insertion readiness from a measured centre.
    out.state=0;
    for(unsigned i=0;i<4;++i)out.head_corners[i]={c.x+((i&1)?half_m:-half_m),
        c.y+((i&2)?-half_m:half_m),c.z};
    return true;
}
}
inline bool AppendCoJWinchesterLoadingGuide(const CoJLoadingMouthGuideFrame& f,
    const CoJWinchesterLoadingSample& s,bool enabled,runtime::StereoHudTextOverlay& out) noexcept {
    if(!s.valid||s.geometry.source!=CoJReloadGeometrySource::post_overlay||
        !loading_mouth_guide_detail::Current(f,s.native,s.frame_sequence,s.player_generation,s.native_context,enabled,out)||
        out.holster_guides.count>=out.holster_guides.zones.size())return false;
    CoJWinchesterLoadingSample checked{};
    if(!BuildCoJWinchesterLoadingSample(s.geometry,s.native,s.native,s.frame_sequence,s.player_generation,s.native_context,{},checked))return false;
    const auto& b=s.geometry.body;
    const auto x=winchester_loading_detail::Cross(winchester_loading_detail::Widen(b.up),winchester_loading_detail::Widen(b.forward));
    // Measured projected-area centroid of the exact receiver lip; not GunLoader
    // or Holder's pivot, and not an invented grip-to-port offset.
    constexpr runtime::Vec3 local{1.228641F,-17.347876F,-4.344577F};
    runtime::Vec3 centre{};
    if(!winchester_loading_detail::Store({b.position.x+x.x*local.x+b.up.x*local.y+b.forward.x*local.z,
        b.position.y+x.y*local.x+b.up.y*local.y+b.forward.y*local.z,
        b.position.z+x.z*local.x+b.up.z*local.y+b.forward.z*local.z},centre))return false;
    runtime::HolsterGuideSurface guide{};
    if(!loading_mouth_guide_detail::Surface(f,centre,.012F,guide))return false;
    out.holster_guides.zones[out.holster_guides.count++]=guide;return true;
}
inline bool AppendCoJRemingtonLoadingGuides(const CoJLoadingMouthGuideFrame& f,
    const CoJRemingtonLoadingSample& s,bool enabled,runtime::StereoHudTextOverlay& out) noexcept {
    if(!s.valid||s.geometry.source!=CoJReloadGeometrySource::post_overlay||
        !loading_mouth_guide_detail::Current(f,s.native,s.frame_sequence,s.player_generation,s.native_context,enabled,out)||
        out.holster_guides.count>out.holster_guides.zones.size()-2)return false;
    CoJRemingtonLoadingSample checked{};
    if(!BuildCoJRemingtonLoadingSample(s.geometry,s.native,s.native,s.frame_sequence,s.player_generation,s.native_context,{},checked))return false;
    std::array<runtime::HolsterGuideSurface,2> guides{};
    for(unsigned i=0;i<2;++i)if(!loading_mouth_guide_detail::Surface(f,s.geometry.mouths[i].position,.008F,guides[i]))return false;
    for(const auto& guide:guides)out.holster_guides.zones[out.holster_guides.count++]=guide;
    return true;
}
}
