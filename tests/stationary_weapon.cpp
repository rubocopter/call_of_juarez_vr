#include "games/call_of_juarez/stationary_weapon.hpp"
#include "games/call_of_juarez/stationary_aim.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
using namespace cojvr::games::call_of_juarez;
namespace {
void Require(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
void* P(int i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(i));}
void* table[176]{};void** env=table;std::map<void*,int> refs;
bool pending=false,active=true,dead=false,controller=true,aliases=false;
bool frame_ok=true,replace_in_frame=false;int frame_reads=0,barrel=3;
bool input_allowed=true,replace_on_axis=false;float sensitivity=2.5F;
std::vector<cojvr::runtime::Vec2> mouse_events;
int steps=0,fail=-1,objects=0,replace_at=-1;
std::uint32_t identity=0xf1234001U;
using Value=long_gun_observer_detail::Value;
bool Step(){Require(!pending,"uncleared JNI exception");if(++steps==fail){pending=true;return false;}return true;}
void* Local(void* p){if(p)++refs[p];return p;}
void __stdcall Delete(void*,void* p){Require(refs[p]>0,"JNI local underflow");if(!--refs[p])refs.erase(p);}
void* __stdcall Exception(void*){return pending?Local(P(999)):nullptr;}
void __stdcall Clear(void*){pending=false;}
void* __stdcall Class(void*,void* o){if(!Step())return nullptr;return Local(o==P(100)?P(110):o==P(103)?P(112):o==P(200)?P(201):P(111));}
void* __stdcall Method(void*,void* cls,const char* name,const char* desc){
    if(!Step())return nullptr;
    if(cls==P(201)&&!std::strcmp(name,"CanProcessInputs")&&!std::strcmp(desc,"()Z"))return P(7);
    if(cls==P(201)&&!std::strcmp(name,"OnInputMouseAxis")&&!std::strcmp(desc,"(IF)V"))return P(8);
    if(cls==P(202)&&!std::strcmp(name,"GetMouseSensitivity")&&!std::strcmp(desc,"()F"))return P(9);
    if(cls==P(112)&&!std::strcmp(name,"m_OwnerMesh")&&!std::strcmp(desc,"LICannonMesh;"))return P(5);
    if(cls==P(112)&&!std::strcmp(name,"m_nElementBarrelExit")&&!std::strcmp(desc,"I"))return P(6);
    const char* names[]{"IsUsingCannon","IsNotAlive","GetCannon","GetThisID"};
    for(int i=0;i<4;++i)if(!std::strcmp(name,names[i])){
        const char* expected=i<2?"()Z":i==3?"()I":cls==P(110)?"()LStationaryCannon;":"()LICannonController;";
        Require(!std::strcmp(desc,expected),"wrong stationary getter signature");return P(i+1);
    }
    Require(false,"stationary observer attempted a setter/factory");return nullptr;
}
unsigned char __stdcall Boolean(void*,void* obj,void* method,const Value* args){
    if(!Step())return 0;
    if(obj==P(200)&&method==P(7)){Require(!args,"CanProcessInputs argument mismatch");return input_allowed;}
    Require(obj==P(100)&&!args,"bad stationary predicate owner");
    Require(method==P(1)||method==P(2),"unexpected predicate");return method==P(1)?active:dead;
}
void* __stdcall Object(void*,void* obj,void* method,const Value* args){
    if(!Step())return nullptr;Require(method==P(3)&&!args,"stationary getter must be no-arg");
    ++objects;if(objects==replace_at)active=false;
    if(obj==P(100))return Local(active?(aliases&&objects>1?P(102):P(101)):nullptr);
    Require(obj==P(101)||obj==P(102),"unknown cannon owner");return Local(controller?P(103):nullptr);
}
int __stdcall Integer(void*,void* obj,void* method,const Value* args){
    if(!Step())return -1;Require((obj==P(101)||obj==P(102))&&method==P(4)&&!args,"bad cannon ID getter");
    return static_cast<int>(identity);
}
unsigned char __stdcall Same(void*,void* a,void* b){
    if(!Step())return 0;if(a==P(102))a=P(101);if(b==P(102))b=P(101);return a==b;
}
void* __stdcall ObjectField(void*,void* object,void* field){
    if(!Step())return nullptr;Require(object==P(103)&&field==P(5),"wrong cannon mesh field");return Local(P(104));
}
int __stdcall IntField(void*,void* object,void* field){
    if(!Step())return -1;Require(object==P(103)&&field==P(6),"wrong barrel field");return barrel;
}
bool Frame(void* context,void* mesh,int element,ElementWorldBasisTarget& out) noexcept {
    ++frame_reads;Require(context==P(500)&&mesh==P(104)&&element==barrel&&refs[mesh]>0,"unowned barrel frame callback");
    out={{100,150,200},{0,1,0},{0,0,-1},frame_ok};
    if(replace_in_frame)active=false;
    return frame_ok;
}
float __stdcall StaticFloat(void*,void* cls,void* method,const Value* args){
    if(!Step())return 0;Require(cls==P(202)&&method==P(9)&&!args,"wrong native input sensitivity getter");return sensitivity;
}
void __stdcall Mouse(void*,void* controller_object,void* method,const stationary_aim_detail::AxisValue* args){
    if(!Step())return;
    Require(controller_object==P(200)&&method==P(8)&&args&&args[0].integer>=0&&args[0].integer<=1,
        "cannon aim bypassed native mouse input");
    mouse_events.push_back({float(args[0].integer),args[1].real});
    if(replace_on_axis)active=false;
}
}
int main(){
    table[15]=reinterpret_cast<void*>(&Exception);table[17]=reinterpret_cast<void*>(&Clear);
    table[23]=reinterpret_cast<void*>(&Delete);table[24]=reinterpret_cast<void*>(&Same);
    table[31]=reinterpret_cast<void*>(&Class);table[33]=reinterpret_cast<void*>(&Method);
    table[36]=reinterpret_cast<void*>(&Object);table[39]=reinterpret_cast<void*>(&Boolean);
    table[51]=reinterpret_cast<void*>(&Integer);
    table[94]=reinterpret_cast<void*>(&Method);table[95]=reinterpret_cast<void*>(&ObjectField);
    table[100]=reinterpret_cast<void*>(&IntField);
    CoJStationaryWeaponOwner owner{};
    Require(ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&owner.active&&owner.cannon==identity,
        "native stationary owner not observed");
    const int complete=steps;Require(refs.empty()&&!pending,"stationary read leaked refs");
    for(int i=1;i<=complete;++i){steps=objects=0;fail=i;owner={true,17};
        Require(!ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&!owner.active&&!owner.cannon&&refs.empty()&&!pending,
            "failed stationary read admitted a stale owner or leaked");}
    fail=-1;steps=objects=0;active=false;
    Require(ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&!owner.active&&!owner.cannon,"no cannon is not known inactive");
    active=true;dead=true;
    Require(!ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&!owner.active,"dead cannon operator admitted");
    dead=false;controller=false;
    Require(!ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&!owner.active,"missing cannon controller admitted");
    controller=true;objects=0;replace_at=3;
    Require(!ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&!owner.active,"owner replacement during read admitted");
    replace_at=-1;active=true;aliases=true;objects=0;
    Require(ReadCoJStationaryWeaponOwner(&env,P(100),owner)&&owner.active,"JNI aliases compared as pointers");
    for(auto id:{0U,0xffffffffU}){identity=id;Require(!ReadCoJStationaryWeaponOwner(&env,P(100),owner),"invalid cannon ID admitted");}
    Require(refs.empty()&&!pending,"final observer refs/exception");

    identity=0xf1234001U;active=true;aliases=false;objects=steps=0;
    CoJStationaryAimRay aim;
    Require(ReadCoJStationaryAim(&env,P(100),Frame,P(500),aim)&&aim.valid&&aim.cannon==identity&&
        aim.element==3&&aim.origin.x==100&&aim.direction.z==-1,"native cannon barrel ray missing");
    const int aim_steps=steps;
    for(int i=1;i<=aim_steps;++i){steps=objects=0;fail=i;
        Require(!ReadCoJStationaryAim(&env,P(100),Frame,P(500),aim)&&!aim.valid&&refs.empty()&&!pending,
            "failed barrel read retained stale ray or leaked references");}
    fail=-1;frame_ok=false;
    Require(!ReadCoJStationaryAim(&env,P(100),Frame,P(500),aim)&&!aim.valid,"invalid barrel frame admitted");
    frame_ok=true;replace_in_frame=true;
    Require(!ReadCoJStationaryAim(&env,P(100),Frame,P(500),aim)&&!aim.valid,"replaced cannon retained reticle");
    replace_in_frame=false;active=true;barrel=-1;const auto frames_before=frame_reads;
    Require(!ReadCoJStationaryAim(&env,P(100),Frame,P(500),aim)&&frame_reads==frames_before,"missing barrel queried root accidentally");
    barrel=3;Require(refs.empty()&&!pending,"barrel observer final cleanup");

    for(std::uint64_t hz:{72ULL,90ULL,120ULL,144ULL}){
        CoJStationaryAimControl control;
        cojvr::runtime::GameplayInputState stick{};stick.active=true;stick.input_context_generation=5;
        CoJStationaryWeaponOwner cannon{true,29};
        (void)control.Update(true,cannon,2,1,1000000,stick);
        stick.turn={.625F,.625F};cojvr::runtime::Vec2 total{};
        for(std::uint64_t frame=1;frame<=hz;++frame){
            const auto now=1000000+frame*1000000/hz;
            auto delta=control.Update(true,cannon,2,frame+1,now,stick);
            total.x+=delta.x;total.y+=delta.y;
            const auto duplicate=control.Update(true,cannon,2,frame+1,now+1,stick);
            Require(duplicate.x==0&&duplicate.y==0,"duplicate pose added cannon movement");
        }
        Require(std::fabs(total.x+45)<.01F&&std::fabs(total.y-45)<.01F,"cannon vertical axis missing or refresh-rate dependent");
        control.Invalidate();
        Require(control.Update(true,cannon,2,hz+2,2100000,stick).y==0,"interruption resumed held aim");
        stick.turn={};(void)control.Update(true,cannon,2,hz+3,2110000,stick);
        stick.turn={0,-.625F};
        Require(control.Update(true,cannon,2,hz+4,2120000,stick).y<0,"downward cannon aim missing");
        cannon.cannon=30;
        Require(control.Update(true,cannon,2,hz+5,2130000,stick).y==0,"new cannon inherited held stick");
    }
    table[63]=reinterpret_cast<void*>(&Mouse);table[113]=reinterpret_cast<void*>(&Method);
    table[137]=reinterpret_cast<void*>(&StaticFloat);
    active=true;steps=objects=0;bool dispatched=false;
    CoJStationaryWeaponOwner cannon_owner{true,identity};
    Require(DispatchCoJStationaryAim(&env,P(100),P(200),P(202),cannon_owner,{-.9F,.45F},dispatched)&&
        dispatched&&mouse_events.size()==2&&mouse_events[0].x==0&&mouse_events[1].x==1&&
        std::fabs(mouse_events[0].y+.36F)<1e-6F&&std::fabs(mouse_events[1].y-.18F)<1e-6F,
        "cannon two-axis dispatch missing or native sensitivity applied twice");
    const int dispatch_steps=steps;
    for(int i=1;i<=dispatch_steps;++i){steps=objects=0;fail=i;mouse_events.clear();dispatched=true;
        Require(!DispatchCoJStationaryAim(&env,P(100),P(200),P(202),cannon_owner,{-.9F,.45F},dispatched)&&
            !dispatched&&refs.empty()&&!pending,"failed cannon axis boundary leaked or acknowledged stale input");}
    fail=-1;input_allowed=false;mouse_events.clear();
    Require(DispatchCoJStationaryAim(&env,P(100),P(200),P(202),cannon_owner,{-.9F,.45F},dispatched)&&
        !dispatched&&mouse_events.empty(),"native input permissions bypassed");
    input_allowed=true;sensitivity=0;
    Require(!DispatchCoJStationaryAim(&env,P(100),P(200),P(202),cannon_owner,{-.9F,.45F},dispatched)&&
        mouse_events.empty(),"invalid sensitivity dispatched angles");
    sensitivity=2.5F;replace_on_axis=true;
    Require(!DispatchCoJStationaryAim(&env,P(100),P(200),P(202),cannon_owner,{-.9F,.45F},dispatched)&&
        mouse_events.size()==1&&!dispatched,"new owner received a second queued axis");
    replace_on_axis=false;active=true;

    CoJStationaryFire gate;owner={true,17};
    cojvr::runtime::GameplayInputState raw{};raw.active=true;raw.input_context_generation=1;
    raw.fire_right=true;
    auto mapped=raw;std::uint64_t sequence=1;
    const auto update=[&](bool known=true){return gate.Filter(known,owner,2,53,sequence++,raw,mapped);};
    Require(update()&&!mapped.fire_left&&!mapped.fire_right,"held R2 fired on cannon entry");
    raw.fire_right=false;mapped=raw;update();
    raw.fire_right=true;mapped={};mapped.active=true;
    Require(update()&&mapped.fire_left&&!mapped.fire_right&&(mapped.digital_available&1U),
        "cannon R2 did not reach native left/action9 after inventory filters");
    mapped=raw;update();Require(mapped.fire_left&&!mapped.fire_right,"held Gatling fire stopped after one frame");
    mapped=raw;--sequence;update();Require(mapped.fire_left&&!mapped.fire_right,"repeated held pose stopped native automatic fire");
    raw.fire_right=false;mapped=raw;update();Require(!mapped.fire_left&&!mapped.fire_right,"Gatling release not neutral");
    raw.fire_left=true;mapped=raw;update();Require(!mapped.fire_left&&!mapped.fire_right,"L2 became an unintended cannon shot");
    raw.fire_left=false;raw.fire_right=true;mapped=raw;update(false);
    Require(!mapped.fire_left&&!mapped.fire_right,"unknown native owner retained a shot");
    mapped=raw;update();Require(!mapped.fire_left,"held trigger replayed after observer recovery");
    raw.fire_right=false;mapped=raw;update();raw.fire_right=true;mapped=raw;update();Require(mapped.fire_left,"fresh recovery shot lost");
    owner.cannon=18;mapped=raw;update();Require(!mapped.fire_left,"new cannon inherited held shot");
    raw.fire_right=false;raw.digital_available&=~2U;mapped=raw;update();
    raw.digital_available|=2U;raw.fire_right=true;mapped=raw;update();Require(!mapped.fire_left,"unavailable input manufactured release");
    raw.fire_right=false;mapped=raw;update();raw.fire_right=true;mapped=raw;update();Require(mapped.fire_left,"second cannon not rearmed");
    owner={};mapped=raw;update();Require(!mapped.fire_left&&!mapped.fire_right,"cannon exit fired held ordinary weapon");
    raw.fire_right=false;mapped=raw;update();raw.fire_right=true;mapped=raw;update();Require(mapped.fire_right&&!mapped.fire_left,"ordinary fresh R2 route changed");
    owner={true,19};raw.fire_right=false;mapped=raw;update();
    raw.fire_right=true;mapped=raw;--sequence;update();Require(!mapped.fire_left,"repeated pose manufactured a cannon edge");
    raw.fire_right=false;mapped=raw;update();raw.fire_right=true;mapped=raw;mapped.active=false;update();
    Require(!mapped.fire_left&&!mapped.fire_right,"blocking UI permitted cannon fire");
    mapped=raw;update();Require(!mapped.fire_left,"UI recovery replayed held cannon fire");
    {
        CoJStationaryFire ordinary;
        auto held=raw;held.fire_right=true;auto output=held;
        (void)ordinary.Filter(true,{},2,53,100,held,output);
        output=held;(void)ordinary.Filter(false,{},2,53,101,held,output);
        output=held;(void)ordinary.Filter(true,{},2,53,102,held,output);
        Require(!output.fire_left&&!output.fire_right,"unknown ownership replayed ordinary held trigger on recovery");
    }
    for(bool cancellation:{false,true}){
        CoJStationaryFire recovery;CoJStationaryWeaponOwner cannon{true,33};
        auto physical=raw;physical.fire_left=physical.fire_right=false;
        auto output=physical;(void)recovery.Filter(true,cannon,2,53,200,physical,output);
        physical.fire_right=true;output=physical;
        (void)recovery.Filter(true,cannon,2,53,201,physical,output);
        Require(output.fire_left,"recovery fixture did not begin native cannon shot");
        if(cancellation){CoJStationaryFireFrame skipped_frame{recovery};}
        else {output=physical;(void)recovery.Filter(true,cannon,2,53,202,physical,output,false);
            Require(!output.fire_left&&!output.fire_right,"explicit loading/quarantine safety was overwritten");}
        output=physical;(void)recovery.Filter(true,cannon,2,53,203,physical,output);
        Require(!output.fire_left&&!output.fire_right,"same-owner interrupted cannon replayed held fire");
        physical.fire_right=false;output=physical;(void)recovery.Filter(true,cannon,2,53,204,physical,output);
        physical.fire_right=true;output=physical;(void)recovery.Filter(true,cannon,2,53,205,physical,output);
        Require(output.fire_left,"cannon did not recover after actual release");
    }
    std::cout<<"Stationary ownership and contextual fire checks passed\n";
}
