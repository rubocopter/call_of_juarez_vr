#include "runtime/reload_haptics.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace cojvr::runtime;
namespace {
void Require(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
UiHapticTime Time(std::int64_t ms){return UiHapticTime{}+std::chrono::milliseconds(ms);}
ReloadInsertionFeedback Event(bool accepted,std::uint8_t hand,std::uint64_t sequence,std::uint64_t input){
    ReloadInsertionFeedback e{accepted,hand,sequence,input,false};e.owner_token=1;return e;
}
struct Fixture {
    ReloadInsertionHaptics policy;
    std::uint64_t input=1,token=3,resource=1,device=1;
    Fixture(){policy.UpdateOwner(1);policy.UpdateContext(true,input,token,Time(1000));(void)Frame(false,1,1000);}
    std::optional<UiHapticPulse> Frame(bool accepted,std::uint64_t seq,std::int64_t ms,
        std::uint8_t hand=1,std::int64_t captured=0){
        policy.UpdateContext(true,input,token,Time(ms));
        ReloadInsertionFeedback event{accepted,hand,seq,input,false};event.owner_token=1;
        return policy.Observe(event,seq,resource,device,
            Time(captured?captured:ms),Time(ms),token);
    }
};
}
int main(){
    Fixture owner_change;
    owner_change.policy.UpdateOwner(2);
    Require(!owner_change.Frame(true,2,1010),"native owner replacement invalidates queued event with unchanged availability/input/resource");
    Require(!owner_change.Frame(true,3,1020),"second queued event cannot establish an old-owner baseline");
    auto fresh_owner=ReloadInsertionFeedback{true,0,4,1,false};fresh_owner.owner_token=2;
    Require(!owner_change.policy.Observe(fresh_owner,4,1,1,Time(1030),Time(1030),3),"new owner establishes silent baseline");
    fresh_owner.event_sequence=5;
    Require(owner_change.policy.Observe(fresh_owner,5,1,1,Time(1120),Time(1120),3).has_value(),"fresh new-owner feedback recovers");
    owner_change.policy.UpdateOwner(0);fresh_owner.event_sequence=6;
    Require(!owner_change.policy.Observe(fresh_owner,6,1,1,Time(1210),Time(1210),3),"unobserved owner is silent");
    fresh_owner.event_sequence=7;
    Require(!owner_change.policy.Observe(fresh_owner,7,1,1,Time(1300),Time(1300),3),"unobserved owner cannot rearm from repeated fresh captures");
    Fixture f;
    Require(!f.Frame(false,2,1010),"unaccepted cartridge intent must remain silent");
    auto pulse=f.Frame(true,3,1020);
    Require(pulse&&pulse->hand==UiHapticHand::left&&UiHapticPulseValid(*pulse),"accepted right-gun insertion pulses free left hand");
    Require(!f.Frame(true,3,1021),"compositor repeat cannot repeat acceptance pulse");
    Require(!f.Frame(true,2,1022),"regressing frame cannot repeat acceptance");
    Require(!f.Frame(false,4,1030),"next ordinary frame stays silent");
    Require(!f.Frame(true,5,1040),"rapid accepted pulse is dropped without queuing");
    Require(!f.Frame(false,6,1110),"cadence recovery must not replay dropped acceptance");
    pulse=f.Frame(true,7,1120,0);
    Require(pulse&&pulse->hand==UiHapticHand::right,"mirrored insertion pulses free right hand");
    Require(!f.Frame(true,8,1210,2),"unclaimed hand is invalid");
    Require(!f.policy.Observe(Event(true,1,8,1),9,1,1,Time(1220),Time(1220),3),"event from another capture is rejected");
    Require(!f.policy.Observe(Event(true,1,10,2),10,1,1,Time(1230),Time(1230),3),"other input generation is rejected");
    for(int boundary=0;boundary<4;++boundary){
        Fixture x;
        if(boundary==0)x.policy.UpdateContext(false,1,3,Time(1010));
        if(boundary==1)x.input=2;
        if(boundary==2)x.token=7;
        if(boundary==3)x.resource=2;
        x.policy.UpdateContext(boundary!=0,x.input,x.token,Time(1010));
        Require(!x.policy.Observe(Event(true,1,2,1),2,x.resource,x.device,Time(1005),Time(1010),3),"queued pre-boundary acceptance is rejected");
        Require(!x.Frame(true,3,1020),"reacquisition must establish silent baseline");
        Require(x.Frame(true,4,1110).has_value(),"fresh post-baseline insertion recovers");
    }
    Fixture replacement;replacement.device=2;
    Require(!replacement.Frame(true,2,1010),"device replacement is silent");
    Require(replacement.Frame(true,3,1100).has_value(),"fresh resource acceptance recovers");
    Fixture restarted;
    Require(restarted.Frame(true,25,1010).has_value(),"original resource delivers before capture reset");
    restarted.resource=2;
    Require(!restarted.Frame(true,1,1110),"new generation with reset sequence establishes silent baseline");
    Require(restarted.Frame(true,2,1200).has_value(),"new generation with reset sequence can deliver fresh acceptance");
    Require(!restarted.Frame(true,2,1210),"new-generation duplicate is rejected");
    Require(!restarted.policy.Observe(Event(true,1,26,1),26,1,1,Time(1220),Time(1220),3),
        "old generation cannot resurrect stale acceptance");
    Fixture restarted_device;
    Require(restarted_device.Frame(true,25,1010).has_value(),"original device delivers before capture reset");
    restarted_device.device=2;
    Require(!restarted_device.Frame(true,1,1110),"device replacement with reset sequence establishes silent baseline");
    Require(restarted_device.Frame(true,2,1200).has_value(),"device replacement with reset sequence can deliver fresh acceptance");
    Fixture stale;
    Require(!stale.Frame(true,2,1200,1,1001),"stale capture cannot pulse");
    Require(!stale.Frame(true,3,1210),"stale gap recovery is silent");
    Require(stale.Frame(true,4,1300).has_value(),"fresh event after stale baseline recovers");
    Require(!stale.Frame(true,5,1310,1,1320),"future capture cannot pulse");
    Fixture rollback;
    Require(!rollback.Frame(true,2,990),"clock regression cannot pulse");
    Require(!rollback.Frame(true,3,1010),"clock recovery baseline silent");
    Fixture unavailable;
    unavailable.policy.UpdateContext(true,1,4,Time(1010));
    Require(!unavailable.policy.Observe(Event(true,1,2,1),2,1,1,Time(1010),Time(1010),4),"native context unavailable stays silent");
    Fixture failed;
    pulse=failed.Frame(true,2,1010);Require(pulse.has_value(),"event consumed before output delivery");
    UiHapticOutputGate gate;gate.Configure(UiHapticHand::left,true);
    Require(!gate.Dispatch(*pulse,[]{return false;}),"output failure stays optional");
    Require(!failed.Frame(true,2,1110),"failed output cannot replay event");
    GameplayInputState raw{};raw.active=true;raw.digital_available=3;
    Pose pose{};pose.position_valid=pose.orientation_valid=true;pose.orientation.w=1;
    Require(ReloadHapticInputAvailable(raw,true,true,true,false,pose,pose,pose),"available gameplay supplies feedback");
    for(int fault=0;fault<6;++fault){
        auto r=raw;auto p=pose;
        if(fault==0)r.weapon_radial=true;
        if(fault==1)r.digital_available=1;
        if(fault==2)p.position_valid=false;
        if(fault==3)p.orientation.w=0;
        if(fault==4)p.position.x=std::numeric_limits<float>::quiet_NaN();
        if(fault==5)r.quick_load=true;
        Require(!ReloadHapticInputAvailable(r,true,true,true,false,pose,p,pose),"conflict or invalid pose suppresses feedback");
    }
    Require(!ReloadHapticInputAvailable(raw,true,true,true,true,pose,pose,pose),"dashboard silent");
    Require(!ReloadHapticInputAvailable(raw,false,true,true,false,pose,pose,pose),"flat gameplay silent");
    Require(!ReloadHapticInputAvailable(raw,true,false,true,false,pose,pose,pose),"unavailable input silent");
    Require(!ReloadHapticInputAvailable(raw,true,true,false,false,pose,pose,pose),"focus loss silent");
    std::cout<<"Reload acceptance haptic ownership, freshness and recovery passed\n";
}
