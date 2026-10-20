// Exercise production JNI observation/forwarding, including every injected
// exception boundary. This runs in a host process, not the game VM.
#include <windows.h>
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
namespace {
using namespace cojvr::games::call_of_juarez;
void* P(int id){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));}
enum { player=1,module,duel,controller,action,foreign,exception=20,
    game_class=100,single_class,armed_class,duel_class,controller_class,action_class,settings_class,object_class,
    active_field=200,controller_field,module_field,duel_field,state_field,player_field,action_field,
    sensx_field,sensy_field,threshx_field,threshy_field,
    draw_field=220,
    id_method=300,time_method,can_method,mouse_method,hands_method,ratio_method,sensitivity_method,delta_method,realdelta_method };
void* functions[174]{};void** environment=functions;
void* vm_functions[8]{};void** vm=vm_functions;
bool pending=false,return_fault_ref=false,can_process=true,foreign_player=false,foreign_module=false;
bool foreign_player_module=false,foreign_type=false;
bool empty_hands=false;
int step=0,fail_step=-1,locals=0,native_state=-1;
float native_time=1;
std::vector<std::pair<int,float>> events;
bool Fault(){if(pending)std::abort();return ++step==fail_step?(pending=true):false;}
void* Ref(void* value,bool fault){if(fault&&!return_fault_ref)return nullptr;if(value)++locals;return value;}
std::int32_t __stdcall Env(void*,void** out,std::int32_t){*out=&environment;return 0;}
void* __stdcall Exception(void*){if(!pending)return nullptr;++locals;return P(exception);}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void*){--locals;}
void* __stdcall Find(void*,const char* name){
    const bool fault=Fault();int id=0;
    if(!std::strcmp(name,"LawmanGame"))id=game_class;
    if(!std::strcmp(name,"LawmanModuleSingle"))id=single_class;
    if(!std::strcmp(name,"ArmedPlayerBeing"))id=armed_class;
    if(!std::strcmp(name,"Duel"))id=duel_class;
    if(!std::strcmp(name,"GameInputController"))id=controller_class;
    if(!std::strcmp(name,"InputSettings"))id=settings_class;
    if(!std::strcmp(name,"GameObject"))id=object_class;
    return Ref(P(id),fault);
}
void* __stdcall Class(void*,void* owner){return Ref(owner==P(action)?P(action_class):P(armed_class),Fault());}
void* __stdcall Field(void*,void*,const char* name,const char*){
    if(Fault())return nullptr;
    const char* names[]{"sm_cActiveGameModule","sm_cInputController","m_Module","m_cDuel",
        "m_iState","m_cPlayer","m_cAction","fPadSensX","fPadSensY","fPadSpeedXTresh","fPadSpeedYTresh"};
    for(int i=0;i<11;++i)if(!std::strcmp(name,names[i]))return P(active_field+i);
    if(!std::strcmp(name,"m_cModule"))return P(module_field);
    if(!std::strcmp(name,"s_fDuelMouseMinVertVel"))return P(draw_field);
    return nullptr;
}
void* __stdcall Static(void*,void*,void* field){return Ref(field==P(active_field)?P(module):P(controller),Fault());}
void* __stdcall Object(void*,void* owner,void* field){
    const bool fault=Fault();void* value=nullptr;
    if(field==P(module_field))value=(owner==P(player)?foreign_player_module:foreign_module)?P(foreign):P(module);
    if(field==P(duel_field))value=P(duel);
    if(field==P(player_field))value=foreign_player?P(foreign):P(player);
    if(field==P(action_field))value=P(action);
    return Ref(value,fault);
}
std::int32_t __stdcall Integer(void*,void*,void*){if(Fault())return -100;return native_state;}
std::uint8_t __stdcall Same(void*,void* a,void* b){if(Fault())return 0;return a==b;}
std::uint8_t __stdcall Instance(void*,void*,void*){if(Fault())return 0;return !foreign_type;}
void* __stdcall Method(void*,void*,const char* name,const char*){
    if(Fault())return nullptr;
    if(!std::strcmp(name,"GetThisID"))return P(id_method);
    if(!std::strcmp(name,"GetTime"))return P(time_method);
    if(!std::strcmp(name,"CanProcessInputs"))return P(can_method);
    if(!std::strcmp(name,"OnInputMouseAxis"))return P(mouse_method);
    if(!std::strcmp(name,"HasSomethingInHands"))return P(hands_method);
    if(!std::strcmp(name,"GetMouseSensitivity"))return P(ratio_method);
    if(!std::strcmp(name,"GetMouseSensitivityY"))return P(sensitivity_method);
    if(!std::strcmp(name,"GetTimeDelta"))return P(delta_method);
    if(!std::strcmp(name,"GetRealDT"))return P(realdelta_method);
    return nullptr;
}
std::int32_t __stdcall CallInt(void*,void*,void*,const JValue*){if(Fault())return 0;return static_cast<std::int32_t>(0x80000042U);}
float __stdcall CallFloat(void*,void*,void* method,const JValue*){if(Fault())return 0;
    if(method==P(ratio_method))return .275F;if(method==P(sensitivity_method))return 1;
    if(method==P(delta_method)||method==P(realdelta_method))return 1.0F/144.0F;return native_time;}
