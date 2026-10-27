#pragma once
#include "runtime/graphics_settings.hpp"
#include "runtime/comfort_view.hpp"
#include "runtime/scope_view.hpp"
#include <array>
#include <cstdint>
#include <d3d11_1.h>
#include <wrl/client.h>

namespace cojvr::backends::openvr {
// Spatial, source-sized processing only. No history, pose change, upscaling or
// game-owned D3D9 resource mutation. Call once per fresh native stereo capture,
// before overlay composition, on the presenter-owned immediate context.
class SpatialImageProcessor final {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<ID3D11Device> owner_;
    ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11VertexShader> vertex_;
    ComPtr<ID3D11PixelShader> pixel_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11DepthStencilState> depth_;
    ComPtr<ID3D11BlendState> blend_;
    ComPtr<ID3D11Texture2D> scratch_;
    ComPtr<ID3D11ShaderResourceView> source_;
    std::array<ComPtr<ID3D11Texture2D>,2> targets_;
    std::array<ComPtr<ID3D11RenderTargetView>,2> views_;
    std::uint32_t width_=0,height_=0,allocations_=0;
    DXGI_FORMAT format_=DXGI_FORMAT_UNKNOWN;
    HRESULT result_=S_OK;
    double queue_ms_=0;
    bool Check(HRESULT result) noexcept {result_=result;return SUCCEEDED(result);}
    bool Prepare(ID3D11Device* device,const std::array<ID3D11Texture2D*,2>& eyes,
                 const D3D11_TEXTURE2D_DESC& description) noexcept;
public:
    // Scope lenses are projected through physical per-eye optics by the caller,
    // which admits current captured-eye ownership. Null lenses retain the legacy
    // centered native-pixel circle without a sight for existing GPU fixtures.
    // Nonzero comfort dimming outside a scope requires the complete physical
    // eye pair. Missing/invalid optics reject before allocations or eye writes.
    [[nodiscard]] bool Process(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& eyes,const runtime::GraphicsSettings& settings,
        float peripheral_dimming=0,bool scope_view=false,
        const std::array<runtime::ScopeLens,2>* scope_lenses=nullptr,bool scope_no_shoot=false,
        const std::array<runtime::EyeView,2>* comfort_eyes=nullptr) noexcept;
    void Reset() noexcept;
    [[nodiscard]] HRESULT last_result() const noexcept {return result_;}
    [[nodiscard]] double last_queue_ms() const noexcept {return queue_ms_;}
    [[nodiscard]] std::uint32_t scratch_allocations() const noexcept {return allocations_;}
};
}
