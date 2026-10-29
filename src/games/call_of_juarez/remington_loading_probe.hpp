#pragma once
#include "games/call_of_juarez/winchester_loading_probe.hpp"

namespace cojvr::games::call_of_juarez {
enum class CoJRemingtonLoadingModel : std::uint8_t { unknown, ordinary, sawed_off };
// Diagnostic GunBarrel-local rear rim centres, native centimetres. Spatial
// order (+X, -X) is NOT a demonstrated native ammunition-slot mapping.
// Original Remington mesh SHA256 d086f18770302d6dd176543d417dc3f929e9ad2ce5b772b89fcfe7e930e7e596;
// SawOff bb8ae35d14e9d34945b33477dc5ae6d226a2ebd7970730249f01dead9c5c2343.
// Rounded mesh measurements do not establish aperture clearance or admission.
inline bool BuildCoJRemingtonLoadingMouths(CoJRemingtonLoadingModel model,
    const ElementWorldBasisTarget& barrel,std::array<ElementWorldBasisTarget,2>& out) noexcept {
    out={};
    if(model!=CoJRemingtonLoadingModel::ordinary&&model!=CoJRemingtonLoadingModel::sawed_off)return false;
    if(!winchester_loading_detail::Rigid(barrel))return false;
    const auto x=winchester_loading_detail::Cross(winchester_loading_detail::Widen(barrel.up),
        winchester_loading_detail::Widen(barrel.forward));
    std::array<ElementWorldBasisTarget,2> mouths{};
    for(unsigned i=0;i<2;++i){
        const double side=i==0?1.:-1.;
        runtime::Vec3 p{};
        if(!winchester_loading_detail::Store({barrel.position.x+side*x.x+2.98*barrel.up.x-7.77*barrel.forward.x,
            barrel.position.y+side*x.y+2.98*barrel.up.y-7.77*barrel.forward.y,
            barrel.position.z+side*x.z+2.98*barrel.up.z-7.77*barrel.forward.z},p))return false;
        mouths[i]={p,barrel.up,barrel.forward,true};
    }
    out=mouths;return true;
}
struct CoJRemingtonLoadingGeometry {
    CoJRemingtonLoadingModel model=CoJRemingtonLoadingModel::unknown;
    CoJReloadGeometrySource source=CoJReloadGeometrySource::unknown;
    ElementWorldBasisTarget root{},body{},lock{},holder{},barrel{};
    std::array<ElementWorldBasisTarget,2> mouths{};
    bool valid=false;
};
struct CoJRemingtonLoadingSample {
    CoJRemingtonLoadingGeometry geometry{};
    CoJReloadTraceSnapshot native{};
    ElementWorldBasisTarget tip{};
    std::uint64_t frame_sequence=0,player_generation=0,native_context=0;
    bool valid=false;
};
// Caller retains the same Java weapon across geometry/pinch and both traces.
inline bool BuildCoJRemingtonLoadingSample(const CoJRemingtonLoadingGeometry& geometry,
    const CoJReloadTraceSnapshot& before,const CoJReloadTraceSnapshot& after,
    std::uint64_t frame,std::uint64_t generation,std::uint64_t context,
    const ElementWorldBasisTarget& current_pinch,CoJRemingtonLoadingSample& out) noexcept {
    out={};
    const char* exact=geometry.model==CoJRemingtonLoadingModel::ordinary?"WeaponRifleRemington":
        geometry.model==CoJRemingtonLoadingModel::sawed_off?"WeaponRifleRemingtonSawOff":nullptr;
    if(!frame||!generation||!(context&1U)||!exact||!geometry.valid||
        (geometry.source!=CoJReloadGeometrySource::pre_native&&geometry.source!=CoJReloadGeometrySource::post_overlay)||
        !before.valid||!after.valid||!before.probe_valid||!after.probe_valid||
        !before.weapon_class||!after.weapon_class||std::strcmp(before.weapon_class,exact)||
        std::strcmp(after.weapon_class,exact)||!before.weapon_id||before.weapon_id>=0xffffffffULL||
        before.weapon_id!=after.weapon_id||
        before.armed_hand<0||before.armed_hand>1||before.armed_hand!=after.armed_hand||
        (geometry.model==CoJRemingtonLoadingModel::ordinary&&before.armed_hand!=0)||before.probe_status<1||before.probe_status>3||
        before.probe_status!=after.probe_status||before.states!=after.states||
        before.animation_id!=after.animation_id||before.current_time!=after.current_time||
        before.start_time!=after.start_time||before.play_time!=after.play_time||
        before.duration!=after.duration||before.min_advance!=after.min_advance||
        before.max_advance!=after.max_advance||before.probe_until!=after.probe_until||
        before.reloading!=after.reloading||before.kind!=CoJManualReloadKind::indexed||
        after.kind!=before.kind||before.ammo_readback!=after.ammo_readback||
        before.reserve_readback!=after.reserve_readback||before.loaded!=after.loaded||
        before.reserves!=after.reserves||before.capacities!=after.capacities||
        before.selected_index!=after.selected_index||before.selected_index<0||before.selected_index>1||
        before.index_count!=2||after.index_count!=2||before.capacities!=std::array<int,2>{1,1}||
        before.reserve_kind!=CoJReloadReserveKind::shotgun||before.reserve_kind!=after.reserve_kind||
        before.reserve_kinds!=std::array<CoJReloadReserveKind,2>{CoJReloadReserveKind::shotgun,CoJReloadReserveKind::shotgun}||
        before.reserve_kinds!=after.reserve_kinds||before.reserve_indices!=std::array<int,2>{2,2}||
        before.reserve_indices!=after.reserve_indices||before.native_body_cycle!=after.native_body_cycle||
        before.drum_phase!=after.drum_phase||!std::isfinite(before.drum_phase))return false;
    for(unsigned h=0;h<2;++h)if(!std::isfinite(before.current_time[h])||
        !std::isfinite(before.start_time[h])||!std::isfinite(before.play_time[h])||
        !std::isfinite(before.duration[h])||!std::isfinite(before.min_advance[h])||
        !std::isfinite(before.max_advance[h]))return false;
    if(!std::isfinite(before.probe_until))return false;
    CoJReloadLocalFrame checked{};
    for(const auto& element:{geometry.root,geometry.body,geometry.lock,geometry.holder,geometry.barrel})
        if(!BuildCoJReloadLocalFrame(geometry.root,element,checked))return false;
    std::array<ElementWorldBasisTarget,2> mouths{};
    if(!BuildCoJRemingtonLoadingMouths(geometry.model,geometry.barrel,mouths))return false;
    for(unsigned i=0;i<2;++i){
        const auto& a=geometry.mouths[i];const auto& b=mouths[i];
        if(!a.valid||a.position.x!=b.position.x||a.position.y!=b.position.y||a.position.z!=b.position.z||
            a.up.x!=b.up.x||a.up.y!=b.up.y||a.up.z!=b.up.z||
            a.forward.x!=b.forward.x||a.forward.y!=b.forward.y||a.forward.z!=b.forward.z)return false;
        if(before.loaded[i]<0||before.loaded[i]>1)return false;
    }
    for(auto reserve:before.reserves)if(reserve<0)return false;
    if(before.ammo_readback!=before.loaded[before.selected_index]||
        before.reserve_readback!=before.reserves[2])return false;
    CoJRemingtonLoadingSample sample{};
    sample.geometry=geometry;sample.native=after;
    sample.frame_sequence=frame;sample.player_generation=generation;sample.native_context=context;
    if(geometry.source==CoJReloadGeometrySource::post_overlay&&current_pinch.valid){
        sample.tip=BuildCoJTexturedReloadNoseFrame(current_pinch);
        if(!sample.tip.valid)return false;
    }
    sample.valid=true;out=sample;return true;
}

inline bool CoJRemingtonLoadingSampleCanPublish(const CoJRemingtonLoadingSample& sample,
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
inline CoJRemingtonLoadingSample QualifyCoJRemingtonLoadingPublication(
    const CoJRemingtonLoadingSample& sample,std::uint64_t frame,std::uint64_t generation,
    std::uint64_t context,bool gameplay,bool native_restored,bool submitted,
    bool mirror_verified,bool graphics_restored,bool mesh_drawn) noexcept {
    if(!mirror_verified||!graphics_restored||!CoJRemingtonLoadingSampleCanPublish(
        sample,frame,generation,context,gameplay,native_restored,submitted))return {};
    auto out=sample;
    if(!mesh_drawn){out.tip={};}
    return out;
}
}
