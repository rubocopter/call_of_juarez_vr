#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include "games/call_of_juarez/ppfx_resource_telemetry.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace cojvr::games::call_of_juarez;
using Status = CoJPostprocessObservationStatus;
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// Sparse byte-addressed x86 memory, deliberately unrelated to host pointers.
struct Memory {
    static constexpr std::uint32_t engine = 0x90000000, renderer = 0xa0000000;
    static constexpr std::uint32_t manager = 0xb0000000, sentinel = 0xb1000000;
    std::map<std::uint32_t, unsigned char> bytes;
    std::vector<std::uint32_t> addresses;
    std::uint32_t fail_address = 0, change_address = 0;
    std::uint32_t change_value = 0;
    unsigned change_hit = 0;
    std::size_t read_calls = 0, fail_call = 0;

    void Zero(std::uint32_t address, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) bytes[address + static_cast<std::uint32_t>(i)] = 0;
    }
    template<class T> void Put(std::uint32_t address, T value) {
        unsigned char data[sizeof(T)]; std::memcpy(data, &value, sizeof(T));
        for (std::size_t i = 0; i < sizeof(T); ++i) bytes[address + static_cast<std::uint32_t>(i)] = data[i];
    }
    void Text(std::uint32_t address, const char* text, std::size_t capacity) {
        Zero(address, capacity);
        for (std::size_t i = 0; text[i] && i < capacity; ++i) bytes[address + static_cast<std::uint32_t>(i)] = text[i];
    }
    static bool Read(void* context, std::uint32_t address, void* output, std::size_t size) noexcept {
        auto& self = *static_cast<Memory*>(context);
        self.addresses.push_back(address);
        if (++self.read_calls == self.fail_call) return false;
        if (self.change_address >= address && std::uint64_t(self.change_address) < std::uint64_t(address) + size) {
            if (++self.change_hit == 2) self.Put(self.change_address, self.change_value);
        }
        if (self.fail_address >= address && std::uint64_t(self.fail_address) < std::uint64_t(address) + size) return false;
        auto* destination = static_cast<unsigned char*>(output);
        for (std::size_t i = 0; i < size; ++i) {
            auto found = self.bytes.find(address + static_cast<std::uint32_t>(i));
            if (found == self.bytes.end()) return false;
            destination[i] = found->second;
        }
        return true;
    }
    CoJPostprocessResourceSnapshot Observe(bool exact = true, std::uint32_t base = engine) {
        addresses.clear(); addresses.reserve(10000); change_hit = 0; read_calls = 0;
        return ObserveCoJPostprocessResources({Read, this}, base, exact);
    }
    static std::uint32_t Effect(unsigned i) { return 0xc0000000 + i * 0x1000; }
    static std::uint32_t Node(unsigned i) { return 0xb2000000 + i * 0x100; }
    static std::uint32_t Handle(unsigned i) { return 0xd0000000 + i * 0x1000; }
    static std::uint32_t Entry(std::uint32_t container, unsigned i) { return container + 4 + i * 0x64; }
    void Texture(std::uint32_t handle, unsigned width, unsigned height) {
        Zero(handle, 0x18);
        Put(handle, handle + 0x800); // Selected COM alias, not a native vtable.
        Put(handle + 4, handle + 0x800);
        Put(handle + 0x10, 1u); Put(handle + 0x14, 113u);
        Put(handle + 0x130, 0u); Put(handle + 0x134, 0x21du);
        Put(handle + 0x13c, width - (width > 1 ? 1u : 0u));
        Put(handle + 0x140, height - (height > 1 ? 1u : 0u)); Put(handle + 0x144, 1u);
        Put(handle + 0x148, width); Put(handle + 0x14c, height); Put(handle + 0x150, 1u);
        Put(handle + 0x154, 1.0f / width); Put(handle + 0x158, 1.0f / height);
        Put(handle + 0x15c, 1.0f);
    }
    void Buffer(std::uint32_t container, unsigned index, const char* name,
        unsigned width, unsigned height, std::uint32_t handle) {
        auto e = Entry(container, index); Zero(e, 0x64); Text(e + 4, name, 64);
        Put(e, handle); Put(e + 0x44, width); Put(e + 0x48, height);
        Put(e + 0x4c, 48u); Put(e + 0x50, 16u);
        Put(e + 0x54, std::uint8_t(1)); Put(e + 0x55, std::uint8_t(1));
        Put(e + 0x61, std::uint8_t(1));
        if (handle) Texture(handle, width, height);
    }
    void Effects(unsigned count, unsigned buffers) {
        Put(manager + 0x650, count);
        Put(sentinel, count ? Node(0) : sentinel);
        Put(sentinel + 4, count ? Node(count - 1) : sentinel);
        for (unsigned i = 0; i < count; ++i) {
            Put(Node(i), i + 1 == count ? sentinel : Node(i + 1));
            Put(Node(i) + 4, i ? Node(i - 1) : sentinel);
            Put(Node(i) + 8, Effect(i));
            Put(Effect(i), engine + 0x3180ac);
            Put(Effect(i) + 0x644, buffers);
            Text(Effect(i) + 0x684, "data/scene30.ppfx", 260);
            for (unsigned j = 0; j < buffers; ++j)
                Buffer(Effect(i), j, "target", 2560, 1440, Handle(16 + i * 16 + j));
        }
    }
    Memory() {
        Put(engine + 0x590074, renderer); Put(engine + 0x590078, 0xe0000000u);
        Put(engine + 0x5902ac, manager); Put(renderer, engine + 0x312334);
        Put(manager, engine + 0x314c48);
        Put(manager + 0x644, 1u); Put(manager + 0x64c, sentinel);
        Put(manager + 0x6b5, std::uint8_t(0));
        Buffer(manager, 0, "global", 2560, 1440, Handle(0));
        Put(renderer + 0x1c8, 2560u); Put(renderer + 0x1cc, 1440u);
        Put(renderer + 0x15c4, 1920u); Put(renderer + 0x15c8, 1080u);
        for (auto offset : {0x1db4, 0x1dc4, 0x1dc8, 0x1c18, 0x1c1c, 0x1c2c, 0x1c3c})
            Put(renderer + offset, 0xe1000000u + static_cast<unsigned>(offset));
        for (unsigned i = 0; i < 16; ++i) Put(renderer + 0x1cd0 + i * 4, Handle(i));
        for (unsigned i = 0; i < 4; ++i) Put(renderer + 0x1d50 + i * 4, Handle(32 + i));
        // Deliberately leave state-cache bytes and sampler pointees unmapped.
        Effects(1, 14);
        const char* names[] = {"1A_16", "1B_16", "4A_16", "1xScreenY_16", "1x1_16", "1A", "1B", "2A", "4A", "4B", "8A", "16A", "16B", "16Prev"};
        const unsigned divisors[] = {1,1,4,0,0,1,1,2,4,4,8,16,16,16};
        for (unsigned i = 0; i < 14; ++i) {
            unsigned width = divisors[i] ? 2560 / divisors[i] : 1;
            unsigned height = divisors[i] ? 1440 / divisors[i] : i == 3 ? 360 : 1;
            Buffer(Effect(0), i, names[i], width, height, Handle(16 + i));
            Put(Entry(Effect(0), i) + 0x58, divisors[i]);
            Put(Entry(Effect(0), i) + 0x5c, i == 3 ? 4u : divisors[i]);
            if (i >= 5) { Put(Entry(Effect(0), i) + 0x60, std::uint8_t(1)); Put(Handle(16+i) + 0x14, 21u); }
        }
    }
};

