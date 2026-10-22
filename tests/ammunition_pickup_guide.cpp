#include "games/call_of_juarez/ammunition_pickup_guide.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
using namespace cojvr::games::call_of_juarez;
namespace {
void Check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
Pose At(Vec3 p){Pose out{};out.position=p;out.position_valid=out.orientation_valid=true;return out;}
struct Fixture {
    CoJManualReloadSession session;
    CoJManualReloadInput s{};
    CoJManualReloadOutput result{};
    GameplayInputState mapped{};
    explicit Fixture(int hand=0){
        s.enabled=s.allowed=true;s.player=7;s.context=5;s.raw.active=true;s.raw.input_context_generation=8;
        s.admission={hand,42,true,true};s.recovery=s.admission;
        s.head=At({0,1.6F,0});s.left=At({-.2F,1.05F,-.1F});s.right=At({.2F,1.05F,-.1F});
        s.native.valid=s.native.probe_valid=true;s.native.weapon_id=42;s.native.armed_hand=hand;s.native.reserve_readback=12;
    }
    void Poll(){++s.sequence;s.now+=50;mapped=s.raw;result=session.Update(mapped,s);}
    void Hold(bool held){if(s.native.armed_hand==0)s.raw.fire_left=held;else s.raw.fire_right=held;}
    Pose& Support(){return s.native.armed_hand==0?s.left:s.right;}
    void Open(){Poll();s.raw.reload=true;Poll();Check(result.start,"session must start natively");session.NoteStart(true);
        s.raw.reload=false;s.native.probe_status=2;s.admission={};Poll();Check(result.tracked,"native waiting must be supervised");}
    bool Draw(StereoHudTextOverlay& out,bool enabled=true){return AppendCoJAmmunitionPickupGuide(s,result,enabled,out);}
};
void PickupMatchesAdmission(){
    for(int hand=0;hand<2;++hand){
        Fixture f(hand);f.Open();
        const auto hint=f.result.gesture.pickup_hint;
        Check(hint.visible&&hint.ready&&hint.in_range&&hint.hand==1-hand,"released support at real source must be ready");
        const float side=hand==0?-.2F:.2F;
        Check(std::abs(hint.position.x-side)<1e-5F&&std::abs(hint.position.y-1.05F)<1e-5F&&
            std::abs(hint.position.z+.1F)<1e-5F,"hint must use the gesture's exact mirrored source");
        StereoHudTextOverlay out{};out.holster_guides.count=1;out.holster_guides.zones[0].state=2;
        Check(f.Draw(out)&&out.holster_guides.count==2&&out.holster_guides.zones[0].state==2,"ammo must append without overwriting other hints");
        const auto& guide=out.holster_guides.zones[1];Vec3 centre{};
        for(auto p:guide.head_corners){centre.x+=p.x*.25F;centre.y+=p.y*.25F;centre.z+=p.z*.25F;}
        Check(guide.kind==GestureGuideKind::ammunition_pickup&&guide.state==1&&std::abs(centre.x-side)<1e-5F&&
            std::abs(centre.y+.55F)<1e-5F&&std::abs(centre.z+.1F)<1e-5F,"captured ammo marker must locate the real source");
        f.Poll(); // A repeat query must not consume an input or cartridge.
        Check(!f.result.insert&&!f.result.gesture.cartridge_held,"visible hint cannot grant ammunition");
        f.Hold(true);f.Poll();
        Check(f.result.gesture.cartridge_held&&!f.result.gesture.pickup_hint.visible,"successful actual pickup must hide the marker");
        out={};Check(!f.Draw(out),"a held cartridge cannot advertise another pickup");
    }
}
void FreshReleaseAndFaults(){
    Fixture f;f.Open();f.Support().position.x=-.8F;f.Poll();
    Check(f.result.gesture.pickup_hint.visible&&!f.result.gesture.pickup_hint.in_range,"far hand must retain a locator");
    f.Hold(true);f.Poll();Check(!f.result.gesture.pickup_hint.ready,"outside held press must request release");
    f.Support().position.x=-.2F;f.Poll();
    Check(!f.result.gesture.cartridge_held&&!f.result.gesture.pickup_hint.ready,"entering while held cannot turn cue green or pick up");
    auto duplicate=f.session.Update(f.mapped,f.s);
    Check(duplicate.gesture.pickup_hint.visible&&!duplicate.gesture.pickup_hint.ready,"same pose must retain the captured cue without arming");
    f.Hold(false);f.Poll();Check(f.result.gesture.pickup_hint.ready&&f.result.gesture.pickup_hint.in_range,"fresh release restores readiness");
    for(int fault=0;fault<9;++fault){
        Fixture x;x.Open();
        if(fault==0)x.s.allowed=false;
        if(fault==1)x.s.recentered=true;
        if(fault==2)x.s.left.position_valid=false;
        if(fault==3)x.s.raw.weapon_radial=true;
        if(fault==4)x.s.native.weapon_id=99;
        if(fault==5)x.s.raw.input_context_generation++;
        if(fault==6)x.s.native.probe_status=3;
        if(fault==7)x.s.now+=300;
        if(fault==8)x.s.enabled=false;
        x.Poll();StereoHudTextOverlay out{};
        Check(!x.result.gesture.pickup_hint.visible&&!x.Draw(out)&&!out.holster_guides.count,"lost session must clear pickup hint");
    }
}
void PresentationGates(){
    for(int reason=0;reason<8;++reason){
        Fixture f;f.Open();StereoHudTextOverlay out{};
        if(reason==0)f.s.native.reserve_readback=0;
        if(reason==1)f.s.native.reserve_readback=-1;
        if(reason==2)f.result.cancel=true;
        if(reason==3)f.result.tracked=false;
        if(reason==4)f.s.head.orientation.x=std::numeric_limits<float>::quiet_NaN();
        if(reason==5)out.holster_guides.count=static_cast<std::uint32_t>(out.holster_guides.zones.size());
        if(reason==6)f.result.gesture.pickup_hint.hand=0;
        Check(!f.Draw(out,reason!=7),"unavailable, wrong-handed, full or disabled hint must reject");
    }
}
void CapturedTrackingFrame(){
    Fixture f;f.s.head.position={2,1.6F,-3};f.s.head.orientation={0,.70710678F,0,.70710678F};
    const auto offset=RotateVector(f.s.head.orientation,{-.2F,-.55F,-.1F});
    f.s.left.position={f.s.head.position.x+offset.x,f.s.head.position.y+offset.y,f.s.head.position.z+offset.z};
    f.Open();StereoHudTextOverlay out{};Check(f.Draw(out),"translated/yawed pickup source lost guide");
    Vec3 centre{};for(auto p:out.holster_guides.zones[0].head_corners){centre.x+=p.x*.25F;centre.y+=p.y*.25F;centre.z+=p.z*.25F;}
    Check(std::abs(centre.x+.2F)<1e-5F&&std::abs(centre.y+.55F)<1e-5F&&std::abs(centre.z+.1F)<1e-5F,
        "head-space capture cannot drift from the real tracking-space pickup source");
}
}
int main(){PickupMatchesAdmission();FreshReleaseAndFaults();PresentationGates();CapturedTrackingFrame();std::cout<<"ammunition pickup guides passed\n";}
