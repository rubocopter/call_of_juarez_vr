#pragma once
#include "runtime/gameplay_ui.hpp"
#include <chrono>
#include <atomic>
#include <cmath>
#include <optional>

namespace cojvr::runtime {
using UiHapticTime = std::chrono::steady_clock::time_point;
// One value-only atomic carries both availability and its transition epoch.
// Repeated observations do not churn the token; missed loss/reacquire edges
// still invalidate every capture made under the previous ownership.
class GameplayUiFeedbackContextMailbox final {
public:
    void Reset() noexcept {
        auto previous = token_.load(std::memory_order_acquire);
        while (!token_.compare_exchange_weak(previous,
            (previous & ~std::uint64_t{1}) + 2, std::memory_order_acq_rel)) {}
    }
    void Publish(bool available) noexcept {
        auto previous = token_.load(std::memory_order_acquire);
        while (Available(previous) != available) {
            const auto next = ((previous & ~std::uint64_t{1}) + 2) | (available ? 1U : 0U);
            if (token_.compare_exchange_weak(previous, next, std::memory_order_acq_rel)) return;
        }
    }
    std::uint64_t Snapshot() const noexcept { return token_.load(std::memory_order_acquire); }
    static bool Available(std::uint64_t token) noexcept { return (token & 1U) != 0; }
    bool Matches(std::uint64_t token) const noexcept { return token == Snapshot() && Available(token); }
private:
    std::atomic<std::uint64_t> token_{0};
};
template<class Diagnostic> void OptionalUiHapticDiagnostic(Diagnostic&& diagnostic) noexcept {
    try { diagnostic(); } catch (...) {}
}
enum class UiHapticHand : std::uint8_t { left, right };
struct UiHapticPulse {
    UiHapticHand hand = UiHapticHand::right;
    float duration_seconds = .012F;
    float frequency_hz = 120.0F;
    float amplitude = .18F;
};
// Feedback bounds only; these values never tune native physics or input.
inline bool UiHapticPulseValid(const UiHapticPulse& pulse) noexcept {
    return (pulse.hand == UiHapticHand::left || pulse.hand == UiHapticHand::right) &&
        std::isfinite(pulse.duration_seconds) && pulse.duration_seconds > 0 && pulse.duration_seconds <= .02F &&
        std::isfinite(pulse.frequency_hz) && pulse.frequency_hz > 0 && pulse.frequency_hz <= 160 &&
        std::isfinite(pulse.amplitude) && pulse.amplitude > 0 && pulse.amplitude <= .25F;
}
inline bool WheelHapticInputAvailable(const GameplayInputState& raw, bool native_gameplay,
    bool polled, bool focused, bool dashboard, const Pose& head, const Pose& hand) noexcept {
    const auto valid_pose = [](const Pose& pose) {
        const auto q = pose.orientation;
        const float norm = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
        return pose.position_valid && pose.orientation_valid &&
            std::isfinite(pose.position.x) && std::isfinite(pose.position.y) && std::isfinite(pose.position.z) &&
            std::isfinite(norm) && norm > .9F && norm < 1.1F;
    };
    return native_gameplay && polled && focused && !dashboard && raw.active && raw.weapon_radial &&
        raw.radial_available && raw.turn_available && std::isfinite(raw.turn.x) && std::isfinite(raw.turn.y) &&
        valid_pose(head) && valid_pose(hand);
}
// Optional outputs are independent from gameplay initialization. An API/device
// failure disables that hand until action initialization; there is no retry queue.
class UiHapticOutputGate final {
public:
    void Configure(UiHapticHand hand, bool ready) noexcept {
        if (hand == UiHapticHand::left || hand == UiHapticHand::right)
            ready_[static_cast<std::size_t>(hand)] = ready;
    }
    void Failed(UiHapticHand hand) noexcept { Configure(hand, false); }
    bool Available(UiHapticHand hand) const noexcept {
        return (hand == UiHapticHand::left || hand == UiHapticHand::right) && ready_[static_cast<std::size_t>(hand)];
    }
    template<class Send> bool Dispatch(const UiHapticPulse& pulse, Send&& send) noexcept {
        if (!Available(pulse.hand) || !UiHapticPulseValid(pulse)) return false;
        try {
            if (send()) return true;
        } catch (...) {}
        Failed(pulse.hand);
        return false;
    }
private:
    std::array<bool,2> ready_{};
};
// Value-only highlight feedback. The renderer supplies only accepted new frame
// metadata and updates current availability at every poll (including repeats).
// Sequence/time, context and resource boundaries establish silent baselines.
// A returned pulse consumes the change immediately, even if delivery fails.
class EquipmentWheelHaptics final {
public:
    static constexpr auto maximum_age = std::chrono::milliseconds(100);
    static constexpr auto minimum_interval = std::chrono::milliseconds(80);
    void UpdateNativeContext(std::uint64_t token, UiHapticTime now) noexcept {
        if (!have_native_context_ || token != native_context_token_) {
            have_highlight_ = false;
            native_since_ = now;
        }
        have_native_context_ = true;
        native_context_token_ = token;
        if (!GameplayUiFeedbackContextMailbox::Available(token)) have_highlight_ = false;
    }
    void UpdateContext(bool available, std::uint64_t generation, UiHapticTime now) noexcept {
        const bool regressed = have_clock_ && now < last_clock_;
        if (!available || regressed || !available_ || generation != context_generation_) {
            have_highlight_ = false;
            available_since_ = now;
        }
        if (last_capture_ != UiHapticTime{} && now >= last_capture_ && now-last_capture_ > maximum_age)
            have_highlight_ = false;
        available_ = available && !regressed;
        context_generation_ = generation;
        have_clock_ = true;
        last_clock_ = now;
    }
    std::optional<UiHapticPulse> Observe(const EquipmentWheelSnapshot& wheel, std::uint64_t sequence,
        std::uint64_t generation, std::uint64_t device, UiHapticTime captured, UiHapticTime now,
        std::uint64_t native_token = 0) noexcept {
        if (sequence == 0 || sequence <= last_sequence_ || generation < resource_generation_) return {};
        last_sequence_ = sequence;
        if (generation != resource_generation_ || device != resource_device_) {
            have_highlight_ = false;
            resource_generation_ = generation; resource_device_ = device;
            // All captures already queued before the replacement was observed
            // are discarded, including a second sample from the new resource.
            resource_since_ = now;
        }
        if ((have_native_context_ &&
             (!GameplayUiFeedbackContextMailbox::Available(native_context_token_) ||
              native_token != native_context_token_ || captured < native_since_)) ||
            !available_ || now < last_clock_ || captured == UiHapticTime{} || captured > now ||
            now-captured > maximum_age || captured < available_since_ || captured < resource_since_ ||
            (last_capture_ != UiHapticTime{} && captured < last_capture_) || !wheel.valid || !wheel.active ||
            wheel.selected < -1 || wheel.selected >= static_cast<int>(wheel.labels.size())) {
            have_highlight_ = false; return {};
        }
        last_capture_ = captured;
        const bool changed = have_highlight_ && highlight_ != wheel.selected;
        highlight_ = wheel.selected; have_highlight_ = true;
        if (!changed || highlight_ < 0 || (wheel.available_mask & (1U << highlight_)) == 0 ||
            (have_pulse_ && (now < last_pulse_ || now-last_pulse_ < minimum_interval))) return {};
        have_pulse_ = true; last_pulse_ = now;
        return UiHapticPulse{};
    }
private:
    bool available_ = false, have_highlight_ = false, have_clock_ = false, have_pulse_ = false;
    int highlight_ = -1;
    std::uint64_t context_generation_ = 0, resource_generation_ = 0, resource_device_ = 0, last_sequence_ = 0;
    UiHapticTime available_since_{}, resource_since_{}, last_clock_{}, last_capture_{}, last_pulse_{};
    bool have_native_context_ = false;
    std::uint64_t native_context_token_ = 0;
    UiHapticTime native_since_{};
};
} // namespace cojvr::runtime
