#pragma once
#include "games/call_of_juarez/body_adapter.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cojvr::games::call_of_juarez {
enum class CoJReloadModel : std::uint8_t { unknown, peacemaker, frontier };
enum class CoJReloadGeometrySource : std::uint8_t { unknown, pre_native, post_overlay };
// Centimetres, relative to a stable root or barrel frame. Read-only diagnostics.
struct CoJReloadLocalFrame {
    runtime::Vec3 position{}, up{}, forward{};
    bool valid = false;
};
inline bool BuildCoJReloadLocalFrame(const ElementWorldBasisTarget& reference,
    const ElementWorldBasisTarget& element,CoJReloadLocalFrame& out) noexcept {
    out={};
    const auto finite=[](runtime::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto dot=[](runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const auto cross=[](runtime::Vec3 a,runtime::Vec3 b){return runtime::Vec3{
        a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
    const auto rigid=[&](const ElementWorldBasisTarget& f){return f.valid&&finite(f.position)&&
        finite(f.up)&&finite(f.forward)&&std::abs(dot(f.up,f.up)-1.F)<.002F&&
        std::abs(dot(f.forward,f.forward)-1.F)<.002F&&std::abs(dot(f.up,f.forward))<.002F;};
    if(!rigid(reference)||!rigid(element))return false;
    const auto right=cross(reference.up,reference.forward);
    const auto project=[&](runtime::Vec3 value){return runtime::Vec3{
        dot(value,right),dot(value,reference.up),dot(value,reference.forward)};};
    const runtime::Vec3 delta{element.position.x-reference.position.x,
        element.position.y-reference.position.y,element.position.z-reference.position.z};
    if(!finite(delta))return false;
    out={project(delta),project(element.up),project(element.forward),true};
    if(!finite(out.position)||!finite(out.up)||!finite(out.forward)){out={};return false;}
    return true;
}
struct CoJReloadGeometrySnapshot {
    CoJReloadModel model = CoJReloadModel::unknown;
    CoJReloadGeometrySource source = CoJReloadGeometrySource::unknown;
    ElementWorldBasisTarget root{}, barrel{};
    ElementWorldBasisTarget drum{}, gate{};
    CoJReloadLocalFrame drum_root{},gate_root{},drum_barrel{},gate_barrel{};
    std::array<ElementWorldBasisTarget,6> mouths{};
    bool valid = false;
};
// Exact model observations in centimetres. These are chamber-mouth geometry,
// not occupied chambers or proof of loading-gate clearance. Caller must bind
// the frames to the current displayed exact model, including native phase.
inline bool BuildCoJReloadMouthFrames(CoJReloadModel model,
    const ElementWorldBasisTarget& drum,
    std::array<ElementWorldBasisTarget,6>& out) noexcept {
    out={};
    const auto finite=[](runtime::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto dot=[](runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    if(!drum.valid || !finite(drum.position) || !finite(drum.up) || !finite(drum.forward) ||
        std::abs(dot(drum.up,drum.up)-1.F)>.001F ||
        std::abs(dot(drum.forward,drum.forward)-1.F)>.001F ||
        std::abs(dot(drum.up,drum.forward))>.001F)return false;
    constexpr std::array<runtime::Vec3,6> peacemaker{{
        {-.000764F,1.424893F,-2.383278F},{-1.234752F,.712396F,-2.383278F},
        {1.233232F,.712472F,-2.383278F},{-1.234718F,-.712462F,-2.383278F},
        {-.000696F,-1.424891F,-2.383278F},{1.233266F,-.712394F,-2.383278F}}};
    constexpr std::array<runtime::Vec3,6> frontier{{
        {1.196130F,.009727F,-2.007212F},{.591903F,1.036132F,-2.008803F},
        {-1.185826F,-.010422F,-2.010485F},{.609356F,-1.026731F,-2.007292F},
        {-.598805F,1.025625F,-2.010424F},{-.581604F,-1.036808F,-2.008913F}}};
    if(model!=CoJReloadModel::peacemaker && model!=CoJReloadModel::frontier)return false;
    const auto& local=model==CoJReloadModel::peacemaker?peacemaker:frontier;
    const auto u=drum.up,f=drum.forward;
    const runtime::Vec3 x{u.y*f.z-u.z*f.y,u.z*f.x-u.x*f.z,u.x*f.y-u.y*f.x};
    for(std::size_t i=0;i<out.size();++i){
        const auto p=local[i];
        out[i]={{drum.position.x+x.x*p.x+u.x*p.y+f.x*p.z,
            drum.position.y+x.y*p.x+u.y*p.y+f.y*p.z,
            drum.position.z+x.z*p.x+u.z*p.y+f.z*p.z},u,f,true};
        if(!finite(out[i].position)){out={};return false;}
    }
    return true;
}
// Gate/loader element origins are pivots, not established loading-port centres.
// This diagnostic ranks observed mouth centres by proximity to that pivot.
// A unique nearest mouth is NOT an accessible chamber or an insertion socket.
enum class CoJReloadPortRankStatus : std::uint8_t { invalid, ambiguous, pivot_only };
struct CoJReloadPortProbe {
    CoJReloadPortRankStatus status = CoJReloadPortRankStatus::invalid;
    int nearest_mouth = -1;
    float nearest_distance_cm = 0.F, runner_up_distance_cm = 0.F;
    float separation_cm = 0.F, axial_offset_cm = 0.F, radial_offset_cm = 0.F;
    bool gate_open_confirmed = false, can_accept = false;
};
inline CoJReloadPortProbe RankCoJReloadGatePivotMouths(
    const CoJReloadGeometrySnapshot& geometry, bool owner_qualified) noexcept {
    CoJReloadPortProbe probe{};
    if(!owner_qualified || !geometry.valid ||
        geometry.source!=CoJReloadGeometrySource::post_overlay ||
        (geometry.model!=CoJReloadModel::peacemaker &&
         geometry.model!=CoJReloadModel::frontier))return probe;
    CoJReloadLocalFrame local{};
    if(!BuildCoJReloadLocalFrame(geometry.root,geometry.barrel,local) ||
       !BuildCoJReloadLocalFrame(geometry.root,geometry.drum,local) ||
       !BuildCoJReloadLocalFrame(geometry.root,geometry.gate,local))return probe;
    std::array<ElementWorldBasisTarget,6> expected{};
    if(!BuildCoJReloadMouthFrames(geometry.model,geometry.drum,expected))return probe;
    const auto sq=[](runtime::Vec3 d){return d.x*d.x+d.y*d.y+d.z*d.z;};
    const auto delta=[](runtime::Vec3 a,runtime::Vec3 b){return runtime::Vec3{
        a.x-b.x,a.y-b.y,a.z-b.z};};
    const auto dot=[](runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    float best=std::numeric_limits<float>::infinity();
    float next=best;
    int best_index=-1;
    for(std::size_t i=0;i<geometry.mouths.size();++i){
        const auto& mouth=geometry.mouths[i];
        const auto& reference=expected[i];
        if(!mouth.valid || !std::isfinite(sq(delta(mouth.position,reference.position))) ||
           sq(delta(mouth.position,reference.position))>.0001F ||
           !std::isfinite(sq(delta(mouth.up,reference.up))) ||
           sq(delta(mouth.up,reference.up))>.000004F ||
           !std::isfinite(sq(delta(mouth.forward,reference.forward))) ||
           sq(delta(mouth.forward,reference.forward))>.000004F)return {};
        const float distance=sq(delta(geometry.gate.position,mouth.position));
        if(!std::isfinite(distance))return {};
        if(distance<best){next=best;best=distance;best_index=static_cast<int>(i);}
        else if(distance<next)next=distance;
    }
    if(best_index<0 || !std::isfinite(next))return probe;
    probe.nearest_distance_cm=std::sqrt(best);
    probe.runner_up_distance_cm=std::sqrt(next);
    probe.separation_cm=probe.runner_up_distance_cm-probe.nearest_distance_cm;
    const auto gap=delta(geometry.gate.position,geometry.mouths[static_cast<std::size_t>(best_index)].position);
    probe.axial_offset_cm=dot(gap,geometry.drum.forward);
    probe.radial_offset_cm=std::sqrt(std::max(0.F,best-probe.axial_offset_cm*probe.axial_offset_cm));
    // 0.005 cm is only a numeric tie threshold for a mesh-space diagnostic.
    // No gameplay acceptance or clearance threshold derives from this ranking.
    if(probe.separation_cm<=.005F){probe.status=CoJReloadPortRankStatus::ambiguous;return probe;}
    probe.nearest_mouth=best_index;
    probe.status=CoJReloadPortRankStatus::pivot_only;
    return probe;
}
}
