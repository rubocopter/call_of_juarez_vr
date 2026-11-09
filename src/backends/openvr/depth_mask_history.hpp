#pragma once
#include "backends/openvr/depth_history_mask.hpp"

namespace cojvr::backends::openvr::dlss {
// Serialized immediate-context owner. Prepare publishes two masks, but does
// not advance history. Commit belongs after BOTH inference outputs complete.
// Copies and later reads stay ordered on that same device's immediate context.
// No source pointer survives Commit/Invalidate; only private depth and values do.
class DepthMaskHistory final {
    template<class T>using ComPtr=Microsoft::WRL::ComPtr<T>;
    struct Eye {
        runtime::TemporalMatrix matrix{};
        std::uint64_t token=0;
        float near_metres=0,far_metres=0,jitter_x=0,jitter_y=0;
    };
    struct Snapshot {PairIdentity identity{};DepthMaskPolicy policy{};std::array<Eye,2> eyes{};};
    ComPtr<ID3D11Device> owner_;
    std::array<ComPtr<ID3D11Texture2D>,2> depths_,white_,pending_depths_;
    std::array<ComPtr<ID3D11RenderTargetView>,2> white_views_;
    DepthHistoryMask mask_;
    Snapshot previous_{},pending_{};
    unsigned width_=0,height_=0;
    bool have_previous_=false,have_pending_=false;
    HRESULT result_=S_OK;
    bool Resources(ID3D11Device*,unsigned,unsigned) noexcept;
public:
    bool Prepare(ID3D11Device*,ID3D11DeviceContext*,const PairFrame&,
        const std::array<Inputs,2>&,std::array<ID3D11Texture2D*,2>& masks) noexcept;
    bool Commit(ID3D11DeviceContext*) noexcept;
    void Invalidate() noexcept;
    void Reset() noexcept;
    [[nodiscard]] HRESULT last_result() const noexcept {return result_;}
};
}
