#pragma once
#include "backends/openvr/dlss_client.hpp"
#include "runtime/temporal_reconstruction.hpp"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>

namespace cojvr::backends::openvr::dlss {
struct GuideSource {
    ID3D11Texture2D* color=nullptr;
    ID3D11Texture2D* linear_depth_alpha=nullptr; // RGBA16F, alpha is positive axial metres
    ID3D11Texture2D* current_color_mask=nullptr; // R8; dynamic/near-pass exclusions, not an invented depth mask
    runtime::TemporalMatrix clip_to_previous=runtime::TemporalIdentity();
    float near_metres=0,far_metres=0,jitter_x=0,jitter_y=0;
    bool history=false;
    // Owning game adapter must qualify actual source semantics. Format/size,
    // source-level USE_WRITEZ or a synthetic test alone cannot set this in CoJ.
    bool source_qualified=false;
};
class TemporalGuides final {
    template<class T>using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<ID3D11Device> owner_;
    ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11VertexShader> vertex_;
    ComPtr<ID3D11PixelShader> pixel_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11BlendState> blend_;
    std::array<std::array<ComPtr<ID3D11Texture2D>,4>,2> textures_;
    std::array<std::array<ComPtr<ID3D11RenderTargetView>,4>,2> views_;
    unsigned width_=0,height_=0;
    HRESULT result_=S_OK;
    bool Prepare(ID3D11Device*,unsigned,unsigned) noexcept;
public:
    bool Generate(ID3D11Device*,ID3D11DeviceContext*,const std::array<GuideSource,2>&) noexcept;
    [[nodiscard]] Inputs EyeInputs(unsigned eye) const noexcept;
    void Reset() noexcept;
    [[nodiscard]] HRESULT last_result() const noexcept{return result_;}
private:
    std::array<Inputs,2> outputs_{};
};
}
