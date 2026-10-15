#pragma once
#include <d3d9.h>
#include <array>
#include <mutex>
#include "runtime/hmd_frame_pacing.hpp"

namespace cojvr::backends::d3d9 {
// Passive presentation retains the most recent successful queue through short
// producer stalls. Admission must neither erase nor renew that timestamp.
inline std::uint64_t NativeStereoAdmissionTick(std::uint64_t last_queued,
    std::uint64_t now, bool settings_captured, bool immersive_movie,
    bool invalidate = false) noexcept {
    return invalidate ? 0 : ((settings_captured || immersive_movie) ? last_queued : now);
}
inline bool NativeStereoFrameFresh(std::uint64_t last_queued, std::uint64_t now) noexcept {
    return last_queued != 0 && now >= last_queued && now - last_queued < 250;
}
// Register only successful current-owner fences. Keep the three most recent
// records, matching the producer's three slots, so abandoned collections cannot
// consume bookkeeping capacity permanently.
// Old callbacks cannot register a fence or erase a newer owner's pending pair.
class NativeStereoQueuePolicy final {
public:
    std::uint64_t ObserveOwner(std::uint64_t owner) noexcept {
        std::lock_guard lock(mutex_);
        if (owner != owner_) {
            InvalidateLocked();
            owner_ = owner;
        }
        return owner_ ? epoch_ : 0;
    }
    void Admit(std::uint64_t epoch, std::uint64_t now,
        bool settings_captured, bool immersive_movie) noexcept {
        std::lock_guard lock(mutex_);
        if (owner_ && epoch == epoch_ && last_queued_tick_)
            last_queued_tick_ = NativeStereoAdmissionTick(
                last_queued_tick_, now, settings_captured, immersive_movie);
    }
    bool BindCapture(std::uint64_t epoch, std::uint64_t sequence) noexcept {
        std::lock_guard lock(mutex_);
        if (!owner_ || epoch != epoch_ || sequence <= capture_sequence_) return false;
        capture_sequence_ = sequence;
        capture_epoch_ = epoch;
        return true;
    }
    std::uint64_t CaptureEpoch(std::uint64_t sequence) noexcept {
        std::lock_guard lock(mutex_);
        return owner_ && sequence == capture_sequence_ && capture_epoch_ == epoch_ ? capture_epoch_ : 0;
    }
    bool Queued(std::uint64_t epoch, std::uint64_t sequence, std::uint64_t now) noexcept {
        std::lock_guard lock(mutex_);
        if (!owner_ || epoch != epoch_ || sequence <= last_sequence_) return false;
        pending_[next_record_++ % pending_.size()] = sequence;
        last_sequence_ = sequence;
        last_queued_tick_ = now;
        return true;
    }
    bool Accept(std::uint64_t sequence) noexcept {
        std::lock_guard lock(mutex_);
        if (!owner_ || !sequence) return false;
        for (auto& pending : pending_) {
            if (pending != sequence) continue;
            pending = 0;
            return true;
        }
        return false;
    }
    bool Fresh(std::uint64_t now) noexcept {
        std::lock_guard lock(mutex_);
        return owner_ && published_ && NativeStereoFrameFresh(last_queued_tick_, now);
    }
    bool Published(std::uint64_t epoch) noexcept {
        std::lock_guard lock(mutex_);
        if (!owner_ || epoch != epoch_) return false;
        published_ = true;
        return true;
    }
    void FlatPublished() noexcept {
        std::lock_guard lock(mutex_);
        published_ = false;
    }
    void Invalidate() noexcept {
        std::lock_guard lock(mutex_);
        InvalidateLocked();
    }
private:
    void InvalidateLocked() noexcept {
        ++epoch_;
        last_queued_tick_ = 0;
        published_ = false;
        capture_epoch_ = 0;
        pending_ = {};
    }
    std::mutex mutex_;
    std::uint64_t owner_ = 0, epoch_ = 1;
    std::uint64_t last_sequence_ = 0, last_queued_tick_ = 0;
    std::uint64_t capture_sequence_ = 0, capture_epoch_ = 0;
    std::array<std::uint64_t, 3> pending_{};
    std::size_t next_record_ = 0;
    bool published_ = false;
};
// The desktop display mode stays game-owned. Only remove its vsync wait
// when a live VR owner supplies a valid headset cadence.
inline void ApplyVrPresentPolicy(D3DPRESENT_PARAMETERS& parameters, bool vr_paced) noexcept {
    if (vr_paced) parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}
// Keep a single timing sample paired with each game-owned device transition.
// Only a successful transition commits both cadence and interval ownership.
class VrPresentPolicy final {
public:
    bool Begin(D3DPRESENT_PARAMETERS& parameters, float reported_hz, bool reset) noexcept {
        pending_requested_ = parameters.PresentationInterval;
        if (reset && injected_ && pending_requested_ == D3DPRESENT_INTERVAL_IMMEDIATE)
            pending_requested_ = requested_;
        pending_hz_ = runtime::HmdFramePacer::ValidRefresh(reported_hz) ? reported_hz : 0.0F;
        pending_ = true;
        parameters.PresentationInterval = pending_requested_;
        ApplyVrPresentPolicy(parameters, pending_hz_ > 0.0F);
        return pending_hz_ > 0.0F;
    }
    void Complete(bool success, D3DPRESENT_PARAMETERS& parameters, runtime::VrFrameCadence& cadence) noexcept {
        if (pending_ && success) {
            requested_ = pending_requested_;
            injected_ = pending_hz_ > 0.0F && parameters.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE;
            (void)cadence.Target(pending_hz_, parameters.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE);
        } else if (pending_) {
            // A game may retry this same structure after a failed Create/Reset.
            parameters.PresentationInterval = pending_requested_;
        }
        pending_ = false;
    }
private:
    DWORD requested_ = D3DPRESENT_INTERVAL_DEFAULT;
    DWORD pending_requested_ = D3DPRESENT_INTERVAL_DEFAULT;
    float pending_hz_ = 0.0F;
    bool injected_ = false;
    bool pending_ = false;
};
}
