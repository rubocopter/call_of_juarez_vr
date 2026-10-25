#pragma once
#include "games/call_of_juarez/holster_geometry.hpp"
#include <cstdint>

namespace cojvr::games::call_of_juarez {
inline constexpr std::string_view kCoJBowVisualMeshSha256=
    "d16928ae927620f567f8384c65c190459460d042ac4cf8b05e9e8eb3d2e27d53";
inline constexpr std::string_view kCoJBowArrowVisualMeshSha256=
    "b13419d78caa2adf54ce3b1e68632b8b79a75ac45f3c29be336dc79767654bde";

// Exact original hierarchy. These are bind profiles, NOT current animation.
// Bow: DBowHolder=1, DArrow=9 parent=1. Arrow: DArrowHolder=0 root,
// ArrowA=1 parent=0. ShowArrowMesh resets the OBJECT then attaches it to
// Right_hand (fallback Bip01 R Hand); it does NOT invert DArrowHolder.
struct CoJBowVisualAssetProfile {
    static constexpr int bow_holder=1,bow_arrow=9,bow_arrow_parent=1;
    static constexpr int arrow_holder=0,arrow_mesh=1,arrow_mesh_parent=0;
    static constexpr CoJEquipmentFrame arrow_holder_bind{{{1,0,0,0, 0,1,0,0, 0,0,1,0,
        -0.09715759009122849,0,-64.27693939208984,1}}};
    static constexpr CoJEquipmentFrame arrow_mesh_local_bind{{{1,0,0,0, 0,1,0,0, 0,0,1,0,
        0.09715759009122849,1.612320374988485e-7,64.27693939208984,1}}};
};

struct CoJBowVisualSample {
    // Opaque copied x86 identities, never dereferenced/JNI handles. A future
    // game-thread adapter must repeat actual JNI object equality/parent identity
    // and exact-build admission around reads; tokens alone cannot prove lifetime.
    std::uint32_t actor=0,bow=0,arrow=0,parent=0;
    std::uint64_t generation=0,frame=0;
    int parent_element=-1,resolved_right_element=-1,holder_element=-1,mesh_element=-1;
    std::string_view player_sha256,bow_sha256,arrow_sha256;
    bool exact_weapon_bow=false,visible=false;
    CoJEquipmentFrame hand_world{},arrow_object_world{},holder_world{},mesh_world{};
};

struct CoJBowVisualObservation {
    std::uint32_t actor=0,bow=0,arrow=0;
    std::uint64_t generation=0,frame=0;
    CoJEquipmentFrame object_in_hand{},holder_in_object{},mesh_in_object{};
    static constexpr bool writer_admitted=false;
    static constexpr bool string_writer_admitted=false;
};

// Value-only observer: does not query JNI, call Show/Hide/UpdateAmmoSkin,
// animate a string, or mutate/render an object. Repeated snapshots must match.
// Never use the bow-root rigid map as the separate arrow's source object.
inline bool ObserveCoJBowVisual(const CoJBowVisualSample& a,const CoJBowVisualSample& b,
                               CoJBowVisualObservation& out) noexcept {
    const auto identity=[](std::uint32_t v){return v!=0 && v!=0xffffffffU;};
    const bool known_player=a.player_sha256==kCoJRayEquipmentMeshSha256 ||
                            a.player_sha256==kCoJBillyEquipmentMeshSha256;
    if(!known_player || !a.exact_weapon_bow || !a.visible ||
       a.bow_sha256!=kCoJBowVisualMeshSha256 || a.arrow_sha256!=kCoJBowArrowVisualMeshSha256 ||
       !identity(a.actor)||!identity(a.bow)||!identity(a.arrow)||a.parent!=a.actor ||
       a.actor==a.bow || a.actor==a.arrow || a.bow==a.arrow || !a.generation || !a.frame ||
       a.parent_element<0 || a.parent_element!=a.resolved_right_element ||
       a.holder_element!=CoJBowVisualAssetProfile::arrow_holder ||
       a.mesh_element!=CoJBowVisualAssetProfile::arrow_mesh){out={};return false;}
    if(a.actor!=b.actor || a.bow!=b.bow || a.arrow!=b.arrow || a.parent!=b.parent ||
       a.generation!=b.generation || a.frame!=b.frame || a.parent_element!=b.parent_element ||
       a.resolved_right_element!=b.resolved_right_element || a.holder_element!=b.holder_element ||
       a.mesh_element!=b.mesh_element || a.player_sha256!=b.player_sha256 ||
       a.bow_sha256!=b.bow_sha256 || a.arrow_sha256!=b.arrow_sha256 ||
       a.exact_weapon_bow!=b.exact_weapon_bow || a.visible!=b.visible ||
       a.hand_world.values!=b.hand_world.values || a.arrow_object_world.values!=b.arrow_object_world.values ||
       a.holder_world.values!=b.holder_world.values || a.mesh_world.values!=b.mesh_world.values){out={};return false;}
    CoJBowVisualObservation result;
    CoJEquipmentFrame hand_inverse,object_inverse;
    if(!InvertCoJEquipmentFrame(a.hand_world,hand_inverse) ||
       !InvertCoJEquipmentFrame(a.arrow_object_world,object_inverse) ||
       !ComposeCoJEquipmentFrames(a.arrow_object_world,hand_inverse,result.object_in_hand) ||
       !ComposeCoJEquipmentFrames(a.holder_world,object_inverse,result.holder_in_object) ||
       !ComposeCoJEquipmentFrames(a.mesh_world,object_inverse,result.mesh_in_object)){out={};return false;}
    result.actor=a.actor;result.bow=a.bow;result.arrow=a.arrow;
    result.generation=a.generation;result.frame=a.frame;
    out=result;return true;
}
} // namespace cojvr::games::call_of_juarez
