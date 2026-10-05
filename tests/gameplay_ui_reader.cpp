#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
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
O compass,waypoints_manager,waypoints,wp,wp2,rotor,rotors,rotor0,rotor1,sprite,sprite2;
O pos,forward,target,target2,label,label2,carried;
O left;
O health,health_text,ammo,totals,big,small,weapons,counters,infos,info,loaded,reserve;
O big_icon,small_icon;
O cross,warning,starts,ends,start,end,can_fire,reasons,ages,collisions,collision;
std::vector<O*> all{&hud_class,&exception,&player,&other,&hud,&components,&slots,&objects,&pistol,&hands,
    &slot0,&slot1,&compass,&waypoints_manager,&waypoints,&wp,&wp2,&rotor,&rotors,&rotor0,&rotor1,
    &sprite,&sprite2,&pos,&forward,&left,&target,&target2,&label,&label2,&carried};
void* table[224]{};void** holder=table;bool pending=false;int pins=0,critical=0;std::string fail;
std::unordered_map<std::string,std::string> tokens;
std::unordered_map<std::string,O> classes;
void Require(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
void* Local(O* o){if(o)++o->refs;return o;}
void Clean(){Require(!pending&&pins==0&&critical==0,"exception/string/primitive leak");for(auto o:all)Require(o->refs==0,"JNI reference leak");for(auto& [n,o]:classes)Require(o.refs==0,"class reference leak");}
void* __stdcall Find(void*,const char* n){if(std::strcmp(n,"HUDManager")==0)return Local(&hud_class);auto& c=classes[n];c.type=n;return Local(&c);}
unsigned char __stdcall Instance(void*,void* o,void* c){auto& type=static_cast<O*>(o)->type;auto& wanted=static_cast<O*>(c)->type;return type==wanted||(wanted=="Weapon"&&type.starts_with("Weapon"))||(wanted=="WeaponFire"&&type=="WeaponPistolPeacemaker");}
void* __stdcall Exception(void*){Require(critical==0,"JNI while primitive pinned");return pending?Local(&exception):nullptr;}
void __stdcall Clear(void*){pending=false;}
void __stdcall Delete(void*,void* v){auto o=static_cast<O*>(v);Require(o&&o->refs>0,"invalid delete");--o->refs;}
unsigned char __stdcall Same(void*,void* a,void* b){return a==b;}
void* __stdcall Class(void*,void* o){return Local(static_cast<O*>(o));}
void* __stdcall Lookup(void*,void*,const char* n,const char*){Require(!pending,"JNI after exception");if(fail==n){pending=true;return nullptr;}return &tokens.emplace(n,n).first->second;}
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
    Require(out.status.active&&out.status.line_count==3&&out.status.lines[0].view()==u"Salud  087"&&
        out.status.lines[1].view()==u"Derecha  3"&&out.status.lines[2].view()==u"Reserva pistola  12",
        "native health, loaded slot and inventory reserve must be copied without recalculation");
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
