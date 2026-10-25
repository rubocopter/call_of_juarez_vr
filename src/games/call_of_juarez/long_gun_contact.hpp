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
    // Independently decoded original WinchesterScope.msh SHA-256:
    // a989d5c4bcaad39e250a0fcaa070c985ec832ec51e2f48e6f2d62eb580d532cd
    // GunBody LOD0 has a 60-triangle wood island (ordinary Winchester: 84).
    // Its measured AABB centre coincides; never infer admission by inheritance.
    if(!std::strcmp(weapon_class,"WeaponRifleWinchesterScope"))
        return {"GunBody",{.009580F,-41.159535F,-4.031803F}};
    return {};
}
}