void ValidRecipeAndAliases() {
    Memory m; auto s = m.Observe();
    Require(s.valid && s.status == Status::observed, "valid 14-descriptor recipe rejected");
    Require(s.effect_count == 1 && s.buffer_count == 15, "global/local counts lost");
    Require(s.renderer == Memory::renderer && s.device == 0xe0000000 && s.manager == Memory::manager,
        "unsigned high-bit roots lost");
    Require(s.buffers[0].global && !s.buffers[1].global, "global distinction lost");
    Require(std::string(s.effects[0].name.data()) == "data/scene30.ppfx", "effect name lost");
    Require(std::string(s.buffers[14].name.data()) == "16Prev", "descriptor order/name lost");
    Require(s.buffers[4].logical_width == 1 && s.buffers[4].logical_height == 360 &&
        s.buffers[4].height_divisor == 4, "one-column reduction lost");
    Require(s.buffers[1].texture.texture_2d == Memory::Handle(16) + 0x800 &&
        s.buffers[1].texture.flags == 0x21d && s.buffers[1].texture.usage == 1 &&
        s.buffers[1].texture.format == 113 && s.buffers[1].texture.inverse_depth == 1.0f,
        "native texture/COM distinction or values lost");
    Require(s.manager_vtable == Memory::engine + 0x314c48 && s.loaded_effect_count == 1 &&
        s.effects[0].vtable == Memory::engine + 0x3180ac, "native class/list size identities lost");
    Require(s.buffers[1].texture.selected_interface == Memory::Handle(16) + 0x800 &&
        s.buffers[1].texture.kind == 0 && s.buffers[1].texture.requested_width == 2559 &&
        s.buffers[1].texture.requested_height == 1439 && s.buffers[1].texture.width == 2560 &&
        s.buffers[1].texture.height == 1440 && s.buffers[1].texture.requested_depth == 1 &&
        s.buffers[1].texture.depth == 1, "requested/physical/alias texture fields conflated");
    Require(s.renderer_state.logical_width == 2560 && s.renderer_state.viewport_clamp_width == 1920 &&
        s.renderer_state.viewport_clamp_height == 1080 &&
        s.renderer_state.current_target == 0xe1001db4 &&
        s.renderer_state.default_color_surface == 0xe1001dc4 &&
        s.renderer_state.default_depth_surface == 0xe1001dc8 &&
        s.renderer_state.internal_color_surface == 0xe1001c18 &&
        s.renderer_state.internal_depth_surface == 0xe1001c1c &&
        s.renderer_state.additional_depth_surface == 0xe1001c2c &&
        s.renderer_state.placeholder_texture == 0xe1001c3c, "renderer cache identities lost");
    Require(s.renderer_state.pixel_samplers[15] == Memory::Handle(15) &&
        s.renderer_state.vertex_samplers[0] == Memory::Handle(32) &&
        s.renderer_state.vertex_samplers[3] == Memory::Handle(35), "split sampler ranges lost");
    const auto saved = m.bytes;
    s = m.Observe(); Require(m.bytes == saved, "decoder mutated synthetic native memory");
    m.Effects(2, 1);
    auto linked = Memory::Entry(Memory::Effect(1), 0);
    m.Put(linked, Memory::Handle(16)); m.Put(linked + 0x62, std::uint8_t(7));
    m.Put(linked + 0x54, std::uint8_t(9));
    s = m.Observe();
    Require(s.valid && s.buffer_count == 3, "linked alias observation failed");
    Require(s.buffers[1].texture.handle == s.buffers[2].texture.handle &&
        s.buffers[2].linked_borrowed == 7 && s.buffers[2].internal_color == 9,
        "alias identity or raw flag bytes normalized/lost");
    m.Put(Memory::Handle(16) + 4, 0u); m.Put(Memory::Handle(16) + 8, 0xf1234000u);
    m.Put(Memory::Handle(16), 0xf1234000u); m.Put(Memory::Handle(16) + 0x130, 2u);
    m.Put(Memory::Handle(16) + 0x14, 0x12345678u);
    Require(m.Observe().valid, "invented format or 2D-only whitelist");
    m.Put(Memory::Handle(16) + 8, 0u); m.Put(Memory::Handle(16) + 12, 0xf1234000u);
    m.Put(Memory::Handle(16) + 0x130, 1u);
    Require(m.Observe().valid, "volume COM identity rejected");
    // Native 23FFA0 chooses 2D for depth < 2; CreateInterfaces ignores that
    // dimension for 2D. Do not reject its unused infinite reciprocal depth.
    m.Put(Memory::Handle(0) + 0x144, 0u); m.Put(Memory::Handle(0) + 0x150, 0u);
    m.Put(Memory::Handle(0) + 0x15c, std::numeric_limits<float>::infinity());
    Require(m.Observe().valid, "native zero-depth 2D texture rejected");
}

