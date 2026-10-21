#pragma once
#include "backends/openvr/hud_text_overlay.hpp"
#include <array>
#include <cmath>

namespace cojvr::backends::openvr {
// Small immutable translucent hint textures; no game assets or scene depth.
inline const std::array<HudTextPanel,3>& HolsterGuidePanels() {
    static const auto panels=[] {
        std::array<HudTextPanel,3> out{};
        constexpr std::array<std::uint32_t,3> colors{0xD8AA60U,0x70DDA0U,0x9A8274U};
        for(unsigned state=0;state<out.size();++state){auto& p=out[state];
            p.width=p.height=64;p.pixels.resize(64*64);
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
                const float dx=(static_cast<float>(x)+.5F-32)/32,dy=(static_cast<float>(y)+.5F-32)/32;
                const float r=std::hypot(dx,dy);unsigned alpha=0;
                if(r<.77F)alpha=state==1?22:10;
                else if(r<.93F)alpha=state==1?140:state==2?45:90;
                else if(r<1)alpha=static_cast<unsigned>((1-r)/.07F*(state==1?140:state==2?45:90));
                p.pixels[y*64+x]=(alpha<<24)|colors[state];
            }
        }
        return out;
    }();
    return panels;
}
}
