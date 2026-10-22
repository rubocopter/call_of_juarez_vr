#pragma once
#include "games/call_of_juarez/indexed_reload_delta.hpp"
#include "games/call_of_juarez/long_gun_observer.hpp"

namespace cojvr::games::call_of_juarez {
// Shared preflight for bridge discovery and full observation. Do not call a
// partial JNI table before the observer gets a chance to reject it.
inline bool CoJIndexedReloadApiAvailable(void* env) noexcept {
    if(!env||!*static_cast<void***>(env))return false;
    for(int index:{6,15,17,23,24,31,33,36,51,94,95,100})
        if(!(*static_cast<void***>(env))[index])return false;
    return true;
}
namespace indexed_reload_detail {
using long_gun_observer_detail::Reader;
using long_gun_observer_detail::Value;

inline bool ReadOwner(Reader& r,void* player,void* weapon,
    const std::array<void*,3>& methods,CoJIndexedReloadSnapshot& sample) noexcept {
    for(int hand=0;hand<2;++hand)for(std::size_t i=0;i<3;++i){
        auto current=r.Get(player,methods[i],hand);
        if(!r.ok)return false;
        if(current.value&&!r.Equal(current,weapon))return false;
        sample.weapon_present[hand][i]=current.value!=nullptr;
    }
    return OwnerShape(sample,GetProfile(sample.weapon_class));
}
inline bool OwnerMatches(Reader& r,void* player,void* weapon,void* owner_field) noexcept {
    auto owner=r.ObjectField(weapon,owner_field);
    return r.ok&&owner.value&&r.Equal(owner,player);
}
inline bool ReadCounts(Reader& r,void* player,void* weapon,void* ammo_method,
    const std::array<void*,3>& reserve_fields,CoJIndexedReloadSnapshot& sample) noexcept {
    const auto profile=GetProfile(sample.weapon_class);
    for(int index=0;index<2;++index){
        Value arg{.integer=index};
        sample.loaded[index]=r.Int(weapon,ammo_method,&arg);
        if(!r.ok||sample.loaded[index]<0||sample.loaded[index]>profile.capacity[index])return false;
    }
    for(std::size_t pool=0;pool<3;++pool)if(reserve_fields[pool]){
        sample.reserves[pool]=r.IntField(player,reserve_fields[pool]);
        // No upper reserve bound is invented: the static contract supplies no
        // qualified current maximum. Nonnegative jint values remain observable.
        if(!r.ok||sample.reserves[pool]<0)return false;
    }
    return true;
}
} // namespace indexed_reload_detail

// Bounded, read-only, owner-thread observation of the pinned original archive.
// Caller admits exact build hashes and current gameplay/player generation,
// excludes retained/faulted ownership overlays and rechecks its native context
// before publishing. This reader cannot validate external native context stamps.
//
// player and expected_retained_weapon are caller-retained JNI references, valid
// for this entire call. A local scoped to ONE poll is sufficient; this function
// creates/deletes only temporary JNI locals and acquires no persistent/native
// lease. Each nonnull active/actual/desired slot in BOTH reads must IsSameObject
// the expected weapon, not merely expose the same numeric GetThisID. cOwner,
// class, IDs, presence and both loaded/typed-reserve vectors must also be stable.
// Partial selected-hand tuples are observable with an empty other hand. Shared
// both-hand tuples require an exact long gun and all three slots in both hands.
// This is an ownership consistency rule, not state/phase/manual admission.
//
// Passive diagnostics may set object_epoch=synchronous_scope=0. For paired
// classification the caller separately holds one player/weapon identity lease
// across BOTH reads and the intervening synchronous operation; it authors a
// nonzero object_epoch from that lease plus a fresh synchronous_scope. Numeric
// ID equality between polls does not establish that lease or epoch. Never infer
// an exact transfer from sparse polls, even if their count deltas happen to fit.
//
// No parameter factories, reload/action/animation/sound calls, setters, native
// geometry, counter objects or retained handles. A failed read clears out and
// drains JNI exceptions/locals through the existing Reader. Repeated reads are
// sampled consistency, not an atomic transaction or detection of unsampled ABA.
inline bool ReadCoJIndexedReload(void* env,void* player,void* expected_retained_weapon,
    int armed_hand,const CoJIndexedReloadContext& context,CoJIndexedReloadSnapshot& out) noexcept {
    using namespace indexed_reload_detail;
    out={};
    if(!env||!player||!expected_retained_weapon||armed_hand<0||armed_hand>1||
        !ValidContext(context)||!CoJIndexedReloadApiAvailable(env))return false;
    Reader r{env};if(!r.Check())return false;
    auto player_class=r.Class(player);if(!r.ok)return false;
    auto weapon_class=r.Class(expected_retained_weapon);if(!r.ok)return false;
    CoJIndexedReloadSnapshot sample{};
    sample.armed_hand=armed_hand;sample.context=context;
    for(auto kind:{CoJIndexedReloadClass::remington,CoJIndexedReloadClass::saw_off,
        CoJIndexedReloadClass::derringer,CoJIndexedReloadClass::lemant}){
        auto exact=r.FindClass(GetProfile(kind).name);if(!r.ok)return false;
        if(r.Equal(weapon_class,exact)){sample.weapon_class=kind;break;}
        if(!r.ok)return false;
    }
    const auto profile=GetProfile(sample.weapon_class);if(!profile.name)return false;
    sample.reserve_kind=profile.pools;
    std::array<void*,3> methods{};
    constexpr const char* names[]{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon"};
    for(std::size_t i=0;i<3;++i){
        methods[i]=r.Method(player_class,names[i],"(I)LWeapon;");if(!r.ok)return false;
    }
    auto owner_field=r.Field(weapon_class,"cOwner","LPawnInventory;");if(!r.ok)return false;
    auto player_id=r.Method(player_class,"GetThisID","()I");if(!r.ok)return false;
    auto weapon_id=r.Method(weapon_class,"GetThisID","()I");if(!r.ok)return false;
    auto ammo_method=r.Method(weapon_class,"GetAmmoCount","(I)I");if(!r.ok)return false;
    std::array<void*,3> reserve_fields{};
    constexpr const char* fields[]{"nAmmoPistol","nAmmoRifle","nAmmoShotgun"};
    for(auto kind:profile.pools){
        const auto pool=static_cast<std::size_t>(kind);
        if(!reserve_fields[pool]){
            reserve_fields[pool]=r.Field(player_class,fields[pool],"I");if(!r.ok)return false;
        }
    }
    if(!OwnerMatches(r,player,expected_retained_weapon,owner_field)||
        !ReadOwner(r,player,expected_retained_weapon,methods,sample))return false;
    sample.player_id=static_cast<std::uint32_t>(r.Int(player,player_id));
    sample.weapon_id=static_cast<std::uint32_t>(r.Int(expected_retained_weapon,weapon_id));
    if(!r.ok||!ValidId(sample.player_id)||!ValidId(sample.weapon_id)||
        !ReadCounts(r,player,expected_retained_weapon,ammo_method,reserve_fields,sample))return false;
    CoJIndexedReloadSnapshot after{};
    after.weapon_class=sample.weapon_class;after.armed_hand=armed_hand;
    if(!ReadOwner(r,player,expected_retained_weapon,methods,after)||
        after.weapon_present!=sample.weapon_present||
        !ReadCounts(r,player,expected_retained_weapon,ammo_method,reserve_fields,after)||
        after.loaded!=sample.loaded||after.reserves!=sample.reserves)return false;
    if(static_cast<std::uint32_t>(r.Int(player,player_id))!=sample.player_id||
        static_cast<std::uint32_t>(r.Int(expected_retained_weapon,weapon_id))!=sample.weapon_id||!r.ok)return false;
    auto class_after=r.Class(expected_retained_weapon);
    if(!r.ok||!r.Equal(class_after,weapon_class)||
        !OwnerMatches(r,player,expected_retained_weapon,owner_field))return false;
    sample.valid=true;
    if(!ValidSnapshot(sample))return false;
    out=sample;return true;
}
} // namespace cojvr::games::call_of_juarez