void AdmissionAndOverflow() {
    Memory m;
    Require(m.Observe(false).status == Status::unknown_build && m.addresses.empty(), "unknown build read memory");
    Require(ObserveCoJPostprocessResources({}, Memory::engine, true).status == Status::invalid_reader, "null reader accepted");
    for (auto base : {0u, 0xffffffffu, 0xfff00000u}) {
        Require(m.Observe(true, base).status == Status::invalid_address && m.addresses.empty(), "invalid/overflow image read");
    }
    for (auto root : {0u, 0xffffffffu}) {
        Memory f; f.Put(Memory::engine + 0x590074, root);
        Require(!f.Observe().valid, "invalid renderer root accepted");
    }
    m.Put(Memory::renderer, Memory::engine + 0x312338);
    Require(m.Observe().status == Status::renderer_mismatch, "foreign renderer vtable accepted");
    Memory p; p.Put(Memory::engine + 0x590074, 0xfffffff0u);
    p.Put(0xfffffff0u, Memory::engine + 0x312334);
    Require(p.Observe().status == Status::invalid_address, "renderer+offset overflow accepted");
    Memory t; t.Put(Memory::Entry(Memory::manager, 0), 0xffffff00u);
    t.Zero(0xffffff04u, 0x14);
    Require(t.Observe().status == Status::invalid_address, "texture+offset overflow accepted");
}

