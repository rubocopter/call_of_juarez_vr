// Exact-bow admission and root ray through the production bridge; no game/JVM.
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include "games/call_of_juarez/body_adapter.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"

namespace {
using namespace cojvr::games::call_of_juarez;
namespace runtime=cojvr::runtime;
void Require(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
struct Vector {float x=0,y=0,z=0;};
int player,bow,alias,replacement,replacement_alias,foreign,player_class,bow_class,other_class,vector_class,exception;
void* active[2]{&alias,&bow};void* actual[2]{nullptr,&bow};void* desired[2]{nullptr,&bow};
bool pending=false,exact=true,missing_holder=false,dead=false,carrying=false;
int refs=0,steps=0,fail_step=-1,mutate_at=-1,writes=0,globals=0;
int barrel=-1,owner=1;bool two[2]{false,true};bool fail_native_write=false;
ElementWorldBasisTarget root{{10,20,30},{0,1,0},{0,0,1},true};
ElementWorldBasisTarget holder_frame{{10.569491F,19.759628F,26.771015F},
    {.152608F,.960247F,-.233744F},{-.255036F,.266768F,.929404F},true};
ElementWorldBasisTarget replacement_root{},replacement_holder{};
bool IsReplacement(void* p){return p==&replacement||p==&replacement_alias;}
bool IsBow(void* p){return p==&bow||p==&alias||IsReplacement(p);}
std::array<Vector,8192> vectors{};int allocated=0;
std::array<void*,8> caches{};std::map<void*,int> locals;
void* held_bow=nullptr;
bool fail_hold_publish=false,fail_hold_clear=false;
bool fail_environment=false;
void* env_table[224]{};void** env_holder=env_table;
void* vm_table[8]{};void** vm_holder=vm_table;
bool Step(){Require(!pending,"JNI call after uncleared exception");
    ++steps;if(steps==mutate_at)desired[owner]=&foreign;
    if(steps==fail_step){pending=true;return false;}return true;}
void* Local(void* p){if(p){++refs;++locals[p];}return p;}
void __stdcall Delete(void*,void* p){Require(locals[p]>0,"local ref underflow");--refs;if(!--locals[p])locals.erase(p);}
void* __stdcall Exception(void*){return pending?Local(&exception):nullptr;}
void __stdcall Clear(void*){pending=false;}
std::int32_t __stdcall GetEnv(void*,void** out,std::int32_t){*out=fail_environment?nullptr:&env_holder;return fail_environment?-1:0;}
void* __stdcall Class(void*,void* p){if(!Step())return nullptr;
    return Local(p==&player?&player_class:(exact?&bow_class:&other_class));}
void* __stdcall Find(void*,const char* name){if(!Step())return nullptr;
    Require(!std::strcmp(name,"WeaponBow")||!std::strcmp(name,"WeaponWhip"),"unqualified class search");
    return Local(!std::strcmp(name,"WeaponBow")?&bow_class:&other_class);}
std::uint8_t __stdcall Instance(void*,void*,void*){if(!Step())return 0;return 0;}
std::uint8_t __stdcall Same(void*,void* a,void* b){if(!Step())return 0;
    if(a==&alias)a=&bow;if(b==&alias)b=&bow;
    if(a==&replacement_alias)a=&replacement;if(b==&replacement_alias)b=&replacement;return a==b;}
void* __stdcall Method(void*,void*,const char* name,const char*){if(!Step())return nullptr;return const_cast<char*>(name);}
void* __stdcall Object(void*,void* p,void* m,const JValue* args){if(!Step())return nullptr;
    if(!std::strcmp(static_cast<char*>(m),"GetPositionVectorVolatile")){
        Require(p==&player&&!args,"wrong actor position getter");vectors.back()={};return Local(&vectors.back());}
    Require(p==&player&&args&&args[0].i>=0&&args[0].i<2,"wrong weapon query");
    auto n=static_cast<const char*>(m);int h=args[0].i;
    if(!std::strcmp(n,"GetActiveWeapon"))return Local(active[h]);
    if(!std::strcmp(n,"GetActualWeaponNotEmpty")||!std::strcmp(n,"GetActualWeapon"))return Local(actual[h]);
    if(!std::strcmp(n,"GetDesiredWeapon"))return Local(desired[h]);
    Require(false,"unexpected object/factory query");return nullptr;}
int __stdcall Integer(void*,void* p,void* m,const JValue* args){if(!Step())return -1;
    auto n=static_cast<const char*>(m);
    if(!std::strcmp(n,"GetBarrelElement"))return barrel;
    if(!std::strcmp(n,"GetElementsNumber"))return 2;
    if(!std::strcmp(n,"GetActualHandForWeapon"))return owner;
    if(!std::strcmp(n,"GetElementID")){Require(IsBow(p),"wrong holder object");
        Require(args&&!std::strcmp(static_cast<char*>(args[0].l),"DBowHolder"),"guessed bow element");return missing_holder?-1:1;}
    Require(false,"unexpected integer getter");return -1;}
std::uint8_t __stdcall Boolean(void*,void* p,void* m,const JValue* args){if(!Step())return 0;
    auto n=static_cast<const char*>(m);
    if(!std::strcmp(n,"IsNotAlive"))return dead;
    if(!std::strcmp(n,"IsCarrying"))return carrying;
    if(std::strstr(n,"OperatedTwoHand"))return two[args[0].i];
    if(!std::strcmp(n,"GetPosition")){Require(p==&player&&args,"wrong actor position query");
        *static_cast<Vector*>(args[0].l)={};return 1;}
    Require(IsBow(p),"wrong frame object");
    if(!std::strcmp(n,"GetElementPos")||!std::strcmp(n,"GetElementLeftVector")||!std::strcmp(n,"GetElementUpVector")){
        Require(args&&args[0].i>=0&&args[0].i<2&&args[1].l,"wrong element frame read");
        const auto& frame=IsReplacement(p)?(args[0].i==0?replacement_root:replacement_holder):
            (args[0].i==0?root:holder_frame);
        const auto x=runtime::Vec3{frame.up.y*frame.forward.z-frame.up.z*frame.forward.y,
            frame.up.z*frame.forward.x-frame.up.x*frame.forward.z,frame.up.x*frame.forward.y-frame.up.y*frame.forward.x};
        const auto v=!std::strcmp(n,"GetElementPos")?frame.position:!std::strcmp(n,"GetElementUpVector")?frame.up:x;
        *static_cast<Vector*>(args[1].l)={v.x,v.y,v.z};return 1;}
    Require(false,"unexpected boolean/native query");return 0;}
void* __stdcall Text(void*,const char* name){if(!Step())return nullptr;
    Require(!std::strcmp(name,"DBowHolder"),"guessed bow holder name");return Local(const_cast<char*>("DBowHolder"));}
void* __stdcall New(void*,void*,void*,const JValue*){if(!Step())return nullptr;
    Require(allocated<static_cast<int>(vectors.size()),"vector fixture exhausted");return Local(&vectors[allocated++]);}
float __stdcall Float(void*,void* p,void* f){if(!Step())return 0;
    auto& v=*static_cast<Vector*>(p);return !std::strcmp(static_cast<char*>(f),"fX")?v.x:!std::strcmp(static_cast<char*>(f),"fY")?v.y:v.z;}
void __stdcall SetFloat(void*,void* p,void* f,float value){if(!Step())return;
    auto& v=*static_cast<Vector*>(p);(!std::strcmp(static_cast<char*>(f),"fX")?v.x:!std::strcmp(static_cast<char*>(f),"fY")?v.y:v.z)=value;}
void* __stdcall Global(void*,void* p){if(!Step())return nullptr;++globals;return p;}
void __stdcall DeleteGlobal(void*,void*){--globals;}
const char* cache_names[]{"cojvrRightOrigin","cojvrLeftOrigin","cojvrRightDirection","cojvrLeftDirection",
    "cojvrRightFxUp","cojvrLeftFxUp","cojvrRightFxForward","cojvrLeftFxForward"};
void* __stdcall Field(void*,void*,const char* name,const char*){if(!Step())return nullptr;return const_cast<char*>(name);}
int Cache(void* field){for(int i=0;i<8;++i)if(!std::strcmp(static_cast<char*>(field),cache_names[i]))return i;
    Require(false,"unknown cache field");return -1;}
void* __stdcall Get(void*,void* p,void* field){if(!Step())return nullptr;Require(p==&player,"wrong cache owner");return Local(caches[Cache(field)]);}
void __stdcall Set(void*,void* p,void* field,void* value){if(!Step())return;Require(p==&player,"wrong cache writer");
    if(!std::strcmp(static_cast<char*>(field),"cojvrPhysicalBowDrawWeapon")){
        if(!value&&fail_hold_clear){pending=true;return;}
        held_bow=value;if(value&&fail_hold_publish)pending=true;return;}
    caches[Cache(field)]=value;}
std::uint8_t __stdcall BoolField(void*,void*,void*){Require(false,"unexpected field read");return 0;}
void __stdcall NativeSet(void*,void* p,void* method,const JValue* args){if(!Step())return;
    Require(IsBow(p),"wrong native writer owner");
    Require(!std::strcmp(static_cast<char*>(method),"FromUpForwardPosElementWorld"),"unqualified native setter");
    ++writes;Require(args&&args[0].i>=0&&args[0].i<2,"wrong frame write");
    if(fail_native_write){pending=true;return;}
    auto v=[](void* ptr){auto& a=*static_cast<Vector*>(ptr);return runtime::Vec3{a.x,a.y,a.z};};
    auto& frame=IsReplacement(p)?(args[0].i==0?replacement_root:replacement_holder):(args[0].i==0?root:holder_frame);
    frame={v(args[3].l),v(args[1].l),v(args[2].l),true};}
bool Close(float a,float b){return std::abs(a-b)<.0002F;}
bool Equal(runtime::Vec3 a,runtime::Vec3 b){return Close(a.x,b.x)&&Close(a.y,b.y)&&Close(a.z,b.z);}
void Reset(){steps=0;fail_step=mutate_at=-1;pending=false;exact=true;missing_holder=dead=carrying=false;
    owner=1;barrel=-1;two[0]=false;two[1]=true;active[0]=&alias;active[1]=&bow;
    actual[0]=desired[0]=nullptr;actual[1]=desired[1]=&bow;}
}
int main(int argc,char** argv){
    env_table[6]=reinterpret_cast<void*>(Find);env_table[15]=reinterpret_cast<void*>(Exception);
    env_table[17]=reinterpret_cast<void*>(Clear);env_table[23]=reinterpret_cast<void*>(Delete);
    env_table[21]=reinterpret_cast<void*>(Global);env_table[22]=reinterpret_cast<void*>(DeleteGlobal);
    env_table[24]=reinterpret_cast<void*>(Same);env_table[30]=reinterpret_cast<void*>(New);
    env_table[31]=reinterpret_cast<void*>(Class);env_table[33]=reinterpret_cast<void*>(Method);
    env_table[32]=reinterpret_cast<void*>(Instance);
    env_table[36]=reinterpret_cast<void*>(Object);env_table[39]=reinterpret_cast<void*>(Boolean);
    env_table[51]=reinterpret_cast<void*>(Integer);env_table[63]=reinterpret_cast<void*>(NativeSet);
    env_table[102]=reinterpret_cast<void*>(Float);env_table[167]=reinterpret_cast<void*>(Text);
    env_table[94]=reinterpret_cast<void*>(Field);env_table[95]=reinterpret_cast<void*>(Get);
    env_table[96]=reinterpret_cast<void*>(BoolField);env_table[104]=reinterpret_cast<void*>(Set);
    env_table[111]=reinterpret_cast<void*>(SetFloat);
    vm_table[6]=reinterpret_cast<void*>(GetEnv);
    JavaPlayerBridge bridge;bridge.vm_=&vm_holder;bridge.being_=&player;
    bridge.vector_class_=&vector_class;bridge.vector_constructor_=const_cast<char*>("<init>");
    bridge.vector_x_field_=const_cast<char*>("fX");bridge.vector_y_field_=const_cast<char*>("fY");bridge.vector_z_field_=const_cast<char*>("fZ");
    bridge.get_position_vector_method_=const_cast<char*>("GetPositionVectorVolatile");
    bridge.element_world_read_lookup_attempted_=true;
    bridge.get_element_position_method_=const_cast<char*>("GetElementPos");
    bridge.get_element_left_method_=const_cast<char*>("GetElementLeftVector");
    bridge.get_element_up_method_=const_cast<char*>("GetElementUpVector");
    JavaPlayerPosition origin{},direction{};ElementWorldBasisTarget frame{};std::string error;
    const auto check_replacement=[&](bool before_commit){
        Reset();const auto original_root=root,original_holder=holder_frame;
        bool unarmed=false;ElementWorldBasisTarget target{};
        bridge.element_world_basis_lookup_attempted_=true;
        bridge.set_element_world_basis_method_=const_cast<char*>("FromUpForwardPosElementWorld");
        Require(bridge.TryPrepareTrackedWeapon(1,holder_frame.position,{100,200,300},{0,1,0},{0,0,-1},
            holder_frame,target,unarmed,&error),"replacement regression failed to capture bow A");
        if(!before_commit){
            Require(bridge.ApplyPreparedTrackedWeapon(1,origin,&error),"replacement regression failed to apply bow A");
            Require(bridge.TryPublishWeaponRay(1,origin,{0,1,0},true),"replacement regression failed to publish bow A");
        }
        // Bow B is a different object with identical frames to the retained A
        // plan. All current native ownership changes coherently, including the
        // other hand's ActiveWeapon alias; class, hand, flags and holder agree.
        replacement_root=bridge.weapon_overlays_[1].targets[0];
        replacement_holder=bridge.weapon_overlays_[1].targets[1];
        active[0]=&replacement_alias;active[1]=actual[1]=desired[1]=&replacement;
        const int before_writes=writes;
        if(before_commit){
            const bool committed=bridge.ApplyPreparedTrackedWeapon(1,origin,&error);
            Require(!committed&&writes==before_writes,
                "coherent A-to-B replacement must reject BEFORE the first commit write");
        }else{
            bool mapped=false;
            Require(!bridge.VerifyTrackedWeapon(1,mapped,&error)&&mapped,
                "coherent same-pose bow replacement passed retained-A verification");
            Require(!bridge.TryGetWeaponBarrel(1,origin,direction,&error),
                "coherent bow replacement escaped the retained-A ray binding");
            Require(!bridge.TryPublishWeaponRay(1,origin,{0,1,0},true)&&!caches[1]&&!caches[3],
                "coherent bow replacement retained old-A ballistic publication");
        }
        Require(globals==1&&bridge.weapon_overlays_[1].weapon==&bow&&bridge.weapon_overlays_[1].active,
            "replacement rejection discarded bow A's preimage/global owner");
        const auto untouched_b=replacement_root;
        Require(bridge.RestoreTrackedWeapons(&error)&&globals==0&&Equal(root.position,original_root.position)&&
            Equal(root.forward,original_root.forward)&&Equal(holder_frame.position,original_holder.position)&&
            Equal(replacement_root.position,untouched_b.position)&&Equal(replacement_root.forward,untouched_b.forward),
            "retained A did not restore or replacement B was mutated");
        Require(refs==0&&!pending,"coherent replacement leaked JNI refs/exception");
        Reset();writes=0;
    };
    if(argc>1){
        check_replacement(!std::strcmp(argv[1],"--replacement-commit"));
        bridge.being_=bridge.vector_class_=bridge.vm_=nullptr;
        std::cout<<"coherent bow replacement regression passed\n";return 0;
    }
    check_replacement(true);check_replacement(false);
    Require(bridge.TryGetWeaponBarrel(1,origin,direction,&error,nullptr,&frame),
        "exact bow with no Barrel must admit the native root ray on its left owner");
    Require(Equal({origin.x,origin.y,origin.z},{10,20,30})&&Equal({direction.x,direction.y,direction.z},{0,0,1}),
        "bow fallback must use root +Z/origin, not firearm -X or DArrow");
    Require(refs==0&&!pending&&writes==0&&globals==0,"bow read leaked/mutated");
    const int read_steps=steps;
    for(int fail=1;fail<=read_steps;++fail){Reset();fail_step=fail;
        Require(!bridge.TryGetWeaponBarrel(1,origin,direction,&error,nullptr,&frame),"JNI failure admitted bow ray");
        Require(refs==0&&!pending&&writes==0&&globals==0,"failed bow read leaked/mutated");}
    Reset();int observed_hand=-1;
    Require(bridge.TryObserveBowAimOwner(observed_hand)&&observed_hand==1,"ActiveWeapon alias stole native bow ownership");
    Require(!bridge.TryGetWeaponBarrel(0,origin,direction),"right active alias gained second bow ray");
    exact=false;Require(!bridge.TryGetWeaponBarrel(1,origin,direction),"nonbow missing barrel gained root fallback");
    Reset();missing_holder=true;Require(!bridge.TryObserveBowAimOwner(observed_hand),"missing authored holder admitted bow");
    Reset();desired[1]=&foreign;Require(!bridge.TryObserveBowAimOwner(observed_hand),"equip replacement admitted bow");
    Reset();actual[0]=&alias;Require(!bridge.TryObserveBowAimOwner(observed_hand),"ambiguous actual bow owner admitted");
    Reset();active[1]=nullptr;Require(!bridge.TryObserveBowAimOwner(observed_hand),"absent active owner admitted bow");
    Reset();two[1]=false;Require(!bridge.TryObserveBowAimOwner(observed_hand),"incoherent native two-hand flag admitted bow");
    Reset();dead=true;Require(!bridge.TryObserveBowAimOwner(observed_hand),"dead bow owner admitted");
    Reset();carrying=true;Require(!bridge.TryObserveBowAimOwner(observed_hand),"carrying bow owner admitted");
    Reset();mutate_at=30;Require(!bridge.TryObserveBowAimOwner(observed_hand)&&observed_hand==-1,
        "owner changed during observation but remained admitted");
    Reset();root.up={.1F,1,0};Require(!bridge.TryGetWeaponBarrel(1,origin,direction),"sheared bow source was repaired into admission");root.up={0,1,0};
    Reset();active[0]=active[1]=nullptr;actual[0]=actual[1]=desired[0]=desired[1]=nullptr;
    Require(!bridge.TryObserveBowAimOwner(observed_hand)&&writes==0&&globals==0,"absent bow performed native mutation");
    Reset();owner=0;actual[0]=desired[0]=active[0]=&bow;actual[1]=desired[1]=active[1]=nullptr;two[0]=true;two[1]=false;
    Require(bridge.TryObserveBowAimOwner(observed_hand)&&observed_hand==0,"qualified right bow owner was hardcoded left");
    Reset();const int before=writes;bool unarmed=false;ElementWorldBasisTarget hand_target{};
    const auto natural_root=root,natural_holder=holder_frame;
    bridge.element_world_basis_lookup_attempted_=true;
    bridge.set_element_world_basis_method_=const_cast<char*>("FromUpForwardPosElementWorld");
    Require(!bridge.TryPrepareTrackedWeapon(0,holder_frame.position,{100,200,300},{0,1,0},{0,0,-1},
        holder_frame,hand_target,unarmed)&&unarmed&&writes==before&&globals==0,
        "right ActiveWeapon alias must be unarmed with no map or native writes");
    const bool captured=bridge.TryPrepareTrackedWeapon(1,holder_frame.position,{100,200,300},{0,1,0},{0,0,-1},
        holder_frame,hand_target,unarmed,&error);
    if(!captured)std::cerr<<"capture detail: "<<error<<'\n';
    Require(captured&&!unarmed&&globals==1&&writes==before,
        "native bow owner failed capture or mutated before parent commit");
    Require(bridge.ApplyPreparedTrackedWeapon(1,origin,&error),"bow scoped commit/verification failed");
    Require(!bridge.TryPrepareTrackedWeapon(0,natural_holder.position,{100,200,300},{0,1,0},{0,0,-1},
        natural_holder,hand_target,unarmed)&&unarmed&&globals==1,
        "shared bow gained a duplicate map after owner commit");
    bool mapped=false;Require(bridge.VerifyTrackedWeapon(1,mapped)&&mapped&&Equal(root.forward,{0,1,0}),
        "committed bow root did not retain vertical pitch");
    Require(bridge.TryPublishBowDrawHold(true)&&held_bow==&bow,
        "qualified physical draw must publish its exact retained native bow owner");
    Require(bridge.TryPublishBowDrawHold(false)&&!held_bow,"physical release must clear native draw owner");
    fail_hold_publish=true;
    Require(!bridge.TryPublishBowDrawHold(true)&&!held_bow&&!pending,
        "throwing post-commit hold setter must roll back native protection");
    fail_hold_publish=false;
    bridge.bow_draw_hold_field_=nullptr;steps=0;
    Require(bridge.TryPublishBowDrawHold(true),"hold fault fixture must first establish valid publication");
    const auto hold_steps=steps;
    Require(bridge.TryPublishBowDrawHold(false),"hold fixture clear failed");
    for(int fault=1;fault<=hold_steps;++fault){
        Reset();bridge.bow_draw_hold_field_=nullptr;fail_step=fault;
        Require(!bridge.TryPublishBowDrawHold(true),"faulted hold publication escaped admission");
        fail_step=-1;
        Require(bridge.TryPublishBowDrawHold(false)&&!held_bow&&!pending&&refs==0,
            "hold fault leaked owner, local reference or exception");
    }
    Reset();
    Require(bridge.TryPublishBowDrawHold(true)&&held_bow==&bow,"same tracked bow may publish another current hold");
    fail_hold_clear=true;
    Require(!bridge.TryPublishBowDrawHold(false)&&held_bow==&bow&&bridge.bow_draw_hold_published_,
        "failed clear must retain cleanup ownership");
    fail_hold_clear=false;
    Require(bridge.TryPublishBowDrawHold(false)&&!held_bow&&!bridge.bow_draw_hold_published_,
        "clear retry must retire retained native hold owner");
    Require(bridge.TryPublishBowDrawHold(true),"hold must recover after successful clear");
    int replacement_player=0;
    const auto retained_globals=globals;
    const auto retained_generation=bridge.being_generation_;
    fail_hold_clear=true;
    Require(!bridge.ResolveBeingMethods(&env_holder,&replacement_player,&error)&&bridge.being_==&player&&
        bridge.being_generation_==retained_generation&&globals==retained_globals&&
        bridge.weapon_overlays_[1].active&&held_bow==&bow,
        "actor replacement must retain A and its transaction while A hold cleanup fails");
    fail_hold_clear=false;
    Require(bridge.TryPublishBowDrawHold(false)&&!held_bow,"retained actor A cleanup must be retryable");
    Require(bridge.TryPublishBowDrawHold(true),"cleaned actor A may reacquire native hold");
    fail_hold_clear=true;
    const auto retained_vm=bridge.vm_;
    const auto retained_vector=bridge.vector_class_;
    bridge.Reset();
    Require(bridge.being_==&player&&bridge.vm_==retained_vm&&bridge.vector_class_==retained_vector&&
        bridge.being_generation_==retained_generation&&globals==retained_globals&&
        bridge.weapon_overlays_[1].active&&bridge.bow_draw_hold_published_&&held_bow==&bow,
        "Reset must retain actor, VM, bindings and transaction when hold cleanup fails");
    fail_hold_clear=false;
    Require(bridge.TryPublishBowDrawHold(false),"Reset cleanup must be retryable");
    Require(bridge.TryPublishWeaponRay(1,origin,{0,1,0},true)&&caches[1]&&caches[3]&&!caches[0]&&!caches[2],
        "bow ray was missing or published to the alias hand");
    auto* ray=static_cast<Vector*>(caches[3]);Require(Equal({ray->x,ray->y,ray->z},{0,1,0}),"published bow ray lost pitch");
    desired[1]=&foreign;
    Require(!bridge.TryPublishBowDrawHold(true)&&!held_bow,
        "owner replacement must clear old native draw protection before rejection");
    Require(!bridge.VerifyTrackedWeapon(1,mapped)&&mapped,"changed bow owner retained verification");
    Require(!bridge.TryPublishWeaponRay(1,origin,{0,1,0},true)&&!caches[1]&&!caches[3],"changed bow owner retained ballistic cache");
    desired[1]=&bow;
    Require(bridge.TryPublishBowDrawHold(true),"frame can retain admitted bow hold before normal restoration");
    fail_native_write=true;
    Require(!bridge.RestoreTrackedWeapons(&error)&&globals==1&&bridge.weapon_overlays_[1].active&&
        bridge.weapon_overlay_faulted_,"failed bow restoration released its retained owner");
    fail_native_write=false;
    Require(bridge.RestoreTrackedWeapons(&error)&&globals==0&&Equal(root.position,natural_root.position)&&
        Equal(root.forward,natural_root.forward)&&Equal(holder_frame.position,natural_holder.position),"bow scoped restoration failed");
    Require(refs==0&&!pending,"bow transaction leaked JNI refs/exception");
    fail_environment=true;
    bridge.Reset();
    Require(bridge.being_==&player&&bridge.vm_==retained_vm&&bridge.vector_class_==retained_vector&&
        bridge.being_generation_==retained_generation&&bridge.bow_draw_hold_published_&&held_bow==&bow,
        "environment loss after frame restoration must not orphan a published bow hold");
    fail_environment=false;
    Require(bridge.TryPublishBowDrawHold(false)&&!held_bow,"environment recovery must clear retained hold");
    // Independent expected directions; a flatten-to-XZ regression fails ±pitch.
    const struct {runtime::Vec3 aim,up;} poses[]{
        {{0,.70710678F,.70710678F},{0,.70710678F,-.70710678F}},
        {{0,-.70710678F,.70710678F},{0,.70710678F,.70710678F}},
        {{0,1,0},{0,0,-1}},{{0,-1,0},{0,0,1}},{{1,0,0},{0,0,1}}};
    const ElementWorldBasisTarget holder{{.569491F,-.240372F,-3.228985F},
        {.152608F,.960247F,-.233744F},{-.255036F,.266768F,.929404F},true};
    for(const auto& pose:poses){
        auto plan=BuildTrackedWeaponFrame(root,holder.position,root.position,root.forward,{100,200,300},pose.aim,pose.up);
        Require(plan.valid&&Equal(plan.root.forward,pose.aim)&&Equal(plan.muzzle_direction,pose.aim),
            "bow map lost controller pitch or roll");
        auto mapped_holder=TransformWeaponElementFrame(root,plan.root,holder);
        Require(mapped_holder.valid&&Equal(mapped_holder.position,{100,200,300}),"bow holder slipped from grip");
        auto restored=TransformWeaponElementFrame(plan.root,root,mapped_holder);
        Require(restored.valid&&Equal(restored.position,holder.position),"bow root map did not roundtrip");
    }
    bridge.being_=bridge.vector_class_=bridge.vm_=nullptr;
    std::cout<<"bow root ray, full-pitch transaction, owner/alias/restore guards, "<<read_steps<<
        " ray and "<<hold_steps<<" hold JNI failure points passed\n";
}
