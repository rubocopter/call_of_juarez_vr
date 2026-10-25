#include "games/call_of_juarez/winchester_reload_probe.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
using namespace cojvr::games::call_of_juarez;
void Need(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
bool Reason(const CoJWinchesterProbeEnd& end,const char* reason){
    return end.reason&&std::strcmp(end.reason,reason)==0;
}
struct Fixture {
    CoJWinchesterReloadProbe probe;
    CoJWinchesterProbeInput input{1000000,7,5,false,false};
    CoJWinchesterProbeObservation observed{42,CoJWinchesterWeaponClass::ordinary,false,5,20};
    CoJWinchesterProbeTick tick{};
    CoJWinchesterProbeSample Step(std::uint64_t advance=350000,bool read=true){
        input.now_us+=advance;tick=probe.Begin(input);
        return tick.poll?probe.Record(read,observed):CoJWinchesterProbeSample{};
    }
    void Exhaust(){
        input.square_down=true;
        for(unsigned i=1;i<=220;++i){
            const auto result=Step(50000);
            Need(tick.poll&&result.emit&&result.sample_index==i,"burst lost its ordered bounded samples");
        }
    }
};
void FireAfterIdle(){
    Fixture f;
    for(int i=0;i<4;++i)Need(f.Step().emit,"initial idle discovery missing");
    Need(!f.Step().emit&&f.tick.poll,"idle discovery must stop emitting but keep polling");
    f.input.fire_down=true;
    const auto fire=f.Step(50000);
    Need(f.tick.poll&&fire.emit,"fresh fire after four idle samples must open a window");
    Need(fire.sample_index==1&&fire.sample_gap_us==0&&fire.ammo_delta==0,"fire retained idle history");
    f.input.now_us+=49999;
    Need(!f.probe.Begin(f.input).poll,"fire burst exceeded 20 Hz");
    ++f.input.now_us;
    Need(f.probe.Begin(f.input).poll,"fire burst missed its 50 ms poll");
    (void)f.probe.Record(true,f.observed);
    f.input.now_us+=4000000;
    const auto expired=f.probe.Begin(f.input);
    Need(Reason(expired.end,"expired"),"held fire renewed or silently ended the bounded window");
}
void DiscoveryAfterExhaustion(){
    Fixture f;f.Exhaust();
    Need(!f.Step(50000).emit&&!f.tick.poll,"exhausted window must return to discovery cadence");
    f.observed.weapon_id=43;
    const auto next=f.Step(300000);
    Need(f.tick.poll,"sample exhaustion must not disable native discovery");
    Need(next.emit&&next.sample_index==1&&next.sample_gap_us==0,"newly equipped rifle failed to rearm");
}
void EndAndNativeRearm(){
    Fixture f;
    f.input.square_down=true;
    CoJWinchesterProbeSample last{};
    for(int i=0;i<220;++i)last=f.Step(50000);
    Need(Reason(last.after,"exhausted")&&last.after.sample_index==220,
        "exhaustion must explicitly delimit the last observed sample");
    Need(last.after.last_sample_us==f.input.now_us,"end lost its last sample clock");
    f.observed.native_reloading=true;
    const auto native=f.Step();
    Need(f.tick.poll&&native.emit&&native.sample_index==1,"new native reload failed to rearm after exhaustion");
    // A held native status must not renew the emission budget indefinitely.
    for(int i=1;i<220;++i)Need(f.Step(50000).emit,"native burst sample missing");
    Need(!f.Step().emit&&f.tick.poll,"held native reload rearmed an exhausted budget");
    f.observed.native_reloading=false;Need(!f.Step().emit,"native release unexpectedly reopened exhaustion");
    f.observed.native_reloading=true;Need(f.Step().emit,"second native reload transition failed to rearm");
}
void HistoryAndLoss(){
    Fixture f;
    const auto first=f.Step();Need(first.emit&&first.sample_index==1,"first sample missing");
    --f.observed.ammo_readback;
    const auto second=f.Step();
    Need(second.sample_index==2&&second.sample_gap_us==350000&&second.ammo_delta==-1,"delta history changed");
    f.input.fire_down=true;
    const auto fire=f.Step(50000);
    Need(Reason(f.tick.end,"retriggered")&&f.tick.end.sample_index==2,"fire did not end previous history");
    Need(fire.emit&&fire.sample_index==1&&fire.ammo_delta==0,"new window retained deltas");
    Need(f.Step(50000,false).unavailable,"read failure did not delimit known ownership");
    Need(!f.Step(350000,false).unavailable,"repeated read failure emitted duplicate unavailable");
    const auto recovered=f.Step(350000);
    Need(recovered.emit&&recovered.sample_index==1&&recovered.sample_gap_us==0,"recovery retained old history");
    const auto end=f.probe.Reset();Need(Reason(end,"disabled"),"disable silently discarded an open window");
    Need(!f.probe.Reset().reason,"duplicate reset ended an already closed window");
}
void ExactClassHistory(){
    Fixture f;
    const auto first=f.Step();Need(first.emit&&first.sample_index==1,"ordinary Winchester first sample missing");
    f.observed.weapon_class=CoJWinchesterWeaponClass::scoped;
    f.observed.ammo_readback=2;
    auto scoped=f.Step();
    Need(Reason(scoped.before,"owner_changed")&&
         scoped.before.weapon_class==CoJWinchesterWeaponClass::ordinary&&
         scoped.before.weapon_id==42,"class change merged ordinary history");
    Need(scoped.emit&&scoped.sample_index==1&&scoped.sample_gap_us==0&&
         scoped.ammo_delta==0,"scoped observation inherited ordinary ammunition");
    f.observed.ammo_readback=3;
    scoped=f.Step();
    Need(scoped.emit&&scoped.sample_index==2&&scoped.ammo_delta==1,
         "scoped own-history delta lost");
    const auto end=f.probe.Reset();
    Need(end.weapon_class==CoJWinchesterWeaponClass::scoped,
         "scoped window end lost exact class");
    f.observed.weapon_class=CoJWinchesterWeaponClass::unknown;
    const auto invalid=f.Step();
    Need(!invalid.emit,"unknown class admitted to probe");
}
}
int main(int argc,char** argv){
    const char* mode=argc>1?argv[1]:"all";
    if(!std::strcmp(mode,"fire")||!std::strcmp(mode,"all"))FireAfterIdle();
    if(!std::strcmp(mode,"discovery")||!std::strcmp(mode,"all"))DiscoveryAfterExhaustion();
    if(!std::strcmp(mode,"native")||!std::strcmp(mode,"all"))EndAndNativeRearm();
    if(!std::strcmp(mode,"history")||!std::strcmp(mode,"all"))HistoryAndLoss();
    if(!std::strcmp(mode,"identity")||!std::strcmp(mode,"all"))ExactClassHistory();
    std::cout<<"Winchester probe cadence, rearming, bounded windows and history passed\n";
}