void MalformedLayouts() {
    for (auto kind : {3u, 0xffffffffu}) {
        Memory m; m.Put(Memory::Handle(0) + 0x130, kind);
        Require(m.Observe().status == Status::object_mismatch, "unknown texture kind accepted");
    }
    for (auto kind : {0u, 1u, 2u}) {
        Memory m; const auto handle = Memory::Handle(0);
        m.Put(handle + 0x130, kind);
        m.Put(handle + 4, 0xf0001000u); m.Put(handle + 8, 0xf0002000u);
        m.Put(handle + 12, 0xf0003000u); m.Put(handle, 0xf0004000u);
        Require(m.Observe().status == Status::object_mismatch, "selected COM alias/type mismatch accepted");
    }
    for (auto count : {0xffffffffu, 0x80000000u, 17u}) {
        Memory m; m.Put(Memory::manager + 0x644, count);
        Require(m.Observe().status == Status::invalid_count, "invalid signed global count accepted");
        Memory f; f.Put(Memory::Effect(0) + 0x644, count);
        Require(f.Observe().status == Status::invalid_count, "invalid signed local count accepted");
    }
    Memory n; for (unsigned i = 0; i < 260; ++i) n.Put(Memory::Effect(0) + 0x684 + i, std::uint8_t('X'));
    Require(n.Observe().status == Status::invalid_name, "unterminated effect accepted");
    Memory b; for (unsigned i = 0; i < 64; ++i) b.Put(Memory::Entry(Memory::manager, 0) + 4 + i, std::uint8_t('X'));
    Require(b.Observe().status == Status::invalid_name, "unterminated buffer accepted");
    Memory edge; const std::string long_name(259, 'X');
    edge.Text(Memory::Effect(0) + 0x684, long_name.c_str(), 260);
    Require(edge.Observe().valid, "effect terminating at byte259 rejected");
    for (unsigned i = 0; i < 63; ++i) edge.Put(Memory::Entry(Memory::manager, 0) + 4 + i, std::uint8_t('X'));
    Require(edge.Observe().valid, "buffer terminating at byte63 rejected");
    Memory c; c.Put(Memory::Node(0), Memory::Node(0));
    Require(c.Observe().status == Status::invalid_topology, "cycle accepted");
    Memory prev; prev.Put(Memory::Node(0) + 4, Memory::Node(0));
    Require(prev.Observe().status == Status::invalid_topology, "broken backward link accepted");
    Memory tail; tail.Put(Memory::sentinel + 4, Memory::sentinel);
    Require(tail.Observe().status == Status::invalid_topology, "inconsistent sentinel accepted");
    Memory cycle; cycle.Effects(3, 0); cycle.Put(Memory::Node(2), Memory::Node(0));
    Require(cycle.Observe().status == Status::invalid_topology, "multi-node cycle accepted");
    Memory repeat; repeat.Effects(2, 0); repeat.Put(Memory::Node(1) + 8, Memory::Effect(0));
    Require(repeat.Observe().status == Status::invalid_topology, "repeated effect identity accepted");
    Memory invalid; invalid.Put(Memory::Node(0) + 8, 0xffffffffu);
    Require(!invalid.Observe().valid, "all-ones effect accepted");
    Memory size; size.Put(Memory::manager + 0x650, 0u);
    Require(size.Observe().status == Status::invalid_topology, "list size/topology disagreement accepted");
    Memory manager_class; manager_class.Put(Memory::manager, Memory::engine + 0x3180ac);
    Require(manager_class.Observe().status == Status::object_mismatch, "foreign manager class accepted");
    Memory effect_class; effect_class.Put(Memory::Effect(0), Memory::engine + 0x314c48);
    Require(effect_class.Observe().status == Status::object_mismatch, "foreign effect class accepted");
    Memory empty; empty.Effects(0, 0); empty.Put(Memory::manager + 0x644, 0u);
    Require(empty.Observe().valid, "consistent empty list rejected");
}

