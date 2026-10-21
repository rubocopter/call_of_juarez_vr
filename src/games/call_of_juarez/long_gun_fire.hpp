#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"
#include "runtime/vr_types.hpp"

namespace cojvr::games::call_of_juarez {
// Attack(1, pressed) retries hand 0 when hand 1 cannot attack. Suppress that
// desktop fallback only for the exact right-owned rifles and an empty left
// hand. This read needs no grip geometry, native idle state or ammo mutation.
inline bool ReadCoJLongGunFireOwnership(void* env,void* player,bool& right_only) noexcept {
    using namespace long_gun_observer_detail;
    right_only=false;
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
        for(int i=0;i<3;++i){
            auto again=r.Get(player,methods[i],h);
            if(!r.ok||!r.Equal(again,owners[h*3+i]))return false;
        }
    }
    if(!owners[0].value)return true;
    auto weapon_class=r.Class(owners[0]);if(!r.ok)return false;
    bool rifle=false;
    for(const char* name:Classes){
        auto exact=r.FindClass(name);if(!r.ok)return false;
        if(r.Equal(weapon_class,exact)){rifle=true;break;}
        if(!r.ok)return false;
    }
    if(!rifle)return true; // Pistols, bow and other native owners keep their route.
    right_only=!owners[3].value;return true;
}
class CoJLongGunFreeTrigger final {
public:
    // Feed physical input before any consumer clears L2. A consumed hold and
    // its release belong here even after weapon/context loss. Repeated poses,
    // unavailable input and focus loss cannot manufacture a physical release.
    void Filter(const runtime::GameplayInputState& raw,std::uint64_t sequence,bool blocked,
        runtime::GameplayInputState& mapped) noexcept {
        const bool fresh=raw.input_context_generation&&sequence&&
            (generation_!=raw.input_context_generation||sequence>sequence_);
        if(fresh){generation_=raw.input_context_generation;sequence_=sequence;}
        const bool available=raw.active&&(raw.digital_available&1U)!=0;
        if(blocked&&available&&raw.fire_left)consumed_=true;
        const bool suppress=blocked||consumed_;
        if(fresh&&available&&!raw.fire_left)consumed_=false;
        if(suppress){mapped.fire_left=false;mapped.digital_available&=~1U;}
    }
private:
    bool consumed_=false;
    std::uint64_t generation_=0,sequence_=0;
};
}
