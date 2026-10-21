#pragma once
#include "games/call_of_juarez/long_gun_observer.hpp"

namespace cojvr::games::call_of_juarez {
// Passive exact Winchester observation. Phase numbers are deliberately raw:
// Winchester's reload tuple and support-hand ownership require live evidence.
// No gesture admission, animation control, ammo writer or reload callback.
struct CoJWinchesterReloadSnapshot {
    bool valid=false,pose_valid=false,native_reloading=false,carrying=false,dead=false;
    std::uint32_t player_id=0,weapon_id=0;
    // Hand 0/right, 1/left; state 0/current,1/destination,2/actual,3/desired.
    std::array<std::array<int,4>,2> states{};
    // For each hand, active/actual/desired weapon occupancy and two-hand flags.
    std::array<std::array<bool,3>,2> weapon_present{},two_handed{};
    std::array<int,2> animation_id{};
    std::array<float,2> animation_time{},animation_duration{};
    // Exact Winchester -> WeaponRifle -> WeaponFireAmmo.GetAmmoCount(0) reads
    // m_iAmmo0Count. A sparse readback alone cannot prove a native transfer or
    // physical chamber occupancy.
    int ammo_readback=0,rifle_reserve=0;
    int holder_element=-1,support_element=-1;
    ElementWorldBasisTarget root{},holder{},support{};
    CoJReloadLocalFrame support_in_weapon{};
};

namespace winchester_reload_detail {
using namespace long_gun_observer_detail;
inline bool Owner(Reader& r,void* player,void* weapon,const std::array<void*,15>& methods,
    CoJWinchesterReloadSnapshot& sample) noexcept {
    sample.dead=r.Bool(player,methods[10]);
    sample.carrying=r.Bool(player,methods[11]);
    sample.native_reloading=r.Bool(player,methods[12]);
    if(!r.ok)return false;
    bool found=false;
    for(int h=0;h<2;++h){
        Value hand{.integer=h};
        for(int i=0;i<3;++i){
            auto current=r.Get(player,methods[i],h);
            if(!r.ok)return false;
            if(current.value){
                if(!r.Equal(current,weapon))return false;
                sample.weapon_present[h][i]=true;found=true;
            }
        }
        for(int i=0;i<4;++i){
            const int state=r.Int(player,methods[3+i],&hand);
            if(!r.ok||state<0||state>1024)return false;
            sample.states[h][i]=state;
        }
        for(int i=0;i<3;++i){
            sample.two_handed[h][i]=r.Bool(player,methods[7+i],&hand);
            if(!r.ok)return false;
        }
    }
    return found;
}
} // namespace winchester_reload_detail

// Game-thread only. Caller verifies exact-game SHA, current player generation,
// gameplay context and no active scoped hand/weapon overlay before frame reads.
// Without read_frame, raw owner/ammo/animation remains observable during overlays.
inline bool ReadCoJWinchesterReload(void* env,void* player,CoJWinchesterReloadSnapshot& out,
    CoJLongGunReadFrame read_frame=nullptr,void* context=nullptr) noexcept {
    using namespace winchester_reload_detail;
    out={};
    if(!env||!player||!*static_cast<void***>(env))return false;
    for(int index:{6,15,17,23,24,31,33,36,39,51,94,95,100,102,167})
        if(!(*static_cast<void***>(env))[index])return false;
    Reader r{env};if(!r.Check())return false;
    auto player_class=r.Class(player);if(!r.ok)return false;
    std::array<void*,15> methods{};
    for(std::size_t i=0;i<methods.size();++i){
        methods[i]=r.Method(player_class,Names[i],Descriptors[i]);if(!r.ok)return false;
    }
    // A reload can make the active/actual/desired slot temporarily absent.
    // Retain a single local from whichever right slot still names the rifle.
    auto active=r.Get(player,methods[0],0);
    auto actual=r.Get(player,methods[1],0);
    auto desired=r.Get(player,methods[2],0);
    if(!r.ok)return false;
    void* weapon=active.value?active.value:actual.value?actual.value:desired.value;
    if(!weapon)return false;
    auto weapon_class=r.Class(weapon);if(!r.ok)return false;
    auto exact=r.FindClass("WeaponRifleWinchester");
    if(!r.ok||!r.Equal(weapon_class,exact))return false;
    auto owner_field=r.Field(weapon_class,"cOwner","LPawnInventory;");
    if(!r.ok)return false;
    auto owner=r.ObjectField(weapon,owner_field);
    if(!r.ok||!owner.value||!r.Equal(owner,player))return false;
    CoJWinchesterReloadSnapshot value{};
    if(!Owner(r,player,weapon,methods,value))return false;
    auto player_id=r.Method(player_class,"GetThisID","()I");
    auto weapon_id=r.Method(weapon_class,"GetThisID","()I");
    if(!r.ok)return false;
    value.player_id=static_cast<std::uint32_t>(r.Int(player,player_id));
    value.weapon_id=static_cast<std::uint32_t>(r.Int(weapon,weapon_id));
    if(!r.ok||!ValidId(value.player_id)||!ValidId(value.weapon_id))return false;
    auto ammo_method=r.Method(weapon_class,"GetAmmoCount","(I)I");
    auto reserve_field=r.Field(player_class,"nAmmoRifle","I");
    if(!r.ok)return false;
    Value ammo_zero{.integer=0};
    value.ammo_readback=r.Int(weapon,ammo_method,&ammo_zero);
    value.rifle_reserve=r.IntField(player,reserve_field);
    if(!r.ok||value.ammo_readback<0||value.ammo_readback>12||value.rifle_reserve<0)return false;
    auto machine_method=r.Method(player_class,"GetHandStateMashine","(I)LPlayerStateMashine;");
    if(!r.ok)return false;
    for(int h=0;h<2;++h){
        auto machine=r.Get(player,machine_method,h);
        if(!r.ok||!machine.value)return false;
        auto cls=r.Class(machine);
        if(!r.ok)return false;
        auto anim=r.Field(cls,"m_iAnimID0","I");
        auto time=r.Field(cls,"m_fCurrentTime","F");
        auto duration=r.Field(cls,"m_fDuration","F");
        if(!r.ok)return false;
        value.animation_id[h]=r.IntField(machine,anim);
        value.animation_time[h]=r.FloatField(machine,time);
        value.animation_duration[h]=r.FloatField(machine,duration);
        if(!r.ok)return false;
    }
    // Geometry is optional: missing authored frames cannot hide the reload
    // state or native ammo delta. Never use an unverified pose as a port.
    if(read_frame){
        auto holder_id=r.Method(weapon_class,"GetElementID","(Ljava/lang/String;)I");
        auto support_id=r.Method(player_class,"GetElementID","(Ljava/lang/String;)I");
        if(!r.ok)return false;
        auto holder=r.Text("Holder");auto support=r.Text("left_hand");
        if(!r.ok)return false;
        Value holder_arg{.object=holder},support_arg{.object=support};
        value.holder_element=r.Int(weapon,holder_id,&holder_arg);
        value.support_element=r.Int(player,support_id,&support_arg);
        if(!r.ok)return false;
        if(value.holder_element>=0&&value.support_element>=0){
            const bool frames=read_frame(context,weapon,0,value.root)&&
                read_frame(context,weapon,value.holder_element,value.holder)&&
                read_frame(context,player,value.support_element,value.support);
            if(!r.Check())return false;
            CoJReloadLocalFrame holder_local{};
            value.pose_valid=frames&&BuildCoJReloadLocalFrame(value.root,value.holder,holder_local)&&
                BuildCoJReloadLocalFrame(value.root,value.support,value.support_in_weapon);
        }
    }
    CoJWinchesterReloadSnapshot after{};
    if(!Owner(r,player,weapon,methods,after)||after.states!=value.states||
        after.two_handed!=value.two_handed||after.weapon_present!=value.weapon_present||
        after.dead!=value.dead||after.carrying!=value.carrying||
        after.native_reloading!=value.native_reloading)return false;
    if(static_cast<std::uint32_t>(r.Int(player,player_id))!=value.player_id||
        static_cast<std::uint32_t>(r.Int(weapon,weapon_id))!=value.weapon_id||
        r.Int(weapon,ammo_method,&ammo_zero)!=value.ammo_readback||
        r.IntField(player,reserve_field)!=value.rifle_reserve||!r.ok)return false;
    auto owner_after=r.ObjectField(weapon,owner_field);
    if(!r.ok||!owner_after.value||!r.Equal(owner_after,player))return false;
    auto class_after=r.Class(weapon);
    if(!r.ok||!r.Equal(class_after,exact))return false;
    value.valid=true;out=value;return true;
}
} // namespace cojvr::games::call_of_juarez
