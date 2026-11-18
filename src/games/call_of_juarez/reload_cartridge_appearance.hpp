#pragma once

#include "games/call_of_juarez/reload_trace.hpp"
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "runtime/reload_ammunition_visual.hpp"
#include <optional>
#include <string_view>

namespace cojvr::games::call_of_juarez {

// Presentation uses the reserve family admitted for this specific carry.
// Indexed weapons require the slot captured by the gesture at pickup.
inline std::optional<runtime::ReloadCartridgeAppearance> CoJHeldReloadCartridgeAppearance(
    const CoJReloadTraceSnapshot& native,int retained_index) noexcept {
    if (!native.valid||!native.probe_valid||native.probe_status!=2||!native.weapon_class||
        !IsCoJNativeObjectIdentity(native.weapon_id)||native.armed_hand<0||native.armed_hand>1)
        return std::nullopt;
    const auto profile=std::find_if(kCoJRevolvers.begin(),kCoJRevolvers.end(),[&](const auto& entry){
        return std::string_view(native.weapon_class)==entry.class_name;
    });
    if (profile==kCoJRevolvers.end()||native.kind!=profile->kind||
        (profile->right_owned_only&&native.armed_hand!=0)||
        native.index_count!=profile->index_count||native.capacities!=profile->capacities)
        return std::nullopt;
    const bool indexed=native.kind==CoJManualReloadKind::indexed;
    if (!indexed&&retained_index!=-1)return std::nullopt;
    const int slot=indexed?retained_index:0;
    if (slot<0||slot>=native.index_count||slot!=native.selected_index)return std::nullopt;
    const auto kind=slot==0?profile->reserve:profile->secondary_reserve;
    const int pool=CoJReloadReserveSlot(kind);
    if (pool<0||native.reserve_indices[slot]!=pool||native.reserve_kinds[slot]!=kind||
        native.reserve_kind!=kind||native.reserve_readback!=native.reserves[pool]||
        native.reserve_readback<=0)
        return std::nullopt;
    switch(kind){
    case CoJReloadReserveKind::pistol:return runtime::ReloadCartridgeAppearance::pistol;
    case CoJReloadReserveKind::rifle:return runtime::ReloadCartridgeAppearance::rifle;
    case CoJReloadReserveKind::shotgun:return runtime::ReloadCartridgeAppearance::shotgun;
    }
    return std::nullopt;
}

}
