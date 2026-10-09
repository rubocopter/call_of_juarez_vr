#pragma once
#include "runtime/hud_text.hpp"
#include "runtime/ui_haptics.hpp"
#include <algorithm>

namespace cojvr::runtime {
inline bool ReloadHapticInputAvailable(const GameplayInputState& raw, bool gameplay,
    bool polled, bool focused, bool dashboard, const Pose& head, const Pose& left,
    const Pose& right) noexcept {
    const auto valid=[](const Pose& p){
        const auto q=p.orientation;
        const double norm=static_cast<double>(q.x)*q.x+static_cast<double>(q.y)*q.y+
            static_cast<double>(q.z)*q.z+static_cast<double>(q.w)*q.w;
        return p.position_valid&&p.orientation_valid&&std::isfinite(p.position.x)&&
            std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&
            std::isfinite(norm)&&norm>.9&&norm<1.1;
    };
    return gameplay&&polled&&focused&&!dashboard&&raw.active&&(raw.digital_available&3U)==3U&&
        !raw.weapon_radial&&!raw.interact&&!raw.quick_save&&!raw.quick_load&&
        !raw.weapon_next&&!raw.weapon_previous&&!raw.hands&&!raw.discard_weapon&&
        !std::any_of(raw.equipment_select.begin(),raw.equipment_select.end(),[](bool v){return v;})&&
        valid(head)&&valid(left)&&valid(right);
}

// Observe only accepted new stereo uploads. Loss/reacquisition and replacement
// establish silent baselines. Each event is consumed before optional delivery.
class ReloadInsertionHaptics final {
public:
    static constexpr auto maximum_age=std::chrono::milliseconds(100);
    static constexpr auto minimum_interval=std::chrono::milliseconds(80);
    void UpdateOwner(std::uint64_t token) noexcept {
        if(token==0||token!=owner_token_)baseline_=false;
        owner_token_=token;
    }
    void UpdateContext(bool available,std::uint64_t input_generation,
        std::uint64_t native_token,UiHapticTime now) noexcept {
        const bool regression=have_clock_&&now<last_clock_;
        const bool usable=available&&GameplayUiFeedbackContextMailbox::Available(native_token);
        if(!usable||regression||!available_||input_generation!=input_generation_||native_token!=native_token_){
            baseline_=false;context_since_=now;
        }
        if(last_capture_!=UiHapticTime{}&&now>=last_capture_&&now-last_capture_>maximum_age)
            baseline_=false;
        available_=usable&&!regression;input_generation_=input_generation;native_token_=native_token;
        have_clock_=true;last_clock_=now;
    }
    std::optional<UiHapticPulse> Observe(const ReloadInsertionFeedback& event,std::uint64_t sequence,
        std::uint64_t generation,std::uint64_t device,UiHapticTime captured,UiHapticTime now,
        std::uint64_t native_token) noexcept {
        if(generation==0||device==0||generation<resource_generation_){baseline_=false;return {};}
        if(generation!=resource_generation_||device!=resource_device_){
            baseline_=false;resource_since_=now;resource_generation_=generation;resource_device_=device;
            // Capture sequences belong to a resource lifetime. A replacement
            // may restart at 1 even when the transport sequence stays monotonic.
            last_sequence_=0;
        }
        if(sequence==0||sequence<=last_sequence_)return {};
        last_sequence_=sequence;
        if(owner_token_==0||event.owner_token!=owner_token_||
            !available_||now<last_clock_||captured==UiHapticTime{}||captured>now||
            now-captured>maximum_age||captured<context_since_||captured<resource_since_||
            (last_capture_!=UiHapticTime{}&&captured<last_capture_)||
            native_token!=native_token_||event.input_generation!=input_generation_){
            baseline_=false;return {};
        }
        last_capture_=captured;
        const bool had_baseline=baseline_;baseline_=true;
        if(!had_baseline||(!event.accepted&&!event.zone_entered)||event.hand>=2||event.event_sequence!=sequence)return {};
        const auto hand=event.hand==0?UiHapticHand::right:UiHapticHand::left;
        if(event.accepted){
            if(have_pulse_&&(now<last_pulse_||now-last_pulse_<minimum_interval))return {};
            have_pulse_=true;last_pulse_=now;
            return UiHapticPulse{hand,.018F,120.F,.25F};
        }
        // Independent cadence: entering the zone must never suppress an
        // immediately following native-confirmed acceptance on trigger release.
        constexpr auto zone_interval=std::chrono::milliseconds(180);
        if(have_zone_pulse_&&(now<last_zone_pulse_||now-last_zone_pulse_<zone_interval))return {};
        have_zone_pulse_=true;last_zone_pulse_=now;
        return UiHapticPulse{hand,.009F,90.F,.10F};
    }
private:
    bool available_=false,baseline_=false,have_clock_=false,have_pulse_=false;
    bool have_zone_pulse_=false;
    std::uint64_t input_generation_=0,native_token_=0,resource_generation_=0,resource_device_=0,last_sequence_=0;
    std::uint64_t owner_token_=0;
    UiHapticTime context_since_{},resource_since_{},last_clock_{},last_capture_{},last_pulse_{};
    UiHapticTime last_zone_pulse_{};
};
}
