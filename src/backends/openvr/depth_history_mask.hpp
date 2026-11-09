#pragma once
#include "backends/openvr/dlss_pair_session.hpp"
#include <d3d11_1.h>
#include <wrl/client.h>

namespace cojvr::backends::openvr::dlss {
struct DepthHistoryMaskSource {
    ID3D11Texture2D* depth=nullptr;          // Private R32 standard device Z.
    ID3D11Texture2D* previous_depth=nullptr; // Immutable previous eye, same extent.
    ID3D11Texture2D* exclusions=nullptr;     // Private R8 native dynamic/near-pass exclusions.
    std::uint64_t current_token=0,previous_token=0;
    runtime::TemporalMatrix clip_to_previous=runtime::TemporalIdentity();
    float jitter_x=0,jitter_y=0,previous_jitter_x=0,previous_jitter_y=0;
    float previous_near_metres=0,previous_far_metres=0;
    float relative_tolerance=0; // Explicit experiment policy, not game qualification.
    unsigned erosion_radius=0; // 0..2 pixels eroded from history-eligible regions.
    bool previous_infinite_far=false,source_qualified=false;
};

// Experimental, serialized worker-owned pass. Adjacent identities and capture
// tokens are caller evidence, never inferred from texture addresses. Caller
// retains immutable inputs through GPU retirement. Does not retain/advance
// history, qualify game motion or present images. Depth agreement cannot detect
// same-depth or transparent motion; explicit native exclusions remain required.
class DepthHistoryMask final {
    template<class T>using ComPtr=Microsoft::WRL::ComPtr<T>;
    ComPtr<ID3D11Device> owner_;
    ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11VertexShader> vertex_;
    ComPtr<ID3D11PixelShader> compare_,erode_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11BlendState> blend_;
    std::array<std::array<ComPtr<ID3D11Texture2D>,2>,2> textures_;
    std::array<std::array<ComPtr<ID3D11RenderTargetView>,2>,2> views_;
    std::array<ComPtr<ID3D11ShaderResourceView>,2> raw_;
    std::array<ID3D11Texture2D*,2> outputs_{};
    unsigned width_=0,height_=0;
    HRESULT result_=S_OK;
    bool Prepare(ID3D11Device*,unsigned,unsigned) noexcept;
public:
    bool Generate(ID3D11Device*,ID3D11DeviceContext*,
        const PairIdentity& current,const PairIdentity& previous,
        const std::array<DepthHistoryMaskSource,2>&) noexcept;
    [[nodiscard]] ID3D11Texture2D* EyeMask(unsigned eye) const noexcept {return eye<2?outputs_[eye]:nullptr;}
    [[nodiscard]] HRESULT last_result() const noexcept {return result_;}
    void Reset() noexcept;
};
}
