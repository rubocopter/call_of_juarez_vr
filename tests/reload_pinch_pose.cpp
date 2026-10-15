#include "games/call_of_juarez/reload_pinch_pose.hpp"
#include "games/call_of_juarez/reload_insertion_pose.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;

namespace {
cojvr::runtime::Pose TrackedPose() {
    cojvr::runtime::Pose pose{};
    pose.position_valid=true;
    pose.orientation_valid=true;
    pose.orientation={0,0,0,1};
    return pose;
}

bool Same(const cojvr::runtime::FingerTrackingState& a,
          const cojvr::runtime::FingerTrackingState& b) {
    return a.available==b.available && a.quality==b.quality && a.curls==b.curls;
}

bool TestPinch(const unsigned support) {
    cojvr::runtime::FingerTrackingState original_left{}, original_right{};
    original_left.available=true;
    original_left.quality=cojvr::runtime::FingerTrackingQuality::full;
    original_left.curls={.1F,.2F,.3F,.4F,.5F};
    original_right.curls={.9F,.8F,.7F,.6F,.5F};
    auto left=original_left,right=original_right;
    CoJReloadPinchInput input{};
    input.manual_load=true;
    input.cartridge_held=true;
    input.cartridge_hand=support;
    input.claimed_hand=support;input.trigger_held=true;
    input.armed_hand=1-static_cast<int>(support);
    input.left=TrackedPose();
    input.right=TrackedPose();
    if(!ApplyCoJReloadPinch(input,left,right))return false;
    const auto& target=support==0?right:left;
    if(!target.available || target.quality!=cojvr::runtime::FingerTrackingQuality::estimated ||
       target.curls[0]>=target.curls[2] || target.curls[1]>=target.curls[3])return false;
    for(float curl:target.curls)if(!std::isfinite(curl)||curl<0||curl>1)return false;
    return support==0?Same(left,original_left):Same(right,original_right);
}

bool TestGuards() {
    CoJReloadPinchInput input{};
    input.manual_load=true;
    input.cartridge_held=true;
    input.cartridge_hand=0;
    input.claimed_hand=0;input.trigger_held=true;
    input.armed_hand=1;
    input.left=TrackedPose();input.right=TrackedPose();
    const auto unchanged=[](const CoJReloadPinchInput& test){
        cojvr::runtime::FingerTrackingState left{},right{};
        left.available=right.available=true;
        left.curls={.1F,.2F,.3F,.4F,.5F};
        right.curls={.5F,.4F,.3F,.2F,.1F};
        const auto prior_left=left,prior_right=right;
        return !ApplyCoJReloadPinch(test,left,right)&&
            Same(left,prior_left)&&Same(right,prior_right);
    };
    if(!unchanged(CoJReloadPinchInput{}))return false;
    auto rejected=input;rejected.manual_load=false;if(!unchanged(rejected))return false;
    rejected=input;rejected.cartridge_held=false;if(!unchanged(rejected))return false;
    rejected=input;rejected.cartridge_hand=2;if(!unchanged(rejected))return false;
    rejected=input;rejected.claimed_hand=2;if(!unchanged(rejected))return false;
    rejected=input;rejected.trigger_held=false;if(!unchanged(rejected))return false;
    rejected=input;rejected.armed_hand=0;if(!unchanged(rejected))return false;
    rejected=input;rejected.armed_hand=-1;if(!unchanged(rejected))return false;
    rejected=input;rejected.left.position_valid=false;if(!unchanged(rejected))return false;
    rejected=input;rejected.right.orientation_valid=false;if(!unchanged(rejected))return false;
    rejected=input;rejected.right.position.x=std::numeric_limits<float>::quiet_NaN();
    if(!unchanged(rejected))return false;
    rejected=input;rejected.left.orientation={0,0,0,0};if(!unchanged(rejected))return false;
    rejected=input;rejected.right.orientation={0,0,0,2};if(!unchanged(rejected))return false;
    // A subsequent frame can return to the original skeletal sample without a latch.
    cojvr::runtime::FingerTrackingState left{},right{};
    const auto raw_left=left,raw_right=right;
    if(!ApplyCoJReloadPinch(input,left,right))return false;
    left=raw_left;right=raw_right;
    input.cartridge_held=false;
    return !ApplyCoJReloadPinch(input,left,right)&&Same(left,raw_left)&&
        right.available&&right.quality==cojvr::runtime::FingerTrackingQuality::estimated&&
        right.curls==std::array<float,5>{};
}

bool TestInsertionPose() {
    for(int hand=0;hand<2;++hand){
        CoJReloadInsertionPose pulse;
        CoJReloadInsertionPoseInput input{};
        input.allowed=true;input.player=1;input.context=2;input.weapon=42;
        input.input_generation=3;input.sequence=9;input.now=1000;
        input.armed_hand=1-hand;input.left=input.right=TrackedPose();
        cojvr::runtime::FingerTrackingState left{},right{};
        left.available=right.available=true;left.curls.fill(.1F);right.curls.fill(.2F);
        const auto raw_left=left,raw_right=right;
        if(pulse.Apply(input,left,right)!=-1)return false; // Intent alone cannot animate.
        input.sequence=10;input.insertion_event=10;
        if(pulse.Apply(input,left,right)!=hand)return false;
        if((hand==0?right:left).curls[0]!=.7F)return false;
        input.insertion_event=0;input.sequence++;input.now+=50;
        left=raw_left;right=raw_right;
        if(pulse.Apply(input,left,right)!=hand||
           std::fabs((hand==0?right:left).curls[0]-.75F)>.0001F||
           !Same(hand==0?left:right,hand==0?raw_left:raw_right))return false;
        input.sequence++;input.now+=80;left=raw_left;right=raw_right;
        if(pulse.Apply(input,left,right)!=hand||
           (hand==0?right:left).curls[0]>=.7F)return false;
        input.sequence++;input.now=1180;left=raw_left;right=raw_right;
        if(pulse.Apply(input,left,right)!=-1||!Same(left,raw_left)||!Same(right,raw_right))return false;
        input.insertion_event=10;input.sequence++;input.now++;
        if(pulse.Apply(input,left,right)!=-1)return false; // Duplicate never restarts.
    }
    for(int fault=0;fault<15;++fault){
        CoJReloadInsertionPose pulse;
        CoJReloadInsertionPoseInput input{};
        input.allowed=true;input.player=1;input.context=2;input.weapon=42;
        input.input_generation=3;input.sequence=10;input.insertion_event=10;input.now=1000;
        input.armed_hand=0;input.left=input.right=TrackedPose();
        cojvr::runtime::FingerTrackingState left{},right{};
        if(pulse.Apply(input,left,right)!=1)return false;
        input.insertion_event=0;input.sequence++;input.now+=20;
        if(fault==0)input.allowed=false;
        if(fault==1)input.recentered=true;
        if(fault==2)input.player++;
        if(fault==3)input.context++;
        if(fault==4)input.weapon++;
        if(fault==5)input.input_generation++;
        if(fault==6)input.armed_hand=1;
        if(fault==7)input.left.position_valid=false;
        if(fault==8)input.right.orientation={0,0,0,0};
        if(fault==9)input.cartridge_held=true;
        if(fault==10)input.support_trigger_held=true;
        if(fault==11)input.sequence=9;
        if(fault==12)input.now=999;
        if(fault==13)input.now=1251;
        if(fault==14)input.left.position.x=std::numeric_limits<float>::quiet_NaN();
        left={};right={};
        if(pulse.Apply(input,left,right)!=-1||left.available||right.available)return false;
        input.allowed=true;input.recentered=false;input.player=1;input.context=2;input.weapon=42;
        input.input_generation=3;input.armed_hand=0;input.left=input.right=TrackedPose();
        input.cartridge_held=input.support_trigger_held=false;input.now=1300;input.sequence=12;
        if(pulse.Apply(input,left,right)!=-1)return false; // Recovery cannot replay.
    }
    // A successful rigid-hand map can still reject scoped finger application.
    // Losing that native eligibility cancels the old pulse permanently.
    CoJReloadInsertionPose pulse;
    CoJReloadInsertionPoseInput input{};
    input.allowed=true;input.player=1;input.context=2;input.weapon=42;
    input.input_generation=3;input.sequence=10;input.insertion_event=10;input.now=1000;
    input.armed_hand=0;input.left=input.right=TrackedPose();
    cojvr::runtime::FingerTrackingState left{},right{};
    if(pulse.Apply(input,left,right)!=1)return false;
    pulse.Cancel();
    input.insertion_event=0;input.sequence++;input.now+=20;left={};right={};
    if(pulse.Apply(input,left,right)!=-1||left.available||right.available)return false;
    input.insertion_event=10;input.sequence++;input.now+=20;
    if(pulse.Apply(input,left,right)!=-1)return false;
    input.insertion_event=12;input.sequence++;input.now+=20;
    if(pulse.Apply(input,left,right)!=1)return false;
    return true;
}
}

