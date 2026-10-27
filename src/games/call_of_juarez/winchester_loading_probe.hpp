#pragma once
#include "games/call_of_juarez/winchester_loading_geometry.hpp"
#include "games/call_of_juarez/reload_trace.hpp"
#include "runtime/textured_cartridge_mesh.hpp"
#include <cstring>

namespace cojvr::games::call_of_juarez {

// Native centimetres. The imported mesh's front plane lies along local -Z;
// its centre (not the mesh origin) meets the displayed finger-pad frame.
// This is the axial centre of that front plane, not a controller-tip proxy.
inline ElementWorldBasisTarget BuildCoJTexturedReloadNoseFrame(
    const ElementWorldBasisTarget& pinch) noexcept {
    const ElementWorldBasisTarget identity{{},{0,1,0},{0,0,1},true};
    CoJReloadLocalFrame checked{};
    if(!BuildCoJReloadLocalFrame(identity,pinch,checked))return {};
    constexpr float nose_z=[]() constexpr {
        float z=runtime::textured_reload_mesh::vertices[0].z;
        for(const auto p:runtime::textured_reload_mesh::vertices)if(p.z<z)z=p.z;
        return z;
    }();
    constexpr float offset_cm=(nose_z-runtime::textured_reload_mesh::centre_z)*100.F;
    ElementWorldBasisTarget result{
        {pinch.position.x+pinch.forward.x*offset_cm,
         pinch.position.y+pinch.forward.y*offset_cm,
         pinch.position.z+pinch.forward.z*offset_cm},pinch.up,
        {-pinch.forward.x,-pinch.forward.y,-pinch.forward.z},true};
    return BuildCoJReloadLocalFrame(identity,result,checked)?result:ElementWorldBasisTarget{};
}

struct CoJWinchesterLoadingSample {
    CoJWinchesterLoadingGeometry geometry{};
    CoJReloadTraceSnapshot native{};
    ElementWorldBasisTarget tip{};
    CoJWinchesterLoadingTipObservation tip_observation{};
    std::uint64_t frame_sequence=0,player_generation=0,native_context=0;
    bool valid=false;
};

// Caller holds the same native Java object during both reads and checks the
// player/generation around JNI. Numeric IDs are observations, never leases.
inline bool BuildCoJWinchesterLoadingSample(const CoJWinchesterLoadingGeometry& geometry,
    const CoJReloadTraceSnapshot& before,const CoJReloadTraceSnapshot& after,
    std::uint64_t frame,std::uint64_t generation,std::uint64_t context,
    const ElementWorldBasisTarget& current_pinch,CoJWinchesterLoadingSample& out) noexcept {
    out={};
    const char* exact=geometry.model==CoJWinchesterLoadingModel::ordinary?"WeaponRifleWinchester":
        geometry.model==CoJWinchesterLoadingModel::scoped?"WeaponRifleWinchesterScope":nullptr;
    if(!frame||!generation||!(context&1U)||!exact||!geometry.valid||
        (geometry.source!=CoJReloadGeometrySource::pre_native&&geometry.source!=CoJReloadGeometrySource::post_overlay)||
        !before.valid||!after.valid||!before.probe_valid||!after.probe_valid||
        !before.weapon_class||!after.weapon_class||std::strcmp(before.weapon_class,exact)||
        std::strcmp(after.weapon_class,exact)||!before.weapon_id||before.weapon_id>=0xffffffffULL||
        before.weapon_id!=after.weapon_id||
        before.armed_hand!=0||after.armed_hand!=0||before.probe_status<1||before.probe_status>3||
        before.probe_status!=after.probe_status||before.states!=after.states||
        before.animation_id!=after.animation_id||before.current_time!=after.current_time||
        before.start_time!=after.start_time||before.play_time!=after.play_time||
        before.duration!=after.duration||before.min_advance!=after.min_advance||
        before.max_advance!=after.max_advance||before.probe_until!=after.probe_until||
        before.reloading!=after.reloading||before.kind!=CoJManualReloadKind::unit||
        after.kind!=before.kind||before.ammo_readback!=after.ammo_readback||
        before.reserve_readback!=after.reserve_readback||before.loaded!=after.loaded||
        before.reserves!=after.reserves||before.capacities!=after.capacities||
        before.drum_phase!=after.drum_phase||!std::isfinite(before.drum_phase))return false;
    for(unsigned h=0;h<2;++h)if(!std::isfinite(before.current_time[h])||
        !std::isfinite(before.start_time[h])||!std::isfinite(before.play_time[h])||
        !std::isfinite(before.duration[h])||!std::isfinite(before.min_advance[h])||
        !std::isfinite(before.max_advance[h]))return false;
    if(!std::isfinite(before.probe_until))return false;
    CoJReloadLocalFrame checked{};
    for(const auto& element:{geometry.root,geometry.body,geometry.loader,geometry.holder,geometry.barrel})
        if(!BuildCoJReloadLocalFrame(geometry.root,element,checked))return false;
    std::array<runtime::Vec3,8> lip{};
    if(!BuildCoJWinchesterLoadingLip(geometry.model,geometry.body,lip))return false;
    for(unsigned i=0;i<lip.size();++i){
        const auto p=geometry.lip[i],expected=lip[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
            std::abs(p.x-expected.x)>.0001F||std::abs(p.y-expected.y)>.0001F||
            std::abs(p.z-expected.z)>.0001F)return false;
    }
    CoJWinchesterLoadingSample sample{};
    sample.geometry=geometry;sample.native=after;
    sample.frame_sequence=frame;sample.player_generation=generation;sample.native_context=context;
    if(geometry.source==CoJReloadGeometrySource::post_overlay&&current_pinch.valid){
        sample.tip=BuildCoJTexturedReloadNoseFrame(current_pinch);
        if(!sample.tip.valid)return false;
        sample.tip_observation=ObserveCoJWinchesterLoadingTip(geometry,sample.tip,true);
        if(!sample.tip_observation.valid)return false;
    }
    sample.valid=true;out=sample;return true;
}

inline bool CoJWinchesterLoadingSampleCanPublish(const CoJWinchesterLoadingSample& sample,
    std::uint64_t frame,std::uint64_t generation,std::uint64_t context,
    bool gameplay,bool native_restored,bool pair_verified) noexcept {
    return sample.valid&&sample.geometry.valid&&sample.frame_sequence==frame&&
        sample.player_generation==generation&&sample.native_context==context&&
        generation!=0&&(context&1U)&&gameplay&&native_restored&&pair_verified;
}

// Final producer admission after mirror, HUD construction and submit_frame.
// Successful upload is delivery evidence only, never compositor visibility or
// headset acceptance. Keep receiver observations when the mesh builder hides a
// near/behind-head cartridge, but remove every displayed-tip field.
inline CoJWinchesterLoadingSample QualifyCoJWinchesterLoadingPublication(
    const CoJWinchesterLoadingSample& sample,std::uint64_t frame,std::uint64_t generation,
    std::uint64_t context,bool gameplay,bool native_restored,bool submitted,
    bool mirror_verified,bool graphics_restored,bool mesh_drawn) noexcept {
    if(!mirror_verified||!graphics_restored||!CoJWinchesterLoadingSampleCanPublish(
        sample,frame,generation,context,gameplay,native_restored,submitted))return {};
    auto out=sample;
    if(!mesh_drawn){out.tip={};out.tip_observation={};}
    return out;
}
}
