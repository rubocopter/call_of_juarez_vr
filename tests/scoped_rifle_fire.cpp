#include "games/call_of_juarez/scoped_rifle_fire.hpp"
#include "games/call_of_juarez/long_gun_grip.hpp"
#include <iostream>

int main(){
    using namespace cojvr::games::call_of_juarez;
    bool ok=true;
    auto check=[&](bool value,const char* why){if(!value){std::cerr<<why<<'\n';ok=false;}};
    CoJLongGunFireRouting owner{true,false,0xf1234567U};
    cojvr::runtime::GameplayInputState raw{};raw.active=true;
    raw.input_context_generation=9;raw.digital_available=~0U;
    CoJScopedRifleFire supported;
    cojvr::runtime::TwoHandGripOutput grip{};grip.consume_grip=true;
    raw.interact=true;
    auto poll=[&](CoJScopedRifleFire& gate,unsigned sequence,bool support){
        auto mapped=raw;
        if(support)FilterCoJLongGunGripInput(grip,mapped);
        gate.Filter(true,owner,7,53,sequence,raw,mapped);return mapped;
    };
    (void)poll(supported,1,true);
    raw.fire_right=true;auto shot=poll(supported,2,true);
    check(shot.fire_left&&!shot.fire_right&&!shot.interact,
        "consumed L1 support blocked scoped R2 native shot channel");
    shot=poll(supported,3,false);
    check(!shot.fire_left&&!shot.fire_right&&shot.interact,
        "unclaimed world interaction did not stop scoped fire");
    raw.fire_right=false;(void)poll(supported,4,true);raw.fire_right=true;
    check(poll(supported,5,true).fire_left,"supported rifle failed fresh rearm");
    CoJScopedRifleFire duplicate;raw={};raw.active=true;
    raw.input_context_generation=9;raw.digital_available=~0U;
    (void)poll(duplicate,1,false);raw.fire_right=true;
    check(!poll(duplicate,1,false).fire_left,"repeated pose manufactured a fresh scoped shot");
    check(poll(duplicate,2,false).fire_left,"fresh pose dropped a permitted scoped shot");
    duplicate.Interrupt();
    check(!poll(duplicate,2,false).fire_left&&!poll(duplicate,3,false).fire_left,
        "interrupted scoped fire resumed from a consumed hold");
    raw.fire_right=false;(void)poll(duplicate,4,false);raw.fire_right=true;
    check(poll(duplicate,5,false).fire_left,"interruption did not recover after fresh release");
    return ok?0:1;
}
