#pragma once
#include "runtime/vr_types.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace cojvr::runtime {
struct UiLabel {
    static constexpr std::size_t capacity = 64;
    std::array<char16_t, capacity> characters{};
    std::uint32_t length = 0;
    [[nodiscard]] std::u16string_view view() const noexcept {
        return length < capacity ? std::u16string_view(characters.data(), length) : std::u16string_view{};
    }
    bool operator==(const UiLabel&) const = default;
};
struct EquipmentWheelSnapshot {
    std::array<UiLabel, 8> labels{};
    std::uint8_t owned_mask = 0;
    std::uint8_t available_mask = 0;
    int selected = -1;
    bool active = false;
    bool valid = false;
    bool operator==(const EquipmentWheelSnapshot&) const = default;
};
struct CompassMarker {
    Vec2 direction{}; UiLabel label{};
    bool operator==(const CompassMarker& other) const noexcept {
        return direction.x == other.direction.x && direction.y == other.direction.y && label == other.label;
    }
};
struct WristCompassSnapshot {
    bool active = false;
    Vec2 north_direction{};
    std::array<CompassMarker, 16> markers{};
    std::uint32_t marker_count = 0;
    bool operator==(const WristCompassSnapshot& other) const noexcept {
        return active == other.active && north_direction.x == other.north_direction.x &&
            north_direction.y == other.north_direction.y && markers == other.markers && marker_count == other.marker_count;
    }
};
struct WristStatusSnapshot {
    static constexpr std::uint32_t pixel_width=512, row_height=48, padding=32;
    bool active=false;
    std::array<UiLabel,10> lines{};
    std::uint32_t line_count=0;
    [[nodiscard]] std::uint32_t pixel_height() const noexcept { return line_count<=lines.size()?padding+row_height*line_count:0; }
    bool operator==(const WristStatusSnapshot&) const = default;
};
struct GameplayUiSnapshot { EquipmentWheelSnapshot wheel{}; WristCompassSnapshot compass{}; WristStatusSnapshot status{}; };
} // namespace cojvr::runtime
