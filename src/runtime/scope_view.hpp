#pragma once
#include <cmath>

namespace cojvr::runtime {
// Top-left-origin eye UVs projected by the game adapter from a shared angular
// aperture through each physical eye FOV. No renderer or exact-game ownership.
struct ScopeLens {
    bool valid = false;
    float center_u = .5F, center_v = .5F;
    float radius_u = 0, radius_v = 0;
};
[[nodiscard]] inline bool ValidScopeLens(const ScopeLens& lens) noexcept {
    return lens.valid && std::isfinite(lens.center_u) && std::isfinite(lens.center_v) &&
        std::isfinite(lens.radius_u) && std::isfinite(lens.radius_v) &&
        lens.radius_u > 0 && lens.radius_v > 0 &&
        lens.center_u >= lens.radius_u && lens.center_v >= lens.radius_v &&
        lens.center_u + lens.radius_u <= 1 && lens.center_v + lens.radius_v <= 1;
}
}
