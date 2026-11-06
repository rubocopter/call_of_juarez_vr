#include "games/call_of_juarez/head_audio_camera.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace cojvr::games::call_of_juarez;
namespace runtime=cojvr::runtime;
int main() {
    using Clock=std::chrono::steady_clock;
    const auto now=Clock::now();
    CoJHeadAudioReference reference;
    reference.camera=1;reference.view=2;reference.player=3;reference.context=4;
    reference.recenter=5;reference.command=6;reference.thread=7;
    reference.captured_at=now;reference.position_correction={0,-.1F,0};
    reference.pose_sequence=10;
    CoJHeadAudioIdentity identity{1,2,3,4,5,6,7};
    CameraHeadTrackingSample tracking;
    tracking.available=true;tracking.input_context=4;tracking.captured_at=now;
    tracking.hmd_pose.sequence=10;
    tracking.hmd_pose.pose.orientation_valid=tracking.hmd_pose.pose.position_valid=true;
    tracking.hmd_pose.pose.position={.2F,.3F,-.4F};
    runtime::Pose relative=tracking.hmd_pose.pose;
    CameraProbeBasis basis{{0,0,1},{0,1,0},{1,0,0}};
    CoJHeadAudioPose pose;
    if(!BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{100,200,300},0,now,pose) ||
        std::fabs(pose.position.x-120)>1e-3F ||std::fabs(pose.position.y-220)>1e-3F ||
        std::fabs(pose.position.z-260)>1e-3F) {
        std::cerr<<"head audio did not use the center-head translation and height correction once\n";return 1;
    }
    auto bad=identity;bad.player++;
    if(BuildCoJHeadAudioPose(reference,bad,tracking,relative,basis,{},0,now,pose))return 2;
    for(unsigned i=0;i<7;++i){auto changed=identity;
        switch(i){case 0:++changed.camera;break;case 1:++changed.view;break;case 2:++changed.player;break;
            case 3:++changed.context;break;case 4:++changed.recenter;break;case 5:++changed.command;break;
            case 6:++changed.thread;break;}
        if(BuildCoJHeadAudioPose(reference,changed,tracking,relative,basis,{},0,now,pose))return 3;
    }
    const auto valid_tracking=tracking;
    tracking.available=false;
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 4;
    tracking=valid_tracking;tracking.captured_at=now-std::chrono::milliseconds(101);
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 5;
    tracking=valid_tracking;tracking.captured_at=now+std::chrono::milliseconds(1);
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 6;
    tracking=valid_tracking;relative.position.x=std::numeric_limits<float>::quiet_NaN();
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 7;
    relative=tracking.hmd_pose.pose;
    tracking.hmd_pose.pose.orientation.w=std::numeric_limits<float>::quiet_NaN();
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 13;
    tracking=valid_tracking;
    auto old_reference=reference;old_reference.captured_at=now-std::chrono::milliseconds(101);
    if(BuildCoJHeadAudioPose(old_reference,identity,tracking,relative,basis,{},0,now,pose))return 14;
    tracking.hmd_pose.sequence=9;
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 15;
    tracking=valid_tracking;
    auto reflected=basis;reflected.right.x=-1;
    if(BuildCoJHeadAudioPose(reference,identity,tracking,relative,reflected,{},0,now,pose))return 8;
    if(!BuildCoJHeadAudioPose(reference,identity,tracking,relative,basis,{},0,now,pose))return 9;
    std::array<float,32> sources{};
    sources[0]=sources[5]=sources[10]=sources[15]=1;
    sources[16]=sources[21]=sources[26]=sources[31]=1;
    const auto original=sources;
    bool synchronized=false;
    if(!WithCoJHeadAudioSource(sources.data()+16,sources.data(),pose,[&]{
        synchronized=true;
        if(sources[28]!=pose.position.x||sources[29]!=pose.position.y||sources[30]!=pose.position.z)
            throw std::runtime_error("source center mismatch");
        sources[0]=99; // Simulates the native inverse calculation.
    })||!synchronized||sources!=original)return 10;
    try {WithCoJHeadAudioSource(sources.data()+16,sources.data(),pose,[&]{
        sources[0]=99;throw std::runtime_error("native unwind");
    });return 11;}catch(const std::runtime_error&){}
    if(sources!=original)return 12;
    std::cout<<"PASS - native head audio identity/age/center and scoped source restoration\n";
}
