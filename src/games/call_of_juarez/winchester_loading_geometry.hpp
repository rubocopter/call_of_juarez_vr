#pragma once

#include "games/call_of_juarez/reload_geometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cojvr::games::call_of_juarez {
enum class CoJWinchesterLoadingModel : std::uint8_t { unknown, ordinary, scoped };

// Native centimetres. Same-frame, owner-qualified displayed geometry only;
// neither the exterior lip nor the loader pivot establishes an insertion path.
struct CoJWinchesterLoadingGeometry {
    CoJWinchesterLoadingModel model = CoJWinchesterLoadingModel::unknown;
    CoJReloadGeometrySource source = CoJReloadGeometrySource::unknown;
    ElementWorldBasisTarget root{}, body{}, loader{}, holder{}, barrel{};
    std::array<runtime::Vec3,8> lip{};
    bool valid = false;
};

namespace winchester_loading_detail {
// Independently measured LOD0 GunBody cycles, recorded in
// docs/research/COJ_RELOAD_OWNERSHIP.md (Measured Winchester receiver lip).
// Ordinary mesh SHA-256:
// be1f0508ea41e809a413256c295ba75b02eeaf7df935c3c1dfb6e623a8ee320e
// Vertices 264,280,282,286,296,284,265,266.
inline constexpr std::array<runtime::Vec3,8> ordinary{{
    {1.2345394813455641F,-14.82387183594983F,-5.256761945434846F},
    {1.2345394813455641F,-20.06876739661675F,-5.249093969468959F},
    {1.2115355534479022F,-20.06876739661675F,-3.4221986955963075F},
    {1.213452547439374F,-14.846875763847493F,-3.4471196174854413F},
    {1.2345394813455641F,-14.84495876985602F,-3.79217853595037F},
    {1.2345394813455641F,-14.488397887442261F,-3.79217853595037F},
    {1.2441244513029233F,-14.496065863408148F,-4.905952044995502F},
    {1.2441244513029233F,-14.821954841958359F,-4.913620020961389F}}};
// Scoped mesh SHA-256:
// a989d5c4bcaad39e250a0fcaa070c985ec832ec51e2f48e6f2d62eb580d532cd
// Vertices 300,316,318,322,332,320,301,302. Coordinates independently agree;
// retaining a separate profile does not admit any other Winchester model.
inline constexpr std::array<runtime::Vec3,8> scoped{{
    {1.2345394813455641F,-14.82387183594983F,-5.256761945434846F},
    {1.2345394813455641F,-20.06876739661675F,-5.249093969468959F},
    {1.2115355534479022F,-20.06876739661675F,-3.4221986955963075F},
    {1.213452547439374F,-14.846875763847493F,-3.4471196174854413F},
    {1.2345394813455641F,-14.84495876985602F,-3.79217853595037F},
    {1.2345394813455641F,-14.488397887442261F,-3.79217853595037F},
    {1.2441244513029233F,-14.496065863408148F,-4.905952044995502F},
    {1.2441244513029233F,-14.821954841958359F,-4.913620020961389F}}};

inline const std::array<runtime::Vec3,8>* Profile(CoJWinchesterLoadingModel model) noexcept {
    switch (model) {
    case CoJWinchesterLoadingModel::ordinary: return &ordinary;
    case CoJWinchesterLoadingModel::scoped: return &scoped;
    default: return nullptr;
    }
}
// Double intermediates avoid overflow/cancellation in finite float world frames.
struct Vector { double x, y, z; };
inline Vector Widen(runtime::Vec3 v) noexcept { return {v.x,v.y,v.z}; }
inline double Dot(Vector a, Vector b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vector Cross(Vector a, Vector b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool Finite(Vector v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool Rigid(const ElementWorldBasisTarget& f) noexcept {
    const auto u=Widen(f.up), z=Widen(f.forward);
    // Basis numerical validation only; no aperture/interaction tolerance.
    return f.valid && Finite(Widen(f.position)) && Finite(u) && Finite(z) &&
        std::abs(Dot(u,u)-1)<=.001 && std::abs(Dot(z,z)-1)<=.001 &&
        std::abs(Dot(u,z))<=.001;
}
inline bool Store(Vector v, runtime::Vec3& out) noexcept {
    constexpr double limit=std::numeric_limits<float>::max();
    if (!Finite(v) || std::abs(v.x)>limit || std::abs(v.y)>limit || std::abs(v.z)>limit)
        return false;
    out={static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)};
    return true;
}
inline Vector Project(Vector v, Vector x, Vector y, Vector z) noexcept {
    return {Dot(v,x),Dot(v,y),Dot(v,z)};
}
} // namespace winchester_loading_detail

inline bool BuildCoJWinchesterLoadingLip(CoJWinchesterLoadingModel model,
    const ElementWorldBasisTarget& body, std::array<runtime::Vec3,8>& out) noexcept {
    using namespace winchester_loading_detail;
    out={};
    const auto* profile=Profile(model);
    if (!profile || !Rigid(body)) return false;
    const auto y=Widen(body.up), z=Widen(body.forward), x=Cross(y,z), p=Widen(body.position);
    std::array<runtime::Vec3,8> transformed{};
    for (std::size_t i=0;i<transformed.size();++i) {
        const auto v=(*profile)[i];
        if (!Store({p.x+x.x*v.x+y.x*v.y+z.x*v.z,
                    p.y+x.y*v.x+y.y*v.y+z.y*v.z,
                    p.z+x.z*v.x+y.z*v.y+z.z*v.z},transformed[i])) return false;
    }
    out=transformed;
    return true;
}

struct CoJWinchesterLoadingTipObservation {
    runtime::Vec3 body_local_tip{}, body_local_axis{};
    float plane_distance_cm = 0, edge_distance_cm = 0;
    bool projected_inside = false, valid = false;
};

// Diagnostic distances only. Positive plane distance is toward the measured
// outward best-fit normal (+X side), not an insertion/clearance determination.
// tip.forward is the actual displayed cartridge nose direction supplied by the
// caller. No direction is inferred from the lip, native pivot or inspection ray.
inline CoJWinchesterLoadingTipObservation ObserveCoJWinchesterLoadingTip(
    const CoJWinchesterLoadingGeometry& geometry, const ElementWorldBasisTarget& tip,
    bool owner_qualified) noexcept {
    using namespace winchester_loading_detail;
    if (!owner_qualified || !geometry.valid ||
        geometry.source!=CoJReloadGeometrySource::post_overlay ||
        !Rigid(geometry.root) || !Rigid(geometry.body) || !Rigid(geometry.loader) ||
        !Rigid(geometry.holder) || !Rigid(geometry.barrel) || !Rigid(tip)) return {};
    std::array<runtime::Vec3,8> expected{};
    if (!BuildCoJWinchesterLoadingLip(geometry.model,geometry.body,expected)) return {};
    for (std::size_t i=0;i<expected.size();++i) {
        const auto a=geometry.lip[i], b=expected[i];
        // The value snapshot must contain this builder's exact current result.
        // No geometric slack can admit stale/misordered/nonfinite lip corners.
        if (a.x!=b.x || a.y!=b.y || a.z!=b.z) return {};
    }
    const auto y=Widen(geometry.body.up), z=Widen(geometry.body.forward), x=Cross(y,z);
    const Vector delta{double(tip.position.x)-geometry.body.position.x,
                       double(tip.position.y)-geometry.body.position.y,
                       double(tip.position.z)-geometry.body.position.z};
    const auto local=Project(delta,x,y,z);
    CoJWinchesterLoadingTipObservation out{};
    if (!Store(local,out.body_local_tip) ||
        !Store(Project(Widen(tip.forward),x,y,z),out.body_local_axis)) return {};

    // Both independent measured profiles have this best-fit plane. Its normal
    // and projected area centroid (with plane X) come from rim-measurements.json;
    // the eight slightly nonplanar corners still own the projected boundary.
    constexpr Vector normal{.9999357444236386,-.002132640050931548,.011133681797010917};
    constexpr Vector origin{1.2286408950131804,-17.34787603165211,-4.344577176263235};
    const double plane=Dot({local.x-origin.x,local.y-origin.y,local.z-origin.z},normal);
    double nearest2=std::numeric_limits<double>::infinity();
    bool inside=false;
    const auto& polygon=*Profile(geometry.model);
    for (std::size_t i=0,j=polygon.size()-1;i<polygon.size();j=i++) {
        const auto a=polygon[j], b=polygon[i];
        const double dy=double(b.y)-a.y, dz=double(b.z)-a.z;
        const double py=local.y-a.y, pz=local.z-a.z;
        const double t=std::clamp((py*dy+pz*dz)/(dy*dy+dz*dz),0.,1.);
        const double ey=py-t*dy, ez=pz-t*dz;
        nearest2=std::min(nearest2,ey*ey+ez*ez);
        // Half-open ray crossing counts a shared vertex once and retains the
        // concave tab/notches. Boundary ties are handled separately below.
        if ((a.z>local.z)!=(b.z>local.z) && local.y<a.y+(local.z-a.z)*dy/dz)
            inside=!inside;
    }
    const double edge=std::sqrt(nearest2);
    if (!std::isfinite(plane) || std::abs(plane)>std::numeric_limits<float>::max() ||
        !std::isfinite(edge) || edge>std::numeric_limits<float>::max()) return {};
    // Only float roundoff at this native-cm coordinate scale is a boundary tie.
    // This is not cartridge radius, contact padding or an ergonomic tolerance.
    const double tie=8*std::numeric_limits<float>::epsilon()*
        std::max({1.,std::abs(local.y),std::abs(local.z)});
    out.plane_distance_cm=static_cast<float>(plane);
    out.edge_distance_cm=static_cast<float>(edge);
    out.projected_inside=inside || edge<=tie;
    out.valid=true;
    return out;
}
}
