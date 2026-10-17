#include "backends/openvr/fixed_output_policy.hpp"
#include "backends/openvr/fixed_output_frame_mailbox.hpp"

#include <iostream>
#include <atomic>
#include <thread>

using namespace cojvr::backends::openvr;
namespace d3d9 = cojvr::backends::d3d9;

d3d9::StereoCpuFrame Frame(std::uint64_t sequence) {
    d3d9::StereoCpuFrame frame{};
    frame.capture_sequence = sequence;
    frame.render_pose_sequence = sequence;
    frame.render_hmd_pose.orientation_valid = frame.render_hmd_pose.position_valid = true;
    return frame;
}

bool OperationInterleavings() {
    // These are the same guarded boundaries used for output commands, upload
    // commit and each compositor call. External calls cannot be retracted.
    FixedOutputFrameAuthority authority;
    const auto captured = authority.Epoch();
    unsigned writes = 0, commits = 0, submissions = 0;
    if (!authority.Execute(captured, [&] { ++writes; return true; }) || writes != 1) return false;
    authority.Invalidate();
    if (authority.Execute(captured, [&] { ++writes; return true; }) || writes != 1 ||
        authority.Execute(captured, [&] { ++commits; return true; }) || commits != 0 ||
        authority.Execute(captured, [&] { ++submissions; return true; }) || submissions != 0) return false;

    const auto interrupted = authority.Epoch();
    if (authority.Execute(interrupted, [&] { ++writes; authority.Invalidate(); return true; }) ||
        authority.Execute(interrupted, [&] { ++writes; return true; }) || writes != 2 ||
        authority.Execute(interrupted, [&] { ++commits; return true; }) || commits != 0) return false;

    const auto fresh = authority.Epoch();
    if (!authority.Execute(fresh, [&] { ++writes; return true; }) ||
        !authority.Execute(fresh, [&] { ++commits; return true; }) || commits != 1) return false;
    // A failed first operation never becomes a successful pair.
    if (authority.Execute(fresh, [] { return false; })) return false;
    // Left Submit succeeded at the API, then ownership was lost. The left call
    // is observable; the pair and right eye must not be confirmed or repeated.
    if (authority.Execute(fresh, [&] { ++submissions; authority.Invalidate(); return true; }) ||
        authority.Execute(fresh, [&] { ++submissions; return true; }) || submissions != 1 ||
        authority.Execute(fresh, [&] { ++submissions; return true; })) return false;
    const auto recovered = authority.Epoch();
    if (!authority.Execute(recovered, [&] { ++submissions; return true; }) ||
        !authority.Execute(recovered, [&] { ++submissions; return true; }) || submissions != 3) return false;
    authority.Invalidate(); // Reject a repeat even when storage remains ready.
    return !authority.Execute(recovered, [&] { ++submissions; return true; }) && submissions == 3;
}

bool ConcurrentMailbox() {
    d3d9::FrameMailbox mailbox;
    FixedOutputFrameMailbox stamped;
    FixedOutputFrameAuthority authority;
    std::atomic_bool done{false}, valid{true};
    std::thread producer([&] {
        for (unsigned i = 0; i < 2000; ++i) {
            const auto epoch = authority.Epoch();
            if (!stamped.Publish(mailbox, authority, Frame(epoch), epoch)) valid = false;
            authority.Invalidate();
        }
        done.store(true, std::memory_order_release);
    });
    std::uint64_t last = 0;
    for (;;) {
        const bool finished = done.load(std::memory_order_acquire);
        d3d9::StereoCpuFrame frame{};
        std::uint64_t epoch = 0;
        if (stamped.Consume(mailbox, frame, epoch)) {
            if (epoch != frame.capture_sequence || frame.capture_sequence <= last) valid = false;
            last = frame.capture_sequence;
        } else if (finished) break;
        else std::this_thread::yield();
    }
    producer.join();
    return valid && last == 2000;
}

