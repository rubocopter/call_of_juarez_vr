#pragma once
#include <d3d9.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>

namespace cojvr::backends::d3d9 {
// Serializes observer admission/closure with native-call ingress/retirement.
// The mutex is never retained while forwarding a D3D9 method.
class RenderResourceCallBoundary final {
public:
    bool Begin(IDirect3DDevice9*) noexcept;
    void Enter(IDirect3DDevice9*) noexcept;
    void Leave() noexcept;
    bool End(IDirect3DDevice9*) noexcept;
private:
    std::mutex mutex_;
    IDirect3DDevice9* device_=nullptr;
    DWORD thread_=0;
    std::uint64_t in_flight_=0;
    bool faulted_=false;
};
enum class RenderResourceOperation : std::uint8_t {
    set_target,set_depth,set_texture,set_viewport,set_scissor,clear,
    draw,draw_indexed,draw_up,draw_indexed_up,stretch_rect,reset
};
struct RenderResourceEvent {
    RenderResourceOperation operation{};
    std::uint32_t selector=0;
    HRESULT result=S_OK;
    std::uintptr_t caller=0,source=0,destination=0;
    bool operator==(const RenderResourceEvent&) const = default;
};
// Values only: no retained COM ownership and no size/content admission.
struct D3D9RenderResourceInfo {
    std::uintptr_t object=0,parent=0,surface=0;
    std::uint32_t width=0,height=0,depth=0,levels=0,format=0,usage=0,pool=0;
    std::uint32_t samples=0,quality=0,type=0;
    bool bound=false,valid=false;
    bool operator==(const D3D9RenderResourceInfo&) const = default;
};
struct D3D9RenderResourceSnapshot {
    std::array<D3D9RenderResourceInfo,4> targets{};
    D3D9RenderResourceInfo depth{};
    // Pixel 0..15, then vertex samplers 257..260 when supported.
    std::array<D3D9RenderResourceInfo,20> textures{};
    std::array<DWORD,4> viewport{};
    std::array<float,2> viewport_depth{};
    std::array<LONG,4> scissor{};
    std::array<DWORD,3> z{};
    std::array<float,32> transforms{};
    std::uintptr_t vertex_shader=0,pixel_shader=0;
    DWORD scissor_enabled=0,target_slots=0,texture_slots=0;
    bool valid=false,transforms_valid=false;
    bool operator==(const D3D9RenderResourceSnapshot&) const = default;
};
[[nodiscard]] D3D9RenderResourceSnapshot ObserveD3D9RenderResources(IDirect3DDevice9*) noexcept;
struct D3D9RenderResourceContext {
    // Native device generation is distinct from generation's probe continuity
    // epoch, which also counts reset attempts. Zero means not supplied.
    std::uint64_t device_generation=0;
    std::uintptr_t device=0;
    std::uint64_t generation=0,frame=0,owner=0,scope=0;
    std::uint32_t thread=0,eye=0,route=0; // gameplay/settings/movie = 0/1/2
};
// Reserve two attempts for each gameplay/settings/movie route. Total stays six;
// long gameplay sessions cannot exhaust the other routes' observation window.
class RenderResourceRouteBudget {
public:
    [[nodiscard]] bool TryAcquire(unsigned route) noexcept {
        if(route>=attempts_.size())return false;
        auto attempt=attempts_[route].load(std::memory_order_relaxed);
        while(attempt<2) {
            if(attempts_[route].compare_exchange_weak(attempt,attempt+1,
                std::memory_order_relaxed))return true;
        }
        return false;
    }
private:
    std::array<std::atomic_uint,3> attempts_{};
};
struct D3D9RenderResourceRecord {
    RenderResourceEvent event{};
    D3D9RenderResourceSnapshot state{};
    std::uint64_t ordinal=0,repeat=1;
};
struct D3D9RenderResourceSummary {
    std::uint64_t calls=0,records=0,read_failures=0;
    bool complete=false,truncated=false;
};
using D3D9RenderResourceSink=void(*)(void*,const D3D9RenderResourceContext&,
    const D3D9RenderResourceRecord*,const D3D9RenderResourceSummary*) noexcept;
class D3D9RenderResourceProbe final {
public:
    static constexpr std::size_t max_records=4096;
    explicit D3D9RenderResourceProbe(D3D9RenderResourceSink sink,void* context=nullptr,
        std::size_t capacity=max_records) noexcept;
    [[nodiscard]] bool Begin(IDirect3DDevice9*,D3D9RenderResourceContext) noexcept;
    bool Record(IDirect3DDevice9*,const RenderResourceEvent&) noexcept;
    [[nodiscard]] bool End(IDirect3DDevice9*,bool complete) noexcept;
private:
    D3D9RenderResourceSink sink_=nullptr;void* sink_context_=nullptr;
    std::size_t capacity_=0;std::vector<D3D9RenderResourceRecord> records_;
    D3D9RenderResourceContext context_{};D3D9RenderResourceSummary summary_{};
    std::uint64_t next_scope_=0;
    std::atomic<DWORD> owner_thread_{0};
    std::atomic_bool faulted_{false};
    bool active_=false;
};
} // namespace cojvr::backends::d3d9
