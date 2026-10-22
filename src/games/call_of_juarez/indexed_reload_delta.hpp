#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
enum class CoJIndexedReloadClass : std::uint8_t {unknown,remington,saw_off,derringer,lemant};
// Fixed positions in the reserve vector. Uninvolved pools are unobserved and
// must remain zero; reserve_kind maps each loaded index to one of these pools.
enum class CoJIndexedReloadReserve : std::uint8_t {pistol,rifle,shotgun,unknown};

struct CoJIndexedReloadContext {
    std::uint64_t player_generation=0,native_context=0,object_epoch=0,synchronous_scope=0;
    bool operator==(const CoJIndexedReloadContext&) const noexcept=default;
};

// No Java/native pointers escape. IDs preserve all jint bits, including LAA.
// Presence order: hand 0/right,1/left; active/actual/desired. Every nonempty
// reference was compared with the retained expected weapon via IsSameObject.
struct CoJIndexedReloadSnapshot {
    bool valid=false;
    CoJIndexedReloadClass weapon_class=CoJIndexedReloadClass::unknown;
    std::uint32_t player_id=0,weapon_id=0;
    int armed_hand=-1;
    CoJIndexedReloadContext context{};
    std::array<std::int32_t,2> loaded{};
    std::array<CoJIndexedReloadReserve,2> reserve_kind{
        CoJIndexedReloadReserve::unknown,CoJIndexedReloadReserve::unknown};
    std::array<std::int32_t,3> reserves{}; // pistol, rifle, shotgun; shared pool once.
    std::array<std::array<bool,3>,2> weapon_present{};
};

namespace indexed_reload_detail {
struct Profile {
    const char* name=nullptr;
    std::array<std::int32_t,2> capacity{};
    std::array<CoJIndexedReloadReserve,2> pools{};
    bool whole_clip=false,both_hands=false;
};
// Static observation profiles, NOT a manual-reload whitelist. Qualified by
// docs/research/COJ_RELOAD_OWNERSHIP.md (pinned original code.pak) and retained
// work/offline-batch-20261022/reload-findings.md, sections A/B. These exact
// chains resolve indexed GetAmmoCount to DoubleAmmo slot 1 / FireAmmo slot 0
// storage. Reserve counters resolve to the named PawnInventory backing fields.
// No runtime parameters are constructed or inferred from a weapon name prefix.
inline constexpr Profile GetProfile(CoJIndexedReloadClass cls) noexcept {
    using Pool=CoJIndexedReloadReserve;
    switch(cls){
    case CoJIndexedReloadClass::remington:
        return {"WeaponRifleRemington",{1,1},{Pool::shotgun,Pool::shotgun},true,true};
    case CoJIndexedReloadClass::saw_off:
        return {"WeaponRifleRemingtonSawOff",{1,1},{Pool::shotgun,Pool::shotgun},true,true};
    case CoJIndexedReloadClass::derringer:
        return {"WeaponPistolDerringer",{1,1},{Pool::pistol,Pool::pistol},true,false};
    case CoJIndexedReloadClass::lemant:
        return {"WeaponPistolLemant",{1,8},{Pool::rifle,Pool::pistol},false,false};
    default:return {};
    }
}
inline constexpr bool ValidId(std::uint32_t id) noexcept {return id!=0&&id!=0xffffffffu;}
inline constexpr bool ValidContext(const CoJIndexedReloadContext& v) noexcept {
    // Passive polling needs no cross-poll lease. A synchronous pair does.
    return v.player_generation&&v.native_context&&(!v.synchronous_scope||v.object_epoch);
}
inline constexpr bool OwnerShape(const CoJIndexedReloadSnapshot& v,const Profile& profile) noexcept {
    if(v.armed_hand<0||v.armed_hand>1)return false;
    int selected=0,other=0;
    for(bool present:v.weapon_present[v.armed_hand])selected+=present?1:0;
    for(bool present:v.weapon_present[1-v.armed_hand])other+=present?1:0;
    // Allow stable absent slots during a single-hand transition. Both-hand
    // representation is deliberately narrower: complete matching tuples for
    // an exact long gun, never a second gun or a partially switching tuple.
    return selected>0&&(!other||(profile.both_hands&&selected==3&&other==3));
}
inline constexpr bool ValidSnapshot(const CoJIndexedReloadSnapshot& v) noexcept {
    const auto profile=GetProfile(v.weapon_class);
    if(!v.valid||!profile.name||!ValidContext(v.context)||!ValidId(v.player_id)||!ValidId(v.weapon_id)||
        v.reserve_kind!=profile.pools||!OwnerShape(v,profile))return false;
    std::array<bool,3> used{};
    for(std::size_t i=0;i<2;++i){
        if(v.loaded[i]<0||v.loaded[i]>profile.capacity[i])return false;
        used[static_cast<std::size_t>(profile.pools[i])]=true;
    }
    for(std::size_t p=0;p<3;++p)
        if(v.reserves[p]<0||(!used[p]&&v.reserves[p]!=0))return false;
    return true;
}
} // namespace indexed_reload_detail

