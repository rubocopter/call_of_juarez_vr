#pragma once
#include <array>
#include <cstdint>

namespace cojvr::games::call_of_juarez {
// Exact WeaponBow has no Barrel. Its shipped fallback is element 0 position
// and +Z, not the firearm barrel's -X. DBowHolder is the authored grip socket.
// Caller owns exact-build/thread/gameplay admission. No JNI reference escapes.
struct CoJBowAimOwner {
    bool valid=false;
    int hand=-1;
    int holder_element=-1;
};
namespace bow_aim_detail {
#if defined(_WIN32)
#define COJ_BOW_CALL __stdcall
#else
#define COJ_BOW_CALL
#endif
union Value {std::int64_t alignment;void* object;std::int32_t integer;};
using Class=void*(COJ_BOW_CALL*)(void*,void*);
using Find=void*(COJ_BOW_CALL*)(void*,const char*);
using Method=void*(COJ_BOW_CALL*)(void*,void*,const char*,const char*);
using Object=void*(COJ_BOW_CALL*)(void*,void*,void*,const Value*);
using Int=std::int32_t(COJ_BOW_CALL*)(void*,void*,void*,const Value*);
using Bool=std::uint8_t(COJ_BOW_CALL*)(void*,void*,void*,const Value*);
using Same=std::uint8_t(COJ_BOW_CALL*)(void*,void*,void*);
using Exception=void*(COJ_BOW_CALL*)(void*);
using Clear=void(COJ_BOW_CALL*)(void*);
using Delete=void(COJ_BOW_CALL*)(void*,void*);
using Text=void*(COJ_BOW_CALL*)(void*,const char*);
#undef COJ_BOW_CALL
struct Reader {
    void* env;bool ok=true;
    template<class T>T Fn(int i)const noexcept{return reinterpret_cast<T>((*static_cast<void***>(env))[i]);}
    bool Check()noexcept{void* ex=Fn<Exception>(15)(env);if(ex){Fn<Clear>(17)(env);Fn<Delete>(23)(env,ex);ok=false;}return ok;}
    struct Local {
        Reader& r;void* value;
        Local(Reader& reader,void* v):r(reader),value(v){}
        Local(const Local&)=delete;
        ~Local(){if(value)r.Fn<Delete>(23)(r.env,value);}
    };
    bool Equal(void* a,void* b)noexcept{const bool equal=ok&&Fn<Same>(24)(env,a,b)!=0;return Check()&&equal;}
    void* Lookup(void* cls,const char* name,const char* desc)noexcept{
        void* m=ok?Fn<Method>(33)(env,cls,name,desc):nullptr;Check();if(!m)ok=false;return m;}
    Local Get(void* p,void* m,int hand)noexcept{
        Value arg{.integer=hand};void* v=ok?Fn<Object>(36)(env,p,m,&arg):nullptr;Check();return {*this,v};}
    int Integer(void* p,void* m,const Value* args=nullptr)noexcept{
        int v=ok?Fn<Int>(51)(env,p,m,args):-1;Check();return v;}
    bool Boolean(void* p,void* m,const Value* args=nullptr)noexcept{
        const bool v=ok&&Fn<Bool>(39)(env,p,m,args)!=0;Check();return v;}
};
}
// Read-only owner proof, repeated before return. ActiveWeapon may alias the bow
// in both hands; actual/desired ownership must name one and the same native hand.
// candidate, when supplied, must be that exact object (JNI aliases are valid).
inline bool ReadCoJBowAimOwner(void* env,void* player,CoJBowAimOwner& out,
    void* candidate=nullptr) noexcept {
    using namespace bow_aim_detail;
    out={};if(!env||!player||!*static_cast<void***>(env))return false;
    for(int i:{6,15,17,23,24,31,33,36,39,51,167})if(!(*static_cast<void***>(env))[i])return false;
    Reader r{env};if(!r.Check())return false;
    Reader::Local cls{r,r.Fn<Class>(31)(env,player)};
    if(!r.Check()||!cls.value)return false;
    std::array<void*,3> weapons{};
    int i=0;
    for(const char* name:{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon"}){
        weapons[i++]=r.Lookup(cls.value,name,"(I)LWeapon;");if(!r.ok)return false;}
    void* actual_hand=r.Lookup(cls.value,"GetActualHandForWeapon","(LWeapon;)I");
    std::array<void*,3> flags{};i=0;
    for(const char* name:{"IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand","IsHandStateMashineOperatedTwoHand"}){
        flags[i++]=r.Lookup(cls.value,name,"(I)Z");if(!r.ok)return false;}
    void* alive=r.Lookup(cls.value,"IsNotAlive","()Z");
    void* carrying=r.Lookup(cls.value,"IsCarrying","()Z");if(!r.ok)return false;
    auto right=r.Get(player,weapons[1],0);if(!r.ok)return false;
    auto left=r.Get(player,weapons[1],1);if(!r.ok)return false;
    if((right.value!=nullptr)==(left.value!=nullptr))return false;
    const int hand=right.value?0:1;void* bow=hand==0?right.value:left.value;
    Reader::Local bow_cls{r,r.Fn<Class>(31)(env,bow)};if(!r.Check()||!bow_cls.value)return false;
    Reader::Local exact{r,r.Fn<Find>(6)(env,"WeaponBow")};if(!r.Check()||!exact.value)return false;
    if(!r.Equal(bow_cls.value,exact.value)||(candidate&&!r.Equal(candidate,bow)))return false;
    void* element=r.Lookup(bow_cls.value,"GetElementID","(Ljava/lang/String;)I");if(!r.ok)return false;
    Reader::Local holder{r,r.Fn<Text>(167)(env,"DBowHolder")};if(!r.Check()||!holder.value)return false;
    Value arg{.object=holder.value};const int holder_id=r.Integer(bow,element,&arg);
    if(!r.ok||holder_id<1||holder_id>255)return false;
    const auto stable=[&]()noexcept{
        if(r.Boolean(player,alive)||!r.ok||r.Boolean(player,carrying)||!r.ok)return false;
        Value weapon_arg{.object=bow};if(r.Integer(player,actual_hand,&weapon_arg)!=hand||!r.ok)return false;
        for(int h=0;h<2;++h){
            for(int m=0;m<3;++m){
                auto current=r.Get(player,weapons[m],h);if(!r.ok)return false;
                // Only ActiveWeapon is allowed to alias the other hand.
                if(h==hand){if(!current.value||!r.Equal(current.value,bow))return false;}
                else if(current.value&&(m!=0||!r.Equal(current.value,bow)))return false;
            }
            Value hand_arg{.integer=h};
            for(void* f:flags)if(r.Boolean(player,f,&hand_arg)!=(h==hand)||!r.ok)return false;
        }
        return true;
    };
    if(!stable()||!stable()||r.Integer(bow,element,&arg)!=holder_id||!r.ok)return false;
    out={true,hand,holder_id};return true;
}
} // namespace cojvr::games::call_of_juarez
