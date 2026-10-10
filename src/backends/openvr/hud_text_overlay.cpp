#include "backends/openvr/hud_text_overlay.hpp"
#include "backends/openvr/gameplay_ui_raster.hpp"
#include "runtime/vr_math.hpp"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace cojvr::backends::openvr {
using Microsoft::WRL::ComPtr;

namespace {
struct GdiDc {
    HDC value = CreateCompatibleDC(nullptr);
    ~GdiDc() { if (value) DeleteDC(value); }
};
struct GdiObject {
    HGDIOBJ value = nullptr;
    ~GdiObject() { if (value) DeleteObject(value); }
};
struct GdiSelection {
    HDC dc;
    HGDIOBJ previous;
    ~GdiSelection() { if (previous) SelectObject(dc, previous); }
};
}

const HudTextPanel& HudTextRaster::Render(const runtime::HudText& text,bool compact) noexcept {
    if (text == text_ && compact==compact_ && (!panel_.pixels.empty() || text.view().empty())) return panel_;
    text_ = text;
    compact_ = compact;
    panel_ = {};
    ++rebuilds_;
    const auto value = text.view();
    if (value.empty()) return panel_;
    try {
        const std::wstring wide(value.begin(), value.end());
        GdiDc dc_owner;
        HDC dc = dc_owner.value;
        if (!dc) return panel_;
        GdiObject font_owner;
        GdiSelection selected_font{dc, nullptr};
        RECT measured{};
        const LONG width = compact?384:1024;
        constexpr LONG padding = 24, max_height = 350;
        const UINT alignment=compact?DT_CENTER:DT_LEFT;
        for (int size = compact?48:36; size >= 16; size -= 2) {
            HFONT font = CreateFontW(-size, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            if (!font) break;
            font_owner.value = font;
            selected_font.previous = SelectObject(dc, font);
            measured = {padding, padding, width - padding, padding};
            DrawTextW(dc, wide.data(), static_cast<int>(wide.size()), &measured,
                DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT | alignment);
            if (measured.bottom - measured.top <= max_height - padding * 2 || size == 16) break;
            SelectObject(dc, selected_font.previous);
            selected_font.previous = nullptr;
            DeleteObject(font);
            font_owner.value = nullptr;
        }
        if (!font_owner.value) return panel_;
        const LONG height = std::clamp(measured.bottom - measured.top + padding * 2,
            72L, max_height);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        GdiObject bitmap_owner{bitmap};
        if (bitmap && bits) {
            GdiSelection selected_bitmap{dc, SelectObject(dc, bitmap)};
            auto* pixels = static_cast<std::uint32_t*>(bits);
            std::fill_n(pixels, width * height, 0xFF151A20U);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(245, 245, 245));
            RECT rect{padding, padding, width - padding, height - padding};
            const auto split=compact?wide.find_last_of(L'\n'):std::wstring::npos;
            if(split==std::wstring::npos){
                DrawTextW(dc, wide.data(), static_cast<int>(wide.size()), &rect,
                    DT_WORDBREAK | DT_NOPREFIX | alignment);
            }else{
                // Reserve the native numeric suffix even when a localized
                // title exceeds the bounded panel's visible height.
                rect.bottom=height-72;
                DrawTextW(dc,wide.data(),static_cast<int>(split),&rect,
                    DT_WORDBREAK|DT_NOPREFIX|DT_CENTER|DT_END_ELLIPSIS);
                GdiObject number_font{CreateFontW(-48,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI")};
                if(number_font.value){
                    GdiSelection select{dc,SelectObject(dc,number_font.value)};
                    RECT number_rect{padding,height-72,width-padding,height-padding};
                    DrawTextW(dc,wide.data()+split+1,static_cast<int>(wide.size()-split-1),&number_rect,
                        DT_CENTER|DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER);
                }
            }
            GdiFlush();
            panel_.width = width;
            panel_.height = static_cast<std::uint32_t>(height);
            panel_.pixels.assign(pixels, pixels + width * height);
            for (auto& pixel : panel_.pixels) pixel |= 0xFF000000U;
        }
    } catch (...) { panel_ = {}; }
    return panel_;
}

bool BuildThreatPanelPlacement(const runtime::ThreatMarker& marker,HudPanelPlacement& out) noexcept {
    out={};const auto d=marker.direction;const float n=d.x*d.x+d.y*d.y;
    if(!std::isfinite(n)||std::abs(n-1)>.01F||!std::isfinite(marker.alpha)||marker.alpha<=0||marker.alpha>1)return false;
    out.captured_quad=true;
    for(unsigned i=0;i<4;++i){
        const float x=(i&1)?.065F:-.065F,y=(i&2)?-.065F:.065F;
        out.head_corners[i]={.4F*d.x+x*d.y+y*d.x,.4F*d.y-x*d.x+y*d.y,-1.5F};
    }
    return true;
}
bool ProjectHudPanel(const HudTextPanel& source, const runtime::EyeView& eye,
    const std::uint32_t target_width, const std::uint32_t target_height,
    const HudPanelPlacement placement, ProjectedHudPanel& result) noexcept {
    result = {};
    const auto& pose = eye.eye_to_head;
    const auto q = pose.orientation;
    const float norm = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (!source.width || !source.height || source.width > 2048 || source.height > 512 ||
        source.pixels.size() != static_cast<std::size_t>(source.width) * source.height ||
        !target_width || !target_height || target_width > 16384 || target_height > 16384 ||
        !pose.position_valid || !pose.orientation_valid || !std::isfinite(norm) ||
        norm < 0.9F || norm > 1.1F || !std::isfinite(pose.position.x) ||
        !std::isfinite(pose.position.y) || !std::isfinite(pose.position.z) ||
        !runtime::IsValidEyeFov(eye.fov) || !std::isfinite(placement.width_m) ||
        placement.width_m <= 0 || placement.width_m > 3 ||
        !std::isfinite(placement.center_y_m) || !std::isfinite(placement.distance_m) ||
        (!placement.captured_quad && (placement.distance_m < 0.5F || placement.distance_m > 5))) return false;
    const auto inverse = runtime::NormalizeQuaternion({-q.x, -q.y, -q.z, q.w});
    const float l = std::tan(eye.fov.angle_left), r = std::tan(eye.fov.angle_right);
    const float d = std::tan(eye.fov.angle_down), u = std::tan(eye.fov.angle_up);
    const float half_w = placement.width_m * 0.5F;
    const float half_h = half_w * static_cast<float>(source.height) / source.width;
    float min_x = 1, max_x = -1, min_y = 1, max_y = -1;
    for (std::size_t corner = 0; corner < 4; ++corner) {
        runtime::Vec3 p{(corner & 1) ? half_w : -half_w,
            placement.center_y_m + ((corner & 2) ? -half_h : half_h), -placement.distance_m};
        if (placement.captured_quad) p = placement.head_corners[corner];
        p = runtime::RotateVector(inverse, {p.x - pose.position.x,
            p.y - pose.position.y, p.z - pose.position.z});
        const float w = -p.z;
        if (!std::isfinite(w) || w <= 0.05F) return false;
        const float x = (2*p.x - (r+l)*w) / (r-l);
        const float y = (2*p.y - (u+d)*w) / (u-d);
        if (!std::isfinite(x) || !std::isfinite(y)) return false;
        result.clip_positions[corner] = {x, y, w*0.5F, w};
        min_x = std::min(min_x, x/w); max_x = std::max(max_x, x/w);
        min_y = std::min(min_y, y/w); max_y = std::max(max_y, y/w);
    }
    if (max_x <= -1 || min_x >= 1 || max_y <= -1 || min_y >= 1) { result = {}; return false; }
    const auto x0 = static_cast<std::uint32_t>(std::floor((std::clamp(min_x, -1.F, 1.F)+1)*0.5F*target_width));
    const auto x1 = static_cast<std::uint32_t>(std::ceil((std::clamp(max_x, -1.F, 1.F)+1)*0.5F*target_width));
    const auto y0 = static_cast<std::uint32_t>(std::floor((1-std::clamp(max_y, -1.F, 1.F))*0.5F*target_height));
    const auto y1 = static_cast<std::uint32_t>(std::ceil((1-std::clamp(min_y, -1.F, 1.F))*0.5F*target_height));
    result.left = std::min(x0, target_width); result.top = std::min(y0, target_height);
    result.width = std::min(x1, target_width) - result.left;
    result.height = std::min(y1, target_height) - result.top;
    return result.width && result.height;
}

bool FitCriticalHudPanels(const std::array<const HudTextPanel*,3>& panels,
    const std::array<runtime::EyeView,2>& eyes,std::array<HudPanelPlacement,3>& result) noexcept {
    result={};float scale=1;
    for(unsigned attempt=0;attempt<36;++attempt,scale*=.9F){
        float bottom=.5F;bool fits=true;
        for(unsigned i=0;i<3;++i){
            const auto* p=panels[i];if(!p||!p->width||!p->height)continue;
            const float width=(i==0?.5F:1.15F)*scale,half=.5F*width*p->height/p->width;
            result[i]={width,bottom+half,1.5F};bottom+=2*half+.03F;
            for(const auto& eye:eyes){
                ProjectedHudPanel projected{};
                if(!ProjectHudPanel(*p,eye,1024,1024,result[i],projected)){fits=false;break;}
                for(const auto& c:projected.clip_positions)
                    if(std::abs(c[0]/c[3])>.96F||std::abs(c[1]/c[3])>.96F)fits=false;
            }
        }
        if(fits)return true;
    }
    result={};return false;
}
struct HudTextCompositor::Impl {
    ComPtr<ID3D11Device> owner;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11VertexShader> cartridge_vs;
    ComPtr<ID3D11PixelShader> cartridge_ps;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11Buffer> cartridge_constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11DepthStencilState> depth;
    std::array<HudTextRaster, 5> raster;
    GameplayUiRaster ui_raster;
    std::array<std::uint64_t, 12> uploaded{};
    std::array<ComPtr<ID3D11ShaderResourceView>, 12> images;
    ComPtr<ID3D11BlendState> blend;
    std::array<ComPtr<ID3D11Texture2D>, 2> targets;
    std::array<ComPtr<ID3D11RenderTargetView>, 2> target_views;
    bool Initialize(ID3D11Device* device) {
        if (owner.Get() == device && vs && cartridge_vs && cartridge_ps && ps && constants && cartridge_constants && sampler && rasterizer && depth && blend) return true;
        owner = device; vs.Reset(); ps.Reset(); constants.Reset(); sampler.Reset(); blend.Reset();
        cartridge_vs.Reset();cartridge_ps.Reset();cartridge_constants.Reset();
        images = {}; uploaded = {}; targets = {}; target_views = {};
        constexpr char shader[] =
            "cbuffer Corners:register(b0){float4 corners[4];float4 material[4];};"
            "struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD;float shade:TEXCOORD1;float alpha:TEXCOORD4;};"
            "V VS(uint i:SV_VertexID){V o;o.p=corners[i];o.uv=material[i].xy;o.shade=material[0].z;o.alpha=material[0].w;return o;}"
            "Texture2D img:register(t0);SamplerState smp:register(s0);"
            "float4 PS(V v):SV_TARGET{float4 c=img.Sample(smp,v.uv);c.rgb*=v.shade;c.a*=v.alpha;return c;}";
        ComPtr<ID3DBlob> vcode, pcode;
        if (FAILED(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vcode, nullptr)) ||
            FAILED(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pcode, nullptr)) ||
            FAILED(device->CreateVertexShader(vcode->GetBufferPointer(), vcode->GetBufferSize(), nullptr, &vs)) ||
            FAILED(device->CreatePixelShader(pcode->GetBufferPointer(), pcode->GetBufferSize(), nullptr, &ps))) return false;
        constexpr unsigned mesh_corners=runtime::StereoHudTextOverlay::reload_cartridge_max_surfaces*4;
        const std::string mesh_shader="cbuffer Mesh:register(b0){float4 corners["+
            std::to_string(mesh_corners)+"];float4 material["+std::to_string(mesh_corners)+
            "];float4 normal["+std::to_string(mesh_corners)+"];float4 head["+std::to_string(mesh_corners)+"];};"
            "struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD;float shade:TEXCOORD1;float3 n:TEXCOORD2;float3 view:TEXCOORD3;float alpha:TEXCOORD4;};"
            "V VS(uint i:SV_VertexID){uint j=i%6;uint k=(i/6)*4+"
            "(j==0?0:j==1?1:j==2?2:j==3?2:j==4?1:3);"
            "V o;o.p=corners[k];o.uv=material[k].xy;o.shade=material[k].z;o.n=normal[k].xyz;o.view=-head[k].xyz;o.alpha=1;return o;}"
            "Texture2D img:register(t0);SamplerState smp:register(s0);"
            "float4 PS(V v):SV_TARGET{float3 c=img.Sample(smp,v.uv).rgb;"
            // Adapt the source's orange case palette to brass; retain the lead nose.
            // Preview lights/reflection are authored, not sampled game lighting.
            "if(c.r>c.g*1.3&&c.r>c.b*2)c*=float3(.84,1.24,1.6);"
            "float3 n=normalize(v.n),view=normalize(v.view),light=normalize(float3(-.35,.7,.62));"
            "float diffuse=max(0,dot(n,light));float3 halfv=normalize(light+view);"
            "float highlight=pow(max(0,dot(n,halfv)),48);"
            "float3 reflection=reflect(-view,n);float sky=saturate(reflection.y*.5+.5);"
            "float3 environment=lerp(float3(.16,.12,.065),float3(.52,.6,.68),sky);"
            "float3 color=c*(.25+.5*diffuse)+c*environment*.55+highlight*.6;"
            "return float4(saturate(color),1);}";
        if(FAILED(D3DCompile(mesh_shader.data(),mesh_shader.size(),nullptr,nullptr,nullptr,
            "VS","vs_4_0",0,0,&vcode,nullptr))||
            FAILED(device->CreateVertexShader(vcode->GetBufferPointer(),vcode->GetBufferSize(),nullptr,&cartridge_vs))||
            FAILED(D3DCompile(mesh_shader.data(),mesh_shader.size(),nullptr,nullptr,nullptr,
                "PS","ps_4_0",0,0,&pcode,nullptr))||
            FAILED(device->CreatePixelShader(pcode->GetBufferPointer(),pcode->GetBufferSize(),nullptr,&cartridge_ps)))return false;
        D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth = 128;
        buffer.Usage = D3D11_USAGE_DEFAULT; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        auto mesh_buffer=buffer;mesh_buffer.ByteWidth=mesh_corners*64;
        D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        D3D11_DEPTH_STENCIL_DESC dd{};
        dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        D3D11_BLEND_DESC bd{};
        auto& rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;
        rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp=rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
        rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        return SUCCEEDED(device->CreateBuffer(&buffer, nullptr, &constants)) &&
            SUCCEEDED(device->CreateBuffer(&mesh_buffer,nullptr,&cartridge_constants)) &&
            SUCCEEDED(device->CreateSamplerState(&sd, &sampler)) &&
            SUCCEEDED(device->CreateRasterizerState(&rd, &rasterizer)) &&
            SUCCEEDED(device->CreateDepthStencilState(&dd, &depth)) &&
            SUCCEEDED(device->CreateBlendState(&bd,&blend));
    }
};

