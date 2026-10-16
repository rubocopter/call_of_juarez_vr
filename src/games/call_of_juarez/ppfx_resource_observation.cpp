#include "games/call_of_juarez/ppfx_resource_observation.hpp"

#include <cmath>
#include <cstring>

namespace cojvr::games::call_of_juarez {
namespace {
using Status = CoJPostprocessObservationStatus;
constexpr std::uint32_t invalid_pointer = 0xffffffffu;
bool Identity(std::uint32_t value) noexcept { return value != 0 && value != invalid_pointer; }
bool OptionalIdentity(std::uint32_t value) noexcept { return value != invalid_pointer; }
std::uint32_t U32(const unsigned char* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
std::int32_t I32(const unsigned char* p) noexcept {
    const auto value = U32(p);
    std::int32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
float F32(const unsigned char* p) noexcept {
    const auto value = U32(p);
    float result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
template<std::size_t N> bool Name(const std::array<char, N>& value) noexcept {
    return value[0] != '\0' && std::memchr(value.data(), 0, N) != nullptr;
}

struct Roots {
    std::uint32_t renderer = 0, manager = 0, device = 0, vtable = 0, manager_vtable = 0;
    std::uint32_t sentinel = 0, first = 0, last = 0;
    std::uint32_t list_size = 0;
    std::int32_t count = 0;
    std::uint8_t dirty = 0;
};

class Decoder {
public:
    Decoder(CoJPostprocessMemoryReader reader, CoJPostprocessResourceSnapshot& snapshot) noexcept
        : reader_(reader), snapshot_(snapshot) {}

    bool Fail(Status status) noexcept { snapshot_.status = status; return false; }

    bool Address(std::uint32_t base, std::uint32_t offset, std::uint32_t& address) noexcept {
        if (!Identity(base) || offset > invalid_pointer - base) return Fail(Status::invalid_address);
        address = base + offset;
        return Identity(address) || Fail(Status::invalid_address);
    }

    bool Bytes(std::uint32_t base, std::uint32_t offset, void* destination, std::size_t size) noexcept {
        std::uint32_t address;
        if (!Address(base, offset, address)) return false;
        if (size == 0 || size - 1 > invalid_pointer - address) return Fail(Status::invalid_address);
        return reader_.read(reader_.context, address, destination, size) || Fail(Status::read_failed);
    }

    bool Word(std::uint32_t base, std::uint32_t offset, std::uint32_t& value) noexcept {
        unsigned char data[4];
        if (!Bytes(base, offset, data, sizeof(data))) return false;
        value = U32(data); return true;
    }
    bool Count(std::uint32_t container, std::int32_t& count) noexcept {
        unsigned char data[4];
        if (!Bytes(container, 0x644, data, sizeof(data))) return false;
        count = I32(data); return true;
    }

    // The second pass compares parent identities before following them. These
    // checks detect some changes; they cannot exclude concurrent mutation/ABA.
    bool ReadRoots(Roots& roots, const Roots* expected = nullptr) noexcept {
        if (!Word(snapshot_.engine_base, 0x590074, roots.renderer) ||
            !Word(snapshot_.engine_base, 0x590078, roots.device) ||
            !Word(snapshot_.engine_base, 0x5902ac, roots.manager)) return false;
        if (expected && (roots.renderer != expected->renderer || roots.device != expected->device ||
            roots.manager != expected->manager)) return Fail(Status::changed_roots);
        if (!Identity(roots.renderer) || !Identity(roots.manager) || !Identity(roots.device))
            return Fail(Status::invalid_address);
        if (!Word(roots.renderer, 0, roots.vtable) || !Word(roots.manager, 0, roots.manager_vtable) ||
            !Count(roots.manager, roots.count) || !Word(roots.manager, 0x650, roots.list_size) ||
            !Word(roots.manager, 0x64c, roots.sentinel) ||
            !Bytes(roots.manager, 0x6b5, &roots.dirty, sizeof(roots.dirty))) return false;
        if (expected && (roots.vtable != expected->vtable || roots.count != expected->count ||
            roots.manager_vtable != expected->manager_vtable || roots.list_size != expected->list_size ||
            roots.sentinel != expected->sentinel || roots.dirty != expected->dirty))
            return Fail(Status::changed_roots);
        if (!Word(roots.sentinel, 0, roots.first) || !Word(roots.sentinel, 4, roots.last)) return false;
        if (expected && (roots.first != expected->first || roots.last != expected->last))
            return Fail(Status::changed_roots);
        return true;
    }

    bool Renderer(std::uint32_t renderer) noexcept {
        auto& r = snapshot_.renderer_state;
        if (!Word(renderer, 0x1c8, r.logical_width) || !Word(renderer, 0x1cc, r.logical_height) ||
            !Word(renderer, 0x15c4, r.viewport_clamp_width) || !Word(renderer, 0x15c8, r.viewport_clamp_height) ||
            !Word(renderer, 0x1db4, r.current_target) ||
            !Word(renderer, 0x1dc4, r.default_color_surface) || !Word(renderer, 0x1dc8, r.default_depth_surface) ||
            !Word(renderer, 0x1c18, r.internal_color_surface) || !Word(renderer, 0x1c1c, r.internal_depth_surface) ||
            !Word(renderer, 0x1c2c, r.additional_depth_surface) || !Word(renderer, 0x1c3c, r.placeholder_texture)) return false;
        if (!r.logical_width || !r.logical_height || !r.viewport_clamp_width || !r.viewport_clamp_height)
            return Fail(Status::invalid_dimensions);
        for (auto value : {r.current_target, r.default_color_surface, r.default_depth_surface,
                r.internal_color_surface, r.internal_depth_surface, r.additional_depth_surface, r.placeholder_texture})
            if (!OptionalIdentity(value)) return Fail(Status::invalid_address);
        unsigned char pixel[16 * 4], vertex[4 * 4];
        if (!Bytes(renderer, 0x1cd0, pixel, sizeof(pixel)) ||
            !Bytes(renderer, 0x1d50, vertex, sizeof(vertex))) return false;
        for (std::size_t i = 0; i < r.pixel_samplers.size(); ++i) {
            r.pixel_samplers[i] = U32(pixel + i * 4);
            if (!OptionalIdentity(r.pixel_samplers[i])) return Fail(Status::invalid_address);
        }
        for (std::size_t i = 0; i < r.vertex_samplers.size(); ++i) {
            r.vertex_samplers[i] = U32(vertex + i * 4);
            if (!OptionalIdentity(r.vertex_samplers[i])) return Fail(Status::invalid_address);
        }
        return true;
    }

    bool Texture(CoJPostprocessTextureObservation& texture) noexcept {
        if (!texture.handle) { unallocated_ = true; return true; }
        std::uint32_t last;
        if (!Address(texture.handle, 0x15f, last)) return false;
        unsigned char interfaces[0x18], dimensions[24], inverse[12];
        if (!Bytes(texture.handle, 0, interfaces, sizeof(interfaces)) ||
            !Word(texture.handle, 0x130, texture.kind) ||
            !Word(texture.handle, 0x134, texture.flags) ||
            !Bytes(texture.handle, 0x13c, dimensions, sizeof(dimensions)) ||
            !Bytes(texture.handle, 0x154, inverse, sizeof(inverse))) return false;
        texture.selected_interface = U32(interfaces);
        texture.texture_2d = U32(interfaces + 4); texture.texture_cube = U32(interfaces + 8);
        texture.texture_volume = U32(interfaces + 12);
        texture.usage = U32(interfaces + 16); texture.format = U32(interfaces + 20);
        texture.requested_width = U32(dimensions); texture.requested_height = U32(dimensions + 4);
        texture.requested_depth = U32(dimensions + 8);
        texture.width = U32(dimensions + 12); texture.height = U32(dimensions + 16); texture.depth = U32(dimensions + 20);
        texture.inverse_width = F32(inverse); texture.inverse_height = F32(inverse + 4);
        texture.inverse_depth = F32(inverse + 8);
        if (!OptionalIdentity(texture.selected_interface) ||
            !OptionalIdentity(texture.texture_2d) || !OptionalIdentity(texture.texture_cube) ||
            !OptionalIdentity(texture.texture_volume)) return Fail(Status::invalid_address);
        if (texture.kind > 2) return Fail(Status::object_mismatch);
        if (!texture.selected_interface || (!texture.texture_2d && !texture.texture_cube && !texture.texture_volume)) {
            unallocated_ = true; return true;
        }
        const auto typed_interface = texture.kind == 0 ? texture.texture_2d :
            texture.kind == 1 ? texture.texture_volume : texture.texture_cube;
        if (texture.selected_interface != typed_interface) return Fail(Status::object_mismatch);
        if (!texture.requested_width || !texture.requested_height ||
            !texture.width || !texture.height || !std::isfinite(texture.inverse_width) ||
            !std::isfinite(texture.inverse_height) || texture.inverse_width <= 0 || texture.inverse_height <= 0)
            return Fail(Status::invalid_dimensions);
        // The native 2D constructor admits depth zero and computes 1/depth;
        // preserve that unused reciprocal (including infinity) as raw data.
        if (texture.kind == 1 && (!texture.requested_depth || !texture.depth ||
            !std::isfinite(texture.inverse_depth) || texture.inverse_depth <= 0))
            return Fail(Status::invalid_dimensions);
        return true;
    }

    bool Buffers(std::uint32_t container, std::int32_t count, bool global) noexcept {
        if (count < 0 || count > 16) return Fail(Status::invalid_count);
        for (std::int32_t i = 0; i < count; ++i) {
            if (snapshot_.buffer_count == snapshot_.max_buffers) return Fail(Status::capacity_exceeded);
            auto& buffer = snapshot_.buffers[snapshot_.buffer_count];
            buffer.container = container; buffer.index = static_cast<std::uint32_t>(i); buffer.global = global;
            if (!Address(container, 4 + buffer.index * 0x64, buffer.entry)) return false;
            unsigned char data[0x64];
            if (!Bytes(buffer.entry, 0, data, sizeof(data))) return false;
            buffer.texture.handle = U32(data); std::memcpy(buffer.name.data(), data + 4, buffer.name.size());
            buffer.logical_width = I32(data + 0x44); buffer.logical_height = I32(data + 0x48);
            buffer.rgb_bits = I32(data + 0x4c); buffer.alpha_bits = I32(data + 0x50);
            buffer.internal_color = data[0x54]; buffer.depth_attachment = data[0x55];
            buffer.width_divisor = I32(data + 0x58); buffer.height_divisor = I32(data + 0x5c);
            buffer.native_bpp = data[0x60]; buffer.temp_shareable = data[0x61]; buffer.linked_borrowed = data[0x62];
            if (!Name(buffer.name)) return Fail(Status::invalid_name);
            if (buffer.logical_width <= 0 || buffer.logical_height <= 0) return Fail(Status::invalid_dimensions);
            if (!Texture(buffer.texture)) return false;
            ++snapshot_.buffer_count;
        }
        return true;
    }

    bool Effects(const Roots& roots) noexcept {
        if (!Identity(roots.first) || !Identity(roots.last)) return Fail(Status::invalid_topology);
        std::uint32_t node = roots.first, previous = roots.sentinel;
        while (node != roots.sentinel) {
            for (std::size_t i = 0; i < snapshot_.effect_count; ++i)
                if (snapshot_.effects[i].node == node) return Fail(Status::invalid_topology);
            if (snapshot_.effect_count >= roots.list_size) return Fail(Status::invalid_topology);
            if (snapshot_.effect_count == snapshot_.max_effects) return Fail(Status::capacity_exceeded);
            auto& effect = snapshot_.effects[snapshot_.effect_count]; effect.node = node;
            unsigned char data[12];
            if (!Bytes(node, 0, data, sizeof(data))) return false;
            effect.next = U32(data); effect.previous = U32(data + 4); effect.effect = U32(data + 8);
            if (effect.previous != previous || !Identity(effect.next) || !Identity(effect.effect) ||
                effect.effect == roots.manager) return Fail(Status::invalid_topology);
            for (std::size_t i = 0; i < snapshot_.effect_count; ++i)
                if (snapshot_.effects[i].effect == effect.effect) return Fail(Status::invalid_topology);
            if (!Word(effect.effect, 0, effect.vtable)) return false;
            if (effect.vtable != snapshot_.engine_base + 0x3180ac) return Fail(Status::object_mismatch);
            if (!Count(effect.effect, effect.buffer_count) ||
                !Bytes(effect.effect, 0x684, effect.name.data(), effect.name.size())) return false;
            if (!Name(effect.name)) return Fail(Status::invalid_name);
            if (!Buffers(effect.effect, effect.buffer_count, false)) return false;
            ++snapshot_.effect_count; previous = node; node = effect.next;
        }
        return (previous == roots.last && snapshot_.effect_count == roots.list_size) || Fail(Status::invalid_topology);
    }

    bool unallocated_ = false;
private:
    CoJPostprocessMemoryReader reader_;
    CoJPostprocessResourceSnapshot& snapshot_;
};
} // namespace

CoJPostprocessResourceSnapshot ObserveCoJPostprocessResources(
    CoJPostprocessMemoryReader reader, std::uint32_t engineBase32, bool exactBuildValidated) noexcept {
    CoJPostprocessResourceSnapshot result;
    result.engine_base = engineBase32;
    if (!exactBuildValidated) return result;
    if (!reader.read) { result.status = Status::invalid_reader; return result; }
    Decoder decoder(reader, result);
    std::uint32_t expected_vtable, last_global;
    // Preflight every image-relative read before consulting the reader.
    if (!decoder.Address(engineBase32, 0x5902af, last_global) ||
        !decoder.Address(engineBase32, 0x312334, expected_vtable)) return result;
    Roots roots;
    if (!decoder.ReadRoots(roots)) return result;
    result.renderer = roots.renderer; result.manager = roots.manager; result.device = roots.device;
    result.renderer_vtable = roots.vtable; result.list_sentinel = roots.sentinel;
    result.manager_vtable = roots.manager_vtable; result.loaded_effect_count = roots.list_size;
    result.global_buffer_count = roots.count; result.manager_dirty = roots.dirty;
    if (roots.vtable != expected_vtable) { decoder.Fail(Status::renderer_mismatch); return result; }
    if (roots.manager_vtable != engineBase32 + 0x314c48) { decoder.Fail(Status::object_mismatch); return result; }
    if (!decoder.Renderer(roots.renderer) || !decoder.Buffers(roots.manager, roots.count, true) ||
        !decoder.Effects(roots)) return result;
    Roots after;
    if (!decoder.ReadRoots(after, &roots)) return result;
    result.status = roots.dirty ? Status::dirty : decoder.unallocated_ ? Status::unallocated : Status::observed;
    result.valid = result.status == Status::observed;
    return result;
}
} // namespace cojvr::games::call_of_juarez
