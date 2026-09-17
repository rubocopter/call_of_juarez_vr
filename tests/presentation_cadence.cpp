#include "backends/openvr/presentation_cadence.hpp"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using cojvr::backends::openvr::PresentationCadence;
    using cojvr::backends::openvr::PresentationContent;

    PresentationCadence cadence;
    if (cadence.presentable() || cadence.pending_content() != PresentationContent::none) {
        return Fail("cadence exposed content before the first producer frame");
    }

    cadence.FrameUploaded();
    if (!cadence.presentable() ||
        cadence.pending_content() != PresentationContent::new_frame) {
        return Fail("first uploaded frame was not classified as new");
    }

    // A failed compositor submit must not consume the new-frame classification.
    if (cadence.pending_content() != PresentationContent::new_frame) {
        return Fail("new frame was consumed without a successful submit");
    }
    cadence.SubmissionSucceeded();

    // Controlled producer pause: no upload occurs here. The presenter must stay
    // alive on the last valid frame and explicitly classify subsequent submits
    // as repetitions instead of inventing new game content.
    for (int repeat = 0; repeat < 8; ++repeat) {
        if (!cadence.presentable() ||
            cadence.pending_content() != PresentationContent::repeated_frame) {
            return Fail("paused producer did not preserve repeatable presentation content");
        }
        cadence.SubmissionSucceeded();
    }

    cadence.FrameUploaded();
    if (cadence.pending_content() != PresentationContent::new_frame) {
        return Fail("resumed producer frame was not classified as new");
    }
    cadence.SubmissionSucceeded();

    cadence.Invalidate();
    if (cadence.presentable() || cadence.pending_content() != PresentationContent::none) {
        return Fail("invalidated presentation state retained stale content");
    }

    std::cout << "Presenter cadence pause/repeat policy passed\n";
    return 0;
}
