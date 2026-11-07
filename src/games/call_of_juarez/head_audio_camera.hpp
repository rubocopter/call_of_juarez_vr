#pragma once
#include "games/call_of_juarez/camera_probe.hpp"
#include <chrono>
#include <cmath>
#include <cstring>

namespace cojvr::games::call_of_juarez {
inline bool HeadAudioRequested(const CameraProbeCommand& command) noexcept {
    return command.tracking_enabled && command.head_tracked_audio_enabled;
}

struct CoJHeadAudioIdentity {
    std::uintptr_t camera=0,view=0;
    std::uint64_t player=0,context=0,recenter=0,command=0,thread=0;
    bool operator==(const CoJHeadAudioIdentity&)const noexcept=default;
};
struct CoJHeadAudioReference:CoJHeadAudioIdentity {
    std::chrono::steady_clock::time_point captured_at{};
    runtime::Vec3 position_correction{};
    std::uint64_t pose_sequence=0;
};
struct CoJHeadAudioPose {CameraProbeBasis basis{};CameraProbeVector position{};};
inline bool BuildCoJHeadAudioPose(const CoJHeadAudioReference& reference,CoJHeadAudioIdentity identity,
    const CameraHeadTrackingSample& tracking,const runtime::Pose& relative,CameraProbeBasis natural,
    CameraProbeVector origin,float owned_yaw,std::chrono::steady_clock::time_point now,
    CoJHeadAudioPose& output)noexcept {
    output={};
    const auto finite=[](auto v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto recent=[&](auto at){return at!=decltype(at){}&&at<=now&&now-at<=std::chrono::milliseconds(100);};
    const auto q=relative.orientation;
    const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    const auto absolute_q=tracking.hmd_pose.pose.orientation;
    const float absolute_norm=absolute_q.x*absolute_q.x+absolute_q.y*absolute_q.y+
        absolute_q.z*absolute_q.z+absolute_q.w*absolute_q.w;
    if(static_cast<const CoJHeadAudioIdentity&>(reference)!=identity||!identity.camera||!identity.view||
        !identity.player||!identity.context||!identity.recenter||!identity.command||!identity.thread||
        !tracking.available||tracking.input_context!=identity.context||!tracking.hmd_pose.sequence||!reference.pose_sequence||
        tracking.hmd_pose.sequence<reference.pose_sequence||!recent(tracking.captured_at)||!recent(reference.captured_at)||
        !relative.orientation_valid||!relative.position_valid||!tracking.hmd_pose.pose.orientation_valid||
        !tracking.hmd_pose.pose.position_valid||!finite(tracking.hmd_pose.pose.position)||
        !std::isfinite(absolute_norm)||absolute_norm<.99F||absolute_norm>1.01F||!finite(relative.position)||!finite(origin)||
        !finite(reference.position_correction)||!std::isfinite(norm)||norm<.99F||norm>1.01F||
        !std::isfinite(owned_yaw)||!IsCameraProbeBasisRigidRightHanded(natural))return false;
    CoJHeadAudioPose result;
    result.basis=ApplyCameraPoseOrientation(natural.forward,natural.up,q,owned_yaw);
    const auto leveled=BuildTrackingReferenceBasis(natural.forward,natural.up,owned_yaw);
    auto head=relative.position;
    head.x+=reference.position_correction.x;head.y+=reference.position_correction.y;head.z+=reference.position_correction.z;
    result.position=ApplyCameraTrackingOffset(origin,leveled,head);
    if(!finite(result.position)||!IsCameraProbeBasisRigidRightHanded(result.basis))return false;
    output=result;return true;
}

// The exact native update consumes a temporary source, producing its ordinary
// derived matrices. Restore both source matrices even during native C++ unwind.
template<class Synchronize> bool WithCoJHeadAudioSource(float* world,float* inverse,
    const CoJHeadAudioPose& pose,Synchronize&& synchronize) {
    if(!world||!inverse||world==inverse||!IsCameraProbeBasisRigidRightHanded(pose.basis)||
        !std::isfinite(pose.position.x)||!std::isfinite(pose.position.y)||!std::isfinite(pose.position.z))return false;
    struct Restore {
        float *world,*inverse;std::array<float,16> saved_world,saved_inverse;
        ~Restore(){std::memcpy(world,saved_world.data(),sizeof(saved_world));std::memcpy(inverse,saved_inverse.data(),sizeof(saved_inverse));}
    } restore{world,inverse,{},{}};
    std::memcpy(restore.saved_world.data(),world,sizeof(restore.saved_world));
    std::memcpy(restore.saved_inverse.data(),inverse,sizeof(restore.saved_inverse));
    std::memcpy(world,&pose.basis.right,sizeof(CameraProbeVector));
    std::memcpy(world+4,&pose.basis.up,sizeof(CameraProbeVector));
    std::memcpy(world+8,&pose.basis.forward,sizeof(CameraProbeVector));
    std::memcpy(world+12,&pose.position,sizeof(CameraProbeVector));
    synchronize();return true;
}
}
