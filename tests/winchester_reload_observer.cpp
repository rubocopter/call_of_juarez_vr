#include "games/call_of_juarez/winchester_reload_observer.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

namespace {
using namespace cojvr::games::call_of_juarez;
void Need(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void* Ptr(int n){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(n));}
union Value{std::int64_t alignment;void* object;std::int32_t integer;};
static_assert(sizeof(Value)==8);
void* slots[224]{};void** env=slots;
std::map<void*,int> locals;
std::map<std::string,int> names;
int next_name=1,step=0,fail_step=-1,pose_reads=0,change=0;
bool exception=false,missing_pose=false,other_class=false,other_owner=false,alias=false,foreign_left=false;
std::uint32_t player_id=0x80000033u,weapon_id=0xf1234001u;
int state[2][4]{{1,1,1,1},{0,0,0,0}};
bool two[2][3]{{true,true,true},{false,false,false}};
bool present[2][3]{{true,true,true},{false,false,false}};
int ammo=5,reserve=26,anim[2]{3,0};
float clock_time[2]{1.25F,0},duration[2]{1.5F,0};
bool Step(){Need(!exception,"call after uncleared JNI exception");return ++step==fail_step?(exception=true,false):true;}
void* Local(void* p){if(p)++locals[p];return p;}
void __stdcall Delete(void*,void* p){Need(locals[p]>0,"local reference underflow");if(!--locals[p])locals.erase(p);}
void* __stdcall Exception(void*){return exception?Local(Ptr(199)):nullptr;}
void __stdcall Clear(void*){exception=false;}
void* __stdcall Class(void*,void* p){if(!Step())return nullptr;
    if(p==Ptr(1))return Local(Ptr(11));
    if(p==Ptr(2)||p==Ptr(3))return Local(Ptr(other_class?13:12));
    if(p==Ptr(4)||p==Ptr(5))return Local(Ptr(14));
    Need(false,"unknown class object");return nullptr;}
void* __stdcall Find(void*,const char* cls){if(!Step())return nullptr;
    Need(!std::strcmp(cls,"WeaponRifleWinchester"),"unscoped class lookup");return Local(Ptr(12));}
unsigned char __stdcall Same(void*,void* a,void* b){if(!Step())return 0;
    if(a==Ptr(3))a=Ptr(2);if(b==Ptr(3))b=Ptr(2);return a==b;}
int Name(const char* n){auto [it,created]=names.try_emplace(n,next_name);if(created)++next_name;return it->second;}
void* __stdcall Lookup(void*,void*,const char* n,const char* descriptor){if(!Step())return nullptr;
    constexpr const char* weapon[]{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon"};
    constexpr const char* hand_state[]{"GetHandStateMashineState","GetHandStateMashineDestinyState",
        "GetActualWeaponState","GetDesiredWeaponState"};
    constexpr const char* hand_flag[]{"IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand",
        "IsHandStateMashineOperatedTwoHand"};
    const char* expected=nullptr;
    for(const auto* s:weapon)if(!std::strcmp(n,s))expected="(I)LWeapon;";
    for(const auto* s:hand_state)if(!std::strcmp(n,s))expected="(I)I";
    for(const auto* s:hand_flag)if(!std::strcmp(n,s))expected="(I)Z";
    for(const auto* s:{"IsNotAlive","IsCarrying","IsWeaponReloading"})if(!std::strcmp(n,s))expected="()Z";
    for(const auto* s:{"GetThisID"})if(!std::strcmp(n,s))expected="()I";
    for(const auto* s:{"GetAmmoCount"})if(!std::strcmp(n,s))expected="(I)I";
    for(const auto* s:{"GetElementID"})if(!std::strcmp(n,s))expected="(Ljava/lang/String;)I";
    for(const auto* s:{"GetHandStateMashine"})if(!std::strcmp(n,s))expected="(I)LPlayerStateMashine;";
    Need(expected&&!std::strcmp(expected,descriptor),"unknown or incorrectly typed getter");
    return Ptr(Name(n));}
void* __stdcall Object(void*,void* owner,void* id,const Value* args){if(!Step())return nullptr;
    const auto name=names.begin();(void)name;
    for(const auto& [n,v]:names)if(Ptr(v)==id){
        if(n=="GetActiveWeapon"||n=="GetActualWeaponNotEmpty"||n=="GetDesiredWeapon"){
            Need(owner==Ptr(1)&&args&&args->integer<2,"wrong hand weapon owner");
            const int index=n=="GetActiveWeapon"?0:n=="GetActualWeaponNotEmpty"?1:2;
            return Local(present[args->integer][index]?Ptr(foreign_left&&args->integer==1?8:alias&&index==2?3:2):nullptr);
        }
        if(n=="GetHandStateMashine"){Need(owner==Ptr(1)&&args,"wrong state machine owner");return Local(Ptr(args->integer?5:4));}
    }
    Need(false,"unknown object method");return nullptr;}
std::int32_t __stdcall Integer(void*,void* obj,void* id,const Value* arg){if(!Step())return -1;
    for(const auto& [n,v]:names)if(Ptr(v)==id){
        if(n=="GetThisID")return static_cast<std::int32_t>(obj==Ptr(1)?player_id:weapon_id);
        if(n=="GetAmmoCount"){Need(obj==Ptr(2)&&arg&&arg->integer==0,"wrong ammo index");return ammo;}
        if(n=="GetElementID")return arg&&arg->object==Ptr(21)?4:10;
        for(int i=0;i<4;++i){constexpr const char* props[]{"GetHandStateMashineState","GetHandStateMashineDestinyState","GetActualWeaponState","GetDesiredWeaponState"};
            if(n==props[i]){Need(obj==Ptr(1)&&arg,"wrong state owner");return state[arg->integer][i];}}
    }
    Need(false,"unknown int method");return -1;}
unsigned char __stdcall Boolean(void*,void* obj,void* id,const Value* arg){if(!Step())return 0;
    Need(obj==Ptr(1),"wrong bool owner");
    for(const auto& [n,v]:names)if(Ptr(v)==id){
        for(int i=0;i<3;++i){constexpr const char* flags[]{"IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand","IsHandStateMashineOperatedTwoHand"};
            if(n==flags[i]){Need(arg,"missing hand");return two[arg->integer][i];}}
        if(n=="IsNotAlive"||n=="IsCarrying")return 0;
        if(n=="IsWeaponReloading")return state[0][3]!=1;
    }
    Need(false,"unknown bool method");return 0;}
void* __stdcall Field(void*,void*,const char* n,const char* descriptor){if(!Step())return nullptr;
    const char* expected=nullptr;
    if(!std::strcmp(n,"cOwner"))expected="LPawnInventory;";
    if(!std::strcmp(n,"nAmmoRifle")||!std::strcmp(n,"m_iAnimID0"))expected="I";
    if(!std::strcmp(n,"m_fCurrentTime")||!std::strcmp(n,"m_fDuration"))expected="F";
    Need(expected&&!std::strcmp(descriptor,expected),"unapproved/incorrect field");return Ptr(Name(n));}
void* __stdcall ObjectField(void*,void*,void* id){if(!Step())return nullptr;
    Need(id==Ptr(Name("cOwner")),"unexpected object field");return Local(Ptr(other_owner?7:1));}
std::int32_t __stdcall IntField(void*,void* obj,void* id){if(!Step())return -1;
    if(id==Ptr(Name("nAmmoRifle"))){Need(obj==Ptr(1),"reserve owner");return reserve;}
    Need(id==Ptr(Name("m_iAnimID0")),"animation id field");return anim[obj==Ptr(5)];}
float __stdcall FloatField(void*,void* obj,void* id){if(!Step())return 0;
    Need(obj==Ptr(4)||obj==Ptr(5),"animation time owner");
    return id==Ptr(Name("m_fCurrentTime"))?clock_time[obj==Ptr(5)]:duration[obj==Ptr(5)];}
void* __stdcall String(void*,const char* s){if(!Step())return nullptr;
    Need(!std::strcmp(s,"Holder")||!std::strcmp(s,"left_hand"),"bad element name");return Local(Ptr(!std::strcmp(s,"Holder")?21:22));}
bool Frame(void*,void* obj,int element,ElementWorldBasisTarget& out) noexcept {
    const int i=pose_reads++;
    Need(i<3&&obj==(i==2?Ptr(1):Ptr(2))&&element==(i==0?0:i==1?4:10),"frame source mismatch");
    Need(locals[Ptr(2)]>0,"lost retained weapon in frame callback");
    out={{static_cast<float>(i),2,3},{0,1,0},{1,0,0},true};
    if(i==2){if(change==1)state[0][1]=22;if(change==2)++weapon_id;if(change==3)other_owner=true;
        if(change==4)ammo++;if(change==5)present[0][2]=false;}
    return Step()&&!missing_pose;
}
void Reset(){Need(locals.empty()&&!exception,"leaked local on previous read");step=pose_reads=0;fail_step=-1;change=0;
    missing_pose=other_class=other_owner=alias=foreign_left=false;player_id=0x80000033u;weapon_id=0xf1234001u;
    ammo=5;reserve=26;anim[0]=3;anim[1]=0;clock_time[0]=1.25F;clock_time[1]=0;duration[0]=1.5F;duration[1]=0;
    for(int h=0;h<2;++h)for(int i=0;i<3;++i){present[h][i]=h==0;two[h][i]=h==0;}
    for(int h=0;h<2;++h)for(int i=0;i<4;++i)state[h][i]=h==0?1:0;
}
bool Read(CoJWinchesterReloadSnapshot& out,bool pose=true){
    const bool result=ReadCoJWinchesterReload(&env,Ptr(1),out,pose?Frame:nullptr,nullptr);
    Need(locals.empty()&&!exception,"JNI reference or exception leaked");Need(result==out.valid,"stale observation on failure");return result;}
void Reject(bool geometry=false){CoJWinchesterReloadSnapshot out{};out.valid=true;
    Need(!Read(out)&&!out.valid,"failed read leaked stale snapshot");if(!geometry)Need(pose_reads==0,"invalid owner entered geometry");}
template<class F>void Set(int n,F f){slots[n]=reinterpret_cast<void*>(f);}
}
int main(){
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(31,Class);Set(33,Lookup);
    Set(36,Object);Set(39,Boolean);Set(51,Integer);Set(94,Field);Set(95,ObjectField);Set(100,IntField);
    Set(102,FloatField);Set(167,String);
    Reset();CoJWinchesterReloadSnapshot out{};
    Need(Read(out),"idle Winchester rejected");Need(out.valid&&out.pose_valid&&pose_reads==3&&
        out.player_id==player_id&&out.weapon_id==weapon_id&&out.ammo_readback==5&&out.rifle_reserve==26,"bad baseline");
    const int total=step;
    Reset();state[0][0]=20;state[0][1]=21;state[0][2]=22;state[0][3]=21;two[0][0]=false;
    Need(Read(out),"unknown native reload tuple rejected");Need(out.states[0][0]==20&&out.states[0][2]==22&&
        !out.two_handed[0][0]&&out.native_reloading,"native state/flag telemetry lost");
    Reset();alias=true;Need(Read(out),"JNI alias is a valid native owner");
    Reset();missing_pose=true;Need(Read(out)&&!out.pose_valid,"geometry failure blocked ammunition and state observation");
    Reset();Need(Read(out,false)&&!out.pose_valid&&pose_reads==0,"geometry-less read failed");
    Reset();other_class=true;Reject();Reset();other_owner=true;Reject();
    for(int i=0;i<3;++i){Reset();present[1][i]=true;Need(Read(out),"same Winchester temporary left ownership rejected");
        Reset();present[1][i]=true;foreign_left=true;Reject();}
    for(int i=1;i<=5;++i){Reset();change=i;Reject(true);}
    for(auto identity:{&player_id,&weapon_id})for(auto v:{0u,0xffffffffu}){Reset();*identity=v;Reject();}
    for(int i=1;i<=total;++i){Reset();fail_step=i;Reject(true);}
    for(int index:{6,15,17,23,24,31,33,36,39,51,94,95,100,102,167}){
        Reset();auto saved=slots[index];slots[index]=nullptr;Reject();slots[index]=saved;}
    Reset();Need(!ReadCoJWinchesterReload(nullptr,Ptr(1),out,nullptr,nullptr),"null JNI accepted");
    std::cout<<"Winchester native reload trace and JNI failure cleanup passed\n";
}
