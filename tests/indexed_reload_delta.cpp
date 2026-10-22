#include "games/call_of_juarez/indexed_reload_delta.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace cojvr::games::call_of_juarez;
using Class=CoJIndexedReloadClass;
using Pool=CoJIndexedReloadReserve;
using Kind=CoJIndexedReloadDeltaKind;
void Need(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}

// Literal fixtures deliberately do not obtain capacities or pool maps from the
// production profile: an incorrect profile must break the accounting tests.
CoJIndexedReloadSnapshot Sample(Class cls=Class::remington,int hand=0){
    CoJIndexedReloadSnapshot v{};
    v.valid=true;v.weapon_class=cls;v.armed_hand=hand;
    v.player_id=0x80000033u;v.weapon_id=0xf1234001u;
    v.context={7,0x80001000u,19,42};
    v.weapon_present[hand]={true,true,true};
    if(cls==Class::lemant){v.reserve_kind={Pool::rifle,Pool::pistol};v.reserves={12,10,0};}
    else if(cls==Class::derringer){v.reserve_kind={Pool::pistol,Pool::pistol};v.reserves={12,0,0};}
    else {v.reserve_kind={Pool::shotgun,Pool::shotgun};v.reserves={0,0,12};}
    return v;
}
void SharedPools(){
    for(auto cls:{Class::remington,Class::saw_off,Class::derringer})for(int hand:{0,1}){
        auto before=Sample(cls,hand),after=before;
        const int pool=cls==Class::derringer?0:2;
        Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::no_change,"unchanged vector misclassified");
        after.loaded[0]=1;--after.reserves[pool];
        auto delta=ClassifyCoJIndexedReloadDelta(before,after,0);
        Need(delta.kind==Kind::coherent_unit_insertion&&delta.loaded==std::array<std::int64_t,2>{1,0}&&
            delta.reserves[pool]==-1&&delta.units==1,"selected shared-pool unit lost");
        Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::mismatch,"wrong selected index accepted");
        after=before;after.loaded[1]=1;--after.reserves[pool];
        Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::coherent_unit_insertion,"index one ignored");
        after.loaded[0]=1;--after.reserves[pool];
        delta=ClassifyCoJIndexedReloadDelta(before,after,0);
        Need(delta.kind==Kind::coherent_native_refill&&delta.units==2&&delta.reserves[pool]==-2,
            "two-slot native refill must debit one shared pool twice, not duplicate its count");
        ++after.reserves[pool];
        Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"under-debited two-slot refill accepted");
    }
}
void MixedPools(){
    auto before=Sample(Class::lemant),after=before;
    after.loaded[0]=1;--after.reserves[1];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::coherent_unit_insertion,"Lemant rifle unit lost");
    before.loaded[0]=1;after=before;after.loaded[1]=1;--after.reserves[0];
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::coherent_unit_insertion,"Lemant pistol unit lost");
    --after.reserves[1];
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::mismatch,"foreign involved reserve changed");
    after=before;after.loaded[1]=1;--after.reserves[1];
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::mismatch,"wrong typed debit accepted");
    before=Sample(Class::lemant);after=before;after.loaded={1,1};--after.reserves[0];--after.reserves[1];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"non-whole-clip Lemant acquired a multi-unit native classification");
    after=before;after.loaded[1]=2;after.reserves[0]-=2;
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::mismatch,"Lemant cannot natively refill two pistol units in one call");
    // Capacity eight is valid, nine is not, even when the vector conserves ammo.
    before.loaded={1,7};after=before;after.loaded[1]=8;--after.reserves[0];
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::coherent_unit_insertion,"Lemant eighth round rejected");
    before=after;after.loaded[1]=9;--after.reserves[0];
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::unavailable,"Lemant ninth round accepted");
}
void Mutations(){
    auto before=Sample(),after=before;
    after.loaded[0]=1;
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"partial loaded-only mutation accepted");
    after=before;--after.reserves[2];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"reserve-only mutation accepted");
    before.loaded={1,0};after=before;after.loaded={0,1};
    Need(ClassifyCoJIndexedReloadDelta(before,after,1).kind==Kind::mismatch,"net-zero barrel exchange accepted");
    after=before;--after.loaded[0];++after.reserves[2];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"unloading classified as insertion");
    after=before;++after.reserves[2];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::mismatch,"pickup classified as reload");
    constexpr auto max=std::numeric_limits<std::int32_t>::max();
    before=Sample();before.reserves[2]=max;after=before;after.loaded[0]=1;--after.reserves[2];
    Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::coherent_unit_insertion,"max positive reserve lost");
    after.reserves[2]=0;
    auto delta=ClassifyCoJIndexedReloadDelta(before,after,0);
    Need(delta.kind==Kind::mismatch&&delta.reserves[2]==-std::int64_t(max),"reserve subtraction narrowed or overflowed");
    delta=ClassifyCoJIndexedReloadDelta(after,before,0);
    Need(delta.kind==Kind::mismatch&&delta.reserves[2]==std::int64_t(max),"reverse reserve subtraction overflowed");
}
void ForeignAndUnavailable(){
    const auto baseline=Sample();
    for(int mode=0;mode<17;++mode){
        auto before=baseline,after=before;
        switch(mode){
        case 0:after.valid=false;break;
        case 1:++after.player_id;break;
        case 2:++after.weapon_id;break;
        case 3:after.armed_hand=1;after.weapon_present={};after.weapon_present[1]={true,true,true};break;
        case 4:after.weapon_class=Class::saw_off;break;
        case 5:++after.context.player_generation;break;
        case 6:++after.context.native_context;break;
        case 7:++after.context.object_epoch;break;
        case 8:++after.context.synchronous_scope;break;
        case 9:before.context.synchronous_scope=after.context.synchronous_scope=0;break;
        case 10:after.reserve_kind={Pool::pistol,Pool::pistol};break;
        case 11:after.reserves[0]=1;break; // An unobserved pool cannot carry data.
        case 12:after.weapon_present[0][2]=false;break;
        case 13:before.context.object_epoch=after.context.object_epoch=0;break;
        case 14:before.context.native_context=after.context.native_context=0;break;
        case 15:before.context.player_generation=after.context.player_generation=0;break;
        case 16:before.weapon_class=after.weapon_class=Class::unknown;break;
        }
        Need(ClassifyCoJIndexedReloadDelta(before,after,0).kind==Kind::unavailable,"foreign/incomplete pair classified");
    }
    for(int index:{-1,2,std::numeric_limits<int>::max()})
        Need(ClassifyCoJIndexedReloadDelta(baseline,baseline,index).kind==Kind::unavailable,"invalid selected index accepted");
    for(auto id:{0u,0xffffffffu})for(int which=0;which<2;++which){
        auto v=baseline;if(which)v.player_id=id;else v.weapon_id=id;
        Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"invalid opaque identity accepted");
    }
    for(auto value:{-1,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),2}){
        for(int i:{0,1}){auto v=baseline;v.loaded[i]=value;
            Need(ClassifyCoJIndexedReloadDelta(v,v,i).kind==Kind::unavailable,"invalid class count accepted");}
    }
    for(auto value:{-1,std::numeric_limits<std::int32_t>::min()}){
        auto v=baseline;v.reserves[2]=value;
        Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"negative reserve accepted");
    }
    auto v=baseline;v.weapon_present[1]={true,true,true};
    Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::no_change,"coherent same-object long-gun representation rejected");
    v.weapon_present[1][2]=false;
    Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"partial both-hand representation accepted");
    v=Sample(Class::derringer);v.weapon_present[1]={true,true,true};
    Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"pistol shared across hands accepted");
    v=baseline;v.armed_hand=-1;
    Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"invalid hand accepted");
    v=baseline;v.weapon_present={};
    Need(ClassifyCoJIndexedReloadDelta(v,v,0).kind==Kind::unavailable,"empty owner admitted");
}
}
int main(){
    SharedPools();MixedPools();Mutations();ForeignAndUnavailable();
    std::cout<<"Indexed reload vector classification passed\n";
}
