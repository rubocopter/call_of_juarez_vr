#pragma once
#include "games/call_of_juarez/long_gun_fire.hpp"
#include <algorithm>

namespace cojvr::games::call_of_juarez {
// WpnLogicRifle.GetAttackMode returns -2 (aim) for button0. A stable scoped
// Winchester needs button1 through native action9, whose native other-hand
// retry selects the right rifle. This grants no attack/ammo/state permission.
class CoJScopedRifleFire final {
public:
    void Interrupt() noexcept { armed_=false;firing_=false; }
    void Filter(bool known,CoJLongGunFireRouting owner,std::uint64_t player,
        std::uint64_t context,std::uint64_t sequence,const runtime::GameplayInputState& raw,
        runtime::GameplayInputState& mapped,bool safe=true) noexcept {
        const bool generation=raw.input_context_generation!=generation_;
        const bool fresh=sequence&&(generation||sequence>sequence_);
        const bool regression=!generation&&sequence<sequence_;
        const bool changed=generation||owner.scoped_right_weapon!=weapon_||player!=player_||context!=context_;
        // L1 held by the support grip has already consumed world Action.
        // Only the remaining mapped Action conflicts with firing.
        const bool conflict=raw.reload||mapped.interact||raw.weapon_radial||raw.alternate_fire||
            raw.quick_save||raw.quick_load||raw.weapon_next||raw.weapon_previous||raw.hands||raw.discard_weapon||
            std::any_of(raw.equipment_select.begin(),raw.equipment_select.end(),[](bool b){return b;});
        const bool available=known&&safe&&raw.active&&mapped.active&&raw.input_context_generation&&
            player&&context&&(context&1U)&&sequence&&!regression&&!conflict&&(raw.digital_available&2U);
        if(changed||!available)Interrupt();
        if(fresh){generation_=raw.input_context_generation;sequence_=sequence;}
        const bool was_scoped=weapon_!=0;
        weapon_=owner.scoped_right_weapon;player_=player;context_=context;
        // Retain claims through unknown ownership, focus loss and replacement.
        // Only a fresh, available physical release retires a consumed hold.
        const bool held[]{raw.fire_left,raw.fire_right};
        for(unsigned i=0;i<2;++i){
            const bool binding=raw.active&&(raw.digital_available&(1U<<i));
            if((!known||weapon_||was_scoped)&&binding&&held[i])claimed_[i]=true;
            if(fresh&&!regression&&binding&&!held[i])claimed_[i]=false;
        }
        if(!known){mapped.fire_left=mapped.fire_right=false;return;}
        if(!weapon_){
            if(claimed_[0])mapped.fire_left=false;
            if(claimed_[1])mapped.fire_right=false;
            return;
        }
        const bool permitted=!raw.fire_right||mapped.fire_right;
        mapped.fire_left=mapped.fire_right=false;
        if(available&&permitted){
            if(fresh&&!raw.fire_right)armed_=true;
            if(fresh)firing_=armed_&&raw.fire_right;
            else if(!raw.fire_right)firing_=false;
            mapped.fire_left=firing_;
        }else Interrupt();
        mapped.digital_available=(mapped.digital_available&~3U)|((raw.digital_available&2U)?1U:0U);
    }
private:
    std::uint64_t player_=0,context_=0,generation_=0,sequence_=0;
    std::uint32_t weapon_=0;
    bool armed_=false,firing_=false;
    std::array<bool,2> claimed_{};
};
class CoJScopedRifleFireFrame final {
    CoJScopedRifleFire& fire_;bool reached_=false;
public:
    explicit CoJScopedRifleFireFrame(CoJScopedRifleFire& fire) noexcept:fire_(fire){}
    ~CoJScopedRifleFireFrame(){if(!reached_)fire_.Interrupt();}
    void ReachedFireBoundary() noexcept {reached_=true;}
};
}
