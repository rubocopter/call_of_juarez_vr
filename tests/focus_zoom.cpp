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
int module_class,bullet_class,module,replacement_module,bullet,replacement_bullet;
int other_bow,bow_alias,ordinary_weapon;
void* module_owner=&module;void* bullet_owner=&bullet;
float current_slomo=1,slomo=.2F;
bool replace_weapon=false,replace_module=false,replace_bullet=false;
bool instance_fail=false,same_fail=false,call_fail=false,object_fail=false;
void* active[2]{},*desired[2]{};bool pending=false;const char* fail=nullptr;
float factor=0.5F,maximum=2;bool class_fail=false,float_fail=false;
void* __stdcall Class(void*,void* p){Require(!pending,"JNI class lookup with pending exception");if(class_fail){pending=true;return nullptr;}++refs;
    if(p==&player)return &clazz;if(p==&module)return &module_class;if(p==&bullet)return &bullet_class;
    Require(false,"unexpected object class");return nullptr;}
void* __stdcall Lookup(void*,void* c,const char* name,const char* sig){
    Require(!pending,"invalid member lookup");
    const bool field=std::strcmp(sig,"F")==0;
    const bool player_field=c==&clazz&&field&&(std::strcmp(name,"m_fSquintFactor")==0||std::strcmp(name,"m_fCurrentMaxSquintZoom")==0);
    const bool bullet_field=c==&bullet_class&&field&&(std::strcmp(name,"m_fCurrentSlomo")==0||std::strcmp(name,"_SLOMO")==0);
    const bool object_field=(c==&clazz&&std::strcmp(name,"m_Module")==0&&std::strcmp(sig,"LLawmanModule;")==0)||
        (c==&module_class&&std::strcmp(name,"m_cBulletTime")==0&&std::strcmp(sig,"LBulletTime;")==0);
    const bool method=c==&clazz&&std::strcmp(sig,"(I)LWeapon;")==0&&(std::strcmp(name,"GetDesiredWeapon")==0||std::strcmp(name,"GetActualWeaponNotEmpty")==0);
    Require(player_field||bullet_field||object_field||method,"unexpected native access");
    if(fail&&std::strcmp(fail,name)==0){pending=true;return nullptr;}
    return const_cast<char*>(name);
}
float __stdcall Float(void*,void* p,void* id){Require(!pending,"invalid float read");if(float_fail){pending=true;return 2;}
    auto name=static_cast<char*>(id);
    if(p==&player)return std::strcmp(name,"m_fSquintFactor")==0?factor:maximum;
    Require(p==&bullet,"unexpected float owner");
    if(replace_weapon)active[0]=desired[0]=&other_bow;
    if(replace_module)module_owner=&replacement_module;
    if(replace_bullet)bullet_owner=&replacement_bullet;
    return std::strcmp(name,"m_fCurrentSlomo")==0?current_slomo:slomo;}
void* __stdcall Object(void*,void* p,void* id){Require(!pending,"object read after exception");
    void* value=nullptr;auto name=static_cast<char*>(id);
    if(p==&player&&std::strcmp(name,"m_Module")==0)value=module_owner;
    else {Require(p==&module&&std::strcmp(name,"m_cBulletTime")==0,"unexpected object field");value=bullet_owner;}
    if(value)++refs;if(object_fail)pending=true;return value;}
void* __stdcall Exception(void*){if(pending){++refs;return &exception;}return nullptr;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void*){Require(refs>0,"reference underflow");--refs;}
void* __stdcall Find(void*,const char* name){if(fail&&std::strcmp(fail,name)==0){pending=true;return nullptr;}
    ++refs;if(std::strcmp(name,"WeaponBow")==0)return &bow_class;
    if(std::strcmp(name,"WeaponRifleWinchesterScope")==0)return &scope_class;
    Require(std::strcmp(name,"LawmanModuleSingle")==0,"unexpected class");return &module_class;}
unsigned char __stdcall Instance(void*,void* object,void* cls){Require(!pending,"instance query after exception");if(instance_fail)pending=true;
    return ((object==&bow_weapon||object==&other_bow||object==&bow_alias)&&cls==&bow_class)||
        (object==&scope_weapon&&cls==&scope_class)||(object==&module&&cls==&module_class);}
unsigned char __stdcall Same(void*,void* a,void* b){Require(!pending,"identity query after exception");if(same_fail)pending=true;
    if(a==&bow_alias)a=&bow_weapon;if(b==&bow_alias)b=&bow_weapon;return a==b;}
