#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

namespace cojvr::runtime {
using BodyHapticTime = std::chrono::steady_clock::time_point;

// QA fixture only. No gameplay event, device layout or vendor pattern is implied.
enum class BodyHapticPattern : std::uint8_t { vest_test };
struct BodyHapticIdentity {
    // Nonzero opaque epochs. Session IDs must not be reused; sequence numbers
    // increase within a session and never restart on owner/input/reconnect edges.
    // Advance availability_epoch for loss/reconnect edges missed between polls.
    std::uint64_t session = 0, owner = 0, input_epoch = 0, availability_epoch = 0;
    bool operator==(const BodyHapticIdentity&) const = default;
};
struct BodyHapticContext {
    BodyHapticIdentity identity{};
    // Caller-owned aggregate availability; no OpenVR or device dependency.
    bool available = false;
};
struct BodyHapticEvent {
    BodyHapticIdentity identity{};
    std::uint64_t sequence = 0;
    BodyHapticTime occurred_at{};
    BodyHapticPattern pattern = BodyHapticPattern::vest_test;
    float intensity = 0.F; // Normalized (0, 1]; not a vendor multiplier.
};
struct BodyHapticCommand {
    BodyHapticIdentity identity{};
    std::uint64_t sequence = 0;
    BodyHapticPattern pattern = BodyHapticPattern::vest_test;
    float intensity = 0.F;
};

// Single-owner, injected-clock admission only. No output, queue or retry state.
// Observe consumes a same-session sequence before optional sink delivery, even
// when its payload is rejected. A returned command is for immediate consumption;
// retaining it for asynchronous dispatch requires a separate freshness contract.
class BodyHapticAdmission final {
public:
    // Matches the existing feedback age bound; this is not measured device latency.
    static constexpr auto maximum_age = std::chrono::milliseconds(100);

    // Poll also when there is no event, so a loss/reacquisition is not skipped.
    void UpdateContext(const BodyHapticContext& context, BodyHapticTime now) noexcept {
        if (shutdown_) return;
        const bool regressed = have_clock_ && now < last_now_;
        if (!have_context_ || context.identity.session != context_.identity.session) {
            last_sequence_ = 0;
        }
        if (!have_context_ || context.identity != context_.identity ||
            context.available != context_.available || !available_ || regressed) {
            baseline_ = false;
            context_since_ = now;
            have_event_time_ = false;
        }
        context_ = context;
        have_context_ = true;
        available_ = context.available && ValidIdentity(context.identity) &&
            now != BodyHapticTime{} && !regressed;
        if (!available_) baseline_ = false;
        // Retain the high-water mark across arbitrarily many regressed samples.
        if (!have_clock_ || now > last_now_) last_now_ = now;
        have_clock_ = true;
    }

    [[nodiscard]] std::optional<BodyHapticCommand> Observe(const BodyHapticEvent& event,
        const BodyHapticContext& context, BodyHapticTime now) noexcept {
        UpdateContext(context, now);
        if (shutdown_ || event.identity.session != context_.identity.session ||
            event.sequence == 0 || event.sequence <= last_sequence_) return {};
        last_sequence_ = event.sequence;
        if (!available_ || event.identity != context_.identity ||
            event.occurred_at == BodyHapticTime{} || event.occurred_at > now ||
            event.occurred_at < context_since_ || now - event.occurred_at > maximum_age ||
            (have_event_time_ && event.occurred_at < last_event_time_) ||
            event.pattern != BodyHapticPattern::vest_test || !std::isfinite(event.intensity) ||
            event.intensity <= 0.F || event.intensity > 1.F) {
            baseline_ = false;
            return {};
        }
        last_event_time_ = event.occurred_at;
        have_event_time_ = true;
        if (!baseline_) { baseline_ = true; return {}; }
        return BodyHapticCommand{event.identity, event.sequence, event.pattern, event.intensity};
    }

    // Terminal for this policy instance. No output resources are owned here.
    void Shutdown() noexcept { shutdown_ = true; available_ = false; baseline_ = false; }

private:
    static bool ValidIdentity(const BodyHapticIdentity& identity) noexcept {
        return identity.session != 0 && identity.owner != 0 && identity.input_epoch != 0 &&
            identity.availability_epoch != 0;
    }
    BodyHapticContext context_{};
    std::uint64_t last_sequence_ = 0;
    BodyHapticTime context_since_{}, last_now_{}, last_event_time_{};
    bool have_context_ = false, have_clock_ = false, have_event_time_ = false;
    bool available_ = false, baseline_ = false, shutdown_ = false;
};
} // namespace cojvr::runtime
