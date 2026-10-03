#pragma once

#include "runtime/hud_text.hpp"
#include <vector>
#include <memory>
#include <d3d11.h>

namespace cojvr::backends::openvr {

struct HudTextPanel {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint32_t> pixels;
};

// Cache owns GDI output only; rebuilding occurs when visible text changes.
class HudTextRaster {
public:
    [[nodiscard]] const HudTextPanel& Render(const runtime::HudText& text) noexcept;
    [[nodiscard]] std::uint64_t rebuilds() const noexcept { return rebuilds_; }
private:
    runtime::HudText text_{};
    HudTextPanel panel_{};
    std::uint64_t rebuilds_ = 0;
};

struct HudPanelPlacement {
    float width_m = 1.15F;
    float center_y_m = 0.0F;
    float distance_m = 1.5F;
};

struct ProjectedHudPanel {
    std::uint32_t left = 0, top = 0, width = 0, height = 0;
    std::array<std::array<float, 4>, 4> clip_positions{};
};

// Homogeneous corners preserve perspective under cant and asymmetric FOV.
[[nodiscard]] bool ProjectHudPanel(const HudTextPanel& source,
    const runtime::EyeView& eye, std::uint32_t target_width,
    std::uint32_t target_height, HudPanelPlacement placement,
    ProjectedHudPanel& result) noexcept;

// Operates exclusively on presenter-owned D3D11 resources/context.
class HudTextCompositor {
public:
    HudTextCompositor();
    ~HudTextCompositor();
    void Reset() noexcept;
    [[nodiscard]] bool Prepare(ID3D11Device* device) noexcept;
    [[nodiscard]] bool Draw(ID3D11Device* device, ID3D11DeviceContext* context,
        const runtime::StereoHudTextOverlay& overlay, std::uint64_t capture_sequence,
        const std::array<ID3D11Texture2D*, 2>& targets) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openvr