union Value{long long alignment;void* object;int integer;};
void* __stdcall Call(void*,void* p,void* id,const Value* args){Require(p==&player&&!pending&&args[0].integer>=0&&args[0].integer<2,"invalid weapon query");
    auto weapon=std::strcmp(static_cast<char*>(id),"GetDesiredWeapon")==0?desired[args[0].integer]:active[args[0].integer];
    if(weapon)++refs;if(call_fail)pending=true;return weapon;}
template<class T>void Set(int n,T fn){table[n]=reinterpret_cast<void*>(fn);}
void CheckReader(bool expected){CoJFocusZoom out{1,2,2,true};Require(ReadCoJFocusZoom(&holder,&player,out)==expected,"reader availability");Require(refs==0&&!pending,"JNI cleanup");if(!expected)Require(!out.valid&&out.magnification==1,"stale zoom retained");}
}
int main(){
    Set(31,Class);Set(94,Lookup);Set(102,Float);Set(15,Exception);Set(17,Clear);Set(23,Delete);
    Set(6,Find);Set(24,Same);Set(32,Instance);Set(33,Lookup);Set(36,Call);Set(95,Object);
    CoJFocusZoom out{};Require(ReadCoJFocusZoom(&holder,&player,out),"native focus unavailable");
    Require(out.valid&&out.factor==.5F&&out.maximum==2&&out.magnification==1.5F,"native interpolation");
    // Original bytecode: half concentration plus 1/4 squint gives 5/8,
    // hence 2.25x with the native cached maximum of 3. No logical Focus
    // request and no weapon parameter factory can supply this result.
    factor=.25F;maximum=3;current_slomo=.6F;active[0]=desired[0]=&bow_weapon;
    Require(ReadCoJFocusZoom(&holder,&player,out)&&std::abs(out.factor-.625F)<1e-6F&&
        std::abs(out.magnification-2.25F)<1e-6F,"bow concentration must combine with cached squint");
    const auto bow_zoom=out;
    Require(refs==0&&!pending,"bow JNI cleanup");
    struct BowCase{float squint,current,target,want;};
    for(auto sample:{BowCase{0,1,.2F,1},BowCase{0,.6F,.2F,2},BowCase{0,.2F,.2F,3},
        BowCase{0,.1F,.2F,3},BowCase{0,.8F,.2F,1.5F},BowCase{.25F,1,.2F,1.5F},
        BowCase{1,.6F,.2F,3},BowCase{0,1,1,1},BowCase{0,.6F,1,1}}){
        factor=sample.squint;current_slomo=sample.current;slomo=sample.target;
        Require(ReadCoJFocusZoom(&holder,&player,out)&&std::abs(out.magnification-sample.want)<1e-6F,"bow native cache ramp/decay");
        Require(refs==0&&!pending,"bow ramp cleanup");
    }
    factor=.25F;current_slomo=.6F;slomo=.2F;
    // JNI handles may alias the same native Java weapon. Both hands may
    // contain that shared two-hand owner; pointer equality is insufficient.
    desired[0]=&bow_alias;CheckReader(true);active[1]=desired[1]=&bow_alias;CheckReader(true);
    active[1]=desired[1]=&other_bow;CheckReader(false);active[1]=desired[1]=nullptr;
    for(int hand=0;hand<2;++hand){active[0]=desired[0]=nullptr;active[hand]=desired[hand]=&bow_weapon;CheckReader(true);
        desired[hand]=nullptr;CheckReader(false);desired[hand]=&other_bow;CheckReader(false);
        active[hand]=nullptr;desired[hand]=&bow_weapon;CheckReader(false);desired[hand]=nullptr;}
    active[0]=desired[0]=&bow_weapon;active[1]=&ordinary_weapon;CheckReader(false);active[1]=nullptr;
    for(auto flag:{&replace_weapon,&replace_module,&replace_bullet}){*flag=true;CheckReader(false);*flag=false;
        active[0]=desired[0]=&bow_weapon;module_owner=&module;bullet_owner=&bullet;}
    for(auto name:{"m_Module","LawmanModuleSingle","m_cBulletTime","m_fCurrentSlomo","_SLOMO"}){
        fail=name;CheckReader(false);fail=nullptr;CheckReader(true);}
    module_owner=nullptr;CheckReader(false);module_owner=&replacement_module;CheckReader(false);module_owner=&module;
    bullet_owner=nullptr;Require(ReadCoJFocusZoom(&holder,&player,out)&&out.magnification==1.5F,"absent native BulletTime must retain ordinary squint");bullet_owner=&bullet;
    for(float v:{0.F,-1.F,1.01F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        current_slomo=v;CheckReader(false);current_slomo=.6F;slomo=v;CheckReader(false);slomo=.2F;}
    for(auto flag:{&instance_fail,&same_fail,&call_fail,&object_fail,&class_fail,&float_fail}){*flag=true;CheckReader(false);*flag=false;CheckReader(true);}
    for(int index:{24,95}){auto saved=table[index];table[index]=nullptr;CheckReader(false);table[index]=saved;}
    active[1]=desired[1]=&scope_weapon;CheckReader(false);active[1]=desired[1]=nullptr;
    active[0]=desired[0]=nullptr;factor=.5F;maximum=2;current_slomo=1;
    current_slomo=.2F;Require(ReadCoJFocusZoom(&holder,&player,out)&&out.magnification==1.5F,"concentration without a bow must not magnify");current_slomo=1;
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
    for(int i=0;i<2;++i){optical[i].eye_to_head.position.y=.001F;optical[i].eye_to_head.position.z=-.002F;
        optical[i].eye_to_head.orientation={0,i?.02F:-.02F,0,.9998F};
        optical[i].eye_to_head.orientation_valid=true;optical[i].eye_to_head.position_valid=true;}
    auto render=optical;
    Require(BuildCoJFocusRenderEyes(optical,bow_zoom,render),"read bow zoom must reach stereo render eyes");
    for(int i=0;i<2;++i){const auto& a=render[i];const auto& b=optical[i];
        Require(std::abs(std::tan(a.fov.angle_left)*2.25F-std::tan(b.fov.angle_left))<1e-5F&&
            std::abs(std::tan(a.fov.angle_right)*2.25F-std::tan(b.fov.angle_right))<1e-5F&&
            std::abs(std::tan(a.fov.angle_up)*2.25F-std::tan(b.fov.angle_up))<1e-5F&&
            std::abs(std::tan(a.fov.angle_down)*2.25F-std::tan(b.fov.angle_down))<1e-5F,"bow asymmetric stereo rays");
        Require(a.eye==b.eye&&a.width==b.width&&a.height==b.height&&
            a.eye_to_head.position.x==b.eye_to_head.position.x&&a.eye_to_head.position.y==b.eye_to_head.position.y&&a.eye_to_head.position.z==b.eye_to_head.position.z&&
            a.eye_to_head.orientation.x==b.eye_to_head.orientation.x&&a.eye_to_head.orientation.y==b.eye_to_head.orientation.y&&
            a.eye_to_head.orientation.z==b.eye_to_head.orientation.z&&a.eye_to_head.orientation.w==b.eye_to_head.orientation.w&&
            a.eye_to_head.position_valid==b.eye_to_head.position_valid&&a.eye_to_head.orientation_valid==b.eye_to_head.orientation_valid,"bow changed optical transform/IPD/identity");
    }
    active[0]=desired[0]=&bow_weapon;fail="_SLOMO";
    Require(!ReadCoJFocusZoom(&holder,&player,out)&&!out.valid,"fault retained a valid bow cache");
    Require(!BuildCoJFocusRenderEyes(optical,out,render),"fault reused previous bow render optics");
    for(int i=0;i<2;++i)Require(render[i].fov.angle_left==optical[i].fov.angle_left&&
        render[i].fov.angle_right==optical[i].fov.angle_right&&render[i].fov.angle_up==optical[i].fov.angle_up&&
        render[i].fov.angle_down==optical[i].fov.angle_down,"fault failed to restore full asymmetric optics");
    Require(refs==0&&!pending,"render fallback JNI cleanup");fail=nullptr;active[0]=desired[0]=nullptr;
    out={1,2,2,true};Require(BuildCoJFocusRenderEyes(optical,out,render),"valid eye zoom rejected");
    for(int i=0;i<2;++i){Require(std::abs(std::tan(render[i].fov.angle_left)*2-std::tan(optical[i].fov.angle_left))<1e-5F,"asymmetric tangent lost");Require(std::abs(std::tan(render[i].fov.angle_right)*2-std::tan(optical[i].fov.angle_right))<1e-5F,"right tangent lost");Require(std::abs(std::tan(render[i].fov.angle_up)*2-std::tan(optical[i].fov.angle_up))<1e-5F&&std::abs(std::tan(render[i].fov.angle_down)*2-std::tan(optical[i].fov.angle_down))<1e-5F,"vertical tangent lost");Require(render[i].eye_to_head.position.x==optical[i].eye_to_head.position.x&&render[i].eye==optical[i].eye&&render[i].width==1440&&render[i].height==1600,"eye identity/pose/extent changed");}
    Require(optical[0].fov.angle_left==-.9F,"runtime optics modified");
    out={};Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unavailable focus fallback");
    out={0,2,1,true};Require(BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unity not exact");
    for(float scale:{.99F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        out.magnification=scale;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"invalid scale accepted");}
    out={1,2,2,true};optical[1].fov.angle_up=1.6F;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"partial stereo zoom after bad optics");
}
