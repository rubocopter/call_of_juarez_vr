#include "games/call_of_juarez/focus_zoom.hpp"
#include "runtime/vr_math.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
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

namespace scoped_tests {
using namespace cojvr::games::call_of_juarez;
void Need(bool value,const char* why){if(!value){std::cerr<<"scope: "<<why<<'\n';std::exit(1);}}
// Exact JNI boundary fixture. Only passive members proven in the original
// archive are implemented; factories, native updates and all writes trap.
int player,player_class,weapon,weapon_alias,other_weapon,bow,bow_class,weapon_class,subclass;
int module,module_class,cache,cache_class,params,params_class,manager,manager_class,camera,camera_class,key,exception,replacement;
void* jni[224]{};void** env=jni;
void* actual[2]{},*desired[2]{};
void* module_ref=&module,*cache_ref=&cache,*params_ref=&params,*manager_ref=&manager,*camera_ref=&camera;
void* weapon_type=&weapon_class,*params_type=&params_class,*cache_type=&cache_class;
float squint=1,maximum=2,scope_zoom=8,fov=60;
std::uint32_t weapon_id=0xf2345678U;
bool dead=false,pending=false;
int states[2]{1,1},refs=0,operations=0,fault_at=0,null_at=0,mutation=0,scope_reads=0;
std::vector<std::string> trace;
std::vector<int> null_operations,nullable_operations;
bool Step(const char* name,bool nullable=false){Need(!pending,"operation with pending Java exception");trace.emplace_back(name);++operations;if(nullable)nullable_operations.push_back(operations);
    if(operations==fault_at)pending=true;return operations==null_at;}
void* Ref(void* object,bool absent=false){if(absent)return nullptr;if(object)++refs;else null_operations.push_back(operations);return object;}
void* __stdcall Exception(void*){return pending?Ref(&exception):nullptr;}
void __stdcall Clear(void*){Need(pending,"cleared absent exception");pending=false;}
void __stdcall Delete(void*,void* object){Need(object&&refs>0,"local reference underflow");--refs;}
void* __stdcall Class(void*,void* object){const bool absent=Step("GetObjectClass",true);
    if(object==&player)return Ref(&player_class,absent);
    if(object==&weapon||object==&weapon_alias)return Ref(weapon_type,absent);
    if(object==&module)return Ref(&module_class,absent);
    if(object==&cache)return Ref(cache_type,absent);
    if(object==&params)return Ref(params_type,absent);
    if(object==&manager)return Ref(&manager_class,absent);
    if(object==&camera)return Ref(&camera_class,absent);
    Need(false,"unknown class owner");return nullptr;}
void* __stdcall Find(void*,const char* name){const bool absent=Step(name,true);
    if(std::strcmp(name,"WeaponBow")==0)return Ref(&bow_class,absent);
    if(std::strcmp(name,"WeaponRifleWinchesterScope")==0)return Ref(&weapon_class,absent);
    if(std::strcmp(name,"WpnLogicRifleWinchesterScope")==0)return Ref(&params_class,absent);
    if(std::strcmp(name,"java/util/TreeMap")==0)return Ref(&cache_class,absent);
    Need(false,"unproven class lookup");return nullptr;}
void* __stdcall Lookup(void*,void* cls,const char* name,const char* sig){const bool absent=Step(name,true);
    const bool player_field=cls==&player_class&&((std::strcmp(name,"m_fSquintFactor")==0&&std::strcmp(sig,"F")==0)||
        (std::strcmp(name,"m_fCurrentMaxSquintZoom")==0&&std::strcmp(sig,"F")==0)||
        (std::strcmp(name,"bDead")==0&&std::strcmp(sig,"Z")==0)||
        (std::strcmp(name,"m_Module")==0&&std::strcmp(sig,"LLawmanModule;")==0));
    const bool module_field=cls==&module_class&&((std::strcmp(name,"m_aWeaponParams")==0&&std::strcmp(sig,"Ljava/util/TreeMap;")==0)||
        (std::strcmp(name,"m_CamerasManager")==0&&std::strcmp(sig,"LCamerasManager;")==0));
    const bool scalar_field=(cls==&params_class&&std::strcmp(name,"m_fScopeZoom")==0&&std::strcmp(sig,"F")==0)||
        (cls==&camera_class&&std::strcmp(name,"fDefaultFOV")==0&&std::strcmp(sig,"F")==0);
    const bool player_method=cls==&player_class&&(((std::strcmp(name,"GetActualWeaponNotEmpty")==0||std::strcmp(name,"GetDesiredWeapon")==0)&&std::strcmp(sig,"(I)LWeapon;")==0)||
        (std::strcmp(name,"GetHandStateMashineState")==0&&std::strcmp(sig,"(I)I")==0));
    const bool passive_method=(cls==&cache_class&&std::strcmp(name,"get")==0&&std::strcmp(sig,"(Ljava/lang/Object;)Ljava/lang/Object;")==0)||
        (cls==&manager_class&&std::strcmp(name,"GetCurrentCamera")==0&&std::strcmp(sig,"()LBeingCamera;")==0);
    const bool identity_method=cls==&weapon_class&&std::strcmp(name,"GetThisID")==0&&std::strcmp(sig,"()I")==0;
    Need(player_field||module_field||scalar_field||player_method||passive_method||identity_method,"factory/update/unproven member access");
    return absent?nullptr:const_cast<char*>(name);}
unsigned char __stdcall Instance(void*,void* object,void* cls){Step("IsInstanceOf");
    return ((object==&weapon||object==&weapon_alias||object==&other_weapon)&&cls==&weapon_class)||(object==&bow&&cls==&bow_class);}
void* Canon(void* object){return object==&weapon_alias?&weapon:object;}
unsigned char __stdcall Same(void*,void* a,void* b){Step("IsSameObject");return Canon(a)==Canon(b);}
union Value{long long alignment;void* object;int integer;};
void* __stdcall Call(void*,void* object,void* id,const Value* args){auto name=static_cast<char*>(id);const bool absent=Step(name,true);
    if(object==&player){Need(args&&args[0].integer>=0&&args[0].integer<2,"hand outside exact range");
        Need(std::strcmp(name,"GetActualWeaponNotEmpty")==0||std::strcmp(name,"GetDesiredWeapon")==0,"mutating player call");
        return Ref(std::strcmp(name,"GetActualWeaponNotEmpty")==0?actual[args[0].integer]:desired[args[0].integer],absent);}
    if(object==&cache){Need(std::strcmp(name,"get")==0&&args&&args[0].object==&key,"wrong retained cache key/get");return Ref(params_ref,absent);}
    Need(object==&manager&&std::strcmp(name,"GetCurrentCamera")==0&&!args,"unproven camera call");return Ref(camera_ref,absent);}
int __stdcall IntCall(void*,void* object,void* id,const Value* args){Step(static_cast<char*>(id));
    if(object==&weapon||object==&weapon_alias){Need(std::strcmp(static_cast<char*>(id),"GetThisID")==0&&!args,"nonpassive weapon call");return static_cast<int>(weapon_id);}
    Need(object==&player&&std::strcmp(static_cast<char*>(id),"GetHandStateMashineState")==0&&args&&args[0].integer>=0&&args[0].integer<2,"unproven state call");return states[args[0].integer];}
void* __stdcall Object(void*,void* object,void* id){auto name=static_cast<char*>(id);const bool absent=Step(name,true);
    if(object==&player&&std::strcmp(name,"m_Module")==0)return Ref(module_ref,absent);
    if(object==&module&&std::strcmp(name,"m_aWeaponParams")==0)return Ref(cache_ref,absent);
    Need(object==&module&&std::strcmp(name,"m_CamerasManager")==0,"unknown object field");return Ref(manager_ref,absent);}
unsigned char __stdcall Boolean(void*,void* object,void* id){Step("bDead");Need(object==&player&&std::strcmp(static_cast<char*>(id),"bDead")==0,"unproven eligibility read");return dead;}
float __stdcall Float(void*,void* object,void* id){auto name=static_cast<char*>(id);Step(name);
    if(object==&player){if(std::strcmp(name,"m_fSquintFactor")==0)return squint;Need(std::strcmp(name,"m_fCurrentMaxSquintZoom")==0,"unknown squint field");return maximum;}
    if(object==&camera){Need(std::strcmp(name,"fDefaultFOV")==0,"zoomed FOV used as default");return fov;}
    Need(object==&params&&std::strcmp(name,"m_fScopeZoom")==0,"wrong cached parameter");const float value=scope_zoom;
    if(++scope_reads==1)switch(mutation){
        case 1:actual[0]=desired[0]=&other_weapon;break;case 2:desired[0]=nullptr;break;
        case 3:module_ref=&replacement;break;case 4:cache_ref=&replacement;break;
        case 5:params_ref=&replacement;break;case 6:manager_ref=&replacement;break;
        case 7:camera_ref=&replacement;break;case 8:scope_zoom=9;break;
        case 9:squint=.5F;break;case 10:dead=true;break;case 11:states[0]=2;break;
        case 12:maximum=3;break;case 13:weapon_id=123;break;case 14:fov=70;break;default:break;}
    return value;}
void* __stdcall String(void*,const char* value){const bool absent=Step("NewStringUTF",true);Need(std::strcmp(value,"WpnLogicRifleWinchesterScope")==0,"wrong native cache key");return Ref(&key,absent);}
template<class T>void Set(int index,T callback){jni[index]=reinterpret_cast<void*>(callback);}
void Reset(){Need(refs==0&&!pending,"previous sample leaked");operations=0;fault_at=0;null_at=0;mutation=0;scope_reads=0;trace.clear();null_operations.clear();nullable_operations.clear();
    actual[0]=desired[0]=&weapon;actual[1]=desired[1]=nullptr;
    module_ref=&module;cache_ref=&cache;params_ref=&params;manager_ref=&manager;camera_ref=&camera;
    weapon_type=&weapon_class;params_type=&params_class;cache_type=&cache_class;
    squint=1;maximum=2;scope_zoom=8;fov=60;weapon_id=0xf2345678U;dead=false;states[0]=states[1]=1;}
CoJFocusZoom Read(bool available=true){CoJFocusZoom result{1,99,99,true,true,true,0xbadc0deU};
    const bool read=ReadCoJFocusZoom(&env,&player,result);
    if(read!=available)std::cerr<<"fault="<<fault_at<<" null="<<null_at<<" mutation="<<mutation<<" operations="<<operations<<"\n";
    Need(read==available,"snapshot availability");
    Need(!pending&&refs==0,"exception/local reference cleanup");
    if(!available)Need(!result.valid&&!result.scoped&&!result.scope_visible&&result.factor==0&&result.maximum==1&&result.magnification==1&&result.scoped_weapon==0,"stale zoom/mask survived rejection");
    return result;}
bool VisibleState(int state){
    // Independent enumeration transcribed from IsScopeVisible and the exact
    // EBeingOperateStates helper switches, including alternate bit 0x80.
    switch(state){case 1:case 3:case 96:case 97:case 98:case 129:case 131:case 224:case 225:case 226:
        case 158:case 159:case 161:case 162:case 164:case 165:case 168:case 169:case 171:case 172:case 174:case 175:
        case 178:case 179:case 181:case 182:case 184:case 185:case 188:case 189:case 191:case 192:case 194:case 195:
        case 198:case 199:case 201:case 202:case 204:case 205:case 208:case 209:case 211:case 212:case 214:case 215:return true;
        default:return false;}}
void Run(){
    std::size_t dump_size=0;getenv_s(&dump_size,nullptr,0,"COJVR_SCOPE_ORACLE_DUMP");const bool dump_native=dump_size>0;
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(31,Class);Set(32,Instance);Set(33,Lookup);
    Set(36,Call);Set(51,IntCall);Set(94,Lookup);Set(95,Object);Set(96,Boolean);Set(102,Float);Set(167,String);
    Reset();auto sample=Read();
    // 8*tan(30 degrees)/tan(40 degrees), NOT the authored parameter 8.
    Need(sample.valid&&sample.scoped&&sample.scope_visible&&sample.scoped_weapon==0xf2345678U&&std::abs(sample.magnification-5.504474F)<2e-5F,"normalized native scope zoom/mask");
    const int count=operations;const auto natural_nulls=null_operations;const auto nullable_sites=nullable_operations;
    for(int op=1;op<=count;++op){Reset();fault_at=op;Read(false);}
    std::cout<<"scope exception sweep: "<<count<<" operations\n";
    int null_count=0;
    for(int op:nullable_sites){
        if(std::find(natural_nulls.begin(),natural_nulls.end(),op)!=natural_nulls.end())continue;
        Reset();null_at=op;Read(false);++null_count;
    }
    std::cout<<"scope null-result sweep: "<<null_count<<" operations\n";
    for(auto id:{1U,0x7fffffffU,0x80000000U,0xfffffffeU}){Reset();weapon_id=id;Need(Read().scoped_weapon==id,"native identity bits lost");}
    for(auto id:{0U,0xffffffffU}){Reset();weapon_id=id;Read(false);}
    for(int index:{24,36,51,95,96,167}){Reset();auto saved=jni[index];jni[index]=nullptr;Read(false);jni[index]=saved;}
    for(int mode=1;mode<=14;++mode){Reset();mutation=mode;Read(false);}
    for(auto field:{&module_ref,&cache_ref,&params_ref,&manager_ref,&camera_ref}){Reset();*field=nullptr;Read(false);}
    for(auto field:{&weapon_type,&params_type,&cache_type}){Reset();*field=&subclass;Read(false);}
    for(float v:{-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Reset();scope_zoom=v;Read(false);}
    for(float v:{0.F,-1.F,180.F,181.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){Reset();fov=v;Read(false);}
    Reset();scope_zoom=.5F;Read(false); // Native result below unity cannot describe scene magnification.
    for(int hand=0;hand<2;++hand){Reset();actual[0]=desired[0]=nullptr;actual[hand]=desired[hand]=&weapon;Read();
        desired[hand]=&weapon_alias;Read();actual[1-hand]=desired[1-hand]=&weapon_alias;Read();
        actual[1-hand]=desired[1-hand]=&other_weapon;Read(false);Reset();actual[1]=desired[1]=&bow;Read(false);
        Reset();actual[hand]=nullptr;desired[hand]=&weapon;Read(false);Reset();desired[hand]=nullptr;actual[hand]=&weapon;Read(false);}
    struct ZoomCase{float parameter,default_fov,want;};
    for(auto c:{ZoomCase{8,80,8},ZoomCase{8,90,9.534029F},ZoomCase{4,60,2.752237F},ZoomCase{3,100,4.26083F}}){
        Reset();scope_zoom=c.parameter;fov=c.default_fov;auto result=Read();Need(result.scope_visible&&std::abs(result.magnification-c.want)<3e-5F,"parameter/FOV normalization");}
    // No scope interpolation: exact native switch at full squint, ordinary cache on the ramp and decay.
    for(float v:{0.F,.1F,.5F,.9F,.999F,1.F,.999F,.5F,0.F}){Reset();squint=v;auto result=Read();
        Need(result.scoped&&result.scope_visible==(v==1)&&std::abs(result.magnification-(v==1?5.504474F:1+v))<2e-5F,"native entry/decay threshold");}
    for(int state=-1;state<=511;++state){Reset();states[0]=state;auto result=Read();Need(result.scope_visible==VisibleState(state),"native state predicate");
        Need(std::abs(result.magnification-(VisibleState(state)?5.504474F:2.F))<2e-5F,"ineligible state must keep ordinary squint");
        if(dump_native)std::cout<<"NATIVE_SCOPE_STATE "<<state<<" "<<result.scope_visible<<" "<<result.magnification<<"\n";}
    for(int state:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max(),-129,65537}){Reset();states[0]=state;Need(!Read().scope_visible,"unknown state admitted");}
    Reset();actual[1]=desired[1]=&weapon_alias;states[0]=2;states[1]=1;Need(!Read().scope_visible,"second eligible alias overrode first ineligible hand");
    states[0]=1;states[1]=2;Need(Read().scope_visible,"second ineligible alias overrode first eligible hand");
    Reset();actual[0]=desired[0]=nullptr;actual[1]=desired[1]=&weapon;states[1]=2;Need(!Read().scope_visible,"empty first hand hid second native state");
    Reset();dead=true;Need(!Read().scope_visible,"dead player scope visible");
    Reset();scope_zoom=0;sample=Read();Need(sample.scoped&&!sample.scope_visible&&sample.magnification==2,"disabled native scope must keep squint");
    Reset();actual[0]=desired[0]=nullptr;sample=Read();Need(!sample.scoped&&!sample.scope_visible&&sample.scoped_weapon==0&&sample.magnification==2,"ordinary owner retained mask");
    std::array<cojvr::runtime::EyeView,2> optical{};
    for(int eye=0;eye<2;++eye){auto& e=optical[eye];e.eye=eye?cojvr::runtime::Eye::right:cojvr::runtime::Eye::left;
        e.fov=eye?cojvr::runtime::EyeFov{-.67F,.94F,.79F,-.59F}:cojvr::runtime::EyeFov{-.91F,.72F,.84F,-.63F};
        e.eye_to_head.position={eye?.032F:-.032F,.002F,-.003F};e.eye_to_head.orientation={0,eye?.02F:-.02F,0,.9998F};
        e.eye_to_head.position_valid=e.eye_to_head.orientation_valid=true;e.width=1440;e.height=1600;}
    for(float parameter:{3.F,4.F,8.F,12.F}){Reset();scope_zoom=parameter;sample=Read();auto render=optical;
        Need(BuildCoJFocusRenderEyes(optical,sample,render),"scope snapshot not consumed by both eyes");
        for(int eye=0;eye<2;++eye){auto pose=render[eye];pose.fov=optical[eye].fov;
            Need(std::abs(pose.eye_to_head.position.x-optical[eye].eye_to_head.position.x/sample.magnification)<1e-7F,
                "scope virtual baseline does not compensate magnification");
            pose.eye_to_head.position=optical[eye].eye_to_head.position;
            Need(pose.eye==optical[eye].eye&&pose.width==1440&&pose.height==1600&&
                std::memcmp(&pose.eye_to_head,&optical[eye].eye_to_head,sizeof(pose.eye_to_head))==0,"changed stereo pose/IPD/cant");
            std::array<float,16> before{},after{};
            Need(cojvr::runtime::BuildReferenceProjectionMatrix(optical[eye].fov,.1F,1000,before)&&
                cojvr::runtime::BuildReferenceProjectionMatrix(render[eye].fov,.1F,1000,after),"complete projection unavailable");
            for(int n=0;n<16;++n){const float want=(n==0||n==5)?before[n]*sample.magnification:before[n];
                Need(std::abs(after[n]-want)<2e-5F,"complete asymmetric matrix changed principal point/depth");}
        }
        params_ref=nullptr;sample=Read(false);Need(!BuildCoJFocusRenderEyes(optical,sample,render),"invalid cache reused scope projection");
        for(int eye=0;eye<2;++eye)Need(std::memcmp(&render[eye].fov,&optical[eye].fov,sizeof(render[eye].fov))==0,"failed cache retained narrowed optics");
    }
    std::cout<<"scope cache/identity/fault/ramp/state/stereo matrix tests passed\n";
}
}

int main(){
    scoped_tests::Run();
    Set(31,Class);Set(94,Lookup);Set(102,Float);Set(15,Exception);Set(17,Clear);Set(23,Delete);
    Set(6,Find);Set(24,Same);Set(32,Instance);Set(33,Lookup);Set(36,Call);Set(95,Object);
    CoJFocusZoom out{};Require(ReadCoJFocusZoom(&holder,&player,out),"native focus unavailable");
    Require(out.valid&&out.factor==.5F&&out.maximum==2&&out.magnification==1.5F,"native interpolation");
    Require(!out.scoped&&!out.scope_visible&&out.scoped_weapon==0,"ordinary focus retained scope mask/owner");
    // Original bytecode: half concentration plus 1/4 squint gives 5/8,
    // hence 2.25x with the native cached maximum of 3. No logical Focus
    // request and no weapon parameter factory can supply this result.
    factor=.25F;maximum=3;current_slomo=.6F;active[0]=desired[0]=&bow_weapon;
    Require(ReadCoJFocusZoom(&holder,&player,out)&&std::abs(out.factor-.625F)<1e-6F&&
        std::abs(out.magnification-2.25F)<1e-6F,"bow concentration must combine with cached squint");
    Require(!out.scoped&&!out.scope_visible&&out.scoped_weapon==0,"bow retained scope mask/owner");
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
    // Native-sized zoom must preserve *displayed* binocular disparity, not
    // multiply it by the magnification while retaining hardware eye offsets.
    for(float magnification:{1.F,1.25F,2.F,4.F,8.F,12.F}){
        CoJFocusZoom z{1,magnification,magnification,true};
        Require(BuildCoJFocusRenderEyes(optical,z,render),"binocular zoom unavailable");
        for(float distance:{.35F,1.F,5.F,50.F,500.F}){
            float displayed[2]{};
            for(unsigned eye=0;eye<2;++eye){
                const auto& o=optical[eye];const auto& r=render[eye];
                const float tangent=(.07F-r.eye_to_head.position.x)/distance;
                const float l=std::tan(r.fov.angle_left),rr=std::tan(r.fov.angle_right);
                const float u=(tangent-l)/(rr-l);
                displayed[eye]=std::tan(o.fov.angle_left)+u*(std::tan(o.fov.angle_right)-std::tan(o.fov.angle_left));
                const float cx=(optical[0].eye_to_head.position.x+optical[1].eye_to_head.position.x)*.5F;
                Require(std::abs(r.eye_to_head.position.x-(cx+(o.eye_to_head.position.x-cx)/magnification))<1e-7F,
                    "zoom retained full baseline and exaggerated binocular disparity");
            }
            Require(std::abs((displayed[0]-displayed[1])-
                (optical[1].eye_to_head.position.x-optical[0].eye_to_head.position.x)/distance)<2e-6F,
                "displayed zoom disparity differs from physical reference");
        }
    }
    Require(BuildCoJFocusRenderEyes(optical,bow_zoom,render),"read bow zoom must reach stereo render eyes");
    for(int i=0;i<2;++i){const auto& a=render[i];const auto& b=optical[i];
        Require(std::abs(std::tan(a.fov.angle_left)*2.25F-std::tan(b.fov.angle_left))<1e-5F&&
            std::abs(std::tan(a.fov.angle_right)*2.25F-std::tan(b.fov.angle_right))<1e-5F&&
            std::abs(std::tan(a.fov.angle_up)*2.25F-std::tan(b.fov.angle_up))<1e-5F&&
            std::abs(std::tan(a.fov.angle_down)*2.25F-std::tan(b.fov.angle_down))<1e-5F,"bow asymmetric stereo rays");
        Require(a.eye==b.eye&&a.width==b.width&&a.height==b.height&&
            std::abs(a.eye_to_head.position.x-b.eye_to_head.position.x/2.25F)<1e-7F&&a.eye_to_head.position.y==b.eye_to_head.position.y&&a.eye_to_head.position.z==b.eye_to_head.position.z&&
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
    for(int i=0;i<2;++i){Require(std::abs(std::tan(render[i].fov.angle_left)*2-std::tan(optical[i].fov.angle_left))<1e-5F,"asymmetric tangent lost");Require(std::abs(std::tan(render[i].fov.angle_right)*2-std::tan(optical[i].fov.angle_right))<1e-5F,"right tangent lost");Require(std::abs(std::tan(render[i].fov.angle_up)*2-std::tan(optical[i].fov.angle_up))<1e-5F&&std::abs(std::tan(render[i].fov.angle_down)*2-std::tan(optical[i].fov.angle_down))<1e-5F,"vertical tangent lost");Require(std::abs(render[i].eye_to_head.position.x-optical[i].eye_to_head.position.x/2.F)<1e-7F&&render[i].eye==optical[i].eye&&render[i].width==1440&&render[i].height==1600,"eye identity/pose/extent changed");}
    Require(optical[0].fov.angle_left==-.9F,"runtime optics modified");
    out={};Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unavailable focus fallback");
    out={0,2,1,true};Require(BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"unity not exact");
    for(float scale:{.99F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        out.magnification=scale;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"invalid scale accepted");}
    out={1,2,2,true};optical[1].fov.angle_up=1.6F;Require(!BuildCoJFocusRenderEyes(optical,out,render)&&render[0].fov.angle_left==optical[0].fov.angle_left,"partial stereo zoom after bad optics");
}
