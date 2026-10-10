#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fixture {
struct O {
    std::string type;
    std::unordered_map<std::string,O*> objects;
    std::unordered_map<std::string,int> ints;
    std::unordered_map<std::string,float> floats;
    std::vector<O*> items;
    std::vector<unsigned char> booleans;
    std::vector<int> integers;
    std::vector<float> decimals;
    std::u16string text;
    bool visible=true;
    int refs=0;
};
O hud_class,exception,player,other,hud,components,slots,objects,pistol,hands,slot0,slot1;
O poses,standing,crouched;
O compass,waypoints_manager,waypoints,wp,wp2,rotor,rotors,rotor0,rotor1,sprite,sprite2;
O pos,forward,target,target2,label,label2,carried;
O left;
O health,health_text,ammo,totals,big,small,weapons,counters,infos,info,loaded,reserve;
O big_icon,small_icon;
O concentration,concentration_icon,countdown,countdown_window,countdown_timer,countdown_bottom,countdown_center;
O horse,horse_icon,horse_fatigue;
O countdown_title,damage,direction,damage_array,direction_array,hit,miss,hit_icon,miss_icon,hit_position,miss_position;
O objective,objective_window,objective_info,tip_objectives,tip_logs,tip_text,logs_text;
O cross,warning,starts,ends,start,end,can_fire,reasons,ages,collisions,collision;
std::vector<O*> all{&hud_class,&exception,&player,&other,&hud,&components,&slots,&objects,&pistol,&hands,
    &slot0,&slot1,&compass,&waypoints_manager,&waypoints,&wp,&wp2,&rotor,&rotors,&rotor0,&rotor1,
    &sprite,&sprite2,&pos,&forward,&left,&target,&target2,&label,&label2,&carried};
