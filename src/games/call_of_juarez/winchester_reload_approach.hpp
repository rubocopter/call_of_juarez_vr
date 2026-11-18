#pragma once

#include "games/call_of_juarez/reload_mouth_preview.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cojvr::games::call_of_juarez {

struct CoJWinchesterReloadApproach {
    runtime::Vec3 centre_cm{};
    runtime::Vec3 inward{};
    // Positive on the receiver's exterior. Decreases with inward progress
    // across successive samples of the same retained weapon and cartridge.
    float outside_distance_cm=0;
    float axis_alignment=0; // Normalized nose dot inward normal, in [-1, 1].
    bool mouth_valid=false; // The centre is also available before pickup.
    bool valid=false;       // The current nose also yielded distance and alignment.
    bool within_capture=false;
};

namespace winchester_reload_approach_detail {
using namespace winchester_loading_detail;

inline Vector Delta(Vector a,Vector b) noexcept {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}

inline Vector Unit(Vector v) noexcept {
    const double length=std::sqrt(Dot(v,v));
    if (!(length>0) || !std::isfinite(length)) return {};
    return {v.x/length,v.y/length,v.z/length};
}

struct LipBasis { Vector anchor,x,y,z; };

inline LipBasis OrderedLipBasis(const std::array<runtime::Vec3,8>& points) noexcept {
    const auto anchor=Widen(points[1]);
    const auto y=Unit(Delta(Widen(points[0]),anchor));
    // On both measured cycles, 1->0 crossed with 1->2 faces GunBody +X.
    const auto x=Unit(Cross(y,Delta(Widen(points[2]),anchor)));
    return {anchor,x,y,Cross(x,y)};
}

inline Vector LipLocal(const LipBasis& basis,Vector point) noexcept {
    return Project(Delta(point,basis.anchor),basis.x,basis.y,basis.z);
}
} // namespace winchester_reload_approach_detail

[[nodiscard]] inline CoJWinchesterReloadApproach BuildCoJWinchesterReloadApproach(
    const CoJReloadMouthPreview& preview) noexcept {
    using namespace winchester_loading_detail;
    using namespace winchester_reload_approach_detail;
    if (preview.count!=preview.points_cm.size()) return {};
    for (const auto point : preview.points_cm)
        if (!Finite(Widen(point))) return {};
    const auto world=OrderedLipBasis(preview.points_cm);
    if (Dot(world.x,world.x)==0) return {};

    // The two independently measured profiles share the ordered coordinates,
    // best-fit exterior normal, and projected-area centroid in GunBody space.
    const auto reference=OrderedLipBasis(ordinary);
    constexpr Vector body_normal{.9999357444236386,-.002132640050931548,.011133681797010917};
    constexpr Vector body_centre{1.2286408950131804,-17.34787603165211,-4.344577176263235};
    const auto normal=Unit(Project(body_normal,reference.x,reference.y,reference.z));
    const auto centre=LipLocal(reference,body_centre);
    const auto along=Unit(Cross({0,0,1},normal));
    const auto across=Cross(normal,along);

    std::array<Vector,8> polygon{};
    // Check the ordered shape within world-space float quantization allowance.
    // This guards orientation even if a malformed preview has eight points.
    constexpr double shape_tolerance_cm=.05;
    for (std::size_t i=0;i<polygon.size();++i) {
        const auto p=LipLocal(world,Widen(preview.points_cm[i]));
        const auto error=Delta(p,LipLocal(reference,Widen(ordinary[i])));
        if (!(Dot(error,error)<=shape_tolerance_cm*shape_tolerance_cm)) return {};
        polygon[i]=Project(Delta(p,centre),normal,along,across);
    }

    CoJWinchesterReloadApproach out{};
    if (!Store({world.anchor.x+centre.x*world.x.x+centre.y*world.y.x+centre.z*world.z.x,
                world.anchor.y+centre.x*world.x.y+centre.y*world.y.y+centre.z*world.z.y,
                world.anchor.z+centre.x*world.x.z+centre.y*world.y.z+centre.z*world.z.z},
               out.centre_cm)||
        !Store({-normal.x*world.x.x-normal.y*world.y.x-normal.z*world.z.x,
                -normal.x*world.x.y-normal.y*world.y.y-normal.z*world.z.y,
                -normal.x*world.x.z-normal.y*world.y.z-normal.z*world.z.z},out.inward)) return {};
    out.mouth_valid=true;
    if (!Rigid(preview.tip_cm)) return out; // Requires a read-valid, finite tip frame.

    const auto tip=Project(Delta(LipLocal(world,Widen(preview.tip_cm.position)),centre),normal,along,across);
    if (!Finite(tip) || std::abs(tip.x)>std::numeric_limits<float>::max()) return out;
    const auto axis=Project(Unit(Widen(preview.tip_cm.forward)),world.x,world.y,world.z);
    out.valid=true;
    out.outside_distance_cm=static_cast<float>(tip.x);
    out.axis_alignment=static_cast<float>(std::clamp(-Dot(axis,normal),-1.,1.));

    double nearest_edge_squared=std::numeric_limits<double>::infinity();
    bool projected_inside=false;
    for (std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[j], b=polygon[i];
        const double dy=b.y-a.y, dz=b.z-a.z;
        const double py=tip.y-a.y, pz=tip.z-a.z;
        const double t=std::clamp((py*dy+pz*dz)/(dy*dy+dz*dz),0.,1.);
        const double ey=py-t*dy, ez=pz-t*dz;
        nearest_edge_squared=std::min(nearest_edge_squared,ey*ey+ez*ez);
        if ((a.z>tip.z)!=(b.z>tip.z) && tip.y<a.y+(tip.z-a.z)*dy/dz)
            projected_inside=!projected_inside;
    }
    // Distance to the filled projected lip on either side of its plane.
    // Outside the polygon, the nearest point lies on its concave perimeter.
    const double planar_distance_squared=projected_inside?0.:nearest_edge_squared;
    constexpr double capture_cm=7.;
    out.within_capture=tip.x*tip.x+planar_distance_squared<=capture_cm*capture_cm;
    return out;
}
} // namespace cojvr::games::call_of_juarez
