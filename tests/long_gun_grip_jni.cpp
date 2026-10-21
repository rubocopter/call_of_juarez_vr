// Production capture, paired hand mapping, commit, ray and restoration. Only
// JNI/native boundaries are simulated; replacement weapons have distinct storage.
#include <windows.h>
#undef near
#undef far
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include "games/call_of_juarez/long_gun_grip.hpp"
#define private public
#include "games/call_of_juarez/java_player_bridge.hpp"
#undef private
#include "../src/games/call_of_juarez/java_player_bridge.cpp"
namespace {
using namespace cojvr::games::call_of_juarez;
namespace rt=cojvr::runtime;
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Vector{float x=0,y=0,z=0;};
struct Weapon{std::array<ElementWorldBasisTarget,3> f{};} a,b;
int player,pc,wc,vc,other,exception;
Weapon* current=&a;
const char* rifle_class="WeaponRifleWinchester";
int holder_element=1;
std::array<ElementWorldBasisTarget,40> hands{};
std::array<Vector,16000> vectors{};int allocated=0;
void* table[224]{};void** env=table;void* vtable[8]{};void** vm=vtable;
std::map<void*,int> locals;
std::array<void*,8> cache{};
bool pending=false,fail_write=false,hidden=true,reloading=false;
int writes=0,globals=0;
std::array<int,4> right_states{1,1,1,1};
void* Local(void* p){if(p)++locals[p];return p;}
void __stdcall Drop(void*,void* p){Check(locals[p]>0,"local underflow");if(!--locals[p])locals.erase(p);}
void* __stdcall Ex(void*){return pending?Local(&exception):nullptr;}
void __stdcall Clear(void*){pending=false;}
int __stdcall Env(void*,void** out,int){*out=&env;return 0;}
void* __stdcall Class(void*,void* p){return Local(p==&player?&pc:&wc);}
void* __stdcall Find(void*,const char* name){return Local(!std::strcmp(name,rifle_class)?&wc:&other);}
unsigned char __stdcall Same(void*,void* x,void* y){return x==y;}
unsigned char __stdcall Instance(void*,void*,void*){return 0;}
unsigned char __stdcall BoolField(void*,void*,void*){Check(false,"unexpected boolean field");return 0;}
void* __stdcall Method(void*,void*,const char* name,const char*){return const_cast<char*>(name);}
bool Name(void* m,const char* n){return !std::strcmp(static_cast<char*>(m),n);}
void* __stdcall Object(void*,void* p,void* m,const JValue* args){
    Check(p==&player,"wrong object getter owner");
    if(Name(m,"GetPositionVectorVolatile")){vectors.back()={};return Local(&vectors.back());}
    Check(args&&args[0].i>=0&&args[0].i<2,"wrong hand query");
    Check(Name(m,"GetActiveWeapon")||Name(m,"GetActualWeaponNotEmpty")||Name(m,"GetDesiredWeapon"),"unknown object getter");
    return args[0].i?nullptr:Local(current);
}
int __stdcall Int(void*,void* p,void* m,const JValue* args){
    if(Name(m,"GetThisID"))return p==&player?100:200; // deliberately SAME ID for replacement B
    if(Name(m,"GetBarrelElement"))return 2;
    if(Name(m,"GetElementsNumber"))return 3;
    if(Name(m,"GetElementID")){
        const auto* n=static_cast<char*>(args[0].l);
        if(p!=&player){Check(!std::strcmp(n,"Holder"),"unknown weapon holder");return holder_element;}
        if(!std::strcmp(n,"left_hand"))return 39;
        if(!std::strcmp(n,"right_hand"))return 19;
        if(!std::strcmp(n,"CoJVRHiddenArms"))return 40;
        Check(false,"unexpected mesh name");
    }
    const char* state_names[]{"GetHandStateMashineState","GetHandStateMashineDestinyState",
        "GetActualWeaponState","GetDesiredWeaponState"};
    for(int i=0;i<4;++i)if(Name(m,state_names[i]))return args[0].i?0:right_states[i];
    Check(false,"unknown integer getter");return -1;
}
rt::Vec3 X(const ElementWorldBasisTarget& f){return rt::two_hand_detail::Cross(f.up,f.forward);}
unsigned char __stdcall Bool(void*,void* p,void* m,const JValue* args){
    if(Name(m,"IsNotAlive")||Name(m,"IsCarrying"))return 0;
    if(Name(m,"IsWeaponReloading"))return reloading;
    if(std::strstr(static_cast<char*>(m),"OperatedTwoHand"))return args[0].i==0;
    if(Name(m,"IsElementHidden"))return hidden;
    if(Name(m,"UnhideElement")){hidden=false;return 1;}
    if(Name(m,"HideElement")){hidden=true;return 1;}
    if(Name(m,"GetBarrelOrigin")||Name(m,"GetBarrelDir")){
        auto& f=static_cast<Weapon*>(p)->f[2];auto v=f.position;
        if(Name(m,"GetBarrelDir")){v=X(f);v={-v.x,-v.y,-v.z};}
        *static_cast<Vector*>(args[0].l)={v.x,v.y,v.z};return 1;
    }
    Check(Name(m,"GetElementPos")||Name(m,"GetElementLeftVector")||Name(m,"GetElementUpVector"),"unknown frame getter");
    const auto& f=p==&player?hands.at(args[0].i):static_cast<Weapon*>(p)->f.at(args[0].i);
    auto v=Name(m,"GetElementPos")?f.position:Name(m,"GetElementUpVector")?f.up:X(f);
    *static_cast<Vector*>(args[1].l)={v.x,v.y,v.z};return 1;
}
void* __stdcall Text(void*,const char* n){return Local(const_cast<char*>(n));}
void* __stdcall New(void*,void*,void*,const JValue*){Check(allocated<int(vectors.size()-1),"vectors exhausted");return Local(&vectors[allocated++]);}
float __stdcall Float(void*,void* p,void* f){auto& v=*static_cast<Vector*>(p);return Name(f,"fX")?v.x:Name(f,"fY")?v.y:v.z;}
void __stdcall SetFloat(void*,void* p,void* f,float x){auto& v=*static_cast<Vector*>(p);(Name(f,"fX")?v.x:Name(f,"fY")?v.y:v.z)=x;}
void* __stdcall Global(void*,void* p){++globals;return p;}
void __stdcall DeleteGlobal(void*,void*){--globals;}
const char* names[]{"cojvrRightOrigin","cojvrLeftOrigin","cojvrRightDirection","cojvrLeftDirection","cojvrRightFxUp","cojvrLeftFxUp","cojvrRightFxForward","cojvrLeftFxForward"};
int Index(void* f){for(int i=0;i<8;++i)if(Name(f,names[i]))return i;Check(false,"unknown cache");return -1;}
void* __stdcall Field(void*,void*,const char* n,const char*){
    for(const auto* known:names)if(!std::strcmp(known,n))return const_cast<char*>(n);
    return nullptr;
}
void* __stdcall Get(void*,void*,void* f){return Local(cache[Index(f)]);}
void __stdcall Set(void*,void*,void* f,void* p){cache[Index(f)]=p;}
void __stdcall Write(void*,void* p,void* m,const JValue* args){
    Check(Name(m,"FromUpForwardPosElementWorld"),"unknown native setter");++writes;
    if(fail_write){pending=true;return;}
    const auto vec=[](void* p){auto& v=*static_cast<Vector*>(p);return rt::Vec3{v.x,v.y,v.z};};
    auto& f=p==&player?hands.at(args[0].i):static_cast<Weapon*>(p)->f.at(args[0].i);
    f={vec(args[3].l),vec(args[1].l),vec(args[2].l),true};
}
bool Equal(const ElementWorldBasisTarget& x,const ElementWorldBasisTarget& y){
    const auto eq=[](auto a,auto b){return std::abs(a.x-b.x)<.002F&&std::abs(a.y-b.y)<.002F&&std::abs(a.z-b.z)<.002F;};
    return eq(x.position,y.position)&&eq(x.up,y.up)&&eq(x.forward,y.forward);
}
void Bind(JavaPlayerBridge& bridge){
    table[6]=(void*)Find;table[15]=(void*)Ex;table[17]=(void*)Clear;table[21]=(void*)Global;table[22]=(void*)DeleteGlobal;
    table[23]=(void*)Drop;table[24]=(void*)Same;table[30]=(void*)New;table[31]=(void*)Class;table[32]=(void*)Instance;
    table[33]=(void*)Method;table[36]=(void*)Object;table[39]=(void*)Bool;table[51]=(void*)Int;table[63]=(void*)Write;
    table[94]=(void*)Field;table[95]=(void*)Get;table[104]=(void*)Set;table[102]=(void*)Float;table[111]=(void*)SetFloat;table[167]=(void*)Text;
    table[96]=(void*)BoolField;
    vtable[6]=(void*)Env;bridge.vm_=&vm;bridge.being_=&player;bridge.being_generation_=7;
    bridge.vector_class_=&vc;bridge.vector_constructor_=(void*)"<init>";
    bridge.vector_x_field_=(void*)"fX";bridge.vector_y_field_=(void*)"fY";bridge.vector_z_field_=(void*)"fZ";
    bridge.get_position_vector_method_=(void*)"GetPositionVectorVolatile";
    bridge.element_world_read_lookup_attempted_=bridge.element_world_basis_lookup_attempted_=true;
    bridge.get_element_position_method_=(void*)"GetElementPos";bridge.get_element_left_method_=(void*)"GetElementLeftVector";
    bridge.get_element_up_method_=(void*)"GetElementUpVector";bridge.set_element_world_basis_method_=(void*)"FromUpForwardPosElementWorld";
    bridge.element_visibility_lookup_attempted_=true;bridge.is_element_hidden_method_=(void*)"IsElementHidden";
    bridge.get_element_id_method_=(void*)"GetElementID";
    bridge.hide_element_method_=(void*)"HideElement";bridge.unhide_element_method_=(void*)"UnhideElement";
    bridge.independent_hand_generation_=7;
    for(int h=0;h<2;++h)for(int i=0;i<20;++i){bridge.independent_hand_elements_[h][i]=h*20+i;hands[h*20+i]={{float(i),float(h*20),0},{0,1,0},{0,0,1},true};}
    hands[39].position={0,20,-30};
    a.f={ElementWorldBasisTarget{{0,0,0},{0,1,0},{0,0,1},true},
        ElementWorldBasisTarget{{19,0,0},{0,1,0},{0,0,1},true},
        ElementWorldBasisTarget{{-40,0,0},{0,1,0},{0,0,1},true}};
}
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"normal";
    if(mode=="root-holder"){rifle_class="WeaponRifleRemington";holder_element=0;}
    JavaPlayerBridge bridge;Bind(bridge);const auto natural=a.f;const auto original_hands=hands;
    bool right_only=false;
    Check(bridge.TryObserveLongGunFireOwnership(right_only)&&right_only,
        "bridge lost right-only rifle trigger ownership before grip acquisition");
    CoJLongGunGripGeometry g{};std::string error;
    Check(bridge.TryObserveLongGunSupport(g.native),"real rifle observation failed");
    const auto guide_epoch=g.native.object_epoch;const auto guide_ref=bridge.long_gun_grip_owner_;
    Check(bridge.IsLongGunGripGuideOwnerCurrent(g.native),"read-only guide owner check rejected an idle rifle");
    reloading=true;
    Check(!bridge.IsLongGunGripGuideOwnerCurrent(g.native)&&bridge.long_gun_grip_epoch_==guide_epoch&&
        bridge.long_gun_grip_owner_==guide_ref&&globals==1&&writes==0,
        "guide-only read failure retired acquisition identity/release baseline");
    reloading=false;
    pending=true;
    Check(!bridge.IsLongGunGripGuideOwnerCurrent(g.native)&&!pending&&locals.empty()&&
        bridge.long_gun_grip_epoch_==guide_epoch&&bridge.long_gun_grip_owner_==guide_ref&&globals==1&&writes==0,
        "optional guide exception leaked or changed acquisition ownership");
    Check(bridge.TryObserveLongGunSupport(g.native)&&g.native.object_epoch==guide_epoch,
        "guide visibility failure changed the next acquisition epoch");
    b.f=a.f;current=&b;
    Check(!bridge.IsLongGunGripGuideOwnerCurrent(g.native)&&bridge.long_gun_grip_epoch_==guide_epoch&&
        bridge.long_gun_grip_owner_==guide_ref&&globals==1,
        "guide accepted a same-ID replacement or changed retained acquisition ownership");
    current=&a;
    g.dominant_socket_cm=hands[19].position;g.muzzle_cm=a.f[2].position;g.muzzle_direction={-1,0,0};
    g.dominant_grip_cm={119,100,100};g.direction={-1,0,0};g.up={0,1,0};g.support_grip_cm={100,120,70};
    const auto frame=BuildCoJLongGunGripFrame(g);Check(frame.valid,"real rifle geometry failed");
    ElementWorldBasisTarget right{};bool unarmed=false;
    const bool prepared=bridge.TryPrepareTrackedWeapon(0,g.dominant_socket_cm,g.dominant_grip_cm,frame.plan.muzzle_direction,
        frame.plan.root.up,hands[3],right,unarmed,&error);
    if(!prepared)std::cerr<<error<<'\n';
    Check(prepared,"real rifle prepare failed");
    Check(bridge.BindPreparedLongGunSupport(g.native,frame.mapped_support_frame,&error),"real rifle binding failed");
    const auto left=TransformWeaponElementFrame(g.native.support,frame.mapped_support_frame,hands[23]);
    Check(bridge.TryApplyIndependentHand(0,original_hands[3],right,&error)&&
        bridge.TryApplyIndependentHand(1,original_hands[23],left,&error),"real paired hand mapping failed");
    JavaPlayerPosition origin{},direction{};bool active=false;
    if(mode=="commit"){
        b.f=a.f;current=&b;const int before=writes;
        Check(!bridge.ApplyPreparedTrackedWeapon(0,origin,&error)&&before==writes,"replacement accepted before first write");
    }else{
        Check(bridge.ApplyPreparedTrackedWeapon(0,origin,&error),"real rifle commit failed");
        Check(bridge.TryObserveLongGunFireOwnership(right_only)&&right_only,
            "scoped weapon/hand map hid native rifle trigger ownership");
        Check(bridge.VerifyTrackedWeapon(0,active,&error)&&active,"paired rifle verification failed");
        Check(bridge.TryGetWeaponBarrel(0,origin,direction,&error)&&bridge.TryPublishWeaponRay(0,origin,direction,true,&error),"real rifle ray failed");
        if(mode=="replace"){
            b.f=a.f;current=&b;
            Check(!bridge.VerifyTrackedWeapon(0,active,&error)&&active,"same-pose replacement verified");
            Check(!bridge.TryGetWeaponBarrel(0,origin,direction,&error),"replacement escaped barrel binding");
            Check(!bridge.TryPublishWeaponRay(0,origin,direction,true)&&!cache[0]&&!cache[2],"replacement retained old ray");
        }else if(mode=="normal"){
            hands[39].position.x+=1;
            Check(!bridge.VerifyTrackedWeapon(0,active,&error),"detached support verified");hands[39].position.x-=1;
            fail_write=true;Check(!bridge.RestoreTrackedHands(&error)&&globals>=3,"failed parent restore lost globals");fail_write=false;
        }
    }
    const auto saved_b=b.f;
    Check(bridge.RestoreTrackedHands(&error),"parent restore retry failed");
    if(mode=="normal"){
        fail_write=true;
        Check(!bridge.RestoreTrackedWeapons(&error)&&bridge.weapon_overlays_[0].weapon==&a&&globals==2,
            "failed child restore lost original weapon/global");
        fail_write=false;
    }
    Check(bridge.RestoreTrackedWeapons(&error),"child restore retry failed");
    for(int i=0;i<3;++i)Check(Equal(a.f[i],natural[i])&&Equal(b.f[i],saved_b[i]),"wrong weapon restored");
    for(int i=0;i<40;++i)Check(Equal(hands[i],original_hands[i]),"hand preimage lost");
    Check(globals==1,"completed overlays did not retain exactly one rifle identity");
    if(mode=="cross-frame"){
        // The support claim outlives the completed/restored element overlay.
        // A new Java object may reuse the exact native ID and all geometry.
        CoJLongGunGripState grip;
        CoJLongGunGripInput input{};input.player_generation=7;input.context=9;
        auto& controls=input.controls;
        controls.enabled=controls.eligible=controls.tracking_valid=true;
        controls.grip_available=controls.support_trigger_available=true;
        controls.input_generation=1;controls.acquisition_radius_m=.1F;
        const auto step=[&]{++controls.sequence;controls.now_ms=controls.sample_time_ms=1000+controls.sequence*10;
            return grip.Update(input,g);};
        Check(bridge.TryObserveLongGunSupport(g.native),"completed-frame owner observation failed");
        (void)step();controls.grip_held=true;
        Check(step().attached,"cross-frame fixture did not acquire support");
        Check(bridge.TryObserveLongGunSupport(g.native)&&step().attached,
            "same Java object lost its completed-frame support claim");
        const auto acquired_epoch=g.native.object_epoch;
        right_states={30,31,38,1};
        Check(bridge.TryObserveLongGunSupport(g.native)&&g.native.firing&&
            g.native.object_epoch==acquired_epoch&&step().attached,
            "native fire/cycle replaced the retained Java identity or support claim");
        right_states={1,1,1,1};
        b.f=a.f;current=&b;
        Check(bridge.TryObserveLongGunSupport(g.native),"same-ID replacement observation failed");
        const auto replaced=step();
        Check(replaced.claim.cancelled&&!replaced.attached&&replaced.claim.consume_grip,
            "same-ID Java replacement inherited support across completed frames");
        Check(!step().attached,"held grip silently reacquired replacement");
        controls.grip_held=false;Check(step().claim.consume_grip,"replacement release leaked");
        controls.grip_held=true;Check(step().claim.acquired,"replacement fresh regrip failed");
        const auto replacement_epoch=g.native.object_epoch;
        // Cleanup failure revokes authority while retaining the exact global
        // handle; a later observation must retry cleanup before replacing it.
        table[22]=nullptr;
        Check(!bridge.ClearLongGunGripOwner()&&globals==1,
            "unavailable JNI cleanup lost the retained rifle reference");
        Check(!bridge.TryObserveLongGunSupport(g.native)&&globals==1,
            "failed cleanup admitted a new rifle identity");
        table[22]=(void*)DeleteGlobal;
        Check(bridge.TryObserveLongGunSupport(g.native)&&globals==1&&
            g.native.object_epoch!=replacement_epoch&&!step().attached,
            "cleanup retry reused retired authority or leaked an identity");
    }
    // Clear and Reset must retire cross-frame identity too. Bind() deliberately
    // supplies borrowed player/vector fixture handles, so own the player before
    // exercising ClearBeing and leave the borrowed vector out of Reset cleanup.
    (void)Global(&env,&player);
    bridge.ClearBeing(&env);
    Check(!bridge.being_&&!globals,"ClearBeing leaked the persistent rifle/player owner");
    bridge.vector_class_=nullptr;
    bridge.long_gun_grip_owner_=Global(&env,&a);
    bridge.long_gun_grip_epoch_=9;bridge.long_gun_grip_generation_=7;
    bridge.Reset();
    Check(locals.empty()&&!globals&&!pending,"JNI reference/exception leak");
    bridge.being_=nullptr;bridge.vm_=nullptr;
    std::cout<<"paired rifle capture/commit/ray/restore and original-object guards passed: "<<mode<<'\n';
}
