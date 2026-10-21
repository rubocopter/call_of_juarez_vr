#pragma once
#include "games/call_of_juarez/reload_geometry.hpp"
#include <cstring>

namespace cojvr::games::call_of_juarez {
struct CoJLongGunContactProfile {
    const char* element=nullptr;
    runtime::Vec3 point_cm{};
};
// Centres of the original meshes' isolated wooden fore-end islands, in the
// owning element's authored centimetres. See COJ_ARM_SKINNING_AND_AIM.md.
// These are exact-game interaction anchors, not native collision volumes or
// a universal hand offset. Original archive identity is admitted by staging.
inline CoJLongGunContactProfile CoJLongGunContact(const char* weapon_class) noexcept {
    if(!weapon_class)return {};
    if(!std::strcmp(weapon_class,"WeaponRifleRemington"))
        return {"GunBarrel",{.001855F,.731875F,10.743126F}};
    if(!std::strcmp(weapon_class,"WeaponRifleRemingtonSawOff"))
        return {"GunBarrel",{.002069F,.732127F,10.742139F}};
    if(!std::strcmp(weapon_class,"WeaponRifleWinchester"))
        return {"GunBody",{.009580F,-41.159535F,-4.031803F}};
    return {};
}
}
