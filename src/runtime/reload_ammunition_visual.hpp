#pragma once

#include "runtime/textured_cartridge_mesh.hpp"

namespace cojvr::runtime {

enum class ReloadCartridgeAppearance { pistol, rifle, shotgun };

namespace reload_ammunition_visual_detail {
inline constexpr float pistol_nose_z = [] {
    float nose = textured_reload_mesh::centre_z;
    for (const auto vertex : textured_reload_mesh::vertices)
        if (vertex.z < nose) nose = vertex.z;
    return nose;
}();
inline constexpr float rifle_axial_scale = 1.65F;
inline constexpr float shotgun_half_length_m = .031F;
// Texel centre for the hull swatch in the procedural proxy atlas.
inline constexpr int shotgun_hull_texel_x = 36;
inline constexpr int shotgun_hull_texel_y = 80;
}

// Signed distance from the shared pinch centre along cartridge-local +Z.
// The front is negative. These offsets describe overlay geometry only.
[[nodiscard]] constexpr float ReloadCartridgeNoseOffsetM(ReloadCartridgeAppearance appearance) noexcept {
    using namespace reload_ammunition_visual_detail;
    constexpr float pistol_offset = pistol_nose_z - textured_reload_mesh::centre_z;
    switch (appearance) {
    case ReloadCartridgeAppearance::rifle: return pistol_offset * rifle_axial_scale;
    case ReloadCartridgeAppearance::shotgun: return -shotgun_half_length_m;
    default: return pistol_offset;
    }
}

}
