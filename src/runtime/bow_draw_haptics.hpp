#pragma once
#include "runtime/hud_text.hpp"
#include "runtime/ui_haptics.hpp"

namespace cojvr::runtime {
// Consume only accepted new stereo uploads. Every ownership/availability/resource
// boundary establishes a silent baseline. Optional output cannot queue a retry.
class BowDrawHaptics final {
public:
    static constexpr auto maximum_age = std::chrono::milliseconds(100);
    static constexpr auto minimum_interval = std::chrono::milliseconds(80);

    void UpdateOwner(std::uint64_t token) noexcept {
        if (token == 0 || token != owner_token_) baseline_ = false;
        owner_token_ = token;
    }
    void UpdateContext(bool available, std::uint64_t input_generation,
        std::uint64_t native_token, UiHapticTime now) noexcept {
        const bool regression = have_clock_ && now < last_clock_;
        const bool usable = available && GameplayUiFeedbackContextMailbox::Available(native_token);
        if (!usable || regression || !available_ || input_generation != input_generation_ ||
            native_token != native_token_) {
            baseline_ = false;
            context_since_ = now;
        }
        if (last_capture_ != UiHapticTime{} && now >= last_capture_ && now-last_capture_ > maximum_age)
            baseline_ = false;
        available_ = usable && !regression;
        input_generation_ = input_generation;
        native_token_ = native_token;
        have_clock_ = true;
        last_clock_ = now;
    }
    std::optional<UiHapticPulse> Observe(const BowDrawFeedback& event, std::uint64_t sequence,
        std::uint64_t generation, std::uint64_t device, UiHapticTime captured,
        UiHapticTime now, std::uint64_t native_token) noexcept {
        if (generation == 0 || device == 0 || generation < resource_generation_) {
            baseline_ = false;
            return {};
        }
        if (generation != resource_generation_ || device != resource_device_) {
            baseline_ = false;
            resource_since_ = now;
            resource_generation_ = generation;
            resource_device_ = device;
            // Capture numbering may restart for a replacement resource.
            last_sequence_ = 0;
        }
        if (sequence == 0 || sequence <= last_sequence_) return {};
        last_sequence_ = sequence;
        if (owner_token_ == 0 || event.owner_token != owner_token_ || !available_ ||
            now < last_clock_ || captured == UiHapticTime{} || captured > now ||
            now-captured > maximum_age || captured < context_since_ || captured < resource_since_ ||
            (last_capture_ != UiHapticTime{} && captured < last_capture_) ||
            native_token != native_token_ || event.input_generation != input_generation_) {
            baseline_ = false;
            return {};
        }
        last_capture_ = captured;
        const bool had_baseline = baseline_;
        baseline_ = true;
        if (!had_baseline || event.event_sequence != sequence) return {};
        switch (event.kind) {
        case BowDrawFeedbackKind::grab:
            return UiHapticPulse{UiHapticHand::right, .008F, 90.F, .12F};
        case BowDrawFeedbackKind::tension:
            if (!std::isfinite(event.tension) || event.tension < 0.F || event.tension > 1.F ||
                (have_tension_pulse_ && (now < last_tension_pulse_ ||
                    now-last_tension_pulse_ < minimum_interval))) return {};
            have_tension_pulse_ = true;
            last_tension_pulse_ = now;
            return UiHapticPulse{UiHapticHand::right, .008F, 90.F, .08F + .12F*event.tension};
        case BowDrawFeedbackKind::release:
            // Independent of tension cadence. This is string release/request,
            // not confirmation of a native arrow, damage or ammunition change.
            return UiHapticPulse{UiHapticHand::right, .016F, 120.F, .25F};
        default:
            return {};
        }
    }
private:
    bool available_ = false, baseline_ = false, have_clock_ = false, have_tension_pulse_ = false;
    std::uint64_t owner_token_ = 0, input_generation_ = 0, native_token_ = 0;
    std::uint64_t resource_generation_ = 0, resource_device_ = 0, last_sequence_ = 0;
    UiHapticTime context_since_{}, resource_since_{}, last_clock_{}, last_capture_{}, last_tension_pulse_{};
};
}