std::uint8_t __stdcall CallBool(void*,void*,void* method,const JValue*){if(Fault())return 0;return method==P(hands_method)?!empty_hands:can_process;}
float __stdcall Real(void*,void*,void* field){if(Fault())return 0;if(field==P(draw_field))return .1F;
    return field==P(sensx_field)||field==P(sensy_field)?10.0F:.4F;}
void __stdcall CallVoid(void*,void* owner,void* method,const JValue* args){
    if(Fault())return;
    if(owner!=P(controller)||method!=P(mouse_method))std::abort();
    events.emplace_back(args[0].i,args[1].f);
}
void Bind(JavaPlayerBridge& bridge){bridge.vm_=&vm;bridge.being_=P(player);bridge.being_generation_=1;}
cojvr::runtime::Pose Aim(bool lowered){
    cojvr::runtime::Pose pose;pose.position_valid=pose.orientation_valid=true;
    if(lowered)pose.orientation={-.5F,0,0,.8660254F};return pose;
}
bool Apply(JavaPlayerBridge& bridge,const cojvr::runtime::GameplayInputState& input,
    std::uint64_t time,std::string* error=nullptr){
    return bridge.TryApplyDuelInput(input,time,Aim(empty_hands),time,error);
}
void Init(){
    functions[6]=reinterpret_cast<void*>(&Find);functions[15]=reinterpret_cast<void*>(&Exception);
    functions[17]=reinterpret_cast<void*>(&Clear);functions[23]=reinterpret_cast<void*>(&Delete);
    functions[24]=reinterpret_cast<void*>(&Same);functions[31]=reinterpret_cast<void*>(&Class);
    functions[32]=reinterpret_cast<void*>(&Instance);functions[33]=reinterpret_cast<void*>(&Method);
    functions[39]=reinterpret_cast<void*>(&CallBool);functions[51]=reinterpret_cast<void*>(&CallInt);
    functions[57]=reinterpret_cast<void*>(&CallFloat);functions[63]=reinterpret_cast<void*>(&CallVoid);
    functions[94]=reinterpret_cast<void*>(&Field);functions[95]=reinterpret_cast<void*>(&Object);
    functions[100]=reinterpret_cast<void*>(&Integer);functions[102]=reinterpret_cast<void*>(&Real);
    functions[113]=reinterpret_cast<void*>(&Method);functions[137]=reinterpret_cast<void*>(&CallFloat);
    functions[144]=reinterpret_cast<void*>(&Field);functions[145]=reinterpret_cast<void*>(&Static);
    functions[152]=reinterpret_cast<void*>(&Real);
    vm_functions[6]=reinterpret_cast<void*>(&Env);
}
}
int main(){
    Init();int failures=0;const auto check=[&](bool v,const char* why){if(!v){std::cerr<<why<<'\n';++failures;}};
    JavaPlayerBridge bridge;Bind(bridge);bool active=true;std::string error;
    check(bridge.TryObserveDuel(active,&error)&&!active&&locals==0,"ordinary inactive duel rejected or leaked references");
    for(int state:{0,1,2,3}){native_state=state;check(bridge.TryObserveDuel(active)&&active&&locals==0,"native duel phase not admitted");}
    native_state=4;check(!bridge.TryObserveDuel(active)&&!active&&locals==0,"unknown native state acquired input");native_state=2;
    for(bool* wrong:{&foreign_player,&foreign_module,&foreign_player_module,&foreign_type}){
        *wrong=true;check(!bridge.TryObserveDuel(active)&&!active&&locals==0,"foreign owner admitted or leaked");*wrong=false;
    }
    step=0;check(bridge.TryObserveDuel(active)&&active,"baseline exact owner failed");const int observation_steps=step;
    for(bool ref:{false,true})for(int fault=1;fault<=observation_steps;++fault){
        step=0;fail_step=fault;return_fault_ref=ref;
        check(!bridge.TryObserveDuel(active)&&!active&&!pending&&locals==0,"observation fault leaked references, exception or authority");
    }
    fail_step=-1;return_fault_ref=false;
    cojvr::runtime::GameplayInputState input;input.active=true;input.input_context_generation=7;
    native_time=1;check(Apply(bridge,input,10,&error)&&locals==0,"center failed");
    input.turn={1,-1};native_time=2;step=0;
    check(Apply(bridge,input,20,&error)&&events.size()==2&&events[0]==std::pair<int,float>{0,-10.0F}&&
        events[1]==std::pair<int,float>{1,-10.0F}&&locals==0,"signed axes did not reach original mouse consumer");
    check(Apply(bridge,input,21)&&events.size()==2,"same native tick replayed input");
    native_time=3;check(Apply(bridge,input,30)&&events.size()==4,"next native tick lost equal input");
    can_process=false;native_time=4;check(Apply(bridge,input,40)&&events.size()==4,"native controller denial was bypassed");
    can_process=true;native_time=5;check(Apply(bridge,input,50)&&events.size()==4,"denial recovery replayed held input");
    for(bool empty:{false,true}) {
      empty_hands=empty;
      JavaPlayerBridge baseline;Bind(baseline);input.turn={};native_time=98;
      check(Apply(baseline,input,980),"draw conversion baseline center failed");
      input.turn={1,-1};native_time=99;step=0;events.clear();
      check(Apply(baseline,input,990)&&events.size()==2&&locals==0,"draw conversion baseline delivery failed");
      if(empty)check((events[1].second*.275F)*(1.0F/144.0F)<-.1F,
          "actual JNI empty-hand mouse output did not qualify as native draw");
      else check(events[1].second==-10.0F,"drawn weapon aiming gain changed");
      const int apply_steps=step;
      for(bool ref:{false,true})for(int fault=1;fault<=apply_steps;++fault){
        JavaPlayerBridge test;Bind(test);input.turn={};native_time=100;
        fail_step=-1;check(Apply(test,input,1000),"fault precondition center failed");
        events.clear();input.turn={1,-1};native_time=101;step=0;fail_step=fault;return_fault_ref=ref;
        check(!Apply(test,input,1010)&&!pending&&locals==0,"apply fault leaked a JNI exception/reference or reported success");
        const auto before=events.size();fail_step=-1;
        check(Apply(test,input,1011)&&events.size()==before,"failed/partial sample replayed without a fresh center");
      }
    }
    check(locals==0&&!pending,"final JNI ownership was not clean");
    JavaPlayerBridge physical;Bind(physical);empty_hands=true;events.clear();input.turn={};
    native_time=200;
    check(physical.TryApplyDuelInput(input,2000,Aim(false),2000),"physical guard baseline failed");
    input.turn.y=-1;native_time=201;
    check(physical.TryApplyDuelInput(input,2010,Aim(false),2010)&&events.empty(),
        "production JNI pre-aimed hand still drew using only the stick");
    native_time=202;
    check(physical.TryApplyDuelInput(input,2020,Aim(true),2020)&&events.empty(),
        "production JNI lowering replayed a held stick");
    input.turn={};native_time=203;
    check(physical.TryApplyDuelInput(input,2030,Aim(true),2030),"physical center failed");
    input.turn.y=-1;native_time=204;
    check(physical.TryApplyDuelInput(input,2040,Aim(true),2040)&&events.size()==1&&events[0].first==1&&
        (events[0].second*.275F)*(1.0F/144.0F)<-.1F,"physical draw lost native conversion");
    empty_hands=false;input.turn.y=1;native_time=205;
    check(physical.TryApplyDuelInput(input,2050,Aim(true),2050)&&events.size()==1,
        "production JNI stick raised a physically lowered gun");
    native_time=206;
    check(physical.TryApplyDuelInput(input,2060,Aim(false),2060)&&events.size()==2&&events[1].second>0,
        "production JNI physical raise lost native upward input");
    check(locals==0&&!pending,"physical guard leaked JNI ownership");
    // Suppression is not a physical stick release. An in-flight native draw
    // becoming occupied cannot turn blocked held UP into an admitted new edge.
    for(int boundary=0;boundary<8;++boundary){
        JavaPlayerBridge recovering;Bind(recovering);events.clear();
        input={};input.active=true;input.input_context_generation=7;empty_hands=true;
        const auto poll=[&](int tick,bool empty,float y,bool tracked=true){
            native_time=float(tick);empty_hands=empty;input.turn={0,y};auto pose=Aim(true);
            pose.orientation_valid=tracked;
            return recovering.TryApplyDuelInput(input,std::uint64_t(tick)*1000,pose,tick);
        };
        check(poll(300,true,0)&&poll(301,true,-1),"recovery draw baseline failed");
        const auto before=events.size();
        if(boundary==0)check(poll(302,true,1,false),"tracking loss failed");
        if(boundary==1){can_process=false;check(poll(302,true,1),"denial failed");can_process=true;}
        if(boundary==2){input.turn_available=false;check(poll(302,true,1),"axis loss failed");input.turn_available=true;}
        if(boundary==3){++input.input_context_generation;check(poll(302,true,1),"context replacement failed");}
        if(boundary==4){++recovering.being_generation_;check(poll(302,true,1),"player replacement failed");}
        if(boundary==5){step=0;fail_step=1;check(!poll(302,true,1),"JNI fault did not fail");fail_step=-1;}
        if(boundary==7)recovering.InvalidateDuelInput(); // Includes recenter admission.
        const int next=boundary==6?403:303;
        check(poll(next,true,1)&&poll(next+1,false,1)&&events.size()==before,
            "filtered neutral rearmed held UP across an ownership/input boundary");
        check(poll(next+2,false,0)&&poll(next+3,false,1)&&events.size()==before,
            "real neutral after recovery bypassed physical raise pitch");
        native_time=float(next+4);input.turn.y=1;
        check(recovering.TryApplyDuelInput(input,std::uint64_t(next+4)*1000,Aim(false),next+4)&&
            events.size()==before+1&&events.back().second>0,"physical raise after real neutral did not recover");
        check(locals==0&&!pending,"recovery guard leaked JNI state");
    }
    return failures?1:0;
}
