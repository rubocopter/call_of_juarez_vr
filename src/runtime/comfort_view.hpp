#pragma once
#include "runtime/vr_math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cojvr::runtime {
// An infinity-depth comfort aperture in head space. Corresponding head rays
// receive the same attenuation, independent of eye baseline, FOV or cant.
struct ComfortEyeProjection {
    std::array<float,4> tangents{}; // left, up, right-left, down-up; top-left UVs.
    Quaternion eye_to_head{};
    float inverse_radius = 0;
};
[[nodiscard]] inline bool BuildComfortEyeProjections(const std::array<EyeView,2>& eyes,
    std::array<ComfortEyeProjection,2>& result) noexcept {
    result={};
    std::array<ComfortEyeProjection,2> candidate{};
    float radius=std::numeric_limits<float>::max();
    for(unsigned eye=0;eye<2;++eye) {
        const auto& view=eyes[eye];const auto& q=view.eye_to_head.orientation;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(view.eye!=(eye?Eye::right:Eye::left)||!view.eye_to_head.orientation_valid||
            !std::isfinite(norm)||std::abs(norm-1.F)>.01F||!IsValidEyeFov(view.fov))return false;
        const float left=std::tan(view.fov.angle_left),right=std::tan(view.fov.angle_right);
        const float up=std::tan(view.fov.angle_up),down=std::tan(view.fov.angle_down);
        candidate[eye].tangents={left,up,right-left,down-up};
        candidate[eye].eye_to_head=NormalizeQuaternion(q);
        // Inward frustum-plane normals. The largest cone about head -Z that
        // fits both eyes has radius min(-normal.z / length(normal.xy)).
        for(const auto plane:{Vec3{1,0,left},Vec3{-1,0,-right},Vec3{0,-1,-up},Vec3{0,1,down}}) {
            const auto n=RotateVector(candidate[eye].eye_to_head,plane);
            const float transverse=std::hypot(n.x,n.y);
            if(!std::isfinite(transverse)||!std::isfinite(n.z)||n.z>=0)return false;
            if(transverse>0)radius=std::min(radius,-n.z/transverse);
        }
    }
    if(!std::isfinite(radius)||radius<=1.e-6F)return false;
    for(auto& eye:candidate)eye.inverse_radius=1.F/radius;
    result=candidate;return true;
}
}
