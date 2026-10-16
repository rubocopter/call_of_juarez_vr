#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cojvr::games::call_of_juarez {

struct CoJPostprocessMemoryReader {
    using Read = bool (*)(void* context, std::uint32_t address,
        void* destination, std::size_t bytes) noexcept;
    Read read = nullptr;
    void* context = nullptr;
};

enum class CoJPostprocessObservationStatus : std::uint8_t {
    observed, unknown_build, invalid_reader, invalid_address, read_failed,
    renderer_mismatch, invalid_count, capacity_exceeded, invalid_name,
    invalid_topology, invalid_dimensions, changed_roots, dirty, unallocated,
    object_mismatch
};

struct CoJPostprocessTextureObservation {
    std::uint32_t handle = 0;
    std::uint32_t selected_interface = 0;
    std::uint32_t texture_2d = 0, texture_cube = 0, texture_volume = 0;
    std::uint32_t usage = 0, format = 0, kind = 0, flags = 0;
    std::uint32_t requested_width = 0, requested_height = 0, requested_depth = 0;
    std::uint32_t width = 0, height = 0, depth = 0; // Native physical fields, not COM GetDesc.
    float inverse_width = 0, inverse_height = 0, inverse_depth = 0;
};

struct CoJPostprocessBufferObservation {
    std::uint32_t container = 0, entry = 0, index = 0;
    bool global = false;
    std::array<char, 64> name{};
    std::int32_t logical_width = 0, logical_height = 0;
    std::int32_t rgb_bits = 0, alpha_bits = 0;
    std::int32_t width_divisor = 0, height_divisor = 0;
    std::uint8_t internal_color = 0, depth_attachment = 0;
    std::uint8_t native_bpp = 0, temp_shareable = 0, linked_borrowed = 0;
    CoJPostprocessTextureObservation texture{};
};

struct CoJPostprocessEffectObservation {
    std::uint32_t node = 0, next = 0, previous = 0, effect = 0;
    std::uint32_t vtable = 0;
    std::int32_t buffer_count = 0;
    std::array<char, 260> name{};
};

struct CoJPostprocessRendererObservation {
    std::uint32_t logical_width = 0, logical_height = 0;
    std::uint32_t viewport_clamp_width = 0, viewport_clamp_height = 0;
    std::uint32_t current_target = 0;
    std::uint32_t default_color_surface = 0, default_depth_surface = 0;
    std::uint32_t internal_color_surface = 0, internal_depth_surface = 0;
    std::uint32_t additional_depth_surface = 0, placeholder_texture = 0;
    // Distinct native-pointer ranges; the intervening sampler-state words are
    // not textures. These values are never followed by the decoder.
    std::array<std::uint32_t, 16> pixel_samplers{};
    std::array<std::uint32_t, 4> vertex_samplers{};
};

// Copied identities only: no retained references, lifetime, atomicity, allocation,
// resize, gameplay-owner or generation authority. The caller admits the exact
// ChromeEngine3 SHA-256 and supplies a bounded, fault-safe read-only reader.
struct CoJPostprocessResourceSnapshot {
    static constexpr std::size_t max_effects = 16;
    static constexpr std::size_t max_buffers = 16 + max_effects * 16;
    bool valid = false; // Structural observation only; status must be observed.
    CoJPostprocessObservationStatus status = CoJPostprocessObservationStatus::unknown_build;
    std::uint32_t engine_base = 0, renderer = 0, manager = 0, device = 0;
    std::uint32_t renderer_vtable = 0, manager_vtable = 0, list_sentinel = 0;
    std::uint32_t loaded_effect_count = 0;
    std::int32_t global_buffer_count = 0;
    std::uint8_t manager_dirty = 0;
    std::size_t effect_count = 0, buffer_count = 0;
    CoJPostprocessRendererObservation renderer_state{};
    std::array<CoJPostprocessEffectObservation, max_effects> effects{};
    std::array<CoJPostprocessBufferObservation, max_buffers> buffers{};
};

[[nodiscard]] CoJPostprocessResourceSnapshot ObserveCoJPostprocessResources(
    CoJPostprocessMemoryReader reader, std::uint32_t engineBase32,
    bool exactBuildValidated) noexcept;

} // namespace cojvr::games::call_of_juarez
