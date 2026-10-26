#pragma once
#include "games/call_of_juarez/camera_probe.hpp"
#include <cmath>

namespace cojvr::games::call_of_juarez {
inline bool BuildCoJScopeLenses(const std::array<runtime::EyeView,2>& physical,
    const CameraWeaponReticle& marker,std::array<runtime::ScopeLens,2>& lenses) noexcept {
    lenses={};if(!marker.active)return false;
    std::array<CameraProbeFrustum,2> optical{};
    float radius=10.F;
    for(unsigned eye=0;eye<2;++eye){
        const auto& p=marker.eyes[eye];
        if(!p.valid||!std::isfinite(p.u)||!std::isfinite(p.v)||p.u<=0||p.u>=1||p.v<=0||p.v>=1||
            !BuildCameraProbeFrustum(physical[eye].fov,1.F,100.F,optical[eye]))return false;
        const auto& f=optical[eye];
        // Match angular radii across both lenses, including asymmetric optics.
        // Reserve a small frame outside the largest shared fitting aperture.
        radius=std::min(radius,std::min(std::min(p.u,1.F-p.u)*(f.right-f.left),
            std::min(p.v,1.F-p.v)*(f.top-f.bottom)));
    }
    radius*=.84F;
    if(!std::isfinite(radius)||radius<=0)return false;
    for(unsigned eye=0;eye<2;++eye){
        const auto& p=marker.eyes[eye];const auto& f=optical[eye];
        lenses[eye]={true,p.u,p.v,radius/(f.right-f.left),radius/(f.top-f.bottom)};
    }
    return true;
}
// While the native scope is active, the rifle alone owns angular aim. A virtual
// scene orientation follows its verified ray, independently of head rotation.
// Physical HMD poses remain with the compositor
// and ordinary view resumes on exit. Keep physical optical principal points
// and the compensated virtual baseline; never converge the texture centres.
inline bool BuildCoJScopedRifleRenderEyes(CameraProbeBasis natural,CameraProbeVector position,
    const runtime::Pose& head,float yaw,const std::array<runtime::EyeView,2>& physical,
    const std::array<runtime::EyeView,2>& zoomed,const CameraProbeWeaponRay& ray,
    float near_plane,float far_plane,std::array<runtime::EyeView,2>& out,
    runtime::Pose& scene_head) noexcept {
    out=zoomed;scene_head=head;
    const auto& q=head.orientation;
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!ray.origin_valid||!ray.direction_valid||!ray.muzzle_verified||
        !head.position_valid||!head.orientation_valid||!std::isfinite(norm)||std::abs(norm-1.F)>.01F||
        !std::isfinite(yaw)||!IsCameraProbeBasisRigidRightHanded(natural))return false;
    const auto reference=BuildTrackingReferenceBasis(natural.forward,natural.up,yaw);
    if(!IsCameraProbeBasisRigidRightHanded(reference))return false;
    const auto dot=[](CameraProbeVector a,CameraProbeVector b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const float x_direction=dot(ray.direction,reference.right);
    const float y_direction=dot(ray.direction,reference.up);
    const float z_direction=dot(ray.direction,reference.forward);
    const float horizontal=std::hypot(x_direction,z_direction);
    if(!std::isfinite(horizontal)||!std::isfinite(y_direction)||
        std::hypot(horizontal,y_direction)<1.e-5F)return false;
    // HeadAngles and the CoJ adapter have opposite yaw signs. Construct a
    // virtual XR yaw/pitch in the tracking reference; never reuse HMD angles.
    const float half_yaw=-.5F*std::atan2(x_direction,-z_direction);
    const float half_pitch=.5F*std::atan2(y_direction,horizontal);
    const runtime::Quaternion aim_orientation{
        std::cos(half_yaw)*std::sin(half_pitch),std::sin(half_yaw)*std::cos(half_pitch),
        -std::sin(half_yaw)*std::sin(half_pitch),std::cos(half_yaw)*std::cos(half_pitch)};
    const auto basis=ApplyCameraPoseOrientation(natural.forward,natural.up,aim_orientation,yaw);
    if(!IsCameraProbeBasisRigidRightHanded(basis))return false;
    const auto head_position=ApplyCameraTrackingOffset(position,reference,head.position);
    const float distance=std::min(5000.F,far_plane*.95F);
    const runtime::Vec3 eye_centre{
        (physical[0].eye_to_head.position.x+physical[1].eye_to_head.position.x)*.5F,
        (physical[0].eye_to_head.position.y+physical[1].eye_to_head.position.y)*.5F,
        (physical[0].eye_to_head.position.z+physical[1].eye_to_head.position.z)*.5F};
    const auto central_position=ApplyCameraEyeOffset(head_position,basis,eye_centre);
    CameraProbeFrustum reference_frustum{};CameraProbeReticlePoint centre_aim{};
    if(!BuildCameraProbeFrustum(physical[0].fov,near_plane,far_plane,reference_frustum)||
        !ProjectWorldAimRayToEyeReticle(ray.origin,ray.direction,central_position,basis,
            reference_frustum,distance,centre_aim))return false;
    const float x=reference_frustum.left+(reference_frustum.right-reference_frustum.left)*centre_aim.u;
    const float y=reference_frustum.bottom+(reference_frustum.top-reference_frustum.bottom)*(1.F-centre_aim.v);
    auto candidate=zoomed;
    for(unsigned eye=0;eye<2;++eye){
        if(!physical[eye].eye_to_head.position_valid||!zoomed[eye].eye_to_head.position_valid)return false;
        const auto eye_position=ApplyCameraEyeOffset(head_position,basis,physical[eye].eye_to_head.position);
        CameraProbeFrustum optical{},magnified{};CameraProbeReticlePoint aim{};
        if(!BuildCameraProbeFrustum(physical[eye].fov,near_plane,far_plane,optical)||
            !BuildCameraProbeFrustum(zoomed[eye].fov,near_plane,far_plane,magnified)||
            !ProjectWorldAimRayToEyeReticle(ray.origin,ray.direction,eye_position,basis,optical,distance,aim))return false;
        candidate[eye].fov={std::atan((magnified.left+x)/near_plane),std::atan((magnified.right+x)/near_plane),
            std::atan((magnified.top+y)/near_plane),std::atan((magnified.bottom+y)/near_plane)};
        CameraProbeFrustum check{};
        if(!BuildCameraProbeFrustum(candidate[eye].fov,near_plane,far_plane,check))return false;
    }
    out=candidate;scene_head.orientation=aim_orientation;return true;
}
}
