#include "games/call_of_juarez/native_fx_probe.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>

using namespace cojvr::games::call_of_juarez;
namespace {
constexpr std::uintptr_t Base = 0x10000000;
constexpr std::uintptr_t Global = Base + 0x37E6D0;
constexpr std::uintptr_t RendererGlobal = Base + 0x590074;
constexpr std::uint32_t Renderer = 0x20000000;
constexpr std::uint32_t Camera = 0x21000000;
struct Fixture {
    std::map<std::uintptr_t, std::uint32_t> words;
    std::uintptr_t short_at = 0;
    std::size_t short_size = 0;
    std::uintptr_t change_at = 0;
    std::size_t reads = 0;
    std::map<std::uintptr_t, int> per_address;
    Fixture() {
        words[Global] = 1;
        words[RendererGlobal] = Renderer;
        words[Renderer + 0x1B0] = Camera;
        words[Renderer + 0x1B8] = 0;
    }
    static std::size_t Read(void* context, std::uintptr_t address,
        void* destination, std::size_t size) noexcept {
        auto& f = *static_cast<Fixture*>(context);
        ++f.reads;
        const int seen = ++f.per_address[address];
        auto entry = f.words.find(address);
        if (entry == f.words.end() || size != sizeof(std::uint32_t)) return 0;
        std::uint32_t value = entry->second;
        if (address == f.change_at && seen > 1) value ^= 1;
        const auto copied = address == f.short_at ? f.short_size : size;
        std::memcpy(destination, &value, copied);
        return copied;
    }
    CoJNativeFxSnapshot Sample(std::uint32_t handle = 0x12340002) {
        return ReadCoJNativeFxSnapshot({Base, true, handle, CoJNativeFxPhase::left_eye},
            {Read, this});
    }
};
bool LookupUnavailable(const CoJNativeFxSnapshot& s) {
    return s.lookup_reason == CoJNativeFxReadReason::missing_owner_lookup_boundary;
}
}

int main() {
    Fixture f;
    auto s = f.Sample();
    if (!s.global_fx || *s.global_fx != 1 ||
        s.global_reason != CoJNativeFxReadReason::observed) {
        std::cerr << "recognized global FX word was not safely observed\n"; return 1;
    }
    if (!LookupUnavailable(s) || s.handle != 0x12340002 ||
        s.phase != CoJNativeFxPhase::left_eye) return 2;
    if (s.fx_camera_address != Camera || s.renderer_address != Renderer ||
        s.plane_reason != CoJNativeFxReadReason::missing_plane_extent_boundary) return 3;
    for (auto [address, count] : f.per_address) {
        if (address != Global && address != RendererGlobal &&
            address != Renderer + 0x1B0 && address != Renderer + 0x1B8) return 4;
    }
    Fixture off; off.words[Global] = 0;
    if (off.Sample().global_fx != 0u) return 5; // zero is observed, not unavailable
    Fixture fallback; fallback.words[Renderer + 0x1B0] = 0;
    fallback.words[Renderer + 0x1B8] = Camera;
    if (fallback.Sample().fx_camera_address != Camera) return 6;
    Fixture none; none.words[RendererGlobal] = 0;
    s = none.Sample();
    if (s.fx_camera_address != 0u || s.renderer_address != 0u ||
        s.camera_reason != CoJNativeFxReadReason::observed) return 7;
    Fixture missing; missing.words.erase(Global);
    s = missing.Sample();
    if (s.global_fx || s.global_reason != CoJNativeFxReadReason::read_failed ||
        !LookupUnavailable(s)) return 8;
    for (std::size_t size : {std::size_t{1}, std::size_t{3}}) {
        Fixture partial; partial.short_at = Global; partial.short_size = size;
        s = partial.Sample();
        if (s.global_fx || s.global_reason != CoJNativeFxReadReason::partial_read ||
            !LookupUnavailable(s)) return 9;
    }
    Fixture race; race.change_at = Global;
    s = race.Sample();
    if (s.global_fx || s.global_reason != CoJNativeFxReadReason::changed_during_read) return 10;
    for (auto address : {RendererGlobal, std::uintptr_t(Renderer + 0x1B0),
                         std::uintptr_t(Renderer + 0x1B8)}) {
        Fixture camera_race; camera_race.change_at = address;
        s = camera_race.Sample();
        if (s.fx_camera_address || s.renderer_address ||
            s.camera_reason != CoJNativeFxReadReason::changed_during_read || !s.global_fx) return 11;
    }
    Fixture partial_camera; partial_camera.short_at = Renderer + 0x1B0;
    partial_camera.short_size = 2;
    s = partial_camera.Sample();
    if (s.fx_camera_address || s.camera_reason != CoJNativeFxReadReason::partial_read ||
        !s.global_fx) return 12;
    Fixture untrusted;
    s = ReadCoJNativeFxSnapshot({Base, false, 1, CoJNativeFxPhase::frame}, {Fixture::Read, &untrusted});
    if (untrusted.reads || s.global_fx || s.fx_camera_address ||
        s.lookup_reason != CoJNativeFxReadReason::unrecognized_engine) return 13;
    for (auto base : {std::uintptr_t{0}, std::uintptr_t{0xFFFFFFFF}}) {
        Fixture bad;
        s = ReadCoJNativeFxSnapshot({base, true, 1, CoJNativeFxPhase::frame}, {Fixture::Read, &bad});
        if (bad.reads || s.global_fx || s.lookup_reason != CoJNativeFxReadReason::invalid_engine_base) return 14;
    }
    // Without a proven owner->manager boundary, arbitrary index/generation
    // inputs cannot cause guessed table reads. This API has no emitter values.
    // These are fallback assertions, not claims to test a native slot resolver.
    for (auto handle : {0u, 0x1234FFFFu, 0xFFFF0001u, 0x12340002u}) {
        Fixture arbitrary;
        if (!LookupUnavailable(arbitrary.Sample(handle))) return 15;
    }
    s = ReadCoJNativeFxSnapshot({Base, true, 1, CoJNativeFxPhase::frame}, {});
    if (s.global_fx || s.global_reason != CoJNativeFxReadReason::read_failed) return 16;
    if (std::strcmp(CoJNativeFxReadReasonName(s.lookup_reason), "missing_owner_lookup_boundary")) return 17;
    std::cout << "native FX observational fallback: memory failures and camera rechecks passed\n";
    return 0;
}
