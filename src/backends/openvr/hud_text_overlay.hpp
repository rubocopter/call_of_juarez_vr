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

enum class HudTextRasterStyle { standard, compact, subtitle };

// Cache owns GDI output only; rebuilding occurs when visible text changes.
class HudTextRaster {
public:
    [[nodiscard]] const HudTextPanel& Render(const runtime::HudText& text,
        HudTextRasterStyle style=HudTextRasterStyle::standard) noexcept;
    [[nodiscard]] std::uint64_t rebuilds() const noexcept { return rebuilds_; }
private:
    runtime::HudText text_{};
    HudTextPanel panel_{};
    std::uint64_t rebuilds_ = 0;
    HudTextRasterStyle style_ = HudTextRasterStyle::standard;
};

struct HudPanelPlacement {
    float width_m = 1.15F;
    float center_y_m = 0.0F;
    float distance_m = 1.5F;
    // Captured head-space corners for a tracked surface, in strip order.
    // Geometry belongs to the producing frame, never the latest presenter pose.
    bool captured_quad = false;
    std::array<runtime::Vec3, 4> head_corners{};
};

// A fixed lower edge keeps successive one/two-line captions from jumping.
[[nodiscard]] HudPanelPlacement BuildSubtitlePlacement(const HudTextPanel& subtitle) noexcept;

struct ProjectedHudPanel {
    std::uint32_t left = 0, top = 0, width = 0, height = 0;
    std::array<std::array<float, 4>, 4> clip_positions{};
};

struct HudSubtitleDiagnostic {
    std::uint64_t capture_sequence=0;
    std::uint32_t characters=0,panel_width=0,panel_height=0;
    HudPanelPlacement placement{};
    bool projection_attempted=false;
    std::array<bool,2> projected{},clipped{},offscreen{},draw_queued{};
    std::array<ProjectedHudPanel,2> quads{};
};

[[nodiscard]] bool BuildThreatPanelPlacement(const runtime::ThreatMarker& marker,
    HudPanelPlacement& result) noexcept;

// Homogeneous corners preserve perspective under cant and asymmetric FOV.
[[nodiscard]] bool ProjectHudPanel(const HudTextPanel& source,
    const runtime::EyeView& eye, std::uint32_t target_width,
    std::uint32_t target_height, HudPanelPlacement placement,
    ProjectedHudPanel& result, ProjectedHudPanel* outside_view=nullptr) noexcept;

// Timer, hint, notices; fit the complete measured stack to BOTH captured eyes.
[[nodiscard]] bool FitCriticalHudPanels(const std::array<const HudTextPanel*,3>& panels,
    const std::array<runtime::EyeView,2>& eyes,std::array<HudPanelPlacement,3>& result) noexcept;

// Operates exclusively on presenter-owned D3D11 resources/context.
class HudTextCompositor {
public:
    [[nodiscard]] HudSubtitleDiagnostic LastSubtitleDiagnostic() const noexcept;
    HudTextCompositor();
    ~HudTextCompositor();
    void Reset() noexcept;
    // Presenter-thread recenter, mode or device-owner transition.
    void ResetReadingAnchor() noexcept;
    [[nodiscard]] bool Prepare(ID3D11Device* device) noexcept;
    // Call when fresh world pixels replace the retained output, including
    // non-HUD frames, before composition/retirement. Failed no-write uploads
    // may retain identity. Drops identity without writes; backing stays cached.
    void DiscardRepeatedText() noexcept;
    // Call under presenter frame authority before every eye submission, even
    // repeats without Draw. Includes captured coaching-glyph lifetime/epoch;
    // expiry restores the complete controlled stack in reverse draw order.
    // Mismatched identities discard without restoring.
    void RetireStaleText(ID3D11Device* device, ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*, 2>& targets,
        bool show_holster_guides, std::uint64_t reload_owner_token,
        std::uint64_t tutorial_owner_token = 0) noexcept;
    // Public Draw is for a newly copied world frame. RetireStaleText may
    // internally redraw only its controlled layers after expiring icon art.
    // Reading panels use this frame's captured HMD pose, never the latest
    // runtime pose; repeated controlled-text refresh retains that same geometry.
    [[nodiscard]] bool Draw(ID3D11Device* device, ID3D11DeviceContext* context,
        const runtime::StereoHudTextOverlay& overlay, std::uint64_t capture_sequence,
        const std::array<ID3D11Texture2D*, 2>& targets,
        bool show_holster_guides = true, std::uint64_t reload_owner_token = 0,
        std::uint64_t bow_owner_token = 0, std::uint64_t tutorial_owner_token = 0,
        const runtime::Pose& render_hmd_pose = {}) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cojvr::backends::openvr
