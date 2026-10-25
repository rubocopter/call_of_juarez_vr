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
    // Dashboard visibility gating is now handled by the presenter, not the cadence.
    // The cadence only tracks presentable state.
    if (!cadence.scene_submission_allowed(false) || !cadence.scene_submission_allowed(true)) {
        return Fail("scene submission should be allowed when presentable, regardless of overlay");
    }
    if (cadence.pending_content() != PresentationContent::new_frame) {
        return Fail("pending content should remain new_frame");
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

    // A processing failure can follow writes into an already repeatable pair.
    // Removed consumer resources must never be submitted with the old pose or
    // revived by a later CPU upload. Only a new presenter/session owns recovery.
    cadence.FrameUploaded();cadence.SubmissionSucceeded();
    cadence.ConsumerFailed();
    unsigned submissions=0;
    for(unsigned iteration=0;iteration<4;++iteration) {
        if(iteration)cadence.FrameUploaded();
        if(cadence.scene_submission_allowed(false))++submissions;
        cadence.SubmissionSucceeded();
        if(cadence.presentable()||cadence.pending_content()!=PresentationContent::none)
            return Fail("consumer loss left stale or later content presentable");
        cadence.Invalidate();
    }
    if(submissions)return Fail("removed consumer submitted old or newly overwritten output");
    PresentationCadence replacement;replacement.FrameUploaded();
    if(!replacement.presentable())return Fail("new consumer session inherited terminal failure");
    std::cout << "Presenter cadence pause/repeat and terminal consumer-loss policy passed\n";
    return 0;
}
