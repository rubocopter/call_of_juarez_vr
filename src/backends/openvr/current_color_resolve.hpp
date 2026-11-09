#pragma once
#include <d3d11_1.h>
#include <wrl/client.h>

namespace cojvr::backends::openvr::dlss {

// Runs on the helper's serialized immediate context. Its caller fences the
// resolve with the NGX evaluation before acknowledging the shared output.
class CurrentColorResolve final {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
public:
    class Targets {
        friend class CurrentColorResolve;
        ComPtr<ID3D11Device> owner_;
        ComPtr<ID3D11ShaderResourceView> color_,mask_;
        ComPtr<ID3D11UnorderedAccessView> output_;
        UINT in_width_=0,in_height_=0,out_width_=0,out_height_=0;
    };
    [[nodiscard]] bool Bind(ID3D11Device*,ID3D11Texture2D* color,ID3D11Texture2D* mask,
                            ID3D11Texture2D* output,Targets&) noexcept;
    // Borrowed scratch for this evaluation. Both eyes reuse it after the
    // preceding evaluation and resolve have retired on the helper context.
    [[nodiscard]] ID3D11Texture2D* Prepare(ID3D11Device*,UINT width,UINT height) noexcept;
    [[nodiscard]] bool Compose(ID3D11DeviceContext*,const Targets&,float jitter_x,float jitter_y) noexcept;
private:
    ComPtr<ID3D11Device> owner_;
    ComPtr<ID3DDeviceContextState> state_;
    ComPtr<ID3D11ComputeShader> shader_;
    ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11Texture2D> scratch_;
    ComPtr<ID3D11ShaderResourceView> reconstructed_;
    UINT width_=0,height_=0;
};
}
