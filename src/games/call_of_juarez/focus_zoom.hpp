#pragma once
#include "runtime/vr_types.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJFocusZoom {
    float factor = 0;
    float maximum = 1;
    float magnification = 1;
    bool valid = false;
};
// Exact-build game-owner caller only. Reads cached native squint fields;
// GetBeingZoom may run CalculateZoom, so it is deliberately never invoked.
// Desired/current bows and the shipped scoped rifle retain physical optics;
// their native factor/scope overrides are separate mechanics.
bool ReadCoJFocusZoom(void* env, void* player, CoJFocusZoom& out) noexcept;
// Render copies only: leave the runtime optics, eye poses and UI optics intact.
bool BuildCoJFocusRenderEyes(const std::array<runtime::EyeView, 2>& optical,
    const CoJFocusZoom& zoom, std::array<runtime::EyeView, 2>& render) noexcept;
} // namespace cojvr::games::call_of_juarez