enum class CoJIndexedReloadDeltaKind : std::uint8_t {
    unavailable,no_change,coherent_unit_insertion,coherent_native_refill,mismatch
};
struct CoJIndexedReloadDelta {
    CoJIndexedReloadDeltaKind kind=CoJIndexedReloadDeltaKind::unavailable;
    std::array<std::int64_t,2> loaded{};
    std::array<std::int64_t,3> reserves{};
    std::int64_t units=0; // Positive only for a coherent unit/refill result.
};

// Pure synchronous vector contract, not event detection or manual admission.
// Caller must bracket ONE synchronous operation with the same retained Java
// player/weapon lease, stable generation/context/armed hand, and a fresh nonzero
// synchronous_scope. object_epoch is authored from that lease, NEVER numeric
// GetThisID equality. The reader cannot establish this cross-snapshot lifetime.
// Sparse polls use scope=0 and always classify unavailable. Even a coherent
// vector proves neither a selected-index native API, mechanism/callback path,
// per-chamber occupancy, physical acceptance nor permission for feedback/writes.
// In particular this does not predict Lemant's ascending native index choice.
inline CoJIndexedReloadDelta ClassifyCoJIndexedReloadDelta(
    const CoJIndexedReloadSnapshot& before,const CoJIndexedReloadSnapshot& after,
    int selected_index) noexcept {
    using namespace indexed_reload_detail;
    using Kind=CoJIndexedReloadDeltaKind;
    CoJIndexedReloadDelta out{};
    if(selected_index<0||selected_index>1||!ValidSnapshot(before)||!ValidSnapshot(after)||
        !before.context.object_epoch||!before.context.synchronous_scope||before.context!=after.context||
        before.player_id!=after.player_id||before.weapon_id!=after.weapon_id||
        before.weapon_class!=after.weapon_class||before.armed_hand!=after.armed_hand||
        before.weapon_present!=after.weapon_present)return out;
    std::int64_t total=0;
    bool unchanged=true,nonnegative=true;
    std::array<std::int64_t,3> debit{};
    for(std::size_t i=0;i<2;++i){
        out.loaded[i]=std::int64_t(after.loaded[i])-std::int64_t(before.loaded[i]);
        unchanged=unchanged&&out.loaded[i]==0;nonnegative=nonnegative&&out.loaded[i]>=0;
        total+=out.loaded[i];
        debit[static_cast<std::size_t>(before.reserve_kind[i])]+=out.loaded[i];
    }
    bool conserved=true;
    for(std::size_t p=0;p<3;++p){
        out.reserves[p]=std::int64_t(after.reserves[p])-std::int64_t(before.reserves[p]);
        unchanged=unchanged&&out.reserves[p]==0;
        conserved=conserved&&out.reserves[p]==-debit[p];
    }
    if(unchanged){out.kind=Kind::no_change;return out;}
    out.kind=Kind::mismatch;
    if(!nonnegative||!conserved)return out;
    if(out.loaded[selected_index]==1&&out.loaded[1-selected_index]==0){
        out.kind=Kind::coherent_unit_insertion;out.units=1;
    }else if(total>1&&GetProfile(before.weapon_class).whole_clip){
        out.kind=Kind::coherent_native_refill;out.units=total;
    }
    return out;
}
} // namespace cojvr::games::call_of_juarez
