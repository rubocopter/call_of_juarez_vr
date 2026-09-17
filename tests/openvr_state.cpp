#include "runtime/openvr_state.hpp"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace cojvr::runtime;

    OpenVrProcessOwnerGate owner_gate;
    int first_owner = 1;
    int second_owner = 2;
    if (!owner_gate.TryClaim(&first_owner) ||
        !owner_gate.TryClaim(&first_owner) ||
        owner_gate.TryClaim(&second_owner) ||
        !owner_gate.owned_by(&first_owner)) {
        return Fail("OpenVR process-owner gate did not reject a competing owner");
    }
    owner_gate.Release(&second_owner);
    if (!owner_gate.owned_by(&first_owner)) {
        return Fail("non-owner release cleared OpenVR process ownership");
    }
    owner_gate.Release(&first_owner);
    if (owner_gate.owned_by(&first_owner) || !owner_gate.TryClaim(&second_owner)) {
        return Fail("OpenVR process-owner gate did not release/reacquire ownership");
    }
    owner_gate.Release(&second_owner);

    OpenVrStateTracker state;
    state.BeginInitialize();
    if (state.state().lifecycle != OpenVrLifecycleState::initializing ||
        state.state().initialized) {
        return Fail("OpenVR state did not enter initializing");
    }

    state.InitializationSucceeded(true);
    state.FocusChanged(true);
    state.TrackingChanged(true);
    state.EyeSubmitResult(Eye::left, true);
    state.EyeSubmitResult(Eye::right, true);
    if (!state.state().initialized || !state.state().connected ||
        !state.state().focused || !state.state().tracking_valid ||
        !state.state().presenting) {
        return Fail("OpenVR ready/presenting state was not established");
    }

    // Focus is observable independently from compositor submission. Losing
    // focus must not fabricate a disconnect or invalid tracking state.
    state.FocusChanged(false);
    if (state.state().focused || !state.state().connected ||
        !state.state().tracking_valid) {
        return Fail("OpenVR focus-loss state corrupted connection/tracking");
    }

    // A partial stereo pair is not a successful presentation.
    state.EyeSubmitResult(Eye::left, true);
    state.EyeSubmitResult(Eye::right, false);
    if (state.state().presenting) {
        return Fail("one-eye submit failure remained presenting");
    }

    state.EyeSubmitResult(Eye::left, true);
    state.EyeSubmitResult(Eye::right, true);
    state.TrackingChanged(false);
    if (state.state().tracking_valid || state.state().presenting) {
        return Fail("invalid tracking did not stop presentation");
    }

    state.TrackingChanged(true);
    state.EyeSubmitResult(Eye::left, true);
    state.EyeSubmitResult(Eye::right, true);
    state.ConnectionChanged(false);
    if (state.state().connected || state.state().focused ||
        state.state().tracking_valid || state.state().presenting) {
        return Fail("runtime/HMD disconnect retained active session state");
    }

    state.ConnectionChanged(true);
    state.TrackingChanged(true);
    state.ShutdownRequested();
    if (!state.state().shutdown_requested || state.state().presenting ||
        state.state().lifecycle != OpenVrLifecycleState::shutdown_requested) {
        return Fail("shutdown request did not quiesce presentation");
    }
    state.ShutdownComplete();
    if (state.state().initialized || state.state().connected ||
        state.state().tracking_valid || state.state().presenting ||
        state.state().shutdown_requested ||
        state.state().lifecycle != OpenVrLifecycleState::shutdown_complete) {
        return Fail("shutdown completion retained live OpenVR state");
    }

    state.BeginInitialize();
    state.InitializationFailed();
    if (state.state().lifecycle != OpenVrLifecycleState::failed ||
        state.state().initialized) {
        return Fail("initialization failure retained initialized state");
    }

    std::cout << "OpenVR lifecycle/failure state policy passed\n";
    return 0;
}
