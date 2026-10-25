#pragma once
#include "runtime/scope_view.hpp"
#include <cstdint>
#include <cstdlib>
#include <cmath>
namespace cojvr::backends::openvr {
template<class Overlay>
inline bool IsCurrentCenteredScopeReticle(const Overlay& overlay,std::uint64_t capture_sequence) noexcept {
    if(!capture_sequence||overlay.frame_sequence!=capture_sequence||!overlay.scope_view)return false;
    // Adapter and transport hand order is explicitly right, left.
    const auto& right=overlay.weapons[0];
    if(!right.active)return false;
    for(unsigned eye=0;eye<2;++eye) {
        const auto& point=right.eyes[eye];
        const auto& lens=overlay.scope_lenses[eye];
        if(!runtime::ValidScopeLens(lens))return false;
        if(!point.valid||!std::isfinite(point.u)||!std::isfinite(point.v)||
            std::abs(point.u-lens.center_u)>.0001F||std::abs(point.v-lens.center_v)>.0001F)return false;
    }
    return true;
}
// BGRA8 warning pixel relative to the projected aim point. Zero preserves the
// captured world; fixed bounds match the existing cross outline footprint.
inline std::uint32_t NoShootReticlePixel(int x, int y) noexcept {
    if(x< -8||x>8||y< -8||y>8)return 0;
    const int ax=std::abs(x),ay=std::abs(y);
    if(ax<=6&&ay<=6&&ax==ay)return 0xFFFF4038U;
    return std::abs(ax-ay)<=1?0xFF000000U:0;
}
}
