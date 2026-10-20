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
bool Same(void* env,void* a,void* b) noexcept {
    const bool same=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,24)(env,a,b)!=0;
    return Check(env)&&same;
}
void* Field(void* env,void* cls,const char* name,const char* sig) noexcept {
    auto field=Fn<void*(__stdcall*)(void*,void*,const char*,const char*)>(env,94)(env,cls,name,sig);
    return Check(env)?field:nullptr;
}
bool ReadFloat(void* env,void* object,void* cls,const char* name,float& value) noexcept {
    auto field=Field(env,cls,name,"F");if(!field)return false;
    value=Fn<float(__stdcall*)(void*,void*,void*)>(env,102)(env,object,field);
    return Check(env)&&std::isfinite(value);
}
bool ReadBowConcentration(void* env,void* player,void* cls,float& factor) noexcept {
    // Being.GetBulletTimeProportional(0,1) -> LawmanModuleSingle's
    // already-owned BulletTime. Never call GetBeingZoom/CalculateZoom,
    // BulletTime.Update, or the weapon-parameter factory.
    auto module_field=Field(env,cls,"m_Module","LLawmanModule;");
    if(!module_field)return false;
    Local module{env,Fn<void*(__stdcall*)(void*,void*,void*)>(env,95)(env,player,module_field)};
    if(!Check(env)||!module.value)return false;
    Local module_cls{env,Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,"LawmanModuleSingle")};
    if(!Check(env)||!module_cls.value)return false;
    const bool single=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,module.value,module_cls.value)!=0;
    if(!Check(env)||!single)return false;
    auto bullet_field=Field(env,module_cls.value,"m_cBulletTime","LBulletTime;");
    if(!bullet_field)return false;
    Local bullet{env,Fn<void*(__stdcall*)(void*,void*,void*)>(env,95)(env,module.value,bullet_field)};
    if(!Check(env))return false;
    float concentration=0;
    if(bullet.value){
        Local bullet_cls{env,Fn<void*(__stdcall*)(void*,void*)>(env,31)(env,bullet.value)};
        if(!Check(env)||!bullet_cls.value)return false;
        float current=0,target=0;
        if(!ReadFloat(env,bullet.value,bullet_cls.value,"m_fCurrentSlomo",current)||
           !ReadFloat(env,bullet.value,bullet_cls.value,"_SLOMO",target)||
           current<=0||current>1||target<=0||target>1)return false;
        // Exact GetProportionalClamp(0,1,current,1,target), including
        // coincident endpoints. A null native BulletTime supplies 1,1.
        if(target<1)concentration=current<=target?1:(1-current)/(1-target);
    }
    Local current_module{env,Fn<void*(__stdcall*)(void*,void*,void*)>(env,95)(env,player,module_field)};
    if(!Check(env)||!Same(env,module.value,current_module.value))return false;
    Local current_bullet{env,Fn<void*(__stdcall*)(void*,void*,void*)>(env,95)(env,module.value,bullet_field)};
    if(!Check(env)||!Same(env,bullet.value,current_bullet.value))return false;
    // ArmedPlayerBeing.GetSquintFactor: squint + bow - squint*bow.
    factor=factor+concentration-factor*concentration;
    return std::isfinite(factor)&&factor>=0&&factor<=1;
}
bool ReadSquintOwner(void* env,void* player,void* cls,float& factor) noexcept {
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
    Local weapons[4]{{env,nullptr},{env,nullptr},{env,nullptr},{env,nullptr}};
    bool has_bow=false;void* bow_owner=nullptr;
    for(int i=0;i<4;++i){
        Value args{.integer=i/2};auto& weapon=weapons[i];
        weapon.value=Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,player,methods[i%2],&args);
        if(!Check(env))return false;
        if(!weapon.value)continue;
        const bool scoped=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,weapon.value,scope.value)!=0;
        if(!Check(env)||scoped)return false;
        const bool is_bow=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,weapon.value,bow.value)!=0;
        if(!Check(env))return false;
        if(is_bow){has_bow=true;bow_owner=weapon.value;}
    }
    if(!has_bow)return true;
    // GetWeaponBow observes desired hands, while the native maximum cache
    // comes from actual hands. Admit one stable bow, including JNI aliases
    // in both hands, only after equip/unequip ownership agrees.
    if(!(*static_cast<void***>(env))[24]||!(*static_cast<void***>(env))[95])return false;
    for(int hand=0;hand<2;++hand){
        auto actual=weapons[hand*2].value;auto desired=weapons[hand*2+1].value;
        if(!Same(env,actual,desired))return false;
        if(actual&&!Same(env,actual,bow_owner))return false;
    }
    if(!ReadBowConcentration(env,player,cls,factor))return false;
    for(int i=0;i<4;++i){
        Value args{.integer=i/2};
        Local current{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,player,methods[i%2],&args)};
        if(!Check(env)||!Same(env,weapons[i].value,current.value))return false;
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
    CoJFocusZoom candidate{};
    const bool fields_ok=ReadFloat(env,player,cls,"m_fSquintFactor",candidate.factor)&&
        ReadFloat(env,player,cls,"m_fCurrentMaxSquintZoom",candidate.maximum);
    bool owner_ok=fields_ok&&candidate.factor>=0&&candidate.factor<=1&&candidate.maximum>=1;
    // Bow concentration can magnify with ordinary Focus completely released.
    // Scoped Winchester still needs its distinct scope optics/overlay boundary.
    if(owner_ok)owner_ok=ReadSquintOwner(env,player,cls,candidate.factor);
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
