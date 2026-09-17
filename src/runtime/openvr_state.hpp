#pragma once

#include "runtime/vr_types.hpp"

#include <cstdint>
#include <mutex>

namespace cojvr::runtime {

enum class OpenVrLifecycleState : std::uint8_t {
    idle,
    initializing,
    ready,
    shutdown_requested,
    shutdown_complete,
    failed,
};

struct OpenVrRuntimeState {
    OpenVrLifecycleState lifecycle = OpenVrLifecycleState::idle;
    bool initialized = false;
    bool connected = false;
    bool focused = false;
    bool tracking_valid = false;
    bool presenting = false;
    bool shutdown_requested = false;
};

// Process-owner arbitration policy. OpenVrRuntime uses one process-global
// instance; the type itself is host-testable without loading the OpenVR SDK.
class OpenVrProcessOwnerGate final {
public:
    [[nodiscard]] bool TryClaim(void* owner) noexcept {
        if (!owner) return false;
        try {
            std::lock_guard lock(mutex_);
            if (owner_ != nullptr && owner_ != owner) return false;
            owner_ = owner;
            return true;
        } catch (...) {
            return false;
        }
    }

    void Release(void* owner) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (owner_ == owner) owner_ = nullptr;
        } catch (...) {
        }
    }

    [[nodiscard]] bool owned_by(void* owner) const noexcept {
        try {
            std::lock_guard lock(mutex_);
            return owner_ != nullptr && owner_ == owner;
        } catch (...) {
            return false;
        }
    }

private:
    mutable std::mutex mutex_;
    void* owner_ = nullptr;
};

// Pure state policy kept independent from the OpenVR SDK so lifecycle and
// failure recovery can be exercised in host tests without a running runtime.
class OpenVrStateTracker final {
public:
    void BeginInitialize() noexcept {
        state_ = {};
        state_.lifecycle = OpenVrLifecycleState::initializing;
    }

    void InitializationSucceeded(const bool connected) noexcept {
        state_.lifecycle = OpenVrLifecycleState::ready;
        state_.initialized = true;
        state_.connected = connected;
    }

    void InitializationFailed() noexcept {
        state_ = {};
        state_.lifecycle = OpenVrLifecycleState::failed;
    }

    void ConnectionChanged(const bool connected) noexcept {
        if (!state_.initialized) return;
        state_.connected = connected;
        if (!connected) {
            state_.focused = false;
            state_.tracking_valid = false;
            state_.presenting = false;
            left_submit_seen_ = false;
            left_submit_ok_ = false;
        }
    }

    void FocusChanged(const bool focused) noexcept {
        state_.focused = state_.initialized && state_.connected && focused;
    }

    void TrackingChanged(const bool valid) noexcept {
        state_.tracking_valid = state_.initialized && state_.connected && valid;
        if (!state_.tracking_valid) state_.presenting = false;
    }

    void EyeSubmitResult(const Eye eye, const bool succeeded) noexcept {
        if (!state_.initialized || !state_.connected || !state_.tracking_valid ||
            state_.shutdown_requested) {
            state_.presenting = false;
            left_submit_seen_ = false;
            left_submit_ok_ = false;
            return;
        }

        if (eye == Eye::left) {
            left_submit_seen_ = true;
            left_submit_ok_ = succeeded;
            if (!succeeded) state_.presenting = false;
            return;
        }

        state_.presenting = left_submit_seen_ && left_submit_ok_ && succeeded;
        left_submit_seen_ = false;
        left_submit_ok_ = false;
    }

    void ShutdownRequested() noexcept {
        if (!state_.initialized) return;
        state_.lifecycle = OpenVrLifecycleState::shutdown_requested;
        state_.shutdown_requested = true;
        state_.presenting = false;
    }

    void ShutdownComplete() noexcept {
        state_ = {};
        state_.lifecycle = OpenVrLifecycleState::shutdown_complete;
        left_submit_seen_ = false;
        left_submit_ok_ = false;
    }

    [[nodiscard]] const OpenVrRuntimeState& state() const noexcept { return state_; }

private:
    OpenVrRuntimeState state_{};
    bool left_submit_seen_ = false;
    bool left_submit_ok_ = false;
};

} // namespace cojvr::runtime
