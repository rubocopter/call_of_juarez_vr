#pragma once
#include "games/call_of_juarez/body_adapter.hpp"
#include <array>
#include <cmath>

namespace cojvr::games::call_of_juarez {
enum class CoJReloadModel : std::uint8_t { unknown, peacemaker, frontier };
struct CoJReloadGeometrySnapshot {
    CoJReloadModel model = CoJReloadModel::unknown;
    ElementWorldBasisTarget drum{}, gate{};
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
}
