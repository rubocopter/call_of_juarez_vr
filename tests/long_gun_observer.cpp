#include "games/call_of_juarez/long_gun_observer.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>

namespace {
using namespace cojvr::games::call_of_juarez;
void Require(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
void* P(int n){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(n));}
constexpr int player=100,weapon=101,alias=102,foreign=103,player_class=110,subclass=114;
void* table[224]{};void** env=table;std::map<void*,int> refs;
int steps=0,fail_step=-1,frames=0,kind=0,change=0,invalid_frame=-1;
bool pending=false,dead=false,carrying=false,reloading=false,alias_desired=false;
std::uint32_t player_id=0x8f120001,weapon_id=0xf1234001;
void* weapons[2][3]{};int states[2][4]{};bool two[2][3]{};
const char* names[]{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon",
    "GetHandStateMashineState","GetHandStateMashineDestinyState","GetActualWeaponState","GetDesiredWeaponState",
    "IsActualWeaponOperatedTwoHand","IsDesiredWeaponOperatedTwoHand","IsHandStateMashineOperatedTwoHand",
    "IsNotAlive","IsCarrying","IsWeaponReloading","GetThisID","GetElementID"};
union Value{std::int64_t alignment;void* object;std::int32_t integer;};
static_assert(sizeof(Value)==8);
bool Step(){Require(!pending,"JNI call after uncleared exception");return ++steps==fail_step?(pending=true,false):true;}
void* Local(void* p){if(p)++refs[p];return p;}
void __stdcall Delete(void*,void* p){Require(refs[p]>0,"local ref underflow");if(!--refs[p])refs.erase(p);}
void* __stdcall Exception(void*){return pending?Local(P(999)):nullptr;}
void __stdcall Clear(void*){pending=false;}
void* __stdcall Class(void*,void* p){if(!Step())return nullptr;
    Require(p==P(player)||p==P(weapon)||p==P(alias)||p==P(foreign),"unexpected class owner");
    return Local(P(p==P(player)?player_class:(kind==3?subclass:111+kind)));}
void* __stdcall Find(void*,const char* n){if(!Step())return nullptr;
    constexpr const char* classes[]{"WeaponRifleWinchester","WeaponRifleRemington","WeaponRifleRemingtonSawOff"};
    for(int i=0;i<3;++i)if(!std::strcmp(n,classes[i]))return Local(P(111+i));
    Require(false,"unallowed FindClass");return nullptr;}
unsigned char __stdcall Same(void*,void* a,void* b){if(!Step())return 0;
    if(a==P(alias))a=P(weapon);if(b==P(alias))b=P(weapon);return a==b;}
void* __stdcall Lookup(void*,void* cls,const char* n,const char* signature){if(!Step())return nullptr;
    for(int i=0;i<15;++i)if(!std::strcmp(n,names[i])){
        const char* expected=i<3?"(I)LWeapon;":i<7?"(I)I":i<10?"(I)Z":i<13?"()Z":i==13?"()I":"(Ljava/lang/String;)I";
        Require(!std::strcmp(signature,expected),"wrong getter descriptor");
        Require((cls==P(player_class))||(i>=13&&(cls==P(111)||cls==P(112)||cls==P(113))),"wrong getter class");
        return P(i+1);
    }
    Require(false,"factory/setter or unknown getter requested");return nullptr;
}
void* __stdcall Object(void*,void* obj,void* method,const Value* args){if(!Step())return nullptr;
    const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    Require(obj==P(player)&&i>=0&&i<3&&args&&args->integer>=0&&args->integer<2,"bad object query");
    auto value=weapons[args->integer][i];
    if(alias_desired&&i==2&&args->integer==0)value=P(alias);
    return Local(value);
}
std::int32_t __stdcall Integer(void*,void* obj,void* method,const Value* args){if(!Step())return -1;
    const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    if(i==13){Require(args==nullptr,"ID getter arguments");return static_cast<std::int32_t>(obj==P(player)?player_id:weapon_id);}
    if(i==14){Require(args&&(args->object==P(120)||args->object==P(121)),"unknown element name");
        Require((obj==P(player))==(args->object==P(121)),"element name belongs to wrong object");return args->object==P(120)?4:10;}
    Require(obj==P(player)&&i>=3&&i<7&&args&&args->integer>=0&&args->integer<2,"bad state query");
    return states[args->integer][i-3];
}
unsigned char __stdcall Boolean(void*,void* obj,void* method,const Value* args){if(!Step())return 0;
    Require(obj==P(player),"bad flag owner");const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    if(i>=10&&i<13){Require(args==nullptr,"permission getter arguments");return i==10?dead:i==11?carrying:reloading;}
    Require(i>=7&&i<10&&args&&args->integer>=0&&args->integer<2,"bad two-hand query");return two[args->integer][i-7];
}
void* __stdcall String(void*,const char* name){if(!Step())return nullptr;
    Require(!std::strcmp(name,"Holder")||!std::strcmp(name,"left_hand"),"guessed support/foregrip name");
    return Local(P(!std::strcmp(name,"Holder")?120:121));}
bool Frame(void* context,void* object,int element,ElementWorldBasisTarget& out) noexcept {
    Require(context==P(500),"callback context lost");
    Require(refs[P(weapon)]==1,"weapon local not retained exactly once through frame callback");
    const int index=frames++;
    Require(index<3&&((index<2&&object==P(weapon))||(index==2&&object==P(player))),"wrong frame object/order");
    Require(element==(index==0?0:index==1?4:10),"wrong native element");
    out={index==0?cojvr::runtime::Vec3{100,200,300}:index==1?cojvr::runtime::Vec3{105,205,290}:cojvr::runtime::Vec3{120,210,270},
         {0,1,0},{1,0,0},true};
    if(index==invalid_frame)out.forward=out.up;
    if(index==2){
        if(change==1)weapons[0][2]=P(foreign);
        if(change==2)states[0][1]=30;
        if(change==3)two[0][1]=false;
        if(change==4)++weapon_id;
        if(change==5)kind=1;
        if(change==6)dead=true;
        if(change==7)++player_id;
    }
    return Step(); // also exercise pending JNI exception from the adapter callback
}
void Reset(){steps=frames=0;fail_step=-1;kind=change=0;invalid_frame=-1;pending=dead=carrying=reloading=alias_desired=false;
    player_id=0x8f120001;weapon_id=0xf1234001;
    for(int h=0;h<2;++h){for(int i=0;i<3;++i){weapons[h][i]=h?nullptr:P(weapon);two[h][i]=h==0;}for(int i=0;i<4;++i)states[h][i]=h?0:1;}
    Require(refs.empty(),"leaked locals before reset");}
bool Read(CoJLongGunSupportSnapshot& out){const bool ok=ReadCoJLongGunSupport(&env,P(player),out,&Frame,P(500));
    Require(refs.empty()&&!pending,"JNI locals/exception leaked");Require(ok==out.valid,"stale valid output");return ok;}
void Reject(bool geometry=false){CoJLongGunSupportSnapshot out{};out.valid=true;
    Require(!Read(out)&&!out.valid&&!out.weapon_class&&!out.support_in_weapon.valid,"invalid observation survived");
    if(!geometry)Require(frames==0,"geometry callback on rejected owner/state");}
template<class T>void Set(int i,T f){table[i]=reinterpret_cast<void*>(f);}
}
int main(){
    Require(IsCoJLongGunSourceFrameRigid({1,2,3},{1,0,0},{0,1,0}),"rigid native source rejected");
    Require(!IsCoJLongGunSourceFrameRigid({1,2,3},{2,0,0},{0,1,0}),"scaled native source hidden by normalization");
    Require(!IsCoJLongGunSourceFrameRigid({1,2,3},{1,0,0},{.1F,1,0}),"sheared native source hidden by orthogonalization");
    Require(!IsCoJLongGunSourceFrameRigid({std::numeric_limits<float>::infinity(),0,0},
        {1,0,0},{0,1,0}),"nonfinite native source accepted");
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(31,Class);
    Set(33,Lookup);Set(36,Object);Set(39,Boolean);Set(51,Integer);Set(167,String);
    Reset();CoJLongGunSupportSnapshot out{};
    Require(Read(out),"idle exact rifle observation unavailable");const int total=steps;
    Require(frames==3&&out.player_id==player_id&&out.weapon_id==weapon_id&&!out.firing,"baseline identity/phase lost");
    Require(out.support_in_weapon.valid&&std::abs(out.support_in_weapon.position.x-30)<.001F&&
        std::abs(out.support_in_weapon.position.y-10)<.001F&&std::abs(out.support_in_weapon.position.z-20)<.001F,
        "translated/rotated full local frame incorrect");
    Require(std::abs(out.support_in_weapon.forward.z-1)<.001F,"local orientation not preserved");
    for(int k=0;k<3;++k){Reset();kind=k;Require(Read(out),"exact concrete rifle rejected");}
    Reset();kind=3;Reject(); // unknown subclass/scope never passes exact class identity
    Reset();alias_desired=true;Require(Read(out),"JNI reference aliases must compare by IsSameObject");
    for(int h=0;h<2;++h)for(int i=0;i<3;++i){Reset();weapons[h][i]=h?P(weapon):P(foreign);Reject();}
    for(int i=0;i<3;++i){Reset();two[0][i]=false;Reject();Reset();two[1][i]=true;Reject();}
    for(int i=0;i<4;++i){Reset();states[1][i]=1;Reject();
        for(int value:{-1,0,10,20,21,22,40,48,129,158,999}){Reset();states[0][i]=value;Reject();}
        for(int value=30;value<=38;++value){Reset();states[0][i]=value;Require(Read(out)&&out.firing,"ordinary firing transition rejected");}}
    for(auto flag:{&dead,&carrying,&reloading}){Reset();*flag=true;Reject();}
    for(auto identity:{&player_id,&weapon_id})for(auto value:{0u,0xffffffffu}){Reset();*identity=value;Reject();}
    for(int i=0;i<3;++i){Reset();invalid_frame=i;Reject(true);}
    for(int i=1;i<=7;++i){Reset();change=i;Reject(true);}
    // Inject at every business JNI/callback operation from a successful traversal.
    for(int i=1;i<=total;++i){Reset();fail_step=i;Reject(true);}
    for(int index:{6,15,17,23,24,31,33,36,39,51,167}){Reset();auto saved=table[index];table[index]=nullptr;Reject();table[index]=saved;}
    Reset();pending=true;Reject();
    Require(!ReadCoJLongGunSupport(nullptr,P(player),out,&Frame,P(500)),"null env accepted");
    Require(!ReadCoJLongGunSupport(&env,nullptr,out,&Frame,P(500)),"null player accepted");
    Require(!ReadCoJLongGunSupport(&env,P(player),out,nullptr,P(500)),"null frame adapter accepted");
    std::cout<<"exact rifle ownership/state/local-frame and exhaustive JNI cleanup passed\n";
}
