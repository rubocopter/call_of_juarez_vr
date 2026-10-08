#include "games/call_of_juarez/focus_zoom.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
using namespace cojvr::games::call_of_juarez;
void Require(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
void* table[224]{};void** holder=table;
int player,clazz,exception,refs=0,bow_class,scope_class,bow_weapon,scope_weapon;
void* active[2]{},*desired[2]{};bool pending=false;const char* fail=nullptr;
float factor=0.5F,maximum=2;bool class_fail=false,float_fail=false;
void* __stdcall Class(void*,void* p){Require(p==&player&&!pending,"JNI class lookup with pending exception");if(class_fail){pending=true;return nullptr;}++refs;return &clazz;}
void* __stdcall Lookup(void*,void* c,const char* name,const char* sig){
    Require(c==&clazz&&!pending,"invalid member lookup");
    const bool field=std::strcmp(sig,"F")==0;
    Require(field?(std::strcmp(name,"m_fSquintFactor")==0||std::strcmp(name,"m_fCurrentMaxSquintZoom")==0):
        (std::strcmp(sig,"(I)LWeapon;")==0&&(std::strcmp(name,"GetDesiredWeapon")==0||std::strcmp(name,"GetActualWeaponNotEmpty")==0)),"unexpected native access");
    if(fail&&std::strcmp(fail,name)==0){pending=true;return nullptr;}
    return const_cast<char*>(name);
}
float __stdcall Float(void*,void* p,void* id){Require(p==&player&&!pending,"invalid float read");if(float_fail){pending=true;return 2;}return std::strcmp(static_cast<char*>(id),"m_fSquintFactor")==0?factor:maximum;}
void* __stdcall Exception(void*){if(pending){++refs;return &exception;}return nullptr;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void*){Require(refs>0,"reference underflow");--refs;}
void* __stdcall Find(void*,const char* name){if(fail&&std::strcmp(fail,name)==0){pending=true;return nullptr;}
    Require(std::strcmp(name,"WeaponBow")==0||std::strcmp(name,"WeaponRifleWinchesterScope")==0,"unexpected class");
    ++refs;return std::strcmp(name,"WeaponBow")==0?&bow_class:&scope_class;}
unsigned char __stdcall Instance(void*,void* object,void* cls){Require(!pending,"instance query after exception");return (object==&bow_weapon&&cls==&bow_class)||(object==&scope_weapon&&cls==&scope_class);}
union Value{long long alignment;void* object;int integer;};
void* __stdcall Call(void*,void* p,void* id,const Value* args){Require(p==&player&&!pending&&args[0].integer>=0&&args[0].integer<2,"invalid weapon query");
    auto weapon=std::strcmp(static_cast<char*>(id),"GetDesiredWeapon")==0?desired[args[0].integer]:active[args[0].integer];
    if(weapon)++refs;return weapon;}
template<class T>void Set(int n,T fn){table[n]=reinterpret_cast<void*>(fn);}
void CheckReader(bool expected){CoJFocusZoom out{1,2,2,true};Require(ReadCoJFocusZoom(&holder,&player,out)==expected,"reader availability");Require(refs==0&&!pending,"JNI cleanup");if(!expected)Require(!out.valid&&out.magnification==1,"stale zoom retained");}
}
int main(){
    Set(31,Class);Set(94,Lookup);Set(102,Float);Set(15,Exception);Set(17,Clear);Set(23,Delete);
    Set(6,Find);Set(32,Instance);Set(33,Lookup);Set(36,Call);
    CoJFocusZoom out{};Require(ReadCoJFocusZoom(&holder,&player,out),"native focus unavailable");
    Require(out.valid&&out.factor==.5F&&out.maximum==2&&out.magnification==1.5F,"native interpolation");
    for(int hand=0;hand<2;++hand)for(auto weapon:{&bow_weapon,&scope_weapon}){
        active[hand]=weapon;CheckReader(false);active[hand]=nullptr;
        desired[hand]=weapon;CheckReader(false);desired[hand]=nullptr;CheckReader(true);
    }
    for(auto name:{"GetActualWeaponNotEmpty","GetDesiredWeapon","WeaponBow","WeaponRifleWinchesterScope"}){
        fail=name;CheckReader(false);fail=nullptr;CheckReader(true);}
    pending=true;CheckReader(false);
    for(auto name:{"m_fSquintFactor","m_fCurrentMaxSquintZoom"}){fail=name;CheckReader(false);}fail=nullptr;
    class_fail=true;CheckReader(false);class_fail=false;float_fail=true;CheckReader(false);float_fail=false;CheckReader(true);
    for(float v:{-1.F,1.01F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){factor=v;CheckReader(false);}factor=0;
    maximum=.99F;CheckReader(false);maximum=2;CheckReader(true);
    maximum=std::numeric_limits<float>::quiet_NaN();CheckReader(false);maximum=2;
    factor=1;CheckReader(true);factor=.25F;maximum=3;Require(ReadCoJFocusZoom(&holder,&player,out)&&out.magnification==1.5F,"native max preserved");
    Require(!ReadCoJFocusZoom(nullptr,&player,out)&&!out.valid,"null environment");
    Require(!ReadCoJFocusZoom(&holder,nullptr,out)&&!out.valid,"null player");
    for(int index:{6,15,17,23,31,32,33,36,94,102}){auto saved=table[index];table[index]=nullptr;
        Require(!ReadCoJFocusZoom(&holder,&player,out)&&!out.valid,"missing JNI function accepted");table[index]=saved;}
    std::array<cojvr::runtime::EyeView,2> optical{};
    for(int i=0;i<2;++i){optical[i].eye=i?cojvr::runtime::Eye::right:cojvr::runtime::Eye::left;optical[i].fov={-.9F,.7F,.8F,-.6F};optical[i].eye_to_head.position.x=i?.032F:-.032F;optical[i].width=1440;optical[i].height=1600;}
    optical[1].fov={-.7F,.9F,.75F,-.65F};
    auto render=optical;out={1,2,2,true};Require(BuildCoJFocusRenderEyes(optical,out,render),"valid eye zoom rejected");
    for(int i=0;i<2;++i){Require(std::abs(std::tan(render[i].fov.angle_left)*2-std::tan(optical[i].fov.angle_left))<1e-5F,"asymmetric tangent lost");Require(std::abs(std::tan(render[i].fov.angle_right)*2-std::tan(optical[i].fov.angle_right))<1e-5F,"right tangent lost");Require(std::abs(std::tan(render[i].fov.angle_up)*2-std::tan(optical[i].fov.angle_up))<1e-5F&&std::abs(std::tan(render[i].fov.angle_down)*2-std::tan(optical[i].fov.angle_down))<1e-5F,"vertical tangent lost");Require(render[i].eye_to_head.position.x==optical[i].eye_to_head.position.x&&render[i].eye==optical[i].eye&&render[i].width==1440&&render[i].height==1600,"eye identity/pose/extent changed");}
    Require(optical[0].fov.angle_left==-.9F,"runtime optics modified");
    out={};Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unavailable focus fallback");
    out={0,2,1,true};Require(BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unity not exact");
    for(float scale:{.99F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        out.magnification=scale;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"invalid scale accepted");}
    out={1,2,2,true};optical[1].fov.angle_up=1.6F;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"partial stereo zoom after bad optics");
}
