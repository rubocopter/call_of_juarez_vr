#pragma once
#include "games/call_of_juarez/reload_geometry.hpp"
#include <array>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Check the native paired +X/up getters before the existing frame reader can
// normalize or orthogonalize them. A repaired frame is not a natural reference.
inline bool IsCoJLongGunSourceFrameRigid(runtime::Vec3 position,runtime::Vec3 x,
    runtime::Vec3 up) noexcept {
    const auto finite=[](runtime::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto dot=[](runtime::Vec3 a,runtime::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    return finite(position)&&finite(x)&&finite(up)&&std::abs(dot(x,x)-1.F)<.002F&&
        std::abs(dot(up,up)-1.F)<.002F&&std::abs(dot(x,up))<.002F;
}
// Value-only native centimetre measurements. No contact/attachment admission.
// Caller supplies an exact-build admitted owner-thread player, stable gameplay
// generation/context and natural pre-overlay frames. Sampling cadence is external.
struct CoJLongGunSupportSnapshot {
    bool valid=false;
    const char* weapon_class=nullptr; // Static exact-class literal, never a JNI reference.
    std::uint32_t player_id=0,weapon_id=0;
    // Hand order: right/left. State order: current/destination/actual/desired.
    std::array<std::array<int,4>,2> states{};
    // Flag order: actual/desired/state machine.
    std::array<std::array<bool,3>,2> two_handed{};
    int holder_element=-1,support_element=-1; // Root is exact native element 0.
    ElementWorldBasisTarget root{},holder{},support{};
    CoJReloadLocalFrame support_in_weapon{};
    bool firing=false; // valid + false = ordinary idle; true = ordinary 30..38 transition.
};
using CoJLongGunReadFrame=bool(*)(void* context,void* object,int element,
    ElementWorldBasisTarget& out) noexcept;

namespace long_gun_observer_detail {
#if defined(_WIN32)
#define COJ_LONG_GUN_JNICALL __stdcall
#else
#define COJ_LONG_GUN_JNICALL
#endif
union Value {std::int64_t alignment;void* object;std::int32_t integer;};
static_assert(sizeof(Value)==8);
using Find=void*(COJ_LONG_GUN_JNICALL*)(void*,const char*);
using ObjectClass=void*(COJ_LONG_GUN_JNICALL*)(void*,void*);
using Lookup=void*(COJ_LONG_GUN_JNICALL*)(void*,void*,const char*,const char*);
using Object=void*(COJ_LONG_GUN_JNICALL*)(void*,void*,void*,const Value*);
using Integer=std::int32_t(COJ_LONG_GUN_JNICALL*)(void*,void*,void*,const Value*);
using Boolean=std::uint8_t(COJ_LONG_GUN_JNICALL*)(void*,void*,void*,const Value*);
using Same=std::uint8_t(COJ_LONG_GUN_JNICALL*)(void*,void*,void*);
using Exception=void*(COJ_LONG_GUN_JNICALL*)(void*);
using Clear=void(COJ_LONG_GUN_JNICALL*)(void*);
using Delete=void(COJ_LONG_GUN_JNICALL*)(void*,void*);
using String=void*(COJ_LONG_GUN_JNICALL*)(void*,const char*);
#undef COJ_LONG_GUN_JNICALL
struct Reader {
    void* env;
    bool ok=true;
    template<class T>T Fn(int i) const noexcept {return reinterpret_cast<T>((*static_cast<void***>(env))[i]);}
    bool Check() noexcept {
        void* ex=Fn<Exception>(15)(env);
        if(ex){Fn<Clear>(17)(env);Fn<Delete>(23)(env,ex);ok=false;}
        return ok;
    }
    struct Local {
        Reader& reader;void* value;
        Local(Reader& r,void* v):reader(r),value(v){}
        Local(const Local&)=delete;
        ~Local(){if(value)reader.Fn<Delete>(23)(reader.env,value);}
        operator void*()const noexcept{return value;}
    };
    Local Class(void* obj) noexcept {
        void* v=ok?Fn<ObjectClass>(31)(env,obj):nullptr;
        Check();if(!v)ok=false;return {*this,v};
    }
    Local FindClass(const char* name) noexcept {
        void* v=ok?Fn<Find>(6)(env,name):nullptr;
        Check();if(!v)ok=false;return {*this,v};
    }
    Local Text(const char* name) noexcept {
        void* v=ok?Fn<String>(167)(env,name):nullptr;
        Check();if(!v)ok=false;return {*this,v};
    }
    void* Method(void* cls,const char* name,const char* desc) noexcept {
        void* m=ok?Fn<Lookup>(33)(env,cls,name,desc):nullptr;
        Check();if(!m)ok=false;return m;
    }
    Local Get(void* obj,void* method,int hand) noexcept {
        Value arg{.integer=hand};void* v=ok?Fn<Object>(36)(env,obj,method,&arg):nullptr;
        Check();return {*this,v};
    }
    bool Equal(void* a,void* b) noexcept {
        const bool equal=ok&&Fn<Same>(24)(env,a,b)!=0;
        Check();return ok&&equal;
    }
    int Int(void* obj,void* method,const Value* args=nullptr) noexcept {
        const int v=ok?Fn<Integer>(51)(env,obj,method,args):-1;Check();return v;
    }
    bool Bool(void* obj,void* method,const Value* args=nullptr) noexcept {
        const bool v=ok&&Fn<Boolean>(39)(env,obj,method,args)!=0;Check();return v;
    }
};
inline constexpr const char* Classes[]{"WeaponRifleWinchester","WeaponRifleRemington","WeaponRifleRemingtonSawOff"};
inline constexpr const char* Names[]{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon",
    "GetHandStateMashineState","GetHandStateMashineDestinyState","GetActualWeaponState","GetDesiredWeaponState",
    "IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand","IsHandStateMashineOperatedTwoHand",
    "IsNotAlive","IsCarrying","IsWeaponReloading","GetThisID","GetElementID"};
inline constexpr const char* Descriptors[]{"(I)LWeapon;","(I)LWeapon;","(I)LWeapon;",
    "(I)I","(I)I","(I)I","(I)I","(I)Z","(I)Z","(I)Z","()Z","()Z","()Z","()I","(Ljava/lang/String;)I"};
inline bool ReadOwner(Reader& r,void* player,void* weapon,const std::array<void*,15>& methods,
    CoJLongGunSupportSnapshot& sample) noexcept {
    for(int i=10;i<13;++i)if(r.Bool(player,methods[i])||!r.ok)return false;
    bool firing=false;
    for(int h=0;h<2;++h){
        Value arg{.integer=h};
        for(int i=0;i<3;++i){
            auto observed=r.Get(player,methods[i],h);
            if(!r.ok || (h==0?(!observed.value||!r.Equal(observed,weapon)):observed.value!=nullptr))return false;
        }
        for(int i=0;i<4;++i){
            const int state=r.Int(player,methods[3+i],&arg);sample.states[h][i]=state;
            if(!r.ok || (h==1?state!=0:(state!=1&&(state<30||state>38))))return false;
            if(h==0&&state!=1)firing=true;
        }
        for(int i=0;i<3;++i){
            const bool flag=r.Bool(player,methods[7+i],&arg);sample.two_handed[h][i]=flag;
            if(!r.ok||flag!=(h==0))return false;
        }
    }
    sample.firing=firing;return true;
}
inline bool ValidId(std::uint32_t id) noexcept {return id!=0&&id!=0xffffffffu;}
} // namespace long_gun_observer_detail

inline bool ReadCoJLongGunSupport(void* env,void* player,CoJLongGunSupportSnapshot& out,
    CoJLongGunReadFrame read_frame,void* context) noexcept {
    using namespace long_gun_observer_detail;
    out={};
    if(!env||!player||!read_frame||!*static_cast<void***>(env))return false;
    for(int i:{6,15,17,23,24,31,33,36,39,51,167})if(!(*static_cast<void***>(env))[i])return false;
    Reader r{env};if(!r.Check())return false;
    auto player_class=r.Class(player);if(!r.ok)return false;
    std::array<void*,15> methods{};
    for(std::size_t i=0;i<methods.size();++i){
        methods[i]=r.Method(player_class,Names[i],Descriptors[i]);if(!r.ok)return false;
    }
    // Retain exactly one weapon local across the caller's native frame reads.
    auto weapon=r.Get(player,methods[0],0);if(!r.ok||!weapon.value)return false;
    auto weapon_class=r.Class(weapon);if(!r.ok)return false;
    CoJLongGunSupportSnapshot sample{};
    for(const char* name:Classes){
        auto cls=r.FindClass(name);if(!r.ok)return false;
        if(r.Equal(weapon_class,cls)){sample.weapon_class=name;break;}
        if(!r.ok)return false;
    }
    if(!sample.weapon_class)return false;
    void* weapon_id=r.Method(weapon_class,"GetThisID","()I");if(!r.ok)return false;
    void* weapon_element=r.Method(weapon_class,"GetElementID","(Ljava/lang/String;)I");if(!r.ok)return false;
    if(!ReadOwner(r,player,weapon,methods,sample))return false;
    sample.player_id=static_cast<std::uint32_t>(r.Int(player,methods[13]));
    sample.weapon_id=static_cast<std::uint32_t>(r.Int(weapon,weapon_id));
    if(!r.ok||!ValidId(sample.player_id)||!ValidId(sample.weapon_id))return false;
    auto holder_name=r.Text("Holder");if(!r.ok)return false;
    auto support_name=r.Text("left_hand");if(!r.ok)return false;
    Value holder_arg{.object=holder_name},support_arg{.object=support_name};
    sample.holder_element=r.Int(weapon,weapon_element,&holder_arg);
    sample.support_element=r.Int(player,methods[14],&support_arg);
    if(!r.ok||sample.holder_element<0||sample.support_element<0)return false;
    const auto frame=[&](void* object,int element,ElementWorldBasisTarget& target) noexcept {
        const bool read=read_frame(context,object,element,target);
        return r.Check()&&read;
    };
    if(!frame(weapon,0,sample.root)||!frame(weapon,sample.holder_element,sample.holder)||
        !frame(player,sample.support_element,sample.support))return false;
    CoJReloadLocalFrame holder_local{};
    if(!BuildCoJReloadLocalFrame(sample.root,sample.holder,holder_local)||
        !BuildCoJReloadLocalFrame(sample.root,sample.support,sample.support_in_weapon))return false;
    CoJLongGunSupportSnapshot after{};
    if(!ReadOwner(r,player,weapon,methods,after)||after.states!=sample.states||
        after.two_handed!=sample.two_handed)return false;
    auto class_after=r.Class(weapon);
    if(!r.ok||!r.Equal(class_after,weapon_class))return false;
    const auto player_after=static_cast<std::uint32_t>(r.Int(player,methods[13]));
    const auto weapon_after=static_cast<std::uint32_t>(r.Int(weapon,weapon_id));
    const int holder_after=r.Int(weapon,weapon_element,&holder_arg);
    const int support_after=r.Int(player,methods[14],&support_arg);
    if(!r.ok||player_after!=sample.player_id||weapon_after!=sample.weapon_id||
        holder_after!=sample.holder_element||support_after!=sample.support_element)return false;
    sample.valid=true;out=sample;return true;
}
} // namespace cojvr::games::call_of_juarez
