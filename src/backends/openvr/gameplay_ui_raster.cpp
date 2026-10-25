#include "backends/openvr/gameplay_ui_raster.hpp"
#include "backends/openvr/reload_cartridge_texture.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace cojvr::backends::openvr {
namespace {
constexpr int kExtent=512, kCenter=256;
constexpr float kPi=3.14159265358979323846F;
struct Canvas {
    HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=nullptr;
    HGDIOBJ old_bitmap=nullptr;
    HFONT font=nullptr;
    HGDIOBJ old_font=nullptr;
    std::uint32_t* pixels=nullptr;
    Canvas() {
        if(!dc) return;
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=kExtent; info.bmiHeader.biHeight=-kExtent;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!bitmap || !pixels) return;
        old_bitmap=SelectObject(dc,bitmap);
        std::fill_n(pixels,kExtent*kExtent,0U);
        font=CreateFontW(-26,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(font) old_font=SelectObject(dc,font);
        SetBkMode(dc,TRANSPARENT);
    }
    ~Canvas(){
        if(old_font) SelectObject(dc,old_font);
        if(old_bitmap) SelectObject(dc,old_bitmap);
        if(font) DeleteObject(font);
        if(bitmap) DeleteObject(bitmap);
        if(dc) DeleteDC(dc);
    }
    void Circle(int x,int y,int radius,COLORREF color) {
        HBRUSH brush=CreateSolidBrush(color); if(!brush) return;
        auto old=SelectObject(dc,brush); auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
        Ellipse(dc,x-radius,y-radius,x+radius,y+radius);
        SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(brush);
    }
    void Segment(int x0,int y0,int x1,int y1,int width,COLORREF color) {
        HPEN pen=CreatePen(PS_SOLID,width,color);if(!pen)return;
        auto old=SelectObject(dc,pen);MoveToEx(dc,x0,y0,nullptr);LineTo(dc,x1,y1);
        SelectObject(dc,old);DeleteObject(pen);
    }
    void Triangle(POINT a,POINT b,POINT c,COLORREF color) {
        HBRUSH brush=CreateSolidBrush(color);if(!brush)return;
        auto old=SelectObject(dc,brush);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
        const POINT points[]{a,b,c};Polygon(dc,points,3);
        SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(brush);
    }
    void Text(std::u16string_view text,int x,int y,COLORREF color) {
        if(text.empty())return;
        const std::wstring value(text.begin(),text.end());
        SetTextColor(dc,color); RECT r{x-68,y-34,x+68,y+34};
        DrawTextW(dc,value.data(),static_cast<int>(value.size()),&r,
            DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_END_ELLIPSIS);
    }
    HudTextPanel Finish() {
        HudTextPanel panel{};if(!pixels)return panel;
        GdiFlush();panel.width=panel.height=kExtent;panel.pixels.assign(pixels,pixels+kExtent*kExtent);
        for(int y=0;y<kExtent;++y) for(int x=0;x<kExtent;++x){
            auto& p=panel.pixels[y*kExtent+x];const int dx=x-kCenter,dy=y-kCenter;
            if(dx*dx+dy*dy<246*246)p|=0xFF000000U;else p=0;
        }
        return panel;
    }
};
bool Direction(const runtime::Vec2 p) {
    const float norm=p.x*p.x+p.y*p.y;
    return std::isfinite(norm)&&norm>.9F&&norm<1.1F;
}
// Wheel-only art: keep the shared Canvas font and other HUD surfaces unchanged.
void WheelLeather(Canvas& c,const runtime::EquipmentWheelSnapshot& state) {
    const float angle=state.selected*kPi/4;
    const float s=std::sin(angle),co=std::cos(angle);
    for(int y=0;y<kExtent;++y)for(int x=0;x<kExtent;++x){
        const int dx=x-kCenter,dy=y-kCenter,r2=dx*dx+dy*dy;
        if(r2>=246*246)continue;
        // Stable fine grain and faint tooling bands; no animated noise or assets.
        const auto hash=static_cast<unsigned>(x)*374761393U^
            static_cast<unsigned>(y)*668265263U;
        const int grain=static_cast<int>((hash^(hash>>13))&7)-3;
        const int tooling=(x+(y/7)%3)%31==0?-3:0;
        const int shade=5-r2/(95*95)+grain+tooling;
        int r=44+shade,g=30+shade,b=21+shade;
        const float forward=dx*s-dy*co,side=dx*co+dy*s;
        const bool selected=state.selected>=0&&forward>0&&
            std::fabs(side)<forward*.41421356237F&&r2>88*88&&r2<222*222;
        if(selected){
            const bool allowed=(state.available_mask&(1U<<state.selected))!=0;
            r+=allowed?34:14;g+=allowed?21:4;b+=allowed?7:3;
            // A narrow outer arc reads as selection without flooding the dial.
            if(r2>215*215&&r2<220*220){r=allowed?197:109;g=allowed?143:79;b=allowed?65:57;}
        }
        const int light=-(dx+dy)/22;
        if(r2>=244*244){r=48;g=34;b=22;}
        else if(r2>=238*238){r=157+light;g=119+light;b=65+light;}
        else if(r2>=236*236){r=59;g=40;b=25;}
        else if(r2>=233*233){r=107+light;g=78+light;b=42+light;}
        else if(r2>=230*230){r=29;g=20;b=15;}
        c.pixels[y*kExtent+x]=(static_cast<unsigned>(r)<<16)|
            (static_cast<unsigned>(g)<<8)|static_cast<unsigned>(b);
    }
}
POINT WheelPoint(float angle,int radius) {
    return {kCenter+static_cast<LONG>(radius*std::sin(angle)),
        kCenter-static_cast<LONG>(radius*std::cos(angle))};
}
void WheelLabel(Canvas& c,std::u16string_view text,int x,int y,COLORREF color) {
    if(text.empty())return;
    const std::wstring value(text.begin(),text.end());
    constexpr UINT flags=DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_END_ELLIPSIS;
    RECT measure{0,0,136,68};
    DrawTextW(c.dc,value.data(),static_cast<int>(value.size()),&measure,flags|DT_CALCRECT);
    const int height=std::min(68L,measure.bottom-measure.top);
    RECT box{x-68,y-height/2,x+68,y-height/2+height};
    SetTextColor(c.dc,color);
    DrawTextW(c.dc,value.data(),static_cast<int>(value.size()),&box,flags);
}
void WheelTooling(Canvas& c) {
    // Saddle stitching, engraved sector grooves and small brass rivets.
    for(int i=0;i<80;++i){
        const float angle=i*kPi/40;
        const auto a=WheelPoint(angle-.007F,225),b=WheelPoint(angle+.007F,225);
        c.Segment(a.x,a.y,b.x,b.y,1,RGB(124,95,59));
    }
    for(int i=0;i<8;++i){
        const float angle=(i+.5F)*kPi/4;
        const auto a=WheelPoint(angle,91),b=WheelPoint(angle,219);
        c.Segment(a.x+1,a.y+1,b.x+1,b.y+1,3,RGB(24,17,13));
        c.Segment(a.x,a.y,b.x,b.y,1,RGB(116,87,51));
        const auto rivet=WheelPoint(angle,236);
        c.Circle(rivet.x,rivet.y,4,RGB(51,35,21));
        c.Circle(rivet.x,rivet.y,2,RGB(193,151,88));
    }
    // A tooled rosette frames the quiet center, away from the action text.
    for(int i=0;i<16;++i){
        const float angle=i*kPi/8;
        c.Triangle(WheelPoint(angle-.055F,79),WheelPoint(angle,88),
            WheelPoint(angle+.055F,79),RGB(108,78,43));
    }
    c.Circle(kCenter,kCenter,80,RGB(28,19,14));
    c.Circle(kCenter,kCenter,77,RGB(151,112,61));
    c.Circle(kCenter,kCenter,75,RGB(69,47,29));
    c.Circle(kCenter,kCenter,71,RGB(33,24,18));
    for(int sign:{-1,1}){
        const int y=kCenter+sign*48;
        c.Segment(kCenter-26,y,kCenter-8,y,1,RGB(144,106,60));
        c.Segment(kCenter+8,y,kCenter+26,y,1,RGB(144,106,60));
        c.Triangle({kCenter-4,y},{kCenter,y-4},{kCenter+4,y},RGB(178,135,75));
        c.Triangle({kCenter-4,y},{kCenter,y+4},{kCenter+4,y},RGB(120,86,46));
    }
}
}
const HudTextPanel& GameplayUiRaster::ControlHint(const runtime::ActionPromptSnapshot& prompts,
    runtime::PromptAction action,std::uint64_t generation,std::uint64_t now,
    bool native_text_visible,bool spanish) noexcept {
    if(native_text_visible){
        if(!control_hint_pixels_.pixels.empty())++control_hint_revision_;
        control_hint_pixels_={};control_hint_label_.clear();control_hint_glyph_={};return control_hint_pixels_;
    }
    const auto glyph=runtime::ResolveActionPromptGlyph(prompts,action,generation,now);
    const bool current=prompts.active&&generation&&prompts.binding_generation==generation;
    const runtime::ActionPromptSnapshot fallback{};
    const auto label=runtime::ResolveActionPrompt(current?prompts:fallback,action,spanish);
    if(label.empty()){
        if(!control_hint_pixels_.pixels.empty())++control_hint_revision_;
        control_hint_pixels_={};control_hint_label_.clear();control_hint_glyph_={};return control_hint_pixels_;
    }
    if(label==control_hint_label_&&glyph==control_hint_glyph_&&!control_hint_pixels_.pixels.empty())return control_hint_pixels_;
    control_hint_pixels_={};++control_hint_revision_;
    try{
        Canvas c;if(!c.pixels)return control_hint_pixels_;
        const bool art=glyph.kind!=runtime::PromptGlyphKind::none;
        const COLORREF ink=RGB(245,235,205);
        if(art){
            constexpr int x=28,y=32,r=14;
            using K=runtime::PromptGlyphKind;
            // Small procedural shapes only; no controller image/asset/license.
            if(glyph.kind==K::circle){c.Circle(x,y,r,ink);c.Circle(x,y,r-3,RGB(1,1,1));}
            else if(glyph.kind==K::cross){c.Segment(x-r,y-r,x+r,y+r,3,ink);c.Segment(x-r,y+r,x+r,y-r,3,ink);}
            else if(glyph.kind==K::square){
                c.Segment(x-r,y-r,x+r,y-r,3,ink);c.Segment(x+r,y-r,x+r,y+r,3,ink);
                c.Segment(x+r,y+r,x-r,y+r,3,ink);c.Segment(x-r,y+r,x-r,y-r,3,ink);
            }else if(glyph.kind==K::triangle){
                c.Segment(x,y-r,x+r,y+r,3,ink);c.Segment(x+r,y+r,x-r,y+r,3,ink);c.Segment(x-r,y+r,x,y-r,3,ink);
            }else if(glyph.kind==K::trigger){
                c.Segment(x-r,y-r,x+r,y-r,3,ink);c.Segment(x+r,y-r,x+9,y+7,3,ink);
                c.Segment(x+9,y+7,x,y+r,3,ink);c.Segment(x,y+r,x-r,y-r,3,ink);
            }else if(glyph.kind==K::grip){
                for(int dy:{-10,0,10})c.Segment(x-r,y+dy,x+r,y+dy,4,ink);
            }
            const wchar_t hand=glyph.hand==runtime::PromptGlyphHand::left?L'L':L'R';
            RECT hand_box{48,0,72,64};SetTextColor(c.dc,ink);
            DrawTextW(c.dc,&hand,1,&hand_box,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }
        // Art never substitutes for the binding's localized name or the semantic
        // fallback. Native dialogue/hint text never enters this optional panel.
        const std::wstring text(label.begin(),label.end());RECT box{art?80:16,0,504,64};
        SetTextColor(c.dc,ink);
        DrawTextW(c.dc,text.data(),static_cast<int>(text.size()),&box,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
        GdiFlush();control_hint_pixels_.width=512;control_hint_pixels_.height=64;
        control_hint_pixels_.pixels.assign(c.pixels,c.pixels+512*64);
        for(auto& pixel:control_hint_pixels_.pixels)if(pixel)pixel|=0xFF000000U;
        control_hint_label_=label;control_hint_glyph_=glyph;
    }catch(...){control_hint_pixels_={};control_hint_label_.clear();control_hint_glyph_={};}
    return control_hint_pixels_;
}
const HudTextPanel& GameplayUiRaster::DecorateControlHint(const HudTextPanel& instruction,
    std::uint64_t revision,const runtime::ActionPromptSnapshot& prompts,runtime::PromptAction action,
    std::uint64_t generation,std::uint64_t now,bool admitted) noexcept {
    const auto glyph=admitted?runtime::ResolveActionPromptGlyph(prompts,action,generation,now):runtime::PromptGlyph{};
    const bool art=glyph.kind!=runtime::PromptGlyphKind::none;
    if(revision==control_decoration_source_revision_&&glyph==control_decoration_glyph_&&
        (!art||!control_decoration_pixels_.pixels.empty()))return art?control_decoration_pixels_:instruction;
    control_decoration_source_revision_=revision;control_decoration_glyph_=glyph;
    control_decoration_pixels_={};++control_decoration_revision_;
    if(!art||!instruction.width||instruction.width>1968||!instruction.height||instruction.height>512||
        instruction.pixels.size()!=static_cast<std::size_t>(instruction.width)*instruction.height)return instruction;
    try{
        const auto& tile=ControlHint(prompts,action,generation,now,false);
        if(tile.width!=512||tile.height!=64||tile.pixels.size()!=512*64)return instruction;
        auto& panel=control_decoration_pixels_;
        panel.width=instruction.width+80;panel.height=std::max(64U,instruction.height);
        panel.pixels.assign(static_cast<std::size_t>(panel.width)*panel.height,0xFF151A20U);
        for(unsigned y=0;y<instruction.height;++y)
            std::copy_n(instruction.pixels.data()+y*instruction.width,instruction.width,
                panel.pixels.data()+y*panel.width+80);
        const unsigned top=(panel.height-64)/2;
        // Only the procedural icon/hand margin, never its standalone button-name
        // text. The full coaching sentence already owns the localized binding.
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<72;++x){
            const auto pixel=tile.pixels[y*512+x];
            if(pixel>>24)panel.pixels[(y+top)*panel.width+x]=pixel;
        }
        return panel;
    }catch(...){control_decoration_pixels_={};return instruction;}
}
const HudTextPanel& GameplayUiRaster::Threat(bool damage) noexcept {
    auto& panel=threat_pixels_[damage?1:0];
    if(!panel.pixels.empty())return panel;
    try {
        panel.width=panel.height=64;panel.pixels.assign(64*64,0);
        // Immutable presentation glyph, separate from the native source's
        // texture. Colour identifies the two qualified native owners.
        const std::uint32_t colour=damage?0xFFFF4848U:0xFF40EA70U;
        for(int y=8;y<=40;++y)for(int x=0;x<64;++x)
            if(std::fabs(std::abs(x-32)-.9F*(y-8))<3.5F)panel.pixels[y*64+x]=colour;
    } catch (...) {panel={};}
    return panel;
}
const HudTextPanel& GameplayUiRaster::InteractionGaze() noexcept {
    if (!interaction_gaze_pixels_.pixels.empty()) return interaction_gaze_pixels_;
    try {
        HudTextPanel panel{}; panel.width = panel.height = 32;
        panel.pixels.assign(32 * 32, 0U);
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
            const float dx = x - 15.5F, dy = y - 15.5F;
            const float radius_squared = dx*dx + dy*dy;
            // Cyan annulus with a dark outline; transparent hole and outside.
            if (radius_squared >= 7.5F*7.5F && radius_squared <= 11.5F*11.5F)
                panel.pixels[y*32+x] = radius_squared >= 8.5F*8.5F &&
                    radius_squared <= 10.5F*10.5F ? 0xFF30E6F2U : 0xFF08161AU;
        }
        interaction_gaze_pixels_ = std::move(panel);
    } catch (...) { interaction_gaze_pixels_ = {}; }
    return interaction_gaze_pixels_;
}
const HudTextPanel& GameplayUiRaster::TexturedCartridge() noexcept {
    if(!textured_cartridge_pixels_.pixels.empty())return textured_cartridge_pixels_;
    try {
        textured_cartridge_pixels_.width=reload_atlas::width;
        textured_cartridge_pixels_.height=reload_atlas::height;
        textured_cartridge_pixels_.pixels.assign(reload_atlas::pixels.begin(),reload_atlas::pixels.end());
    }catch(...){textured_cartridge_pixels_={};}
    return textured_cartridge_pixels_;
}
const HudTextPanel& GameplayUiRaster::Cartridge() noexcept {
    if(!cartridge_pixels_.pixels.empty())return cartridge_pixels_;
    try {
        HudTextPanel panel{};panel.width=48;panel.height=140;
        panel.pixels.assign(panel.width*panel.height,0U);
        // Procedural brass/copper shading, plus a separate rear-primer swatch.
        // Geometry uses an authored low-poly CC0 .44 Magnum silhouette.
        for(int y=6;y<134;++y)for(int x=0;x<48;++x){
            const bool tip=y<42;
            const int half=tip?std::min(15,3+(y-6)/2):(y>=124?20:17);
            const int dx=std::abs(x-24);if(dx>half)continue;
            const int light=std::max(0,24-dx*2);
            const bool edge=dx>=half-1||y==123||y==133;
            const unsigned r=edge?104U:static_cast<unsigned>((tip?171:184)+light);
            const unsigned g=edge?70U:static_cast<unsigned>((tip?86:139)+light);
            const unsigned b=edge?31U:static_cast<unsigned>((tip?49:55)+light);
            panel.pixels[y*48+x]=0xFF000000U|(r<<16)|(g<<8)|b;
        }
        // Primer is sampled by the final two cartridge quads, not a texture
        // obtained from the original game or from the downloadable model.
        for(int y=104;y<116;++y)for(int x=19;x<30;++x)
            panel.pixels[y*48+x]=0xFFB3A28BU;
        cartridge_pixels_=std::move(panel);
    }catch(...){cartridge_pixels_={};}
    return cartridge_pixels_;
}
const HudTextPanel& GameplayUiRaster::Wheel(const runtime::EquipmentWheelSnapshot& state) noexcept {
    if(state==wheel_ && (!wheel_pixels_.pixels.empty() || !state.active))return wheel_pixels_;
    wheel_=state;wheel_pixels_={};++wheel_revision_;
    if(!state.valid||!state.active||state.selected< -1||state.selected>=8)return wheel_pixels_;
    try {
        Canvas c;if(!c.pixels)return wheel_pixels_;
        WheelLeather(c,state);
        WheelTooling(c);
        for(int i=0;i<8;++i){
            const float angle=i*kPi/4;
            const int x=kCenter+static_cast<int>(165*std::sin(angle));
            const int y=kCenter-static_cast<int>(165*std::cos(angle));
            const bool allowed=(state.available_mask&(1U<<i))!=0;
            if(state.selected==i){
                c.Triangle(WheelPoint(angle-.06F,94),WheelPoint(angle,104),
                    WheelPoint(angle+.06F,94),allowed?RGB(226,179,100):RGB(139,105,79));
            }
            // Disabled text stays legible; a separate barred stamp avoids relying
            // on color alone. Neither decoration changes native eligibility.
            if(!allowed){
                c.Circle(x,y+43,7,RGB(139,116,86));
                c.Circle(x,y+43,5,RGB(43,31,23));
                c.Segment(x-4,y+47,x+4,y+39,2,RGB(168,143,108));
            }
            WheelLabel(c,state.labels[i].view(),x,y,
                allowed?RGB(245,229,197):RGB(169,151,125));
        }
        WheelLabel(c,state.selected<0?state.cancel_label.view():state.confirm_label.view(),
            kCenter,kCenter,RGB(245,229,197));
        wheel_pixels_=c.Finish();
    }catch(...){wheel_pixels_={};}
    return wheel_pixels_;
}
const HudTextPanel& GameplayUiRaster::Compass(const runtime::WristCompassSnapshot& state,
    const std::uint64_t monotonic_ms) noexcept {
    bool valid=state.active&&Direction(state.north_direction)&&state.marker_count<=state.markers.size();
    if(valid)for(std::size_t i=0;i<state.marker_count;++i)valid=Direction(state.markers[i].direction)&&valid;
    // Tracking updates geometry each frame; pixel content is bounded to 10 Hz.
    // Invalid or hidden owners clear immediately, irrespective of the budget.
    if(valid&&monotonic_ms&&compass_update_ms_&&monotonic_ms>=compass_update_ms_&&
        monotonic_ms-compass_update_ms_<100&&!compass_pixels_.pixels.empty())return compass_pixels_;
    if(state==compass_ && (!compass_pixels_.pixels.empty()||!state.active))return compass_pixels_;
    compass_=state;compass_pixels_={};++compass_revision_;
    compass_update_ms_=valid?monotonic_ms:0;
    if(!valid)return compass_pixels_;
    try{
        Canvas c;if(!c.pixels)return compass_pixels_;
        c.Circle(kCenter,kCenter,246,RGB(48,40,31));
        c.Circle(kCenter,kCenter,239,RGB(151,119,67));
        c.Circle(kCenter,kCenter,234,RGB(63,52,36));
        c.Circle(kCenter,kCenter,222,RGB(179,150,98));
        c.Circle(kCenter,kCenter,218,RGB(21,26,32));
        // Quiet wrist-fixed graduations frame the native bearing and objectives.
        for(int i=0;i<24;++i){
            const float angle=i*kPi/12;
            const bool major=i%3==0;
            const int inner=major?198:205;
            c.Segment(kCenter+static_cast<int>(inner*std::sin(angle)),
                kCenter-static_cast<int>(inner*std::cos(angle)),
                kCenter+static_cast<int>(212*std::sin(angle)),
                kCenter-static_cast<int>(212*std::cos(angle)),major?3:2,
                major?RGB(177,157,117):RGB(88,86,75));
        }
        const auto n=state.north_direction;
        const int nx=kCenter+static_cast<int>(165*n.x),ny=kCenter-static_cast<int>(165*n.y);
        const POINT side_a{kCenter+static_cast<int>(11*n.y),kCenter+static_cast<int>(11*n.x)};
        const POINT side_b{kCenter-static_cast<int>(11*n.y),kCenter-static_cast<int>(11*n.x)};
        c.Triangle({kCenter-static_cast<int>(72*n.x),kCenter+static_cast<int>(72*n.y)},
            side_b,side_a,RGB(189,183,165));
        c.Triangle({kCenter+static_cast<int>(133*n.x),kCenter-static_cast<int>(133*n.y)},
            side_a,side_b,RGB(200,70,55));
        c.Text(u"N",nx,ny,RGB(250,235,205));
        for(std::size_t i=0;i<state.marker_count;++i){
            const auto d=state.markers[i].direction;
            const int x=kCenter+static_cast<int>(190*d.x),y=kCenter-static_cast<int>(190*d.y);
            c.Circle(x,y,18,RGB(21,26,32));
            c.Circle(x,y,13,RGB(235,188,70));
            c.Circle(x,y,5,RGB(255,230,162));
        }
        c.Circle(kCenter,kCenter,12,RGB(151,119,67));
        c.Circle(kCenter,kCenter,6,RGB(245,235,205));compass_pixels_=c.Finish();
    }catch(...){compass_pixels_={};}
    return compass_pixels_;
}
const HudTextPanel& GameplayUiRaster::Status(const runtime::WristStatusSnapshot& state) noexcept {
    if(state==status_&&(!status_pixels_.pixels.empty()||!state.active))return status_pixels_;
    status_=state;status_pixels_={};++status_revision_;
    if(!state.active||!state.line_count||state.line_count>state.lines.size())return status_pixels_;
    for(std::uint32_t i=0;i<state.line_count;++i)if(state.lines[i].view().empty())return status_pixels_;
    try{
        Canvas c;if(!c.pixels)return status_pixels_;
        const int height=static_cast<int>(state.pixel_height());
        const auto brush=CreateSolidBrush(RGB(31,26,21));const auto pen=CreatePen(PS_SOLID,3,RGB(151,119,67));
        if(!brush||!pen){if(brush)DeleteObject(brush);if(pen)DeleteObject(pen);return status_pixels_;}
        const auto old_brush=SelectObject(c.dc,brush),old_pen=SelectObject(c.dc,pen);
        Rectangle(c.dc,8,8,kExtent-8,height-8);
        SelectObject(c.dc,old_brush);SelectObject(c.dc,old_pen);DeleteObject(brush);DeleteObject(pen);
        const auto font=CreateFontW(-30,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Georgia");
        const auto old_font=font?SelectObject(c.dc,font):nullptr;
        const bool health_meter=state.health_row>=0&&state.health_row<static_cast<int>(state.line_count)&&
            state.health_percent>=0&&state.health_percent<=100;
        std::array<bool,2> meters{};
        for(unsigned i=0;i<meters.size();++i){
            const auto& m=state.meters[i];
            meters[i]=m.row>=0&&m.row<static_cast<int>(state.line_count)&&m.percent>=0&&m.percent<=100&&
                (m.span==runtime::WristStatusMeterSpan::left||m.span==runtime::WristStatusMeterSpan::right)&&
                !(health_meter&&m.row==state.health_row);
        }
        if(meters[0]&&meters[1]&&state.meters[0].row==state.meters[1].row&&
           state.meters[0].span==state.meters[1].span)meters={}; // ambiguous coverage: keep text
        const auto& badge=state.badge;
        const bool badge_valid=badge.row>=0&&badge.row<static_cast<int>(state.line_count)&&
            (badge.kind==runtime::WristStatusBadgeKind::ready||badge.kind==runtime::WristStatusBadgeKind::active||
             badge.kind==runtime::WristStatusBadgeKind::cooldown||badge.kind==runtime::WristStatusBadgeKind::unavailable);
        for(std::uint32_t i=0;i<state.line_count;++i){
            // Display thresholds only; never infer damage/death or trigger input.
            const bool health=health_meter&&static_cast<int>(i)==state.health_row;
            const COLORREF color=health&&state.health_percent<=25?RGB(255,151,132):
                health&&state.health_percent<=50?RGB(255,215,140):RGB(245,235,205);
            SetTextColor(c.dc,color);
            const auto text=state.lines[i].view();const std::wstring value(text.begin(),text.end());
            const bool row_badge=badge_valid&&badge.row==static_cast<int>(i);
            RECT box{24,static_cast<LONG>(16+i*state.row_height),row_badge?444:kExtent-24,static_cast<LONG>(16+(i+1)*state.row_height)};
            DrawTextW(c.dc,value.data(),static_cast<int>(value.size()),&box,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        if(old_font)SelectObject(c.dc,old_font);if(font)DeleteObject(font);
        if(badge_valid){
            constexpr int x=466;const int y=40+badge.row*static_cast<int>(state.row_height);
            const auto kind=badge.kind;
            const COLORREF color=kind==runtime::WristStatusBadgeKind::ready?RGB(143,184,131):
                kind==runtime::WristStatusBadgeKind::active?RGB(90,200,239):
                kind==runtime::WristStatusBadgeKind::cooldown?RGB(232,186,98):RGB(125,118,106);
            c.Circle(x,y,13,color);c.Circle(x,y,10,RGB(31,26,21));
            if(kind==runtime::WristStatusBadgeKind::ready){
                c.Segment(x-6,y,x-2,y+4,3,color);c.Segment(x-2,y+4,x+6,y-4,3,color);
            }else if(kind==runtime::WristStatusBadgeKind::active){
                c.Segment(x+2,y-7,x-4,y+1,3,color);c.Segment(x-4,y+1,x+4,y-1,3,color);
                c.Segment(x+4,y-1,x-2,y+7,3,color);
            }else if(kind==runtime::WristStatusBadgeKind::cooldown){
                c.Segment(x,y-7,x,y,2,color);c.Segment(x,y,x+5,y+3,2,color);
            }else c.Segment(x-6,y+6,x+6,y-6,2,color);
        }
        GdiFlush();
        for(unsigned i=0;i<meters.size();++i)if(meters[i]){
            const auto& m=state.meters[i];
            constexpr int width=224;
            const int left=m.span==runtime::WristStatusMeterSpan::left?24:264;
            const int bottom=16+(m.row+1)*static_cast<int>(state.row_height)-4;
            const int fill=width*m.percent/100;
            const int reserve=m.warning_high?100-m.percent:m.percent;
            const std::uint32_t color=reserve<=25?0x00E87760U:reserve<=50?0x00E8BA62U:0x008FB883U;
            for(int y=bottom-4;y<bottom;++y)for(int x=0;x<width;++x)
                c.pixels[y*kExtent+left+x]=x<fill?color:0x00443E36U;
        }
        if(health_meter){
            constexpr int left=24,width=kExtent-48;
            const int bottom=16+(state.health_row+1)*static_cast<int>(state.row_height)-4;
            const int fill=width*state.health_percent/100;
            const std::uint32_t color=state.health_percent<=25?0x00E87760U:
                state.health_percent<=50?0x00E8BA62U:0x008FB883U;
            for(int y=bottom-4;y<bottom;++y)for(int x=0;x<width;++x)
                c.pixels[y*kExtent+left+x]=x<fill?color:0x00443E36U;
        }
        status_pixels_.width=kExtent;status_pixels_.height=height;
        status_pixels_.pixels.assign(c.pixels,c.pixels+kExtent*height);
        for(int y=0;y<height;++y)for(int x=0;x<kExtent;++x){
            auto& pixel=status_pixels_.pixels[y*kExtent+x];
            if(x>=8&&x<kExtent-8&&y>=8&&y<height-8)pixel|=0xFF000000U;else pixel=0;
        }
    }catch(...){status_pixels_={};}
    return status_pixels_;
}
}
