#pragma once
#include "games/call_of_juarez/native_render_transaction.hpp"
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include "runtime/vr_types.hpp"
#include <d3d9.h>
#include <memory>
#include <string_view>

namespace cojvr::games::call_of_juarez {
struct CoJIndependentRenderEvidence {
    std::uint64_t draws=0,full_extent_depth_draws=0;
    std::uint32_t color_width=0,color_height=0,depth_width=0,depth_height=0;
    std::uint32_t viewport_width=0,viewport_height=0;
};
// Exact CoJ render-thread adapter. Only Begin inside an admitted natural full
// render-view owner. Capture must finish queuing its copy before End.
class CoJIndependentRenderScope final {
    struct Impl;
public:
    explicit CoJIndependentRenderScope(CoJRenderMemory memory={});
    ~CoJIndependentRenderScope();
    CoJIndependentRenderScope(const CoJIndependentRenderScope&)=delete;
    CoJIndependentRenderScope& operator=(const CoJIndependentRenderScope&)=delete;
    [[nodiscard]] bool Begin(IDirect3DDevice9*,std::uint64_t generation,
        const CoJPostprocessResourceSnapshot&,std::uint32_t height,runtime::Eye) noexcept;
    [[nodiscard]] bool End() noexcept;
    [[nodiscard]] bool Mirror() noexcept;
    [[nodiscard]] bool ClearHistories() noexcept;
    [[nodiscard]] bool InvalidateBeforeReset() noexcept;
    [[nodiscard]] bool Shutdown() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool restoration_pending() const noexcept;
    [[nodiscard]] std::uint64_t payload_bytes() const noexcept;
    void RecordNativeDraw(IDirect3DDevice9*,std::uintptr_t caller) noexcept;
    [[nodiscard]] CoJIndependentRenderEvidence evidence() const noexcept;
    [[nodiscard]] std::string_view last_error() const noexcept;
private:
    std::unique_ptr<Impl> impl_;
};
}
