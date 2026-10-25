#include "games/call_of_juarez/long_gun_grip.hpp"
#include "games/call_of_juarez/long_gun_fire.hpp"
#include "games/call_of_juarez/long_gun_guide.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::games::call_of_juarez;
using namespace cojvr::runtime;
namespace {
namespace runtime=cojvr::runtime;
void Check(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
bool Near(Vec3 a,Vec3 b){return std::abs(a.x-b.x)<.002F&&std::abs(a.y-b.y)<.002F&&std::abs(a.z-b.z)<.002F;}
void ContactAt(CoJLongGunSupportSnapshot& n,Vec3 point){
    const auto p=CoJLongGunContact(n.weapon_class).point_cm;
    n.contact_element=8;n.contact_anchor={{point.x-p.x,point.y-p.y,point.z-p.z},{0,1,0},{0,0,1},true};
}
CoJLongGunSupportSnapshot Native(){
    CoJLongGunSupportSnapshot n{};n.valid=true;n.object_epoch=1;n.weapon_class="WeaponRifleWinchester";
    n.player_id=0x81234567;n.weapon_id=0xf1234567;n.holder_element=4;n.support_element=10;
    n.states[0]={1,1,1,1};n.two_handed[0]={true,true,true};
    n.root={{100,200,300},{0,1,0},{0,0,1},true};
    n.holder={{110,200,300},{0,1,0},{0,0,1},true};
    n.support={{130,210,260},{0,1,0},{0,0,1},true};
    ContactAt(n,n.support.position);
    Check(BuildCoJReloadLocalFrame(n.root,n.support,n.support_in_weapon),"fixture local");return n;
}
CoJLongGunGripGeometry Geometry(){
    CoJLongGunGripGeometry g{};g.native=Native();g.dominant_socket_cm={110,200,300};
    g.muzzle_cm={115,207,220};g.muzzle_direction={0,0,-1};
    g.dominant_grip_cm={400,500,600};g.direction={0,0,-1};g.up={0,1,0};
    g.support_grip_cm={420,510,560};return g;
}
void GeometryAndSinglePlan(){
    auto g=Geometry();auto out=BuildCoJLongGunGripFrame(g);
    Check(out.valid,"measured support plan missing");
    Check(Near(out.mapped_support_frame.position,g.support_grip_cm),"measured support point missed");
    Check(Near(out.plan.muzzle_origin,{405,507,520}),"muzzle offset replaced by support ray");
    Check(out.contact_distance_m<.00001F,"ordinary contact map incorrect");
    // A different support direction rotates the WHOLE map about the right socket.
    g.support_grip_cm={440,510,620};out=BuildCoJLongGunGripFrame(g);
    Check(out.valid&&IsCoJLongGunGripFrameAdmitted(out)&&
        Near(out.mapped_support_frame.position,g.support_grip_cm),
        "valid off-axis support was rejected or missed");
    auto replay=BuildTrackedWeaponFrame(g.native.root,g.dominant_socket_cm,g.muzzle_cm,
        g.muzzle_direction,g.dominant_grip_cm,out.plan.muzzle_direction,out.plan.root.up);
    Check(replay.valid&&Near(replay.root.position,out.plan.root.position)&&
        Near(replay.root.up,out.plan.root.up)&&Near(replay.root.forward,out.plan.root.forward)&&
        Near(replay.muzzle_origin,out.plan.muzzle_origin),"existing capture cannot reproduce solver map");
    const auto anchor=TransformWeaponElementFrame(g.native.root,out.plan.root,
        {g.dominant_socket_cm,{0,1,0},{0,0,1},true});
    Check(anchor.valid&&Near(anchor.position,g.dominant_grip_cm),"dominant socket drifted");
    g.support_grip_cm={480,520,640};out=BuildCoJLongGunGripFrame(g);
    Check(out.valid&&out.support_error_m>.4F,"unequal spacing hidden");
    Check(std::abs(out.solution.source_span_m-std::sqrt(.21F))<.0001F,"cm/metre conversion incorrect");
}
void PlayerAnimationCannotMoveWeaponContact(){
    auto g=Geometry();
    const auto before=BuildCoJLongGunAcquisitionFrame(g);
    Check(before.contact.valid,"contact regression fixture invalid");
    // The free native hand can be animated independently of the rifle mesh.
    // Its anatomy is still read, but it must not author the fore-end position.
    g.native.support.position.x+=50;
    Check(BuildCoJReloadLocalFrame(g.native.root,g.native.support,g.native.support_in_weapon),"detached hand fixture");
    const auto after=BuildCoJLongGunAcquisitionFrame(g);
    Check(after.contact.valid&&Near(before.contact.position,after.contact.position),
        "player left_hand moved the weapon contact away from its mesh");
    g.native.contact_anchor.position.y+=20;
    const auto moved=BuildCoJLongGunAcquisitionFrame(g);
    Check(moved.contact.valid&&Near(moved.contact.position,{before.contact.position.x,before.contact.position.y+20,before.contact.position.z}),
        "weapon element movement did not carry its mesh contact");
}
void MeshContactPlacementAndCancellation(){
    for(const char* cls:{"WeaponRifleWinchester","WeaponRifleRemington","WeaponRifleRemingtonSawOff","WeaponRifleWinchesterScope"}){
        auto g=Geometry();g.native.weapon_class=cls;ContactAt(g.native,{130,210,260});
        // Rotate the sampled mesh element 90 degrees about Y. The authored
        // point must follow the real child frame, not the player's socket.
        g.native.contact_anchor.forward={1,0,0};
        const auto p=CoJLongGunContact(cls).point_cm;
        const auto anchor=g.native.contact_anchor.position;
        const Vec3 expected{anchor.x+p.z,anchor.y+p.y,anchor.z-p.x};
        const auto acquired=BuildCoJLongGunAcquisitionFrame(g);
        Check(acquired.contact.valid&&Near(acquired.support.position,expected),"contact ignored mesh-element orientation");
        g.native.support.position={999,888,777};
        Check(BuildCoJReloadLocalFrame(g.native.root,g.native.support,g.native.support_in_weapon),"remote anatomical socket fixture");
        g.support_grip_cm=acquired.contact.position;
        const auto frame=BuildCoJLongGunGripFrame(g);
        Check(frame.valid&&Near(frame.mapped_support_frame.position,g.support_grip_cm)&&frame.contact_distance_m<.00001F,
            "actual mesh contact could not drive the rigid rifle solution");
        g.native.contact_anchor.valid=false;
        Check(!BuildCoJLongGunAcquisitionFrame(g).contact.valid&&!BuildCoJLongGunGripFrame(g).valid,
            "missing mesh source fell back to remote anatomical socket");
    }
}
void NativeRootHolderIsValid(){
    for(const char* cls:{"WeaponRifleWinchester","WeaponRifleRemington","WeaponRifleRemingtonSawOff","WeaponRifleWinchesterScope"}){
        auto g=Geometry();g.native.weapon_class=cls;
        ContactAt(g.native,g.native.support.position);
        // Live Remington resolves the named Holder to native element zero.
        // A root socket is valid; only a missing (-1) element is invalid.
        g.native.holder_element=0;g.native.holder=g.native.root;
        g.dominant_socket_cm=g.native.root.position;g.support_grip_cm={430,510,560};
        const auto frame=BuildCoJLongGunGripFrame(g);
        Check(frame.valid&&IsCoJLongGunGripFrameAdmitted(frame),"native root Holder rejected rifle support");
        Check(Near(frame.mapped_support_frame.position,g.support_grip_cm),"root Holder moved measured support");
        g.native.holder_element=-1;
        Check(!BuildCoJLongGunGripFrame(g).valid,"missing Holder admitted a rifle map");
    }
}
struct Fixture {
    CoJLongGunGripState state;CoJLongGunGripGeometry g=Geometry();CoJLongGunGripInput in{};
    Fixture(const char* cls="WeaponRifleWinchester"){g.native.weapon_class=cls;ContactAt(g.native,g.native.support.position);
        in.player_generation=3;in.context=5;in.controls.input_generation=7;
        in.controls.enabled=in.controls.eligible=in.controls.tracking_valid=true;
        in.controls.grip_available=in.controls.support_trigger_available=true;
        in.controls.acquisition_radius_m=.1F;in.controls.sequence=1;
        in.controls.sample_time_ms=in.controls.now_ms=1000;}
    CoJLongGunGripOutput Step(){++in.controls.sequence;in.controls.now_ms+=10;
        in.controls.sample_time_ms=in.controls.now_ms;return state.Update(in,g);}
    CoJLongGunGripOutput Attach(){(void)Step();in.controls.grip_held=true;auto o=Step();
        Check(o.claim.acquired&&o.attached&&o.frame.valid,"fresh idle support claim failed");return o;}
};
void CrossingSupportHandCannotFlipRifle(){
    auto g=Geometry();
    // The same measured grip separation can place the support controller on
    // the opposite side of the right grip, forcing a 180-degree barrel flip.
    g.support_grip_cm={380,490,640};
    const auto crossed=BuildCoJLongGunGripFrame(g);
    Check(crossed.valid&&crossed.support_error_m<.00001F,
        "crossed hand fixture must remain a valid rigid transform");
    Check(!IsCoJLongGunGripFrameAdmitted(crossed),
        "crossed support hand admitted a backwards rifle");
    Fixture f;
    (void)f.Attach();
    f.g.support_grip_cm=g.support_grip_cm;
    Check(!f.state.Refresh(f.in.player_generation,f.in.context,f.g).valid,
        "render refresh kept the inverted support pose");
    const auto o=f.Step();
    Check(!o.attached&&o.claim.cancelled&&o.claim.consume_grip,
        "crossing while attached did not release the rifle support claim");
    f.g=Geometry();
    Check(!f.Step().attached,"returning across the grip plane silently reattached");
    f.in.controls.grip_held=false;
    Check(f.Step().claim.consume_grip,"release following a crossing was not consumed");
    f.in.controls.grip_held=true;
    Check(f.Step().claim.acquired,"fresh regrip after crossing failed");
}
void OwnershipAndRelease(){
    Fixture f;auto o=f.Attach();
    GameplayInputState mapped{};mapped.active=mapped.interact=mapped.fire_left=mapped.fire_right=true;
    FilterCoJLongGunGripInput(o.claim,mapped);
    Check(!mapped.interact&&!mapped.fire_left&&mapped.fire_right&&!(mapped.digital_available&65),"support controls leaked");
    // Ordinary native cycling changes the live support pose, never the acquired contact.
    f.g.native.firing=true;f.g.native.states[0]={30,31,38,1};
    f.g.native.support.position={999,888,777};
    f.g.native.contact_anchor.position={777,666,555};
    Check(BuildCoJReloadLocalFrame(f.g.native.root,f.g.native.support,f.g.native.support_in_weapon),"cycle fixture");
    o=f.Step();Check(o.attached&&Near(o.frame.mapped_support_frame.position,{420,510,560}),"cycle reseeded support contact");
    f.in.controls.support_trigger_held=true;f.g.native.valid=false;o=f.Step();
    Check(o.claim.cancelled&&!o.attached&&o.claim.consume_grip&&o.claim.consume_support_trigger,"owner loss leaked hold");
    f.g.native=Native();o=f.Step();Check(!o.attached,"held owner recovery attached");
    f.in.controls.grip_held=false;o=f.Step();Check(o.claim.consume_grip&&o.claim.consume_support_trigger,"release leaked");
    o=f.Step();Check(!o.claim.consume_grip&&o.claim.consume_support_trigger,"L2 barrier lost");
    f.in.controls.support_trigger_available=false;f.in.controls.support_trigger_held=false;
    Check(f.Step().claim.consume_support_trigger,"missing L2 counted as release");
    f.in.controls.support_trigger_available=true;Check(f.Step().claim.consume_support_trigger,"L2 release leaked");
    Check(!f.Step().claim.consume_support_trigger,"L2 failed to recover");
}
void ChangedMeshContactRetiresClaim(){
    Fixture f;(void)f.Attach();++f.g.native.contact_element;
    Check(!f.state.Refresh(f.in.player_generation,f.in.context,f.g).valid,
        "changed mesh contact retained render authority");
    const auto out=f.Step();
    Check(!out.attached&&out.claim.cancelled&&out.claim.consume_grip,
        "changed mesh contact did not cancel the physical claim");
}
void ScopedMeasuredGeometryAndOwnership(){
    auto g=Geometry();g.native.weapon_class="WeaponRifleWinchesterScope";
    // Independent original scoped-mesh measurement, not a profile-derived
    // expected value. Rotate GunBody +90Y and move the anatomical socket away.
    g.native.contact_anchor={{130,250,260},{0,1,0},{1,0,0},true};
    g.native.support.position={999,888,777};
    Check(BuildCoJReloadLocalFrame(g.native.root,g.native.support,g.native.support_in_weapon),"scoped anatomy fixture");
    const auto acquisition=BuildCoJLongGunAcquisitionFrame(g);
    Check(acquisition.contact.valid&&Near(acquisition.support.position,{125.968197F,208.840465F,259.990420F}),
        "scoped contact did not follow its independently measured GunBody fore-end");
    g.support_grip_cm=acquisition.contact.position;
    const auto mapped=BuildCoJLongGunGripFrame(g);
    Check(IsCoJLongGunGripFrameAdmitted(mapped)&&mapped.contact_distance_m<.00001F&&
        Near(mapped.mapped_support_frame.position,g.support_grip_cm),"scoped measured contact failed rigid mapping");
    for(int state:{30,31,32,33,34,35,36,37,38,158,159,160,161,162,163,164,165,166}){
        Fixture f("WeaponRifleWinchesterScope");(void)f.Attach();
        f.g.native.firing=true;f.g.native.states[0]={state,state,state,state};
        f.g.native.support.position={999,888,777};f.g.native.contact_anchor.position={777,666,555};
        Check(BuildCoJReloadLocalFrame(f.g.native.root,f.g.native.support,f.g.native.support_in_weapon),"scoped cycle fixture");
        Check(f.Step().attached&&f.state.Refresh(f.in.player_generation,f.in.context,f.g).valid,
            "scoped fire/cycle lost frozen contact");
        f.in.controls.grip_held=false;Check(!f.Step().attached,"scoped release retained support");
        Fixture fresh("WeaponRifleWinchesterScope");fresh.g.native.states[0][0]=state;fresh.g.native.firing=true;
        (void)fresh.Step();fresh.in.controls.grip_held=true;
        Check(!fresh.Step().attached,"scoped firing acquired new support");
    }
    for(int state:{0,20,21,22,29,39,128,130,148,149,157,167,168}){
        Fixture f("WeaponRifleWinchesterScope");(void)f.Attach();f.g.native.states[0][0]=state;
        Check(!f.state.Refresh(f.in.player_generation,f.in.context,f.g).valid,"unqualified scoped state refreshed");
        auto out=f.Step();Check(!out.attached&&out.claim.cancelled&&out.claim.consume_grip,
            "unqualified scoped state retained a held claim");
    }
    Fixture alternate("WeaponRifleWinchesterScope");alternate.g.native.states[0]={129,129,129,129};
    (void)alternate.Attach();
    alternate.g.native.firing=true;alternate.g.native.states[0]={158,159,166,129};
    Check(alternate.Step().attached,"alternate scoped cycle cancelled the grip");
    alternate.g.native.firing=false;alternate.g.native.states[0]={129,129,129,129};
    Check(alternate.Step().attached,"alternate scoped idle lost the frozen grip");
    for(int why=0;why<7;++why){
        Fixture f("WeaponRifleWinchesterScope");(void)f.Attach();
        if(why==0)++f.g.native.object_epoch;
        if(why==1)++f.g.native.contact_element;
        if(why==2)f.g.native.weapon_class="WeaponRifleWinchester";
        if(why==3)f.g.native.weapon_class="UnknownScopedSubclass";
        if(why==4)++f.in.player_generation;
        if(why==5)++f.in.context;
        if(why==6)f.in.controls.tracking_valid=false;
        const auto out=f.Step();Check(!out.attached&&out.claim.cancelled&&out.claim.consume_grip,
            "scoped owner/source loss retained support");
        Check(!f.Step().attached,"held scoped grip reacquired after cancellation");
    }
}
void RejectionsAndRefresh(){
    for(int why=0;why<14;++why){Fixture f;(void)f.Attach();
        if(why==0)++f.g.native.weapon_id;if(why==1)++f.g.native.player_id;
        if(why==2)++f.in.player_generation;if(why==3)++f.in.context;
        if(why==4)f.g.native.weapon_class="WeaponRifleWinchesterScope";
        if(why==5)f.g.native.states[0][1]=21;if(why==6)f.g.native.two_handed[1][0]=true;
        if(why==7)++f.g.native.support_element;if(why==8)f.in.controls.recentered=true;
        if(why==9)f.g.native.root.up={0,2,0};if(why==10)f.in.controls.eligible=false;
        if(why==11)f.g.support_grip_cm.x=std::numeric_limits<float>::quiet_NaN();
        if(why==12)f.g.support_grip_cm={1200,1000,600};
        if(why==13)f.g.support_grip_cm={400.1F,500,600};
        auto o=f.Step();Check(!o.attached&&o.claim.cancelled&&o.claim.consume_grip,"changed/invalid owner retained");}
    Fixture f;f.g.native.firing=true;f.g.native.states[0][0]=30;
    (void)f.Step();f.in.controls.grip_held=true;Check(!f.Step().attached,"fire acquired support");
    Fixture h;(void)h.Attach();Check(h.state.Refresh(h.in.player_generation,h.in.context,h.g).valid,"same owner refresh rejected");
    auto distant=h.g;distant.support_grip_cm.x+=500;
    Check(!h.state.Refresh(h.in.player_generation,h.in.context,distant).valid,"refresh bypassed separation bounds");
    ++h.g.native.weapon_id;Check(!h.state.Refresh(h.in.player_generation,h.in.context,h.g).valid,"new owner got old map");
    Fixture q;(void)q.Attach();q.state.Cancel();Check(!q.state.attached(),"cancel kept attachment");
    auto o=q.Step();Check(o.claim.consume_grip&&!o.attached,"cancel forgot physical hold");
}
void UnattachedFreeTrigger(){
    CoJLongGunFreeTrigger state;
    GameplayInputState raw{};raw.active=true;raw.input_context_generation=1;
    raw.fire_left=raw.fire_right=true;
    auto mapped=raw;state.Filter(raw,1,CoJLongGunFireRouting{true,false},mapped);
    Check(!mapped.fire_left&&mapped.fire_right&&!(mapped.digital_available&1U),
        "unattached rifle free trigger reached native other-hand attack fallback");
    mapped=raw;state.Filter(raw,2,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_left&&mapped.fire_right,"changing weapon leaked consumed L2 hold");
    raw.active=false;raw.fire_left=false;mapped=raw;state.Filter(raw,3,CoJLongGunFireRouting{},mapped);
    raw.active=true;raw.fire_left=true;mapped=raw;state.Filter(raw,4,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_left,"inactive input counted as physical release");
    raw.digital_available&=~1U;raw.fire_left=false;mapped=raw;state.Filter(raw,5,CoJLongGunFireRouting{},mapped);
    raw.digital_available|=1U;raw.fire_left=true;++raw.input_context_generation;
    mapped=raw;state.Filter(raw,1,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_left,"missing sample or source replacement released held trigger");
    raw.fire_left=false;mapped=raw;state.Filter(raw,1,CoJLongGunFireRouting{},mapped);
    raw.fire_left=true;mapped=raw;state.Filter(raw,2,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_left,"duplicate sequence counted as release");
    raw.fire_left=false;mapped=raw;state.Filter(raw,3,CoJLongGunFireRouting{},mapped);
    Check(!(mapped.digital_available&1U),"consumed release escaped its owner");
    raw.fire_left=true;mapped=raw;state.Filter(raw,4,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&mapped.fire_right,"fresh pistol triggers did not recover independently");
    CoJLongGunFreeTrigger ordinary; mapped=raw;ordinary.Filter(raw,4,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&mapped.fire_right,"ordinary dual pistol trigger was blocked");
}
void LeftOwnedRifleFreeTrigger(){
    CoJLongGunFreeTrigger state;
    GameplayInputState raw{};raw.active=true;raw.input_context_generation=1;
    raw.fire_left=raw.fire_right=true;
    auto mapped=raw;
    state.Filter(raw,1,CoJLongGunFireRouting{false,true},mapped);
    Check(mapped.fire_left&&!mapped.fire_right&&!(mapped.digital_available&2U),
        "left-owned rifle let the free right trigger reach native fallback");
    mapped=raw;state.Filter(raw,2,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&!mapped.fire_right,"switching weapons leaked a consumed right trigger");
    raw.fire_right=false;raw.digital_available&=~2U;
    mapped=raw;state.Filter(raw,3,CoJLongGunFireRouting{},mapped);
    raw.digital_available|=2U;raw.fire_right=true;
    mapped=raw;state.Filter(raw,4,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_right,"missing right source counted as a physical release");
    raw.fire_right=false;mapped=raw;state.Filter(raw,4,CoJLongGunFireRouting{},mapped);
    raw.fire_right=true;mapped=raw;state.Filter(raw,5,CoJLongGunFireRouting{},mapped);
    Check(!mapped.fire_right,"repeated sample released the right trigger claim");
    raw.fire_right=false;mapped=raw;state.Filter(raw,6,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&!(mapped.digital_available&2U),"right release consumed the armed left trigger");
    raw.fire_right=true;mapped=raw;state.Filter(raw,7,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&mapped.fire_right,"right trigger did not recover after available release");
    mapped=raw;state.Filter(raw,8,CoJLongGunFireRouting{true,true},mapped);
    Check(!mapped.fire_left&&!mapped.fire_right&&!(mapped.digital_available&3U),
        "unknown owner failed to withhold both fallback routes");
    raw.fire_left=false;mapped=raw;state.Filter(raw,9,CoJLongGunFireRouting{},mapped);
    raw.fire_left=true;mapped=raw;state.Filter(raw,10,CoJLongGunFireRouting{},mapped);
    Check(mapped.fire_left&&!mapped.fire_right,"one trigger release discharged the other hand's claim");
}
void RepeatedRenderSamples(){
    Fixture f;CoJLongGunGripSampleClock clock;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    (void)f.state.Update(f.in,f.g);
    ++f.in.controls.sequence;f.in.controls.now_ms+=10;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    f.in.controls.grip_held=true;Check(f.state.Update(f.in,f.g).attached,"dated fresh grip failed");
    f.in.controls.now_ms+=10;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    Check(f.state.Update(f.in,f.g).attached,"duplicate render cancelled attachment");
    f.in.controls.now_ms+=101;
    f.in.controls.sample_time_ms=clock.Observe(7,f.in.controls.sequence,f.in.controls.now_ms);
    Check(!f.state.Update(f.in,f.g).attached,"duplicate render renewed stale tracking");
    Check(clock.Observe(8,1,2000)==2000,"replacement source failed to restart dating");
}
void ApproachAndPressFromReleasedGrip(){
    for(int why=0;why<3;++why){
        Fixture f;
        if(why==0)f.g.support_grip_cm={480,520,640}; // valid tracking, excessive separation
        if(why==1)f.g.support_grip_cm={380,490,640}; // opposite hemisphere
        if(why==2)f.g.support_grip_cm=f.g.dominant_grip_cm; // overlapping controllers
        Check(!f.Step().attached,"outside neutral geometry acquired a rifle");
        f.g=Geometry();f.in.controls.grip_held=true;
        Check(f.Step().claim.acquired,"released grip outside contact could not acquire on approach/press");
    }
    Fixture held;held.g.support_grip_cm={480,520,640};(void)held.Step();
    held.in.controls.grip_held=true;
    Check(!held.Step().attached,"outside press acquired a rifle");
    held.g=Geometry();Check(!held.Step().attached,"outside held press queued a later acquisition");
    held.in.controls.grip_held=false;(void)held.Step();held.in.controls.grip_held=true;
    Check(held.Step().claim.acquired,"release/repress failed after an outside press");
    Fixture lost;(void)lost.Step();lost.in.controls.eligible=false;(void)lost.Step();
    lost.in.controls.eligible=true;lost.in.controls.grip_held=true;
    Check(!lost.Step().attached,"hard context loss preserved a stale released baseline");
}
void CapturedGripGuide(){
    Fixture f;runtime::StereoHudTextOverlay hud{};
    runtime::Pose head{{0,1.7F,0},{},true,true},right{{.2F,1.2F,-.5F},{},true,true};
    // Geometry fixture's contact is exactly the support controller. In identity
    // tracking/world axes its captured head-relative delta is (20,10,-40) cm.
    Check(AppendCoJLongGunGripGuide(f.in,f.g,head,right,true,false,hud),"rifle contact guide was not captured");
    Check(hud.holster_guides.count==1,"rifle guide did not append exactly one bounded surface");
    auto guide=hud.holster_guides.zones[0];runtime::Vec3 centre{};
    for(auto p:guide.head_corners){centre.x+=p.x*.25F;centre.y+=p.y*.25F;centre.z+=p.z*.25F;}
    Check(Near(centre,{.4F,-.4F,-.9F}),"guide lost native cm/metre or captured head/right-grip registration");
    Check(guide.state==1,"near released hand did not advertise the contact zone");
    auto rotated=f.g;rotated.world_from_tracking={runtime::Vec3{0,0,-1},{0,1,0},{1,0,0}};
    auto turned=head;turned.orientation={0,-.70710678F,0,.70710678F};hud={};
    Check(AppendCoJLongGunGripGuide(f.in,rotated,turned,right,true,false,hud),"rotated capture lost rifle guide");
    runtime::Vec3 turned_centre{};for(auto p:hud.holster_guides.zones[0].head_corners){
        turned_centre.x+=p.x*.25F;turned_centre.y+=p.y*.25F;turned_centre.z+=p.z*.25F;}
    Check(Near(turned_centre,{-.3F,-.4F,-.6F}),"game yaw and HMD yaw used different capture references");
    f.g.support_grip_cm={480,520,640};hud={};
    Check(AppendCoJLongGunGripGuide(f.in,f.g,head,right,true,false,hud),"far hand lost its discovery guide");
    const auto far=hud.holster_guides.zones[0];runtime::Vec3 far_centre{};
    for(auto p:far.head_corners){far_centre.x+=p.x*.25F;far_centre.y+=p.y*.25F;far_centre.z+=p.z*.25F;}
    Check(Near(centre,far_centre)&&far.state==0,"guide followed the solver/left hand instead of ordinary rifle contact");
    f.g=Geometry();f.in.controls.grip_held=true;hud={};
    Check(AppendCoJLongGunGripGuide(f.in,f.g,head,right,true,false,hud)&&hud.holster_guides.zones[0].state==2,
        "held outside attempt advertised a fresh grip without release");
    f.in.controls.grip_held=false;
    for(int why=0;why<7;++why){auto in=f.in;auto g=f.g;auto h=head;hud={};
        if(why==0)in.controls.eligible=false;if(why==1)g.native.firing=true;
        if(why==2)h.orientation_valid=false;if(why==3)in.controls.now_ms+=101;
        if(why==4)g.native.valid=false;
        Check(!AppendCoJLongGunGripGuide(in,g,h,right,why!=5,why==6,hud)&&hud.holster_guides.count==0,
            "unavailable/attached/disabled rifle retained a guide");
    }
    hud.holster_guides.count=4;hud.holster_guides.zones[0].state=2;
    Check(AppendCoJLongGunGripGuide(f.in,f.g,head,right,true,false,hud)&&
        hud.holster_guides.count==5&&hud.holster_guides.zones[0].state==2,
        "rifle guide overwrote existing holster hints");
    hud.holster_guides.count=static_cast<std::uint32_t>(hud.holster_guides.zones.size());
    Check(!AppendCoJLongGunGripGuide(f.in,f.g,head,right,true,false,hud)&&hud.holster_guides.count==hud.holster_guides.zones.size(),
        "guide capacity overrun changed existing surfaces");
}
}
int main(){ScopedMeasuredGeometryAndOwnership();PlayerAnimationCannotMoveWeaponContact();MeshContactPlacementAndCancellation();GeometryAndSinglePlan();NativeRootHolderIsValid();CrossingSupportHandCannotFlipRifle();OwnershipAndRelease();ChangedMeshContactRetiresClaim();RejectionsAndRefresh();UnattachedFreeTrigger();LeftOwnedRifleFreeTrigger();RepeatedRenderSamples();ApproachAndPressFromReleasedGrip();CapturedGripGuide();
    std::cout<<"longgun measured geometry, single-map replay, native cycle/ownership and physical releases passed\n";}
