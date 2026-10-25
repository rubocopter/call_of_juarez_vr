#pragma once
#include "runtime/vr_types.hpp"

namespace cojvr::games::call_of_juarez {
struct CoJFocusZoom {
    float factor = 0;
    float maximum = 1;
    float magnification = 1;
    bool valid = false;
    bool scoped = false; // Stable exact WinchesterScope owner, including zoom ramp/decay.
    bool scope_visible = false; // Valid native full-squint/state predicate; mask may activate.
    std::uint32_t scoped_weapon = 0; // Native GetThisID bits, for input/muzzle owner correlation.
};
// Exact-build game-owner caller only. Reads cached native squint fields;
// GetBeingZoom may run CalculateZoom, so it is deliberately never invoked.
// Stable current/desired bow ownership additionally observes the native
// BulletTime caches and combines concentration with squint. No native update,
// parameter factory or weapon write. Stable exact Scoped Winchester ownership
// reads the existing module TreeMap parameter and default-camera FOV, reproducing
// native scope eligibility and normalization. Missing/changed cache identities
// invalidate the entire snapshot (including mask flags). Ineligible scope uses
// ordinary cached squint; full native eligibility sets scope_visible. Presentation
// owns the binocular scene aperture/reticle; no telescope render texture is read.
bool ReadCoJFocusZoom(void* env, void* player, CoJFocusZoom& out) noexcept;
// Render copies only: narrow tangents and divide the virtual scene baseline by
// magnification around its original centre, preserving displayed disparity.
// Runtime hardware IPD, tracking, native restore preimages and UI stay physical.
bool BuildCoJFocusRenderEyes(const std::array<runtime::EyeView, 2>& optical,
    const CoJFocusZoom& zoom, std::array<runtime::EyeView, 2>& render) noexcept;
} // namespace cojvr::games::call_of_juarez
