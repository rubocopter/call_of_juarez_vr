#pragma once
#include "runtime/vr_types.hpp"
#include <algorithm>
#include <cmath>

namespace cojvr::games::call_of_juarez {
class CoJMountedInputComfort {
    std::uint64_t player_=0,input_=0;
    int steering_=0;
    bool ready_=false;
public:
    void Invalidate() noexcept { *this={}; }
    // Intentional comfort thresholds in raw controller space, not measured
    // horse physics. Native sign-only reins need a deliberate lateral intent.
    // On-foot input still receives its exact original 0.04 shaping downstream.
    runtime::GameplayInputState Update(runtime::GameplayInputState input,
        bool known, bool mounted, std::uint64_t generation, bool gallop) noexcept {
        if(known&&!mounted&&generation) { Invalidate();return input; }
        if(!input.active||!known||!generation) {
            Invalidate();input.move={};input.walk=input.run=false;return input;
        }
        input.walk=!gallop; // Only release-qualified native R1 can lift walk.
        if(!input.move_available||!std::isfinite(input.move.x)||!std::isfinite(input.move.y)) {
            Invalidate();input.move={};return input;
        }
        if(player_!=generation||input_!=input.input_context_generation) {
            Invalidate();player_=generation;input_=input.input_context_generation;
        }
        const float x=std::clamp(input.move.x,-1.F,1.F);
        const float y=std::clamp(input.move.y,-1.F,1.F);
        // Horse reverses the rein target at low speed for any negative forward
        // request. The recorded false reverse inputs were below 40% of lateral
        // travel. Ignore shallow backward diagonals even before reins engage.
        constexpr float reverse_to_lateral_ratio=.50F;
        if(y<0.F&&-y<=reverse_to_lateral_ratio*std::abs(x))input.move.y=0.F;
        const float enter=std::max(.35F,.55F*std::abs(y));
        const float release=std::max(.20F,.35F*std::abs(y));
        if(std::abs(x)<=.20F)ready_=true;
        if(!ready_)steering_=0;
        else if(std::abs(x)>=enter)steering_=x<0?-1:1;
        else if(std::abs(x)<=release||x*steering_<=0)steering_=0;
        input.move.x=steering_?x:0.F;
        return input;
    }
};
} // namespace cojvr::games::call_of_juarez