HudTextCompositor::HudTextCompositor() : impl_(std::make_unique<Impl>()) {}
HudTextCompositor::~HudTextCompositor() = default;
void HudTextCompositor::Reset() noexcept { impl_.reset(); }
bool HudTextCompositor::Prepare(ID3D11Device* device) noexcept {
    if (!device) return false;
    try {
        if (!impl_) impl_ = std::make_unique<Impl>();
        return impl_->Initialize(device);
    } catch (...) { return false; }
}

bool HudTextCompositor::Draw(ID3D11Device* device, ID3D11DeviceContext* context,
    const runtime::StereoHudTextOverlay& overlay, const std::uint64_t capture_sequence,
    const std::array<ID3D11Texture2D*, 2>& targets) noexcept {
    if (!device || !context || !capture_sequence || overlay.frame_sequence != capture_sequence)
        return false;
    // Cartridge geometry is optional. Reject it in BOTH eyes without starving
    // essential HUD panels or discarding the captured scene.
    const bool cartridge_visible=[&]() noexcept {
        if(!overlay.reload_cartridge_visible||
            overlay.reload_cartridge_surface_count>overlay.reload_cartridge_max_surfaces)return false;
        for(const auto& eye:overlay.eyes){
            const auto& pose=eye.eye_to_head;const auto q=pose.orientation;
            const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
            if(!pose.position_valid||!pose.orientation_valid||!std::isfinite(norm)||
                std::abs(norm-1.F)>.001F||!runtime::IsValidEyeFov(eye.fov))return false;
            const runtime::Quaternion inverse{-q.x,-q.y,-q.z,q.w};
            const auto count=overlay.reload_cartridge_surface_count?overlay.reload_cartridge_surface_count:1;
            for(std::size_t i=0;i<count;++i){
                const auto& face=overlay.reload_cartridge_surfaces[i];
                if(overlay.reload_cartridge_surface_count&&(!std::isfinite(face.material_uv.x)||!std::isfinite(face.material_uv.y)||
                    face.material_uv.x<0||face.material_uv.x>1||face.material_uv.y<0||face.material_uv.y>1||
                    !std::isfinite(face.shade)||face.shade<0||face.shade>1))return false;
                if(overlay.reload_cartridge_textured)for(unsigned j=0;j<4;++j){
                    const auto uv=face.corner_uv[j];const auto shade=face.corner_shade[j];
                    if(!std::isfinite(uv.x)||!std::isfinite(uv.y)||uv.x<0||uv.x>1||uv.y<0||uv.y>1||
                        !std::isfinite(shade)||shade<0||shade>1)return false;
                    const auto n=face.corner_normal[j];const float normal_norm=n.x*n.x+n.y*n.y+n.z*n.z;
                    if(!std::isfinite(normal_norm)||std::abs(normal_norm-1.F)>.01F)return false;
                }
                const auto& corners=overlay.reload_cartridge_surface_count?face.head_corners:
                    overlay.reload_cartridge_head_corners;
                for(auto p:corners){
                    p=runtime::RotateVector(inverse,{p.x-pose.position.x,p.y-pose.position.y,p.z-pose.position.z});
                    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.z>=-.05F)return false;
                }
            }
        }
        return true;
    }();
    // Independent optional interaction marker: invalid capture/optics in either eye
    // hides it in both eyes while existing HUD panels retain their own gates.
    const bool interaction_gaze_visible = [&]() noexcept {
        if (!overlay.interaction_gaze_visible) return false;
        for (const auto& eye : overlay.eyes) {
            const auto& pose = eye.eye_to_head;
            const auto q = pose.orientation;
            const float norm = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
            if (!pose.position_valid || !pose.orientation_valid || !std::isfinite(norm) ||
                std::abs(norm - 1.F) > .001F || !runtime::IsValidEyeFov(eye.fov)) return false;
            const runtime::Quaternion inverse{-q.x,-q.y,-q.z,q.w};
            for (auto p : overlay.interaction_gaze_head_corners) {
                p = runtime::RotateVector(inverse, {p.x-pose.position.x,
                    p.y-pose.position.y, p.z-pose.position.z});
                if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
                    !std::isfinite(p.z) || p.z >= -.05F) return false;
            }
        }
        return true;
    }();
    if (overlay.text.hint.view().empty() && overlay.text.interaction.view().empty() &&
        overlay.text.subtitle.view().empty() && !overlay.ui.wheel.active &&
        !(overlay.ui.compass.active && overlay.compass_surface_valid) &&
        !(overlay.ui.status.active && overlay.status_surface_valid) &&
        !cartridge_visible && !interaction_gaze_visible && overlay.mission_timer.view().empty() &&
        overlay.mission_notices.view().empty() && !overlay.threats.count) return true;
    try {
        if (!Prepare(device)) return false;
        const std::array<const runtime::HudText*, 5> texts{
            &overlay.text.hint, &overlay.text.interaction, &overlay.text.subtitle,
            &overlay.mission_timer,&overlay.mission_notices};
        std::array<HudPanelPlacement, 12> placement{{{1.15F, .48F, 1.5F},
            {.85F, -.22F, 1.5F}, {1.15F, -.48F, 1.5F}, {.44F,0,1.0F}, {}, {}, {}, {},
            {.5F,.58F,1.5F},{1.15F,.68F,1.5F},{},{}}};
        placement[4].captured_quad=true;
        placement[4].head_corners=overlay.compass_head_corners;
        placement[5].captured_quad=true;
        placement[5].head_corners=overlay.status_head_corners;
        // Slot seven carries captured controller-oriented surfaces (legacy quad
        // when no surfaces are present), not game ammo.
        // Like the existing transparent compositor panels, it is always on
        // top: the transported color images contain no game depth for occlusion.
        placement[6].captured_quad=true;
        placement[6].head_corners=overlay.reload_cartridge_head_corners;
        // Eighth slot owns captured interaction feedback, independent of weapon alignment.
        placement[7].captured_quad = true;
        placement[7].head_corners = overlay.interaction_gaze_head_corners;
        const auto& subtitle = impl_->raster[2].Render(*texts[2]);
        const auto& interaction = impl_->raster[1].Render(*texts[1]);
        const auto& timer=impl_->raster[3].Render(*texts[3],true);
        const auto& notices=impl_->raster[4].Render(*texts[4]);
        bool critical_fits=true;
        if(timer.width||notices.width){
            const auto& hint=impl_->raster[0].Render(*texts[0]);std::array<HudPanelPlacement,3> fit{};
            critical_fits=FitCriticalHudPanels({&timer,&hint,&notices},overlay.eyes,fit);
            if(critical_fits){placement[8]=fit[0];if(hint.width)placement[0]=fit[1];placement[9]=fit[2];}
        }
        if (subtitle.width && interaction.width) {
            const float subtitle_top = placement[2].center_y_m +
                .5F * placement[2].width_m * subtitle.height / subtitle.width;
            const float interaction_half_height =
                .5F * placement[1].width_m * interaction.height / interaction.width;
            placement[1].center_y_m = std::max(placement[1].center_y_m,
                subtitle_top + interaction_half_height + .05F);
        }
        auto compass=overlay.ui.compass;
        if(!overlay.compass_surface_valid)compass.active=false;
        const auto& wheel_panel=impl_->ui_raster.Wheel(overlay.ui.wheel);
        const auto& compass_panel=impl_->ui_raster.Compass(compass,GetTickCount64());
        auto status=overlay.ui.status;if(!overlay.status_surface_valid)status.active=false;
        const auto& status_panel=impl_->ui_raster.Status(status);
        const HudTextPanel empty_cartridge{};
        const auto& cartridge_panel=cartridge_visible?
            (overlay.reload_cartridge_textured?impl_->ui_raster.TexturedCartridge():impl_->ui_raster.Cartridge()):empty_cartridge;
        const HudTextPanel empty_gaze{};
        const auto& gaze_panel = interaction_gaze_visible ?
            impl_->ui_raster.InteractionGaze() : empty_gaze;
        for (std::size_t part = 0; part < placement.size(); ++part) {
            const bool critical_text=part==8||part==9,threat=part>=10;
            if(critical_text&&!critical_fits)continue;
            const auto text_index=critical_text?part-5:part;
            const auto& panel = (part<3||critical_text) ? impl_->raster[text_index].Render(*texts[text_index],text_index==3) :
                threat?impl_->ui_raster.Threat(part==11):
                (part==3 ? wheel_panel : (part==4 ? compass_panel : (part==5 ? status_panel :
                    (part==6 ? cartridge_panel : gaze_panel))));
            const auto revision=(part<3||critical_text) ? impl_->raster[text_index].rebuilds() : threat?1:
                (part==3 ? impl_->ui_raster.wheel_revision() : (part==4 ? impl_->ui_raster.compass_revision() : (part==5 ? impl_->ui_raster.status_revision() : part==6&&overlay.reload_cartridge_textured?2:1)));
            // Retain the immutable cartridge GPU cache across gesture hiding.
            if(part==6&&!cartridge_visible)continue;
            // Immutable marker pixels/GPU cache survive hidden captures.
            if(part==7&&!interaction_gaze_visible)continue;
            if(threat&&(!overlay.threats.count||overlay.threats.count>overlay.threats.markers.size()))continue;
            if (panel.pixels.empty()) { impl_->images[part].Reset(); continue; }
            if (!impl_->images[part] || impl_->uploaded[part] != revision) {
                D3D11_TEXTURE2D_DESC td{}; td.Width = panel.width; td.Height = panel.height;
                td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE;
                td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data{panel.pixels.data(), panel.width * 4, 0};
                ComPtr<ID3D11Texture2D> texture;
                impl_->images[part].Reset();
                if (FAILED(device->CreateTexture2D(&td, &data, &texture)) ||
                    FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &impl_->images[part]))) {
                    if (part==7) continue; // Optional feedback failure preserves essential HUD.
                    return false;
                }
                impl_->uploaded[part] = revision;
            }
            const bool cartridge_surfaces=part==6&&overlay.reload_cartridge_surface_count!=0;
            if(cartridge_surfaces){
                // One bounded constant upload and draw per eye, even for the
                // imported mesh. Do not add a Draw/UpdateSubresource per triangle.
                constexpr std::size_t corners=runtime::StereoHudTextOverlay::reload_cartridge_max_surfaces*4;
                struct MeshConstants{std::array<std::array<float,4>,corners> positions{},materials{},normals{},head{};};
                for(std::size_t eye=0;eye<2;++eye){
                    if(!targets[eye])return false;
                    D3D11_TEXTURE2D_DESC td{};targets[eye]->GetDesc(&td);
                    MeshConstants batch{};unsigned faces=0;
                    for(unsigned surface=0;surface<overlay.reload_cartridge_surface_count;++surface){
                        const auto& face=overlay.reload_cartridge_surfaces[surface];
                        auto capture=placement[part];capture.head_corners=face.head_corners;
                        ProjectedHudPanel projected{};
                        if(!ProjectHudPanel(panel,overlay.eyes[eye],td.Width,td.Height,capture,projected))continue;
                        const auto& c=face.head_corners;const auto e=overlay.eyes[eye].eye_to_head.position;
                        const runtime::Vec3 a{c[1].x-c[0].x,c[1].y-c[0].y,c[1].z-c[0].z};
                        const runtime::Vec3 b{c[2].x-c[0].x,c[2].y-c[0].y,c[2].z-c[0].z};
                        const runtime::Vec3 n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
                        if(n.x*(e.x-c[0].x)+n.y*(e.y-c[0].y)+n.z*(e.z-c[0].z)<=0)continue;
                        for(unsigned j=0;j<4;++j){batch.positions[4*faces+j]=projected.clip_positions[j];
                            const auto uv=overlay.reload_cartridge_textured?face.corner_uv[j]:face.material_uv;
                            const auto shade=overlay.reload_cartridge_textured?face.corner_shade[j]:face.shade;
                            batch.materials[4*faces+j]={uv.x,uv.y,shade,0};
                            const auto vertex_normal=face.corner_normal[j],p=face.head_corners[j];
                            batch.normals[4*faces+j]={vertex_normal.x,vertex_normal.y,vertex_normal.z,0};
                            batch.head[4*faces+j]={p.x-e.x,p.y-e.y,p.z-e.z,0};}
                        ++faces;
                    }
                    if(!faces)continue;
                    if(impl_->targets[eye].Get()!=targets[eye]){
                        impl_->target_views[eye].Reset();impl_->targets[eye]=targets[eye];
                        if(FAILED(device->CreateRenderTargetView(targets[eye],nullptr,&impl_->target_views[eye])))return false;
                    }
                    if(!impl_->target_views[eye])return false;
                    context->UpdateSubresource(impl_->cartridge_constants.Get(),0,nullptr,&batch,0,0);
                    auto* render_target=impl_->target_views[eye].Get();auto* constant=impl_->cartridge_constants.Get();
                    auto* image=impl_->images[part].Get();auto* sampler=impl_->sampler.Get();
                    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(td.Width),static_cast<float>(td.Height),0,1};
                    context->OMSetRenderTargets(1,&render_target,nullptr);
                    context->OMSetBlendState(impl_->blend.Get(),nullptr,0xFFFFFFFF);
                    context->OMSetDepthStencilState(impl_->depth.Get(),0);
                    context->RSSetState(impl_->rasterizer.Get());context->RSSetViewports(1,&viewport);
                    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    context->VSSetShader(impl_->cartridge_vs.Get(),nullptr,0);context->VSSetConstantBuffers(0,1,&constant);
                    context->GSSetShader(nullptr,nullptr,0);
                    context->PSSetShader(overlay.reload_cartridge_textured?impl_->cartridge_ps.Get():impl_->ps.Get(),nullptr,0);
                    context->PSSetShaderResources(0,1,&image);context->PSSetSamplers(0,1,&sampler);
                    context->Draw(6*faces,0);
                    ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
                    context->OMSetRenderTargets(0,nullptr,nullptr);
                }
                continue;
            }
            const std::size_t surface_count=threat?overlay.threats.count:cartridge_surfaces?overlay.reload_cartridge_surface_count:1;
            for(std::size_t surface=0;surface<surface_count;++surface){
                auto surface_placement=placement[part];
                if(threat){
                    const auto& marker=overlay.threats.markers[surface];
                    if(marker.damage!=(part==11)||!BuildThreatPanelPlacement(marker,surface_placement))continue;
                }
                if(cartridge_surfaces)surface_placement.head_corners=
                    overlay.reload_cartridge_surfaces[surface].head_corners;
                std::array<ProjectedHudPanel, 2> quads{};
                std::array<bool,2> eye_projected{};
                std::array<D3D11_TEXTURE2D_DESC, 2> descriptions{};
                bool projected = true;
                for (std::size_t eye = 0; eye < 2; ++eye) {
                    if (!targets[eye]) return false;
                    targets[eye]->GetDesc(&descriptions[eye]);
                    eye_projected[eye] = ProjectHudPanel(panel, overlay.eyes[eye],
                        descriptions[eye].Width, descriptions[eye].Height, surface_placement, quads[eye]);
                    projected = eye_projected[eye] && projected;
                }
                if (!projected&&!cartridge_surfaces) continue;
                for (std::size_t eye = 0; eye < 2; ++eye) {
                    if(!eye_projected[eye])continue;
                    if(cartridge_surfaces){
                        const auto& c=surface_placement.head_corners;
                        const runtime::Vec3 a{c[1].x-c[0].x,c[1].y-c[0].y,c[1].z-c[0].z};
                        const runtime::Vec3 b{c[2].x-c[0].x,c[2].y-c[0].y,c[2].z-c[0].z};
                        const runtime::Vec3 normal{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
                        const auto e=overlay.eyes[eye].eye_to_head.position;
                        // Convex surfaces only: back-face removal gives cartridge
                        // self-occlusion without claiming unavailable game depth.
                        if(normal.x*(e.x-c[0].x)+normal.y*(e.y-c[0].y)+normal.z*(e.z-c[0].z)<=0)continue;
                    }
                    const auto& td = descriptions[eye];
                    const auto& quad = quads[eye];
                    if (impl_->targets[eye].Get() != targets[eye]) {
                        impl_->target_views[eye].Reset();
                        impl_->targets[eye] = targets[eye];
                        if (FAILED(device->CreateRenderTargetView(targets[eye], nullptr, &impl_->target_views[eye]))) return false;
                    }
                    if (!impl_->target_views[eye]) return false;
                    struct DrawConstants {
                        std::array<std::array<float,4>,4> corners;
                        std::array<std::array<float,4>,4> material;
                    } constants{quad.clip_positions,{}};
                    for(std::size_t i=0;i<4;++i){
                        constants.material[i]={static_cast<float>(i&1),static_cast<float>((i>>1)&1),1.F,
                            threat?overlay.threats.markers[surface].alpha:1.F};
                        if(cartridge_surfaces){
                            const auto& face=overlay.reload_cartridge_surfaces[surface];
                            constants.material[i]={face.material_uv.x,face.material_uv.y,face.shade,1.F};
                        }
                    }
                    context->UpdateSubresource(impl_->constants.Get(), 0, nullptr, &constants, 0, 0);
                    auto* render_target = impl_->target_views[eye].Get();
                    auto* constant = impl_->constants.Get();
                    auto* image = impl_->images[part].Get();
                    auto* sampler = impl_->sampler.Get();
                    const D3D11_VIEWPORT viewport{0, 0, static_cast<float>(td.Width), static_cast<float>(td.Height), 0, 1};
                    context->OMSetRenderTargets(1, &render_target, nullptr);
                    context->OMSetBlendState(impl_->blend.Get(), nullptr, 0xFFFFFFFF);
                    context->OMSetDepthStencilState(impl_->depth.Get(), 0);
                    context->RSSetState(impl_->rasterizer.Get());
                    context->RSSetViewports(1, &viewport);
                    context->IASetInputLayout(nullptr);
                    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                    context->VSSetShader(impl_->vs.Get(), nullptr, 0);
                    context->VSSetConstantBuffers(0, 1, &constant);
                    context->GSSetShader(nullptr, nullptr, 0);
                    context->PSSetShader(impl_->ps.Get(), nullptr, 0);
                    context->PSSetConstantBuffers(0, 1, &constant);
                    context->PSSetShaderResources(0, 1, &image);
                    context->PSSetSamplers(0, 1, &sampler);
                    context->Draw(4, 0);
                    ID3D11ShaderResourceView* empty = nullptr;
                    context->PSSetShaderResources(0, 1, &empty);
                    context->OMSetRenderTargets(0, nullptr, nullptr);
                }
            }
        }
        return true;
    } catch (...) { return false; }
}
} // namespace cojvr::backends::openvr
