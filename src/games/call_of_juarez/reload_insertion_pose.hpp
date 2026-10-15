#pragma once
#include "games/call_of_juarez/reload_pinch_pose.hpp"
#include <algorithm>
#include <cstdint>

namespace cojvr::games::call_of_juarez {

// Presentation only: insertion_event is supplied solely after the native
// one-round ammo/reserve transfer is observed. No mechanics or native calls.
struct CoJReloadInsertionPoseInput {
    bool allowed=false,recentered=false,cartridge_held=false,support_trigger_held=false;
    std::uint64_t player=0,context=0,weapon=0,input_generation=0;
    std::uint64_t sequence=0,now=0,insertion_event=0;
    int armed_hand=-1;
    runtime::Pose left{},right{};
};

class CoJReloadInsertionPose {
public:
    void Cancel() noexcept {active_=false;}
    [[nodiscard]] int Apply(const CoJReloadInsertionPoseInput& s,
        runtime::FingerTrackingState& left,runtime::FingerTrackingState& right) noexcept {
        const bool regression=observed_&&(s.sequence<sequence_||s.now<now_);
        const bool gap=observed_&&s.now>=fresh_at_&&s.now-fresh_at_>250;
        const bool fresh=!observed_||s.sequence>sequence_;
        const bool same=s.player==player_&&s.context==context_&&s.weapon==weapon_&&
            s.input_generation==generation_&&s.armed_hand==armed_hand_;
        const bool valid=s.allowed&&!s.recentered&&!s.cartridge_held&&!s.support_trigger_held&&
            s.player!=0&&s.context!=0&&s.weapon!=0&&(s.armed_hand==0||s.armed_hand==1)&&
            CoJReloadPoseTracked(s.left)&&CoJReloadPoseTracked(s.right)&&!regression&&!gap;
        if(!valid||!same)active_=false;
        // Consume even rejected events. Context recovery cannot replay a push.
        const bool new_event=s.insertion_event!=0&&s.insertion_event>last_event_;
        last_event_=std::max(last_event_,s.insertion_event);
        if(valid&&fresh&&new_event){
            player_=s.player;context_=s.context;weapon_=s.weapon;
            generation_=s.input_generation;armed_hand_=s.armed_hand;
            started_=s.now;active_=true;
        }
        if(!regression){
            now_=s.now;
            if(fresh){observed_=true;sequence_=s.sequence;fresh_at_=s.now;}
        }
        if(!active_)return -1;
        const auto age=s.now-started_;
        if(age>=180){active_=false;return -1;}
        const int hand=1-armed_hand_;
        auto& target=hand==0?right:left;
        std::array<float,5> rest{};
        if(target.available){
            for(const auto curl:target.curls)
                if(!std::isfinite(curl)||curl<0.F||curl>1.F){active_=false;return -1;}
            rest=target.curls;
        }
        // .7 -> .75 moves the measured Ray thumb pad 1.45 mm (left) / 2.71 mm
        // (right). Keep the insertion sample's pinch, push for 50 ms, then
        // release to this frame's ordinary fingers over the remaining 130 ms.
        const float press=std::min(static_cast<float>(age)/50.F,1.F);
        const float release=age<=50?0.F:static_cast<float>(age-50)/130.F;
        const std::array<float,5> pressed={.7F+.05F*press,.7F,.88F,.88F,.82F};
        target.available=true;target.quality=runtime::FingerTrackingQuality::estimated;
        for(unsigned i=0;i<5;++i)target.curls[i]=pressed[i]+(rest[i]-pressed[i])*release;
        return hand;
    }
private:
    bool active_=false,observed_=false;
    std::uint64_t player_=0,context_=0,weapon_=0,generation_=0,last_event_=0;
    std::uint64_t sequence_=0,now_=0,fresh_at_=0,started_=0;
    int armed_hand_=-1;
};
}
