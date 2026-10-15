#pragma once
#include "runtime/vr_types.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace cojvr::games::call_of_juarez {
struct CoJFistContactIdentity {
    std::uint64_t pose=0,player=0,owner=0,context=0,input=0;
    float native_yaw=0; // Compensation alone is not the visual reference identity.
    bool valid=false;
    std::array<runtime::Vec3,3> reference_basis{{{1,0,0},{0,1,0},{0,0,1}}};
};
inline bool CoJFistContactContinuous(const CoJFistContactIdentity& previous,
    const CoJFistContactIdentity& current,std::uint64_t previous_pose) noexcept {
    for(std::size_t i=0;i<previous.reference_basis.size();++i){
        const auto a=previous.reference_basis[i],b=current.reference_basis[i];
        // Only float representation noise is tolerated, not a camera motion
        // threshold. Unit reference axes are built by native float rotations.
        constexpr float rounding=32.F*std::numeric_limits<float>::epsilon();
        const auto same=[](float x,float y){return std::isfinite(x)&&std::isfinite(y)&&std::fabs(x-y)<=rounding;};
        if(!same(a.x,b.x)||!same(a.y,b.y)||!same(a.z,b.z))return false;
    }
    return previous.valid&&current.valid&&previous.pose==previous_pose&&
        previous.player==current.player&&previous.owner==current.owner&&
        previous.context==current.context&&previous.input==current.input&&
        std::isfinite(previous.native_yaw)&&std::isfinite(current.native_yaw);
}
struct CoJFistContactSweep {
    std::array<std::array<runtime::Vec3,2>,7> segments{};
    std::uint8_t count=0;
    bool valid=false;
};
inline CoJFistContactSweep BuildCoJFistContactSweep(
    const std::array<runtime::Vec3,4>& previous,
    const std::array<runtime::Vec3,4>& current,bool continuous) noexcept {
    CoJFistContactSweep out{};
    const auto finite=[](runtime::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    for(std::size_t i=0;i<current.size();++i)
        if(!finite(current[i])||(continuous&&!finite(previous[i])))return {};
    const auto append=[&](runtime::Vec3 a,runtime::Vec3 b){
        const float x=b.x-a.x,y=b.y-a.y,z=b.z-a.z;
        const float length2=x*x+y*y+z*z;
        // Centimetres: native payload permits at most 25 cm per segment.
        // Below 0.01 cm there is no useful line for the native trace query.
        if(!std::isfinite(length2)||length2>625.F)return false;
        if(length2>=.0001F)out.segments[out.count++]={a,b};
        return true;
    };
    if(continuous)for(std::size_t i=0;i<current.size();++i)
        if(!append(previous[i],current[i]))return {};
    // Current skin cross-section keeps contact observable while motion is
    // slow or zero. These rays may probe a latch but cannot authorize damage.
    for(std::size_t i=1;i<current.size();++i)
        if(!append(current[i-1],current[i]))return {};
    out.valid=out.count!=0;
    return out;
}
}
