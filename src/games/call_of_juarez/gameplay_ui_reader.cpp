#include "games/call_of_juarez/gameplay_ui_reader.hpp"
#include <algorithm>
#include <cmath>
#include <initializer_list>
namespace cojvr::games::call_of_juarez {
namespace {
union Value { std::int64_t alignment; void* object; std::int32_t integer; };
template<class T> T Fn(void* env,std::size_t i) noexcept { return reinterpret_cast<T>((*static_cast<void***>(env))[i]); }
using Lookup=void*(__stdcall*)(void*,void*,const char*,const char*);
using Field=void*(__stdcall*)(void*,void*,void*);
using CallObject=void*(__stdcall*)(void*,void*,void*,const Value*);
using CallInt=int(__stdcall*)(void*,void*,void*,const Value*);
using CallBool=std::uint8_t(__stdcall*)(void*,void*,void*,const Value*);
struct Reader {
    void* env;bool ok=true;
    bool Check() noexcept {
        auto ex=Fn<void*(__stdcall*)(void*)>(env,15)(env);
        if(ex){Fn<void(__stdcall*)(void*)>(env,17)(env);Fn<void(__stdcall*)(void*,void*)>(env,23)(env,ex);ok=false;}
        return ok;
    }
    struct Local {
        Reader& r;void* value;
        Local(Reader& a,void* b):r(a),value(b){}
        Local(const Local&)=delete;
        ~Local(){if(value)Fn<void(__stdcall*)(void*,void*)>(r.env,23)(r.env,value);}
        operator void*()const noexcept{return value;}
    };
    Local Class(const char* name) noexcept {
        auto c=ok?Fn<void*(__stdcall*)(void*,const char*)>(env,6)(env,name):nullptr;
        Check();if(!c)ok=false;return {*this,c};
    }
    void* Id(void* object,const char* name,const char* sig,bool method=false) noexcept {
        if(!object||!ok){ok=false;return nullptr;}
        Local c(*this,Fn<void*(__stdcall*)(void*,void*)>(env,31)(env,object));Check();
        auto id=ok&&c.value?Fn<Lookup>(env,method?33:94)(env,c,name,sig):nullptr;
        Check();if(!id)ok=false;return id;
    }
    Local Get(void* object,const char* name,const char* sig) noexcept {
        auto id=Id(object,name,sig);auto v=ok?Fn<Field>(env,95)(env,object,id):nullptr;
        Check();return {*this,v};
    }
    Local Static(void* cls,const char* name,const char* sig) noexcept {
        auto id=ok?Fn<Lookup>(env,144)(env,cls,name,sig):nullptr;Check();if(!id)ok=false;
        auto v=ok?Fn<Field>(env,145)(env,cls,id):nullptr;Check();return {*this,v};
    }
    bool Same(void* a,void* b) noexcept {
        bool yes=ok&&a&&b&&Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,24)(env,a,b)!=0;Check();return yes&&ok;
    }
    bool Instance(void* o,const char* name,bool required=true) noexcept {
        if(!o){if(required)ok=false;return false;}auto c=Class(name);
        bool yes=ok&&Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,32)(env,o,c)!=0;Check();
        if(required&&!yes)ok=false;return yes&&ok;
    }
    int Int(void* o,const char* name) noexcept {
        auto id=Id(o,name,"I");int v=ok?Fn<int(__stdcall*)(void*,void*,void*)>(env,100)(env,o,id):0;Check();return v;
    }
    int Byte(void* o,const char* name) noexcept {
        auto id=Id(o,name,"B");int v=ok?Fn<std::int8_t(__stdcall*)(void*,void*,void*)>(env,97)(env,o,id):0;Check();return v;
    }
    bool Bool(void* o,const char* name) noexcept {
        auto id=Id(o,name,"Z");bool v=ok&&Fn<std::uint8_t(__stdcall*)(void*,void*,void*)>(env,96)(env,o,id)!=0;Check();return v&&ok;
    }
    float Float(void* o,const char* name) noexcept {
        auto id=Id(o,name,"F");float v=ok?Fn<float(__stdcall*)(void*,void*,void*)>(env,102)(env,o,id):0;Check();
        if(!std::isfinite(v))ok=false;return v;
    }
    bool Visible(void* o) noexcept {
        if(!o||!ok)return false;auto id=Id(o,"IsActuallyVisible","()Z",true);
        bool v=ok&&Fn<CallBool>(env,39)(env,o,id,nullptr)!=0;Check();return v&&ok;
    }
    int Size(void* o,bool array=false) noexcept {
        if(!o||!ok)return 0;int n=0;
        if(array)n=Fn<int(__stdcall*)(void*,void*)>(env,171)(env,o);
        else{auto id=Id(o,"size","()I",true);if(ok)n=Fn<CallInt>(env,51)(env,o,id,nullptr);}
        Check();if(n<0||n>256)ok=false;return ok?n:0;
    }
    Local At(void* o,int i,bool array=false,bool list=false) noexcept {
        void* v=nullptr;
        if(ok&&array)v=Fn<void*(__stdcall*)(void*,void*,int)>(env,173)(env,o,i);
        else if(ok){auto id=Id(o,list?"get":"elementAt","(I)Ljava/lang/Object;",true);Value a{.integer=i};if(ok)v=Fn<CallObject>(env,36)(env,o,id,&a);}
        Check();return {*this,v};
    }
    Local WeaponForHand(void* player,int hand) noexcept {
        auto id=Id(player,"GetActualWeaponNotEmpty","(I)LWeapon;",true);Value a{.integer=hand};
        auto v=ok?Fn<CallObject>(env,36)(env,player,id,&a):nullptr;Check();return {*this,v};
    }
    template<class T> bool PrimitivePair(void* player,const char* name,const char* signature,std::array<T,2>& out) noexcept {
        auto array=Get(player,name,signature);
        if(!array.value||Size(array,true)!=2){ok=false;return false;}
        if(!ok)return false;
        auto data=Fn<void*(__stdcall*)(void*,void*,std::uint8_t*)>(env,222)(env,array,nullptr);
        // No JNI call while critical storage is borrowed. Copy only two values
        // then release read-only with JNI_ABORT before checking exceptions.
        if(data){std::copy_n(static_cast<const T*>(data),2,out.begin());
            Fn<void(__stdcall*)(void*,void*,void*,int)>(env,223)(env,array,data,2);}
        Check();if(!data)ok=false;return ok;
    }
    runtime::Vec3 Vector(void* v) noexcept {
        if(!v){ok=false;return {};}
        return {Float(v,"fX"),Float(v,"fY"),Float(v,"fZ")};
    }
    template<class T> void Copy(void* string,T& out) noexcept {
        if(!string||!ok)return;
        int length=Fn<int(__stdcall*)(void*,void*)>(env,164)(env,string);Check();if(!ok||length<=0)return;
        auto data=Fn<const char16_t*(__stdcall*)(void*,void*,std::uint8_t*)>(env,165)(env,string,nullptr);Check();
        if(!data)ok=false;
        if(ok){auto n=std::min(static_cast<std::size_t>(length),T::capacity-1);
            bool trunc=n<static_cast<std::size_t>(length);
            if(trunc){--n;if(n&&data[n-1]>=0xD800&&data[n-1]<=0xDBFF)--n;}
            std::copy_n(data,n,out.characters.begin());if(trunc)out.characters[n++]=u'\u2026';out.length=static_cast<std::uint32_t>(n);}
        if(data)Fn<void(__stdcall*)(void*,void*,const char16_t*)>(env,166)(env,string,data);Check();if(!ok)out={};
    }
};
void Label(runtime::UiLabel& out,std::u16string_view text) noexcept {
    auto n=std::min(text.size(),out.capacity-1);std::copy_n(text.data(),n,out.characters.data());out.length=static_cast<std::uint32_t>(n);
}
void PoseStatus(void* env,void* component,runtime::UiLabel& out) noexcept {
    out={};Reader r{env};
    // Optional presentation only. The caller has verified the HUDPlayer's
    // manager/player ownership. Use a separate reader so pose lookup failures
    // clear their exception without discarding accepted health/ammo text.
    if(r.Bool(component,"m_bForceUpdate"))return;
    auto poses=r.Get(component,"m_aPoses","[LUIWindow;");
    if(!poses.value||r.Size(poses,true)!=2)return;
    auto standing=r.At(poses,0,true),crouched=r.At(poses,1,true);
    if(!r.Instance(standing,"UIWindow")||!r.Instance(crouched,"UIWindow"))return;
    const bool stand_visible=r.Visible(standing),crouch_visible=r.Visible(crouched);
    const bool last_standing=r.Bool(component,"m_bLastStanding");
    const bool in_shadow=r.Bool(component,"m_bLastHidden");
    const float alpha=r.Float(component,"m_fAlpha");
    if(!r.ok||stand_visible==crouch_visible||last_standing!=stand_visible||alpha<=0||alpha>1)return;
    // Native hidden means shadow + enabled recognition feedback; it does not
    // prove invisibility to enemies. No negative/"spotted" state is inferred.
    Label(out,stand_visible?(in_shadow?u"De pie · En sombra":u"De pie"):
        (in_shadow?u"Agachado · En sombra":u"Agachado"));
}
bool OwnedHudComponent(Reader& r,void* component,const char* type,void* hud,void* player) noexcept {
    if(!component||!r.Instance(component,type))return false;
    auto owner=r.Get(component,"m_Being","LBeing;");
    auto manager=r.Get(component,"m_cHUDManager","LHUDManager;");
    return r.Same(owner,player)&&r.Same(manager,hud)&&r.Visible(component);
}
bool VisibleTexture(Reader& r,void* sprite,const char* type) noexcept {
    if(!sprite||!r.Instance(sprite,type)||!r.Visible(sprite))return false;
    const float alpha=r.Float(sprite,"m_fTextureAlpha");
    return r.ok&&alpha>0&&alpha<=1;
}
void ConcentrationStatus(void* env,void* components,void* hud,void* player,runtime::UiLabel& out) noexcept {
    out={};Reader r{env};auto component=r.At(components,10,true);
    if(!OwnedHudComponent(r,component,"HUDBulletTime",hud,player))return;
    auto owner=r.Get(component,"m_cPlayer","LArmedPlayerBeing;");
    if(!r.Same(owner,player))return;
    auto icon=r.Get(component,"m_cIcon","LUIStatic;");
    if(!VisibleTexture(r,icon,"UIStatic"))return;
    const int mode=r.Int(component,"m_nMode");if(!r.ok)return;
    // UpdateData commits the naturally selected icon mode. No eligibility
    // query, cooldown clock or gameplay update is executed by observation.
    switch(mode){
    case 1:Label(out,u"Concentración · En uso");break;
    case 2:Label(out,u"Concentración · Recargando");break;
    case 3:case 4:Label(out,u"Concentración · Lista");break;
    case 5:Label(out,u"Concentración · No preparada");break;
    default:break;
    }
}
bool CachedNumber(Reader& r,void* sprite,runtime::UiLabel& out) noexcept {
    out={};if(!sprite)return r.ok;
    if(!r.Instance(sprite,"UIStatic"))return false;
    if(!r.Visible(sprite))return r.ok;
    const float alpha=r.Float(sprite,"m_fCurTextAlpha");
    if(!r.ok)return false;if(alpha<=0||alpha>1)return true;
    auto text=r.Get(sprite,"m_sLocalizedText","Ljava/lang/String;");r.Copy(text,out);
    const auto digits=out.view();
    return r.ok&&digits.size()<=10&&
        std::all_of(digits.begin(),digits.end(),[](char16_t c){return c>=u'0'&&c<=u'9';});
}
void CountdownStatus(void* env,void* components,void* hud,void* player,runtime::UiLabel& out) noexcept {
    out={};Reader r{env};auto component=r.At(components,17,true);
    if(!OwnedHudComponent(r,component,"HUDCountdownTimer",hud,player))return;
    auto timer=r.Get(component,"m_cCountdownTimer","LCountdownTimer;");
    if(!r.Instance(timer,"CountdownTimer"))return;
    auto window=r.Get(component,"m_cWindow","LUIWindow;");
    if(!r.Instance(window,"UIWindow")||!r.Visible(window))return;
    auto bottom=r.Get(component,"m_cBottomText","LUIStatic;");
    auto center=r.Get(component,"m_cCenterText","LUIStatic;");
    runtime::UiLabel bottom_number{},center_number{};
    if(!CachedNumber(r,bottom,bottom_number)||!CachedNumber(r,center,center_number))return;
    const bool has_bottom=!bottom_number.view().empty(),has_center=!center_number.view().empty();
    if(!r.ok||has_bottom==has_center)return;
    // UpdateText stores int(time-to-finish)+1 in precisely one text owner.
    // Duels use center text, ordinary titled timers use bottom text. Copy it
    // unchanged: a countdown is not native permission to draw or fire.
    const auto number=has_bottom?bottom_number.view():center_number.view();
    Label(out,u"Cuenta atrás  ");
    std::copy(number.begin(),number.end(),out.characters.begin()+out.length);
    out.length+=static_cast<std::uint32_t>(number.size());
}
template<class T> bool CachedText(Reader& r,void* sprite,T& out) noexcept {
    out={};if(!sprite||!r.Instance(sprite,"UIStatic")||!r.Visible(sprite))return false;
    const float alpha=r.Float(sprite,"m_fCurTextAlpha");
    if(!r.ok||alpha<=0||alpha>1)return false;
    auto text=r.Get(sprite,"m_sLocalizedText","Ljava/lang/String;");r.Copy(text,out);
    return r.ok;
}
void AppendText(runtime::HudText& out,std::u16string_view text) noexcept {
    if(text.empty()||out.length>=out.capacity-1)return;
    if(out.length>=out.capacity-2)return; // No space for separator plus content.
    if(out.length)out.characters[out.length++]=u'\n';
    const auto room=out.capacity-1-out.length;
    const bool truncated=room<text.size();
    auto n=std::min(truncated&&room?room-1:room,text.size());
    if(n<text.size()&&n&&text[n-1]>=0xD800&&text[n-1]<=0xDBFF)--n;
    std::copy_n(text.begin(),n,out.characters.begin()+out.length);
    out.length+=static_cast<std::uint32_t>(n);
    if(n<text.size()&&out.length<out.capacity-1)out.characters[out.length++]=u'\u2026';
}
void MissionTimer(void* env,void* components,void* hud,void* player,runtime::HudText& out) noexcept {
    out={};Reader r{env};auto component=r.At(components,17,true);
    if(!OwnedHudComponent(r,component,"HUDCountdownTimer",hud,player))return;
    auto timer=r.Get(component,"m_cCountdownTimer","LCountdownTimer;");
    auto window=r.Get(component,"m_cWindow","LUIWindow;");
    if(!r.Instance(timer,"CountdownTimer")||!r.Instance(window,"UIWindow")||!r.Visible(window))return;
    auto bottom=r.Get(component,"m_cBottomText","LUIStatic;");auto center=r.Get(component,"m_cCenterText","LUIStatic;");
    runtime::UiLabel a{},b{};
    if(!CachedNumber(r,bottom,a)||!CachedNumber(r,center,b)||a.view().empty()==b.view().empty()||!r.ok)return;
    // Native blink/visibility and exclusive number owner are authoritative.
    // This panel does not depend on health/ammo row capacity or a wrist pose.
    auto title=r.Get(component,"m_cTitle","LUIStatic;");runtime::HudText text{};
    (void)CachedText(r,title,text);if(!r.ok)return;
    const auto number=a.view().empty()?b.view():a.view();
    const auto title_room=out.capacity-2-number.size();
    if(text.length>title_room){
        auto n=title_room-1;
        if(n&&text.characters[n-1]>=0xD800&&text.characters[n-1]<=0xDBFF)--n;
        text.characters[n++]=u'\u2026';text.length=static_cast<std::uint32_t>(n);
    }
    AppendText(out,text.view());AppendText(out,number);
}
void MissionNotice(void* env,void* components,void* hud,void* player,int slot,
    const char* type,runtime::HudText& out) noexcept {
    Reader r{env};auto component=r.At(components,slot,true);
    if(!OwnedHudComponent(r,component,type,hud,player))return;
    if(slot==12){
        auto window=r.Get(component,"m_cMainWindow","LUIWindowInfo;");
        if(!r.Instance(window,"UIWindowInfo")||!r.Visible(window))return;
    }
    auto sprite=r.Get(component,slot==12?"m_cInfo":"m_cText","LUIStatic;");runtime::HudText text{};
    if(CachedText(r,sprite,text))AppendText(out,text.view());
}
void Threats(void* env,void* components,void* hud,void* player,int slot,
    const char* type,CoJGameplayUiSnapshot& out) noexcept {
    Reader r{env};auto component=r.At(components,slot,true);
    if(!OwnedHudComponent(r,component,type,hud,player))return;
    // IsInstanceOf the base alone admits its direction-only subclass. A
    // misplaced/aliased owner must never manufacture red damage feedback.
    if(slot==16&&r.Instance(component,"HUDDirectionIndicator",false))return;
    auto indicators=r.Get(component,"m_aIndicators","[LDamageIndicator;");
    if(!indicators.value)return;
    const int count=r.Size(indicators,true);
    // The shipped HUD has six icons per owner. Unknown layouts fail closed.
    if(!r.ok||count>6)return;
    std::array<CoJGameplayUiSnapshot::Threat,6> values{};std::uint32_t used=0;
    for(int i=0;i<count&&r.ok;++i){
        auto indicator=r.At(indicators,i,true);
        if(!r.Instance(indicator,"DamageIndicator"))return;
        if(!r.Bool(indicator,"m_bActive"))continue;
        auto position=r.Get(indicator,"m_cPositioner","LUIWindow;");
        auto icon=r.Get(indicator,"m_cIcon","LUIWindow;");
        if(!r.Instance(position,"UIWindow")||!r.Visible(position)||!VisibleTexture(r,icon,"UIWindow"))continue;
        const float angle=r.Float(indicator,"m_fDamageAngle"),alpha=r.Float(icon,"m_fTextureAlpha");
        if(!r.ok)return;
        values[used++]={angle,alpha,slot==16};
    }
    if(!r.ok||out.threat_count+used>out.threats.size())return;
    std::copy_n(values.begin(),used,out.threats.begin()+out.threat_count);out.threat_count+=used;
}
void CriticalAlerts(void* env,void* player,CoJGameplayUiSnapshot& out) noexcept {
    Reader r{env};auto cls=r.Class("HUDManager");auto hud=r.Static(cls,"sm_cMainHUDManager","LHUDManager;");
    if(!hud.value)return;auto owner=r.Get(hud,"m_Being","LBeing;");
    if(!r.Same(owner,player)||!r.Visible(hud))return;
    auto components=r.Get(hud,"m_aHudComponents","[LHUDComponent;");
    if(!components.value||r.Size(components,true)!=23||!r.ok)return;
    MissionTimer(env,components,hud,player,out.mission_timer);
    MissionNotice(env,components,hud,player,12,"HUDObjective",out.mission_notices);
    MissionNotice(env,components,hud,player,13,"HUDTipObjectives",out.mission_notices);
    MissionNotice(env,components,hud,player,14,"HUDTipLogs",out.mission_notices);
    Threats(env,components,hud,player,15,"HUDDirectionIndicator",out);
    Threats(env,components,hud,player,16,"HUDDamageIndicator",out);
}
void AppendPercent(runtime::UiLabel& out,int percent) noexcept {
    // Only callers' checked native 0-100 values reach this bounded formatter.
    if(percent==100)out.characters[out.length++]=u'1';
    if(percent>=10)out.characters[out.length++]=static_cast<char16_t>(u'0'+(percent/10)%10);
    out.characters[out.length++]=static_cast<char16_t>(u'0'+percent%10);
    out.characters[out.length++]=u'%';
}
void HorseStatus(void* env,void* components,void* hud,void* player,runtime::UiLabel& out) noexcept {
    out={};Reader r{env};auto component=r.At(components,18,true);
    if(!OwnedHudComponent(r,component,"HUDHorse",hud,player))return;
    auto icon=r.Get(component,"m_cHorseIcon","LUIWindow;");
    auto fill=r.Get(component,"m_cTirednessIconFill","LUIWindow;");
    if(!VisibleTexture(r,icon,"UIWindow")||!VisibleTexture(r,fill,"UIWindow"))return;
    if(r.Bool(component,"m_bUpdateHealthLevel"))return;
    const int health=r.Int(component,"m_nHealthLevel");
    const float fatigue=r.Float(component,"m_fLastTiredness");
    if(!r.ok||health<0||health>100||fatigue<0||fatigue>100)return;
    // Show/Reset invalidate these caches. Keep the native health percentage
    // and fatigue meaning; do not infer remaining stamina or refresh the horse.
    Label(out,u"Caballo · Salud ");AppendPercent(out,health);
    constexpr std::u16string_view suffix=u" · Fatiga ";
    std::copy(suffix.begin(),suffix.end(),out.characters.begin()+out.length);
    out.length+=static_cast<std::uint32_t>(suffix.size());AppendPercent(out,static_cast<int>(fatigue));
}
bool Inventory(void* env,void* player,runtime::EquipmentWheelSnapshot& out) noexcept {
    Reader r{env};auto slots=r.Get(player,"m_aInvSlots","Ljava/util/ArrayList;");
    auto items=r.Get(player,"m_aInvSlotsObjects","[LInvObject;");
    if(!slots.value||!items.value)return r.ok;
    const int count=r.Size(slots),item_count=r.Size(items,true);
    constexpr int types[]{2,1,0,4,3,5,7};
    constexpr std::u16string_view names[]{u"Left pistol",u"Right pistol",u"Long weapon",u"Dynamite",u"Bible / whip",u"Bow",u"Hands",u"Discard"};
    for(int i=0;i<8;++i)Label(out.labels[i],names[i]);
    const bool changes=r.Bool(player,"m_bWeaponChangeEnabled"),punch=r.Bool(player,"m_bPunchingEnabled"),throws=r.Bool(player,"m_bWeaponThrowEnabled");
    auto carried=r.Get(player,"m_cCarriedObject","LControlObject;");
    // The active reference is single-player. This is the shipped SP carry gate;
    // multiplayer availability remains outside this campaign adapter.
    const bool carry_ok=!carried.value||r.Bool(player,"m_bCanCarryWithWeaponInSingleGame");
    for(int i=0;i<count&&r.ok;++i){auto slot=r.At(slots,i,false,true);r.Instance(slot,"InventorySlot");
        auto owner=r.Get(slot,"m_cPawn","LPawnInventory;");if(!r.Same(owner,player)){r.ok=false;break;}
        int index=r.Int(slot,"m_nIndex"),type=r.Byte(slot,"m_nType");
        if(index<0||index>=item_count){r.ok=false;break;}
        auto item=r.At(items,index,true);if(!item.value)continue;
        auto item_owner=r.Get(item,"cOwner","LPawnInventory;");if(!r.Same(item_owner,player)){r.ok=false;break;}
        if(!r.Instance(item,"Weapon",false))continue;
        for(int k=0;k<7;++k)if(types[k]==type){
            out.owned_mask|=static_cast<std::uint8_t>(1U<<k);
            if(k==4)Label(out.labels[k],r.Instance(item,"WeaponWhip",false)?u"Whip":u"Bible");
        }
        if(type!=7)out.owned_mask|=0x80;
    }
    if(r.ok){out.valid=true;out.available_mask=changes&&carry_ok?out.owned_mask:0;
        if(!punch)out.available_mask&=static_cast<std::uint8_t>(~0x40U);
        // WeaponThrow has the change/throw gates but no carrying restriction.
        out.available_mask&=0x7F;
        if(changes&&throws)out.available_mask|=out.owned_mask&0x80;}
    else out={};return r.ok;
}
bool Status(void* env,void* player,runtime::WristStatusSnapshot& out) noexcept {
    Reader r{env};runtime::WristStatusSnapshot value{};
    runtime::UiLabel pose{};
    auto cls=r.Class("HUDManager");auto hud=r.Static(cls,"sm_cMainHUDManager","LHUDManager;");
    if(!hud.value)return r.ok;
    auto owner=r.Get(hud,"m_Being","LBeing;");
    if(!r.Same(owner,player)||!r.Visible(hud))return r.ok;
    auto components=r.Get(hud,"m_aHudComponents","[LHUDComponent;");
    if(!components.value||r.Size(components,true)!=23)return false;
    // Read the native presentation caches; never call health/ammo updates or
    // weapon parameter factories. Each component retains its own permission.
    constexpr int indices[]{0,7,1};
    constexpr const char* classes[]{"HUDPlayer","HUDWeapons","HUDAmmoCounters"};
    auto append=[&](void* sprite,std::u16string_view prefix,bool health=false) noexcept {
        if(!sprite||!r.Visible(sprite))return;
        if(!r.Instance(sprite,"UIStatic"))return;
        if(!(r.Float(sprite,"m_fCurTextAlpha")>0))return;
        auto text=r.Get(sprite,"m_sLocalizedText","Ljava/lang/String;");runtime::UiLabel number{};r.Copy(text,number);
        const auto digits=number.view();if(digits.empty())return;
        if(digits.size()>10||std::any_of(digits.begin(),digits.end(),[](char16_t c){return c<u'0'||c>u'9';})||
            value.line_count>=value.lines.size()||prefix.size()+digits.size()>=runtime::UiLabel::capacity){r.ok=false;return;}
        // Parse only the already admitted native HUDPlayer health digits,
        // never an ammo row, localized label or a new game health query.
        if(health&&digits.size()<=3){
            int percent=0;for(const auto digit:digits)percent=percent*10+(digit-u'0');
            if(percent<=100){value.health_row=static_cast<int>(value.line_count);value.health_percent=percent;}
        }
        auto& line=value.lines[value.line_count++];Label(line,prefix);
        std::copy(digits.begin(),digits.end(),line.characters.begin()+line.length);
        line.length+=static_cast<std::uint32_t>(digits.size());
    };
    for(int domain=0;domain<3&&r.ok;++domain){
        auto component=r.At(components,indices[domain],true);if(!component.value)continue;
        if(!r.Instance(component,classes[domain]))break;
        auto being=r.Get(component,"m_Being","LBeing;");auto manager=r.Get(component,"m_cHUDManager","LHUDManager;");
        if(!r.Same(being,player)||!r.Same(manager,hud)||!r.Visible(component))continue;
        if(domain==0){
            auto health_player=r.Get(component,"m_cPlayer","LPlayerBeing;");if(!r.Same(health_player,player))continue;
            auto health=r.Get(component,"m_cHealth","LUIStatic;");append(health,u"Salud  ",true);
            if(r.ok)PoseStatus(env,component,pose);
        }else if(domain==1){
            auto counters=r.Get(component,"m_tAmmoCounters","[LUIStatic;");
            auto infos=r.Get(component,"m_tSlotsWeaponInfo","[LHUDWeapons$WeaponInfo;");
            if(!counters.value||!infos.value)continue;
            if(r.Size(counters,true)!=6||r.Size(infos,true)!=6){r.ok=false;break;}
            constexpr std::u16string_view names[]{u"Izquierda  ",u"Derecha  ",u"Arma larga  ",u"Dinamita  ",u"Equipo  ",u"Arco  "};
            for(int i=0;i<6&&r.ok;++i){
                auto info=r.At(infos,i,true);if(!info.value)continue;
                if(!r.Instance(info,"HUDWeapons$WeaponInfo"))break;
                const bool show=r.Bool(info,"m_bShowAmmo"),active=r.Bool(info,"m_bActiveAmmo");
                if(!show||!active)continue;
                auto counter=r.At(counters,i,true);if(!counter.value)continue;
                if(r.Float(counter,"m_fCurTextAlpha")>0)append(counter,names[i]);
            }
        }else{
            auto totals=r.Get(component,"m_cAmmoCountersTotal","[LUIStatic;");
            auto big=r.Get(component,"m_cBulletIconsBig","[LUIWindow;");
            auto small=r.Get(component,"m_cBulletIconsSmall","[LUIWindow;");
            if(!totals.value||!big.value||!small.value)continue;
            if(r.Size(totals,true)!=3||r.Size(big,true)!=3||r.Size(small,true)!=3){r.ok=false;break;}
            constexpr std::u16string_view names[]{u"Reserva rifle  ",u"Reserva escopeta  ",u"Reserva pistola  "};
            for(int i=0;i<3&&r.ok;++i){
                auto counter=r.At(totals,i,true);auto large=r.At(big,i,true);auto little=r.At(small,i,true);
                const bool visible=r.Visible(large)|r.Visible(little);
                if(counter.value&&visible&&r.Float(counter,"m_fCurTextAlpha")>0)append(counter,names[i]);
            }
        }
    }
    if(r.ok){
        // Isolate optional cached presentation failures from health/ammo and
        // each other. Essential rows retain priority within the ten-row card;
        // the transient countdown gets the first remaining row.
        for(auto read:{CountdownStatus,HorseStatus,ConcentrationStatus}){
            if(value.line_count==value.lines.size())break;
            runtime::UiLabel optional{};read(env,components,hud,player,optional);
            if(!optional.view().empty())value.lines[value.line_count++]=optional;
        }
        if(!pose.view().empty()&&value.line_count<value.lines.size())value.lines[value.line_count++]=pose;
        value.active=value.line_count!=0;out=value;
    }else out={};return r.ok;
}
bool Compass(void* env,void* player,CoJGameplayUiSnapshot& out) noexcept {
    Reader r{env};auto cls=r.Class("HUDManager");auto hud=r.Static(cls,"sm_cMainHUDManager","LHUDManager;");
    if(!hud.value)return r.ok;auto owner=r.Get(hud,"m_Being","LBeing;");
    if(!r.Same(owner,player)||!r.Visible(hud))return r.ok;
    auto components=r.Get(hud,"m_aHudComponents","[LHUDComponent;");
    if(r.Size(components,true)!=23)return false;
    auto compass=r.At(components,19,true);if(!compass.value)return r.ok;
    if(!r.Instance(compass,"HUDCompass"))return false;
    auto component_player=r.Get(compass,"m_Being","LBeing;");
    auto component_manager=r.Get(compass,"m_cHUDManager","LHUDManager;");
    if(!r.Same(component_player,player)||!r.Same(component_manager,hud))return r.ok;
    if(!r.Visible(compass))return r.ok;
    out.map_angle_degrees=r.Float(compass,"m_fMapAngle");
    auto position=r.Get(compass,"m_vPlayerPos","LVector;");out.player_position_cm=r.Vector(position);
    auto forward=r.Get(compass,"m_vPlayerForward","LVector;");out.player_forward=r.Vector(forward);
    auto left=r.Get(compass,"m_vPlayerRight","LVector;");out.player_left=r.Vector(left);
    auto manager=r.Get(compass,"m_cWaypointsManager","LHUDWaypointsManager;");
    auto rotor=r.Get(compass,"m_cRotorText","LUIRotorText;");
    if(!manager.value||!rotor.value)return r.ok;
    auto waypoints=r.Get(manager,"m_cWaypoints","Ljava/util/Vector;");auto elements=r.Get(rotor,"m_cElements","Ljava/util/Vector;");
    const int count=r.Size(waypoints),element_count=r.Size(elements);
    // m_nRotorIdx is the monotonically assigned element ID, not its current
    // vector index. Removing a location compacts the vector without reindexing.
    std::array<int,256> rotor_ids{};
    for(int i=0;i<element_count&&r.ok;++i){auto elem=r.At(elements,i);
        if(!r.Instance(elem,"UIRotorText$RotorTextElement"))break;
        const int id=r.Int(elem,"m_nIndex");
        if(id<0||std::find(rotor_ids.begin(),rotor_ids.begin()+i,id)!=rotor_ids.begin()+i){r.ok=false;break;}
        rotor_ids[i]=id;
    }
    for(int i=0;i<count&&r.ok;++i){auto wp=r.At(waypoints,i);if(!r.Instance(wp,"HUDWaypointsManager$HUDWaypoint"))break;
        if(!r.Bool(wp,"m_bActive"))continue;const int id=r.Int(wp,"m_nRotorIdx");
        auto found=std::find(rotor_ids.begin(),rotor_ids.begin()+element_count,id);
        if(id<0||found==rotor_ids.begin()+element_count){r.ok=false;break;}
        const int index=static_cast<int>(found-rotor_ids.begin());
        auto elem=r.At(elements,index);if(!r.Instance(elem,"UIRotorText$RotorTextElement"))break;
        bool visible=false;
        for(const char* field:{"m_cElement","m_cPoint","m_cObjectPoint"}){auto sprite=r.Get(elem,field,"LUIStatic;");visible=r.Visible(sprite)||visible;}
        if(!visible)continue;
        if(out.waypoint_count==out.waypoints.size())continue;
        auto& marker=out.waypoints[out.waypoint_count];
        auto text_owner=r.Get(elem,"m_cElement","LUIStatic;");
        if(text_owner.value){auto text=r.Get(text_owner,"m_sLocalizedText","Ljava/lang/String;");r.Copy(text,marker.label);}
        else {auto text=r.Get(wp,"m_sName","Ljava/lang/String;");r.Copy(text,marker.label);}
        auto pos=r.Get(wp,"m_vPos","LVector;");marker.position_cm=r.Vector(pos);
        marker.angle_degrees=r.Float(elem,"m_fAngle");marker.visible=true;++out.waypoint_count;
    }
    if(r.ok){out.compass_valid=true;out.compass_visible=true;}return r.ok;
}
}
bool ReadCoJGameplayUi(void* env, void* player, CoJGameplayUiSnapshot& out) noexcept {
    out={};if(!env||!player||!*static_cast<void***>(env))return false;
    constexpr std::size_t required[]{6,15,17,23,24,31,32,33,36,39,51,94,95,96,97,100,102,144,145,164,165,166,171,173};
    for(auto i:required)if(!(*static_cast<void***>(env))[i])return false;
    Reader entry{env};if(!entry.Check())return false;
    const bool inventory=Inventory(env,player,out.inventory);const bool compass=Compass(env,player,out);
    const bool status=Status(env,player,out.status);
    CriticalAlerts(env,player,out);
    if(!compass){out.compass_valid=false;out.compass_visible=false;out.waypoint_count=0;out.waypoints={};out.map_angle_degrees=0;out.player_position_cm={};out.player_forward={};out.player_left={};}
    return inventory&&compass&&status;
}
bool ReadCoJNoShoot(void* env, void* player, int hand, CoJNoShootSnapshot& out) noexcept {
    out={};if(!env||!player||(hand!=0&&hand!=1)||!*static_cast<void***>(env))return false;
    constexpr std::size_t required[]{6,15,17,23,24,31,32,33,36,39,94,95,96,100,102,144,145,171,173,222,223};
    for(auto i:required)if(!(*static_cast<void***>(env))[i])return false;
    Reader r{env};if(!r.Check())return false;
    auto cls=r.Class("HUDManager");auto hud=r.Static(cls,"sm_cMainHUDManager","LHUDManager;");
    if(!hud.value)return r.ok;
    auto owner=r.Get(hud,"m_Being","LBeing;");if(!r.Same(owner,player)||!r.Visible(hud))return r.ok;
    auto components=r.Get(hud,"m_aHudComponents","[LHUDComponent;");
    if(!components.value||r.Size(components,true)!=23)return false;
    auto cross=r.At(components,3+hand,true);if(!cross.value)return r.ok;
    if(!r.Instance(cross,"HUDCrosshairHand"))return false;
    auto being=r.Get(cross,"m_Being","LBeing;");auto manager=r.Get(cross,"m_cHUDManager","LHUDManager;");
    if(!r.Same(being,player)||!r.Same(manager,hud)||r.Int(cross,"m_iHand")!=hand||!r.Visible(cross))return r.ok;
    auto warning=r.Get(cross,"dontShoot","LSprite;");
    if(!warning.value)return r.ok;if(!r.Instance(warning,"Sprite"))return false;
    const bool visible=r.Visible(warning);
    auto weapon=r.WeaponForHand(player,hand);if(!weapon.value)return r.ok;
    if(!r.Instance(weapon,"WeaponFire",false)||r.Instance(weapon,"WeaponWhip",false))return r.ok;
    auto weapon_owner=r.Get(weapon,"cOwner","LPawnInventory;");
    if(!r.Same(weapon_owner,player)||!r.Bool(weapon,"m_bInHands"))return r.ok;
    auto traced=r.Get(player,hand==0?"cojvrRightTargetWeapon":"cojvrLeftTargetWeapon","LWeapon;");
    if(!r.Same(traced,weapon))return r.ok;
    std::array<std::uint8_t,2> can_fire{};std::array<std::int32_t,2> reasons{};std::array<float,2> ages{};
    if(!r.PrimitivePair(player,"m_abCanFireAtTarget","[Z",can_fire)||
       !r.PrimitivePair(player,"m_anCantFireReason","[I",reasons)||
       !r.PrimitivePair(player,"m_afLastAimCheckTime","[F",ages))return false;
    const float age=ages[hand];if(!std::isfinite(age)||age<0||age>.25F)return r.ok;
    auto starts=r.Get(player,"m_vLCStart","[LVector;");auto ends=r.Get(player,"m_vLCEnd","[LVector;");
    auto collisions=r.Get(player,"m_cLastCollision","[LCollision;");
    if(!starts.value||!ends.value||!collisions.value||r.Size(starts,true)!=2||r.Size(ends,true)!=2||r.Size(collisions,true)!=2)return false;
    auto collision=r.At(collisions,hand,true);if(!collision.value)return r.ok;
    auto start=r.At(starts,hand,true);auto end=r.At(ends,hand,true);
    CoJNoShootSnapshot value{};value.trace_start_cm=r.Vector(start);value.trace_end_cm=r.Vector(end);
    value.hand=hand;value.reason=reasons[hand];value.age_seconds=age;
    value.warning_visible=visible&&!can_fire[hand]&&(value.reason==1||value.reason==3);
    if(!r.ok)return false;value.valid=true;out=value;return true;
}
}
