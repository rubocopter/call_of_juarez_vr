#include "games/call_of_juarez/long_gun_observer.hpp"
#include "games/call_of_juarez/long_gun_fire.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>

namespace {
using namespace cojvr::games::call_of_juarez;
void Require(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
void* P(int n){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(n));}
constexpr int player=100,weapon=101,alias=102,foreign=103,player_class=110,subclass=115;
void* table[224]{};void** env=table;std::map<void*,int> refs;
int steps=0,fail_step=-1,frames=0,kind=0,change=0,invalid_frame=-1,object_reads=0,owner_change_at=-1;
int contact_id=10;
int weapon_id_reads=0;
bool change_on_id_read=false,replace_on_id_read=false;
bool pending=false,dead=false,carrying=false,reloading=false,alias_desired=false;
bool replace_on_weapon_class=false;
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
    if(p!=P(player)&&replace_on_weapon_class)for(int i=0;i<3;++i)weapons[0][i]=P(foreign);
    return Local(P(p==P(player)?player_class:(kind==4?subclass:111+kind)));}
void* __stdcall Find(void*,const char* n){if(!Step())return nullptr;
    constexpr const char* classes[]{"WeaponRifleWinchester","WeaponRifleRemington","WeaponRifleRemingtonSawOff","WeaponRifleWinchesterScope"};
    for(int i=0;i<4;++i)if(!std::strcmp(n,classes[i]))return Local(P(111+i));
    Require(false,"unallowed FindClass");return nullptr;}
unsigned char __stdcall Same(void*,void* a,void* b){if(!Step())return 0;
    if(a==P(alias))a=P(weapon);if(b==P(alias))b=P(weapon);return a==b;}
void* __stdcall Lookup(void*,void* cls,const char* n,const char* signature){if(!Step())return nullptr;
    for(int i=0;i<15;++i)if(!std::strcmp(n,names[i])){
        const char* expected=i<3?"(I)LWeapon;":i<7?"(I)I":i<10?"(I)Z":i<13?"()Z":i==13?"()I":"(Ljava/lang/String;)I";
        Require(!std::strcmp(signature,expected),"wrong getter descriptor");
        Require((cls==P(player_class))||(i>=13&&(cls==P(111)||cls==P(112)||cls==P(113)||cls==P(114))),"wrong getter class");
        return P(i+1);
    }
    Require(false,"factory/setter or unknown getter requested");return nullptr;
}
void* __stdcall Object(void*,void* obj,void* method,const Value* args){if(!Step())return nullptr;
    if(++object_reads==owner_change_at)for(int i=0;i<3;++i)weapons[0][i]=P(foreign);
    const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    Require(obj==P(player)&&i>=0&&i<3&&args&&args->integer>=0&&args->integer<2,"bad object query");
    auto value=weapons[args->integer][i];
    if(alias_desired&&i==2&&args->integer==0)value=P(alias);
    return Local(value);
}
std::int32_t __stdcall Integer(void*,void* obj,void* method,const Value* args){if(!Step())return -1;
    const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    if(i==13){Require(args==nullptr,"ID getter arguments");
        if(obj!=P(player)){
            if(++weapon_id_reads==2&&change_on_id_read)++weapon_id;
            if(replace_on_id_read)for(int slot=0;slot<3;++slot)weapons[0][slot]=P(foreign);
        }
        return static_cast<std::int32_t>(obj==P(player)?player_id:weapon_id);}
    if(i==14){Require(args&&(args->object==P(120)||args->object==P(121)||args->object==P(122)),"unknown element name");
        Require((obj==P(player))==(args->object==P(121)),"element name belongs to wrong object");return args->object==P(120)?4:args->object==P(121)?10:contact_id;}
    Require(obj==P(player)&&i>=3&&i<7&&args&&args->integer>=0&&args->integer<2,"bad state query");
    return states[args->integer][i-3];
}
unsigned char __stdcall Boolean(void*,void* obj,void* method,const Value* args){if(!Step())return 0;
    Require(obj==P(player),"bad flag owner");const auto i=static_cast<int>(reinterpret_cast<std::uintptr_t>(method))-1;
    if(i>=10&&i<13){Require(args==nullptr,"permission getter arguments");return i==10?dead:i==11?carrying:reloading;}
    Require(i>=7&&i<10&&args&&args->integer>=0&&args->integer<2,"bad two-hand query");return two[args->integer][i-7];
}
void* __stdcall String(void*,const char* name){if(!Step())return nullptr;
    Require(!std::strcmp(name,"Holder")||!std::strcmp(name,"left_hand")||
        !std::strcmp(name,(kind==0||kind==3)?"GunBody":"GunBarrel"),"unknown mesh contact name");
    return Local(P(!std::strcmp(name,"Holder")?120:!std::strcmp(name,"left_hand")?121:122));}
