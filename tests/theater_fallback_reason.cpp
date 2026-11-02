#include "games/call_of_juarez/theater_fallback_reason.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace cojvr::games::call_of_juarez;

namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
void Expect(const CoJPresentationOwner& owner, bool observed, bool immersive,
            bool settings, bool release, CoJTheaterFallbackReason reason) {
    Check(ClassifyCoJTheaterFallback(owner, observed, immersive, settings, release) == reason,
          "incorrect theater fallback classification");
    Check(std::string_view(CoJTheaterFallbackReasonName(reason)) != "unknown",
          "unmapped theater diagnostic reason");
}
}

int main() {
    CoJPresentationOwner normal{};
    normal.valid = true;
    normal.player_alive = true;
    CoJPresentationOwner movie = normal;
    movie.movie_playing = true;
    CoJPresentationOwner frozen = movie;
    frozen.timer_frozen = true;
    CoJPresentationOwner mission = movie;
    mission.mission_end_visible = true;
    CoJPresentationOwner dead = movie;
    dead.player_alive = false;

    using R = CoJTheaterFallbackReason;
    Expect({}, false, false, false, false, R::owner_observation_failed);
    Expect(normal, false, false, false, false, R::owner_observation_failed);
    Expect({}, true, false, true, true, R::owner_observation_failed);
    Expect(movie, true, false, false, false, R::native_blocking_presentation);
    Expect(frozen, true, false, false, false, R::native_blocking_presentation);
    Expect(frozen, true, true, false, false, R::native_stereo_stale); // Diagnostic input only.
    Expect(mission, true, false, false, false, R::native_blocking_presentation);
    Expect(dead, true, false, false, false, R::native_blocking_presentation);
    Expect(movie, true, true, true, true, R::ui_release_barrier);
    Expect(movie, true, true, true, false, R::settings_capture);
    Expect(movie, true, true, false, false, R::native_stereo_stale);
    Expect(normal, true, false, false, true, R::ui_release_barrier);
    Expect(normal, true, false, true, false, R::settings_capture);
    Expect(normal, true, false, false, false, R::native_stereo_stale);
    std::cout << "theater fallback reason checks passed\n";
}