int main() {
    CoJMotionReloadOwner owner{0,42,true,true};
    CoJReloadTraceSnapshot native{};native.valid=native.probe_valid=true;
    native.armed_hand=0;native.weapon_id=42;native.probe_status=2;
    if(!CoJManualSupportFingersAllowed(true,1,owner,native)||
       CoJManualSupportFingersAllowed(false,1,owner,native)||
       CoJManualSupportFingersAllowed(true,0,owner,native))return 1;
    for(int fault=0;fault<7;++fault){auto o=owner;auto n=native;
        if(fault==0)o.valid=false;if(fault==1)o.weapon_id++;
        if(fault==2)o.armed_hand=1;if(fault==3)o.single_round_supported=false;
        if(fault==4)n.probe_valid=false;if(fault==5)n.valid=false;if(fault==6)n.probe_status=3;
        if(CoJManualSupportFingersAllowed(true,1,o,n))return 1;
    }
    CoJReloadPinchInput waiting{};
    waiting.manual_load=true;waiting.armed_hand=0;
    waiting.left=TrackedPose();waiting.right=TrackedPose();
    cojvr::runtime::FingerTrackingState free{},armed{};
    (void)ApplyCoJReloadPinch(waiting,free,armed);
    if(!free.available||free.quality!=cojvr::runtime::FingerTrackingQuality::estimated||
       free.curls!=std::array<float,5>{}||armed.available){
        std::cerr<<"Empty manual support must restore resting fingers instead of retaining native reload pinch\n";
        return 1;
    }
    if(!TestPinch(0)||!TestPinch(1)||!TestGuards()||!TestInsertionPose()){
        std::cerr<<"Reload pinch selection or tracking guard failed\n";
        return 1;
    }
    return 0;
}
