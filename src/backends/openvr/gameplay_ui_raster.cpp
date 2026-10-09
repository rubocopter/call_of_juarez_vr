#include "backends/openvr/gameplay_ui_raster.hpp"
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
        c.Circle(kCenter,kCenter,246,RGB(21,26,32));
        for(int i=0;i<8;++i){
            const float angle=i*kPi/4;
            const int x=kCenter+static_cast<int>(165*std::sin(angle));
            const int y=kCenter-static_cast<int>(165*std::cos(angle));
            const bool allowed=(state.available_mask&(1U<<i))!=0;
            if(state.selected==i)c.Circle(x,y,65,allowed?RGB(132,88,24):RGB(65,40,40));
            c.Text(state.labels[i].view(),x,y,allowed?RGB(245,235,205):RGB(100,105,110));
        }
        c.Circle(kCenter,kCenter,72,RGB(38,45,53));
        c.Text(state.selected<0?u"Cancelar":u"Confirmar",kCenter,kCenter,RGB(245,235,205));
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
        SetTextColor(c.dc,RGB(245,235,205));
        for(std::uint32_t i=0;i<state.line_count;++i){
            const auto text=state.lines[i].view();const std::wstring value(text.begin(),text.end());
            RECT box{24,static_cast<LONG>(16+i*state.row_height),kExtent-24,static_cast<LONG>(16+(i+1)*state.row_height)};
            DrawTextW(c.dc,value.data(),static_cast<int>(value.size()),&box,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        if(old_font)SelectObject(c.dc,old_font);if(font)DeleteObject(font);
        GdiFlush();status_pixels_.width=kExtent;status_pixels_.height=height;
        status_pixels_.pixels.assign(c.pixels,c.pixels+kExtent*height);
        for(int y=0;y<height;++y)for(int x=0;x<kExtent;++x){
            auto& pixel=status_pixels_.pixels[y*kExtent+x];
            if(x>=8&&x<kExtent-8&&y>=8&&y<height-8)pixel|=0xFF000000U;else pixel=0;
        }
    }catch(...){status_pixels_={};}
    return status_pixels_;
}
}
