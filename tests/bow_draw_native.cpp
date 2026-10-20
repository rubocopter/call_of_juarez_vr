#include "games/call_of_juarez/bow_draw_native.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace {
using namespace cojvr::games::call_of_juarez;
using Value=bow_aim_detail::Value;
#if defined(_WIN32)
#define JNI_CALL __stdcall
#else
#define JNI_CALL
#endif
void Require(bool v,const char* text){if(!v){std::cerr<<text<<'\n';std::exit(1);}}
int player,bow,alias,replacement,foreign,player_class,bow_class,other_class,exception;
void* active[2]{};void* actual[2]{};void* desired[2]{};
void* table[224]{};void** holder=table;
std::map<void*,int> locals;
std::vector<int> required_null_steps;
int refs=0,steps=0,fail=-1,frame_calls=0,frame_fail=-1,change=0;
bool pending=false,exact=true,dead=false,carrying=false,can_attack=true,held_right=true,silent_null=false;
bool two[2]{false,true};int owner=1,state=129,holder_id=1,arrow_id=2;
std::uint32_t id=0xf1234567U;
CoJBowDrawFrame frames[3]{};
bool Step(){Require(!pending,"JNI call with pending exception");++steps;
    if(steps==fail){pending=!silent_null;return false;}return true;}
void* Local(void* p){if(p){++refs;++locals[p];}return p;}
void JNI_CALL Delete(void*,void* p){Require(locals[p]>0,"local reference underflow");--refs;if(!--locals[p])locals.erase(p);}
void* JNI_CALL Exception(void*){return pending?Local(&exception):nullptr;}
void JNI_CALL Clear(void*){pending=false;}
void* JNI_CALL Class(void*,void* p){if(!Step())return nullptr;required_null_steps.push_back(steps);
    return Local(p==&player?&player_class:exact?&bow_class:&other_class);}
void* JNI_CALL Find(void*,const char* n){if(!Step())return nullptr;required_null_steps.push_back(steps);
    Require(!std::strcmp(n,"WeaponBow"),"unexpected exact class");return Local(&bow_class);}
std::uint8_t JNI_CALL Same(void*,void* a,void* b){if(!Step())return 0;
    if(a==&alias)a=&bow;if(b==&alias)b=&bow;return a==b;}
void* JNI_CALL Method(void*,void*,const char* n,const char* d){if(!Step())return nullptr;
    required_null_steps.push_back(steps);
    const bool object=!std::strcmp(n,"GetActiveWeapon")||!std::strcmp(n,"GetActualWeaponNotEmpty")||!std::strcmp(n,"GetDesiredWeapon");
    const bool handbool=std::strstr(n,"OperatedTwoHand")||!std::strcmp(n,"CanAttack")||!std::strcmp(n,"GetAttackState");
    const char* expected=object?"(I)LWeapon;":handbool?"(I)Z":!std::strcmp(n,"GetActualHandForWeapon")?"(LWeapon;)I":
        !std::strcmp(n,"GetElementID")?"(Ljava/lang/String;)I":!std::strcmp(n,"GetHandStateMashineState")?"(I)I":
        !std::strcmp(n,"GetThisID")?"()I":"()Z";
    Require(!std::strcmp(d,expected),"wrong native method descriptor");return const_cast<char*>(n);}
void* JNI_CALL Object(void*,void* p,void* m,const Value* args){if(!Step())return nullptr;
    Require(p==&player&&args&&args[0].integer>=0&&args[0].integer<2,"wrong weapon getter");
    if(args[0].integer==1)required_null_steps.push_back(steps);
    int h=args[0].integer;auto n=static_cast<const char*>(m);
    if(!std::strcmp(n,"GetActiveWeapon"))return Local(active[h]);
    if(!std::strcmp(n,"GetActualWeaponNotEmpty"))return Local(actual[h]);
    Require(!std::strcmp(n,"GetDesiredWeapon"),"unexpected object getter");return Local(desired[h]);}
