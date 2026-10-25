#pragma once
#include "backends/openvr/dlss_protocol.hpp"
#include <d3d11.h>
#include <filesystem>
#include <memory>
#include <string_view>

namespace cojvr::backends::openvr::dlss {
struct Inputs {
    ID3D11Texture2D* color=nullptr; // RGBA16F, LDR presentation color
    ID3D11Texture2D* depth=nullptr; // R32F, standard device Z, 0 near / 1 far
    ID3D11Texture2D* motion=nullptr;// RG16F, previous UV - current UV, unjittered
    ID3D11Texture2D* bias=nullptr;  // R8, current-color bias for unsupported motion
    bool reset=true;
    float jitter_x=0,jitter_y=0;
};
// Optional backend; no game data can be inferred from a successful evaluation.
// Owns only its helper, private shared inputs/output and their completion proof.
class Client final {
public:
    Client();~Client();
    Client(const Client&)=delete;Client& operator=(const Client&)=delete;
    [[nodiscard]] bool Start(ID3D11Device* device,const std::filesystem::path& helper) noexcept;
    [[nodiscard]] bool Build(unsigned eye,unsigned width,unsigned height,unsigned output_width,
                             unsigned output_height,Quality quality) noexcept;
    [[nodiscard]] bool Evaluate(ID3D11DeviceContext* context,unsigned eye,const Inputs& input) noexcept;
    [[nodiscard]] ID3D11Texture2D* Output(unsigned eye) const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] std::uint32_t last_ngx_result() const noexcept;
    [[nodiscard]] std::uint32_t helper_bits() const noexcept;
    void Stop() noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
