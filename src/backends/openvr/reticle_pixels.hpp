#pragma once
#include <cstdint>
#include <cstdlib>
namespace cojvr::backends::openvr {
// BGRA8 warning pixel relative to the projected aim point. Zero preserves the
// captured world; fixed bounds match the existing cross outline footprint.
inline std::uint32_t NoShootReticlePixel(int x, int y) noexcept {
    if(x< -8||x>8||y< -8||y>8)return 0;
    const int ax=std::abs(x),ay=std::abs(y);
    if(ax<=6&&ay<=6&&ax==ay)return 0xFFFF4038U;
    return std::abs(ax-ay)<=1?0xFF000000U:0;
}
}