std::int32_t JNI_CALL Integer(void*,void* p,void* m,const Value* args){if(!Step())return -1;
    auto n=static_cast<const char*>(m);
    if(!std::strcmp(n,"GetActualHandForWeapon"))return owner;
    if(!std::strcmp(n,"GetThisID")){Require(p==&bow,"wrong ID owner");return static_cast<std::int32_t>(id);}
    if(!std::strcmp(n,"GetHandStateMashineState")){Require(p==&player&&args&&args[0].integer==1,"wrong state hand");return state;}
    Require(!std::strcmp(n,"GetElementID")&&p==&bow&&args,"unexpected integer getter");
    auto name=static_cast<const char*>(args[0].object);
    if(!std::strcmp(name,"DBowHolder"))return holder_id;
    Require(!std::strcmp(name,"DArrow"),"guessed nock element");return arrow_id;}
std::uint8_t JNI_CALL Boolean(void*,void* p,void* m,const Value* args){if(!Step())return 0;
    Require(p==&player,"wrong boolean owner");auto n=static_cast<const char*>(m);
    if(!std::strcmp(n,"IsNotAlive"))return dead;
    if(!std::strcmp(n,"IsCarrying"))return carrying;
    if(!std::strcmp(n,"CanAttack")){Require(args&&args[0].integer==1,"wrong permission hand");return can_attack;}
    if(!std::strcmp(n,"GetAttackState")){Require(args&&args[0].integer==0,"wrong held-right hand");return held_right;}
    Require(std::strstr(n,"OperatedTwoHand")&&args,"unexpected boolean getter");return two[args[0].integer];}
void* JNI_CALL Text(void*,const char* n){if(!Step())return nullptr;required_null_steps.push_back(steps);return Local(const_cast<char*>(n));}
bool Frame(void*,void* env,void* p,int element,CoJBowDrawFrame& out) noexcept {
    Require(env==&holder&&p==&bow&&element>=0&&element<3,"wrong geometry callback owner");
    ++frame_calls;if(frame_calls==frame_fail){pending=true;return false;}
    out=frames[element];
    if(frame_calls==3){if(change==1)actual[1]=desired[1]=active[1]=&replacement;
        if(change==2)++state;if(change==3)++id;if(change==4)held_right=!held_right;
        if(change==5)can_attack=!can_attack;if(change==6)++arrow_id;if(change==7)++holder_id;}
    return true;
}
void Reset(){Require(refs==0&&!pending&&locals.empty(),"previous read leaked refs/exception");
    steps=0;fail=-1;frame_calls=0;frame_fail=-1;change=0;silent_null=false;
    required_null_steps.clear();
    exact=true;dead=carrying=false;can_attack=held_right=true;two[0]=false;two[1]=true;
    owner=1;state=129;holder_id=1;arrow_id=2;id=0xf1234567U;
    active[0]=&alias;actual[0]=desired[0]=nullptr;active[1]=actual[1]=desired[1]=&bow;
    for(int i=0;i<3;++i){frames[i]={};frames[i].position={float(i),2,3};frames[i].valid=true;}}
