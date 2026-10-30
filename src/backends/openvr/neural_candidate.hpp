#pragma once
#include "backends/openvr/neural_session.hpp"
#include "runtime/vr_types.hpp"

namespace cojvr::backends::openvr::neural {
struct Options {
    bool enabled=false;
    std::filesystem::path helper,runtime;
    unsigned stage_height=384,timeout_ms=8;
};
// Polled independently of fresh image delivery. A brief loss/reacquisition must
// reset temporal history even when no capture was uploaded during the gap.
class ContextFence final {
    bool available_=false;
public:
    bool Observe(bool available) noexcept {const bool changed=available_!=available;available_=available;return changed;}
};
// Explicit startup-only developer opt-in; absent or malformed means disabled.
Options LoadOptions(const std::filesystem::path& file) noexcept;
bool BuildViews(const runtime::Pose& head,const std::array<runtime::EyeView,2>& eyes,
                std::array<View,2>& output) noexcept;
}