bool Frame(void* context,void* object,int element,ElementWorldBasisTarget& out) noexcept {
    Require(context==P(500),"callback context lost");
    Require(refs[P(weapon)]==1,"weapon local not retained exactly once through frame callback");
    const int index=frames++;
    Require(index<4&&((index!=2&&object==P(weapon))||(index==2&&object==P(player))),"wrong frame object/order");
    Require(element==(index==0?0:index==1?4:10),"wrong native element");
    out={index==0?cojvr::runtime::Vec3{100,200,300}:index==1?cojvr::runtime::Vec3{105,205,290}:cojvr::runtime::Vec3{120,210,270},
         {0,1,0},{1,0,0},true};
    if(index==invalid_frame)out.forward=out.up;
    if(index==3){
        if(change==1)weapons[0][2]=P(foreign);
        if(change==2)states[0][1]=30;
        if(change==3)two[0][1]=false;
        if(change==4)++weapon_id;
        if(change==5)kind=1;
        if(change==6)dead=true;
        if(change==7)++player_id;
        if(change==8)++contact_id;
    }
    return Step(); // also exercise pending JNI exception from the adapter callback
}
void Reset(){steps=frames=0;fail_step=-1;kind=change=0;invalid_frame=-1;pending=dead=carrying=reloading=alias_desired=false;
    replace_on_weapon_class=false;
    weapon_id_reads=0;change_on_id_read=replace_on_id_read=false;
    contact_id=10;
    object_reads=0;owner_change_at=-1;
    player_id=0x8f120001;weapon_id=0xf1234001;
    for(int h=0;h<2;++h){for(int i=0;i<3;++i){weapons[h][i]=h?nullptr:P(weapon);two[h][i]=h==0;}for(int i=0;i<4;++i)states[h][i]=h?0:1;}
    Require(refs.empty(),"leaked locals before reset");}
bool Read(CoJLongGunSupportSnapshot& out){const bool ok=ReadCoJLongGunSupport(&env,P(player),out,&Frame,P(500));
    Require(refs.empty()&&!pending,"JNI locals/exception leaked");Require(ok==out.valid,"stale valid output");return ok;}
void Reject(bool geometry=false){CoJLongGunSupportSnapshot out{};out.valid=true;
    Require(!Read(out)&&!out.valid&&!out.weapon_class&&!out.support_in_weapon.valid,"invalid observation survived");
    if(!geometry)Require(frames==0,"geometry callback on rejected owner/state");}
