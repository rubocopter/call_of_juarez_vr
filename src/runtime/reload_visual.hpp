#pragma once
#include "runtime/hud_text.hpp"
#include "runtime/vr_math.hpp"
#include "runtime/reload_cartridge_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

namespace cojvr::runtime {
namespace reload_visual_detail {
// Low-poly visual proxy sized from Pichuliru's CC0 Flat Ammunition .44 Magnum
// OBJ envelope (metres): Y [-.010396, .030494], radius <= .00653.
// The original 224-triangle mesh is NOT loaded. Keep the existing exported tip
// fixed so the independent motion-reload insertion contract cannot drift.
inline constexpr float source_base_y = -.010396F, source_tip_y = .030494F;
inline constexpr float source_shoulder_y = .022244F;
inline constexpr float tip_z = -.0475F;
inline constexpr float base_z = tip_z + source_tip_y - source_base_y;
inline constexpr float shoulder_z = tip_z + source_tip_y - source_shoulder_y;
inline constexpr float radius = .00653F;
inline constexpr std::array<Vec3,6> ring{{{radius,0,0},{radius*.5F,radius*.8660254F,0},
    {-radius*.5F,radius*.8660254F,0},{-radius,0,0},{-radius*.5F,-radius*.8660254F,0},
    {radius*.5F,-radius*.8660254F,0}}};
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
// bullet faces, two base quads and two primer quads: fixed storage, no allocations.
// This is an authored 16-surface approximation of a CC0 model's dimensions,
// rather than an imported copy of the original mesh or materials.
// The color-only compositor cannot occlude this cartridge against scene depth.
[[nodiscard]] inline bool BuildReloadCartridgeProxyOverlay(const Pose& head,
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
        const float shade=.65F+.35F*(ring[i].y/radius+1.F)*.5F;
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
    // Rear primer, overlaid on the base. This is intentionally coplanar:
    // the cartridge uses ordered compositor quads and no scene depth buffer.
    const auto primer_vertex=[](std::size_t i){
        auto p=ring[i];p.x*=.35F;p.y*=.35F;p.z=base_z;return p;
    };
    capture({primer_vertex(0),primer_vertex(1),primer_vertex(3),primer_vertex(2)},
        {.5F,108.F/140.F},.88F);
    capture({primer_vertex(0),primer_vertex(3),primer_vertex(5),primer_vertex(4)},
        {.5F,108.F/140.F},.88F);
    for(std::size_t i=0;i<count;++i)for(const auto p:faces[i].head_corners)
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.z>=-.05F)return false;
    out.reload_cartridge_surfaces=faces;
    out.reload_cartridge_surface_count=count;
    out.reload_cartridge_visible=true;
    return true;
}

// Full imported CC0 geometry; colors are authored for the existing compositor.
// All capture data stays value-only. Far-to-near ordering provides bounded
// self overlap; no native scene depth or ammunition object is claimed.
[[nodiscard]] inline bool BuildReloadCartridgeOverlay(const Pose& head,
    const Pose& anchor,bool visible,StereoHudTextOverlay& out) noexcept {
    out.reload_cartridge_visible=false;out.reload_cartridge_surface_count=0;
    out.reload_cartridge_head_corners={};out.reload_cartridge_surfaces={};
    using namespace reload_visual_detail;
    if(!visible||!Valid(head)||!Valid(anchor))return false;
    const auto q=head.orientation;
    const Quaternion inverse{-q.x,-q.y,-q.z,q.w};
    std::array<Vec3,reload_mesh::vertices.size()> captured{};
    for(std::size_t i=0;i<captured.size();++i){
        const auto p=RotateVector(anchor.orientation,reload_mesh::vertices[i]);
        captured[i]=RotateVector(inverse,{anchor.position.x-head.position.x+p.x,
            anchor.position.y-head.position.y+p.y,anchor.position.z-head.position.z+p.z});
        const auto v=captured[i];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.z>=-.05F)return false;
    }
    auto& surfaces=out.reload_cartridge_surfaces;
    for(std::size_t i=0;i<reload_mesh::triangles.size();++i){
        const auto t=reload_mesh::triangles[i];auto& face=surfaces[i];
        face.head_corners={captured[t[0]],captured[t[1]],captured[t[2]],captured[t[2]]};
        const auto a=reload_mesh::vertices[t[0]],b=reload_mesh::vertices[t[1]],c=reload_mesh::vertices[t[2]];
        const float z=(a.z+b.z+c.z)/3.F;
        const float r2=(a.x*a.x+a.y*a.y+b.x*b.x+b.y*b.y+c.x*c.x+c.y*c.y)/3.F;
        face.material_uv={.5F,z<=shoulder_z?24.F/140.F:
            z>base_z-.001F&&r2<.000007F?108.F/140.F:80.F/140.F};
        const Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        const Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        if(!std::isfinite(length)||length<=0)return false;
        face.shade=.72F+.28F*std::max(0.F,n.y/length);
    }
    std::sort(surfaces.begin(),surfaces.end(),[](const auto& a,const auto& b){
        const auto depth=[](const auto& f){return f.head_corners[0].z+f.head_corners[1].z+f.head_corners[2].z;};
        return depth(a)<depth(b);
    });
    out.reload_cartridge_surface_count=static_cast<std::uint32_t>(reload_mesh::triangles.size());
    out.reload_cartridge_visible=true;return true;
}
}
