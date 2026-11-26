#pragma once
#include "backends/openvr/hud_text_overlay.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace cojvr::backends::openvr {
// Small immutable translucent hint textures; no game assets or scene depth.
inline const std::array<HudTextPanel,3>& GlyphGuideBackgroundPanels() {
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
inline const std::array<HudTextPanel,3>& HolsterGuidePanels() {
    static const auto panels=[] {
        std::array<HudTextPanel,3> out{};
        constexpr unsigned extent=128;
        constexpr float half=extent*.5F;
        constexpr std::array<std::uint32_t,3> colors{0xD8AA60U,0x70DDA0U,0x9A8274U};
        constexpr std::array<unsigned,3> opacity{100,155,50};
        for(unsigned state=0;state<out.size();++state){auto& p=out[state];
            p.width=p.height=extent;p.pixels.resize(extent*extent);
            for(unsigned y=0;y<extent;++y)for(unsigned x=0;x<extent;++x){
                const float r=std::hypot(x+.5F-half,y+.5F-half);
                // 1.5px stroke plus one-pixel edge coverage, entirely inside
                // the canonical interaction radius. No translucent centre fill.
                const float distance=std::abs(r-(half-1.25F))-.75F;
                const float coverage=std::fmax(0.F,std::fmin(1.F,.5F-distance));
                const unsigned alpha=static_cast<unsigned>(coverage*opacity[state]+.5F);
                p.pixels[y*extent+x]=alpha?(alpha<<24)|colors[state]:0;
            }
        }
        return out;
    }();
    return panels;
}

namespace guide_art {
struct Stroke {
    float x0, y0, x1, y1, radius;
};

inline void Paint(HudTextPanel& panel, std::span<const Stroke> strokes,
                  std::uint32_t color, unsigned opacity) {
    for (const auto& stroke : strokes) {
        const auto margin = stroke.radius + 1.F;
        const auto x0 = std::max(0, static_cast<int>(std::floor(std::min(stroke.x0, stroke.x1) - margin)));
        const auto y0 = std::max(0, static_cast<int>(std::floor(std::min(stroke.y0, stroke.y1) - margin)));
        const auto x1 = std::min(static_cast<int>(panel.width),
            static_cast<int>(std::ceil(std::max(stroke.x0, stroke.x1) + margin)));
        const auto y1 = std::min(static_cast<int>(panel.height),
            static_cast<int>(std::ceil(std::max(stroke.y0, stroke.y1) + margin)));
        const float dx = stroke.x1 - stroke.x0, dy = stroke.y1 - stroke.y0;
        const float length_squared = dx*dx + dy*dy;
        for (int y=y0; y<y1; ++y) for (int x=x0; x<x1; ++x) {
            const float px=x+.5F-stroke.x0, py=y+.5F-stroke.y0;
            const float t=std::clamp((px*dx+py*dy)/length_squared,0.F,1.F);
            const float distance=std::hypot(px-t*dx,py-t*dy);
            const float coverage=std::clamp(stroke.radius+.5F-distance,0.F,1.F);
            const auto alpha=static_cast<unsigned>(coverage*opacity+.5F);
            auto& pixel=panel.pixels[y*panel.width+x];
            if(alpha>(pixel>>24))pixel=(alpha<<24)|color;
        }
    }
}
} // namespace guide_art

inline const std::array<HudTextPanel,3>& EquipmentHolsterGuidePanels(runtime::GestureGuideKind kind) {
    using guide_art::Stroke;
    static constexpr Stroke pistol[]{{54,95,67,95,3.8F},{67,94,78,94,2.1F},
        {56,95,50,106,3.F},{58,99,65,101,1.1F},{65,101,68,97,1.1F}};
    static constexpr Stroke long_gun[]{{45,100,51,96,3.F},{51,96,67,96,2.8F},
        {67,95,84,95,1.5F},{63,98,61,103,1.1F},{61,103,56,102,1.1F}};
    static constexpr Stroke bow[]{{58,87,64,88,1.6F},{64,88,71,93,1.6F},{71,93,74,98,1.6F},
        {74,98,71,103,1.6F},{71,103,64,108,1.6F},{64,108,58,109,1.6F},
        {58,87,58,109,1.F},{51,98,72,98,.85F}};
    static const auto build=[](std::span<const Stroke> strokes) {
        auto out=HolsterGuidePanels();
        constexpr std::array<std::uint32_t,3> colors{0xD8AA60U,0x70DDA0U,0x9A8274U};
        constexpr std::array<unsigned,3> opacity{130,200,70};
        for(unsigned state=0;state<out.size();++state)
            guide_art::Paint(out[state],strokes,colors[state],opacity[state]);
        return out;
    };
    using Kind=runtime::GestureGuideKind;
    switch(kind){
    case Kind::holster_pistol: { static const auto panels=build(pistol); return panels; }
    case Kind::holster_long_gun: { static const auto panels=build(long_gun); return panels; }
    case Kind::holster_bow: { static const auto panels=build(bow); return panels; }
    default: return HolsterGuidePanels();
    }
}

inline const std::array<HudTextPanel,3>& SupportGripGuidePanels() {
    static const auto panels=[] {
        using guide_art::Stroke;
        std::array<HudTextPanel,3> out{};
        constexpr std::array<std::uint32_t,3> colors{0x68DAE8U,0x70DDA0U,0x9A8274U};
        constexpr std::array<unsigned,3> bracket_opacity{150,225,80};
        constexpr Stroke brackets[]{{17,18,11,18,1.2F},{11,18,11,46,1.2F},{11,46,17,46,1.2F},
            {47,18,53,18,1.2F},{53,18,53,46,1.2F},{53,46,47,46,1.2F}};
        constexpr Stroke label[]{{21,24,21,40,1.5F},{21,40,29,40,1.5F},
            {35,28,40,24,1.5F},{40,24,40,40,1.5F},{35,40,45,40,1.5F}};
        for(unsigned state=0;state<3;++state){
            auto& panel=out[state];
            panel.width=panel.height=64;
            panel.pixels.resize(64*64);
            guide_art::Paint(panel,brackets,colors[state],bracket_opacity[state]);
            guide_art::Paint(panel,label,colors[state],state==2?140:230);
        }
        return out;
    }();
    return panels;
}
inline const std::array<HudTextPanel,3>& AmmunitionPickupGuidePanels() {
    static const auto panels=[] {
        auto out=GlyphGuideBackgroundPanels();
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
inline const std::array<HudTextPanel,3>& LoadingMouthGuidePanels() {
    static const auto panels=[] {
        auto out=GlyphGuideBackgroundPanels();
        constexpr std::array<std::uint32_t,3> colors{0x6ECBE0U,0x70DDA0U,0xEAC67AU};
        for(unsigned i=0;i<out.size();++i)
            for(auto& pixel:out[i].pixels)pixel=(pixel&0xff000000U)|colors[i];
        auto& loaded=out[2].pixels;
        for(int y=8;y<56;++y)for(int x=8;x<56;++x){
            const int dx=2*x-63,dy=2*y-63,r2=dx*dx+dy*dy;
            if(r2<47*47)loaded[y*64+x]=r2<13*13?0xE08F622FU:0xE0EAC67AU;
        }
        return out;
    }();
    return panels;
}
inline const std::array<HudTextPanel,3>& SpentCaseGuidePanels() {
    static const auto panels=[] {
        std::array<HudTextPanel,3> out{};
        out[2]=LoadingMouthGuidePanels()[2];
        for(auto& pixel:out[2].pixels){
            const auto color=pixel&0xffffffU;
            const auto brass=color==0x8F622FU?0x382519U:
                color==0xEAC67AU?0x946435U:0x73502CU;
            pixel=(pixel&0xff000000U)|brass;
        }
        return out;
    }();
    return panels;
}
inline const std::array<HudTextPanel,3>& BowStringGuidePanels() {
    static const auto panels=[] {
        auto out=GlyphGuideBackgroundPanels();
        constexpr std::array<std::uint32_t,3> colors{0x68DAE8U,0x70DDA0U,0x9A8274U};
        for(unsigned state=0;state<out.size();++state){
            auto& p=out[state];
            for(auto& pixel:p.pixels)pixel=(pixel&0xff000000U)|colors[state];
            // Thin string and nock dot, distinct from ammo, L1 and holster hints.
            for(int y=16;y<48;++y)for(int x=30;x<34;++x)
                p.pixels[y*64+x]=0xb0000000U|colors[state];
            for(int y=29;y<35;++y)for(int x=29;x<35;++x)
                p.pixels[y*64+x]=0xe0000000U|colors[state];
        }
        return out;
    }();
    return panels;
}
}
