#pragma once
#include "backends/openvr/neural_candidate.hpp"
#include "backends/d3d9/stereo_frame.hpp"
#include <d3d11.h>

namespace cojvr::backends::openvr::neural {
class Presenter final {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit Presenter(Options options);
    ~Presenter();
    Presenter(const Presenter&)=delete;Presenter& operator=(const Presenter&)=delete;
    void Toggle() noexcept;
    void Invalidate() noexcept;
    void ObserveContext(bool available) noexcept;
    void SetRequested(bool requested,std::uint64_t revision) noexcept;
    bool busy() const noexcept;
    bool Pump(ID3D11Device*,ID3D11DeviceContext*,const std::array<ID3D11Texture2D*,2>&,
              std::uint64_t active_frame,std::uint64_t device_id,std::uint64_t generation,bool allowed) noexcept;
    d3d9::StereoCpuFrame& RetainedFrame() noexcept;
    std::uint64_t applied_pairs() const noexcept;
    // Optional effect; false always means caller retains the complete native
    // pair. Device removal must still be checked by the caller before submit.
    bool Process(ID3D11Device*,ID3D11DeviceContext*,const std::array<ID3D11Texture2D*,2>&,
                 const d3d9::StereoCpuFrame&,const std::array<runtime::EyeView,2>&,
                 std::uint64_t revision,bool allowed) noexcept;
    const char* status() const noexcept;
    bool enabled() const noexcept;
};
Options LoadGameOptions() noexcept;
}
