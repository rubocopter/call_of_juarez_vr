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
inline const std::array<HudTextPanel,3>& SupportGripGuidePanels() {
    static const auto panels=[] {
        auto out=HolsterGuidePanels();
        constexpr std::array<std::uint32_t,3> colors{0x68DAE8U,0x70DDA0U,0x9A8274U};
        constexpr unsigned glyphs[2][7]{{16,16,16,16,16,16,31},{4,12,4,4,4,4,14}};
        for(unsigned state=0;state<3;++state){
            for(auto& pixel:out[state].pixels)pixel=(pixel&0xff000000U)|colors[state];
            for(unsigned letter=0;letter<2;++letter)for(unsigned row=0;row<7;++row)
                for(unsigned col=0;col<5;++col)if(glyphs[letter][row]&(1U<<(4-col)))
                    for(unsigned y=0;y<3;++y)for(unsigned x=0;x<3;++x)
                        out[state].pixels[(21+row*3+y)*64+16+letter*18+col*3+x]=0xe0000000U|colors[state];
        }
        return out;
    }();
    return panels;
}
inline const std::array<HudTextPanel,3>& AmmunitionPickupGuidePanels() {
    static const auto panels=[] {
        auto out=HolsterGuidePanels();
        constexpr std::array<std::uint32_t,3> colors{0xF0C76AU,0x70DDA0U,0x9A8274U};
        for(unsigned state=0;state<3;++state){
            for(auto& pixel:out[state].pixels)pixel=(pixel&0xff000000U)|colors[state];
            for(int y=10;y<51;++y)for(int x=20;x<44;++x){
                const int half=y<25?3+(y-10)/2:9;
                const bool body=std::abs(x-32)<=half;
                const bool rim=y>=46&&y<50&&x>=21&&x<=43;
                if(body||rim)out[state].pixels[y*64+x]=0xe0000000U|colors[state];
            }
        }
        return out;
    }();
    return panels;
}
}
