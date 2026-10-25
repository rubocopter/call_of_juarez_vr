#pragma once
#include "games/call_of_juarez/holster_asset_profiles.hpp"

namespace cojvr::games::call_of_juarez {
inline constexpr std::string_view kCoJRayEquipmentMeshSha256=
    "9b3f4579cd32476f866dc1aad5a1bd0f1bc913449548f40f7c76bd37cd8d33b5";
inline constexpr std::string_view kCoJBillyEquipmentMeshSha256=
    "ce1e70dfab7ddae4f4036518820e34c84a18d9dc18ab48163815d4ac1dd15be7";
enum class CoJHolsterSide { unknown, right, left };

inline const CoJHolsterAssetProfile* FindCoJHolsterAssetProfile(std::string_view sha256) noexcept {
    for(const auto& profile:kCoJHolsterAssetProfiles)if(profile.mesh_sha256==sha256)return &profile;
    return nullptr;
}

// Original native GetOwnerElementIDFromSlot: right/left are distinct authored
// mounts. Billy has no HolsterL. Indices are original MSH ordinals; the parent
// must prove a derived mesh preserves them before using them as native IDs.
inline int CoJHolsterBodyMountElement(std::string_view body_sha256,CoJHolsterSide side) noexcept {
    if(body_sha256==kCoJRayEquipmentMeshSha256){
        if(side==CoJHolsterSide::right)return 14;
        if(side==CoJHolsterSide::left)return 8;
    }
    if(body_sha256==kCoJBillyEquipmentMeshSha256 && side==CoJHolsterSide::right)return 16;
    return -1;
}

struct CoJHolsterGeometry {
    CoJEquipmentFrame weapon_world{},grasp_world{},grasp_in_holster{};
    int body_mount_element=-1;
    int holder_element=-1,holster_element=-1;
    static constexpr bool contact_admitted=false;
    static constexpr bool mesh_attach_admitted=false;
};

// Pure prediction from a same-frame mount B. No body offset is added here:
// the caller supplies B in the requested natural OR displayed coordinate frame.
// H * weaponWorld == B; G * weaponWorld == graspWorld. Preserve full rotations.
inline bool BuildCoJHolsterGeometry(std::string_view weapon_sha256,std::string_view body_sha256,
    CoJHolsterSide side,const CoJEquipmentFrame& body_mount,CoJHolsterGeometry& out) noexcept {
    const auto* profile=FindCoJHolsterAssetProfile(weapon_sha256);
    const int mount=CoJHolsterBodyMountElement(body_sha256,side);
    CoJHolsterGeometry result;
    CoJEquipmentFrame inverse;
    if(!profile || mount<0 || !InvertCoJEquipmentFrame(profile->holster,inverse) ||
       !ComposeCoJEquipmentFrames(inverse,body_mount,result.weapon_world) ||
       !ComposeCoJEquipmentFrames(profile->holder,result.weapon_world,result.grasp_world) ||
       !ComposeCoJEquipmentFrames(profile->holder,inverse,result.grasp_in_holster)){out={};return false;}
    result.body_mount_element=mount;
    result.holder_element=profile->holder_element;
    result.holster_element=profile->holster_element;
    out=result;return true;
}
} // namespace cojvr::games::call_of_juarez
