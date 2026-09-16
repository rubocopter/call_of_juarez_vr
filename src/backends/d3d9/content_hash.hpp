#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cojvr::backends::d3d9 {

// Fast diagnostic hash for B8G8R8X8/A8 surfaces. The alpha/X byte is excluded
// because classic D3D9 may leave it undefined. This hash is only used for
// content-change/equality checks; its numeric value is not a stable file format.
[[nodiscard]] inline std::uint64_t HashBgrxSurfaceIgnoringAlpha(
    const void* pixels,
    const std::size_t pitch,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    if (!pixels || width == 0 || height == 0) return 0;
    const std::size_t active_row_bytes = static_cast<std::size_t>(width) * 4U;
    if (pitch < active_row_bytes) return 0;

    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    constexpr std::uint64_t kTwoPixelRgbMask = 0x00FFFFFF00FFFFFFULL;
    constexpr std::uint32_t kOnePixelRgbMask = 0x00FFFFFFU;

    std::uint64_t hash = kOffset;
    const auto* row = static_cast<const std::uint8_t*>(pixels);
    for (std::uint32_t y = 0; y < height; ++y) {
        std::uint32_t x = 0;
        for (; x + 1U < width; x += 2U) {
            std::uint64_t pair = 0;
            std::memcpy(&pair, row + static_cast<std::size_t>(x) * 4U, sizeof(pair));
            hash ^= pair & kTwoPixelRgbMask;
            hash *= kPrime;
        }
        if (x < width) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, row + static_cast<std::size_t>(x) * 4U, sizeof(pixel));
            hash ^= pixel & kOnePixelRgbMask;
            hash *= kPrime;
        }
        row += pitch;
    }
    return hash;
}

// Equality check used on the hot presentation path. Unlike the diagnostic hash,
// this can return at the first RGB difference while still ignoring the undefined
// alpha/X byte. Valid stereo frames normally diverge very early, so this avoids
// scanning both complete eye images merely to prove that they are not identical.
[[nodiscard]] inline bool BgrxSurfacesEqualIgnoringAlpha(
    const void* left_pixels,
    const std::size_t left_pitch,
    const void* right_pixels,
    const std::size_t right_pitch,
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    if (!left_pixels || !right_pixels || width == 0 || height == 0) return false;
    const std::size_t active_row_bytes = static_cast<std::size_t>(width) * 4U;
    if (left_pitch < active_row_bytes || right_pitch < active_row_bytes) return false;

    constexpr std::uint32_t kRgbMask = 0x00FFFFFFU;
    const auto* left_row = static_cast<const std::uint8_t*>(left_pixels);
    const auto* right_row = static_cast<const std::uint8_t*>(right_pixels);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            std::uint32_t left_pixel = 0;
            std::uint32_t right_pixel = 0;
            const std::size_t offset = static_cast<std::size_t>(x) * 4U;
            std::memcpy(&left_pixel, left_row + offset, sizeof(left_pixel));
            std::memcpy(&right_pixel, right_row + offset, sizeof(right_pixel));
            if ((left_pixel & kRgbMask) != (right_pixel & kRgbMask)) return false;
        }
        left_row += left_pitch;
        right_row += right_pitch;
    }
    return true;
}

} // namespace cojvr::backends::d3d9