void IncompleteAndChanged() {
    Memory dirty; dirty.Put(Memory::manager + 0x6b5, std::uint8_t(9)); auto s = dirty.Observe();
    Require(!s.valid && s.status == Status::dirty && s.manager_dirty == 9 && s.buffer_count == 15, "dirty snapshot promoted or discarded");
    Memory missing; missing.Put(Memory::Entry(Memory::Effect(0), 0), 0u); s = missing.Observe();
    Require(!s.valid && s.status == Status::unallocated && s.buffer_count == 15 && s.buffers[1].texture.handle == 0,
        "unallocated descriptor promoted or discarded");
    Memory com; com.Put(Memory::Handle(0) + 4, 0u);
    Require(com.Observe().status == Status::unallocated, "native object without COM allocation accepted");
    for (auto address : {Memory::engine + 0x590074, Memory::engine + 0x590078, Memory::engine + 0x5902ac,
            Memory::renderer, Memory::manager + 0x64c, Memory::sentinel, Memory::sentinel + 4,
            Memory::manager + 0x644, Memory::manager + 0x6b5, Memory::manager, Memory::manager + 0x650}) {
        Memory change; change.change_address = address;
        change.change_value = (address == Memory::manager + 0x644 || address == Memory::manager + 0x650) ? 2u : 1u;
        Require(!change.Observe().valid, "root changed on reread accepted");
        Require(change.change_hit >= 2, "root not reread");
    }
    Memory d; d.Put(Memory::Entry(Memory::manager, 0) + 0x44, 0u);
    Require(d.Observe().status == Status::invalid_dimensions, "zero logical width accepted");
    Memory signed_dimension; signed_dimension.Put(Memory::Entry(Memory::manager, 0) + 0x48, 0xffffffffu);
    Require(signed_dimension.Observe().status == Status::invalid_dimensions, "negative logical height accepted");
    Memory physical; physical.Put(Memory::Handle(0) + 0x14c, 0u);
    Require(physical.Observe().status == Status::invalid_dimensions, "zero physical height accepted");
    Memory nan; nan.Put(Memory::Handle(0) + 0x154, std::numeric_limits<float>::quiet_NaN());
    Require(nan.Observe().status == Status::invalid_dimensions, "NaN inverse size accepted");
}

void CapsAndReadFailures() {
    Memory cap; cap.Put(Memory::manager + 0x644, 16u);
    for (unsigned i = 0; i < 16; ++i) cap.Buffer(Memory::manager, i, "global", 2560, 1440, Memory::Handle(i));
    cap.Effects(16, 16); auto s = cap.Observe();
    Require(s.valid && s.effect_count == 16 && s.buffer_count == 272, "exact capacity rejected/truncated");
    Memory over; over.Effects(17, 0); s = over.Observe();
    Require(!s.valid && s.status == Status::capacity_exceeded && s.effect_count == 16, "effect overflow not incomplete/bounded");
    Memory good; Require(good.Observe().valid, "read-fault fixture invalid");
    const auto reads = good.addresses;
    for (auto address : reads) {
        Memory fail; fail.fail_address = address;
        Require(!fail.Observe().valid, "failed memory read accepted");
    }
    // Fail every *invocation*, including final root rereads, not just the first
    // access to an address. A reader may partially fill destination then fail.
    for (std::size_t call = 1; call <= reads.size(); ++call) {
        good.fail_call = call;
        auto failed = good.Observe();
        Require(!failed.valid && failed.status == Status::read_failed, "individual read failure promoted");
        Require(good.read_calls == call, "decoder continued reading after callback failure");
    }
}
} // namespace

static_assert(std::is_trivially_copyable_v<CoJPostprocessResourceSnapshot>);
static_assert(noexcept(ObserveCoJPostprocessResources({}, 0, false)));
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--telemetry") {
        Memory memory;
        const auto snapshot = memory.Observe();
        if (!snapshot.valid) return 1;
        CoJPostprocessObservationContext context;
        context.thread = 3; context.frame = 9; context.owner = 20;
        context.device = snapshot.device; context.device_generation = 2;
        context.probe_epoch = 7;
        for (unsigned eye = 0; eye < 2; ++eye) {
            context.eye = eye; context.scope = eye + 1;
            for (bool begin : {true, false}) {
                context.begin = begin; context.scope_complete = !begin;
                std::cout << FormatCoJPostprocessObservation(snapshot, context) << '\n';
            }
        }
        return 0;
    }
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {{"recipe, aliases and unsigned identities", ValidRecipeAndAliases},
        {"admission and overflow", AdmissionAndOverflow}, {"malformed layouts", MalformedLayouts},
        {"incomplete and changed roots", IncompleteAndChanged}, {"capacity and every read fault", CapsAndReadFailures}};
    unsigned failed = 0;
    for (const auto& test : tests) {
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << test.name << ": " << error.what() << '\n'; }
    }
    return failed ? 1 : 0;
}
