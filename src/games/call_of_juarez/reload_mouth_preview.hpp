#pragma once
#include "games/call_of_juarez/motion_reload_owner.hpp"
#include "games/call_of_juarez/remington_loading_probe.hpp"
#include <string_view>

namespace cojvr::games::call_of_juarez {

// Diagnostic geometry in native centimetres, predicted from current input.
// Polygon order and the shotgun's spatial order carry no ammo-slot authority.
struct CoJReloadMouthPreview {
    std::array<runtime::Vec3,8> points_cm{};
    std::size_t count=0;
    ElementWorldBasisTarget tip_cm{};
    runtime::Vec3 lever_cm{};
    bool lever_valid=false;
};

inline const char* CoJReloadMouthElement(const CoJReloadTraceSnapshot& native) noexcept {
    if(!native.weapon_class||native.armed_hand<0||native.armed_hand>1)return nullptr;
    const std::string_view name{native.weapon_class};
    if(native.armed_hand==0&&(name=="WeaponRifleWinchester"||name=="WeaponRifleWinchesterScope"))
        return "GunBody";
    if(name=="WeaponRifleRemingtonSawOff"||(native.armed_hand==0&&name=="WeaponRifleRemington"))
        return "GunBarrel";
    return nullptr;
}

inline bool SameCoJReloadMouthPhase(const CoJReloadTraceSnapshot& a,
    const CoJReloadTraceSnapshot& b) noexcept {
    return a.valid&&b.valid&&a.probe_valid&&b.probe_valid&&a.probe_status==2&&b.probe_status==2&&
        a.reserve_readback>0&&a.reserve_readback==b.reserve_readback&&
        IsCoJNativeObjectIdentity(a.weapon_id)&&a.weapon_id==b.weapon_id&&
        a.armed_hand==b.armed_hand&&a.weapon_class&&b.weapon_class&&
        std::string_view(a.weapon_class)==b.weapon_class&&a.kind==b.kind&&
        a.selected_index==b.selected_index&&a.loaded==b.loaded&&a.reserves==b.reserves&&
        a.states==b.states&&a.animation_id==b.animation_id&&a.current_time==b.current_time&&
        a.start_time==b.start_time&&a.play_time==b.play_time&&a.duration==b.duration&&
        a.min_advance==b.min_advance&&a.max_advance==b.max_advance&&
        a.probe_until==b.probe_until&&a.reloading==b.reloading;
}

inline CoJReloadMouthPreview BuildCoJReloadMouthPreview(const CoJReloadTraceSnapshot& native,
    const ElementWorldBasisTarget& natural_root,const ElementWorldBasisTarget& natural_mouth,
    const TrackedWeaponFramePlan& plan) noexcept {
    if(!CoJReloadMouthElement(native)||!plan.valid||
        !winchester_loading_detail::Rigid(natural_root)||!winchester_loading_detail::Rigid(natural_mouth)||
        !winchester_loading_detail::Rigid(plan.root))return {};
    const auto mouth=TransformWeaponElementFrame(natural_root,plan.root,natural_mouth);
    const std::string_view name{native.weapon_class};
    CoJReloadMouthPreview out{};
    if(name=="WeaponRifleWinchester"||name=="WeaponRifleWinchesterScope"){
        const auto model=name=="WeaponRifleWinchester"?CoJWinchesterLoadingModel::ordinary:
            CoJWinchesterLoadingModel::scoped;
        if(BuildCoJWinchesterLoadingLip(model,mouth,out.points_cm)){
            out.count=out.points_cm.size();
            // Centre of the pinned GunLever LOD0 model bounds in GunBody space.
            // Stock Winchester and Scope have the same authored lever pose.
            const auto x=winchester_loading_detail::Cross(winchester_loading_detail::Widen(mouth.up),
                winchester_loading_detail::Widen(mouth.forward));
            constexpr runtime::Vec3 local{.00495F,-5.84833F,2.93542F};
            out.lever_valid=winchester_loading_detail::Store({mouth.position.x+x.x*local.x+mouth.up.x*local.y+mouth.forward.x*local.z,
                mouth.position.y+x.y*local.x+mouth.up.y*local.y+mouth.forward.y*local.z,
                mouth.position.z+x.z*local.x+mouth.up.z*local.y+mouth.forward.z*local.z},out.lever_cm);
        }
    }else{
        const auto model=name=="WeaponRifleRemington"?CoJRemingtonLoadingModel::ordinary:
            CoJRemingtonLoadingModel::sawed_off;
        std::array<ElementWorldBasisTarget,2> rims{};
        if(BuildCoJRemingtonLoadingMouths(model,mouth,rims)){
            for(std::size_t i=0;i<rims.size();++i)out.points_cm[i]=rims[i].position;
            out.count=rims.size();
        }
    }
    return out;
}
}
