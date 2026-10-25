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
void* Method(void* env,void* cls,const char* name,const char* sig) noexcept {
    auto method=Fn<void*(__stdcall*)(void*,void*,const char*,const char*)>(env,33)(env,cls,name,sig);
    return Check(env)?method:nullptr;
}
bool ReadObject(void* env,void* owner,void* cls,const char* name,const char* sig,Local& out) noexcept {
    auto field=Field(env,cls,name,sig);if(!field)return false;
    out.value=Fn<void*(__stdcall*)(void*,void*,void*)>(env,95)(env,owner,field);
    return Check(env)&&out.value;
}
bool ObjectClass(void* env,void* object,Local& out) noexcept {
    out.value=Fn<void*(__stdcall*)(void*,void*)>(env,31)(env,object);
    return Check(env)&&out.value;
}
bool ScopeStateVisible(int state) noexcept {
    // Exact ArmedPlayerBeing.IsScopeVisible and EBeingOperateStates
    // IsAlt/IsAttackBeg/IsAttackLoop. Clear only the native alternate bit.
    const int base=state&~128;
    switch(base){case 1:case 3:case 96:case 97:case 98:return true;default:break;}
    if(!(state&128))return false;
    switch(base){
        case 30:case 31:case 33:case 34:case 36:case 37:
        case 40:case 41:case 43:case 44:case 46:case 47:
        case 50:case 51:case 53:case 54:case 56:case 57:
        case 60:case 61:case 63:case 64:case 66:case 67:
        case 70:case 71:case 73:case 74:case 76:case 77:
        case 80:case 81:case 83:case 84:case 86:case 87:return true;
        default:return false;
    }
}
bool ReadScopedOptics(void* env,void* player,void* cls,int hand,CoJFocusZoom& candidate) noexcept {
    constexpr int required[]{24,51,95,96,167};
    for(auto index:required)if(!(*static_cast<void***>(env))[index])return false;
    // Original code.pak f9db47c166e03f23...: GetWeaponParam creates on a
    // miss. Read the retained TreeMap entry directly, never GetLogicParams,
    // GetWeaponParam, GetScopeZoom, GetBeingZoom or a camera zoom update.
    Local module{env,nullptr},module_cls{env,nullptr},cache{env,nullptr},cache_cls{env,nullptr};
    if(!ReadObject(env,player,cls,"m_Module","LLawmanModule;",module)||
       !ObjectClass(env,module.value,module_cls)||
       !ReadObject(env,module.value,module_cls.value,"m_aWeaponParams","Ljava/util/TreeMap;",cache)||
       !ObjectClass(env,cache.value,cache_cls))return false;
    Local tree_cls{env,Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,"java/util/TreeMap")};
    if(!Check(env)||!tree_cls.value||!Same(env,cache_cls.value,tree_cls.value))return false;
    auto get=Method(env,cache_cls.value,"get","(Ljava/lang/Object;)Ljava/lang/Object;");
    if(!get)return false;
    Local key{env,Fn<void*(__stdcall*)(void*,const char*)>(env,167)(env,"WpnLogicRifleWinchesterScope")};
    if(!Check(env)||!key.value)return false;
    Value key_arg{.object=key.value};
    Local params{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,cache.value,get,&key_arg)};
    if(!Check(env)||!params.value)return false;
    Local params_cls{env,nullptr};
    if(!ObjectClass(env,params.value,params_cls))return false;
    Local expected_params{env,Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,"WpnLogicRifleWinchesterScope")};
    if(!Check(env)||!expected_params.value||!Same(env,params_cls.value,expected_params.value))return false;
    // Being.GetCurrentCamera is module -> cached CamerasManager -> current
    // array entry. Keep its manager and camera identities in the snapshot.
    Local manager{env,nullptr},manager_cls{env,nullptr};
    if(!ReadObject(env,module.value,module_cls.value,"m_CamerasManager","LCamerasManager;",manager)||
       !ObjectClass(env,manager.value,manager_cls))return false;
    auto current_camera=Method(env,manager_cls.value,"GetCurrentCamera","()LBeingCamera;");
    if(!current_camera)return false;
    Local camera{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,manager.value,current_camera,nullptr)};
    if(!Check(env)||!camera.value)return false;
    Local camera_cls{env,nullptr};
    if(!ObjectClass(env,camera.value,camera_cls))return false;
    auto dead_field=Field(env,cls,"bDead","Z");
    if(!dead_field)return false;
    const auto dead=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,96)(env,player,dead_field);
    if(!Check(env))return false;
    auto hand_state=Method(env,cls,"GetHandStateMashineState","(I)I");
    if(!hand_state)return false;
    Value hand_arg{.integer=hand};
    const int state=Fn<std::int32_t(__stdcall*)(void*,void*,void*,const Value*)>(env,51)(env,player,hand_state,&hand_arg);
    if(!Check(env))return false;
    float default_fov=0,parameter=0;
    // BaseCamera.GetDefaultFOV is exactly a read of this inherited field;
    // GetFOV would observe the already zoomed camera and compound the zoom.
    if(!ReadFloat(env,camera.value,camera_cls.value,"fDefaultFOV",default_fov)||
       !ReadFloat(env,params.value,params_cls.value,"m_fScopeZoom",parameter)||
       default_fov<=0||default_fov>=180||parameter<0)return false;
    Local current_module{env,nullptr},current_cache{env,nullptr},current_manager{env,nullptr};
    if(!ReadObject(env,player,cls,"m_Module","LLawmanModule;",current_module)||
       !Same(env,module.value,current_module.value)||
       !ReadObject(env,module.value,module_cls.value,"m_aWeaponParams","Ljava/util/TreeMap;",current_cache)||
       !Same(env,cache.value,current_cache.value))return false;
    Local current_params{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,cache.value,get,&key_arg)};
    if(!Check(env)||!Same(env,params.value,current_params.value)||
       !ReadObject(env,module.value,module_cls.value,"m_CamerasManager","LCamerasManager;",current_manager)||
       !Same(env,manager.value,current_manager.value))return false;
    Local latest_camera{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,manager.value,current_camera,nullptr)};
    if(!Check(env)||!Same(env,camera.value,latest_camera.value))return false;
    float latest=0;
    if(!ReadFloat(env,player,cls,"m_fSquintFactor",latest)||latest!=candidate.factor||
       !ReadFloat(env,player,cls,"m_fCurrentMaxSquintZoom",latest)||latest!=candidate.maximum||
       !ReadFloat(env,camera.value,camera_cls.value,"fDefaultFOV",latest)||latest!=default_fov||
       !ReadFloat(env,params.value,params_cls.value,"m_fScopeZoom",latest)||latest!=parameter)return false;
    const auto latest_dead=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,96)(env,player,dead_field);
    if(!Check(env)||latest_dead!=dead)return false;
    const int latest_state=Fn<std::int32_t(__stdcall*)(void*,void*,void*,const Value*)>(env,51)(env,player,hand_state,&hand_arg);
    if(!Check(env)||latest_state!=state)return false;
    candidate.scoped=true;
    // IsScopeVisible returns on the FIRST actual hand with positive scope
    // zoom, including false for an ineligible state; it does not OR the hands.
    candidate.scope_visible=!dead&&candidate.factor==1&&parameter>0&&ScopeStateVisible(state);
    if(candidate.scope_visible){
        constexpr double radians=0.017453292519943295;
        // Native DegreesToPercents rounds tan(degrees)*100 to float before
        // the float multiply/divide in GetScopeZoom. The authored 8 is data.
        const float camera_tan=static_cast<float>(std::tan(default_fov*.5*radians)*100);
        const float reference_tan=static_cast<float>(std::tan(40*radians)*100);
        candidate.maximum=parameter*camera_tan/reference_tan;
        if(!std::isfinite(candidate.maximum)||candidate.maximum<1)return false;
    }
    return true;
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
bool ReadSquintOwner(void* env,void* player,void* cls,CoJFocusZoom& candidate) noexcept {
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
    bool has_bow=false,has_scope=false;void* owner=nullptr;
    for(int i=0;i<4;++i){
        Value args{.integer=i/2};auto& weapon=weapons[i];
        weapon.value=Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,player,methods[i%2],&args);
        if(!Check(env))return false;
        if(!weapon.value)continue;
        const bool scoped=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,weapon.value,scope.value)!=0;
        if(!Check(env))return false;
        if(scoped){has_scope=true;owner=weapon.value;}
        const bool is_bow=Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,weapon.value,bow.value)!=0;
        if(!Check(env))return false;
        if(is_bow){has_bow=true;owner=weapon.value;}
    }
    if(has_bow&&has_scope)return false;
    if(!has_bow&&!has_scope)return true;
    // GetWeaponBow observes desired hands, while the native maximum cache
    // comes from actual hands. Bow and scope each require one stable owner,
    // including JNI aliases in both hands, after equip/unequip agrees.
    if(!(*static_cast<void***>(env))[24]||!(*static_cast<void***>(env))[95])return false;
    for(int hand=0;hand<2;++hand){
        auto actual=weapons[hand*2].value;auto desired=weapons[hand*2+1].value;
        if(!Same(env,actual,desired))return false;
        if(actual&&!Same(env,actual,owner))return false;
    }
    void* identity_method=nullptr;
    if(has_scope){
        Local exact_class{env,nullptr};
        if(!(*static_cast<void***>(env))[51]||!ObjectClass(env,owner,exact_class)||
           !Same(env,exact_class.value,scope.value))return false;
        identity_method=Method(env,exact_class.value,"GetThisID","()I");
        if(!identity_method)return false;
        const auto id=Fn<std::int32_t(__stdcall*)(void*,void*,void*,const Value*)>(env,51)(env,owner,identity_method,nullptr);
        if(!Check(env))return false;
        candidate.scoped_weapon=static_cast<std::uint32_t>(id);
        if(candidate.scoped_weapon==0||candidate.scoped_weapon==0xffffffffU||
           !ReadScopedOptics(env,player,cls,weapons[0].value?0:1,candidate))return false;
    }else if(!ReadBowConcentration(env,player,cls,candidate.factor))return false;
    for(int i=0;i<4;++i){
        Value args{.integer=i/2};
        Local current{env,Fn<void*(__stdcall*)(void*,void*,void*,const Value*)>(env,36)(env,player,methods[i%2],&args)};
        if(!Check(env)||!Same(env,weapons[i].value,current.value))return false;
    }
    if(has_scope){
        const auto id=Fn<std::int32_t(__stdcall*)(void*,void*,void*,const Value*)>(env,51)(env,owner,identity_method,nullptr);
        if(!Check(env)||static_cast<std::uint32_t>(id)!=candidate.scoped_weapon)return false;
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
    // Scoped optics observe the exact retained parameter/default-camera cache
    // and native visibility predicate. Display masking belongs to the caller.
    if(owner_ok)owner_ok=ReadSquintOwner(env,player,cls,candidate);
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
    const auto finite=[](runtime::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!optical[0].eye_to_head.position_valid||!optical[1].eye_to_head.position_valid||
        !finite(optical[0].eye_to_head.position)||!finite(optical[1].eye_to_head.position))return false;
    const runtime::Vec3 centre{
        (optical[0].eye_to_head.position.x+optical[1].eye_to_head.position.x)*.5F,
        (optical[0].eye_to_head.position.y+optical[1].eye_to_head.position.y)*.5F,
        (optical[0].eye_to_head.position.z+optical[1].eye_to_head.position.z)*.5F};
    auto scaled=optical;
    for(auto& eye:scaled){
        auto angle=[&](float a){return std::atan(std::tan(a)/zoom.magnification);};
        eye.fov={angle(eye.fov.angle_left),angle(eye.fov.angle_right),angle(eye.fov.angle_up),angle(eye.fov.angle_down)};
        if(!ValidFov(eye.fov))return false;
        // Narrowing angular coverage multiplies disparity by magnification.
        // Compensate only the copied scene cameras about their unchanged centre;
        // runtime headset/IPD, tracked poses, UI and restored native camera stay
        // physical. No independently converging/toed-in eye cameras are created.
        auto& p=eye.eye_to_head.position;
        p={centre.x+(p.x-centre.x)/zoom.magnification,
           centre.y+(p.y-centre.y)/zoom.magnification,
           centre.z+(p.z-centre.z)/zoom.magnification};
    }
    render=scaled;return true;
}
}
