#pragma once

#include "backends/d3d9/sampled_depth_capture.hpp"
#include "backends/openvr/scene_depth_transfer.hpp"
#include "games/call_of_juarez/ppfx_resource_observation.hpp"
#include "games/call_of_juarez/temporal_source_probe.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <wrl/client.h>

namespace cojvr::games::call_of_juarez {

// Bounded developer probe. The adapter selects the exact scene's depth owner.
// An observed GPU snapshot is evidence for coverage review, never DLSS input admission.
class SampledSceneDepth final {
    using Capture = backends::d3d9::D3D9SampledDepthCapture;
    using Snapshot = backends::d3d9::D3D9SampledDepthSnapshot;
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    struct Scope {
        std::uint64_t frame = 0, generation = 0, integrity = 0, pose = 0;
        std::uintptr_t owner = 0, color = 0, depth = 0, terminal = 0;
        unsigned eye = 0, width = 0, height = 0;
        bool clear_started = false, initialized = false, rejected = false;
        Snapshot snapshot;
        std::chrono::steady_clock::time_point capture_time{};
        CoJNativeTemporalOptics optics{};
    } scope_;
    struct Pending {
        Snapshot snapshot;
        std::uint64_t frame = 0, pose = 0;
        std::uintptr_t owner = 0;
        unsigned eye = 0;
        std::uint64_t integrity = 0,epoch = 0;
        std::chrono::steady_clock::time_point capture_time{};
        CoJTemporalPreviewOptics optics{};
    };
    std::array<Pending, Capture::slot_count> pending_{};
    ComPtr<IDirect3DDevice9> device_;
    ComPtr<IDirect3DSurface9> original_depth_, replacement_;
    std::unique_ptr<Capture> capture_;
    std::uint64_t capture_generation_ = 0, sequence_ = 0;
    unsigned capture_width_ = 0, capture_height_ = 0;
    DWORD owner_thread_ = 0;
    bool private_call_ = false, rollback_pending_ = false, disabled_ = false;
    bool full_size_color_;
    std::uint64_t capture_epoch_ = 0;
    const char* last_error_ = "none";
    HRESULT last_result_ = S_OK;
    bool Reject(const char* reason, HRESULT result = E_FAIL) noexcept {
        last_error_ = reason;
        last_result_ = result;
        return false;
    }
    static bool Full(const RECT* rect, unsigned w, unsigned h) noexcept {
        return !rect || (rect->left == 0 && rect->top == 0 &&
            rect->right == static_cast<LONG>(w) && rect->bottom == static_cast<LONG>(h));
    }
    static bool Shape(IDirect3DSurface9* surface, D3DFORMAT format, unsigned w, unsigned h) noexcept {
        D3DSURFACE_DESC desc{};
        return surface && SUCCEEDED(surface->GetDesc(&desc)) && desc.Format == format &&
            desc.Width == w && desc.Height == h && desc.Pool == D3DPOOL_DEFAULT &&
            desc.MultiSampleType == D3DMULTISAMPLE_NONE && desc.MultiSampleQuality == 0;
    }
    struct PrivateCall {
        bool& flag;
        explicit PrivateCall(bool& value) noexcept : flag(value) { flag = true; }
        ~PrivateCall() { flag = false; }
    };
public:
    struct PreviewObservation {
        backends::openvr::dlss::SceneDepthStamp stamp{};
        unsigned eye=0;
        Snapshot snapshot;
        std::chrono::steady_clock::time_point capture_time{};
    };
    explicit SampledSceneDepth(bool full_size_color=false) noexcept:full_size_color_(full_size_color) {}
    // Startup-only, before this adapter has ever allocated a capture pool.
    [[nodiscard]] bool ConfigureFullSizeColor(bool enabled) noexcept {
        if(capture_epoch_||capture_||scope_.frame||owner_thread_)return false;
        full_size_color_=enabled;return true;
    }
    [[nodiscard]] bool rollback_pending() const noexcept { return rollback_pending_; }
    [[nodiscard]] const char* last_error() const noexcept { return last_error_; }
    [[nodiscard]] HRESULT last_result() const noexcept { return last_result_; }
    [[nodiscard]] bool Begin(IDirect3DDevice9* device, const CoJPostprocessResourceSnapshot& native,
        std::uint64_t generation, std::uint64_t integrity, std::uint64_t frame,
        std::uintptr_t owner, unsigned eye, std::uint64_t pose) noexcept {
        if (disabled_) return false; // Preserve the original initialization failure.
        if (rollback_pending_) return Reject("restore_pending");
        if (scope_.frame) return Reject("scope_busy");
        if (!device || !frame || !generation || !integrity || !owner || !pose || eye > 1)
            return Reject("invalid_capture_identity");
        if (!native.valid) return Reject("native_snapshot_invalid");
        if (native.device != reinterpret_cast<std::uintptr_t>(device)) return Reject("native_device_mismatch");
        if (owner_thread_ && owner_thread_ != GetCurrentThreadId()) return Reject("owner_thread_mismatch");
        const auto& renderer = native.renderer_state;
        if (!renderer.internal_color_surface || !renderer.internal_depth_surface) return Reject("scene_surfaces_unavailable");
        const CoJPostprocessTextureObservation* terminal = nullptr;
        for (std::size_t index = 0; index < native.buffer_count; ++index) {
            const auto& buffer = native.buffers[index];
            if (std::string_view(buffer.name.data()) != "1B_16") continue;
            if (terminal) return Reject("terminal_ambiguous");
            terminal = &buffer.texture;
        }
        if (!terminal || !terminal->texture_2d || !terminal->width || !terminal->height) return Reject("terminal_unavailable");
        if (capture_ && (device_.Get() != device || capture_generation_ != generation ||
            capture_width_ != terminal->width || capture_height_ != terminal->height)) {
            if (!Retire()) return Reject("retirement_pending");
        }
        if (!capture_) {
            try {
                auto candidate = std::make_unique<Capture>();
                const backends::d3d9::D3D9SampledDepthConfig config{terminal->width, terminal->height,
                    D3DFMT_A16B16G16R16F, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, generation,full_size_color_};
                const auto initialized = candidate->Initialize(device, config);
                if (FAILED(initialized)) {
                    disabled_ = true;
                    return Reject(candidate->last_error(), initialized);
                }
                if(capture_epoch_==std::numeric_limits<std::uint64_t>::max()) {
                    disabled_=true;return Reject("capture_epoch_exhausted");
                }
                ++capture_epoch_;
                capture_ = std::move(candidate);
                device_ = device;
                capture_generation_ = generation;
                capture_width_ = terminal->width;
                capture_height_ = terminal->height;
                owner_thread_ = GetCurrentThreadId();
            } catch (...) { disabled_ = true; return Reject("allocation_failed", E_OUTOFMEMORY); }
        }
        scope_ = {frame, generation, integrity, pose, owner, renderer.internal_color_surface,
            renderer.internal_depth_surface, terminal->texture_2d, eye, terminal->width, terminal->height};
        scope_.capture_time=std::chrono::steady_clock::now();
        last_error_ = "none";
        last_result_ = S_OK;
        return true;
    }
    void BeforeClear(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects,
        DWORD flags, float z) noexcept {
        if (private_call_ || GetCurrentThreadId() != owner_thread_ || device_.Get() != device || !scope_.frame || scope_.rejected ||
            !(flags & D3DCLEAR_ZBUFFER)) return;
        ComPtr<IDirect3DSurface9> color, depth;
        if (FAILED(device->GetRenderTarget(0, &color)) ||
            reinterpret_cast<std::uintptr_t>(color.Get()) != scope_.color ||
            FAILED(device->GetDepthStencilSurface(&depth))) return;
        if (scope_.clear_started) { scope_.rejected = true; return; }
        D3DVIEWPORT9 viewport{};
        DWORD stencil = TRUE;
        if (count || rects || flags != (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER) || z != 1.0F ||
            reinterpret_cast<std::uintptr_t>(depth.Get()) != scope_.depth ||
            !Shape(color.Get(), D3DFMT_A16B16G16R16F, scope_.width, scope_.height) ||
            !Shape(depth.Get(), D3DFMT_D24X8, scope_.width, scope_.height) ||
            FAILED(device->GetViewport(&viewport)) || viewport.X || viewport.Y ||
            viewport.Width != scope_.width || viewport.Height != scope_.height ||
            viewport.MinZ != 0.0F || viewport.MaxZ != 1.0F ||
            FAILED(device->GetRenderState(D3DRS_STENCILENABLE, &stencil)) || stencil) {
            scope_.rejected = true;
            (void)Reject("scene_clear_contract");
            return;
        }
        auto* replacement = capture_->native_depth_surface();
        if (!replacement) { scope_.rejected = true; return; }
        PrivateCall guard(private_call_);
        if (FAILED(device->SetDepthStencilSurface(replacement))) { scope_.rejected = true; return; }
        original_depth_ = std::move(depth);
        replacement_ = replacement;
        scope_.clear_started = true;
        rollback_pending_ = true;
    }
    void AfterClear(IDirect3DDevice9* device, HRESULT result) noexcept {
        if (private_call_ || device_.Get() != device || !scope_.clear_started || scope_.initialized || scope_.rejected) return;
        if (SUCCEEDED(result)) scope_.initialized = true;
        else { scope_.rejected = true; (void)Restore(); }
    }
    [[nodiscard]] IDirect3DSurface9* MapDepth(IDirect3DDevice9* device, IDirect3DSurface9* surface) noexcept {
        return !private_call_ && GetCurrentThreadId() == owner_thread_ && scope_.frame && scope_.initialized && rollback_pending_ &&
            device_.Get() == device && surface == original_depth_.Get() ? replacement_.Get() : surface;
    }
    void AfterCopy(IDirect3DDevice9* device, IDirect3DSurface9* source, const RECT* source_rect,
        IDirect3DSurface9* destination, const RECT* destination_rect, HRESULT result,
        CoJNativeTemporalOptics (*read_optics)(unsigned,std::uint64_t,std::uint64_t) noexcept=nullptr) noexcept {
        if (private_call_ || GetCurrentThreadId() != owner_thread_ || device_.Get() != device || !scope_.initialized || scope_.rejected ||
            FAILED(result) || reinterpret_cast<std::uintptr_t>(source) != scope_.color || !destination) return;
        ComPtr<IDirect3DTexture9> container;
        if (FAILED(destination->GetContainer(IID_PPV_ARGS(&container))) ||
            reinterpret_cast<std::uintptr_t>(container.Get()) != scope_.terminal) return;
        ComPtr<IDirect3DSurface9> depth, current_color;
        DWORD stencil = TRUE;
        if (scope_.snapshot.valid() || !Full(source_rect, scope_.width, scope_.height) ||
            !Full(destination_rect, scope_.width, scope_.height) ||
            !Shape(destination, D3DFMT_A16B16G16R16F, scope_.width, scope_.height) ||
            FAILED(device->GetRenderTarget(0, &current_color)) || current_color.Get() != source ||
            FAILED(device->GetDepthStencilSurface(&depth)) || depth.Get() != replacement_.Get() ||
            FAILED(device->GetRenderState(D3DRS_STENCILENABLE, &stencil)) || stencil) {
            scope_.rejected = true;
            return;
        }
        PrivateCall guard(private_call_);
        if (FAILED(capture_->Snapshot(++sequence_, scope_.generation, scope_.snapshot, true))) {
            scope_.rejected = true;
            // Repair any private sampling state while still at the capture boundary.
            if (FAILED(capture_->RetryRestore())) disabled_ = true;
        } else if (read_optics) scope_.optics=read_optics(scope_.eye,scope_.frame,scope_.pose);
    }
    [[nodiscard]] bool End(std::uint64_t frame, unsigned eye, std::uint64_t generation,
                           std::uint64_t integrity, std::uintptr_t owner, bool complete, bool& queued) noexcept {
        queued = false;
        const bool publish = complete && !scope_.rejected && scope_.frame == frame && scope_.eye == eye &&
            scope_.generation == generation && scope_.integrity == integrity && scope_.owner == owner && scope_.snapshot.valid();
        if (!publish && last_result_ == S_OK)
            (void)Reject(scope_.rejected ? "native_capture_rejected" :
                !complete ? "native_scope_incomplete" :
                !scope_.clear_started ? "scene_clear_missing" :
                !scope_.initialized ? "scene_clear_failed" :
                !scope_.snapshot.valid() ? "terminal_copy_missing" : "scope_identity_changed");
        const bool restored = Restore();
        if (!restored) (void)Reject("native_depth_restore_pending");
        if (publish && restored) {
            for (auto& item : pending_) if (!item.snapshot.valid()) {
                item = {std::move(scope_.snapshot), scope_.frame, scope_.pose, scope_.owner, scope_.eye,
                    scope_.integrity,capture_epoch_,scope_.capture_time,{scope_.optics,false}};
                queued = true;
                break;
            }
        }
        if (publish && restored && !queued) (void)Reject("pending_capture_slots_busy");
        scope_ = {};
        return restored;
    }
    [[nodiscard]] bool Restore() noexcept {
        if (!rollback_pending_) return true;
        if (GetCurrentThreadId() != owner_thread_) return false;
        PrivateCall guard(private_call_);
        ComPtr<IDirect3DSurface9> current;
        if (FAILED(capture_->RetryRestore())) return false;
        const auto result = device_->GetDepthStencilSurface(&current);
        if (FAILED(result) && result != D3DERR_NOTFOUND) return false;
        if (current.Get() == replacement_.Get()) {
            if (FAILED(device_->SetDepthStencilSurface(original_depth_.Get()))) return false;
            current.Reset();
            if (FAILED(device_->GetDepthStencilSurface(&current)) || current.Get() != original_depth_.Get()) return false;
        }
        replacement_.Reset();
        original_depth_.Reset();
        rollback_pending_ = false;
        return true;
    }
    [[nodiscard]] HRESULT CaptureFinalColor(IDirect3DDevice9* device, IDirect3DSurface9* source,
        std::uint64_t generation, std::uint64_t frame, unsigned eye, std::uint64_t pose,
        const CoJNativeTemporalOptics& optics={}) noexcept {
        if (owner_thread_ != GetCurrentThreadId() || !device || device_.Get() != device ||
            !source || rollback_pending_ || generation != capture_generation_)
            return S_FALSE;
        for (auto& item : pending_) {
            if (!item.snapshot.valid() || item.frame != frame || item.eye != eye || item.pose != pose)
                continue;
            PrivateCall guard(private_call_);
            const auto result=item.snapshot.AttachFinalColorPreview(source);
            if (result==S_OK) {
                item.optics.final_color_optics_valid=optics.valid;
                item.optics.final_color_match=optics.valid && item.optics.depth==optics;
            }
            return result;
        }
        return S_FALSE;
    }
    template<class Report> void Poll(Report&& report) {
        PollItems([&](const Pending& item,HRESULT result) {
            report(item.frame,item.pose,item.owner,item.eye,item.snapshot,result);
        });
    }
    // The source adapter authors these stamps from its exact admitted scope.
    // Epoch tracks capture-resource lifetime, not input/settings/history policy.
    template<class Report> void PollPackets(Report&& report) {
        if(!full_size_color_)return;
        PollPreviews([&](const PreviewObservation& observation,HRESULT result,const auto& optics) {
            const backends::openvr::dlss::SceneDepthEye packet{
                observation.stamp,observation.eye,observation.snapshot,observation.capture_time};
            report(packet,result,optics);
        });
    }
    // Reduced color/depth exports need no full-size color carrier. Keep these
    // observations distinct from worker packets that require both shared planes.
    template<class Report> void PollPreviews(Report&& report) {
        PollItems([&](const Pending& item,HRESULT result) {
            const PreviewObservation observation{
                {item.snapshot.producer_device(),item.snapshot.generation(),item.frame,item.pose,
                 item.owner,item.integrity,item.epoch},item.eye,item.snapshot,item.capture_time};
            report(observation,result,item.optics);
        });
    }
private:
    template<class Report> void PollItems(Report&& report) {
        if (owner_thread_ != GetCurrentThreadId()) return;
        for (auto& item : pending_) {
            if (!item.snapshot.valid()) continue;
            const auto result = item.snapshot.PollReady();
            if (result == S_FALSE) continue;
            report(item,result);
            item = {};
        }
    }
public:
    [[nodiscard]] bool Retire() noexcept {
        if (owner_thread_ && owner_thread_ != GetCurrentThreadId()) return false;
        if (!Restore()) return false;
        scope_ = {};
        pending_ = {};
        capture_.reset();
        device_.Reset();
        owner_thread_ = 0;
        disabled_ = false;
        return true;
    }
};
}
