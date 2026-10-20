#include "runtime/bow_draw_haptics.hpp"
#include "runtime/reload_haptics.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace cojvr::runtime;
namespace {
void Require(bool value, const char* why) {
    if (!value) { std::cerr << why << '\n'; std::exit(1); }
}
UiHapticTime Time(std::int64_t ms) { return UiHapticTime{} + std::chrono::milliseconds(ms); }
using Kind = BowDrawFeedbackKind;
struct Fixture {
    BowDrawHaptics policy;
    std::uint64_t owner=1, input=1, native=3, resource=1, device=1;
    Fixture() {
        policy.UpdateOwner(owner);
        Require(!Frame(Kind::grab,1,1000), "initial event establishes silent baseline");
    }
    std::optional<UiHapticPulse> Frame(Kind kind, std::uint64_t sequence,
        std::int64_t now, float tension=.5F, std::int64_t captured=0) {
        policy.UpdateContext(true,input,native,Time(now));
        return Observe({owner,input,sequence,kind,tension},sequence,now,captured);
    }
    std::optional<UiHapticPulse> Observe(BowDrawFeedback event, std::uint64_t sequence,
        std::int64_t now, std::int64_t captured=0) {
        return policy.Observe(event,sequence,resource,device,
            Time(captured?captured:now),Time(now),native);
    }
};
void Pulse(const std::optional<UiHapticPulse>& p, float duration, float amplitude, const char* why) {
    Require(p && UiHapticPulseValid(*p) && p->hand==UiHapticHand::right &&
        std::abs(p->duration_seconds-duration)<.00001F &&
        std::abs(p->amplitude-amplitude)<.00001F,why);
}
}
int main() {
    Fixture gestures;
    Pulse(gestures.Frame(Kind::grab,2,1010),.008F,.12F,"grab pulses right string hand");
    Pulse(gestures.Frame(Kind::tension,3,1020,0),.008F,.08F,"physical draw starts at bounded low amplitude");
    Require(!gestures.Frame(Kind::tension,4,1099,1),"tension cadence drops before 80ms");
    Pulse(gestures.Frame(Kind::tension,5,1100,.5F),.008F,.14F,"normalized physical half draw scales amplitude");
    Pulse(gestures.Frame(Kind::release,6,1101),.016F,.25F,"string release is immediate even after tension, without arrow acknowledgement");
    Require(!gestures.Frame(Kind::none,7,1110),"ordinary frame cannot replay release");
    (void)gestures.Frame(Kind::none,8,1180);
    Pulse(gestures.Frame(Kind::tension,9,1181,1),.008F,.20F,"physical full draw bounds amplitude");
    Require(!gestures.Frame(Kind::release,9,1182),"duplicate capture cannot change event kind and replay");
    Require(!gestures.Frame(Kind::grab,8,1183),"regressing capture cannot replay");
    Require(!gestures.Frame(Kind::release,0,1184),"zero capture cannot pulse");
    Require(!gestures.Observe({1,1,10,Kind::release,.5F},11,1190),"event must belong to this capture");

    Fixture owner;
    owner.policy.UpdateOwner(2);
    Require(!owner.Frame(Kind::release,2,1010),"weapon switch drops queued old-owner release");
    Require(!owner.Frame(Kind::release,3,1020),"repeated old-owner frames cannot rearm");
    owner.owner=2;
    Require(!owner.Frame(Kind::release,4,1030),"new owner establishes silent baseline");
    Pulse(owner.Frame(Kind::grab,5,1040),.008F,.12F,"fresh owner recovers independently of reload ownership");
    owner.policy.UpdateOwner(0);
    Require(!owner.Frame(Kind::release,6,1050),"unobserved owner rejects feedback");
    Require(!owner.Frame(Kind::release,7,1060),"unobserved owner cannot gain baseline");
    owner.policy.UpdateOwner(2);
    Require(!owner.Frame(Kind::release,8,1070),"owner loss/reacquire missed between frames is silent");

    for(int boundary=0;boundary<5;++boundary) {
        Fixture f;
        if(boundary==0) f.policy.UpdateContext(false,1,3,Time(1010));
        if(boundary==1) ++f.input;
        if(boundary==2) f.native=7;
        if(boundary==3) ++f.resource;
        if(boundary==4) ++f.device;
        f.policy.UpdateContext(boundary!=0,f.input,f.native,Time(1010));
        Require(!f.Observe({1,1,2,Kind::release,.5F},2,1010,1005),"queued feedback cannot cross availability/input/native/resource/device boundary");
        Require(!f.Frame(Kind::release,3,1020),"boundary recovery establishes silent baseline");
        Pulse(f.Frame(Kind::grab,4,1030),.008F,.12F,"fresh post-boundary gesture recovers");
    }
    for(int replacement=0;replacement<2;++replacement) {
        Fixture f;
        Pulse(f.Frame(Kind::release,50,1010),.016F,.25F,"original resource can deliver");
        if(replacement==0) ++f.resource; else ++f.device;
        Require(!f.Frame(Kind::release,1,1020),"replacement with restarted sequence silently baselines");
        Pulse(f.Frame(Kind::release,2,1030),.016F,.25F,"fresh replacement capture recovers");
        if(replacement==0) {
            --f.resource;
            Require(!f.Frame(Kind::release,51,1040),"old resource generation cannot resurrect release");
        }
    }
    for(int invalid=0;invalid<2;++invalid) {
        Fixture f;
        if(invalid==0) f.resource=0; else f.device=0;
        Require(!f.Frame(Kind::release,2,1010),"absent resource identity stays silent");
    }
    Fixture stale;
    Require(!stale.Frame(Kind::release,2,1200,.5F,1001),"capture older than 100ms drops");
    Require(!stale.Frame(Kind::release,3,1210),"stale gap recovery silently baselines");
    Pulse(stale.Frame(Kind::grab,4,1220),.008F,.12F,"fresh capture after stale baseline recovers");
    Require(!stale.Frame(Kind::release,5,1230,.5F,1240),"future capture drops");
    Require(!stale.Frame(Kind::release,6,1240),"future capture recovery silently baselines");
    Pulse(stale.Frame(Kind::release,7,1250),.016F,.25F,"future capture fault recovers");
    Require(!stale.Frame(Kind::release,8,1260,.5F,1249),"capture clock regression drops");
    Fixture clock;
    Require(!clock.Frame(Kind::release,2,990),"poll clock regression drops");
    Require(!clock.Frame(Kind::release,3,1010),"poll clock recovery silently baselines");
    Require(!clock.Observe({1,1,4,Kind::release,.5F},4,1009),"observe clock regression drops");
    Fixture missing_time;
    Require(!missing_time.policy.Observe({1,1,2,Kind::release,.5F},2,1,1,UiHapticTime{},Time(1010),3),"zero capture clock drops");
    Fixture native;
    native.native=4;
    Require(!native.Frame(Kind::release,2,1010),"native unavailable epoch stays silent");
    native.native=3;
    Require(!native.Frame(Kind::release,3,1020),"native reacquire silently baselines");
    Require(!native.Observe({1,2,4,Kind::release,.5F},4,1030),"foreign input generation drops");

    const float invalid_tension[]={-.01F,1.01F,std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()};
    for(float tension:invalid_tension) {
        Fixture f;
        Require(!f.Frame(Kind::tension,2,1010,tension),"non-normalized physical tension fails closed");
        Require(!f.Frame(Kind::tension,2,1100,.5F),"invalid consumed capture cannot be corrected and replayed");
    }
    Fixture invalid_kind;
    Require(!invalid_kind.Frame(static_cast<Kind>(255),2,1010),"unknown event kind stays silent");
    for(int output=0;output<3;++output) {
        Fixture f;
        auto pulse=f.Frame(Kind::release,2,1010);
        Require(pulse.has_value(),"event consumed before optional output");
        UiHapticOutputGate gate;
        gate.Configure(UiHapticHand::left,true);
        gate.Configure(UiHapticHand::right,output!=0);
        int calls=0;
        Require(!gate.Dispatch(*pulse,[&]()->bool { ++calls; if(output==2) throw 1; return false; }),"unavailable/failed/throwing output is optional");
        Require(calls==(output==0?0:1),"optional output has no retries");
        Require(gate.Available(UiHapticHand::left)&&!gate.Available(UiHapticHand::right),"right output failure preserves independent left output");
        Require(!f.Frame(Kind::release,2,1020),"failed output never replays capture");
        Require(!gate.Dispatch(*pulse,[&] { ++calls; return true; }),"failed output stays disabled");
        Require(calls==(output==0?0:1),"disabled output never calls transport again");
    }

    GameplayInputState raw{}; raw.active=true; raw.digital_available=3;
    Pose pose{}; pose.position_valid=pose.orientation_valid=true; pose.orientation.w=1;
    Require(ReloadHapticInputAvailable(raw,true,true,true,false,pose,pose,pose),"bow shares qualified gameplay availability");
    for(int conflict=0;conflict<12;++conflict) {
        auto r=raw;
        if(conflict==0) r.weapon_radial=true;
        if(conflict==1) r.interact=true;
        if(conflict==2) r.quick_save=true;
        if(conflict==3) r.quick_load=true;
        if(conflict==4) r.weapon_next=true;
        if(conflict==5) r.weapon_previous=true;
        if(conflict==6) r.hands=true;
        if(conflict==7) r.discard_weapon=true;
        if(conflict==8) r.equipment_select[0]=true;
        if(conflict==9) r.digital_available=1;
        if(conflict==10) r.active=false;
        if(conflict==11) r.equipment_select.back()=true;
        Require(!ReloadHapticInputAvailable(r,true,true,true,false,pose,pose,pose),"raw conflicting input immediately silences bow");
    }
    for(int hand=0;hand<3;++hand) {
        Pose poses[]={pose,pose,pose}; poses[hand].position.x=std::numeric_limits<float>::quiet_NaN();
        Require(!ReloadHapticInputAvailable(raw,true,true,true,false,poses[0],poses[1],poses[2]),"head/either grip invalidity silences bow");
    }
    Require(!ReloadHapticInputAvailable(raw,false,true,true,false,pose,pose,pose),"flat mode silent");
    Require(!ReloadHapticInputAvailable(raw,true,false,true,false,pose,pose,pose),"unpolled input silent");
    Require(!ReloadHapticInputAvailable(raw,true,true,false,false,pose,pose,pose),"focus loss silent");
    Require(!ReloadHapticInputAvailable(raw,true,true,true,true,pose,pose,pose),"dashboard silent");
    std::cout << "Bow controller haptic ownership, freshness, cadence and optional output passed\n";
}
