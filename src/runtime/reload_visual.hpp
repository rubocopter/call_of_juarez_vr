#pragma once
#include "runtime/hud_text.hpp"
#include "runtime/vr_math.hpp"
#include <cmath>
#include <optional>

namespace cojvr::runtime {
namespace reload_visual_detail {
inline constexpr float base_z = -.0125F, shoulder_z = -.0375F, tip_z = -.0475F;
inline constexpr std::array<Vec3,6> ring{{{.006F,0,0},{.003F,.0051961524F,0},
    {-.003F,.0051961524F,0},{-.006F,0,0},{-.003F,-.0051961524F,0},
    {.003F,-.0051961524F,0}}};
[[nodiscard]] inline bool Valid(const Pose& p) noexcept {
    const auto q=p.orientation;
    const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return p.position_valid&&p.orientation_valid&&std::isfinite(p.position.x)&&
        std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&
        std::isfinite(n)&&std::abs(n-1.F)<=.001F;
}
}

// Tracking-space tip position matches the geometry below exactly. Its local +Z
// points inward along support-local -Z, as required by explicit reload sockets.
// A local-Y half turn retains grip roll and the support up axis. Presentation, not
// a native ammunition object or a demonstrated game loading-port contract.
[[nodiscard]] inline std::optional<Pose> BuildReloadCartridgeTipPose(
    const Pose& support) noexcept {
    if(!reload_visual_detail::Valid(support))return std::nullopt;
    Pose tip=support;
    const auto delta=RotateVector(support.orientation,{0,0,reload_visual_detail::tip_z});
    tip.position={support.position.x+delta.x,support.position.y+delta.y,support.position.z+delta.z};
    const auto q=support.orientation;
    // support_from_tip = (0,1,0,0): post-multiply for a grip-local Y rotation.
    tip.orientation={-q.z,q.w,q.x,-q.y};
    if(!reload_visual_detail::Valid(tip))return std::nullopt;
    return tip;
}

// Capture with the eye images' head/support poses. Six case faces, six tapered
// bullet faces and two base quads: fixed storage, no geometry allocations.
// The color-only compositor cannot occlude this cartridge against scene depth.
[[nodiscard]] inline bool BuildReloadCartridgeOverlay(const Pose& head,
    const Pose& support, bool visible, StereoHudTextOverlay& out) noexcept {
    out.reload_cartridge_visible=false;
    out.reload_cartridge_head_corners={}; // Legacy sprite only; new captures use surfaces.
    out.reload_cartridge_surface_count=0;
    out.reload_cartridge_surfaces={};
    using namespace reload_visual_detail;
    if(!visible||!Valid(head)||!Valid(support))return false;
    const auto q=head.orientation;
    const Quaternion inverse{-q.x,-q.y,-q.z,q.w};
    std::array<ReloadCartridgeSurface,StereoHudTextOverlay::reload_cartridge_max_surfaces> faces{};
    std::uint32_t count=0;
    const auto capture=[&](std::array<Vec3,4> local,Vec2 uv,float shade){
        auto& face=faces[count++];face.material_uv=uv;face.shade=shade;
        for(std::size_t i=0;i<4;++i){
            const auto p=RotateVector(support.orientation,local[i]);
            face.head_corners[i]=RotateVector(inverse,{support.position.x-head.position.x+p.x,
                support.position.y-head.position.y+p.y,support.position.z-head.position.z+p.z});
        }
    };
    const auto vertex=[](std::size_t i,float z){auto p=ring[i];p.z=z;return p;};
    for(std::size_t i=0;i<6;++i){
        const auto j=(i+1)%6;
        const float shade=.65F+.35F*(ring[i].y/.006F+1.F)*.5F;
        capture({vertex(i,shoulder_z),vertex(j,shoulder_z),vertex(i,base_z),vertex(j,base_z)},
            {.5F,80.F/140.F},shade);
        // A degenerate quad is a triangle; its shared apex is the exported tip.
        capture({vertex(j,shoulder_z),vertex(i,shoulder_z),Vec3{0,0,tip_z},Vec3{0,0,tip_z}},
            {.5F,24.F/140.F},shade);
    }
    capture({vertex(0,base_z),vertex(1,base_z),vertex(3,base_z),vertex(2,base_z)},
        {.5F,80.F/140.F},.65F);
    capture({vertex(0,base_z),vertex(3,base_z),vertex(5,base_z),vertex(4,base_z)},
        {.5F,80.F/140.F},.65F);
    for(std::size_t i=0;i<count;++i)for(const auto p:faces[i].head_corners)
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.z>=-.05F)return false;
    out.reload_cartridge_surfaces=faces;
    out.reload_cartridge_surface_count=count;
    out.reload_cartridge_visible=true;
    return true;
}
}
