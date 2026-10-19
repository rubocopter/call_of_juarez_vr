#pragma once
#include "runtime/vr_types.hpp"
#include <cstdint>
#include <cmath>
#include <limits>

namespace cojvr::games::call_of_juarez {
struct CoJDuelPadParameters { float sensitivity_x=0,sensitivity_y=0,threshold_x=0,threshold_y=0; };
struct CoJDuelInputOwner { std::uint64_t player_generation=0; std::uint32_t action_identity=0; float game_time=0; };
struct CoJDuelMouseAxes { float x=0,y=0;bool dispatch=false; };
struct CoJDuelDrawParameters {
    bool hands_empty=false;
    float input_sensitivity=0,mouse_sensitivity_y=0,real_dt=0,minimum_displacement=0;
};
class CoJDuelInputState {
public:
    // Retain the consumed clock fence across faults/owner loss. Recovery must
    // observe a center; it cannot retry a partially delivered sample.
    void Invalidate() noexcept { known_=false;armed_=false; }
    CoJDuelMouseAxes Update(const runtime::GameplayInputState& input,CoJDuelInputOwner owner,
        CoJDuelPadParameters pad,std::uint64_t monotonic_us,
        CoJDuelDrawParameters draw={}) noexcept {
        const auto finite_nonnegative=[](float value){return std::isfinite(value)&&value>=0;};
        if(!input.active||!input.turn_available||input.weapon_radial||!owner.player_generation||
            !owner.action_identity||owner.action_identity==0xffffffffU||!monotonic_us||
            !finite_nonnegative(owner.game_time)||!std::isfinite(input.turn.x)||
            !std::isfinite(input.turn.y)||std::fabs(input.turn.x)>1||std::fabs(input.turn.y)>1||
            !std::isfinite(pad.sensitivity_x)||pad.sensitivity_x<=0||
            !std::isfinite(pad.sensitivity_y)||pad.sensitivity_y<=0||
            !finite_nonnegative(pad.threshold_x)||!finite_nonnegative(pad.threshold_y)) {
            Invalidate();return {};
        }
        const bool changed=!known_||generation_!=owner.player_generation||
            action_!=owner.action_identity||context_!=input.input_context_generation;
        if(changed){armed_=false;known_=true;generation_=owner.player_generation;
            action_=owner.action_identity;context_=input.input_context_generation;}
        if(clock_known_&&monotonic_us<last_monotonic_us_){Invalidate();return {};}
        if(clock_known_&&monotonic_us==last_monotonic_us_)return {};
        last_monotonic_us_=monotonic_us;
        if(clock_known_&&clock_generation_==owner.player_generation&&owner.game_time<last_game_time_) {
            last_game_time_=owner.game_time;Invalidate();return {};
        }
        // A stereo pair, bridge phase or faster HMD refresh must not create
        // another native input event at the same naturally advancing game time.
        if(clock_known_&&clock_generation_==owner.player_generation&&owner.game_time==last_game_time_)
            return {};
        clock_known_=true;clock_generation_=owner.player_generation;last_game_time_=owner.game_time;
        const auto shape=[](float axis){
            // Shipped GameInputController.OnInputJoyAxis, before pad-as-mouse.
            return std::fabs(axis)<.3F?0.0F:(axis>0?(axis-.3F)/.7F:(axis+.3F)/.7F);
        };
        const float x=-shape(input.turn.x)*pad.sensitivity_x;
        // XR +Y is up. The native joystick's +Y is down and is negated
        // by OnInputJoyAxis(joy,5,value); preserve the resulting mouse sign.
        const float y=shape(input.turn.y)*pad.sensitivity_y;
        if(!std::isfinite(x)||!std::isfinite(y)){Invalidate();return {};}
        const float admitted_x=std::fabs(x)<pad.threshold_x?0.0F:x;
        float admitted_y=std::fabs(y)<pad.threshold_y?0.0F:y;
        if(!armed_){if(shape(input.turn.x)==0&&shape(input.turn.y)==0)armed_=true;return {};}
        if(draw.hands_empty&&admitted_y<0) {
            if(!std::isfinite(draw.input_sensitivity)||draw.input_sensitivity<=0||
                !std::isfinite(draw.mouse_sensitivity_y)||draw.mouse_sensitivity_y<=0||
                !std::isfinite(draw.real_dt)||draw.real_dt<=0||
                !std::isfinite(draw.minimum_displacement)||draw.minimum_displacement<=0) {
                Invalidate();return {};
            }
            // A centered -> down gesture means one native minimum draw
            // displacement PLUS this sample's ordinary pad displacement.
            // Convert displacement back to mouse rate with the current native
            // clock/sensitivities. This is not a fixed gain or an equip call.
            const double minimum_rate=static_cast<double>(draw.minimum_displacement)*
                draw.mouse_sensitivity_y/(static_cast<double>(draw.input_sensitivity)*draw.real_dt);
            admitted_y=static_cast<float>(static_cast<double>(admitted_y)-minimum_rate);
            const auto qualifies=[&](float raw){
                const float rotation=raw*draw.input_sensitivity;
                const float displacement=rotation*draw.real_dt;
                const float native_value=displacement/draw.mouse_sensitivity_y;
                return std::isfinite(native_value)&&native_value < -draw.minimum_displacement;
            };
            // Bound correction of rounding through the native float operations;
            // do not silently admit an overflow or invent a fallback gain.
            for(int i=0;i<8&&std::isfinite(admitted_y)&&!qualifies(admitted_y);++i)
                admitted_y=std::nextafter(admitted_y,-std::numeric_limits<float>::infinity());
            if(!std::isfinite(admitted_y)||!qualifies(admitted_y)){Invalidate();return {};}
        }
        return {admitted_x,admitted_y,admitted_x!=0||admitted_y!=0};
    }
private:
    std::uint64_t generation_=0,context_=0,last_monotonic_us_=0,clock_generation_=0;
    std::uint32_t action_=0;
    float last_game_time_=0;
    bool known_=false,armed_=false,clock_known_=false;
};
}
