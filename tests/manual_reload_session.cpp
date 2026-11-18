#include "games/call_of_juarez/manual_reload_session.hpp"
#include "games/call_of_juarez/manual_reload_settings.hpp"
#include "games/call_of_juarez/ammunition_pickup_guide.hpp"
#include "games/call_of_juarez/manual_reload_target.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
void Check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
Pose At(Vec3 p){Pose pose{};pose.position=p;pose.position_valid=pose.orientation_valid=true;return pose;}
struct Fixture{
    CoJManualReloadSession session;
    CoJManualReloadInput s{}; GameplayInputState mapped{};
    Fixture(){s.enabled=s.allowed=true;s.player=7;s.context=5;
        s.raw.active=true;s.raw.input_context_generation=8;
        s.admission={0,42,true,true};s.recovery=s.admission;
        s.head=At({0,1.6F,0});s.left=At({-.2F,1.05F,-.1F});s.right=At({.2F,1.35F,-.35F});
        s.native.valid=s.native.probe_valid=true;s.native.weapon_id=42;s.native.armed_hand=0;s.native.states[0]={1,1,1,1};
        s.native.reserve_readback=10;
    }
    CoJManualReloadOutput Poll(){++s.sequence;s.now+=50;mapped=s.raw;return session.Update(mapped,s);}
    void Open(){(void)Poll();s.raw.reload=true;auto out=Poll();Check(out.start,"Square must prepare");
        session.NoteStart(true);s.raw.reload=false;s.native.probe_status=1;s.native.states[0]={20,21,21,21};
        Check(Poll().tracked,"owned opening must keep VR presentation");
        s.admission={};s.native.probe_status=2;s.native.states[0]={21,21,21,21};
        Check(Poll().tracked,"owned waiting must keep VR presentation");
        Check(session.phase()==CoJManualReloadPhase::manual_load,"opening -> manual");}
    void Claim(){s.raw.fire_left=false;(void)Poll();s.raw.fire_left=true;Check(Poll().gesture.cartridge_held,"waist trigger pickup");}
    void Carry(Pose& support,const Pose& target){
        const auto start=support.position,end=target.position;
        for(int step=1;step<=8;++step){
            const float t=static_cast<float>(step)/8.F;
            support=At({start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t,start.z+(end.z-start.z)*t});
            const auto out=Poll();
            Check(out.gesture.cartridge_held&&!out.insert,"sampled held carry keeps its token without inserting");
        }
    }
    CoJManualReloadOutput Insert(){Carry(s.left,s.right);s.raw.fire_left=false;return Poll();}
};
struct IndexedFixture:Fixture {
    IndexedFixture(int hand,bool lemant) {
        s.native.kind=s.admission.kind=s.recovery.kind=CoJManualReloadKind::indexed;
        s.admission.single_round_supported=s.recovery.single_round_supported=false;
        s.admission.manual_supported=s.recovery.manual_supported=true;
        s.native.native_body_cycle=true;
        s.native.index_count=2;s.native.selected_index=1;
        s.native.capacities=lemant?std::array<int,2>{1,8}:std::array<int,2>{1,1};
        s.native.loaded=lemant?std::array<int,2>{0,2}:std::array<int,2>{0,0};
        s.native.reserves={10,7,9};
        s.native.reserve_kinds=lemant?
            std::array{CoJReloadReserveKind::rifle,CoJReloadReserveKind::pistol}:
            std::array{CoJReloadReserveKind::shotgun,CoJReloadReserveKind::shotgun};
        s.native.reserve_indices=lemant?std::array<int,2>{1,0}:std::array<int,2>{2,2};
        s.native.armed_hand=s.admission.armed_hand=s.recovery.armed_hand=hand;
        s.right=At(hand==0?Vec3{.2F,1.35F,-.35F}:Vec3{.2F,1.05F,-.1F});
        s.left=At(hand==1?Vec3{-.2F,1.35F,-.35F}:Vec3{-.2F,1.05F,-.1F});
    }
    bool& SupportTrigger(){return s.native.armed_hand==0?s.raw.fire_left:s.raw.fire_right;}
    bool& ArmedTrigger(){return s.native.armed_hand==0?s.raw.fire_right:s.raw.fire_left;}
    Pose& Support(){return s.native.armed_hand==0?s.left:s.right;}
    Pose& Armed(){return s.native.armed_hand==0?s.right:s.left;}
    bool SupportFire()const{return s.native.armed_hand==0?mapped.fire_left:mapped.fire_right;}
    void Pickup(){SupportTrigger()=false;(void)Poll();SupportTrigger()=true;
        Check(Poll().gesture.cartridge_held,"indexed fresh waist pickup missing");}
    CoJManualReloadOutput Release(){Carry(Support(),Armed());SupportTrigger()=false;return Poll();}
};
void SchofieldMotionSession() {
    const auto open=[](Fixture& f,int loaded){
        f.s.schofield_motion=true;
        f.s.native.weapon_class="WeaponPistolSchofield";
        f.s.native.kind=f.s.admission.kind=f.s.recovery.kind=CoJManualReloadKind::clip;
        f.s.native.capacities[0]=6;
        f.s.native.ammo_readback=f.s.native.loaded[0]=loaded;
        f.s.armed_aim=At({});
        f.Open();
    };
    const auto move=[](Fixture& f,float depth){
        f.s.right.position.z+=depth;
        return f.Poll();
    };
    for(int loaded:{0,3,6}){
        Fixture f;
        open(f,loaded);
        const auto first=move(f,-.055F);
        Check(!first.cancel&&!first.unload,"short forward step closed the drum");
        const auto close=move(f,-.055F);
        Check(close.cancel&&!close.unload&&!close.supervise&&
            std::strcmp(close.reason,"schofield_forward_flick")==0,
            "forward flick must close at every load count");
        f.s.raw.fire_right=true;f.s.native.probe_status=3;(void)f.Poll();
        Check(!f.mapped.fire_right,"closing leaked a held fire input");
        f.s.native.probe_status=4;(void)f.Poll();
        Check(!f.mapped.fire_right,"native retirement replayed a held fire input");
    }
    for(int loaded:{0,3,6}){
        Fixture f;
        open(f,loaded);
        f.s.right.position.x+=.055F;Check(!f.Poll().unload,"lateral step requested unload");
        f.s.right.position.x+=.055F;Check(!f.Poll().unload,"lateral sweep requested unload");
        Check(!move(f,.055F).unload,"short rearward step unloaded");
        const auto eject=move(f,.055F);
        Check(!eject.cancel&&eject.unload==(loaded>0)&&eject.eject_empty==(loaded==0),
            "backward flick must eject without closing at every load count");
        if(loaded)f.session.NoteSchofieldUnloadConfirmed();
        Check(f.session.schofield_chambers_cleared(),"ejection receipt did not clear chamber outlines");
        f.s.native.ammo_readback=f.s.native.loaded[0]=0;
        Check(!move(f,-.055F).cancel&&!move(f,-.055F).cancel,
            "return stroke immediately closed after ejecting");
        for(int i=0;i<4;++i)(void)f.Poll();
        (void)move(f,.055F);
        Check(!move(f,.055F).eject_empty,"empty ejection replayed in the same wait");
    }
    Fixture wrist;
    open(wrist,0);
    wrist.Claim();auto bundle=wrist.Insert();
    Check(bundle.insert,"empty drum must permit a bundle");
    wrist.session.CompleteInsertion(true,true,bundle);
    wrist.s.native.ammo_readback=wrist.s.native.loaded[0]=6;
    (void)wrist.Poll();
    wrist.s.armed_aim.orientation={.70710678F,0,0,.70710678F};
    Check(wrist.Poll().unload,"backward wrist rotation could not eject a newly placed bundle");
    wrist.session.NoteSchofieldUnloadConfirmed();
    wrist.s.native.ammo_readback=wrist.s.native.loaded[0]=0;
    for(int i=0;i<4;++i)(void)wrist.Poll();
    wrist.s.armed_aim.orientation={};
    Check(wrist.Poll().cancel,"forward wrist rotation could not close without translation");
}
void ManualReloadCapability() {
    using Mode=CoJReloadMode;
    using Choice=ManualReloadPreference;
    Check(ResolveCoJReloadMode(false,false,Choice::on)==Mode::legacy_motion,"preference manufactured installed capability");
    Check(ResolveCoJReloadMode(false,false,Choice::off)==Mode::native,"missing installation metadata bypassed explicit off");
    Check(ResolveCoJReloadMode(true,false,Choice::on)==Mode::native,"unverified manual archive admitted a reload gesture");
    Check(ResolveCoJReloadMode(false,true,Choice::on)==Mode::native,"undeclared manual archive admitted a reload gesture");
    Check(ResolveCoJReloadMode(true,true,Choice::install_default)==Mode::manual,"migration disabled an installed capability");
    Check(ResolveCoJReloadMode(true,true,Choice::on)==Mode::manual,"explicit on did not enable verified capability");
    Check(ResolveCoJReloadMode(true,true,Choice::off)==Mode::native,"off routed through a patched legacy ticket");
    const std::string exe(64,'A'), archive(64,'B');
    const std::string prefix="{\n \"manualReloadInstalled\": true,\n \"codeArchiveManaged\": true,\n"
        " \"originalCodeSha256\": \"F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12\",\n"
        " \"stagedExecutableSha256\": \""+exe+"\",\n";
    const auto stage=prefix+"\"stagedCodeSha256\": \""+archive+"\"}";
    Check(CoJManualReloadStageArchiveHash(stage,exe)==archive,"valid staged archive identity rejected");
    Check(CoJManualReloadStageArchiveHash(stage,archive).empty(),"foreign executable authorized manual reload");
    Check(CoJManualReloadStageArchiveHash(prefix+"\"stagedCodeSha256\": null}",exe).empty(),"missing derivative authorized manual reload");
    Check(CoJManualReloadStageArchiveHash(prefix+"\"manualReloadInstalled\": false,\"stagedCodeSha256\": \""+archive+"\"}",exe).empty(),
        "ambiguous stage identity authorized manual reload");
}
void DisableWhileCarrying() {
    Fixture f;f.Open();f.Claim();
    f.s.raw.reload=true;f.s.raw.fire_right=true;
    f.s.enabled=false;f.s.native_cleanup_pending=true;
    auto result=f.Poll();
    Check(result.cancel&&result.transition_controls&&!result.tracked&&!result.supervise&&!result.insert&&
        !result.target.valid&&!result.gesture.cartridge_held&&!f.mapped.reload&&!f.mapped.fire_left&&!f.mapped.fire_right,
        "disable did not retire the carry and block held reload/fire");
    Check(f.session.native_cleanup_pending(),"disable lost the retained native cleanup request");
    f.s.raw.reload=f.s.raw.fire_left=f.s.raw.fire_right=false;
    Check(f.Poll().transition_controls==11U,"pending native cleanup accepted neutral input");
    f.s.enabled=true;
    Check(f.Poll().transition_controls==11U&&f.session.native_cleanup_pending(),
        "re-enabling released the fence before native cleanup succeeded");
    f.s.enabled=false;
    f.s.native_cleanup_pending=false;f.s.native={};f.s.raw.digital_available&=~8U;
    Check(f.Poll().transition_controls&8U,"unavailable Square counted as a verified release");
    Check(!f.session.native_cleanup_pending(),"verified cleanup remained queued");
    f.s.raw.digital_available|=8U;
    f.mapped=f.s.raw;
    Check(f.session.Update(f.mapped,f.s).transition_controls&8U,"duplicate pose released the disable fence");
    ++f.s.raw.input_context_generation;
    Check(f.Poll().transition_controls&8U,"replacement baseline released the disable fence");
    f.s.raw.reload=true;
    Check((f.Poll().transition_controls&8U)&&!f.mapped.reload,"held Square escaped on owner replacement");
    f.s.raw.reload=false;
    (void)f.Poll();
    f.s.raw.reload=f.s.raw.fire_left=f.s.raw.fire_right=true;
    result=f.Poll();
    Check(!result.transition_controls&&!result.start&&!result.insert&&f.mapped.reload&&f.mapped.fire_left&&f.mapped.fire_right,
        "disabled session failed to return ordinary input after retirement and a fresh release");
}
void RetiredSampleRetainsCleanup() {
    Fixture f;f.Open();f.Claim();
    const auto previous_generation=f.s.raw.input_context_generation;
    ++f.s.raw.input_context_generation;
    (void)f.Poll();
    f.s.raw.input_context_generation=previous_generation;
    f.s.enabled=false;f.s.native_cleanup_pending=true;
    const auto result=f.Poll();
    Check(result.transition_controls==11U&&f.session.native_cleanup_pending(),
        "retired pose discarded the native cancellation retry");
}
void IndexedCycleSession() {
    for(int hand:{0,1})for(bool lemant:{false,true}) {
        IndexedFixture f(hand,lemant);f.Open();f.Pickup();
        f.s.native.selected_index=0;
        auto out=f.Release();
        Check(out.insert&&out.selected_index==1&&f.session.selected_index()==1,
            "pending indexed native operation lost its captured selection");
        const auto before=f.s.native;
        ++f.s.native.loaded[1];--f.s.native.reserves[lemant?0:2];f.s.native.probe_status=1;
        Check(CoJUnitManualInsertionObserved(true,true,before,f.s.native,out.selected_index),
            "indexed replay did not prove its selected typed-pool transfer");
        f.session.CompleteInsertion(true,true,out);
        Check(f.session.selected_index()==-1,"successful indexed operation retained old selection");
        StereoHudTextOverlay guide{};
        auto old_input=f.s;old_input.native=before;
        Check(!out.gesture.cartridge_held&&!out.gesture.pickup_hint.visible&&
            !AppendCoJAmmunitionPickupGuide(old_input,out,true,guide),
            "same-frame completed indexed insertion published a stale waiting offer");
        f.SupportTrigger()=true;
        out=f.Poll();
        Check(out.tracked&&out.supervise&&!out.insert&&!out.gesture.cartridge_held&&
            !out.gesture.pickup_hint.visible&&!f.SupportFire()&&
            f.session.phase()==CoJManualReloadPhase::opening&&f.session.selected_index()==-1,
            "indexed replay offered/carried a round or released its consumed support hold");
        f.s.native.probe_status=2;
        out=f.Poll();
        Check(out.tracked&&out.supervise&&!out.gesture.cartridge_held&&!out.insert&&!f.SupportFire(),
            "return to native wait rearmed the held indexed pickup");
        // A new cartridge still requires a released waist pickup, even though
        // the last insertion ended beside the gun and another slot is eligible.
        f.SupportTrigger()=false;(void)f.Poll();f.SupportTrigger()=true;
        Check(!f.Poll().gesture.cartridge_held,"indexed cycle borrowed unit-style in-hand replenishment");
        f.Support()=At(hand==0?Vec3{-.2F,1.05F,-.1F}:Vec3{.2F,1.05F,-.1F});
        Check(!f.Poll().gesture.cartridge_held,"held trigger entering waist bypassed fresh pickup release");
        f.Pickup();Check(f.session.selected_index()==0,"next native wait retained previous pickup index");
        f.s.native.selected_index=1;
        out=f.Release();Check(out.insert&&out.selected_index==0,"new indexed carry redirected on release");
        const auto final_before=f.s.native;
        ++f.s.native.loaded[0];--f.s.native.reserves[lemant?1:2];f.s.native.probe_status=3;
        Check(CoJUnitManualInsertionObserved(true,true,final_before,f.s.native,out.selected_index),
            "indexed final close lost captured slot or typed reserve");
        f.session.CompleteInsertion(true,true,out);out=f.Poll();
        Check(out.tracked&&!out.supervise&&!out.insert&&!out.gesture.cartridge_held&&
            f.session.phase()==CoJManualReloadPhase::closing,"native indexed close offered another round");
    }
    for(int hand:{0,1})for(bool carried:{false,true}) {
        IndexedFixture f(hand,true);f.Open();if(carried)f.Pickup();
        f.s.native.probe_status=1;f.SupportTrigger()=true;
        auto out=f.Poll();
        Check(!out.gesture.cartridge_held&&!out.gesture.pickup_hint.visible&&!out.insert&&!f.SupportFire(),
            "replay failed to withdraw an indexed offer/carry while retaining support ownership");
        f.ArmedTrigger()=true;out=f.Poll();
        Check(out.cancel&&!f.mapped.fire_left&&!f.mapped.fire_right,
            "armed trigger did not close indexed replay without firing");
        f.s.enabled=false;f.s.native.probe_status=4;
        const auto support_binding=hand==0?1U:2U;
        f.s.raw.digital_available&=~support_binding;(void)f.Poll();
        Check(!f.SupportFire(),"disabled indexed cleanup released an unavailable held support trigger");
        f.s.raw.digital_available|=support_binding;
        Check(!f.Poll().insert&&!f.SupportFire(),"availability recovery rearmed consumed support fire");
        f.SupportTrigger()=false;f.ArmedTrigger()=false;(void)f.Poll();
        f.SupportTrigger()=true;f.ArmedTrigger()=true;out=f.Poll();
        Check(!out.start&&!out.cancel&&!out.insert&&f.mapped.fire_left&&f.mapped.fire_right,
            "disabled indexed session failed to recover ordinary native fire after physical releases");
    }
}
void IndexedMissRetiresSelection() {
    for(int hand:{0,1}) {
        IndexedFixture f(hand,true);
        f.Open();
        f.Pickup();
        Check(f.session.selected_index()==1,"fixture did not capture the pistol reserve slot");
        // The player releases at the waist, then presses again on the very
        // next sample. The release consumes input but ends the carry.
        f.SupportTrigger()=false;
        auto out=f.Poll();
        Check(!out.insert&&!out.gesture.cartridge_held&&f.session.selected_index()==-1,
            "missed indexed release retained the old slot for the next pickup");
        f.s.native.selected_index=0;
        f.SupportTrigger()=true;
        out=f.Poll();
        Check(out.gesture.cartridge_held&&f.session.selected_index()==0,
            "next indexed pickup did not acquire the new native slot");
    }
}
void IndexedReleaseBarriers() {
    for(int hand:{0,1})for(int boundary=0;boundary<6;++boundary) {
        IndexedFixture f(hand,true);f.Open();f.Pickup();
        f.ArmedTrigger()=true;Check(f.Poll().cancel,"release barrier fixture failed to close");
        f.s.enabled=false;f.s.native.probe_status=4;(void)f.Poll();
        const auto saved_sequence=f.s.sequence,saved_now=f.s.now;
        const auto binding=hand==0?1U:2U;
        f.SupportTrigger()=false;
        if(boundary==0){f.mapped=f.s.raw;(void)f.session.Update(f.mapped,f.s);} // Duplicate release.
        if(boundary==1){f.s.raw.digital_available&=~binding;(void)f.Poll();}
        if(boundary==2){f.s.raw.active=false;(void)f.Poll();}
        if(boundary==3){f.s.now+=300;(void)f.Poll();}
        if(boundary==4){f.s.sequence=0;(void)f.Poll();}
        if(boundary==5){f.SupportTrigger()=true;f.ArmedTrigger()=false;(void)f.Poll();}
        f.SupportTrigger()=true;f.s.raw.active=true;f.s.raw.digital_available|=binding;
        f.s.raw.input_context_generation=8;
        f.s.sequence=std::max(f.s.sequence,saved_sequence);f.s.now=std::max(f.s.now,saved_now);
        (void)f.Poll();Check(!f.SupportFire(),"invalid/other-hand release discharged the old support hold");
        f.SupportTrigger()=false;auto release=f.Poll();
        Check(!f.SupportFire()&&!release.insert,"physical release sample leaked fire or insertion");
        f.SupportTrigger()=true;(void)f.Poll();
        Check(f.SupportFire(),"same-source fresh available release failed to recover native support fire");
    }
}
void IndexedReplacementSource() {
    for(int hand:{0,1})for(int boundary=0;boundary<4;++boundary)for(bool probe_hold:{false,true}) {
        IndexedFixture f(hand,true);f.Open();f.Pickup();
        f.s.sequence=1000;f.ArmedTrigger()=true;Check(f.Poll().cancel,"replacement fixture failed to close");
        f.s.enabled=false;f.s.native.probe_status=4;(void)f.Poll();
        const auto old_context=f.s.context,old_player=f.s.player,old_generation=f.s.raw.input_context_generation;
        if(boundary==0)++f.s.raw.input_context_generation;
        if(boundary==1)f.s.context+=2;
        if(boundary==2){f.s.context+=2;f.s.raw.input_context_generation=1;}
        if(boundary==3)++f.s.player;
        const auto new_context=f.s.context,new_player=f.s.player,new_generation=f.s.raw.input_context_generation;
        f.s.sequence=0;f.SupportTrigger()=false;auto baseline=f.Poll();
        Check(!f.SupportFire()&&!baseline.start&&!baseline.insert,"replacement baseline released physical input authority");
        f.mapped=f.s.raw;auto duplicate=f.session.Update(f.mapped,f.s);
        Check(!f.SupportFire()&&!duplicate.insert,"duplicate replacement baseline released support hold");
        if(probe_hold) {
            f.SupportTrigger()=true;(void)f.Poll();
            Check(!f.SupportFire(),"replacement baseline false discharged the old support claim");
            // An old tuple with a greater sequence cannot replace the current source.
            f.s.context=old_context;f.s.player=old_player;f.s.raw.input_context_generation=old_generation;
            const auto seq=f.s.sequence;f.s.sequence=5000;f.SupportTrigger()=false;(void)f.Poll();
            f.s.context=new_context;f.s.player=new_player;f.s.raw.input_context_generation=new_generation;
            f.s.sequence=seq;f.SupportTrigger()=true;(void)f.Poll();
            Check(!f.SupportFire(),"retired source release discharged current support hold");
        }
        f.SupportTrigger()=false;f.ArmedTrigger()=false;const auto release=f.Poll();
        const auto support_mask=hand==0?2U:1U;
        Check(!f.SupportFire()&&!release.insert&&(release.gesture.consumed_trigger_mask&support_mask)!=0,
            "fresh replacement release failed to consume its own sample");
        f.SupportTrigger()=true;f.ArmedTrigger()=true;const auto ordinary=f.Poll();
        Check(f.mapped.fire_left&&f.mapped.fire_right&&!ordinary.insert&&!ordinary.start,
            "fresh replacement release failed to recover native fire below old sequence watermark");
        const auto seq=f.s.sequence;
        f.s.context=old_context;f.s.player=old_player;f.s.raw.input_context_generation=old_generation;
        f.s.sequence=5000;f.s.enabled=true;f.s.raw.reload=false;(void)f.Poll();f.s.raw.reload=true;
        auto retired=f.Poll();
        Check(!retired.start&&!retired.insert&&!retired.gesture.cartridge_held&&
            !f.mapped.fire_left&&!f.mapped.fire_right&&!f.mapped.reload,
            "retired source reacquired manual/native input authority after recovery");
        f.s.context=new_context;f.s.player=new_player;f.s.raw.input_context_generation=new_generation;
        f.s.sequence=seq;f.s.enabled=false;f.s.raw.reload=false;(void)f.Poll();
        Check(f.mapped.fire_left&&f.mapped.fire_right,"retired held sample recreated a current trigger claim");
        f.s.enabled=true;f.s.admission=f.s.recovery;
        ++f.s.raw.input_context_generation;f.s.sequence=0;
        f.SupportTrigger()=f.ArmedTrigger()=false;(void)f.Poll(); // New neutral baseline.
        f.s.raw.reload=true;
        Check(!f.Poll().start&&!f.mapped.reload,"replacement baseline rearmed Square without a fresh release");
        f.s.raw.reload=false;(void)f.Poll();f.s.raw.reload=true;
        Check(f.Poll().start,"current replacement source failed to rearm Square after fresh release");
    }
}
bool HasCurrentGestureTarget(const CoJManualReloadOutput& out,const CoJManualReloadInput& s){
    const auto p=s.native.armed_hand==0?s.right.position:s.left.position;
    return out.target.valid&&out.target.sequence==s.sequence&&out.target.observed_ms==s.now&&
        out.target.player==s.player&&out.target.context==s.context&&out.target.weapon==s.native.weapon_id&&
        out.target.input_generation==s.raw.input_context_generation&&out.target.armed_hand==s.native.armed_hand&&
        out.target.position.x==p.x&&out.target.position.y==p.y&&out.target.position.z==p.z;
}
void CurrentGestureTarget(){
    Fixture f;f.Open();f.Claim();const auto out=f.Poll();
    Check(HasCurrentGestureTarget(out,f.s),
        "manual session must publish the same current tracking-space target used for insertion");
}
void ManualStartRequiresNativeRelease() {
    for (bool empty_trigger:{false,true}) {
        Fixture f;
        if(empty_trigger) f.s.native.ammo_readback=0;
        (void)f.Poll();
        // A prior native action31 press remains applied after a failed release.
        // No ticket can be delivered until that native edge is neutral.
        f.s.native_reload_held=true;
        f.s.raw.reload=!empty_trigger;
        f.s.raw.fire_right=empty_trigger;
        const auto blocked=f.Poll();
        Check(!blocked.start&&!f.mapped.reload&&f.session.phase()==CoJManualReloadPhase::ready,
            "manual opening acquired a native action still held from an earlier request");
        f.s.native_reload_held=false;
        Check(!f.Poll().start,"held input reopened after native release without a fresh edge");
        f.s.raw.reload=f.s.raw.fire_right=false;
        (void)f.Poll();
        f.s.raw.reload=!empty_trigger;
        f.s.raw.fire_right=empty_trigger;
        Check(f.Poll().start,"fresh manual edge failed after both releases");
    }
}
void AllProfileTargetGuides(){
    for(const auto& profile:kCoJRevolvers)for(int hand=0;hand<2;++hand){
        if(profile.right_owned_only&&hand==1)continue;
        IndexedFixture f(hand,false);
        f.s.native.kind=f.s.admission.kind=f.s.recovery.kind=profile.kind;
        f.s.native.weapon_class=profile.class_name;
        f.s.native.weapon_id=f.s.admission.weapon_id=f.s.recovery.weapon_id=0x80000042ULL;
        f.Open();f.Pickup();auto out=f.Poll();
        StereoHudTextOverlay hud{};hud.frame_sequence=77;hud.reload_feedback.owner_token=12;
        Check(AppendCoJManualReloadTargetGuide(f.s,out,77,true,hud)&&hud.holster_guides.count==1,
            "every exact manual family needs its actual gesture target, including Schofield");
        const auto& guide=hud.holster_guides.zones[0];Vec3 centre{};
        for(const auto p:guide.head_corners){centre.x+=p.x*.25F;centre.y+=p.y*.25F;centre.z+=p.z*.25F;}
        Check(std::abs(centre.x-out.target.position.x)<.00001F&&
            std::abs(centre.y-(out.target.position.y-f.s.head.position.y))<.00001F&&
            std::abs(centre.z-out.target.position.z)<.00001F,
            "guide centre differs from the exact tracking-space gesture target");
    }
}
void TargetGuideFreshnessAndReadiness(){
    Fixture f;f.s.native.weapon_class="WeaponPistolPeacemaker";f.Open();
    Check(!f.Poll().target.valid,"waiting offer must not claim a carried insertion target");
    f.Claim();auto result=f.Poll();
    Check(HasCurrentGestureTarget(result,f.s),"initial carry target lost source identity");
    const auto original=f.s;
    StereoHudTextOverlay hud{};hud.frame_sequence=77;hud.reload_feedback.owner_token=12;
    for(int fault=0;fault<30;++fault){
        auto s=original;auto r=result;auto h=hud;bool enabled=true;std::uint64_t capture=77;
        switch(fault){
        case 0:enabled=false;break;case 1:r.target.valid=false;break;
        case 2:s.sequence++;break;case 3:s.now++;break;case 4:s.player++;break;
        case 5:s.context+=2;break;case 6:s.raw.input_context_generation++;break;
        case 7:s.native.weapon_id++;break;case 8:s.native.armed_hand=1;break;
        case 9:s.native.probe_status=1;break;case 10:s.native.reserve_readback=0;break;
        case 11:s.native.weapon_class="WeaponPistolSchofield_Unknown";break;
        case 12:s.native.kind=CoJManualReloadKind::clip;break;
        case 13:r.target.position.x=std::numeric_limits<float>::quiet_NaN();break;
        case 14:r.target.position.x+=.01F;break;
        case 15:s.right.position.x+=.01F;break;
        case 16:s.left.orientation_valid=false;break;
        case 17:s.head.position_valid=false;break;
        case 18:h.reload_feedback.owner_token=0;break;
        case 19:capture=0;break;case 20:capture++;break;
        case 21:h.frame_sequence=0;break;
        case 22:h.holster_guides.count=h.holster_guides.zones.size();break;
        case 23:r.cancel=true;break;case 24:r.tracked=false;break;
        case 25:r.gesture.cartridge_held=false;break;
        case 26:r.gesture.cartridge_hand=0;break;
        case 27:s.recentered=true;break;case 28:s.enabled=false;break;
        case 29:s.allowed=false;break;
        }
        const auto count=h.holster_guides.count;
        Check(!AppendCoJManualReloadTargetGuide(s,r,capture,enabled,h)&&h.holster_guides.count==count,
            "invalid/stale/forged target guide escaped without atomic rejection");
    }
    // Policy readiness drives the marker; visual size grants no admission.
    Check(!result.gesture.insertion_ready&&AppendCoJManualReloadTargetGuide(f.s,result,77,true,hud)&&
        hud.holster_guides.zones[0].state==0,"unready carry marker claimed readiness");
    f.Carry(f.s.left,At({f.s.right.position.x+.179F,f.s.right.position.y,f.s.right.position.z}));
    result=f.Poll();hud.holster_guides={};
    Check(result.gesture.insertion_ready&&HasCurrentGestureTarget(result,f.s)&&
        AppendCoJManualReloadTargetGuide(f.s,result,77,true,hud)&&hud.holster_guides.zones[0].state==1,
        "policy-ready carry did not publish a ready current target");
    f.s.raw.fire_left=false;result=f.Poll();
    Check(result.insert&&!result.target.valid,"release must withdraw its visual target on insertion intent");
    f.session.CompleteInsertion(true,true,result);
    Check(!result.target.valid,"completed native transfer retained its old guide receipt");
    result=f.Poll();Check(result.target.valid,"native-confirmed replenishment lost its current guide");
    auto duplicate=f.session.Update(f.mapped,f.s);
    Check(!duplicate.target.valid,"duplicate pose observation republished a new target receipt");
    f.s.native.reserve_readback=0;Check(!f.Poll().target.valid,"depleted native reserve retained an active target");

    Fixture moved;moved.s.native.weapon_class="WeaponPistolPeacemaker";moved.Open();moved.Claim();
    auto before=moved.Poll();moved.s.right.position.x+=.05F;auto after=moved.Poll();
    Check(after.target.valid&&after.target.position.x==moved.s.right.position.x&&
        after.target.position.x!=before.target.position.x,"weapon movement reused last-frame target");
    moved.s.recentered=true;Check(!moved.Poll().target.valid,"cancelled tracking kept a guide target");

    Fixture rotated;rotated.s.native.weapon_class="WeaponPistolPeacemaker";rotated.Open();rotated.Claim();
    rotated.s.head.orientation={0,0,.38268343F,.92387953F};
    result=rotated.Poll();hud.holster_guides={};
    Check(AppendCoJManualReloadTargetGuide(rotated.s,result,77,true,hud),"head rotation hid a current front target");
    const auto q=NormalizeQuaternion(rotated.s.head.orientation);
    const auto p=result.target.position,h=rotated.s.head.position;
    const auto expected=RotateVector({-q.x,-q.y,-q.z,q.w},{p.x-h.x,p.y-h.y,p.z-h.z});
    Vec3 actual{};for(const auto corner:hud.holster_guides.zones[0].head_corners){
        actual.x+=corner.x*.25F;actual.y+=corner.y*.25F;actual.z+=corner.z*.25F;}
    Check(std::abs(actual.x-expected.x)<.00001F&&std::abs(actual.y-expected.y)<.00001F&&
        std::abs(actual.z-expected.z)<.00001F,"guide applied head translation/rotation twice or omitted it");
}
void ReloadPressedDuringSchofieldShot() {
    for(int outcome:{0,1,2,3,4,5,6}) {
        Fixture f;
        f.s.native.kind=CoJManualReloadKind::clip;
        f.s.native.ammo_readback=3;
        f.s.native.probe_status=4;
        const CoJMotionReloadOwner idle{0,42,true,false,CoJManualReloadKind::clip,true};
        f.s.admission=idle;
        (void)f.Poll();
        f.s.admission={};
        f.s.after_shot=idle;
        f.s.native_reload_held=outcome==5;
        f.s.raw.reload=true;
        Check(!f.Poll().start&&!f.mapped.reload,"shot tail emitted reload before idle");
        f.s.raw.reload=false;
        Check(!f.Poll().start,"button release during the shot dispatched reload");
        if(outcome==1)f.s.native.weapon_id=43;
        if(outcome==2)for(int i=0;i<10;++i)(void)f.Poll();
        if(outcome==3) {
            f.s.raw.digital_available&=~8U;
            (void)f.Poll();
            f.s.raw.digital_available|=8U;
        }
        if(outcome==4) {
            f.s.raw.fire_right=true;
            (void)f.Poll();
            f.s.raw.fire_right=false;
        }
        f.s.after_shot={};
        f.s.admission=idle;
        f.s.admission.weapon_id=f.s.native.weapon_id;
        f.s.native_reload_held=outcome==6;
        const auto result=f.Poll();
        Check(result.start==(outcome==0)&&f.mapped.reload==(outcome==0),
            "shot-tail reload intent was lost or survived its ownership/input fence");
        f.s.native_reload_held=false;
        Check(!f.Poll().start&&!f.mapped.reload,"deferred reload dispatched again or after its native edge was blocked");
    }
}
int main(){
    ManualReloadCapability();
    DisableWhileCarrying();
    RetiredSampleRetainsCleanup();
    ReloadPressedDuringSchofieldShot();
    {
        Fixture outside;
        outside.Open();outside.Claim();
        auto target=outside.s.right;
        target.position.x+=.181F;
        outside.Carry(outside.s.left,target);
        Check(!outside.Poll().gesture.insertion_ready,"carry beyond 18 cm signaled readiness");
        outside.s.raw.fire_left=false;
        Check(!outside.Poll().insert,"release beyond 18 cm inserted ammunition");
    }
    CurrentGestureTarget();
    ManualStartRequiresNativeRelease();
    AllProfileTargetGuides();
    TargetGuideFreshnessAndReadiness();
    IndexedReplacementSource();
    IndexedReleaseBarriers();
    IndexedCycleSession();
    IndexedMissRetiresSelection();
    {
        Fixture indexed;
        indexed.s.native.kind=indexed.s.admission.kind=indexed.s.recovery.kind=CoJManualReloadKind::indexed;
        indexed.s.admission.single_round_supported=indexed.s.recovery.single_round_supported=false;
        indexed.s.admission.manual_supported=indexed.s.recovery.manual_supported=true;
        indexed.s.native.index_count=2;indexed.s.native.selected_index=1;
        indexed.s.native.capacities={1,8};indexed.s.native.loaded={0,2};
        indexed.s.native.reserves={10,10,0};
        indexed.s.native.reserve_kinds={CoJReloadReserveKind::rifle,CoJReloadReserveKind::pistol};
        indexed.Open();indexed.Claim();
        indexed.s.native.selected_index=0; // A later suggestion cannot redirect the held unit.
        auto inserted=indexed.Insert();
        Check(inserted.insert&&inserted.selected_index==1,
            "indexed pickup did not retain its selected native ammo slot");
        indexed.session.CompleteInsertion(true,true,inserted);
        Check(indexed.session.selected_index()==-1,"completed indexed insertion retained old selection");
    }
    {
        Fixture indexed;
        indexed.s.native.kind=indexed.s.admission.kind=indexed.s.recovery.kind=CoJManualReloadKind::indexed;
        indexed.s.native.index_count=2;indexed.s.native.selected_index=1;
        indexed.s.native.capacities={1,8};indexed.s.native.loaded={0,2};
        indexed.s.native.reserves={10,10,0};
        indexed.s.native.reserve_kinds={CoJReloadReserveKind::rifle,CoJReloadReserveKind::pistol};
        indexed.Open();indexed.Claim();
        indexed.s.native.loaded[1]=8;indexed.s.native.selected_index=0;
        const auto lost=indexed.Poll();
        Check(!lost.insert&&!lost.gesture.cartridge_held,
            "full captured slot silently redirected its cartridge to another eligible slot");
        indexed.s.left=indexed.s.right;indexed.s.raw.fire_left=false;
        Check(!indexed.Poll().insert,"invalidated indexed carry inserted on physical release");
    }
    {
        Fixture indexed;indexed.s.native.kind=indexed.s.admission.kind=CoJManualReloadKind::indexed;
        indexed.s.native.ammo_readback=0;(void)indexed.Poll();indexed.s.raw.fire_right=true;
        Check(!indexed.Poll().start,"indexed empty trigger inferred native fire index from ammo0");
    }
    {
        Fixture clip;clip.s.native.kind=clip.s.admission.kind=clip.s.recovery.kind=CoJManualReloadKind::clip;
        clip.s.admission.single_round_supported=clip.s.recovery.single_round_supported=false;
        clip.s.admission.manual_supported=clip.s.recovery.manual_supported=true;
        clip.Open();clip.Claim();auto inserted=clip.Insert();Check(inserted.insert,"clip gesture missing");
        clip.session.CompleteInsertion(true,true,inserted);
        Check(clip.session.phase()==CoJManualReloadPhase::closing&&!inserted.supervise,
            "successful clip insertion incorrectly offered another unit");
    }
    {
        Fixture cycle;cycle.Open();cycle.Claim();
        auto inserted=cycle.Insert();Check(inserted.insert,"missing cycle insertion");
        cycle.session.CompleteInsertion(true,true,inserted);
        cycle.s.native.probe_status=1; // Native body replay after a unit.
        const auto replay=cycle.Poll();
        Check(replay.tracked&&replay.supervise&&!replay.insert&&!replay.gesture.cartridge_held&&
            cycle.session.phase()==CoJManualReloadPhase::opening,
            "native per-cartridge body replay still offers a waiting-state cartridge");
        cycle.s.native.probe_status=2;(void)cycle.Poll();
        Check(cycle.session.phase()==CoJManualReloadPhase::manual_load,
            "body replay failed to return to supervised manual loading");
    }
    for(int hand:{0,1}) {
        Fixture empty;empty.s.admission.armed_hand=empty.s.recovery.armed_hand=empty.s.native.armed_hand=hand;
        empty.s.native.ammo_readback=0;empty.s.native.reserve_readback=6;
        (void)empty.Poll();
        (hand==0?empty.s.raw.fire_right:empty.s.raw.fire_left)=true;
        auto out=empty.Poll();
        Check(out.start&&empty.mapped.reload&&!empty.mapped.fire_left&&!empty.mapped.fire_right&&
            std::strcmp(out.reason,"empty_trigger")==0,"empty armed trigger bypassed manual preparation");
        empty.session.NoteStart(true);
        empty.s.native.probe_status=1;empty.s.native.reloading=true;
        empty.s.native.states[0]={20,21,21,21};
        out=empty.Poll();
        Check(!out.start&&!out.cancel&&empty.session.phase()==CoJManualReloadPhase::opening,
            "the opening empty-trigger hold immediately closed its own reload");
        for(unsigned i=0;i<3;++i)Check(!empty.Poll().cancel,"continued opening hold closed manual session");
        (hand==0?empty.s.raw.fire_right:empty.s.raw.fire_left)=false;
        (void)empty.Poll();
        (hand==0?empty.s.raw.fire_right:empty.s.raw.fire_left)=true;
        out=empty.Poll();
        Check(out.cancel&&std::strcmp(out.reason,"fire_close")==0,
            "release and fresh armed trigger failed to close the prepared reload");
    }
    {
        Fixture ordinary;ordinary.s.admission.single_round_supported=false;
        ordinary.s.native_fallback_allowed=true;(void)ordinary.Poll();ordinary.s.raw.fire_right=true;
        const auto out=ordinary.Poll();Check(!out.start&&ordinary.mapped.fire_right,
            "manual empty-trigger path stole unsupported native fire/reload");
    }
    // A 90 Hz delivery clock must distinguish movement between unique poses.
    // Reproduce coarse 15.6 ms ticks: the carry cancels before reaching the gun.
    // With the same positions/sequence and QPC millisecond delivery it survives
    // and emits a single insertion intent on release (never native ammo credit).
    for(bool coarse:{true,false}) {
        Fixture f;f.Open();f.Claim();
        const auto begin=f.s.left.position;const auto end=f.s.right.position;
        const auto base=f.s.now;bool retained=true;CoJManualReloadOutput out;
        for(unsigned step=1;step<=20;++step) {
            const float t=step/20.F;f.s.left=At({begin.x+(end.x-begin.x)*t,
                begin.y+(end.y-begin.y)*t,begin.z+(end.z-begin.z)*t});
            ++f.s.sequence;const auto elapsed=step*11ULL;
            f.s.now=base+(coarse?(elapsed/16)*16:elapsed);f.mapped=f.s.raw;
            out=f.session.Update(f.mapped,f.s);retained&=out.gesture.cartridge_held;
        }
        f.s.raw.fire_left=false;++f.s.sequence;f.s.now+=11;f.mapped=f.s.raw;
        out=f.session.Update(f.mapped,f.s);
        Check(coarse?(!retained&&!out.insert):(retained&&out.insert),"coarse-clock carry regression / high-resolution insertion");
    }
    // A visible round is an interaction offer, not inventory. Do not offer or
    // insert one when the current native waiting snapshot has no reserve.
    for(const int reserve:{0,-1}){
        Fixture empty;empty.Open();empty.s.native.reserve_readback=reserve;
        (void)empty.Poll();empty.s.raw.fire_left=true;
        auto out=empty.Poll();
        Check(out.tracked&&out.supervise&&!out.gesture.cartridge_held&&!out.insert&&
            !out.gesture.pickup_hint.visible&&!empty.mapped.fire_left,
            "empty reserve offered a physical cartridge or released the manual trigger claim");
        empty.s.native.reserve_readback=2;
        Check(!empty.Poll().gesture.cartridge_held,"reserve recovery rearmed a held pickup trigger");
        empty.s.raw.fire_left=false;(void)empty.Poll();empty.s.raw.fire_left=true;
        Check(empty.Poll().gesture.cartridge_held,"fresh pickup failed after native reserve recovery");
        empty.s.native.reserve_readback=0;
        out=empty.Poll();
        Check(!out.gesture.cartridge_held&&!out.insert&&!empty.mapped.fire_left,
            "reserve loss retained a carried cartridge or let its trigger fire");
        empty.s.raw.fire_left=false;
        Check(!empty.Poll().insert,"release after reserve loss requested a native insertion");
    }
    // Native completion may skip the observed closing sample (e.g. the last
    // unit fills the gun). A continuation token belongs only to that session.
    for(const auto finished:{0,4}){
        Fixture completed;completed.Open();completed.Claim();
        Check(completed.Insert().insert,"last insertion requested before native completion");
        completed.session.NoteInsertion(true);
        completed.s.native.probe_status=finished;
        (void)completed.Poll();
        Check(completed.session.phase()==CoJManualReloadPhase::ready,"native direct completion reaches ready");
        completed.s.admission={0,42,true,true};
        completed.Open(); // Support remains at the gun, well outside the waist.
        completed.s.raw.fire_left=true;
        Check(!completed.Poll().gesture.cartridge_held,
            "new manual session cannot borrow a replenishment token from native direct completion");
        completed.s.raw.fire_left=false;
        Check(!completed.Poll().insert,"old continuation cannot load a round in a new session");
    }
    for(int result=0;result<3;++result){
        Fixture x;x.Open();x.Claim();auto intent=x.Insert();
        Check(intent.insert&&intent.tracked,"insertion begins with an owned intent");
        const bool completed=result!=0,accepted=result==2;
        x.session.CompleteInsertion(completed,accepted,intent);
        Check(intent.insert,"native outcome must retain the already-consumed insertion intent for diagnostics");
        if(accepted){
            Check(!intent.cancel&&intent.tracked&&intent.supervise&&
                x.session.phase()==CoJManualReloadPhase::manual_load,
                "successful native insertion preserves the tracked wait");
            Check(x.Poll().gesture.cartridge_held,"success grants one fresh continuation token");
        }else{
            const char* why=completed?"native_insertion_rejected":"native_insertion_failed";
            Check(intent.cancel&&!intent.tracked&&!intent.supervise&&std::strcmp(intent.reason,why)==0&&
                x.session.phase()==CoJManualReloadPhase::closing,
                "synchronous native failure yields tracked ownership and identifies rejection versus invocation fault");
            Check(!x.Poll().insert&&!x.Poll().supervise,"failed insertion cannot queue a retry or renew the watchdog");
        }
    }
    Fixture disabled;disabled.Open();disabled.s.raw.reload=true;
    Check(disabled.Poll().cancel,"Square closes the owned session before disable");
    disabled.s.enabled=false;disabled.s.native.probe_status=4;
    (void)disabled.Poll();disabled.s.raw.reload=false;(void)disabled.Poll();
    disabled.s.raw.reload=true;
    Check(!disabled.Poll().start&&disabled.mapped.reload,
        "disabling manual reload restores native Square after the physical release");
    // Square must not borrow a release from another input owner or from before
    // availability was lost. Otherwise a held button opens on menu/tracking return.
    for(int boundary=0;boundary<10;++boundary){
        Fixture x;(void)x.Poll();
        if(boundary==0)x.s.raw.input_context_generation++;
        if(boundary==1)x.s.player++;
        if(boundary==2)x.s.context+=2;
        if(boundary==8)x.s.native.weapon_id=x.s.admission.weapon_id=x.s.recovery.weapon_id=43;
        if(boundary==9){x.s.now+=300;}
        if(boundary>=3&&boundary<8){
            if(boundary==3)x.s.raw.digital_available&=~8U;
            if(boundary==4)x.s.allowed=false;
            if(boundary==5)x.s.left.position_valid=false;
            if(boundary==6)x.s.enabled=false;
            if(boundary==7)x.s.raw.weapon_radial=true;
            (void)x.Poll();
            x.s.raw.digital_available|=8U;x.s.allowed=true;x.s.left.position_valid=true;
            x.s.enabled=true;x.s.raw.weapon_radial=false;
        }
        x.s.raw.reload=true;
        Check(!x.Poll().start,"held Square cannot open across owner/availability boundary");
        Check(!x.Poll().start,"held Square stays disarmed on subsequent fresh samples");
        x.s.raw.reload=false;(void)x.Poll();x.s.raw.reload=true;
        Check(x.Poll().start,"a fresh available release rearms Square in the recovered owner");
    }
    Fixture f;f.Open();f.Claim();auto inserted=f.Insert();Check(inserted.insert,"valid release inserts");
    Check(!f.mapped.reload,"insertion must not dispatch native action31");f.session.NoteInsertion(true);
    Check(f.Poll().gesture.cartridge_held,"accepted insertion replenishes visual token");
    f.s.raw.fire_left=true;(void)f.Poll();f.s.raw.fire_left=false;Check(f.Poll().insert,"second fresh release inserts once");
    auto duplicate=f.session.Update(f.mapped,f.s);Check(!duplicate.insert,"duplicate cannot reinsert");
    f.session.NoteInsertion(true);(void)f.Poll();f.s.raw.reload=true;
    Check(f.Poll().cancel&&!f.mapped.reload,"second Square closes without conventional reload");
    f.s.native.probe_status=3;f.s.native.states[0]={22,1,1,1};(void)f.Poll();
    f.s.native.probe_status=4;f.s.native.states[0]={1,1,1,1};f.s.admission={0,42,true,true};
    Check(!f.Poll().start&&f.session.phase()==CoJManualReloadPhase::ready,"held Square cannot reopen after close");
    f.s.raw.reload=false;(void)f.Poll();f.s.raw.reload=true;Check(f.Poll().start,"fresh Square rearms");
    for(int reason=0;reason<13;++reason){Fixture x;x.Open();x.Claim();
        if(reason==0)x.s.allowed=false;
        if(reason==1)x.s.left.position_valid=false;
        if(reason==2)x.s.player++;
        if(reason==3)x.s.context++;
        if(reason==4)x.s.recentered=true;
        if(reason==5)x.s.raw.quick_load=true;
        if(reason==6)x.s.raw.fire_right=true;
        if(reason==7)x.s.enabled=false;
        if(reason==8)x.s.native.weapon_id++;
        if(reason==9)x.s.raw.input_context_generation++;
        if(reason==10)x.s.raw.quick_save=true;
        if(reason==11)x.s.raw.weapon_next=true;
        if(reason==12)x.s.raw.weapon_radial=true;
        const auto out=x.Poll();Check(out.cancel&&!out.insert&&!out.tracked,"interruption must cancel and yield");
        Check(!x.mapped.fire_left,"held support claim stays suppressed on cancellation");
        if(reason==6)Check(!x.mapped.fire_right,"close by fire cannot shoot until release");
    }
    Fixture mirrored;mirrored.s.right=At({.2F,1.05F,-.1F});mirrored.s.left=At({-.2F,1.35F,-.35F});
    mirrored.s.admission={1,42,true,true};mirrored.s.recovery=mirrored.s.admission;mirrored.s.native.armed_hand=1;
    mirrored.s.native.states[0]={};mirrored.s.native.states[1]={1,1,1,1};(void)mirrored.Poll();
    mirrored.s.raw.reload=true;Check(mirrored.Poll().start,"left armed Square opens");mirrored.session.NoteStart(true);
    mirrored.s.raw.reload=false;mirrored.s.admission={};mirrored.s.native.probe_status=2;
    mirrored.s.native.states[1]={21,21,21,21};(void)mirrored.Poll();
    mirrored.s.raw.fire_right=true;Check(mirrored.Poll().gesture.cartridge_held,"mirrored free trigger picks up");
    mirrored.Carry(mirrored.s.right,mirrored.s.left);mirrored.s.raw.fire_right=false;
    Check(mirrored.Poll().insert&&!mirrored.mapped.reload,"mirrored release inserts without action31");
    Fixture miss;miss.Open();miss.Claim();miss.Carry(miss.s.left,At({-1,1.2F,0}));miss.s.raw.fire_left=false;
    Check(!miss.Poll().insert,"dropped cartridge is not inserted");
    Fixture full;full.Open();full.s.native.probe_status=3;full.s.native.states[0]={22,1,1,1};
    Check(full.Poll().tracked&&full.session.phase()==CoJManualReloadPhase::closing,"coherent native close keeps VR presentation");
    Fixture failed;failed.Open();failed.Claim();Check(failed.Insert().insert,"request before reject");
    failed.session.NoteInsertion(false);const auto failure=failed.Poll();
    Check(failure.cancel,"native insertion failure closes, never retries");
    Check(std::strcmp(failure.reason,"native_operation_failed")==0,
        "cancellation must retain the native rejection reason after clearing pending failure");
    Check(!failed.Poll().supervise,"failed closing must not renew a still-waiting native watchdog");
    Fixture retry;retry.Open();retry.session.NoteInsertion(false);retry.session.Cancel();
    retry.s.native.probe_status=3;(void)retry.Poll();retry.s.native.probe_status=4;(void)retry.Poll();
    retry.s.admission={0,42,true,true};retry.s.raw.reload=false;(void)retry.Poll();retry.s.raw.reload=true;
    Check(retry.Poll().start,"failed insertion permits fresh reopening");retry.session.NoteStart(true);
    retry.s.raw.reload=false;retry.s.native.probe_status=1;
    Check(!retry.Poll().cancel,"old insertion rejection must not cancel a fresh session");
    Fixture unavailable;unavailable.Open();unavailable.Claim();unavailable.s.raw.digital_available&=~1U;
    Check(unavailable.Poll().cancel,"unavailable support trigger must cancel supervision");
    unavailable.s.native.probe_status=4;(void)unavailable.Poll();
    Check(!unavailable.mapped.fire_left,"unavailable trigger cannot satisfy release barrier");
    Fixture unsupported;(void)unsupported.Poll();unsupported.s.admission.single_round_supported=false;unsupported.s.native_fallback_allowed=true;unsupported.s.raw.reload=true;
    Check(!unsupported.Poll().start&&unsupported.mapped.reload,"unsupported keeps native Square");
    Fixture stale;stale.Open();stale.s.now+=300;Check(stale.Poll().cancel,"tracking publication gap closes");
    for(int fault=0;fault<3;++fault){
        Fixture x;x.Open();
        if(fault==0)x.s.recentered=true;
        if(fault==1)x.s.now+=300;
        if(fault==2)x.s.sequence=0;
        const auto cancelled=x.Poll();
        const char* reason=fault==0?"recentered":fault==1?"sample_gap":"sample_regression";
        Check(cancelled.cancel&&std::strcmp(cancelled.reason,reason)==0,
            "pose discontinuity cancellation must distinguish recenter, gap and regression");
    }
    for(int fault=0;fault<7;++fault){Fixture x;(void)x.Poll();x.s.raw.reload=true;
        if(fault==0)x.s.native.valid=false;
        if(fault==1)x.s.native.probe_valid=false;
        if(fault==2)x.s.left.orientation_valid=false;
        if(fault==3)x.s.now+=300;
        if(fault==4)x.s.sequence=0;
        if(fault==5)x.s.admission={};
        if(fault==6)x.s.allowed=false;
        Check(!x.Poll().start&&!x.mapped.reload,"failed manual admission must never fall through to native Square");
        x.s.native.valid=x.s.native.probe_valid=true;x.s.left.orientation_valid=true;x.s.allowed=true;
        x.s.admission={0,42,true,true};
        Check(!x.Poll().start&&!x.mapped.reload,"held rejected Square cannot open or fall through after recovery");
        x.s.raw.reload=false;(void)x.Poll();x.s.raw.reload=true;
        Check(x.Poll().start,"fresh Square recovers after admission failure");
    }
    SchofieldMotionSession();
    std::cout<<"manual reload session passed\n";
}
