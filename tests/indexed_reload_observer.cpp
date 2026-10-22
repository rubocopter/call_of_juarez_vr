#include "games/call_of_juarez/indexed_reload_observer.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <string>

#if defined(_WIN32)
#define TEST_JNICALL __stdcall
#else
#define TEST_JNICALL
#endif
namespace {
using namespace cojvr::games::call_of_juarez;
using ClassKind=CoJIndexedReloadClass;
using Pool=CoJIndexedReloadReserve;
void Need(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void* Ptr(int n){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(n));}
union Value{std::int64_t alignment;void* object;std::int32_t integer;};
static_assert(sizeof(Value)==8);
constexpr const char* Classes[]{"WeaponRifleRemington","WeaponRifleRemingtonSawOff","WeaponPistolDerringer","WeaponPistolLemant"};
constexpr const char* Slots[]{"GetActiveWeapon","GetActualWeaponNotEmpty","GetDesiredWeapon"};
constexpr const char* Pools[]{"nAmmoPistol","nAmmoRifle","nAmmoShotgun"};
void* slots[224]{};void** env=slots;
std::map<void*,int> locals;
std::map<std::string,int> names;
int next_local=1000,next_name=300,step=0,fail_at=-1,null_at=-1;
int weapon_class=0,hand=0,mutation=0,owner_pass=0,native_owner=1;
bool pending=false,return_on_fault=false,required_null=false;
std::uint32_t player_id=0x80000033u,weapon_id=0xf1234001u;
std::array<std::array<int,3>,2> present{};
std::array<std::int32_t,2> loaded{};
std::array<std::int32_t,3> reserves{};
std::array<int,3> reserve_reads{};
CoJIndexedReloadContext context{7,0x80001000u,19,42};
int Identity(void* ref){
    const auto it=locals.find(ref);
    return it==locals.end()?static_cast<int>(reinterpret_cast<std::uintptr_t>(ref)):it->second;
}
void* Local(int object){
    if(!object)return nullptr;
    auto ref=Ptr(next_local++);Need(locals.emplace(ref,object).second,"duplicate fake local");return ref;
}
bool Step(){Need(!pending,"forward JNI call with a pending exception");++step;
    if(step==fail_at){pending=true;return false;}return true;}
void* Returned(int object,bool success,bool required=false){
    if(step==null_at&&required)required_null=true;
    return step==null_at||(!success&&!return_on_fault)?nullptr:Local(object);
}
void TEST_JNICALL Delete(void*,void* ref){Need(locals.erase(ref)==1,"deleting a borrowed or already deleted JNI ref");}
void* TEST_JNICALL Exception(void*){return pending?Local(199):nullptr;}
void TEST_JNICALL Clear(void*){Need(pending,"clearing no exception");pending=false;}
unsigned char TEST_JNICALL Same(void*,void* a,void* b){
    if(!Step())return 0;return Identity(a)==Identity(b);
}
void* TEST_JNICALL ObjectClass(void*,void* obj){
    const bool ok=Step();const int identity=Identity(obj);
    Need(identity==1||identity==2||identity==3,"class read outside retained owner");
    return Returned(identity==1?10:20+weapon_class,ok,true);
}
void* TEST_JNICALL Find(void*,const char* name){
    const bool ok=Step();
    for(int i=0;i<4;++i)if(!std::strcmp(name,Classes[i]))return Returned(20+i,ok,true);
    Need(false,"unexpected exact-class lookup");return nullptr;
}
int Name(const char* name){auto [it,added]=names.try_emplace(name,next_name);if(added)++next_name;return it->second;}
std::string NameOf(void* id){
    for(const auto& [name,value]:names)if(id==Ptr(value))return name;
    Need(false,"unknown method/field handle");return {};
}
void* TEST_JNICALL Method(void*,void* cls,const char* name,const char* descriptor){
    if(!Step())return nullptr;
    if(step==null_at){required_null=true;return nullptr;}
    const int type=Identity(cls);
    bool approved=false;
    for(const auto* s:Slots)if(!std::strcmp(name,s))approved=type==10&&!std::strcmp(descriptor,"(I)LWeapon;");
    if(!std::strcmp(name,"GetThisID"))approved=(type==10||(type>=20&&type<24))&&!std::strcmp(descriptor,"()I");
    if(!std::strcmp(name,"GetAmmoCount"))approved=type>=20&&type<24&&!std::strcmp(descriptor,"(I)I");
    Need(approved,"non-getter, factory, wrong class or wrong method descriptor");return Ptr(Name(name));
}
void Mutate(){
    switch(mutation){
    case 1:present[hand][2]=3;break; // Same numeric ID and exact class, different Java object.
    case 2:native_owner=4;break;
    case 3:++weapon_id;break;
    case 4:++player_id;break;
    case 5:++loaded[1];break;
    case 6:--reserves[weapon_class==2?0:weapon_class==3?1:2];break;
    case 7:present[hand][2]=0;break;
    case 8:present[1-hand][0]=3;break;
    case 9:weapon_class=4;break;
    }
}
void* TEST_JNICALL Object(void*,void* obj,void* id,const Value* arg){
    const bool ok=Step();
    if(!ok)return Returned(2,false);
    Need(Identity(obj)==1&&arg&&arg->integer>=0&&arg->integer<2,"wrong hand/owner");
    const auto name=NameOf(id);
    if(name==Slots[0]&&arg->integer==0&&++owner_pass==2)Mutate();
    for(int i=0;i<3;++i)if(name==Slots[i])return Returned(present[arg->integer][i],true);
    Need(false,"unexpected object method");return nullptr;
}
std::int32_t TEST_JNICALL Integer(void*,void* obj,void* id,const Value* arg){
    if(!Step())return -1;
    const auto name=NameOf(id);const int owner=Identity(obj);
    if(name=="GetThisID"){
        Need(owner==1||owner==2,"unexpected ID owner");
        return static_cast<std::int32_t>(owner==1?player_id:weapon_id);
    }
    Need(name=="GetAmmoCount"&&owner==2&&arg&&arg->integer>=0&&arg->integer<2,"non-indexed or foreign loaded getter");
    return loaded[arg->integer];
}
void* TEST_JNICALL Field(void*,void* cls,const char* name,const char* descriptor){
    if(!Step())return nullptr;
    if(step==null_at){required_null=true;return nullptr;}
    const int type=Identity(cls);
    bool approved=!std::strcmp(name,"cOwner")&&type>=20&&type<24&&!std::strcmp(descriptor,"LPawnInventory;");
    for(int p=0;p<3;++p)if(!std::strcmp(name,Pools[p])){
        const bool used=weapon_class==3?p<2:weapon_class==2?p==0:p==2;
        approved=type==10&&used&&!std::strcmp(descriptor,"I");
    }
    Need(approved,"unproven field/pool or wrong descriptor");return Ptr(Name(name));
}
void* TEST_JNICALL ObjectField(void*,void* obj,void* id){
    const bool ok=Step();
    Need(Identity(obj)==2&&NameOf(id)=="cOwner","unexpected owner field");
    return Returned(native_owner,ok,true);
}
std::int32_t TEST_JNICALL IntField(void*,void* obj,void* id){
    if(!Step())return -1;
    Need(Identity(obj)==1,"foreign reserve owner");
    const auto name=NameOf(id);
    for(int p=0;p<3;++p)if(name==Pools[p]){++reserve_reads[p];return reserves[p];}
    Need(false,"unapproved integer field");return -1;
}
void Reset(int cls=0,int armed_hand=0){
    Need(locals.empty()&&!pending,"previous read leaked JNI state");
    step=0;fail_at=null_at=-1;return_on_fault=required_null=false;weapon_class=cls;hand=armed_hand;
    owner_pass=mutation=0;native_owner=1;context={7,0x80001000u,19,42};
    player_id=0x80000033u;weapon_id=0xf1234001u;
    present={};present[hand]={2,2,2};loaded={0,0};reserve_reads={};
    reserves=cls==3?std::array<std::int32_t,3>{12,10,0}:cls==2?std::array<std::int32_t,3>{12,0,0}:
        std::array<std::int32_t,3>{0,0,12};
}
bool Read(CoJIndexedReloadSnapshot& out){
    const bool result=ReadCoJIndexedReload(&env,Ptr(1),Ptr(2),hand,context,out);
    Need(locals.empty()&&!pending,"reader leaked JNI local or exception");
    Need(result==out.valid,"failure retained a valid snapshot");return result;
}
void Reject(){CoJIndexedReloadSnapshot out{};out.valid=true;out.loaded={1,1};
    Need(!Read(out)&&!out.valid&&out.player_id==0&&out.weapon_id==0&&out.loaded==std::array<std::int32_t,2>{},
        "rejected read published partial/stale values");}
template<class T>void Set(int index,T fn){slots[index]=reinterpret_cast<void*>(fn);}
void BaselinesAndFaults(){
    for(int cls=0;cls<4;++cls)for(int armed_hand:{0,1}){
        Reset(cls,armed_hand);CoJIndexedReloadSnapshot out{};
        Need(Read(out),"exact indexed family/hand rejected");const int calls=step;
        const ClassKind kinds[]{ClassKind::remington,ClassKind::saw_off,ClassKind::derringer,ClassKind::lemant};
        Need(out.weapon_class==kinds[cls]&&out.armed_hand==armed_hand&&out.player_id==0x80000033u&&
            out.weapon_id==0xf1234001u&&out.context==context&&out.reserves==reserves,"snapshot ownership/counts lost");
        const auto expected=cls==3?std::array<Pool,2>{Pool::rifle,Pool::pistol}:cls==2?
            std::array<Pool,2>{Pool::pistol,Pool::pistol}:std::array<Pool,2>{Pool::shotgun,Pool::shotgun};
        Need(out.reserve_kind==expected,"wrong static typed pool mapping");
        Need(reserve_reads==(cls==3?std::array<int,3>{2,2,0}:cls==2?
            std::array<int,3>{2,0,0}:std::array<int,3>{0,0,2}),"shared reserve read twice in one sample");
        for(bool nonnull:{false,true})for(int call=1;call<=calls;++call){
            Reset(cls,armed_hand);fail_at=call;return_on_fault=nonnull;Reject();
        }
    }
}
void OwnerAndCounts(){
    for(int cls=0;cls<4;++cls)for(int armed_hand:{0,1}){
        CoJIndexedReloadSnapshot out{};
        Reset(cls,armed_hand);native_owner=4;Reject();
        Reset(cls,armed_hand);native_owner=0;Reject();
        Reset(cls,armed_hand);present[hand][1]=3;Reject(); // replacement with same ID
        Reset(cls,armed_hand);present[1-hand][0]=3;Reject();
        Reset(cls,armed_hand);present[hand]={};Reject();
        Reset(cls,armed_hand);present[hand][0]=0;
        Need(Read(out),"stable missing active slot hid actual/desired ownership");
        Reset(cls,armed_hand);present[1-hand]={2,2,2};
        if(cls<2)Need(Read(out),"coherent same-weapon both-hand long gun rejected");else Reject();
        Reset(cls,armed_hand);present[1-hand][0]=2;Reject();
        for(int change=1;change<=9;++change){Reset(cls,armed_hand);mutation=change;Reject();}
        for(int index:{0,1}){
            for(int count:{-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),2}){
                if(cls==3&&index==1&&count==2)continue;
                Reset(cls,armed_hand);loaded[index]=count;Reject();
            }
        }
        Reset(cls,armed_hand);loaded={1,cls==3?8:1};Need(Read(out),"valid class capacity rejected");
        Reset(cls,armed_hand);reserves[cls==2?0:cls==3?1:2]=-1;Reject();
        Reset(cls,armed_hand);for(auto& count:reserves)if(count)count=std::numeric_limits<std::int32_t>::max();
        Need(Read(out),"positive jint reserve limit rejected without evidence");
    }
    Reset(3);loaded[1]=9;Reject();Reset(3);reserves[0]=-1;Reject();
    Reset(3);reserves[1]=std::numeric_limits<std::int32_t>::min();Reject();
    Reset();weapon_class=4;Reject(); // unknown subclass, not IsInstanceOf admission
    for(auto id:{0u,0xffffffffu})for(bool player:{false,true}){
        Reset();if(player)player_id=id;else weapon_id=id;Reject();
    }
    for(int which=0;which<3;++which){
        Reset();if(which==0)context.player_generation=0;
        if(which==1)context.native_context=0;if(which==2)context.object_epoch=0;Reject();
    }
    Reset();context.object_epoch=context.synchronous_scope=0;CoJIndexedReloadSnapshot out{};
    Need(Read(out),"passive observation incorrectly requires a synchronous call");
    Need(ClassifyCoJIndexedReloadDelta(out,out,0).kind==CoJIndexedReloadDeltaKind::unavailable,
        "sparse passive sample inferred an exact transfer");
}
void EntryAndMissingFunctions(){
    CoJIndexedReloadSnapshot out{};
    constexpr int required[]{6,15,17,23,24,31,33,36,51,94,95,100};
    for(int slot:required){Reset();auto saved=slots[slot];slots[slot]=nullptr;
        Need(!CoJIndexedReloadApiAvailable(&env)&&step==0,"bridge preflight called an incomplete JNI API");
        Reject();slots[slot]=saved;}
    Reset();Need(CoJIndexedReloadApiAvailable(&env)&&step==0,"complete JNI preflight performed native calls");
    Reset();pending=true;Reject();
    Reset();hand=-1;Reject();Reset();hand=2;Reject();
    Reset();Need(!ReadCoJIndexedReload(nullptr,Ptr(1),Ptr(2),0,context,out),"null env admitted");
    Need(!ReadCoJIndexedReload(&env,nullptr,Ptr(2),0,context,out),"null player admitted");
    Need(!ReadCoJIndexedReload(&env,Ptr(1),nullptr,0,context,out),"missing retained weapon admitted");
    void** no_functions=nullptr;
    Need(!ReadCoJIndexedReload(&no_functions,Ptr(1),Ptr(2),0,context,out),"null JNI table admitted");
    // Missing references/lookups without an exception must also fail cleanly.
    Reset();Need(Read(out),"baseline unavailable");const int calls=step;
    for(int call=1;call<=calls;++call){Reset();null_at=call;
        const bool result=Read(out);
        if(required_null)Need(!result&&!out.valid,"missing required reference/lookup accepted");}
}
}
int main(){
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);
    Set(31,ObjectClass);Set(33,Method);Set(36,Object);Set(51,Integer);
    Set(94,Field);Set(95,ObjectField);Set(100,IntField);
    BaselinesAndFaults();OwnerAndCounts();EntryAndMissingFunctions();
    std::cout<<"Indexed read-only snapshots, ownership and JNI cleanup passed\n";
}
#undef TEST_JNICALL
