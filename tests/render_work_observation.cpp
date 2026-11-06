#include "games/call_of_juarez/render_work_observation.hpp"

#include <iostream>

using namespace cojvr::games::call_of_juarez;
namespace {
bool Check(bool value, const char* reason) {
    if (!value) std::cerr << reason << '\n';
    return value;
}
CoJRenderWorkStamp Stamp(std::uint64_t frame = 10, unsigned eye = 0) {
    return {frame, 0xf1234567u, 3, 7, 23, eye, CoJRenderWorkRoute::gameplay};
}
void Eye(CoJRenderWorkObservation& o, CoJRenderWorkStamp s, std::uint64_t start,
    std::uint64_t duration, bool complete = true) {
    o.Observe(s, true, false, start);
    o.Observe(s, false, complete, start + duration);
}
void Pair(CoJRenderWorkObservation& o, std::uint64_t frame, std::uint64_t l = 10,
    std::uint64_t r = 20) {
    Eye(o, Stamp(frame), 100, l);
    Eye(o, Stamp(frame, 1), 200, r);
}
bool ExactPairAndStatistics() {
    CoJRenderWorkObservation o;
    Pair(o, 10, 10, 20);
    Pair(o, 100, 20, 40);
    Pair(o, 190, 30, 60);
    bool ok = Check(o.samples().size() == 3, "complete full-wrapper pairs must be retained");
    if (!o.samples().empty()) {
        const auto& p = o.samples()[0];
        ok &= Check(p.left == Stamp() && p.right == Stamp(10, 1) &&
            p.left_cpu_wall_ns == 10 && p.right_cpu_wall_ns == 20,
            "CPU-wall sample must retain exact eye/frame/LAA owner/generation/epoch/thread");
    }
    const auto l = o.Statistics(CoJRenderWorkRoute::gameplay, 0);
    const auto r = o.Statistics(CoJRenderWorkRoute::gameplay, 1);
    const auto p = o.Statistics(CoJRenderWorkRoute::gameplay, 2);
    ok &= Check(l.count == 3 && l.mean_ns == 20 && l.min_ns == 10 && l.max_ns == 30 &&
        l.p50_ns == 20 && l.p95_ns == 30 && l.p99_ns == 30 && r.mean_ns == 40 &&
        p.mean_ns == 60 && p.max_ns == 90, "per-eye and summed-wrapper nearest-rank statistics");
    ok &= Check(o.Statistics(CoJRenderWorkRoute::movie, 0).count == 0 &&
        o.Statistics(CoJRenderWorkRoute::gameplay, 3).count == 0,
        "empty/invalid statistics must not invent measurements");
    const auto c = o.counters();
    return Check(c.attempts == 3 && c.begins == 6 && c.completes == 6 && !c.rejected,
        "wrapper counters count actual observed scopes, never draws") && ok;
}
bool RejectMixedIdentity() {
    bool ok = true;
    for (unsigned field = 0; field != 8; ++field) {
        CoJRenderWorkObservation o;
        Eye(o, Stamp(), 100, 10);
        auto right = Stamp(10, 1);
        switch (field) {
        case 0: ++right.frame; break;
        case 1: ++right.owner; break;
        case 2: ++right.generation; break;
        case 3: ++right.epoch; break;
        case 4: ++right.thread; break;
        case 5: right.route = CoJRenderWorkRoute::movie; break;
        case 6: right.eye = 0; break;
        case 7: right.eye = 2; break;
        }
        Eye(o, right, 200, 20);
        Eye(o, Stamp(10, 1), 300, 20);
        ok &= Check(o.samples().empty(), "a mixed or duplicate eye must poison pending pair");
        Pair(o, 100);
        ok &= Check(o.samples().size() == 1, "fresh pair recovers after rejected identity");
    }
    for (unsigned field = 0; field != 8; ++field) {
        CoJRenderWorkObservation o;
        auto end = Stamp();
        switch (field) {
        case 0: ++end.frame; break;
        case 1: ++end.owner; break;
        case 2: ++end.generation; break;
        case 3: ++end.epoch; break;
        case 4: ++end.thread; break;
        case 5: end.route = CoJRenderWorkRoute::settings; break;
        case 6: end.eye = 1; break;
        case 7: end.eye = 2; break;
        }
        o.Observe(Stamp(), true, false, 100);
        o.Observe(end, false, true, 110);
        Eye(o, Stamp(10, 1), 200, 20);
        ok &= Check(o.samples().empty(), "end must match every entry identity field");
    }
    return ok;
}
bool RejectInvalidAndIncomplete() {
    bool ok = true;
    for (unsigned field = 0; field != 8; ++field) {
        CoJRenderWorkObservation o;
        auto s = Stamp();
        switch (field) {
        case 0: s.frame = 0; break;
        case 1: s.owner = 0; break;
        case 2: s.generation = 0; break;
        case 3: s.epoch = 0; break;
        case 4: s.thread = 0; break;
        case 5: s.route = CoJRenderWorkRoute::invalid; break;
        case 6: s.eye = 2; break;
        case 7: s.owner = ~std::uintptr_t{0}; break;
        }
        Eye(o, s, 100, 10);
        Eye(o, Stamp(10, 1), 200, 10);
        ok &= Check(o.samples().empty(), "unknown/invalid native stamp cannot yield a sample");
    }
    for (unsigned failed_eye = 0; failed_eye != 2; ++failed_eye) {
        CoJRenderWorkObservation o;
        Eye(o, Stamp(), 100, 10, failed_eye != 0);
        Eye(o, Stamp(10, 1), 200, 20, failed_eye != 1);
        Eye(o, Stamp(10, 1), 300, 20);
        ok &= Check(o.samples().empty(), "exception/rejected wrapper cannot be repaired by late end/replay");
    }
    CoJRenderWorkObservation regressing;
    regressing.Observe(Stamp(), true, false, 100);
    regressing.Observe(Stamp(), false, true, 99);
    Eye(regressing, Stamp(10, 1), 200, 20);
    return Check(regressing.samples().empty(), "regressing CPU clock must reject, not underflow") && ok;
}
bool RejectNestedAndUnbalanced() {
    bool ok = true;
    for (const unsigned nested_eye : {0u, 1u}) {
        CoJRenderWorkObservation o;
        if (nested_eye) Eye(o, Stamp(), 100, 10);
        auto s = Stamp(10, nested_eye);
        o.Observe(s, true, false, 200);
        o.Observe(s, true, false, 201);
        o.Observe(s, false, true, 210);
        o.Observe(s, false, true, 220);
        Eye(o, Stamp(10, 1), 300, 20);
        ok &= Check(o.samples().empty() && o.counters().nested == 1,
            "nested scope poisons entire pair until all entries drain");
        Pair(o, 100);
        ok &= Check(o.samples().size() == 1, "balanced fresh scope recovers after nested rejection");
    }
    CoJRenderWorkObservation o;
    Eye(o, Stamp(), 100, 10);
    o.Observe(Stamp(), false, true, 111);
    Eye(o, Stamp(10, 1), 200, 20);
    ok &= Check(o.samples().empty() && o.counters().unbalanced == 1,
        "unbalanced/duplicate end invalidates pending left");
    CoJRenderWorkObservation unterminated;
    unterminated.Observe(Stamp(), true, false, 100);
    Pair(unterminated, 100);
    return Check(unterminated.samples().empty(), "missing end cannot create a later valid mixed pair") && ok;
}
bool ResetAndBudgets() {
    CoJRenderWorkObservation o;
    Pair(o, 10);
    Eye(o, Stamp(100), 100, 10);
    o.Invalidate();
    Eye(o, Stamp(100, 1), 200, 20);
    bool ok = Check(o.samples().size() == 1 && o.counters().invalidations == 1,
        "reset retires pending eye but preserves earlier aggregate evidence");
    Pair(o, 190);
    Pair(o, 190);
    Pair(o, 189);
    Pair(o, 191);
    ok &= Check(o.samples().size() == 2, "duplicate/backwards/unscheduled frames cannot replay samples");
    auto fresh = Stamp(280);
    fresh.epoch = 8;
    fresh.generation = 4;
    Eye(o, fresh, 300, 7);
    fresh.eye = 1;
    Eye(o, fresh, 400, 9);
    ok &= Check(o.samples().size() == 3 && o.samples().back().right == fresh &&
        o.Statistics(CoJRenderWorkRoute::gameplay, 0).mean_ns == 9,
        "new reset/generation domain produces fresh stamped evidence without relabelling old samples");
    CoJRenderWorkObservation active;
    active.Observe(Stamp(), true, false, 100);
    active.Invalidate();
    active.Observe(Stamp(), false, true, 110);
    Eye(active, Stamp(10, 1), 200, 20);
    ok &= Check(active.samples().empty(), "reset during wrapper invalidates in-flight timing");
    CoJRenderWorkObservation full;
    for (std::uint64_t i = 0; i != 300; ++i) {
        Pair(full, 10 + 90 * i);
        if (i % 2) full.Invalidate();
    }
    ok &= Check(full.samples().size() == CoJRenderWorkObservation::capacity && full.finished() &&
        full.counters().attempts == CoJRenderWorkObservation::capacity,
        "sample storage and lifetime budget stay bounded through reset");
    CoJRenderWorkObservation rejected;
    for (std::uint64_t i = 0; i != 300; ++i) {
        Eye(rejected, Stamp(10 + 90 * i), 100, 10, false);
        rejected.Invalidate();
    }
    ok &= Check(rejected.samples().empty() && rejected.finished() &&
        rejected.counters().attempts == CoJRenderWorkObservation::attempt_limit,
        "failed attempts also exhaust lifetime budget without valid pairs");
    return ok;
}
bool RouteAndTailStatistics() {
    CoJRenderWorkObservation o;
    for (std::uint64_t i = 1; i <= 20; ++i) Pair(o, 10 + 90 * i, i, i * 2);
    auto movie = Stamp(20);
    movie.route = CoJRenderWorkRoute::movie;
    Eye(o, movie, 100, 400);
    movie.eye = 1;
    Eye(o, movie, 600, 500);
    auto gameplay = o.Statistics(CoJRenderWorkRoute::gameplay, 0);
    auto cinematic = o.Statistics(CoJRenderWorkRoute::movie, 2);
    return Check(gameplay.count == 20 && gameplay.mean_ns == 10.5 && gameplay.p50_ns == 10 &&
        gameplay.p95_ns == 19 && gameplay.p99_ns == 20 && cinematic.count == 1 &&
        cinematic.mean_ns == 900, "route aggregates isolate movie work and use correct tail ranks");
}
bool StartupRoutesCannotStarveGameplay() {
    bool ok = true;
    for (bool complete : {false, true}) {
        CoJRenderWorkObservation o;
        for (auto route : {CoJRenderWorkRoute::settings, CoJRenderWorkRoute::movie}) {
            for (std::uint64_t i = 0; i < 150; ++i) {
                auto s = Stamp(10 + 90 * i);
                s.route = route;
                Eye(o, s, 100, 10, complete);
                s.eye = 1;
                Eye(o, s, 200, 20, complete);
            }
        }
        Pair(o, 15000);
        ok &= Check(o.Statistics(CoJRenderWorkRoute::gameplay, 0).count == 1,
            "long menus/movies, even failed wrappers, must retain gameplay measurement budget");
    }
    return ok;
}
bool ClocksOnlyForSampledScopes() {
    CoJRenderWorkObservation o;
    auto left = Stamp();
    bool ok = Check(o.NeedsTimestamp(left, true), "selected wrapper must request start timestamp");
    o.Observe(left, true, false, 100);
    ok &= Check(o.NeedsTimestamp(left, false) && !o.NeedsTimestamp(left, true),
        "active selected end needs clock but nested entry must not request one");
    o.Observe(left, false, true, 110);
    auto right = Stamp(10, 1);
    ok &= Check(o.NeedsTimestamp(right, true), "matched right wrapper must request timestamp");
    Eye(o, right, 200, 20);
    left.frame = 11;
    ok &= Check(!o.NeedsTimestamp(left, true), "unscheduled frame must not request a clock read");
    Eye(o, left, 0, 0);
    ok &= Check(!o.NeedsTimestamp(Stamp(11, 1), true), "unsampled right must not request clock");
    left.frame = 100;
    ok &= Check(o.NeedsTimestamp(left, true), "later scheduled frame resumes bounded timing");
    o.Observe(left, true, false, 100);
    o.Invalidate();
    return Check(!o.NeedsTimestamp(left, false), "reset-in-flight must skip rejected timing work") && ok;
}
}
int main() {
    bool ok = ExactPairAndStatistics();
    ok &= RejectMixedIdentity();
    ok &= RejectInvalidAndIncomplete();
    ok &= RejectNestedAndUnbalanced();
    ok &= ResetAndBudgets();
    ok &= RouteAndTailStatistics();
    ok &= StartupRoutesCannotStarveGameplay();
    ok &= ClocksOnlyForSampledScopes();
    return ok ? 0 : 1;
}
