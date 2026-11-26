#include "games/call_of_juarez/dynamite_motion.hpp"
#include <iostream>

namespace {
using namespace cojvr::games::call_of_juarez;

struct Fixture {
    CoJDynamiteMotion motion;
    CoJDynamiteMotionSample sample{
        .owner=13, .player=7, .context=3, .input_generation=11,
        .head={.orientation_valid=true,.position_valid=true},
        .right={.position={.30F,-.25F,0},.orientation_valid=true,.position_valid=true},
        .allowed=true};

    bool At(float forward_m, std::uint64_t dt=25000) {
        sample.time_us+=dt;
        ++sample.sequence;
        sample.right.position.z=-forward_m;
        return motion.Update(sample);
    }

    bool Arm() {
        return At(.20F) || At(.08F) || At(-.06F) || At(-.19F) ||
            At(-.22F) || At(-.22F) || At(-.22F);
    }

    bool Forward() {
        return At(-.06F) || At(.10F) || At(.27F);
    }
};

bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main() {
    bool ok=true;
    Fixture cold;
    ok &= Check(!cold.At(-.22F) && !cold.At(-.22F) && !cold.At(-.22F) &&
        !cold.At(-.22F) && !cold.Forward(), "a cold behind pose armed a swing");
    Fixture passive;
    ok &= Check(!passive.Arm() && !passive.At(-.10F,100000) &&
        !passive.At(.02F,100000) && !passive.At(.14F,100000) &&
        !passive.At(.26F,100000), "a slow reach fired");
    Fixture valid;
    ok &= Check(!valid.Arm() && valid.Forward(), "deliberate behind-to-forward swing was missed");
    ok &= Check(!valid.motion.Update(valid.sample) && !valid.At(.31F) && !valid.At(.28F),
        "holding forward or replaying a sample repeated a throw");
    ok &= Check(!valid.Arm() && valid.Forward(), "fresh backswing did not rearm");
    Fixture reset;
    ok &= Check(!reset.Arm(), "backswing fired early");
    reset.sample.allowed=false;
    ok &= Check(!reset.At(-.13F), "suppressed context accepted motion");
    reset.sample.allowed=true;
    ok &= Check(!reset.Forward(), "context loss preserved an armed swing");
    for (auto member : {&CoJDynamiteMotionSample::owner,&CoJDynamiteMotionSample::player,
                        &CoJDynamiteMotionSample::context,&CoJDynamiteMotionSample::input_generation}) {
        Fixture changed;
        ok &= Check(!changed.Arm(), "backswing fired early");
        ++(changed.sample.*member);
        ok &= Check(!changed.Forward(), "a new owner inherited an armed swing");
    }
    Fixture raw;
    ok &= Check(!raw.Arm(), "backswing fired early");
    raw.sample.physical_fire=true;
    ok &= Check(!raw.At(-.06F) && !raw.At(.10F), "motion competed with physical fire");
    raw.sample.physical_fire=false;
    ok &= Check(!raw.At(.27F), "button release replayed a discarded swing");
    Fixture stale;
    ok &= Check(!stale.Arm() && !stale.motion.Update(stale.sample) &&
        !stale.At(-.06F,180000) && !stale.At(.10F) && !stale.At(.27F),
        "a long tracking gap preserved an armed swing");
    Fixture teleport;
    ok &= Check(!teleport.Arm() && !teleport.At(.35F), "a single tracking jump fired");
    Fixture head_only;
    ok &= Check(!head_only.Arm(), "backswing fired early");
    for (const float retreat : {.16F,.32F,.49F}) {
        head_only.sample.head.position.z=retreat;
        ok &= Check(!head_only.At(-.22F), "head motion fired with a stationary controller");
    }
    Fixture interrupted;
    ok &= Check(!interrupted.Arm(), "backswing fired early");
    interrupted.motion.Reset();
    ok &= Check(!interrupted.Forward(), "render interruption preserved an armed swing");
    cojvr::runtime::GameplayInputState controls{.active=true};
    ok &= Check(CoJDynamiteMotionControlsClear(controls), "neutral controls denied motion");
    controls.fire_left=true;
    ok &= Check(!CoJDynamiteMotionControlsClear(controls), "raw L2 allowed simultaneous motion");
    controls.fire_left=false;
    controls.fire_right=true;
    ok &= Check(!CoJDynamiteMotionControlsClear(controls), "raw R2 allowed simultaneous motion");
    controls.fire_right=false;
    controls.equipment_select[3]=true;
    ok &= Check(!CoJDynamiteMotionControlsClear(controls), "equipment selection allowed motion");
    return ok ? 0 : 1;
}
