#include "games/call_of_juarez/hud_boundary_probe.hpp"
#include <iostream>
#include <vector>

using namespace cojvr::games::call_of_juarez;
namespace {
int sprite_calls = 0, flush_calls = 0;
void* forwarded_owner = nullptr;
std::uint32_t forwarded_option = 0;
std::vector<CoJHudBoundaryEvent> observations;
void __fastcall Sprites(void* owner, void*) { ++sprite_calls; forwarded_owner = owner; }
void __fastcall Flush(void* owner, void*, std::uint32_t option) {
    ++flush_calls; forwarded_owner = owner; forwarded_option = option;
}
void __fastcall Foreign(void*, void*) {}
void Observe(void*, const CoJHudBoundaryEvent& event) noexcept { observations.push_back(event); }
struct Race { void** entry = nullptr; bool applied = false; };
bool Protect(void* address, std::size_t, std::uint32_t, std::uint32_t* previous, void* context) noexcept {
    auto& race = *static_cast<Race*>(context);
    if (previous) *previous = 0x20;
    if (!race.applied && address == race.entry) {
        *race.entry = reinterpret_cast<void*>(&Foreign); race.applied = true;
    }
    return true;
}
void* Compare(void** entry, void* replacement, void* expected, void*) noexcept {
    void* actual = *entry;
    if (actual == expected) *entry = replacement;
    return actual;
}
}
int main() {
    void* sprites[] = {reinterpret_cast<void*>(&Sprites)};
    void* flush[] = {reinterpret_cast<void*>(&Flush)};
    CoJHudBoundaryHookTargets targets{sprites, sprites[0], flush, flush[0]};
    if (InstallCoJHudBoundaryProbe(targets, false, Observe, nullptr)) return 1;
    if (sprites[0] != reinterpret_cast<void*>(&Sprites)) return 2;
    auto bad = targets; bad.expected_sprites = reinterpret_cast<void*>(&Foreign);
    if (InstallCoJHudBoundaryProbe(bad, true, Observe, nullptr)) return 3;
    if (!InstallCoJHudBoundaryProbe(targets, true, Observe, nullptr)) return 4;
    using SpriteFn = void(__thiscall*)(void*);
    using FlushFn = void(__thiscall*)(void*, std::uint32_t);
    int owner = 42;
    reinterpret_cast<SpriteFn>(sprites[0])(&owner);
    reinterpret_cast<FlushFn>(flush[0])(&owner, 0xABCD);
    if (sprite_calls != 1 || flush_calls != 1 || forwarded_owner != &owner ||
        forwarded_option != 0xABCD || observations.size() != 4) return 5;
    if (observations[0].stage != CoJHudBoundaryStage::sprites_begin ||
        observations[1].stage != CoJHudBoundaryStage::sprites_end ||
        observations[2].stage != CoJHudBoundaryStage::flush_begin ||
        observations[3].stage != CoJHudBoundaryStage::flush_end ||
        observations[0].pass_sequence != observations[3].pass_sequence) return 6;
    const auto observed = observations.size();
    reinterpret_cast<FlushFn>(flush[0])(&owner, 0); // unrelated flush has no pending sprite pass
    if (observations.size() != observed || flush_calls != 2) return 7;
    if (!RestoreCoJHudBoundaryProbe()) return 8;
    if (sprites[0] != targets.expected_sprites || flush[0] != targets.expected_flush) return 9;
    reinterpret_cast<SpriteFn>(sprites[0])(&owner);
    if (sprite_calls != 2 || observations.size() != observed) return 10;
    if (!InstallCoJHudBoundaryProbe(targets, true, Observe, nullptr) ||
        !RestoreCoJHudBoundaryProbe()) return 11;
    // A foreign install between precheck and slot CAS is never overwritten.
    for (void** raced : {sprites, flush}) {
        Race race{raced};
        cojvr::backends::d3d9::VtableMemoryOperations operations{Protect, Compare, &race};
        if (InstallCoJHudBoundaryProbe(targets, true, Observe, nullptr, operations) ||
            *raced != reinterpret_cast<void*>(&Foreign)) return 15;
        void** other = raced == sprites ? flush : sprites;
        if (*other != (raced == sprites ? targets.expected_flush : targets.expected_sprites)) return 16;
        // Simulate the foreign owner restoring its own slot before another test.
        *raced = raced == sprites ? targets.expected_sprites : targets.expected_flush;
        if (!RestoreCoJHudBoundaryProbe()) return 17;
    }
    if (!InstallCoJHudBoundaryProbe(targets, true, Observe, nullptr)) return 18;
    observations.clear();
    int owner_a = 1, owner_b = 2;
    for (int pass = 0; pass < 300; ++pass) {
        reinterpret_cast<SpriteFn>(sprites[0])(pass % 2 == 0 ? &owner_a : &owner_b);
        reinterpret_cast<FlushFn>(flush[0])(&owner, 0);
    }
    bool seen_a = false, seen_b = false;
    for (const auto& event : observations) {
        if (event.pass_sequence <= 8 || event.stage != CoJHudBoundaryStage::sprites_begin) continue;
        seen_a |= event.owner == &owner_a; seen_b |= event.owner == &owner_b;
    }
    if (!seen_a || !seen_b || observations.size() % 4 != 0 || !RestoreCoJHudBoundaryProbe()) return 19;
    void* foreign_sprites[] = {reinterpret_cast<void*>(&Sprites)};
    void* foreign_flush[] = {reinterpret_cast<void*>(&Flush)};
    CoJHudBoundaryHookTargets foreign_targets{
        foreign_sprites, foreign_sprites[0], foreign_flush, foreign_flush[0]};
    if (!InstallCoJHudBoundaryProbe(foreign_targets, true, Observe, nullptr)) return 12;
    foreign_sprites[0] = reinterpret_cast<void*>(&Foreign);
    if (RestoreCoJHudBoundaryProbe() || foreign_sprites[0] != reinterpret_cast<void*>(&Foreign) ||
        foreign_flush[0] != foreign_targets.expected_flush) return 13;
    if (RestoreCoJHudBoundaryProbe()) return 14; // repeated shutdown cannot hide a foreign slot
    std::cout << "HUD probe forwarding, exact-build guard and owned restoration passed\n";
}
