#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include "games/call_of_juarez/mounted_motion_observer.hpp"
using namespace cojvr::games::call_of_juarez;
namespace {
void Require(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void* P(int i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(i));}
void* table[176]{};void** env=table;std::map<void*,int> refs;
bool pending=false,mounted=true,reciprocal=true,aliases=false,null_vector=false,failed_ref=false;
int nonfinite_field=0;
int steps=0,fail=-1,change_at=-1,drift_at=-1,horse_reads=0,first_velocity_step=0;
std::uint32_t player_id=0xf1234001U,horse_id=0xe1234002U;
float velocity=800.F;
using Value=long_gun_observer_detail::Value;
constexpr const char* names[]{"m_cHorse","m_cHorseman","m_fFwdLocalSpeed","m_fSideLocalSpeed",
    "m_fCurrentTurnSpeed","m_iSpeedKeyState","m_iSpeedState","m_bRun","m_bCanMove",
    "bTied","m_bNewPlayerControl","m_vGlobalSpeed","fX","fY","fZ"};
constexpr const char* descriptors[]{"LHorse;","LHumanBeing;","F","F","F","I","I","Z","Z","Z","Z","LVector;","F","F","F"};
bool Step(){Require(!pending,"JNI call issued with pending exception");++steps;
    if(steps==change_at)mounted=false;
    if(steps==drift_at)velocity=801.F;
    if(steps==fail){pending=true;return false;}return true;}
void* Local(void* p){if(p)++refs[p];return p;}
void __stdcall Delete(void*,void* p){Require(refs[p]>0,"local reference underflow");if(!--refs[p])refs.erase(p);}
void* __stdcall Exception(void*){return pending?Local(P(999)):nullptr;}
void __stdcall Clear(void*){pending=false;}
void* __stdcall Class(void*,void*){const bool ok=Step();return ok||failed_ref?Local(P(200)):nullptr;}
void* __stdcall Field(void*,void*,const char* name,const char* descriptor){
    if(!Step())return nullptr;
    for(int i=0;i<15;++i)if(!std::strcmp(name,names[i])){
        Require(!std::strcmp(descriptor,descriptors[i]),"wrong shipped field descriptor");return P(i+1);}
    Require(false,"observer queried a field outside the native contract");return nullptr;
}
void* __stdcall Method(void*,void*,const char* name,const char* descriptor){
    if(!Step())return nullptr;
    Require(!std::strcmp(name,"GetThisID")&&!std::strcmp(descriptor,"()I"),"observer invoked a gameplay method");return P(90);
}
void* __stdcall Object(void*,void* object,void* field){
    if(!Step())return failed_ref?Local(P(400)):nullptr;
    const auto f=reinterpret_cast<std::uintptr_t>(field);
    if(f==1){Require(object==P(100),"mount read on foreign rider");++horse_reads;return Local(mounted?(aliases&&horse_reads>1?P(102):P(101)):nullptr);}
    if(f==2)return Local(reciprocal?P(100):P(103));
    if(f==12)return Local(null_vector?nullptr:object==P(100)?P(301):P(300));
    Require(false,"unexpected object field");return nullptr;
}
unsigned char __stdcall Same(void*,void* a,void* b){
    if(!Step())return 0;if(a==P(102))a=P(101);if(b==P(102))b=P(101);return a==b;
}
int __stdcall Integer(void*,void* object,void* method,const Value* args){
    if(!Step())return -1;Require(method==P(90)&&!args,"bad identity getter");
    return static_cast<std::int32_t>(object==P(100)?player_id:horse_id);
}
int __stdcall IntField(void*,void*,void* field){
    if(!Step())return -1;Require(field==P(6)||field==P(7),"unexpected integer field");return field==P(6)?4:0x82;
}
unsigned char __stdcall BoolField(void*,void*,void* field){
    if(!Step())return 0;Require(field==P(8)||field==P(9)||field==P(10)||field==P(11),"unexpected boolean field");return field!=P(10);
}
float __stdcall FloatField(void*,void* object,void* field){
    if(!Step())return 0;
    if(field==P(nonfinite_field))return std::numeric_limits<float>::quiet_NaN();
    if(field==P(3))return 1.F;if(field==P(4))return -.25F;if(field==P(5))return -35.F;
    Require(field==P(13)||field==P(14)||field==P(15),"unexpected scalar field");
    if(object==P(300)&&field==P(13)&&!first_velocity_step)first_velocity_step=steps;
    return field==P(13)?(object==P(300)?velocity:10.F):field==P(14)?0.F:20.F;
}
void Reset(){Require(refs.empty(),"previous observation leaked references");pending=false;mounted=reciprocal=true;
    aliases=null_vector=failed_ref=false;nonfinite_field=0;steps=horse_reads=first_velocity_step=0;fail=change_at=drift_at=-1;velocity=800.F;player_id=0xf1234001U;horse_id=0xe1234002U;}
void Empty(const CoJMountedMotionSnapshot& s){Require(!s.valid&&!s.player_id&&!s.horse_id&&s.velocity_cm_s.x==0,"failure published partial native state");}
}
int main(){
    table[15]=reinterpret_cast<void*>(&Exception);table[17]=reinterpret_cast<void*>(&Clear);
    table[23]=reinterpret_cast<void*>(&Delete);table[24]=reinterpret_cast<void*>(&Same);
    table[31]=reinterpret_cast<void*>(&Class);table[33]=reinterpret_cast<void*>(&Method);
    table[51]=reinterpret_cast<void*>(&Integer);table[94]=reinterpret_cast<void*>(&Field);
    table[95]=reinterpret_cast<void*>(&Object);table[96]=reinterpret_cast<void*>(&BoolField);
    table[100]=reinterpret_cast<void*>(&IntField);table[102]=reinterpret_cast<void*>(&FloatField);
    CoJMountedMotionSnapshot s{};
    Reset();Require(ReadCoJMountedMotion(&env,P(100),s)&&s.valid&&s.player_id==player_id&&s.horse_id==horse_id,
        "mounted native state unavailable or LAA identities lost");
    Require(s.forward_request==1&&s.side_request==-.25F&&s.turn_rate_degrees_s==-35&&
        s.speed_key_state==4&&s.speed_state==0x82&&s.run&&s.rider_run&&s.can_move&&!s.tied&&s.new_player_control&&
        s.velocity_cm_s.x==800&&s.velocity_cm_s.z==20&&s.rider_velocity_cm_s.x==10,
        "native gait/input/cached velocity changed meaning");
    const int success_steps=steps,drift_step=first_velocity_step+1;
    Require(refs.empty()&&!pending,"successful observation leaked refs/exception");
    for(bool ref_on_error:{false,true})for(int i=1;i<=success_steps;++i){Reset();fail=i;failed_ref=ref_on_error;
        Require(!ReadCoJMountedMotion(&env,P(100),s)&&refs.empty()&&!pending,"JNI failure admitted or leaked");Empty(s);}
    Reset();aliases=true;Require(ReadCoJMountedMotion(&env,P(100),s),"JNI aliases compared as pointers");
    Reset();mounted=false;Require(!ReadCoJMountedMotion(&env,P(100),s),"absent horse admitted");Empty(s);
    Reset();reciprocal=false;Require(!ReadCoJMountedMotion(&env,P(100),s),"foreign rider admitted");Empty(s);
    Reset();null_vector=true;Require(!ReadCoJMountedMotion(&env,P(100),s),"missing cached velocity admitted");Empty(s);
    for(auto id:{0U,0xffffffffU}){Reset();horse_id=id;Require(!ReadCoJMountedMotion(&env,P(100),s),"invalid horse identity admitted");Empty(s);
        Reset();player_id=id;Require(!ReadCoJMountedMotion(&env,P(100),s),"invalid rider identity admitted");Empty(s);}
    for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        Reset();velocity=bad;Require(!ReadCoJMountedMotion(&env,P(100),s),"nonfinite native velocity admitted");Empty(s);}
    for(int field:{3,4,5,13,14,15}){Reset();nonfinite_field=field;
        Require(!ReadCoJMountedMotion(&env,P(100),s),"nonfinite native input/rate/vector axis admitted");Empty(s);}
    // Change ownership during the read, up to the final mount-field access.
    // IsSameObject itself cannot schedule a native mount/dismount callback.
    for(int i=1;i<=success_steps-3;++i){Reset();change_at=i;
        Require(!ReadCoJMountedMotion(&env,P(100),s),"dismount during native observation admitted");Empty(s);}
    Reset();drift_at=drift_step;
    Require(!ReadCoJMountedMotion(&env,P(100),s),"drifting cached state admitted");Empty(s);
    for(int slot:{15,17,23,24,31,33,51,94,95,96,100,102}){Reset();auto old=table[slot];table[slot]=nullptr;
        Require(!ReadCoJMountedMotion(&env,P(100),s)&&steps==0,"missing JNI ABI partially executed");Empty(s);table[slot]=old;}
    Reset();pending=true;Require(!ReadCoJMountedMotion(&env,P(100),s)&&!pending&&refs.empty(),"entry exception survived observer");Empty(s);
    Reset();Require(!ReadCoJMountedMotion(nullptr,P(100),s)&&!ReadCoJMountedMotion(&env,nullptr,s),"null owner/env admitted");
    CoJMountedTraceBudget budget;
    Require(!budget.Admit(false,true,1,1000000)&&!budget.Admit(true,false,1,1000000),"disabled/ineligible trace admitted");
    Require(budget.Admit(true,true,1,1000000)&&!budget.Admit(true,true,1,1100000)&&
        !budget.Admit(true,true,2,1099999)&&budget.Admit(true,true,2,1100000),"trace duplicate/cadence contract lost");
    Require(!budget.Admit(true,true,3,500000)&&!budget.Admit(true,true,4,599999)&&budget.Admit(true,true,5,600000),"clock regression erased cadence");
    for(std::uint64_t i=3;i<2048;++i)Require(budget.Admit(true,true,i+3,600000+(i-2)*100000),"trace budget ended early");
    Require(budget.exhausted()&&!budget.Admit(true,true,9000,900000000),"trace exceeded attempt cap");
    Require(!budget.Admit(true,false,9001,900100000)&&budget.exhausted(),"owner loss refilled diagnostic budget");
    budget.ObserveEnabled(false); // Command processed, then tracking/menu early return; no Admit call.
    budget.ObserveEnabled(true);
    Require(budget.Admit(true,true,9002,900200000)&&budget.attempts()==1,
        "disable/re-enable outside eligible gameplay retained an exhausted budget");
    Require(!budget.Admit(false,false,0,0)&&!budget.exhausted()&&budget.Admit(true,true,1,1),"disable/re-enable did not start a fresh diagnostic interval");
    Require(refs.empty()&&!pending,"final observer cleanup");
    std::cout<<"mounted native observer and trace budget passed\n";
}