void ScopedFireIdentity(){
    CoJLongGunFireRouting routing{};
    Reset();kind=0;
    Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&routing.right_button1_weapon==weapon_id,
        "ordinary Winchester R2 still reaches native aim/zoom instead of button1 fire");
    for(int model:{0,3})for(auto id:{0x12340001u,0x80000000u,0xf1234001u,0xfffffffeu}){
        Reset();kind=model;weapon_id=id;
        Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&routing.block_left&&!routing.block_right&&
            routing.right_button1_weapon==id,"stable exact scoped right-hand fire identity unavailable");
        Require(refs.empty()&&!pending&&frames==0,"scoped fire read leaked JNI or read support geometry");
    }
    const int total=steps;
    const auto reject=[&](const char* reason="failed scoped fire read leaked identity/routing/JNI state"){routing={true,true,123};
        Require(!ReadCoJLongGunFireOwnership(&env,P(player),routing)&&!routing.block_left&&!routing.block_right&&
            !routing.right_button1_weapon&&refs.empty()&&!pending&&frames==0,
            reason);};
    for(int i=1;i<=total;++i){Reset();kind=3;fail_step=i;reject();}
    for(auto id:{0u,0xffffffffu}){Reset();kind=3;weapon_id=id;reject();}
    Reset();kind=3;auto integer=table[51];table[51]=nullptr;reject();table[51]=integer;
    Reset();kind=3;change_on_id_read=true;reject("scoped fire identity was not bracketed against a changing GetThisID");
    Reset();kind=3;replace_on_id_read=true;reject();
    Reset();kind=3;replace_on_weapon_class=true;reject();
    Reset();kind=3;alias_desired=true;
    Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&routing.right_button1_weapon==weapon_id,
        "scoped fire identity rejected a JNI alias");
    for(int h=0;h<2;++h)for(int slot=0;slot<3;++slot){
        Reset();kind=3;weapons[h][slot]=h?P(foreign):nullptr;reject();
    }
    for(int k=0;k<5;++k){
        Reset();kind=k;
        if(k==0||k==3)for(int slot=0;slot<3;++slot){weapons[0][slot]=nullptr;weapons[1][slot]=P(weapon);}
        auto saved=table[51];table[51]=nullptr;
        Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&!routing.right_button1_weapon&&
            !weapon_id_reads&&refs.empty()&&!pending,"non-scoped-right route required GetThisID/JNI51 or leaked scope identity");
        table[51]=saved;
    }
    Reset();kind=3;for(int i=0;i<3;++i)weapons[1][i]=P(foreign);
    Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&!routing.right_button1_weapon&&!weapon_id_reads,
        "occupied support hand admitted scoped right ownership");
}
template<class T>void Set(int i,T f){table[i]=reinterpret_cast<void*>(f);}
}
int main(int argc,char** argv){
    Require(IsCoJLongGunSourceFrameRigid({1,2,3},{1,0,0},{0,1,0}),"rigid native source rejected");
    Require(!IsCoJLongGunSourceFrameRigid({1,2,3},{2,0,0},{0,1,0}),"scaled native source hidden by normalization");
    Require(!IsCoJLongGunSourceFrameRigid({1,2,3},{1,0,0},{.1F,1,0}),"sheared native source hidden by orthogonalization");
    Require(!IsCoJLongGunSourceFrameRigid({std::numeric_limits<float>::infinity(),0,0},
        {1,0,0},{0,1,0}),"nonfinite native source accepted");
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(31,Class);
    Set(33,Lookup);Set(36,Object);Set(39,Boolean);Set(51,Integer);Set(167,String);
    // Parent owns fire routing; allow support-only verification while that
    // independent implementation is in flight. Default runs both contracts.
    if(argc<2||std::strcmp(argv[1],"support-only"))ScopedFireIdentity();
    Reset();CoJLongGunSupportSnapshot out{};
    CoJLongGunFireRouting fire_routing{};
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&fire_routing.block_left&&!fire_routing.block_right,
        "right-only rifle native fire ownership not recognized without a grip");
    Require(refs.empty()&&!pending&&frames==0,"fire ownership read leaked JNI or sampled geometry");
    const int fire_steps=steps;
    for(int k=0;k<3;++k){
        Reset();kind=k;
        for(int i=0;i<3;++i){weapons[0][i]=nullptr;weapons[1][i]=P(weapon);}
        CoJLongGunFireRouting routing{};
        Require(ReadCoJLongGunFireOwnership(&env,P(player),routing)&&!routing.block_left&&routing.block_right,
            "left-only rifle did not protect its free right trigger");
        Require(refs.empty()&&!pending&&frames==0,"left rifle fire read leaked or sampled geometry");
        const int left_steps=steps;
        for(int failed=1;failed<=left_steps;++failed){
            Reset();kind=k;fail_step=failed;
            for(int i=0;i<3;++i){weapons[0][i]=nullptr;weapons[1][i]=P(weapon);}
            routing={true,true};
            Require(!ReadCoJLongGunFireOwnership(&env,P(player),routing)&&!routing.block_left&&!routing.block_right&&
                refs.empty()&&!pending,"left owner JNI fault retained stale fire routing or references");
        }
    }
    for(int k=0;k<3;++k){Reset();kind=k;reloading=true;states[0][0]=21;
        Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&fire_routing.block_left&&!fire_routing.block_right,
            "known rifle trigger ownership incorrectly depends on reload/idle pose");}
    Reset();kind=4;
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,"unknown weapon inherited rifle policy");
    Reset();for(int i=0;i<3;++i)weapons[1][i]=P(foreign);
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,"occupied left hand blocked as free");
    Reset();for(auto& hand:weapons)for(auto& slot:hand)slot=nullptr;
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
        "empty hands inherited a rifle trigger claim");
    Reset();kind=4;for(int i=0;i<3;++i){weapons[0][i]=nullptr;weapons[1][i]=P(weapon);}
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
        "unknown left-hand weapon inherited a rifle trigger claim");
    Reset();for(int i=0;i<3;++i)weapons[1][i]=P(weapon);
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
        "shared occupied weapon was mistaken for an empty hand");
    Reset();alias_desired=true;
    Require(ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&fire_routing.block_left&&!fire_routing.block_right,"fire owner used raw JNI alias equality");
    for(int h=0;h<2;++h)for(int slot=0;slot<3;++slot){
        Reset();weapons[h][slot]=h?P(foreign):nullptr;
        Require(!ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
            "partial weapon tuple admitted native other-hand attack fallback");
    }
    Reset();owner_change_at=7;
    Require(!ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
        "changing fire ownership tuple returned a known-safe route");
    Reset();replace_on_weapon_class=true;
    Require(!ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right,
        "post-classification owner replacement escaped final tuple verification");
    for(int i=1;i<=fire_steps;++i){Reset();fail_step=i;fire_routing={true,true};
        Require(!ReadCoJLongGunFireOwnership(&env,P(player),fire_routing)&&!fire_routing.block_left&&!fire_routing.block_right&&refs.empty()&&!pending,
            "failed fire ownership observation leaked locals/exception/stale verdict");}
    Reset();
    Require(Read(out),"idle exact rifle observation unavailable");const int total=steps;
    Require(frames==4&&out.contact_element==10&&out.contact_anchor.valid&&out.player_id==player_id&&out.weapon_id==weapon_id&&!out.firing,"baseline identity/phase/contact lost");
    Require(out.support_in_weapon.valid&&std::abs(out.support_in_weapon.position.x-30)<.001F&&
        std::abs(out.support_in_weapon.position.y-10)<.001F&&std::abs(out.support_in_weapon.position.z-20)<.001F,
        "translated/rotated full local frame incorrect");
    Require(std::abs(out.support_in_weapon.forward.z-1)<.001F,"local orientation not preserved");
    for(int k=0;k<4;++k){Reset();kind=k;Require(Read(out),"exact concrete rifle rejected");}
    Reset();kind=4;Reject(); // unknown subclass never passes exact class identity
    Reset();alias_desired=true;Require(Read(out),"JNI reference aliases must compare by IsSameObject");
    for(int h=0;h<2;++h)for(int i=0;i<3;++i){Reset();weapons[h][i]=h?P(weapon):P(foreign);Reject();}
    for(int i=0;i<3;++i){Reset();two[0][i]=false;Reject();Reset();two[1][i]=true;Reject();}
    for(int i=0;i<4;++i){Reset();states[1][i]=1;Reject();
        for(int value:{-1,0,10,20,21,22,40,48,129,158,999}){Reset();states[0][i]=value;Reject();}
        for(int value=30;value<=38;++value){Reset();states[0][i]=value;Require(Read(out)&&out.firing,"ordinary firing transition rejected");}}
    for(auto flag:{&dead,&carrying,&reloading}){Reset();*flag=true;Reject();}
    for(auto identity:{&player_id,&weapon_id})for(auto value:{0u,0xffffffffu}){Reset();*identity=value;Reject();}
    for(int i=0;i<4;++i){Reset();invalid_frame=i;Reject(true);}
    Reset();contact_id=-1;Reject();
    for(int i=1;i<=8;++i){Reset();change=i;Reject(true);}
    // Inject at every business JNI/callback operation from a successful traversal.
    for(int i=1;i<=total;++i){Reset();fail_step=i;Reject(true);}
    for(int index:{6,15,17,23,24,31,33,36,39,51,167}){Reset();auto saved=table[index];table[index]=nullptr;Reject();table[index]=saved;}
    Reset();pending=true;Reject();
    Require(!ReadCoJLongGunSupport(nullptr,P(player),out,&Frame,P(500)),"null env accepted");
    Require(!ReadCoJLongGunSupport(&env,nullptr,out,&Frame,P(500)),"null player accepted");
    Require(!ReadCoJLongGunSupport(&env,P(player),out,nullptr,P(500)),"null frame adapter accepted");
    // Scoped admission must retain the same bracketed owner/state/contact and
    // exception cleanup guarantees; no scope/alternate state is implied.
    Reset();kind=3;Require(Read(out)&&!out.firing,"scoped idle support unavailable");
    Require(!std::strcmp(out.weapon_class,"WeaponRifleWinchesterScope")&&out.contact_element==10,
        "scoped exact class/contact lost");
    const int scoped_steps=steps;
    for(int i=0;i<4;++i){
        for(int state=30;state<=38;++state){Reset();kind=3;states[0][i]=state;
            Require(Read(out)&&out.firing,"scoped ordinary fire/cycle state rejected");}
        Reset();kind=3;states[0][i]=129;
        Require(Read(out)&&!out.firing,"scoped alternate idle rejected or classified as firing");
        for(int state=158;state<=166;++state){Reset();kind=3;states[0][i]=state;
            Require(Read(out)&&out.firing,"scoped alternate fire/cycle state rejected");}
        for(int state:{0,20,21,22,29,39,40,48,128,130,148,149,157,167,168}){
            Reset();kind=3;states[0][i]=state;Reject();}
        Reset();kind=3;states[1][i]=1;Reject();
    }
    for(int h=0;h<2;++h)for(int i=0;i<3;++i){
        Reset();kind=3;weapons[h][i]=h?P(weapon):P(foreign);Reject();
        Reset();kind=3;two[h][i]=h!=0;Reject();
    }
    for(auto flag:{&dead,&carrying,&reloading}){Reset();kind=3;*flag=true;Reject();}
    for(int i=0;i<4;++i){Reset();kind=3;invalid_frame=i;Reject(true);}
    for(int i=1;i<=8;++i){Reset();kind=3;change=i;Reject(true);}
    for(int i=1;i<=scoped_steps;++i){Reset();kind=3;fail_step=i;Reject(true);}
    std::cout<<"exact rifle ownership/state/local-frame and exhaustive JNI cleanup passed\n";
}