int main() {
    if (!OperationInterleavings()) {
        std::cerr << "output operation/commit/eye-submit authority interleaving failed\n"; return 1;
    }
    if (!ConcurrentMailbox()) {
        std::cerr << "concurrent mailbox paired a frame with a different capture epoch\n"; return 1;
    }
    FixedOutputFrameAuthority authority;
    const auto captured = authority.Epoch();
    if (!captured || !authority.Allows(captured) || authority.Allows(0)) return 1;
    // Stamp belongs to capture authorization. Reading the latest epoch on the
    // consumer would wrongly bless this queued/partially written old pair.
    authority.Invalidate();
    if (authority.Allows(captured)) {
        std::cerr << "invalidation admitted an old queued capture/repeat/second eye\n";
        return 1;
    }
    const auto fresh = authority.Epoch();
    if (!authority.Allows(fresh)) return 1;
    authority.Invalidate(); // Reset attempt may fail without changing device generation.
    authority.Invalidate(); // Owner rejection may repeat before another capture.
    if (authority.Allows(fresh) || authority.Allows(captured) || !authority.Allows(authority.Epoch())) return 1;

    d3d9::FrameMailbox mailbox;
    FixedOutputFrameMailbox stamped;
    const auto queued_epoch = authority.Epoch();
    if (!stamped.Publish(mailbox, authority, Frame(1), queued_epoch)) return 1;
    authority.Invalidate();
    d3d9::StereoCpuFrame consumed{};
    std::uint64_t consumed_epoch = 0;
    if (!stamped.Consume(mailbox, consumed, consumed_epoch) || consumed.capture_sequence != 1 ||
        consumed_epoch != queued_epoch || authority.Allows(consumed_epoch)) {
        std::cerr << "old queued capture was stamped with consumer's new epoch\n"; return 1;
    }
    const auto authorized_epoch = authority.Epoch();
    if (!stamped.Publish(mailbox, authority, Frame(2), authorized_epoch) ||
        stamped.Publish(mailbox, authority, Frame(3), queued_epoch) ||
        stamped.Publish(mailbox, authority, Frame(3), 0) ||
        !stamped.Consume(mailbox, consumed, consumed_epoch) || consumed.capture_sequence != 2 ||
        consumed_epoch != authorized_epoch || !authority.Allows(consumed_epoch)) return 1;
    if (!stamped.Publish(mailbox, authority, Frame(4), authorized_epoch)) return 1;
    authority.Invalidate();
    const auto replacement_epoch = authority.Epoch();
    if (!stamped.Publish(mailbox, authority, Frame(5), replacement_epoch) ||
        stamped.Publish(mailbox, authority, Frame(0), authorized_epoch) ||
        !stamped.Consume(mailbox, consumed, consumed_epoch) || consumed.capture_sequence != 5 ||
        consumed_epoch != replacement_epoch || !authority.Allows(consumed_epoch)) return 1;
    // A stopped publisher and a failed mailbox validation cannot alter a stamp.
    if (!stamped.Publish(mailbox, authority, Frame(6), replacement_epoch) ||
        stamped.Publish(mailbox, authority, Frame(0), replacement_epoch) ||
        stamped.Publish(mailbox, authority, Frame(5), replacement_epoch) ||
        !stamped.Consume(mailbox, consumed, consumed_epoch) || consumed.capture_sequence != 6 ||
        consumed_epoch != replacement_epoch) return 1;
    consumed_epoch = replacement_epoch;
    if (stamped.Consume(mailbox, consumed, consumed_epoch) || consumed_epoch != 0) return 1;
    mailbox.Stop();
    if (stamped.Publish(mailbox, authority, Frame(7), replacement_epoch) ||
        stamped.Consume(mailbox, consumed, consumed_epoch)) return 1;
    std::cout << "Fixed-output capture authority rejects stale epochs\n";
    return 0;
}
