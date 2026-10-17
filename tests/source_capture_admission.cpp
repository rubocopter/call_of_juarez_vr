#include "backends/d3d9/source_capture_admission.hpp"
#include "backends/d3d9/vr_present_policy.hpp"
#include "backends/openvr/fixed_output_policy.hpp"
#include <iostream>

using cojvr::backends::d3d9::NativeStereoQueuePolicy;
using cojvr::backends::d3d9::SourceCaptureAdmission;
using cojvr::backends::openvr::FixedOutputFrameAuthority;

bool Bootstrap() {
    NativeStereoQueuePolicy queue;
    SourceCaptureAdmission admission;
    FixedOutputFrameAuthority output;
    const auto owner = queue.ObserveOwner(7);
    const auto original_token = output.Epoch();
    admission.BeginFrame();
    if (!queue.BindCapture(owner, 1) || !queue.Queued(owner, 1, 100)) return false;
    admission.Queued(1);
    // Reproduce the first-pair condition: fenced, not collected/published yet.
    if (queue.Fresh(101)) return false;
    const bool preserve = admission.PreserveForPresent(1, true,
        queue.CaptureEpoch(1) == owner, output.Allows(original_token));
    if (!preserve) output.Invalidate(); // Flat capture would change source mode.
    if (!preserve || !output.Allows(original_token) || queue.Fresh(101)) return false;
    admission.BeginFrame(); // Next Begin collects using the ORIGINAL token.
    return queue.Accept(1) && output.Allows(original_token) && queue.Published(owner) &&
        queue.Fresh(102) && !queue.Fresh(350) && admission.queued_sequence() == 0;
}

bool RejectionAndLifetime() {
    SourceCaptureAdmission admission;
    if (admission.PreserveForPresent(1, true, true, true)) return false;
    for (unsigned rejected = 0; rejected < 3; ++rejected) {
        admission.Queued(2);
        if (admission.PreserveForPresent(2, rejected != 0, rejected != 1, rejected != 2) ||
            admission.queued_sequence() != 0) return false;
    }
    admission.Queued(3);
    admission.Invalidate(); // Capture/scope/reset/owner fault.
    if (admission.PreserveForPresent(3, true, true, true)) return false;
    admission.Queued(4);
    admission.BeginFrame(); // Includes early blocked-owner or tracking rejection.
    if (admission.PreserveForPresent(4, true, true, true)) return false;
    admission.Queued(5);
    if (admission.PreserveForPresent(4, true, true, true) || admission.queued_sequence() != 5) return false;
    return admission.PreserveForPresent(5, true, true, true) &&
        !admission.PreserveForPresent(5, true, true, true);
}

bool OwnerAndFenceRejection() {
    NativeStereoQueuePolicy queue;
    SourceCaptureAdmission admission;
    const auto owner = queue.ObserveOwner(11);
    if (!queue.BindCapture(owner, 1) || !queue.Queued(owner, 1, 100)) return false;
    admission.Queued(1);
    const auto replacement = queue.ObserveOwner(12);
    if (admission.PreserveForPresent(1, true, queue.CaptureEpoch(1) == owner, true)) return false;
    admission.BeginFrame();
    // An old callback must not register a new pair or activate the exemption.
    if (queue.Queued(owner, 2, 101)) admission.Queued(2);
    return replacement != owner && !admission.PreserveForPresent(2, true, true, true);
}

int main() {
    if (!Bootstrap() || !RejectionAndLifetime() || !OwnerAndFenceRejection()) {
        std::cerr << "source capture admission regression\n";
        return 1;
    }
    std::cout << "source capture admission passed\n";
}
