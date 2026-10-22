#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"
#include "runtime/vr_types.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJLongGunFireRouting {bool block_left=false,block_right=false;};
// Attack(hand, pressed) retries the other hand when the requested one cannot
// attack. An empty hand must not fire the sole exact rifle in the other hand.
// This read needs no grip geometry, native idle state or ammo mutation, and
// grants no support-grip, optic or manual-reload admission to either hand.
inline bool ReadCoJLongGunFireOwnership(void* env,void* player,CoJLongGunFireRouting& routing) noexcept {
    using namespace long_gun_observer_detail;
    routing={};
    if(!env||!player||!*static_cast<void***>(env))return false;
    for(int i:{6,15,17,23,24,31,33,36})if(!(*static_cast<void***>(env))[i])return false;
    Reader r{env};if(!r.Check())return false;
    auto cls=r.Class(player);if(!r.ok)return false;
    std::array<void*,3> methods{};
    for(int i=0;i<3;++i){methods[i]=r.Method(cls,Names[i],Descriptors[i]);if(!r.ok)return false;}
    // Retain and bracket every active/actual/desired slot before either a
    // positive or negative verdict. An old active left pistol is not proof
    // that the current actual/desired left hand can consume its own attack.
    std::array<Reader::Local,6> owners{
        r.Get(player,methods[0],0),r.Get(player,methods[1],0),r.Get(player,methods[2],0),
        r.Get(player,methods[0],1),r.Get(player,methods[1],1),r.Get(player,methods[2],1)};
    if(!r.ok)return false;
    for(int h=0;h<2;++h){
        for(int i=1;i<3;++i)if(!r.Equal(owners[h*3],owners[h*3+i]))return false;
    }
    bool rifle=false;
    const bool right=owners[0].value!=nullptr,left=owners[3].value!=nullptr;
    if(right!=left){
        auto weapon_class=r.Class(owners[right?0:3]);if(!r.ok)return false;
        for(const char* name:Classes){
            auto exact=r.FindClass(name);if(!r.ok)return false;
            if(r.Equal(weapon_class,exact)){rifle=true;break;}
            if(!r.ok)return false;
        }
    }
    // Classification may resolve classes; bracket its whole read with the
    // retained tuples as well, including negative/occupied-hand verdicts.
    for(int h=0;h<2;++h)for(int i=0;i<3;++i){
        auto again=r.Get(player,methods[i],h);
        if(!r.ok||!r.Equal(again,owners[h*3+i]))return false;
    }
    routing={rifle&&right,rifle&&left};return true;
}
class CoJLongGunFreeTrigger final {
public:
    // Feed both physical triggers before any consumer clears them. Claims and
    // releases remain independent across weapon/context/source loss. Repeated
    // poses, unavailable input and focus loss cannot manufacture a release.
    void Filter(const runtime::GameplayInputState& raw,std::uint64_t sequence,CoJLongGunFireRouting blocked,
        runtime::GameplayInputState& mapped) noexcept {
        const bool fresh=raw.input_context_generation&&sequence&&
            (generation_!=raw.input_context_generation||sequence>sequence_);
        if(fresh){generation_=raw.input_context_generation;sequence_=sequence;}
        const bool blocks[]{blocked.block_left,blocked.block_right};
        constexpr bool runtime::GameplayInputState::* triggers[]{
            &runtime::GameplayInputState::fire_left,&runtime::GameplayInputState::fire_right};
        for(unsigned i=0;i<2;++i){
            const auto bit=1U<<i;
            const bool available=raw.active&&(raw.digital_available&bit)!=0;
            const bool held=raw.*triggers[i];
            if(blocks[i]&&available&&held)consumed_[i]=true;
            const bool suppress=blocks[i]||consumed_[i];
            if(fresh&&available&&!held)consumed_[i]=false;
            if(suppress){mapped.*triggers[i]=false;mapped.digital_available&=~bit;}
        }
    }
private:
    std::array<bool,2> consumed_{};
    std::uint64_t generation_=0,sequence_=0;
};
}
