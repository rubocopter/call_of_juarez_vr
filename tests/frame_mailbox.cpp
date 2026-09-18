#include "backends/d3d9/frame_mailbox.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {

cojvr::backends::d3d9::StereoCpuFrame MakeFrame(const std::uint64_t sequence) {
    cojvr::backends::d3d9::StereoCpuFrame frame{};
    frame.device_id = 0x1234;
    frame.generation = 7;
    frame.capture_sequence = sequence;
    frame.render_pose_sequence = sequence;
    frame.render_hmd_pose.orientation = {0.0F, 0.0F, 0.0F, 1.0F};
    frame.render_hmd_pose.orientation_valid = true;
    frame.render_hmd_pose.position = {0.0F, 0.0F, 0.0F};
    frame.render_hmd_pose.position_valid = true;
    frame.capture_time = std::chrono::steady_clock::now();
    for (auto& eye : frame.eyes) {
        eye.width = 2;
        eye.height = 1;
        eye.stride = 8;
        eye.pixels.resize(8, static_cast<std::uint8_t>(sequence));
    }
    return frame;
}

int Fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using cojvr::backends::d3d9::FrameMailbox;
    using cojvr::backends::d3d9::StereoCpuFrame;

    FrameMailbox mailbox;
    if (!mailbox.Publish(MakeFrame(1)) || !mailbox.Publish(MakeFrame(2))) {
        return Fail("mailbox rejected valid increasing frames");
    }
    StereoCpuFrame consumed{};
    if (!mailbox.WaitConsumeLatest(consumed, 0) || consumed.capture_sequence != 2) {
        return Fail("mailbox did not replace stale pending content with the newest frame");
    }
    if (mailbox.Publish(MakeFrame(2)) || mailbox.Publish(MakeFrame(1))) {
        return Fail("mailbox accepted a stale capture sequence");
    }

    std::thread producer([&mailbox] {
        for (std::uint64_t sequence = 3; sequence <= 100; ++sequence) {
            (void)mailbox.Publish(MakeFrame(sequence));
        }
    });
    producer.join();
    if (!mailbox.WaitConsumeLatest(consumed, 0) || consumed.capture_sequence != 100) {
        return Fail("slow-consumer path did not remain bounded at the latest frame");
    }

    const auto stats = mailbox.stats();
    if (stats.published != 100 || stats.consumed != 2 ||
        stats.replaced_pending == 0 || stats.rejected_stale != 2) {
        return Fail("mailbox accounting does not distinguish replace/consume/stale events");
    }

    mailbox.Stop();
    if (mailbox.Publish(MakeFrame(101))) {
        return Fail("stopped mailbox accepted producer content");
    }
    if (mailbox.WaitConsumeLatest(consumed, 1)) {
        return Fail("stopped empty mailbox produced a frame");
    }

    mailbox.Reset();
    if (!mailbox.Publish(MakeFrame(1)) || !mailbox.WaitConsumeLatest(consumed, 1)) {
        return Fail("mailbox reset did not restore producer/consumer operation");
    }

    std::cout << "Bounded latest-frame mailbox tests passed\n";
    return 0;
}
