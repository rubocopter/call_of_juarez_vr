#pragma once

#include "runtime/vr_types.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace cojvr::runtime {

// Value-only, bounded UTF-16. Native references never leave the game owner.
struct HudText {
    static constexpr std::size_t capacity = 1024;
    std::array<char16_t, capacity> characters{};
    std::uint32_t length = 0;
    [[nodiscard]] std::u16string_view view() const noexcept {
        return length < capacity ? std::u16string_view(characters.data(), length)
                                 : std::u16string_view{};
    }
    bool operator==(const HudText&) const = default;
};

struct HudTextSnapshot {
    HudText hint{};
    HudText interaction{};
    HudText subtitle{};
};

struct StereoHudTextOverlay {
    std::uint64_t frame_sequence = 0;
    std::array<EyeView, 2> eyes{};
    HudTextSnapshot text{};
};

} // namespace cojvr::runtime
