#pragma once
#include "games/call_of_juarez/indexed_reload_delta.hpp"
#include <algorithm>

namespace cojvr::games::call_of_juarez {
enum class CoJIndexedReloadPredictionKind : std::uint8_t {
    unavailable,no_transfer,selected_unit,other_unit,multiple_units
};
struct CoJIndexedReloadPrediction {
    CoJIndexedReloadPredictionKind kind=CoJIndexedReloadPredictionKind::unavailable;
    std::array<std::int32_t,2> loaded{}; // Predicted positive increments, never stored ammo.
    std::array<std::int32_t,3> reserve_debit{}; // Shared pool counted once.
};

// Static preflight model of pinned PawnArmed.WeaponReload(LWeapon;)Z, including
// ascending index choice and whole-clip continuation. This is NOT admission,
// a selected-index API or an inventory writer. A future caller still needs
// native eligibility, ownership, free hands, phase/port qualification and a
// same-lease post-call ClassifyCoJIndexedReloadDelta before any unit feedback.
// In particular selected_unit cannot authorize a gesture, callback or retry.
// Passive samples lack a synchronous lease and intentionally produce no model.
inline CoJIndexedReloadPrediction PredictCoJIndexedNativeReload(
    const CoJIndexedReloadSnapshot& before,int selected_index) noexcept {
    using namespace indexed_reload_detail;
    using Kind=CoJIndexedReloadPredictionKind;
    CoJIndexedReloadPrediction out{};
    if(selected_index<0||selected_index>1||!ValidSnapshot(before)||
        !before.context.object_epoch||!before.context.synchronous_scope)return out;
    const auto profile=GetProfile(before.weapon_class);
    auto remaining=before.reserves;
    std::int32_t total=0;
    for(std::size_t index=0;index<2;++index){
        const auto pool=static_cast<std::size_t>(profile.pools[index]);
        const auto free=profile.capacity[index]-before.loaded[index];
        const auto requested=profile.whole_clip?free:std::min(free,std::int32_t{1});
        const auto amount=std::min(requested,remaining[pool]);
        out.loaded[index]=amount;out.reserve_debit[pool]+=amount;
        remaining[pool]-=amount;total+=amount;
        if(amount&&!profile.whole_clip)break;
    }
    out.kind=total==0?Kind::no_transfer:total>1?Kind::multiple_units:
        out.loaded[selected_index]==1?Kind::selected_unit:Kind::other_unit;
    return out;
}
} // namespace cojvr::games::call_of_juarez
