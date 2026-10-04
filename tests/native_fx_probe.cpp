#include "games/call_of_juarez/native_fx_probe.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <bit>

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
constexpr std::uint32_t OwnerId=0x30000000, Binding=0x30000100,
    Owner=0x30000200, Module=0x30001000, Manager=0x30002000,
    Table=0x30003000, Emitter=0x30004000, Handle=0x12340002;
void PopulateEmitter(Fixture& f) {
    auto& w=f.words;
    w[OwnerId+4]=Binding; w[Binding+4]=Owner; w[Owner]=Base+0x2F0000;
    w[Owner+0x24]=Module; w[Module]=Base+0x2F1000; w[Module+0x370]=Manager;
    w[Manager+0x18]=Table; w[Manager+0x20]=4;
    w[Table+32]=Handle&0xFFFF0000; w[Table+36]=Emitter;
    w[Emitter]=Base+0x2FDE7C; w[Emitter+4]=Manager; w[Emitter+0x58]=Handle;
    w[Manager+4]=std::bit_cast<std::uint32_t>(1.5F);
    for (auto offset:{0xC,0x10,0x14}) w[Emitter+offset]=std::bit_cast<std::uint32_t>(1.F);
    w[Emitter+0x5C]=0x101;
    for (auto offset:{0xE0,0xE4,0xE8,0xF0,0xF4,0xF8})
        w[Emitter+offset]=std::bit_cast<std::uint32_t>(10.F);
    w[Emitter+0x40C]=3; w[Emitter+0x410]=1; w[Emitter+0x414]=2;
}
CoJNativeFxSnapshot EmitterSample(Fixture& f, std::uint32_t handle=Handle) {
    return ReadCoJNativeFxSnapshot({Base,true,handle,CoJNativeFxPhase::left_eye,OwnerId},
        {Fixture::Read,&f});
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
    for (auto base : {std::uintptr_t{0}, std::uintptr_t{0xFFFFFFFF}, std::uintptr_t{0xFFA00000}}) {
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
    Fixture live; PopulateEmitter(live); s=EmitterSample(live);
    if (!s.emitter || s.lookup_reason!=CoJNativeFxReadReason::observed ||
        s.emitter->live!=3 || s.emitter->emitted!=1 || s.emitter->expired!=2 ||
        s.emitter->clock!=1.5F || s.emitter->position[0]!=10.F ||
        !s.emitter->enabled || !s.emitter->render_enabled) return 18;
    Fixture rotated; PopulateEmitter(rotated); rotated.words[Emitter]=Base+0x2FDEB8;
    if (!EmitterSample(rotated).emitter) return 19;
    Fixture stale; PopulateEmitter(stale); s=EmitterSample(stale,0xABCD0002);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::stale_handle ||
        stale.per_address.count(Emitter)) return 20;
    Fixture index; PopulateEmitter(index); s=EmitterSample(index,0x1234FFFF);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::invalid_slot ||
        index.per_address.count(Table+0xFFFF*16)) return 21;
    for (auto address:{OwnerId+4,Binding+4,Owner+0x24,Module+0x370,
                       Manager+0x18,Manager+0x20,Table+32,Table+36,Emitter}) {
        Fixture changed; PopulateEmitter(changed); changed.change_at=address;
        s=EmitterSample(changed);
        if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::changed_during_read) return 22;
    }
    Fixture unknown; PopulateEmitter(unknown); unknown.words[Emitter]=Base+0x2F0000;
    s=EmitterSample(unknown);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::unknown_emitter_type ||
        unknown.per_address.count(Emitter+0x40C)) return 23;
    Fixture partial_emitter; PopulateEmitter(partial_emitter);
    partial_emitter.short_at=Emitter+0x410; partial_emitter.short_size=2;
    s=EmitterSample(partial_emitter);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::partial_read) return 24;
    Fixture nan; PopulateEmitter(nan); nan.words[Emitter+0xE0]=0x7FC00000;
    s=EmitterSample(nan);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::invalid_values) return 25;
    Fixture wrong_owner; PopulateEmitter(wrong_owner); wrong_owner.words[Owner]=0x40000000;
    s=EmitterSample(wrong_owner);
    if (s.emitter || s.lookup_reason!=CoJNativeFxReadReason::invalid_owner) return 26;
    CoJNativeFxObservationWindow budget;
    if (budget.Observe(0,OwnerId,0,0) || !budget.Observe(0,OwnerId,1,10) ||
        budget.Observe(0,OwnerId,1,11) || !budget.Observe(0,OwnerId,1,30) ||
        budget.Observe(0,OwnerId,1,510)) return 27;
    budget.Invalidate(0);
    for (int shot=2;shot<=16;++shot)
        if (!budget.Observe(0,OwnerId,shot,static_cast<std::uint64_t>(shot)*1000)) return 28;
    if (budget.Observe(0,OwnerId,17,17000) || budget.NeedsPoll(17000)) return 29;
    budget.Invalidate(0); budget.Invalidate(1);
    if (budget.Observe(1,OwnerId,1,18000) || budget.NeedsPoll(18000)) return 30;
    std::cout << "native FX owner/generation/subtype reads, failure cleanup and bounded cadence passed\n";
    return 0;
}