bool Read(CoJBowDrawNativeObservation& out){return ReadCoJBowDrawNative(&holder,&player,out,Frame,nullptr);}
void Rejected(CoJBowDrawNativeObservation& out,const char* why){out.valid=true;out.weapon_id=19;
    Require(!Read(out)&&!out.valid&&out.weapon_id==0,why);Require(refs==0&&!pending,"rejection leaked refs/exception");}
}
int main(){
    table[6]=reinterpret_cast<void*>(Find);table[15]=reinterpret_cast<void*>(Exception);table[17]=reinterpret_cast<void*>(Clear);
    table[23]=reinterpret_cast<void*>(Delete);table[24]=reinterpret_cast<void*>(Same);table[31]=reinterpret_cast<void*>(Class);
    table[33]=reinterpret_cast<void*>(Method);table[36]=reinterpret_cast<void*>(Object);table[39]=reinterpret_cast<void*>(Boolean);
    table[51]=reinterpret_cast<void*>(Integer);table[167]=reinterpret_cast<void*>(Text);
    CoJBowDrawNativeObservation out{};Reset();
    Require(Read(out)&&out.valid&&out.weapon_id==id&&out.current_state==129&&out.can_attack&&out.held_right&&out.release_ready,
        "exact left bow ALT_MOVE must admit value-only native observation");
    const int calls=steps;Require(refs==0&&!pending&&frame_calls==3,"successful read leaked or skipped geometry");
    const auto null_calls=required_null_steps;
    Require(out.arrow_element==2&&out.holder_element==1&&out.arrow.position==frames[2].position&&out.root.forward==frames[0].forward,
        "DArrow/root+Z/holder not preserved");
    for(int s:{1,131,138,148,149,150,158,159,160,219}){Reset();state=s;
        Require(Read(out)&&out.valid&&!out.release_ready&&out.current_state==s,"unready/continuation state disappeared or permitted release");
        if(s==131)Require(out.gate==CoJBowDrawNativeGate::alt_start,"ALT_START not distinguished");}
    Reset();can_attack=false;Require(Read(out)&&out.valid&&!out.release_ready&&out.gate==CoJBowDrawNativeGate::native_denied,"native permission bypass");
    Reset();held_right=false;Require(Read(out)&&out.valid&&!out.release_ready&&out.gate==CoJBowDrawNativeGate::right_released,"missing right hold bypass");
    Require(!std::strcmp(CoJBowDrawNativeGateName(out.gate),"right_released"),"unstable gate log name");
    for(auto bad:{0U,0xffffffffU}){Reset();id=bad;Rejected(out,"invalid native identity admitted");}
    for(int c=1;c<=7;++c){Reset();change=c;Rejected(out,"unstable owner/state/identity/element admitted");}
    Reset();exact=false;Rejected(out,"derived/foreign class admitted");
    Reset();dead=true;Rejected(out,"dead player admitted");Reset();carrying=true;Rejected(out,"carrying player admitted");
    Reset();actual[0]=&alias;Rejected(out,"ambiguous actual owner admitted");
    Reset();desired[1]=&foreign;Rejected(out,"pending replacement admitted");
    Reset();actual[0]=desired[0]=active[0]=&bow;actual[1]=desired[1]=active[1]=nullptr;owner=0;two[0]=true;two[1]=false;
    Rejected(out,"right-owned bow admitted");
    for(int e=0;e<3;++e)for(int k=0;k<5;++k){Reset();
        if(k==0)frames[e].valid=false;if(k==1)frames[e].position[0]=std::numeric_limits<float>::quiet_NaN();
        if(k==2)frames[e].right={2,0,0};if(k==3)frames[e].up={.1F,1,0};if(k==4)frames[e].forward={0,0,-1};
        Rejected(out,"invalid raw geometry normalized or admitted");}
    Reset();arrow_id=-1;Rejected(out,"missing DArrow admitted");Reset();holder_id=-1;Rejected(out,"missing holder admitted");
    for(int f=1;f<=3;++f){Reset();frame_fail=f;Rejected(out,"geometry exception admitted");}
    for(int f=1;f<=calls;++f){Reset();fail=f;Rejected(out,"JNI exception admitted");}
    for(int f:null_calls){Reset();fail=f;silent_null=true;Rejected(out,"required JNI null without exception admitted");}
    for(int slot:{6,15,17,23,24,31,33,36,39,51,167}){Reset();void* saved=table[slot];table[slot]=nullptr;
        Rejected(out,"missing JNI function admitted");table[slot]=saved;}
    Reset();pending=true;Rejected(out,"entry exception admitted");
    Reset();Require(!ReadCoJBowDrawNative(nullptr,&player,out,Frame,nullptr)&&!out.valid,"null env admitted");
    Require(!ReadCoJBowDrawNative(&holder,nullptr,out,Frame,nullptr)&&!out.valid,"null player admitted");
    Require(!ReadCoJBowDrawNative(&holder,&player,out,nullptr,nullptr)&&!out.valid,"null callback admitted");
    for(int bad:{-1,256}){Reset();state=bad;Rejected(out,"invalid state admitted");}
    Reset();arrow_id=holder_id;Rejected(out,"aliased holder/nock element admitted");
    Reset();frames[0].right={0,0,-1};frames[0].forward={1,0,0};
    Require(Read(out)&&out.root.right==frames[0].right&&out.root.forward==frames[0].forward,"valid rotated native axes changed");
    Require(refs==0&&!pending,"final observation leaked");
    std::cout<<"bow native observation, owner/state gates, strict nock frames and "<<calls<<" JNI failure points passed\n";
}
