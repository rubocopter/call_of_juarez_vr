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

const HudTextPanel& HudTextRaster::Render(const runtime::HudText& text) noexcept {
    if (text == text_ && (!panel_.pixels.empty() || text.view().empty())) return panel_;
    text_ = text;
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
        constexpr LONG width = 1024, padding = 24, max_height = 350;
        for (int size = 36; size >= 16; size -= 2) {
            HFONT font = CreateFontW(-size, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            if (!font) break;
            font_owner.value = font;
            selected_font.previous = SelectObject(dc, font);
            measured = {padding, padding, width - padding, padding};
            DrawTextW(dc, wide.data(), static_cast<int>(wide.size()), &measured,
                DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
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
            DrawTextW(dc, wide.data(), static_cast<int>(wide.size()), &rect,
                DT_WORDBREAK | DT_NOPREFIX | DT_LEFT);
            GdiFlush();
            panel_.width = width;
            panel_.height = static_cast<std::uint32_t>(height);
            panel_.pixels.assign(pixels, pixels + width * height);
            for (auto& pixel : panel_.pixels) pixel |= 0xFF000000U;
        }
    } catch (...) { panel_ = {}; }
    return panel_;
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

struct HudTextCompositor::Impl {
    ComPtr<ID3D11Device> owner;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11DepthStencilState> depth;
    std::array<HudTextRaster, 3> raster;
    GameplayUiRaster ui_raster;
    std::array<std::uint64_t, 6> uploaded{};
    std::array<ComPtr<ID3D11ShaderResourceView>, 6> images;
    ComPtr<ID3D11BlendState> blend;
    std::array<ComPtr<ID3D11Texture2D>, 2> targets;
    std::array<ComPtr<ID3D11RenderTargetView>, 2> target_views;
    bool Initialize(ID3D11Device* device) {
        if (owner.Get() == device && vs && ps && constants && sampler && rasterizer && depth && blend) return true;
        owner = device; vs.Reset(); ps.Reset(); constants.Reset(); sampler.Reset(); blend.Reset();
        images = {}; uploaded = {}; targets = {}; target_views = {};
        constexpr char shader[] =
            "cbuffer Corners:register(b0){float4 corners[4];};"
            "struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD;};"
            "V VS(uint i:SV_VertexID){V o;o.p=corners[i];o.uv=float2(i&1,(i>>1)&1);return o;}"
            "Texture2D img:register(t0);SamplerState smp:register(s0);"
            "float4 PS(V v):SV_TARGET{return img.Sample(smp,v.uv);}";
        ComPtr<ID3DBlob> vcode, pcode;
        if (FAILED(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vcode, nullptr)) ||
            FAILED(D3DCompile(shader, sizeof(shader)-1, nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pcode, nullptr)) ||
            FAILED(device->CreateVertexShader(vcode->GetBufferPointer(), vcode->GetBufferSize(), nullptr, &vs)) ||
            FAILED(device->CreatePixelShader(pcode->GetBufferPointer(), pcode->GetBufferSize(), nullptr, &ps))) return false;
        D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth = 64;
        buffer.Usage = D3D11_USAGE_DEFAULT; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
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
    if (overlay.text.hint.view().empty() && overlay.text.interaction.view().empty() &&
        overlay.text.subtitle.view().empty() && !overlay.ui.wheel.active &&
        !(overlay.ui.compass.active && overlay.compass_surface_valid) &&
        !(overlay.ui.status.active && overlay.status_surface_valid)) return true;
    try {
        if (!Prepare(device)) return false;
        const std::array<const runtime::HudText*, 3> texts{
            &overlay.text.hint, &overlay.text.interaction, &overlay.text.subtitle};
        std::array<HudPanelPlacement, 6> placement{{{1.15F, .48F, 1.5F},
            {.85F, -.22F, 1.5F}, {1.15F, -.48F, 1.5F}, {.44F,0,1.0F}, {}, {}}};
        placement[4].captured_quad=true;
        placement[4].head_corners=overlay.compass_head_corners;
        placement[5].captured_quad=true;
        placement[5].head_corners=overlay.status_head_corners;
        const auto& subtitle = impl_->raster[2].Render(*texts[2]);
        const auto& interaction = impl_->raster[1].Render(*texts[1]);
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
        for (std::size_t part = 0; part < placement.size(); ++part) {
            const auto& panel = part<3 ? impl_->raster[part].Render(*texts[part]) :
                (part==3 ? wheel_panel : (part==4 ? compass_panel : status_panel));
            const auto revision=part<3 ? impl_->raster[part].rebuilds() :
                (part==3 ? impl_->ui_raster.wheel_revision() : (part==4 ? impl_->ui_raster.compass_revision() : impl_->ui_raster.status_revision()));
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
                    FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &impl_->images[part]))) return false;
                impl_->uploaded[part] = revision;
            }
            std::array<ProjectedHudPanel, 2> quads{};
            std::array<D3D11_TEXTURE2D_DESC, 2> descriptions{};
            bool projected = true;
            for (std::size_t eye = 0; eye < 2; ++eye) {
                if (!targets[eye]) return false;
                targets[eye]->GetDesc(&descriptions[eye]);
                projected = ProjectHudPanel(panel, overlay.eyes[eye],
                    descriptions[eye].Width, descriptions[eye].Height, placement[part], quads[eye]) && projected;
            }
            if (!projected) continue;
            for (std::size_t eye = 0; eye < 2; ++eye) {
                const auto& td = descriptions[eye];
                const auto& quad = quads[eye];
                if (impl_->targets[eye].Get() != targets[eye]) {
                    impl_->target_views[eye].Reset();
                    impl_->targets[eye] = targets[eye];
                    if (FAILED(device->CreateRenderTargetView(targets[eye], nullptr, &impl_->target_views[eye]))) return false;
                }
                if (!impl_->target_views[eye]) return false;
                context->UpdateSubresource(impl_->constants.Get(), 0, nullptr, quad.clip_positions.data(), 0, 0);
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
                context->PSSetShaderResources(0, 1, &image);
                context->PSSetSamplers(0, 1, &sampler);
                context->Draw(4, 0);
                ID3D11ShaderResourceView* empty = nullptr;
                context->PSSetShaderResources(0, 1, &empty);
                context->OMSetRenderTargets(0, nullptr, nullptr);
            }
        }
        return true;
    } catch (...) { return false; }
}
} // namespace cojvr::backends::openvr
