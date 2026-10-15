#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <span>
#include "runtime/vr_types.hpp"
#include "runtime/hud_text.hpp"
#include "games/call_of_juarez/body_adapter.hpp"
#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include "games/call_of_juarez/focus_zoom.hpp"
#include "games/call_of_juarez/motion_reload_owner.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
#include <cstring>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
namespace {
void* env_table[176]{};void** env_holder=env_table;
void* vm_table[8]{};void** vm_holder=vm_table;
int player,cls,ex,selected_id,owner_id,punch_id,constructor_id,xid,yid,zid,vector_class,row_class;
int native_hand_id,actual_id,identity_id,weapon,entry_field,hit_field;
int contact_id,contact_result=0;bool contact_allow=true;
int overlay_alias,arms_object,position_method,left_method,up_method,hidden_method;
int model_method,hide_method,unhide_method,model_reads=0,model_lookups=0;
bool has_ray=true,has_billy=false;
std::string model_name;
bool geometry_mode=false,same_owner=true,hidden_arms=true,frame_drift=false,geometry_generation_change=false;
bool same_exception=false;
int same_calls=0,frame_reads=0;
CoJHandFrames live_frames{};
int holding=0,identity_calls=0;
bool counters_missing=false,replace_owner=false,replace_generation=false,foreign_weapon=false;
JavaPlayerBridge* observing_bridge=nullptr;
struct Vector{float x=0,y=0,z=0;} vectors[128];
struct Array{int length=0;void* items[64]{};} arrays[64];
int array_count=0;bool curved=false;
int refs=0,step=0,fail=-1,created=0,calls=0,failures=0;
bool pending=false,null_failure=false,selected=true,native_result=true;
bool Fail(){if(step++==fail){pending=!null_failure;return true;}return false;}
void Check(bool ok,const char* msg){if(!ok){std::cerr<<msg<<'\n';++failures;}}
int __stdcall GetEnv(void*,void** out,int){*out=&env_holder;return 0;}
void* __stdcall Exception(void*){if(!pending)return nullptr;++refs;return &ex;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void*){--refs;}
void* __stdcall Class(void*,void* object){if(Fail())return nullptr;++refs;return object==&player?static_cast<void*>(&cls):static_cast<void*>(&row_class);}
void* __stdcall Method(void*,void*,const char* name,const char* desc){
    if(Fail())return nullptr;
    if(geometry_mode){
        ++model_lookups;
        if(!std::strcmp(name,"GetElementID")&&!std::strcmp(desc,"(Ljava/lang/String;)I"))return &model_method;
        if(!std::strcmp(name,"HideElement")&&!std::strcmp(desc,"(I)Z"))return &hide_method;
        if(!std::strcmp(name,"UnhideElement")&&!std::strcmp(desc,"(I)Z"))return &unhide_method;
        if(!std::strcmp(name,"IsElementHidden")&&!std::strcmp(desc,"(I)Z"))return &hidden_method;
        Check(false,"unexpected geometry JNI lookup dependency");return nullptr;
    }
    if(!std::strcmp(name,"IsBoxing")&&!std::strcmp(desc,"()Z"))return &selected_id;
    if(!std::strcmp(name,"cojvrBoxingOwner")&&!std::strcmp(desc,"()I"))return &owner_id;
    if(!std::strcmp(name,"cojvrPhysicalPunch")&&!std::strcmp(desc,"(II[[LVector;)Z"))return &punch_id;
    if(!std::strcmp(name,"cojvrPhysicalPunchContact")&&!std::strcmp(desc,"(IIZ[[LVector;)I"))return &contact_id;
    if(!std::strcmp(name,"cojvrPhysicalPunchNativeHand")&&!std::strcmp(desc,"(I)I"))return &native_hand_id;
    if(!std::strcmp(name,"GetActualWeapon")&&!std::strcmp(desc,"(I)LWeapon;"))return &actual_id;
    if(!std::strcmp(name,"GetThisID")&&!std::strcmp(desc,"()I"))return &identity_id;
    Check(false,"unexpected exact JNI method");return nullptr;
}
unsigned char __stdcall Boolean(void*,void*,void* method,const JValue* args){
    if(Fail())return 0;
    if(geometry_mode){
        if(method==&hidden_method)return hidden_arms?1:0;
        Check(method==&position_method||method==&left_method||method==&up_method,"geometry uses read-only frame getters");
        const int id=args[0].i;Check(id>=0&&id<20,"current frame element bounds");
        const auto& f=live_frames[static_cast<std::size_t>(id)].frame;
        auto* v=static_cast<Vector*>(args[1].l);
        if(method==&position_method){v->x=f.position.x+(frame_drift?1.F:0.F);v->y=f.position.y;v->z=f.position.z;}
        else if(method==&left_method){v->x=1;v->y=0;v->z=0;}
        else {v->x=0;v->y=1;v->z=0;}
        ++frame_reads;
        if(geometry_generation_change&&frame_reads==1)++observing_bridge->being_generation_;
        return 1;
    }
    if(method==&selected_id)return selected?1:0;
    Check(method==&punch_id,"punch signature");++calls;
    Check(args[0].i==1&&args[1].i==17,"physical hand and expected weapon owner");
    auto* root=static_cast<Array*>(args[2].l);
    Check(root->length==(curved?2:1),"all physical segments admitted in one native attempt");
    auto* first=static_cast<Array*>(root->items[0]);auto* last=static_cast<Array*>(root->items[root->length-1]);
    Check(first->length==2&&last->length==2,"native rows contain exact endpoint pair");
    auto* a=static_cast<Vector*>(first->items[0]);auto* b=static_cast<Vector*>(last->items[1]);
    Check(a->x==10&&a->y==20&&a->z==30&&b->z==18,"native cm endpoints match physical sweep");
    if(curved){auto* middle=static_cast<Vector*>(first->items[1]);auto* next=static_cast<Vector*>(last->items[0]);
        Check(middle->x==15&&middle->z==24&&next->x==15&&next->z==24,"curve corner survives both adjacent segments");}
    return native_result?1:0;
}
int __stdcall Integer(void*,void*,void* method,const JValue* args){
    if(Fail()){pending=true;return 0;}
    if(geometry_mode){
        Check(method==&model_method&&args[0].l==&model_name,"model detection uses real GetElementID string route");
        ++model_reads;
        if(model_name=="RayBody")return has_ray?25:-1;
        if(model_name=="BillyBody")return has_billy?26:-1;
        Check(false,"unexpected model identity name");return -1;
    }
    if(method==&contact_id){
        ++calls;Check(args[0].i==1&&args[1].i==17&&(args[2].z!=0)==contact_allow,"exact contact status JNI arguments");
        auto* root=static_cast<Array*>(args[3].l);
        Check(root->length==2,"independent contact rows, no connecting chord");
        auto* a=static_cast<Array*>(root->items[0]);auto* b=static_cast<Array*>(root->items[1]);
        Check(a->length==2&&b->length==2,"contact rows have endpoint pairs");
        Check(static_cast<Vector*>(a->items[0])->x==1&&static_cast<Vector*>(a->items[1])->x==2&&
            static_cast<Vector*>(b->items[0])->x==100&&static_cast<Vector*>(b->items[1])->x==100,"short or static independent segments preserved");
        if(replace_generation)++observing_bridge->being_generation_;
        return contact_result;
    }
    if(method==&native_hand_id){
        Check(args&&args[0].i==17,"native-hand query binds expected owner");
        ++identity_calls;
        if(identity_calls>1&&replace_generation)++observing_bridge->being_generation_;
        return (foreign_weapon||(identity_calls>1&&replace_owner))?-1:holding;
    }
    Check(method==&owner_id||method==&identity_id,"exact owner identity method");return 17;
}
void* __stdcall Object(void*,void* target,void* method,const JValue* args){
    if(Fail())return nullptr;
    Check(target==&player&&method==&actual_id&&args[0].i==holding,"read actual native slot, not physical fist");
    ++refs;return &weapon;
}
void* __stdcall Field(void*,void*,const char* name,const char* desc){
    if(Fail())return nullptr;
    Check(!std::strcmp(desc,"I"),"diagnostic counters are exact integers");
    if(counters_missing){pending=true;return nullptr;}
    if(!std::strcmp(name,"cojvrPhysicalMeleeEntries"))return &entry_field;
    if(!std::strcmp(name,"cojvrPhysicalNativeHitCalls"))return &hit_field;
    Check(false,"punch observer must not look up firearm diagnostic fields");return nullptr;
}
int __stdcall IntField(void*,void* target,void* field){
    if(Fail()){pending=true;return 0;}
    Check(target==&weapon&&(field==&entry_field||field==&hit_field),"read owned melee counters only");
    return field==&entry_field?7:3;
}
unsigned char __stdcall Same(void*,void* alias,void* object){
    ++same_calls;Check(alias==&overlay_alias&&object==&player,"geometry must use JVM identity for global/local aliases");
    if(same_exception){pending=true;return 0;}
    return same_owner?1:0;
}
void* __stdcall String(void*,const char* text){
    if(Fail())return nullptr;model_name=text;++refs;return &model_name;
}
float __stdcall ReadFloat(void*,void* object,void* field){
    if(Fail()){pending=true;return 0;}
    const auto* v=static_cast<Vector*>(object);
    return field==&xid?v->x:field==&yid?v->y:v->z;
}
bool FrameRead(void*,int element,ElementWorldBasisTarget& frame)noexcept{
    if(element<0||element>=20)return false;frame=live_frames[static_cast<std::size_t>(element)].frame;return true;
}
bool FrameWrite(void*,int element,const ElementWorldBasisTarget& frame)noexcept{
    if(element<0||element>=20)return false;live_frames[static_cast<std::size_t>(element)].frame=frame;return true;
}
void* __stdcall New(void*,void*,void*,const JValue*){if(Fail())return nullptr;++refs;return &vectors[created++];}
void __stdcall Float(void*,void* obj,void* field,float value){if(Fail()){pending=true;return;}auto* v=static_cast<Vector*>(obj);
    if(field==&xid)v->x=value;else if(field==&yid)v->y=value;else v->z=value;}
void* __stdcall Find(void*,const char*){Check(false,"physical payload must use the actual Vector array class, without loader lookup");pending=true;return nullptr;}
void* __stdcall NewArray(void*,int length,void* component,void*){if(Fail())return nullptr;
    Check(component==&row_class||component==&vector_class,"native array component type");
    auto* a=&arrays[array_count++];a->length=length;++refs;return a;}
void __stdcall SetElement(void*,void* obj,int index,void* value){if(Fail()){pending=true;return;}
    auto* a=static_cast<Array*>(obj);Check(index>=0&&index<a->length,"native array bounds");a->items[index]=value;}
void Reset(){refs=step=created=calls=array_count=0;fail=-1;pending=null_failure=curved=false;selected=native_result=true;
    identity_calls=holding=0;counters_missing=replace_owner=replace_generation=foreign_weapon=false;
    contact_result=0;contact_allow=true;
    geometry_mode=false;same_owner=hidden_arms=true;frame_drift=geometry_generation_change=false;same_calls=frame_reads=0;
    same_exception=false;
    model_reads=model_lookups=0;has_ray=true;has_billy=false;model_name.clear();
    for(auto& v:vectors)v={};for(auto& a:arrays)a={};}
}
int main(){
    env_table[15]=reinterpret_cast<void*>(Exception);env_table[17]=reinterpret_cast<void*>(Clear);
    env_table[23]=reinterpret_cast<void*>(Delete);env_table[31]=reinterpret_cast<void*>(Class);
    env_table[33]=reinterpret_cast<void*>(Method);env_table[39]=reinterpret_cast<void*>(Boolean);
    env_table[51]=reinterpret_cast<void*>(Integer);env_table[30]=reinterpret_cast<void*>(New);
    env_table[36]=reinterpret_cast<void*>(Object);env_table[94]=reinterpret_cast<void*>(Field);
    env_table[100]=reinterpret_cast<void*>(IntField);
    env_table[24]=reinterpret_cast<void*>(Same);env_table[102]=reinterpret_cast<void*>(ReadFloat);
    env_table[167]=reinterpret_cast<void*>(String);
    env_table[111]=reinterpret_cast<void*>(Float);env_table[6]=reinterpret_cast<void*>(Find);
    env_table[172]=reinterpret_cast<void*>(NewArray);env_table[174]=reinterpret_cast<void*>(SetElement);
    vm_table[6]=reinterpret_cast<void*>(GetEnv);
    JavaPlayerBridge b;b.vm_=&vm_holder;b.being_=&player;
    observing_bridge=&b;
    b.vector_class_=&vector_class;b.vector_constructor_=&constructor_id;
    b.vector_x_field_=&xid;b.vector_y_field_=&yid;b.vector_z_field_=&zid;
    bool observed=false;std::uint32_t owner=0;Reset();
    Check(b.TryObservePhysicalBoxing(observed,owner)&&observed&&owner==17&&refs==0,"native boxing observation");
    Reset();Check(b.TryPhysicalPunch(1,17,{10,20,30},{10,20,18})&&calls==1&&refs==0,"synchronous physical attempt");
    const int steps=step;
    for(bool null: {false,true})for(int f=0;f<steps;++f){Reset();fail=f;null_failure=null;
        Check(!b.TryPhysicalPunch(1,17,{10,20,30},{10,20,18}),"failed JNI step cannot deliver success");
        Check(refs==0&&!pending,"punch local references and exception cleaned");}
    Reset();native_result=false;Check(!b.TryPhysicalPunch(1,17,{10,20,30},{10,20,18})&&calls==1,"native rejection not replayed");
    const std::array<JavaPlayerPosition,3> path{{{10,20,30},{15,20,24},{10,20,18}}};
    Reset();curved=true;Check(b.TryPhysicalPunchPath(1,17,path)&&calls==1&&refs==0,"curved attempt keeps ordered native rows");
    const int curve_steps=step;
    for(bool null:{false,true})for(int f=0;f<curve_steps;++f){Reset();curved=true;fail=f;null_failure=null;
        Check(!b.TryPhysicalPunchPath(1,17,path),"curve allocation/publication fault rejects native delivery");
        Check(refs==0&&!pending,"curve JNI references and exception cleaned at every step");}
    Reset();Check(!b.TryPhysicalPunch(2,17,{10,20,30},{10,20,18})&&step==0,"invalid hand rejects before JNI");
    Reset();Check(!b.TryPhysicalPunch(1,17,{10,20,30},{10,20,-40})&&step==0,"oversized sweep rejects before JNI");
    for(int h:{0,1}){
        Reset();holding=h;counters_missing=true;int resolved=-1;
        Check(b.TryResolvePhysicalPunchNativeHand(17,resolved)&&resolved==h&&refs==0,"identity does not depend on counter fields");
        Reset();holding=h;b.being_generation_=41;CoJPhysicalPunchDiagnostics d{};
        Check(b.TryObservePhysicalPunchDiagnostics(d)&&d.valid&&d.owner_id==17&&d.native_hand==h&&
            d.melee_entries==7&&d.hit_calls==3&&d.being_generation==41&&refs==0,"fresh value-only native melee diagnostics");
    }
    Reset();CoJPhysicalPunchDiagnostics diagnostics{};b.being_generation_=41;
    Check(b.TryObservePhysicalPunchDiagnostics(diagnostics),"diagnostic baseline");const int diagnostic_steps=step;
    for(bool null:{false,true})for(int f=0;f<diagnostic_steps;++f){
        Reset();fail=f;null_failure=null;diagnostics.valid=true;
        Check(!b.TryObservePhysicalPunchDiagnostics(diagnostics)&&!diagnostics.valid,"every failed diagnostic step rejects counter success");
        Check(diagnostics.melee_entries==0&&diagnostics.hit_calls==0,"partial counters never escape as evidence");
        Check(refs==0&&!pending,"diagnostic faults clean all JNI references and pending exceptions");
    }
    Reset();counters_missing=true;
    Check(!b.TryObservePhysicalPunchDiagnostics(diagnostics)&&!diagnostics.valid&&diagnostics.native_hand==0&&
        diagnostics.owner_id==17&&diagnostics.being_generation==41&&refs==0&&!pending,"missing counters retain independently resolved identity");
    for(bool generation:{false,true}){
        Reset();replace_generation=generation;replace_owner=!generation;b.being_generation_=41;
        Check(!b.TryObservePhysicalPunchDiagnostics(diagnostics)&&!diagnostics.valid&&diagnostics.native_hand==-1&&
            diagnostics.owner_id==0,"identity or generation replacement invalidates complete snapshot");
        Check(refs==0&&!pending,"replacement leaves no retained diagnostic references");
    }
    Reset();foreign_weapon=true;int resolved=0;
    Check(!b.TryResolvePhysicalPunchNativeHand(17,resolved)&&resolved==-1,"foreign class rejected by exact helper");
    Reset();resolved=0;Check(!b.TryResolvePhysicalPunchNativeHand(0,resolved)&&resolved==-1&&step==0,"invalid owner fails before JNI");
    const std::array<std::array<JavaPlayerPosition,2>,2> segments{{{{{1,2,3},{2,2,3}}},{{{100,2,3},{100,2,3}}}}};
    for(bool allow:{false,true})for(int result:{-1,0,1,2}){
        Reset();contact_allow=allow;contact_result=result;
        Check(static_cast<int>(b.TryPhysicalPunchContact(1,17,allow,segments))==result&&calls==1&&refs==0&&!pending,
            "contact bridge decodes every distinct status without conflating clear or denial");
    }
    Reset();contact_result=2;Check(b.TryPhysicalPunchContact(1,17,true,segments)==CoJPunchContactResult::attacked,"contact baseline");
    const int contact_steps=step;
    for(bool null:{false,true})for(int f=0;f<contact_steps;++f){
        Reset();fail=f;null_failure=null;
        Check(b.TryPhysicalPunchContact(1,17,true,segments)==CoJPunchContactResult::unavailable,"failed contact step cannot decode zero as clear");
        Check(refs==0&&!pending,"all contact JNI fault paths clean references and exceptions");
    }
    for(int result:{-2,3,100}){Reset();contact_result=result;
        Check(b.TryPhysicalPunchContact(1,17,true,segments)==CoJPunchContactResult::unavailable&&refs==0,"unknown native status fails closed");}
    Reset();replace_generation=true;contact_result=2;
    Check(b.TryPhysicalPunchContact(1,17,true,segments)==CoJPunchContactResult::unavailable&&refs==0,"contact generation replacement rejects stale result");
    Reset();auto invalid=segments;invalid[0][1].x=27;
    Check(b.TryPhysicalPunchContact(1,17,true,invalid)==CoJPunchContactResult::unavailable&&step==0,"oversized independent segment rejects before JNI");
    Reset();invalid=segments;invalid[0][0].x=std::numeric_limits<float>::quiet_NaN();
    Check(b.TryPhysicalPunchContact(1,17,true,invalid)==CoJPunchContactResult::unavailable&&step==0,"nonfinite contact rejects before JNI");
    // Exercise the real bridge/overlay verification with distinct JNI handles
    // for one Being. Model/skin arithmetic has a separate measured fixture.
    Reset();geometry_mode=true;b.being_generation_=71;b.independent_hand_generation_=71;
    b.fist_skin_generation_=71;b.fist_skin_model_=0;
    b.element_world_read_lookup_attempted_=true;
    b.get_element_position_method_=&position_method;b.get_element_left_method_=&left_method;b.get_element_up_method_=&up_method;
    b.is_element_hidden_method_=&hidden_method;b.independent_arms_object_=&arms_object;b.independent_arms_element_=21;
    const ElementWorldBasisTarget identity{{0,0,0},{0,1,0},{0,0,1},true};
    for(std::size_t i=0;i<live_frames.size();++i){live_frames[i]={static_cast<int>(i),identity};b.independent_hand_elements_[0][i]=static_cast<int>(i);}
    auto& hand_overlay=b.hand_overlays_[0];hand_overlay.object=&overlay_alias;
    Check(hand_overlay.frames.Apply(live_frames,identity,identity,{0,0,0},nullptr,FrameRead,FrameWrite),"real hand overlay fixture admission");
    CoJFistContactGeometry geometry{};
    Check(b.TryReadFistContactGeometry(0,geometry)&&geometry.valid&&same_calls==1&&frame_reads==120&&refs==0,
        "geometry accepts same Being through different global/local JNI handles and verifies plus reads20 current frames");
    const auto reset_geometry=[&](){Reset();geometry_mode=true;b.being_generation_=71;b.independent_hand_generation_=71;};
    reset_geometry();same_owner=false;geometry.valid=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&frame_reads==0,"foreign display owner rejects before frame reads");
    reset_geometry();same_exception=true;geometry.valid=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&frame_reads==0,
        "identity exception rejects geometry before frame reads");
    Check(!pending&&refs==0,"fist IsSameObject failure clears pending exception and local throwable reference");
    reset_geometry();b.independent_hand_generation_=70;geometry.valid=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&same_calls==0,"old hand generation rejects before JNI identity");
    reset_geometry();hidden_arms=false;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid,"unverified hidden-arm presentation rejects contact geometry");
    reset_geometry();frame_drift=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid,"live hand drift from display transaction rejects geometry");
    reset_geometry();geometry_generation_change=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&refs==0,"replacement during native frame read rejects final geometry");
    reset_geometry();Check(!b.TryReadFistContactGeometry(2,geometry)&&same_calls==0,"invalid geometry hand rejects before JNI");
    for(int model_case=0;model_case<4;++model_case){
        reset_geometry();b.fist_skin_generation_=71;b.fist_skin_model_=-1;
        b.element_visibility_lookup_attempted_=false;
        b.get_element_id_method_=b.hide_element_method_=b.unhide_element_method_=nullptr;
        has_ray=(model_case&1)!=0;has_billy=(model_case&2)!=0;
        const bool expected=model_case==1||model_case==2;
        Check(b.TryReadFistContactGeometry(0,geometry)==expected&&geometry.valid==expected,
            "actual bridge discovers Ray-only or Billy-only and rejects both/neither");
        Check(model_reads==2&&model_lookups==4&&refs==0&&!pending,
            "model discovery exercises visibility lookup and both native names without leaking local refs");
        Check(b.fist_skin_model_==(expected?(has_ray?0:1):-1),"qualified model cache matches unique native identity");
    }
    reset_geometry();b.fist_skin_generation_=71;b.fist_skin_model_=0;has_ray=false;has_billy=true;
    Check(b.TryReadFistContactGeometry(0,geometry)&&model_reads==0,"same generation reuses qualified model cache");
    b.being_generation_=72;b.independent_hand_generation_=72;
    Check(b.TryReadFistContactGeometry(0,geometry)&&b.fist_skin_generation_==72&&b.fist_skin_model_==1&&model_reads==2,
        "new player generation invalidates Ray cache and discovers Billy through native model reader");
    reset_geometry();b.independent_hand_faulted_=true;
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&same_calls==0,"faulted hand transaction rejects geometry before JNI");
    b.independent_hand_faulted_=false;
    reset_geometry();Check(hand_overlay.frames.Restore({0,0,0},nullptr,FrameRead,FrameWrite),"actual overlay restoration succeeds");
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid&&same_calls==0,"inactive restored overlay has no contact authority");
    Check(hand_overlay.frames.Apply(live_frames,identity,identity,{0,0,0},nullptr,FrameRead,FrameWrite),"overlay reacquires after completed restore");
    Check(!hand_overlay.frames.Restore({0,0,0},nullptr,FrameRead,nullptr)&&hand_overlay.frames.faulted(),"actual failed restoration retains faulted overlay");
    Check(!b.TryReadFistContactGeometry(0,geometry)&&!geometry.valid,"failed restoration cannot publish contact geometry");
    hand_overlay.object=nullptr;hand_overlay.frames={};b.independent_arms_object_=nullptr;
    JavaPlayerBridge reset_probe;reset_probe.fist_skin_generation_=71;reset_probe.fist_skin_model_=1;
    reset_probe.Reset();
    Check(reset_probe.fist_skin_generation_==0&&reset_probe.fist_skin_model_==-1,"actual bridge Reset clears cached model and generation");
    b.being_=nullptr;b.vm_=nullptr;return failures?1:0;
}