void* table[224]{};void** holder=table;bool pending=false;int pins=0,critical=0;std::string fail;
O* fail_owner=nullptr;
std::unordered_map<std::string,std::string> tokens;
std::unordered_map<std::string,O> classes;
void Require(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
void* Local(O* o){if(o)++o->refs;return o;}
void Clean(){Require(!pending&&pins==0&&critical==0,"exception/string/primitive leak");for(auto o:all)Require(o->refs==0,"JNI reference leak");for(auto& [n,o]:classes)Require(o.refs==0,"class reference leak");}
void* __stdcall Find(void*,const char* n){if(std::strcmp(n,"HUDManager")==0)return Local(&hud_class);auto& c=classes[n];c.type=n;return Local(&c);}
unsigned char __stdcall Instance(void*,void* o,void* c){auto& type=static_cast<O*>(o)->type;auto& wanted=static_cast<O*>(c)->type;return type==wanted||(wanted=="Weapon"&&type.starts_with("Weapon"))||(wanted=="WeaponFire"&&type=="WeaponPistolPeacemaker")||(wanted=="HUDDamageIndicator"&&type=="HUDDirectionIndicator");}
void* __stdcall Exception(void*){Require(critical==0,"JNI while primitive pinned");return pending?Local(&exception):nullptr;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void* v){auto o=static_cast<O*>(v);Require(o&&o->refs>0,"invalid delete");--o->refs;}
unsigned char __stdcall Same(void*,void* a,void* b){return a==b;}
void* __stdcall Class(void*,void* o){return Local(static_cast<O*>(o));}
void* __stdcall Lookup(void*,void* owner,const char* n,const char* sig){
    Require(!pending,"JNI after exception");
    const auto& type=static_cast<O*>(owner)->type;
    if(type=="DamageIndicator"||type=="HUDDirectionIndicator"||type=="HUDDamageIndicator"||
        type=="HUDObjective"||type=="HUDTipObjectives"||type=="HUDTipLogs"){
        std::string expected;
        if(std::strcmp(n,"m_Being")==0)expected="LBeing;";
        else if(std::strcmp(n,"m_cHUDManager")==0)expected="LHUDManager;";
        else if(std::strcmp(n,"IsActuallyVisible")==0)expected="()Z";
        else if(std::strcmp(n,"m_aIndicators")==0)expected="[LDamageIndicator;";
        else if(std::strcmp(n,"m_bActive")==0)expected="Z";
        else if(std::strcmp(n,"m_fDamageAngle")==0)expected="F";
        else if(std::strcmp(n,"m_cIcon")==0||std::strcmp(n,"m_cPositioner")==0)expected="LUIWindow;";
        else if(std::strcmp(n,"m_cMainWindow")==0)expected="LUIWindowInfo;";
        else if(std::strcmp(n,"m_cInfo")==0||std::strcmp(n,"m_cText")==0)expected="LUIStatic;";
        Require(!expected.empty()&&expected==sig,"unproven critical HUD signature requested");
    }
    if(type=="HUDBulletTime"||type=="HUDCountdownTimer"||type=="HUDHorse"){
        std::string expected;
        if(std::strcmp(n,"m_Being")==0)expected="LBeing;";
        else if(std::strcmp(n,"m_cHUDManager")==0)expected="LHUDManager;";
        else if(std::strcmp(n,"IsActuallyVisible")==0)expected="()Z";
        else if(type=="HUDBulletTime"){
            if(std::strcmp(n,"m_cIcon")==0)expected="LUIStatic;";
            if(std::strcmp(n,"m_cPlayer")==0)expected="LArmedPlayerBeing;";
            if(std::strcmp(n,"m_nMode")==0)expected="I";
        }else if(type=="HUDCountdownTimer"){
            if(std::strcmp(n,"m_cWindow")==0)expected="LUIWindow;";
            if(std::strcmp(n,"m_cCountdownTimer")==0)expected="LCountdownTimer;";
            if(std::strcmp(n,"m_cBottomText")==0||std::strcmp(n,"m_cCenterText")==0||std::strcmp(n,"m_cTitle")==0)expected="LUIStatic;";
        }else{
            if(std::strcmp(n,"m_cHorseIcon")==0||std::strcmp(n,"m_cTirednessIconFill")==0)expected="LUIWindow;";
            if(std::strcmp(n,"m_fLastTiredness")==0)expected="F";
            if(std::strcmp(n,"m_nHealthLevel")==0)expected="I";
            if(std::strcmp(n,"m_bUpdateHealthLevel")==0)expected="Z";
        }
        Require(!expected.empty()&&expected==sig,"unproven special HUD field/method signature requested");
    }
    if(fail==n&&(!fail_owner||fail_owner==owner)){pending=true;return nullptr;}
    return &tokens.emplace(n,n).first->second;
}
std::string Name(void* id){return *static_cast<std::string*>(id);}
void* __stdcall ObjectField(void*,void* o,void* id){Require(!pending,"pending object read");return Local(static_cast<O*>(o)->objects[Name(id)]);}
int __stdcall IntField(void*,void* o,void* id){return static_cast<O*>(o)->ints[Name(id)];}
signed char __stdcall ByteField(void* e,void* o,void* id){return static_cast<signed char>(IntField(e,o,id));}
unsigned char __stdcall BoolField(void* e,void* o,void* id){return IntField(e,o,id)!=0;}
float __stdcall FloatField(void*,void* o,void* id){return static_cast<O*>(o)->floats[Name(id)];}
union V{long long align;void* object;int integer;};
unsigned char __stdcall CallBool(void*,void* o,void* id,const V*){Require(Name(id)=="IsActuallyVisible","game mutation method requested");return static_cast<O*>(o)->visible;}
int __stdcall CallInt(void*,void* o,void* id,const V*){Require(Name(id)=="size","non-read-only int call");return static_cast<int>(static_cast<O*>(o)->items.size());}
void* __stdcall CallObject(void*,void* o,void* id,const V* args){auto a=static_cast<O*>(o);if(Name(id)=="GetActualWeaponNotEmpty"){Require(args[0].integer==0||args[0].integer==1,"invalid hand lookup");return Local(a->objects[args[0].integer==0?"rightWeapon":"leftWeapon"]);}Require(Name(id)=="get"||Name(id)=="elementAt","gameplay method requested");Require(args[0].integer>=0&&static_cast<std::size_t>(args[0].integer)<a->items.size(),"unchecked collection index");return Local(a->items[args[0].integer]);}
int __stdcall Length(void*,void* o){auto a=static_cast<O*>(o);if(!a->booleans.empty())return static_cast<int>(a->booleans.size());if(!a->integers.empty())return static_cast<int>(a->integers.size());if(!a->decimals.empty())return static_cast<int>(a->decimals.size());return static_cast<int>(a->items.size());}
void* __stdcall Element(void*,void* o,int i){auto a=static_cast<O*>(o);Require(i>=0&&static_cast<std::size_t>(i)<a->items.size(),"unchecked array index");return Local(a->items[i]);}
int __stdcall StringLength(void*,void* o){return static_cast<int>(static_cast<O*>(o)->text.size());}
const char16_t* __stdcall Chars(void*,void* o,unsigned char*){++pins;return static_cast<O*>(o)->text.data();}
void __stdcall Release(void*,void*,const char16_t*){--pins;}
void* __stdcall Critical(void*,void* o,unsigned char*){Require(critical==0,"nested primitive borrow");if(fail=="critical"){pending=true;return nullptr;}++critical;auto a=static_cast<O*>(o);if(!a->booleans.empty())return a->booleans.data();if(!a->integers.empty())return a->integers.data();return a->decimals.data();}
void __stdcall Unpin(void*,void*,void*,int mode){Require(critical==1&&mode==2,"read-only primitive release required");--critical;}
template<class T>void Set(int i,T f){table[i]=reinterpret_cast<void*>(f);}
void Init(){
    for(auto o:{&concentration,&concentration_icon,&countdown,&countdown_window,&countdown_timer,
        &countdown_bottom,&countdown_center,&horse,&horse_icon,&horse_fatigue})all.push_back(o);
    for(auto o:{&countdown_title,&damage,&direction,&damage_array,&direction_array,&hit,&miss,&hit_icon,&miss_icon,
        &hit_position,&miss_position,&objective,&objective_window,&objective_info,&tip_objectives,&tip_logs,&tip_text,&logs_text})all.push_back(o);
    countdown.objects["m_cTitle"]=&countdown_title;countdown_title.type="UIStatic";
    countdown_title.objects["m_sLocalizedText"]=&countdown_title;countdown_title.text=u"Time left";
    countdown_title.floats["m_fCurTextAlpha"]=1;
    damage.type="HUDDamageIndicator";direction.type="HUDDirectionIndicator";
    objective.type="HUDObjective";tip_objectives.type="HUDTipObjectives";tip_logs.type="HUDTipLogs";
    for(auto o:{&damage,&direction,&objective,&tip_objectives,&tip_logs}){
        o->objects["m_Being"]=&player;o->objects["m_cHUDManager"]=&hud;
    }
    damage.objects["m_aIndicators"]=&damage_array;direction.objects["m_aIndicators"]=&direction_array;
    damage_array.items={&hit};direction_array.items={&miss};hit.type=miss.type="DamageIndicator";
    hit.objects["m_cIcon"]=&hit_icon;hit.objects["m_cPositioner"]=&hit_position;
    miss.objects["m_cIcon"]=&miss_icon;miss.objects["m_cPositioner"]=&miss_position;
    for(auto o:{&hit_icon,&miss_icon,&hit_position,&miss_position}){o->type="UIWindow";o->floats["m_fTextureAlpha"]=1;}
    hit.ints["m_bActive"]=miss.ints["m_bActive"]=1;
    hit.floats["m_fDamageAngle"]=90;miss.floats["m_fDamageAngle"]=0;
    hit_icon.floats["m_fTextureAlpha"]=.5F;
    objective.objects["m_cMainWindow"]=&objective_window;objective_window.type="UIWindowInfo";
    objective.objects["m_cInfo"]=&objective_info;
    tip_objectives.objects["m_cText"]=&tip_text;tip_logs.objects["m_cText"]=&logs_text;
    for(auto o:{&objective_info,&tip_text,&logs_text}){
        o->type="UIStatic";o->objects["m_sLocalizedText"]=o;o->floats["m_fCurTextAlpha"]=1;
    }
    objective_info.text=u"Escape before time runs out";tip_text.text=u"Objectives updated";logs_text.text=u"New journal entry";
    concentration.type="HUDBulletTime";countdown.type="HUDCountdownTimer";horse.type="HUDHorse";
    for(auto o:{&concentration,&countdown,&horse}){o->objects["m_Being"]=&player;o->objects["m_cHUDManager"]=&hud;}
    concentration.objects["m_cPlayer"]=&player;concentration.objects["m_cIcon"]=&concentration_icon;
    concentration_icon.type="UIStatic";concentration_icon.floats["m_fTextureAlpha"]=1;
    concentration.ints["m_nMode"]=4;
    countdown.objects["m_cWindow"]=&countdown_window;countdown_window.type="UIWindow";
    countdown.objects["m_cCountdownTimer"]=&countdown_timer;countdown_timer.type="CountdownTimer";
    countdown.objects["m_cBottomText"]=&countdown_bottom;countdown.objects["m_cCenterText"]=&countdown_center;
    for(auto o:{&countdown_bottom,&countdown_center}){o->type="UIStatic";o->objects["m_sLocalizedText"]=o;o->floats["m_fCurTextAlpha"]=1;}
    countdown_bottom.text=u"5";
    horse.objects["m_cHorseIcon"]=&horse_icon;horse.objects["m_cTirednessIconFill"]=&horse_fatigue;
    for(auto o:{&horse_icon,&horse_fatigue}){o->type="UIWindow";o->floats["m_fTextureAlpha"]=1;}
    horse.ints["m_nHealthLevel"]=75;horse.floats["m_fLastTiredness"]=40;
    for(auto o:{&poses,&standing,&crouched})all.push_back(o);
    for(auto o:{&cross,&warning,&starts,&ends,&start,&end,&can_fire,&reasons,&ages,&collisions,&collision})all.push_back(o);
    Set(222,Critical);Set(223,Unpin);
    cross.type="HUDCrosshairHand";warning.type="Sprite";cross.ints["m_iHand"]=0;
    cross.objects["m_Being"]=&player;cross.objects["m_cHUDManager"]=&hud;cross.objects["dontShoot"]=&warning;
    player.objects["rightWeapon"]=&pistol;player.objects["cojvrRightTargetWeapon"]=&pistol;
    starts.items={&start,&start};ends.items={&end,&end};end.floats["fZ"]=1000;
    can_fire.booleans={0,1};reasons.integers={1,3};ages.decimals={.01F,.01F};collisions.items={&collision,&collision};
    player.objects["m_vLCStart"]=&starts;player.objects["m_vLCEnd"]=&ends;player.objects["m_cLastCollision"]=&collisions;
    player.objects["m_abCanFireAtTarget"]=&can_fire;player.objects["m_anCantFireReason"]=&reasons;player.objects["m_afLastAimCheckTime"]=&ages;
    pistol.ints["m_bInHands"]=1;
    for(auto o:{&health,&health_text,&ammo,&totals,&big,&small,&weapons,&counters,&infos,&info,&loaded,&reserve,&big_icon,&small_icon})all.push_back(o);
    Set(6,Find);Set(15,Exception);Set(17,Clear);Set(23,Delete);Set(24,Same);Set(31,Class);Set(32,Instance);
    Set(33,Lookup);Set(39,CallBool);Set(36,CallObject);Set(51,CallInt);
    Set(94,Lookup);Set(95,ObjectField);Set(96,BoolField);Set(97,ByteField);Set(100,IntField);Set(102,FloatField);
    Set(144,Lookup);Set(145,ObjectField);Set(164,StringLength);Set(165,Chars);Set(166,Release);Set(171,Length);Set(173,Element);
    hud_class.objects["sm_cMainHUDManager"]=&hud;hud.objects["m_Being"]=&player;
    slot0.type=slot1.type="InventorySlot";pistol.type="WeaponPistolPeacemaker";hands.type="WeaponHands";
    compass.type="HUDCompass";wp.type=wp2.type="HUDWaypointsManager$HUDWaypoint";rotor0.type=rotor1.type="UIRotorText$RotorTextElement";
    hud.objects["m_aHudComponents"]=&components;components.items.resize(23);components.items[19]=&compass;components.items[3]=&cross;
    components.items[0]=&health;components.items[1]=&ammo;components.items[7]=&weapons;
    health.type="HUDPlayer";ammo.type="HUDAmmoCounters";weapons.type="HUDWeapons";
    for(auto o:{&health,&ammo,&weapons}){o->objects["m_Being"]=&player;o->objects["m_cHUDManager"]=&hud;}
    health.objects["m_cPlayer"]=&player;health.objects["m_cHealth"]=&health_text;
    health_text.type=loaded.type=reserve.type="UIStatic";
    health_text.objects["m_sLocalizedText"]=&health_text;health_text.text=u"087";health_text.floats["m_fCurTextAlpha"]=1;
    ammo.objects["m_cAmmoCountersTotal"]=&totals;totals.items={nullptr,nullptr,&reserve};
    ammo.objects["m_cBulletIconsBig"]=&big;big.items={nullptr,nullptr,&big_icon};
    ammo.objects["m_cBulletIconsSmall"]=&small;small.items={nullptr,nullptr,&small_icon};
    small_icon.visible=false;reserve.objects["m_sLocalizedText"]=&reserve;reserve.text=u"12";reserve.floats["m_fCurTextAlpha"]=1;
    weapons.objects["m_tAmmoCounters"]=&counters;counters.items.resize(6);counters.items[1]=&loaded;
    weapons.objects["m_tSlotsWeaponInfo"]=&infos;infos.items.resize(6);infos.items[1]=&info;
    info.type="HUDWeapons$WeaponInfo";info.ints["m_bActiveAmmo"]=info.ints["m_bShowAmmo"]=1;
    loaded.objects["m_sLocalizedText"]=&loaded;loaded.text=u"3";loaded.floats["m_fCurTextAlpha"]=1;
    player.objects["m_aInvSlots"]=&slots;player.objects["m_aInvSlotsObjects"]=&objects;
    player.ints["m_bWeaponChangeEnabled"]=1;player.ints["m_bPunchingEnabled"]=1;player.ints["m_bWeaponThrowEnabled"]=1;
    slots.items={&slot0,&slot1};objects.items={&pistol,&hands};
    slot0.objects["m_cPawn"]=&player;slot1.objects["m_cPawn"]=&player;
    slot0.ints["m_nIndex"]=0;slot0.ints["m_nType"]=1;slot1.ints["m_nIndex"]=1;slot1.ints["m_nType"]=7;
    pistol.objects["cOwner"]=&player;hands.objects["cOwner"]=&player;
    pistol.ints["m_nSlot"]=1;hands.ints["m_nSlot"]=7;
    compass.objects["m_cWaypointsManager"]=&waypoints_manager;waypoints_manager.objects["m_cWaypoints"]=&waypoints;
    compass.objects["m_Being"]=&player;compass.objects["m_cHUDManager"]=&hud;
    compass.objects["m_cRotorText"]=&rotor;rotor.objects["m_cElements"]=&rotors;
    compass.objects["m_vPlayerPos"]=&pos;compass.objects["m_vPlayerForward"]=&forward;compass.floats["m_fMapAngle"]=123;
    compass.objects["m_vPlayerRight"]=&left;left.floats["fX"]=-1;
    forward.floats["fZ"]=1;pos.floats["fX"]=100;
    waypoints.items={&wp,&wp2};rotors.items={&rotor0,&rotor1};
    wp.objects["m_sName"]=&label;wp.objects["m_vPos"]=&target;wp.ints["m_bActive"]=1;wp.ints["m_nRotorIdx"]=0;
    wp2.objects["m_sName"]=&label2;wp2.objects["m_vPos"]=&target2;wp2.ints["m_bActive"]=1;wp2.ints["m_nRotorIdx"]=1;
    rotor0.objects["m_cElement"]=&sprite;rotor1.objects["m_cElement"]=&sprite2;
    sprite.objects["m_sLocalizedText"]=&label;sprite2.objects["m_sLocalizedText"]=&label2;
    rotor0.floats["m_fAngle"]=90;rotor1.floats["m_fAngle"]=180;
    rotor0.ints["m_nIndex"]=0;rotor1.ints["m_nIndex"]=1;
    target.floats["fZ"]=1000;target2.floats["fX"]=2000;label.text=u"Objetivo — niño";label2.text=u"Salida";
}
void SpecialStatusChecks(){
    using namespace cojvr::games::call_of_juarez;CoJGameplayUiSnapshot out{};
    auto read=[&](){Require(ReadCoJGameplayUi(&holder,&player,out)&&out.inventory.valid&&out.compass_valid,
        "optional native HUD failure escaped its presentation domain");Clean();};
    auto absent=[&](){read();Require(out.status.line_count==3&&out.status.lines[0].view()==u"Salud  087",
        "hidden/invalid optional HUD must preserve health/ammo only");};
    components.items[10]=&concentration;
    struct Mode{int mode;std::u16string_view text;};
    for(auto sample:{Mode{1,u"Concentración · En uso"},Mode{2,u"Concentración · Recargando"},
        Mode{3,u"Concentración · Lista"},Mode{4,u"Concentración · Lista"},Mode{5,u"Concentración · No preparada"}}){
        concentration.ints["m_nMode"]=sample.mode;read();
        Require(out.status.line_count==4&&out.status.lines[3].view()==sample.text,"native concentration mode cache lost or conflated");
    }
    for(int mode:{-1,0,6}){concentration.ints["m_nMode"]=mode;absent();}concentration.ints["m_nMode"]=4;
    concentration.objects["m_cPlayer"]=&other;absent();concentration.objects["m_cPlayer"]=&player;
    concentration_icon.type="HUDHint";absent();concentration_icon.type="UIStatic";
    concentration_icon.visible=false;absent();concentration_icon.visible=true;
    for(float alpha:{0.F,-.1F,1.1F,std::numeric_limits<float>::quiet_NaN()}){
        concentration_icon.floats["m_fTextureAlpha"]=alpha;absent();
    }concentration_icon.floats["m_fTextureAlpha"]=1;
    // HUDPlayer also has m_cPlayer; deny only the optional concentration owner.
    fail_owner=&concentration;
    for(const char* field:{"m_cIcon","m_cPlayer","m_nMode"}){fail=field;absent();}fail.clear();fail_owner=nullptr;
    components.items[10]=nullptr;components.items[17]=&countdown;
    read();Require(out.mission_timer.view()==u"Time left\n5","mission timer must copy native title and number outside wrist status");
    components.items[15]=&direction;components.items[16]=&damage;
    components.items[12]=&objective;components.items[13]=&tip_objectives;components.items[14]=&tip_logs;
    read();Require(out.threat_count==2&&!out.threats[0].damage&&out.threats[1].damage&&out.threats[1].alpha==.5F,
        "native direction and damage must retain separate colours and natural fade alpha");
    Require(out.mission_notices.view()==u"Escape before time runs out\nObjectives updated\nNew journal entry",
        "naturally displayed objective/journal notices were lost");
    hit.ints["m_bActive"]=0;miss_icon.visible=false;objective_info.visible=false;tip_text.visible=false;logs_text.visible=false;
    read();Require(out.threat_count==0&&out.mission_notices.view().empty(),"hidden or inactive native notices survived a fresh sample");
    hit.ints["m_bActive"]=1;miss_icon.visible=true;objective_info.visible=tip_text.visible=logs_text.visible=true;
    damage.objects["m_Being"]=&other;read();Require(out.threat_count==1&&!out.threats[0].damage,"foreign damage owner exposed a signal");
    damage.objects["m_Being"]=&player;
    for(float a:{-1.F,0.F,1.1F,std::numeric_limits<float>::quiet_NaN()}){
        hit_icon.floats["m_fTextureAlpha"]=a;read();Require(out.threat_count==1,"invalid alpha survived or suppressed another native domain");
    }hit_icon.floats["m_fTextureAlpha"]=.5F;
    fail="m_cInfo";fail_owner=&objective;read();Require(out.mission_notices.view()==u"Objectives updated\nNew journal entry",
        "one notice lookup error discarded independent notices");fail.clear();fail_owner=nullptr;
    objective.objects["m_cHUDManager"]=&other;read();Require(out.mission_notices.view()==u"Objectives updated\nNew journal entry",
        "foreign objective manager exposed a mission notice");objective.objects["m_cHUDManager"]=&hud;
    hud.visible=false;Require(ReadCoJGameplayUi(&holder,&player,out),"hidden native HUD read failed");Clean();
    Require(out.mission_timer.view().empty()&&out.mission_notices.view().empty()&&out.threat_count==0,
        "hidden HUD retained critical alerts");hud.visible=true;
    damage_array.items.assign(7,&hit);read();Require(out.threat_count==1&&!out.threats[0].damage,
        "unknown native indicator layout exposed threats or suppressed independent direction");damage_array.items={&hit};
    hit.floats["m_fDamageAngle"]=std::numeric_limits<float>::quiet_NaN();read();
    Require(out.threat_count==1&&!out.threats[0].damage,"nonfinite native angle exposed an invalid damage marker");hit.floats["m_fDamageAngle"]=90;
    objective_info.text=std::u16string(1400,u'x');objective_info.text[1021]=0xD83D;objective_info.text[1022]=0xDE00;
    read();Require(out.mission_notices.length<out.mission_notices.capacity&&out.mission_notices.view().back()==u'\u2026',
        "bounded native notification text did not preserve truncation/surrogate boundary");objective_info.text=u"Escape before time runs out";
    countdown_title.text=std::u16string(1400,u'x');countdown_bottom.text=u"51";
    read();Require(out.mission_timer.view().ends_with(u"\n51"),"long title consumed authoritative native countdown digits");
    countdown_title.text=u"Time left";countdown_bottom.text=u"5";
    damage.type="HUDDirectionIndicator";read();Require(out.threat_count==1&&!out.threats[0].damage,
        "direction subclass in damage slot was misclassified as a red hit");damage.type="HUDDamageIndicator";
    for(int index:{12,13,14,15,16})components.items[index]=nullptr;
    read();Require(out.status.line_count==4&&out.status.lines[3].view()==u"Cuenta atrás  5",
        "native countdown number must be copied without recalculating time or claiming draw permission");
    read();Require(out.status.lines[3].view()==u"Cuenta atrás  5","reader advanced the native countdown");
    countdown_bottom.text.clear();countdown_center.text=u"1";read();
    Require(out.status.lines[3].view()==u"Cuenta atrás  1","duel center countdown did not retain native cached integer");
    countdown_bottom.text=u"5";absent();countdown_center.text.clear();
    for(auto text:{u"",u"-1",u"3s",u"12345678901"}){countdown_bottom.text=text;absent();}countdown_bottom.text=u"5";
    countdown.objects["m_cCountdownTimer"]=nullptr;absent();countdown.objects["m_cCountdownTimer"]=&countdown_timer;
    countdown_timer.type="Duel";absent();countdown_timer.type="CountdownTimer";
    for(auto o:{&countdown_window,&countdown_bottom}){o->visible=false;absent();o->visible=true;}
    countdown_bottom.floats["m_fCurTextAlpha"]=0;absent();countdown_bottom.floats["m_fCurTextAlpha"]=1;
    countdown_bottom.type="UIWindow";absent();countdown_bottom.type="UIStatic";
    for(const char* field:{"m_cWindow","m_cCountdownTimer","m_cBottomText","m_cCenterText"}){fail=field;absent();}fail.clear();
    fail="m_sLocalizedText";fail_owner=&countdown_bottom;absent();fail.clear();fail_owner=nullptr;
    components.items[17]=nullptr;components.items[18]=&horse;
    read();Require(out.status.line_count==4&&out.status.lines[3].view()==u"Caballo · Salud 75% · Fatiga 40%",
        "native horse condition caches must retain health and tiredness meanings");
    for(auto sample:{std::pair{0,0.F},std::pair{100,100.F}}){
        horse.ints["m_nHealthLevel"]=sample.first;horse.floats["m_fLastTiredness"]=sample.second;read();
        Require(out.status.lines[3].view()==(sample.first==0?u"Caballo · Salud 0% · Fatiga 0%":u"Caballo · Salud 100% · Fatiga 100%"),
            "horse cache percentage endpoints lost");
    }horse.ints["m_nHealthLevel"]=75;horse.floats["m_fLastTiredness"]=40;
    for(float fatigue:{-1.F,100.1F,std::numeric_limits<float>::quiet_NaN()}){horse.floats["m_fLastTiredness"]=fatigue;absent();}
    horse.floats["m_fLastTiredness"]=40;
    for(int health_level:{-1,101}){horse.ints["m_nHealthLevel"]=health_level;absent();}horse.ints["m_nHealthLevel"]=75;
    horse.ints["m_bUpdateHealthLevel"]=1;absent();horse.ints["m_bUpdateHealthLevel"]=0;
    for(auto o:{&horse_icon,&horse_fatigue}){
        o->visible=false;absent();o->visible=true;
        o->type="HUDHint";absent();o->type="UIWindow";
        o->floats["m_fTextureAlpha"]=0;absent();o->floats["m_fTextureAlpha"]=1;
    }
    for(const char* field:{"m_cHorseIcon","m_cTirednessIconFill","m_nHealthLevel","m_bUpdateHealthLevel","m_fLastTiredness"}){fail=field;absent();}fail.clear();
    for(auto o:{&concentration,&countdown,&horse}){
        const int index=o==&concentration?10:o==&countdown?17:18;components.items[18]=nullptr;components.items[index]=o;
        o->visible=false;absent();o->visible=true;
        auto type=o->type;o->type="HUDHint";absent();o->type=type;
        o->objects["m_Being"]=&other;absent();o->objects["m_Being"]=&player;
        o->objects["m_cHUDManager"]=&other;absent();o->objects["m_cHUDManager"]=&hud;
        for(const char* field:{"m_Being","m_cHUDManager","IsActuallyVisible"}){fail=field;fail_owner=o;absent();}fail.clear();fail_owner=nullptr;
        components.items[index]=nullptr;
    }
    components.items[10]=&concentration;components.items[17]=&countdown;components.items[18]=&horse;
    read();Require(out.status.line_count==6&&out.status.lines[3].view()==u"Cuenta atrás  5"&&
        out.status.lines[4].view()==u"Caballo · Salud 75% · Fatiga 40%"&&out.status.lines[5].view()==u"Concentración · Lista",
        "optional HUD priorities must retain native countdown, horse condition and concentration");
    fail="m_nMode";read();Require(out.status.line_count==5&&out.status.lines[3].view()==u"Cuenta atrás  5",
        "one optional HUD exception discarded another optional domain");fail.clear();
    hud.objects["m_Being"]=&other;Require(ReadCoJGameplayUi(&holder,&player,out)&&!out.status.active,
        "foreign HUD exposed optional status");Clean();hud.objects["m_Being"]=&player;
    const auto saved_counters=counters.items,saved_infos=infos.items,saved_totals=totals.items,saved_big=big.items;
    counters.items.assign(6,&loaded);infos.items.assign(6,&info);totals.items.assign(2,&reserve);totals.items.push_back(nullptr);
    big.items.assign(3,&big_icon);read();
    Require(out.status.line_count==10&&out.status.lines[8].view()==u"Reserva escopeta  12"&&out.status.lines[9].view()==u"Cuenta atrás  5",
        "optional status exceeded ten rows, displaced health/ammo or failed countdown priority");
    totals.items[2]=&reserve;read();Require(out.status.line_count==10&&out.status.lines[9].view()==u"Reserva pistola  12",
        "full native health/ammo card displaced by optional status");
    counters.items=saved_counters;infos.items=saved_infos;totals.items=saved_totals;big.items=saved_big;
    components.items[10]=components.items[17]=components.items[18]=nullptr;
}
}
int main(){using namespace fixture;using namespace cojvr::games::call_of_juarez;Init();CoJGameplayUiSnapshot out{};
    CoJNoShootSnapshot no{};
    Require(ReadCoJNoShoot(&holder,&player,0,no)&&no.valid&&no.warning_visible&&no.hand==0&&no.reason==1&&no.trace_end_cm.z==1000,"owned native protected-target warning missing");Clean();
    warning.visible=false;Require(ReadCoJNoShoot(&holder,&player,0,no)&&!no.warning_visible,"hidden native warning resurrected");Clean();warning.visible=true;
    can_fire.booleans[0]=1;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.warning_visible,"allowed shot mislabelled protected");Clean();can_fire.booleans[0]=0;
    for(int reason:{-1,0,2,4,5}){reasons.integers[0]=reason;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.warning_visible,"unprotected/empty-ammo reason mislabelled");Clean();}reasons.integers[0]=3;
    Require(ReadCoJNoShoot(&holder,&player,0,no)&&no.warning_visible,"protected non-human actor warning lost");Clean();reasons.integers[0]=1;
    for(auto o:{&hud,&cross}){o->objects["m_Being"]=&other;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"foreign HUD owner accepted");Clean();o->objects["m_Being"]=&player;}
    cross.objects["m_cHUDManager"]=&other;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"foreign warning manager accepted");Clean();cross.objects["m_cHUDManager"]=&hud;
    player.objects["cojvrRightTargetWeapon"]=&hands;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"trace from previous weapon accepted");Clean();player.objects["cojvrRightTargetWeapon"]=&pistol;
    pistol.objects["cOwner"]=&other;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"foreign current weapon accepted");Clean();pistol.objects["cOwner"]=&player;
    pistol.ints["m_bInHands"]=0;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"put-away trace accepted");Clean();pistol.ints["m_bInHands"]=1;
    pistol.type="WeaponWhip";ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"whip helper conflated with firearm protection");Clean();pistol.type="WeaponPistolPeacemaker";
    for(float age:{-.1F,.26F,std::numeric_limits<float>::quiet_NaN()}){ages.decimals[0]=age;ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"stale/nonfinite native age accepted");Clean();}ages.decimals[0]=.01F;
    reasons.integers.push_back(0);Require(!ReadCoJNoShoot(&holder,&player,0,no)&&!no.valid,"unknown primitive array layout accepted");Clean();reasons.integers.pop_back();
    fail="critical";Require(!ReadCoJNoShoot(&holder,&player,0,no)&&!no.valid,"failed primitive borrow retained warning");Clean();fail.clear();
    fail="dontShoot";Require(!ReadCoJNoShoot(&holder,&player,0,no)&&!no.valid,"JNI failure retained warning");Clean();fail.clear();
    components.items[4]=&cross;player.objects["leftWeapon"]=&pistol;player.objects["cojvrLeftTargetWeapon"]=&pistol;
    cross.ints["m_iHand"]=1;can_fire.booleans[1]=0;Require(ReadCoJNoShoot(&holder,&player,1,no)&&no.valid&&no.warning_visible&&no.hand==1,"left hand native owner not read independently");Clean();
    ReadCoJNoShoot(&holder,&player,0,no);Require(!no.valid,"wrong hand crosshair accepted");Clean();cross.ints["m_iHand"]=0;
    Require(!ReadCoJNoShoot(nullptr,&player,0,no)&&!no.valid&&!ReadCoJNoShoot(&holder,&player,2,no),"invalid reader entry accepted");
    Require(ReadCoJNoShoot(&holder,&player,0,no)&&no.warning_visible,"reader did not recover after denied/faulted reads");Clean();
    Require(ReadCoJGameplayUi(&holder,&player,out),"native inventory and objective owners must be readable");
    Require(out.inventory.valid&&out.inventory.owned_mask==0xC2&&out.inventory.available_mask==0xC2,"only owned right pistol/hands/discard permitted");
    Require(out.compass_valid&&out.compass_visible&&out.map_angle_degrees==123&&out.waypoint_count==2&&out.waypoints[0].label.view()==label.text,"native multiple waypoint guidance/heading lost");Clean();
    Require(out.status.health_percent==87&&out.status.health_row==0,
        "owned displayed health must supply optional percentage and its actual row");
    for(const auto number:{u"000",u"025",u"100",u"101",u"9999999999"}){
        health_text.text=number;Require(ReadCoJGameplayUi(&holder,&player,out),"numeric native health text must remain readable");Clean();
        const int want=health_text.text==u"000"?0:health_text.text==u"025"?25:health_text.text==u"100"?100:-1;
        Require(out.status.health_percent==want&&out.status.health_row==(want<0?-1:0),
            "health gauge must reject out-of-range values without discarding native numeric text");
    }
    health_text.text=u"087";health_text.visible=false;
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.active&&out.status.health_percent==-1&&
        out.status.health_row==-1&&out.status.lines[0].view()==u"Derecha  3",
        "hidden native health must clear gauge without treating an ammo row as health");Clean();
    health_text.visible=true;Require(ReadCoJGameplayUi(&holder,&player,out),"health recovery read failed");Clean();
    Require(out.status.active&&out.status.line_count==3&&out.status.lines[0].view()==u"Salud  087"&&
        out.status.lines[1].view()==u"Derecha  3"&&out.status.lines[2].view()==u"Reserva pistola  12",
        "native health, loaded slot and inventory reserve must be copied without recalculation");
    health.objects["m_aPoses"]=&poses;poses.items={&standing,&crouched};
    standing.type=crouched.type="UIWindow";crouched.visible=false;
    health.ints["m_bLastStanding"]=1;health.floats["m_fAlpha"]=.9F;
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.line_count==4&&
        out.status.lines[3].view()==u"De pie","visible native standing pose missing from wrist card");Clean();
    standing.visible=false;crouched.visible=true;health.ints["m_bLastStanding"]=0;
    health.ints["m_bLastHidden"]=1;health.floats["m_fAlpha"]=.125F;
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.lines[3].view()==u"Agachado · En sombra",
        "native crouch/shadow feedback must follow HUD presentation caches");Clean();
    health.ints["m_bLastHidden"]=0;health.floats["m_fAlpha"]=.9F;
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.lines[3].view()==u"Agachado",
        "leaving native shadow retained hidden feedback");Clean();
    // Ambiguous visibility/cache transitions must not affect accepted health/ammo.
    standing.visible=true;ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"two visible pose icons must suppress ambiguous stance only");Clean();standing.visible=false;
    crouched.visible=false;ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"hidden native pose must not be resurrected");Clean();crouched.visible=true;
    health.ints["m_bLastStanding"]=1;ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"pose cache/visible icon disagreement must be rejected");Clean();health.ints["m_bLastStanding"]=0;
    health.ints["m_bForceUpdate"]=1;ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"reset/pending native update exposed stale pose");Clean();health.ints["m_bForceUpdate"]=0;
    for(float alpha:{0.F,-.1F,1.1F,std::numeric_limits<float>::quiet_NaN()}){
        health.floats["m_fAlpha"]=alpha;ReadCoJGameplayUi(&holder,&player,out);
        Require(out.status.line_count==3,"invalid/transparent native pose alpha exposed stance");Clean();
    }health.floats["m_fAlpha"]=.9F;
    crouched.type="HUDHint";ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"foreign pose class accepted");Clean();crouched.type="UIWindow";
    poses.items.push_back(&standing);ReadCoJGameplayUi(&holder,&player,out);
    Require(out.status.line_count==3,"unknown native pose layout accepted");Clean();poses.items.pop_back();
    for(const char* field:{"m_aPoses","m_bLastStanding","m_bLastHidden","m_bForceUpdate","m_fAlpha"}){
        fail=field;Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.line_count==3&&out.inventory.valid,
            "optional stance JNI failure must preserve health/ammo and clear exceptions");Clean();
    }fail.clear();
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.line_count==4,"stance did not recover after optional failure");Clean();
    health.visible=false;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"hidden native HUDPlayer leaked health/stance");Clean();health.visible=true;
    const auto original_counters=counters.items,original_infos=infos.items;
    const auto original_totals=totals.items,original_big=big.items;
    counters.items.assign(6,&loaded);infos.items.assign(6,&info);
    totals.items.assign(3,&reserve);big.items.assign(3,&big_icon);
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.status.line_count==10&&
        out.status.lines[9].view()==u"Reserva pistola  12",
        "optional pose must not exceed raster bounds or displace native health/ammo");Clean();
    counters.items=original_counters;infos.items=original_infos;totals.items=original_totals;big.items=original_big;
    health.objects["m_aPoses"]=nullptr;
    SpecialStatusChecks();
    health_text.visible=false;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"hidden native health resurrected");Clean();health_text.visible=true;
    health_text.floats["m_fCurTextAlpha"]=0;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"transparent native health resurrected");Clean();health_text.floats["m_fCurTextAlpha"]=1;
    info.ints["m_bActiveAmmo"]=0;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"inactive slot claimed as loaded active ammo");Clean();info.ints["m_bActiveAmmo"]=1;
    reserve.floats["m_fCurTextAlpha"]=0;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"transparent native reserve resurrected");Clean();reserve.floats["m_fCurTextAlpha"]=1;
    big_icon.visible=false;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"hidden reserve icon retained counter");Clean();big_icon.visible=true;
    health.objects["m_cPlayer"]=&other;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"foreign health player exposed");Clean();health.objects["m_cPlayer"]=&player;
    for(auto component:{&health,&ammo,&weapons}){
        component->objects["m_cHUDManager"]=&other;ReadCoJGameplayUi(&holder,&player,out);Require(out.status.line_count==2,"foreign status manager exposed");Clean();component->objects["m_cHUDManager"]=&hud;
    }
    hud.visible=false;ReadCoJGameplayUi(&holder,&player,out);Require(!out.status.active,"hidden manager retained status");Clean();hud.visible=true;
    fail="m_sLocalizedText";Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.status.active&&out.inventory.valid,"status JNI failure must clear only status domain");Clean();fail.clear();
    loaded.text=u"bad";Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.status.active,"unexpected native number must fail closed");Clean();loaded.text=u"3";
    totals.items.resize(4);Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.status.active,"unknown ammo array layout accepted");Clean();totals.items.resize(3);
    wp.ints["m_nRotorIdx"]=42;rotor0.ints["m_nIndex"]=42;rotors.items={&rotor1,&rotor0};
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.waypoints[0].label.view()==label.text&&out.waypoints[0].angle_degrees==90,"native stable rotor ID mistaken for collection index after removal/reorder");Clean();
    wp.ints["m_nRotorIdx"]=0;rotor0.ints["m_nIndex"]=0;rotors.items={&rotor0,&rotor1};
    rotor1.ints["m_nIndex"]=0;Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"duplicate rotor identity accepted");Clean();rotor1.ints["m_nIndex"]=1;
    sprite.visible=false;Require(ReadCoJGameplayUi(&holder,&player,out)&&out.waypoint_count==1&&out.waypoints[0].label.view()==label2.text,"hidden native objective resurrected");Clean();sprite.visible=true;
    player.ints["m_bWeaponChangeEnabled"]=0;Require(ReadCoJGameplayUi(&holder,&player,out)&&out.inventory.owned_mask==0xC2&&out.inventory.available_mask==0,"tutorial permission ignored");Clean();player.ints["m_bWeaponChangeEnabled"]=1;
    player.ints["m_bPunchingEnabled"]=0;Require(ReadCoJGameplayUi(&holder,&player,out)&&(out.inventory.available_mask&0x40)==0,"punch gate ignored");Clean();player.ints["m_bPunchingEnabled"]=1;
    player.objects["m_cCarriedObject"]=&carried;Require(ReadCoJGameplayUi(&holder,&player,out)&&out.inventory.available_mask==0x80,"carry-with-weapon permission or independent throw gate ignored");Clean();player.objects["m_cCarriedObject"]=nullptr;
    player.ints["m_bWeaponThrowEnabled"]=0;Require(ReadCoJGameplayUi(&holder,&player,out)&&(out.inventory.available_mask&0x80)==0,"native throw permission ignored");Clean();player.ints["m_bWeaponThrowEnabled"]=1;
    slot0.ints["m_nIndex"]=99;Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.inventory.valid,"bad slot index not rejected");Clean();slot0.ints["m_nIndex"]=0;
    pistol.objects["cOwner"]=&other;Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.inventory.valid,"foreign item owner accepted");Clean();pistol.objects["cOwner"]=&player;
    hud.objects["m_Being"]=&other;Require(ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"foreign HUD leaked guidance");Clean();hud.objects["m_Being"]=&player;
    compass.objects["m_Being"]=&other;Require(ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"foreign component player leaked guidance");Clean();compass.objects["m_Being"]=&player;
    compass.objects["m_cHUDManager"]=&other;Require(ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"foreign component manager leaked guidance");Clean();compass.objects["m_cHUDManager"]=&hud;
    compass.type="HUDHint";Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"wrong native component class accepted");Clean();compass.type="HUDCompass";
    wp.ints["m_bActive"]=0;Require(ReadCoJGameplayUi(&holder,&player,out)&&out.waypoint_count==1,"inactive native waypoint resurrected");Clean();wp.ints["m_bActive"]=1;
    wp.ints["m_nRotorIdx"]=-1;Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"negative native rotor index accepted");Clean();wp.ints["m_nRotorIdx"]=0;
    compass.visible=false;Require(ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid&&out.waypoint_count==0,"hidden compass retained markers");Clean();compass.visible=true;
    fail="m_fMapAngle";Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"JNI failure retained stale compass");Clean();fail.clear();
    target.floats["fX"]=std::numeric_limits<float>::quiet_NaN();Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"nonfinite objective accepted");Clean();target.floats["fX"]=0;
    label.text=std::u16string(100,u'x');label.text[61]=0xD83D;label.text[62]=0xDE00;
    Require(ReadCoJGameplayUi(&holder,&player,out)&&out.waypoints[0].label.length<64&&out.waypoints[0].label.view().back()==u'\u2026',"bounded native labels corrupted");Clean();
    waypoints.items.resize(257,&wp);Require(!ReadCoJGameplayUi(&holder,&player,out)&&!out.compass_valid,"unbounded native waypoint collection");Clean();
    Require(!ReadCoJGameplayUi(nullptr,&player,out)&&!out.inventory.valid&&!out.compass_valid,"missing env retained old state");
    std::cout<<"Native equipment/compass ownership, visibility, permissions and cleanup passed\n";
}
