#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"
#include "runtime/vr_types.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJStationaryWeaponOwner {
    bool active=false;
    std::uint32_t cannon=0;
};
// Hash-qualified caller, current owner thread, no retained JNI/native pointers.
// Inventory slots are deliberately not evidence of stationary-weapon ownership.
inline bool ReadCoJStationaryWeaponOwner(void* env,void* player,CoJStationaryWeaponOwner& out) noexcept {
    using namespace long_gun_observer_detail;
    out={};
    if(!env||!player||!*static_cast<void***>(env))return false;
    for(int slot:{15,17,23,24,31,33,36,39,51})if(!(*static_cast<void***>(env))[slot])return false;
    Reader r{env};if(!r.Check())return false;
    auto player_class=r.Class(player);
    void* using_cannon=r.Method(player_class,"IsUsingCannon","()Z");
    void* get_cannon=r.Method(player_class,"GetCannon","()LStationaryCannon;");
    if(!r.ok)return false;
    const auto get=[&](void* object,void* method) noexcept -> Reader::Local {
        void* value=r.ok?r.Fn<Object>(36)(env,object,method,nullptr):nullptr;
        r.Check();return {r,value};
    };
    const bool active=r.Bool(player,using_cannon);
    auto cannon=get(player,get_cannon);
    if(!r.ok||active!=(cannon.value!=nullptr))return false;
    if(!active){
        auto again=get(player,get_cannon);
        return r.ok&&!again.value&&!r.Bool(player,using_cannon)&&r.ok;
    }
    auto cannon_class=r.Class(cannon);
    void* id=r.Method(cannon_class,"GetThisID","()I");
    void* get_controller=r.Method(cannon_class,"GetCannon","()LICannonController;");
    void* not_alive=r.Method(player_class,"IsNotAlive","()Z");
    auto controller=get(cannon,get_controller);
    const auto identity=static_cast<std::uint32_t>(r.Int(cannon,id));
    if(!r.ok||!controller.value||!identity||identity==0xffffffffU||r.Bool(player,not_alive)||!r.ok)return false;
    auto again=get(player,get_cannon);
    auto controller_again=get(cannon,get_controller);
    if(!r.ok||!r.Equal(again,cannon)||!r.Equal(controller_again,controller)||
        !r.Bool(player,using_cannon)||!r.ok||
        static_cast<std::uint32_t>(r.Int(cannon,id))!=identity||!r.ok)return false;
    out={true,identity};return true;
}
class CoJStationaryFire {
public:
    // Called after non-fire actions and all inventory-specific fire filters.
    // Native action9 -> HumanBeing.Attack(1) owns Cannon StartFire/StopFire.
    // R2 owns that logical action here; ordinary per-hand mappings stay intact.
    bool Filter(bool known,CoJStationaryWeaponOwner owner,std::uint64_t player,
        std::uint64_t context,std::uint64_t sequence,const runtime::GameplayInputState& raw,
        runtime::GameplayInputState& mapped,bool safety_admitted=true) noexcept {
        const bool new_input=raw.input_context_generation!=generation_;
        const bool fresh=sequence&&(new_input||!sequence_||sequence>sequence_);
        const bool regression=!new_input&&sequence<sequence_;
        const bool changed=player!=player_||context!=context_||owner.cannon!=cannon_||
            raw.input_context_generation!=generation_||owner.active!=owned_;
        const bool conflict=raw.interact||raw.weapon_radial||raw.quick_save||raw.quick_load||
            raw.objectives||raw.logs||raw.weapon_next||raw.weapon_previous||raw.hands||raw.discard_weapon||
            raw.reload||raw.alternate_fire||std::any_of(raw.equipment_select.begin(),raw.equipment_select.end(),[](bool x){return x;});
        const bool available=known&&raw.active&&mapped.active&&player&&context&&
            raw.input_context_generation&&(raw.digital_available&2U)&&!conflict&&!regression&&safety_admitted;
        if(changed||!available){armed_=false;firing_=false;}
        if(fresh)sequence_=sequence;
        player_=player;context_=context;generation_=raw.input_context_generation;
        cannon_=owner.cannon;owned_=known&&owner.active;
        const bool held[]{raw.fire_left,raw.fire_right};
        for(unsigned i=0;i<2;++i){
            const bool binding=raw.active&&(raw.digital_available&(1U<<i));
            if((!known||owner.active)&&binding&&held[i])claims_[i]=true;
            if(fresh&&binding&&!held[i])claims_[i]=false;
        }
        if(!known||!safety_admitted){mapped.fire_left=mapped.fire_right=false;return true;}
        if(!owner.active){
            if(claims_[0])mapped.fire_left=false;
            if(claims_[1])mapped.fire_right=false;
            return claims_[0]||claims_[1];
        }
        mapped.fire_left=mapped.fire_right=false;
        if(available){
            if(fresh&&!raw.fire_right)armed_=true;
            if(fresh)firing_=raw.fire_right&&armed_;
            else if(!raw.fire_right)firing_=false;
            mapped.fire_left=firing_;
        }
        // Availability follows R2, not the unused physical L2 binding. Neutral
        // values still pass through Translate so an owned native shot stops.
        mapped.digital_available=(mapped.digital_available&~3U)|((raw.digital_available&2U)?1U:0U);
        return true;
    }
    // Cancel without manufacturing a release or abandoning consumed holds.
    void Interrupt() noexcept {armed_=false;firing_=false;}
private:
    std::uint64_t player_=0,context_=0,generation_=0,sequence_=0;
    std::uint32_t cannon_=0;
    bool owned_=false,armed_=false,firing_=false;
    std::array<bool,2> claims_{};
};
// Every outer render invocation must either visit the fire boundary or disarm.
// Reentrant eye renders do not create this guard. It performs no native cleanup.
class CoJStationaryFireFrame final {
    CoJStationaryFire& gate_;bool reached_=false;
public:
    explicit CoJStationaryFireFrame(CoJStationaryFire& gate) noexcept:gate_(gate){}
    CoJStationaryFireFrame(const CoJStationaryFireFrame&)=delete;
    ~CoJStationaryFireFrame(){if(!reached_)gate_.Interrupt();}
    void ReachedFireBoundary() noexcept {reached_=true;}
};
}
