#pragma once
#include "runtime/gameplay_utility.hpp"
#include "runtime/vr_settings.hpp"
#include <algorithm>
#include <cmath>

namespace cojvr::runtime {
// Returns physical yaw (right positive); the game adapter owns native signs.
// A changed mode/owner, unavailable source or discontinuous clock requires a
// fresh neutral sample. Neither a switch nor a recovered held stick can turn.
class GameplayTurnState final {
public:
    [[nodiscard]] float Update(const GameplayInputState& input,std::uint64_t time_us,
        TurnMode mode) noexcept {
        const bool valid_mode=mode==TurnMode::smooth||mode==TurnMode::snap;
        const bool available=input.active&&input.turn_available&&!input.weapon_radial&&valid_mode&&
            std::isfinite(input.turn.x)&&std::isfinite(input.turn.y);
        const bool interrupted=!known_||!available||mode!=mode_||
            context_!=input.input_context_generation||time_us<previous_us_||time_us-previous_us_>100000;
        const bool duplicate=known_&&time_us==previous_us_;
        if(interrupted){smooth_={};armed_=latched_=false;}
        known_=available;mode_=mode;context_=input.input_context_generation;previous_us_=time_us;
        if(!available)return 0;
        if(mode==TurnMode::smooth)return smooth_.Update(input,time_us);
        if(duplicate&&!interrupted)return 0;
        if(std::hypot(input.turn.x,input.turn.y)<=.35F){armed_=true;latched_=false;return 0;}
        if(!armed_||latched_||std::fabs(input.turn.x)<.70F)return 0;
        latched_=true;return input.turn.x>0?45.0F:-45.0F;
    }
private:
    SmoothTurnState smooth_{};
    TurnMode mode_=TurnMode::smooth;
    std::uint64_t previous_us_=0,context_=0;
    bool known_=false,armed_=false,latched_=false;
};
}
