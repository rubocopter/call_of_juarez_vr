#include "games/call_of_juarez/focus_zoom.hpp"
#include <cmath>
namespace cojvr::games::call_of_juarez {
namespace {
template<class T>T Fn(void* env,int index) noexcept {return reinterpret_cast<T>((*static_cast<void***>(env))[index]);}
bool Check(void* env) noexcept {
    auto exception=Fn<void*(__stdcall*)(void*)>(env,15)(env);
    if(!exception)return true;
    Fn<void(__stdcall*)(void*)>(env,17)(env);
    Fn<void(__stdcall*)(void*,void*)>(env,23)(env,exception);
    return false;
}
struct Local {
    void* env;void* value;
    ~Local(){if(value)Fn<void(__stdcall*)(void*,void*)>(env,23)(env,value);}
};
union Value{std::int64_t alignment;void* object;std::int32_t integer;};
bool OrdinarySquintOwner(void* env,void* player,void* cls) noexcept {
    // Both getters only read the shipped hand-state objects. Never query
    // Weapon.GetScopeZoom/GetLogicParams: that path can create parameters.
    void* methods[2]{};int index=0;
    for(auto name:{"GetActualWeaponNotEmpty","GetDesiredWeapon"}){
        methods[index]=Fn<void*(__stdcall*)(void*,void*,const char*,const char*)>(env,33)(env,cls,name,"(I)LWeapon;");
        if(!Check(env)||!methods[index++])return false;
    }
    Local bow{env,Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,"WeaponBow")};
    if(!Check(env)||!bow.value)return false;
    Local scope{env,Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,"WeaponRifleWinchesterScope")};
    if(!Check(env)||!scope.value)return false;
    for(int hand=0;hand<2;++hand)for(auto method:methods){
        Value args{.integer=hand};
        Local weapon{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,player,method,&args)};
        if(!Check(env))return false;
        if(!weapon.value)continue;
        for(auto special:{bow.value,scope.value}){
            const bool excluded=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,weapon.value,special)!=0;
            if(!Check(env)||excluded)return false;
        }
    }
    return true;
}
bool ValidFov(const runtime::EyeFov& f) noexcept {
    constexpr float half_pi=1.57079632679F;
    return std::isfinite(f.angle_left)&&std::isfinite(f.angle_right)&&
        std::isfinite(f.angle_up)&&std::isfinite(f.angle_down)&&
        f.angle_left<0&&f.angle_left>-half_pi&&f.angle_right>0&&f.angle_right<half_pi&&
        f.angle_down<0&&f.angle_down>-half_pi&&f.angle_up>0&&f.angle_up<half_pi;
}
}
bool ReadCoJFocusZoom(void* env,void* player,CoJFocusZoom& out) noexcept {
    out={};if(!env||!player||!*static_cast<void***>(env))return false;
    constexpr int required[]{6,15,17,23,31,32,33,36,94,102};
    for(auto index:required)if(!(*static_cast<void***>(env))[index])return false;
    if(!Check(env))return false;
    auto cls=Fn<void*(__stdcall*)(void*,void*)>(env,31)(env,player);
    const bool class_ok=Check(env);
    if(!class_ok||!cls){if(cls)Fn<void(__stdcall*)(void*,void*)>(env,23)(env,cls);return false;}
    auto read=[&](const char* name,float& value){
        auto field=Fn<void*(__stdcall*)(void*,void*,const char*,const char*)>(env,94)(env,cls,name,"F");
        if(!Check(env)||!field)return false;
        value=Fn<float(__stdcall*)(void*,void*,void*)>(env,102)(env,player,field);
        return Check(env)&&std::isfinite(value);
    };
    CoJFocusZoom candidate{};
    const bool fields_ok=read("m_fSquintFactor",candidate.factor)&&
        read("m_fCurrentMaxSquintZoom",candidate.maximum);
    bool owner_ok=fields_ok&&candidate.factor>=0&&candidate.factor<=1&&candidate.maximum>=1;
    // ArmedPlayerBeing mixes bow concentration into GetSquintFactor and
    // replaces GetSquintZoom for scopes. Those mechanics retain separate gates.
    if(owner_ok&&candidate.factor>0)owner_ok=OrdinarySquintOwner(env,player,cls);
    Fn<void(__stdcall*)(void*,void*)>(env,23)(env,cls);
    if(!owner_ok)return false;
    // Native GetSquintProportional(1,currentMax): preserve the native smoothing
    // including its decay after focus release; logical input is not execution.
    candidate.magnification=1+(candidate.maximum-1)*candidate.factor;
    if(!std::isfinite(candidate.magnification)||candidate.magnification<1)return false;
    candidate.valid=true;out=candidate;return true;
}
bool BuildCoJFocusRenderEyes(const std::array<runtime::EyeView,2>& optical,
    const CoJFocusZoom& zoom,std::array<runtime::EyeView,2>& render) noexcept {
    render=optical;
    if(!zoom.valid||!std::isfinite(zoom.magnification)||zoom.magnification<1||
        !ValidFov(optical[0].fov)||!ValidFov(optical[1].fov))return false;
    if(zoom.magnification==1)return true;
    auto scaled=optical;
    for(auto& eye:scaled){
        auto angle=[&](float a){return std::atan(std::tan(a)/zoom.magnification);};
        eye.fov={angle(eye.fov.angle_left),angle(eye.fov.angle_right),angle(eye.fov.angle_up),angle(eye.fov.angle_down)};
        if(!ValidFov(eye.fov))return false;
    }
    render=scaled;return true;
}
}
