#include "backends/openvr/hud_text_overlay.hpp"
#include "backends/openvr/gameplay_ui_raster.hpp"
#include "backends/openvr/holster_guide_pixels.hpp"
#include "runtime/action_prompts.hpp"
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
    std::array<std::uint64_t, 21> uploaded{};
    std::array<ComPtr<ID3D11ShaderResourceView>, 21> images;
    ComPtr<ID3D11BlendState> blend;
    std::array<ComPtr<ID3D11Texture2D>, 2> targets;
    std::array<ComPtr<ID3D11RenderTargetView>, 2> target_views;
    struct TextPatch {
        ComPtr<ID3D11Texture2D> image;
        D3D11_TEXTURE2D_DESC description{};
        ProjectedHudPanel rectangle{};
        bool saved=false;
    };
    // Two text parts, two eyes. Only these rectangles consume backing memory;
    // never allocate whole-eye preimages in this x86 presenter.
    std::array<TextPatch,4> text_patches;
    ComPtr<ID3D11DeviceContext> text_context;
    std::array<ComPtr<ID3D11Texture2D>,2> text_targets;
    std::uint64_t text_generation=0,text_reload_owner=0,text_until_ms=0;
    std::uint64_t text_glyph_sample_ms=0,text_glyph_until_ms=0;
    bool text_reload=false;
    void DiscardText() noexcept {
        for(auto& patch:text_patches){patch.saved=false;patch.rectangle={};}
        text_context.Reset();text_targets={};
        text_generation=text_reload_owner=text_until_ms=0;text_reload=false;
        text_glyph_sample_ms=text_glyph_until_ms=0;
    }
    bool PrepareTextPatches(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& outputs,std::size_t part,
        const std::array<ProjectedHudPanel,2>& quads,
        const std::array<D3D11_TEXTURE2D_DESC,2>& descriptions) noexcept {
        if(context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||outputs[0]==outputs[1])return false;
        ComPtr<ID3D11Device> context_owner;context->GetDevice(&context_owner);
        if(context_owner.Get()!=device)return false;
        std::array<D3D11_TEXTURE2D_DESC,2> needed{};
        for(std::size_t eye=0;eye<2;++eye){
            const auto& td=descriptions[eye];
            ComPtr<ID3D11Device> target_owner;outputs[eye]->GetDevice(&target_owner);
            if(target_owner.Get()!=device||td.SampleDesc.Count!=1||td.SampleDesc.Quality||
                td.MipLevels!=1||td.ArraySize!=1||td.Usage!=D3D11_USAGE_DEFAULT||
                !(td.BindFlags&D3D11_BIND_RENDER_TARGET)||
                (td.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&td.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB&&
                 td.Format!=DXGI_FORMAT_B8G8R8X8_UNORM&&td.Format!=DXGI_FORMAT_B8G8R8X8_UNORM_SRGB&&
                 td.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&td.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))return false;
            auto& desc=needed[eye];
            desc.Width=quads[eye].width;desc.Height=quads[eye].height;
            desc.Format=td.Format;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
            desc.Usage=D3D11_USAGE_DEFAULT; // Copy-only: no bind, CPU or misc flags.
        }
        constexpr std::uint64_t max_bytes=16ULL*1024*1024;
        std::uint64_t total=0;
        for(std::size_t slot=0;slot<text_patches.size();++slot){
            const auto& patch=text_patches[slot];
            if(slot/2==part)total+=std::uint64_t(needed[slot%2].Width)*needed[slot%2].Height*4;
            else if(patch.image)total+=std::uint64_t(patch.description.Width)*patch.description.Height*4;
        }
        if(total>max_bytes)return false;
        // Release replaced caches BEFORE allocating either successor so the
        // cap also holds during resize, including retained other-part caches.
        for(std::size_t eye=0;eye<2;++eye){
            auto& patch=text_patches[2*part+eye];const auto& desc=needed[eye];
            if(patch.image&&(patch.description.Width!=desc.Width||
                patch.description.Height!=desc.Height||patch.description.Format!=desc.Format)){
                patch.image.Reset();patch.description={};
            }
        }
        for(std::size_t eye=0;eye<2;++eye){
            auto& patch=text_patches[2*part+eye];
            if(!patch.image){
                if(FAILED(device->CreateTexture2D(&needed[eye],nullptr,&patch.image)))return false;
                patch.description=needed[eye];
            }
            // RTVs are also preflighted for both eyes before saving/drawing.
            if(targets[eye].Get()!=outputs[eye]){
                target_views[eye].Reset();targets[eye]=outputs[eye];
            }
            if(!target_views[eye]&&
                FAILED(device->CreateRenderTargetView(outputs[eye],nullptr,&target_views[eye])))return false;
        }
        return true;
    }
    bool Initialize(ID3D11Device* device) {
        if (owner.Get() == device && vs && cartridge_vs && cartridge_ps && ps && constants && cartridge_constants && sampler && rasterizer && depth && blend) return true;
        owner = device; vs.Reset(); ps.Reset(); constants.Reset(); sampler.Reset(); blend.Reset();
        cartridge_vs.Reset();cartridge_ps.Reset();cartridge_constants.Reset();
        images = {}; uploaded = {}; targets = {}; target_views = {};
        DiscardText();text_patches={};
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
void HudTextCompositor::DiscardRepeatedText() noexcept {
    if(impl_)impl_->DiscardText();
}
void HudTextCompositor::RetireStaleText(ID3D11Device* device,ID3D11DeviceContext* context,
    const std::array<ID3D11Texture2D*,2>& targets,const bool show_holster_guides,
    const std::uint64_t reload_owner_token) noexcept {
    if(!impl_||!impl_->text_context)return;
    if(!device||!context||impl_->owner.Get()!=device||impl_->text_context.Get()!=context||
        impl_->text_targets[0].Get()!=targets[0]||impl_->text_targets[1].Get()!=targets[1]){
        DiscardRepeatedText();return;
    }
    const bool stale_generation=impl_->text_generation&&
        impl_->text_generation!=runtime::CurrentActionPrompts().binding_generation;
    const auto now=GetTickCount64();
    const bool stale_reload=impl_->text_reload&&(!show_holster_guides||!reload_owner_token||
        reload_owner_token!=impl_->text_reload_owner||!impl_->text_until_ms||
        now>=impl_->text_until_ms);
    const bool stale_glyph=impl_->text_glyph_until_ms&&
        (now<impl_->text_glyph_sample_ms||now>=impl_->text_glyph_until_ms);
    if(!stale_generation&&!stale_reload&&!stale_glyph)return;
    // Retire the complete controlled stack if any layer expires. Reversing the
    // draw order preserves preimages even when its text rectangles overlap.
    for(std::size_t slot=impl_->text_patches.size();slot>0;--slot){
        const auto& patch=impl_->text_patches[slot-1];
        if(!patch.saved)continue;
        const auto& r=patch.rectangle;
        const D3D11_BOX box{0,0,0,r.width,r.height,1};
        context->CopySubresourceRegion(targets[(slot-1)%2],0,r.left,r.top,0,patch.image.Get(),0,&box);
    }
    DiscardRepeatedText(); // Consume identity once, retain bounded GPU backing.
}
bool HudTextCompositor::Prepare(ID3D11Device* device) noexcept {
    if (!device) return false;
    try {
        if (!impl_) impl_ = std::make_unique<Impl>();
        return impl_->Initialize(device);
    } catch (...) { return false; }
}

bool HudTextCompositor::Draw(ID3D11Device* device, ID3D11DeviceContext* context,
    const runtime::StereoHudTextOverlay& overlay, const std::uint64_t capture_sequence,
    const std::array<ID3D11Texture2D*, 2>& targets,const bool show_holster_guides,
    const std::uint64_t reload_owner_token) noexcept {
    DiscardRepeatedText(); // World was freshly copied: never restore its predecessor.
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
    const bool guide_data=show_holster_guides&&overlay.holster_guides.count>0&&
        overlay.holster_guides.count<=overlay.holster_guides.zones.size();
    if (overlay.text.hint.view().empty() && overlay.text.interaction.view().empty() &&
        overlay.text.subtitle.view().empty() && !overlay.ui.wheel.active &&
        !(overlay.ui.compass.active && overlay.compass_surface_valid) &&
        !(overlay.ui.status.active && overlay.status_surface_valid) &&
        !cartridge_visible && !interaction_gaze_visible && overlay.mission_timer.view().empty() &&
        overlay.mission_notices.view().empty() && !overlay.threats.count && !guide_data) return true;
    try {
        if (!Prepare(device)) return false;
        const runtime::HudText hidden_hint{};
        const auto now=GetTickCount64();
        const auto current_generation=runtime::CurrentActionPrompts().binding_generation;
        const bool stale_bindings=overlay.text.controller_prompt_generation&&
            overlay.text.controller_prompt_generation!=current_generation;
        const bool stale_reload=overlay.reload_instruction&&(!show_holster_guides||!reload_owner_token||
            reload_owner_token!=overlay.reload_feedback.owner_token||!overlay.reload_instruction_until_ms||
            now>=overlay.reload_instruction_until_ms||!overlay.text.hint.view().empty());
        const auto coaching_glyph=!stale_bindings&&!stale_reload?
            runtime::ResolveReloadInstructionGlyph(overlay,current_generation,now):runtime::PromptGlyph{};
        const std::array<const runtime::HudText*, 5> texts{
            stale_bindings?&hidden_hint:&overlay.text.hint,
            stale_bindings||stale_reload?&hidden_hint:&overlay.text.interaction, &overlay.text.subtitle,
            &overlay.mission_timer,&overlay.mission_notices};
        std::array<HudPanelPlacement, 21> placement{{{1.15F, .48F, 1.5F},
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
        const auto& interaction_text = impl_->raster[1].Render(*texts[1]);
        // Only the semantic reload owner supplies artwork metadata. Native HUD
        // strings are never parsed and retain their original pixels/priority.
        const auto& interaction=impl_->ui_raster.DecorateControlHint(interaction_text,
            impl_->raster[1].rebuilds(),overlay.control_hint_prompts,overlay.control_hint_action,
            current_generation,now,coaching_glyph.kind!=runtime::PromptGlyphKind::none);
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
        const std::array<bool,2> controlled{overlay.text.controller_prompt_generation!=0,
            overlay.text.controller_prompt_generation!=0||overlay.reload_instruction};
        // Keep ordinary native text order unchanged. Controlled hint/interaction
        // alone move to the end, so their saved backgrounds include all HUD.
        for (std::size_t order = 0; order < placement.size()+2; ++order) {
            const auto part=order<placement.size()?order:order-placement.size();
            if(part<2&&((order<placement.size())==controlled[part]))continue;
            const bool critical_text=part==8||part==9,threat=part==10||part==11,guide=part>=12;
            const bool ammunition_guide=part>=18,support_guide=part>=15&&!ammunition_guide;
            const auto guide_kind=ammunition_guide?runtime::GestureGuideKind::ammunition_pickup:
                support_guide?runtime::GestureGuideKind::support_grip:runtime::GestureGuideKind::holster;
            const auto guide_state=guide?(part-12)%3:0;
            if(guide&&!guide_data)continue;
            if(ammunition_guide&&(!reload_owner_token||reload_owner_token!=overlay.reload_feedback.owner_token))continue;
            if(guide&&!std::any_of(overlay.holster_guides.zones.begin(),
                overlay.holster_guides.zones.begin()+overlay.holster_guides.count,
                [&](const auto& zone){return zone.kind==guide_kind&&zone.state==guide_state;}))continue;
            if(critical_text&&!critical_fits)continue;
            const auto text_index=critical_text?part-5:part;
            const auto& panel = part==1?interaction:(part<3||critical_text) ? impl_->raster[text_index].Render(*texts[text_index],text_index==3) :
                guide?(ammunition_guide?AmmunitionPickupGuidePanels()[guide_state]:support_guide?SupportGripGuidePanels()[guide_state]:HolsterGuidePanels()[guide_state]):threat?impl_->ui_raster.Threat(part==11):
                (part==3 ? wheel_panel : (part==4 ? compass_panel : (part==5 ? status_panel :
                    (part==6 ? cartridge_panel : gaze_panel))));
            const auto revision=part==1?impl_->ui_raster.control_decoration_revision():
                (part<3||critical_text) ? impl_->raster[text_index].rebuilds() : (threat||guide)?1:
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
                    if (part==7||guide||(part<2&&controlled[part])) continue; // Optional feedback failure preserves essential HUD.
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
            const std::size_t surface_count=guide?overlay.holster_guides.count:threat?overlay.threats.count:cartridge_surfaces?overlay.reload_cartridge_surface_count:1;
            for(std::size_t surface=0;surface<surface_count;++surface){
                auto surface_placement=placement[part];
                if(guide){
                    const auto& zone=overlay.holster_guides.zones[surface];
                    if(zone.kind!=guide_kind||zone.state!=guide_state)continue;
                    surface_placement.captured_quad=true;surface_placement.head_corners=zone.head_corners;
                }
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
                const bool fenced_text=part<2&&controlled[part];
                if(fenced_text){
                    if(!impl_->PrepareTextPatches(device,context,targets,part,quads,descriptions))continue;
                    impl_->text_context=context;
                    for(std::size_t eye=0;eye<2;++eye)impl_->text_targets[eye]=targets[eye];
                    impl_->text_generation=overlay.text.controller_prompt_generation;
                    if(part==1&&overlay.reload_instruction){
                        impl_->text_reload=true;impl_->text_reload_owner=overlay.reload_feedback.owner_token;
                        impl_->text_until_ms=overlay.reload_instruction_until_ms;
                        if(coaching_glyph.kind!=runtime::PromptGlyphKind::none){
                            impl_->text_glyph_sample_ms=overlay.control_hint_prompts.glyph_sample_ms;
                            impl_->text_glyph_until_ms=overlay.control_hint_prompts.glyph_deadline_ms;
                        }
                    }
                    for(std::size_t eye=0;eye<2;++eye){
                        auto& patch=impl_->text_patches[2*part+eye];const auto& r=quads[eye];
                        const D3D11_BOX box{r.left,r.top,0,r.left+r.width,r.top+r.height,1};
                        context->CopySubresourceRegion(patch.image.Get(),0,0,0,0,targets[eye],0,&box);
                        patch.rectangle=r;patch.saved=true;
                    }
                }
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
