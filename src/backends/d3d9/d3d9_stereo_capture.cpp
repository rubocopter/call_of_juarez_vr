#include "backends/d3d9/d3d9_stereo_capture.hpp"

#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace cojvr::backends::d3d9 {
namespace {

using Microsoft::WRL::ComPtr;
constexpr D3DFORMAT kD3dFormatNull = static_cast<D3DFORMAT>(0x4C4C554E); // 'NULL'
constexpr std::size_t kCaptureRingSize = 3;

std::size_t EyeIndex(const runtime::Eye eye) noexcept {
    return eye == runtime::Eye::left ? 0U : 1U;
}

bool SupportedFormat(const D3DFORMAT format) noexcept {
    return format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8;
}

bool SameDescription(const D3DSURFACE_DESC& lhs, const D3DSURFACE_DESC& rhs) noexcept {
    return lhs.Width == rhs.Width && lhs.Height == rhs.Height &&
        lhs.Format == rhs.Format &&
        lhs.MultiSampleType == rhs.MultiSampleType &&
        lhs.MultiSampleQuality == rhs.MultiSampleQuality;
}

std::string Failure(const char* stage, const HRESULT result) {
    std::ostringstream out;
    out << stage << " failed with HRESULT 0x" << std::hex
        << static_cast<unsigned long>(result);
    return out.str();
}

double MillisecondsBetween(
    const std::chrono::steady_clock::time_point begin,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

struct D3D9StereoCapture::Impl {
    enum class TransportMode : std::uint8_t {
        classic_cpu_fallback,
        d3d9ex_shared_texture,
    };

    struct Slot {
        std::array<ComPtr<IDirect3DTexture9>, 2> gpu_textures{};
        std::array<ComPtr<IDirect3DSurface9>, 2> gpu_eyes{};
        std::array<ComPtr<IDirect3DSurface9>, 2> system_eyes{};
        std::array<bool, 2> system_eye_locked{};
        std::array<std::uintptr_t, 2> shared_handles{};
        ComPtr<IDirect3DQuery9> fence;
        std::shared_ptr<ProducerFrameReleaseState> release_state{};
        std::array<bool, 2> eye_captured{};
        std::uint64_t frame_sequence = 0;
        std::uint64_t render_pose_sequence = 0;
        runtime::Pose render_hmd_pose{};
        std::chrono::steady_clock::time_point capture_time{};
        bool pending = false;
        bool fence_flush_issued = false;

        void ResetState() noexcept {
            eye_captured = {};
            frame_sequence = 0;
            render_pose_sequence = 0;
            render_hmd_pose = {};
            capture_time = {};
            pending = false;
            fence_flush_issued = false;
            release_state.reset();
        }
    };

    std::array<Slot, kCaptureRingSize> slots{};
    ComPtr<IDirect3DDevice9> resource_device;
    D3DSURFACE_DESC source_desc{};
    std::uintptr_t device_id = 0;
    std::uint64_t generation = 0;
    std::size_t current_slot = kCaptureRingSize;
    std::uint64_t current_frame = 0;
    D3D9StereoCaptureStats stats{};
    std::string last_error;
    std::string capture_description;
    std::string collect_description;
    std::string fallback_reason;
    TransportMode transport_mode = TransportMode::classic_cpu_fallback;
    bool resources_initialized = false;

    [[nodiscard]] std::uint32_t RingDepth() const noexcept {
        std::uint32_t depth = 0;
        for (const auto& slot : slots) {
            if (slot.pending || slot.release_state) ++depth;
        }
        return depth;
    }

    void UpdateRingDepth() noexcept {
        stats.ring_depth = RingDepth();
        stats.ring_depth_peak = std::max(stats.ring_depth_peak, stats.ring_depth);
    }

    void ReclaimConsumerSlots() noexcept {
        for (auto& slot : slots) {
            if (slot.release_state &&
                slot.release_state->consumer_done.load(std::memory_order_acquire)) {
                for (std::size_t eye = 0; eye < slot.system_eyes.size(); ++eye) {
                    if (slot.system_eye_locked[eye] && slot.system_eyes[eye]) {
                        const HRESULT unlock = slot.system_eyes[eye]->UnlockRect();
                        if (FAILED(unlock)) {
                            last_error = Failure("capture ring deferred UnlockRect", unlock);
                            ++stats.frames_invalidated;
                        }
                    }
                    slot.system_eye_locked[eye] = false;
                }
                slot.ResetState();
                ++stats.consumer_releases;
            }
        }
        UpdateRingDepth();
    }

    void ReleaseResources() noexcept {
        ReclaimConsumerSlots();
        for (auto& slot : slots) {
            for (std::size_t eye = 0; eye < slot.system_eyes.size(); ++eye) {
                if (slot.system_eye_locked[eye] && slot.system_eyes[eye]) {
                    (void)slot.system_eyes[eye]->UnlockRect();
                }
                slot.system_eye_locked[eye] = false;
                slot.system_eyes[eye].Reset();
            }
            for (auto& texture : slot.gpu_textures) texture.Reset();
            for (auto& surface : slot.gpu_eyes) surface.Reset();
            slot.shared_handles = {};
            slot.fence.Reset();
            if (slot.pending || slot.release_state) ++stats.frames_invalidated;
            slot.ResetState();
        }
        resource_device.Reset();
        source_desc = {};
        device_id = 0;
        generation = 0;
        current_slot = kCaptureRingSize;
        current_frame = 0;
        fallback_reason.clear();
        transport_mode = TransportMode::classic_cpu_fallback;
        resources_initialized = false;
    }

    bool EnsureResources(
        IDirect3DDevice9* device,
        IDirect3DSurface9* source,
        const std::uint64_t resource_generation) noexcept {
        D3DSURFACE_DESC desc{};
        HRESULT hr = source->GetDesc(&desc);
        if (FAILED(hr)) {
            last_error = Failure("capture surface GetDesc", hr);
            return false;
        }
        if (desc.Format == kD3dFormatNull) {
            last_error = "capture source is D3DFMT_NULL auxiliary render target";
            return false;
        }
        if (!SupportedFormat(desc.Format)) {
            last_error = "unsupported D3D9 capture-surface format " +
                std::to_string(static_cast<unsigned>(desc.Format));
            return false;
        }
        if (resources_initialized && resource_device.Get() == device &&
            generation == resource_generation && SameDescription(desc, source_desc)) {
            return true;
        }

        ReclaimConsumerSlots();
        for (const auto& slot : slots) {
            if (slot.release_state) {
                last_error =
                    "capture resource transition deferred while consumer owns a producer slot";
                UpdateRingDepth();
                return false;
            }
        }
        ReleaseResources();

        ComPtr<IDirect3DDevice9Ex> ex_device;
        const bool is_d3d9ex = SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&ex_device))) &&
            ex_device;
        bool shared_ready = is_d3d9ex;
        HRESULT shared_failure = S_OK;
        if (shared_ready) {
            for (auto& slot : slots) {
                for (std::size_t eye = 0; eye < slot.gpu_eyes.size(); ++eye) {
                    HANDLE shared_handle = nullptr;
                    hr = device->CreateTexture(
                        desc.Width,
                        desc.Height,
                        1,
                        D3DUSAGE_RENDERTARGET,
                        desc.Format,
                        D3DPOOL_DEFAULT,
                        &slot.gpu_textures[eye],
                        &shared_handle);
                    if (FAILED(hr) || !slot.gpu_textures[eye] || !shared_handle) {
                        shared_failure = FAILED(hr) ? hr : E_FAIL;
                        shared_ready = false;
                        break;
                    }
                    hr = slot.gpu_textures[eye]->GetSurfaceLevel(0, &slot.gpu_eyes[eye]);
                    if (FAILED(hr) || !slot.gpu_eyes[eye]) {
                        shared_failure = FAILED(hr) ? hr : E_FAIL;
                        shared_ready = false;
                        break;
                    }
                    slot.shared_handles[eye] =
                        reinterpret_cast<std::uintptr_t>(shared_handle);
                }
                if (!shared_ready) break;
            }
        }

        if (!shared_ready) {
            for (auto& slot : slots) {
                for (auto& texture : slot.gpu_textures) texture.Reset();
                for (auto& surface : slot.gpu_eyes) surface.Reset();
                slot.shared_handles = {};
            }
            fallback_reason = !is_d3d9ex
                ? "classic_d3d9_device_has_no_shared_resource_interop"
                : Failure("D3D9Ex shared render-target texture creation", shared_failure);
            ++stats.fallback_activations;
            for (auto& slot : slots) {
                for (std::size_t eye = 0; eye < slot.gpu_eyes.size(); ++eye) {
                    auto& surface = slot.gpu_eyes[eye];
                    hr = device->CreateRenderTarget(
                        desc.Width,
                        desc.Height,
                        desc.Format,
                        D3DMULTISAMPLE_NONE,
                        0,
                        FALSE,
                        &surface,
                        nullptr);
                    if (FAILED(hr) || !surface) {
                        last_error = Failure("capture ring render-target creation", hr);
                        ReleaseResources();
                        return false;
                    }
                    hr = device->CreateOffscreenPlainSurface(
                        desc.Width,
                        desc.Height,
                        desc.Format,
                        D3DPOOL_SYSTEMMEM,
                        &slot.system_eyes[eye],
                        nullptr);
                    if (FAILED(hr) || !slot.system_eyes[eye]) {
                        last_error = Failure(
                            "capture ring system-memory surface creation", hr);
                        ReleaseResources();
                        return false;
                    }
                }
            }
            transport_mode = TransportMode::classic_cpu_fallback;
        } else {
            transport_mode = TransportMode::d3d9ex_shared_texture;
        }

        for (auto& slot : slots) {
            hr = device->CreateQuery(D3DQUERYTYPE_EVENT, &slot.fence);
            if (FAILED(hr) || !slot.fence) {
                last_error = Failure("capture ring event-query creation", hr);
                ReleaseResources();
                return false;
            }
        }

        source_desc = desc;
        resource_device = device; // ComPtr takes ownership (AddRef)
        device_id = reinterpret_cast<std::uintptr_t>(device);
        generation = resource_generation;
        resources_initialized = true;
        return true;
    }

    bool AcquireSlot(const std::uint64_t frame_sequence) noexcept {
        if (current_frame == frame_sequence && current_slot < slots.size()) return true;
        ReclaimConsumerSlots();
        current_frame = 0;
        current_slot = slots.size();
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (!slots[index].pending && !slots[index].release_state &&
                slots[index].frame_sequence == 0) {
                current_slot = index;
                current_frame = frame_sequence;
                slots[index].frame_sequence = frame_sequence;
                slots[index].capture_time = std::chrono::steady_clock::now();
                slots[index].eye_captured = {};
                return true;
            }
        }
        ++stats.frames_dropped_no_slot;
        UpdateRingDepth();
        last_error = "capture ring saturated; no completed slot is available";
        return false;
    }

    Slot* OldestPending() noexcept {
        Slot* oldest = nullptr;
        std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max();
        for (auto& slot : slots) {
            if (slot.pending && slot.frame_sequence < sequence) {
                oldest = &slot;
                sequence = slot.frame_sequence;
            }
        }
        return oldest;
    }

};

D3D9StereoCapture::D3D9StereoCapture() : impl_(std::make_unique<Impl>()) {}
D3D9StereoCapture::~D3D9StereoCapture() = default;

bool D3D9StereoCapture::CaptureEye(
    IDirect3DDevice9* device,
    const runtime::Eye eye,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation) noexcept {
    if (!device) {
        impl_->last_error = "D3D9 stereo capture requires device, frame and generation";
        return false;
    }
    try {
        ComPtr<IDirect3DSurface9> source;
        const HRESULT hr = device->GetRenderTarget(0, &source);
        if (FAILED(hr) || !source) {
            impl_->last_error = Failure("GetRenderTarget(0)", FAILED(hr) ? hr : E_FAIL);
            return false;
        }
        return CaptureEyeSurface(device, source.Get(), eye, frame_sequence, generation);
    } catch (...) {
        impl_->last_error = "D3D9 stereo eye capture raised an exception";
        return false;
    }
}

bool D3D9StereoCapture::CaptureEyeSurface(
    IDirect3DDevice9* device,
    IDirect3DSurface9* source_surface,
    const runtime::Eye eye,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation) noexcept {
    if (!device || !source_surface || frame_sequence == 0 || generation == 0) {
        impl_->last_error = "D3D9 stereo capture requires device, frame and generation";
        return false;
    }
    try {
        const auto begin = std::chrono::steady_clock::now();
        impl_->last_error.clear();
        if (!impl_->EnsureResources(device, source_surface, generation) ||
            !impl_->AcquireSlot(frame_sequence)) {
            return false;
        }

        ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        const bool source_is_backbuffer = SUCCEEDED(back_buffer_result) && back_buffer &&
            source_surface == back_buffer.Get();
        D3DVIEWPORT9 viewport{};
        const bool viewport_valid = SUCCEEDED(device->GetViewport(&viewport));

        Impl::Slot& slot = impl_->slots[impl_->current_slot];
        const std::size_t eye_index = EyeIndex(eye);
        HRESULT hr = device->StretchRect(
            source_surface, nullptr, slot.gpu_eyes[eye_index].Get(), nullptr, D3DTEXF_NONE);
        if (FAILED(hr)) {
            impl_->last_error = Failure("stereo capture GPU copy StretchRect", hr);
            slot.ResetState();
            impl_->current_slot = impl_->slots.size();
            impl_->current_frame = 0;
            return false;
        }
        slot.eye_captured[eye_index] = true;
        const auto end = std::chrono::steady_clock::now();

        std::ostringstream out;
        out << "transport="
            << (impl_->transport_mode == Impl::TransportMode::d3d9ex_shared_texture
                    ? "d3d9ex_shared_texture_ring"
                    : "classic_d3d9_locked_systemmem_fallback")
            << ";capture_source=render_target0"
            << ";source_is_backbuffer=" << (source_is_backbuffer ? "true" : "false")
            << ";eye_surface=" << impl_->source_desc.Width << 'x' << impl_->source_desc.Height
            << ";viewport=";
        if (viewport_valid) {
            out << viewport.X << ',' << viewport.Y << ',' << viewport.Width << ','
                << viewport.Height << ',' << viewport.MinZ << ',' << viewport.MaxZ;
        } else {
            out << "unavailable";
        }
        out << ";format=" << static_cast<unsigned>(impl_->source_desc.Format)
            << ";source_pool=" << static_cast<unsigned>(impl_->source_desc.Pool)
            << ";source_msaa=" << static_cast<unsigned>(impl_->source_desc.MultiSampleType)
            << ";capture_pool=default"
            << ";capture_usage=render_target"
            << ";capture_msaa=0"
            << ";ring_slots=" << impl_->slots.size()
            << ";ring_depth=" << impl_->stats.ring_depth;
        if (!impl_->fallback_reason.empty()) {
            out << ";fallback_reason=" << impl_->fallback_reason;
        }
        out
            << std::fixed << std::setprecision(3)
            << ";gpu_copy_queue_ms=" << MillisecondsBetween(begin, end);
        impl_->capture_description = out.str();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 stereo eye capture raised an exception";
        return false;
    }
}

bool D3D9StereoCapture::CaptureFlatFrameImmediate(
    IDirect3DDevice9* device,
    IDirect3DSurface9* source_surface,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation,
    const runtime::Pose& render_hmd_pose,
    const std::uint64_t render_pose_sequence,
    StereoCpuFrame& frame) noexcept {
    frame = {};
    if (!device || !source_surface || frame_sequence == 0 || generation == 0 ||
        render_pose_sequence == 0 || !render_hmd_pose.orientation_valid ||
        !render_hmd_pose.position_valid) {
        impl_->last_error = "immediate flat capture requires device, source, frame, generation and render pose";
        return false;
    }
    try {
        const auto begin = std::chrono::steady_clock::now();
        impl_->last_error.clear();
        D3DSURFACE_DESC desc{};
        HRESULT hr = source_surface->GetDesc(&desc);
        if (FAILED(hr)) {
            impl_->last_error = Failure("flat capture surface GetDesc", hr);
            return false;
        }
        if (!SupportedFormat(desc.Format) || desc.Format == kD3dFormatNull) {
            impl_->last_error = "unsupported immediate flat-capture surface format";
            return false;
        }

        ComPtr<IDirect3DSurface9> resolved;
        IDirect3DSurface9* readback_source = source_surface;
        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            hr = device->CreateRenderTarget(
                desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0,
                FALSE, &resolved, nullptr);
            if (FAILED(hr) || !resolved) {
                impl_->last_error = Failure("flat capture transient resolve target creation", hr);
                return false;
            }
            hr = device->StretchRect(
                source_surface, nullptr, resolved.Get(), nullptr, D3DTEXF_NONE);
            if (FAILED(hr)) {
                impl_->last_error = Failure("flat capture transient MSAA resolve", hr);
                return false;
            }
            readback_source = resolved.Get();
        }

        ComPtr<IDirect3DSurface9> system_memory;
        hr = device->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM,
            &system_memory, nullptr);
        if (FAILED(hr) || !system_memory) {
            impl_->last_error = Failure("flat capture system-memory surface creation", hr);
            return false;
        }
        hr = device->GetRenderTargetData(readback_source, system_memory.Get());
        if (FAILED(hr)) {
            impl_->last_error = Failure("flat capture GetRenderTargetData", hr);
            return false;
        }

        D3DLOCKED_RECT locked{};
        hr = system_memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !locked.pBits || locked.Pitch <= 0) {
            if (SUCCEEDED(hr)) (void)system_memory->UnlockRect();
            impl_->last_error = Failure("flat capture LockRect", FAILED(hr) ? hr : E_FAIL);
            return false;
        }
        const std::size_t row_bytes = static_cast<std::size_t>(desc.Width) * 4U;
        const std::size_t source_pitch = static_cast<std::size_t>(locked.Pitch);
        if (source_pitch < row_bytes) {
            (void)system_memory->UnlockRect();
            impl_->last_error = "flat capture returned a pitch smaller than the pixel row";
            return false;
        }

        CpuEyeFrame eye{};
        eye.width = desc.Width;
        eye.height = desc.Height;
        eye.stride = static_cast<std::uint32_t>(row_bytes);
        eye.format = CpuPixelFormat::bgrx8_unorm;
        eye.pixels.resize(row_bytes * desc.Height);
        const auto* source_row = static_cast<const std::uint8_t*>(locked.pBits);
        auto* destination_row = eye.pixels.data();
        for (UINT row = 0; row < desc.Height; ++row) {
            std::memcpy(destination_row, source_row, row_bytes);
            source_row += source_pitch;
            destination_row += row_bytes;
        }
        hr = system_memory->UnlockRect();
        if (FAILED(hr)) {
            impl_->last_error = Failure("flat capture UnlockRect", hr);
            return false;
        }

        frame.device_id = reinterpret_cast<std::uintptr_t>(device);
        frame.generation = generation;
        frame.capture_sequence = frame_sequence;
        frame.render_pose_sequence = render_pose_sequence;
        frame.render_hmd_pose = render_hmd_pose;
        frame.capture_time = begin;
        frame.eyes[0] = std::move(eye);
        frame.eyes[1] = frame.eyes[0];

        const auto end = std::chrono::steady_clock::now();
        std::ostringstream detail;
        detail << "transport=immediate_flat_d3d9_cpu_mailbox"
               << ";capture_source=backbuffer"
               << ";eye_surface=" << desc.Width << 'x' << desc.Height
               << ";format=" << static_cast<unsigned>(desc.Format)
               << ";source_msaa=" << static_cast<unsigned>(desc.MultiSampleType)
               << ";persistent_default_pool=false"
               << std::fixed << std::setprecision(3)
               << ";capture_ms=" << MillisecondsBetween(begin, end);
        impl_->capture_description = detail.str();
        impl_->collect_description = detail.str();
        return true;
    } catch (...) {
        impl_->last_error = "immediate flat D3D9 capture raised an exception";
        frame = {};
        return false;
    }
}

bool D3D9StereoCapture::EndFrame(
    const std::uint64_t frame_sequence,
    const runtime::Pose& render_hmd_pose,
    const std::uint64_t render_pose_sequence) noexcept {
    try {
        if (impl_->current_frame != frame_sequence || impl_->current_slot >= impl_->slots.size()) {
            impl_->last_error = "stereo capture frame has no active ring slot";
            return false;
        }
        Impl::Slot& slot = impl_->slots[impl_->current_slot];
        if (render_pose_sequence == 0 || !render_hmd_pose.orientation_valid ||
            !render_hmd_pose.position_valid) {
            impl_->last_error = "stereo capture frame requires the exact valid HMD render pose";
            slot.ResetState();
            impl_->current_frame = 0;
            impl_->current_slot = impl_->slots.size();
            return false;
        }
        if (!slot.eye_captured[0] || !slot.eye_captured[1]) {
            impl_->last_error = "stereo capture frame requires both GPU eye copies";
            slot.ResetState();
            impl_->current_frame = 0;
            impl_->current_slot = impl_->slots.size();
            return false;
        }
        slot.render_hmd_pose = render_hmd_pose;
        slot.render_pose_sequence = render_pose_sequence;
        const HRESULT hr = slot.fence->Issue(D3DISSUE_END);
        if (FAILED(hr)) {
            impl_->last_error = Failure("capture ring fence Issue", hr);
            slot.ResetState();
            impl_->current_frame = 0;
            impl_->current_slot = impl_->slots.size();
            return false;
        }
        slot.pending = true;
        ++impl_->stats.frames_fenced;
        impl_->UpdateRingDepth();
        impl_->current_frame = 0;
        impl_->current_slot = impl_->slots.size();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 stereo frame finalization raised an exception";
        return false;
    }
}

bool D3D9StereoCapture::TryCollectReady(StereoCpuFrame& frame) noexcept {
    const auto reset_metadata_keep_pixels = [&frame]() noexcept {
        frame.producer_lease.Release();
        frame.device_id = 0;
        frame.generation = 0;
        frame.capture_sequence = 0;
        frame.transport_sequence = 0;
        frame.render_pose_sequence = 0;
        frame.render_hmd_pose = {};
        frame.capture_time = {};
        frame.presentation_mode = FramePresentationMode::native_stereo;
        frame.transport = StereoFrameTransport::cpu_bgrx;
        frame.shared_eyes = {};
        for (auto& eye : frame.eyes) {
            eye.width = 0;
            eye.height = 0;
            eye.stride = 0;
            eye.format = CpuPixelFormat::bgrx8_unorm;
            eye.borrowed_pixels = nullptr;
        }
    };
    reset_metadata_keep_pixels();
    try {
        if (!impl_->resources_initialized || !impl_->resource_device.Get()) return false;
        Impl::Slot* slot = impl_->OldestPending();
        if (!slot) return false;
        const auto poll_begin = std::chrono::steady_clock::now();
        BOOL event_complete = FALSE;
        HRESULT ready = slot->fence->GetData(
            &event_complete,
            static_cast<DWORD>(sizeof(event_complete)),
            0);
        bool flushed = false;
        if (ready == S_FALSE && !slot->fence_flush_issued) {
            ready = slot->fence->GetData(
                &event_complete,
                static_cast<DWORD>(sizeof(event_complete)),
                D3DGETDATA_FLUSH);
            slot->fence_flush_issued = true;
            flushed = true;
            ++impl_->stats.query_flushes;
        }
        const auto poll_end = std::chrono::steady_clock::now();
        if (FAILED(ready)) {
            impl_->last_error = Failure("capture ring fence GetData", ready);
            slot->ResetState();
            ++impl_->stats.frames_invalidated;
            impl_->UpdateRingDepth();
            return false;
        }
        const bool fence_ready = ready == S_OK && event_complete != FALSE;
        if (!fence_ready) {
            ++impl_->stats.query_not_ready;
            std::ostringstream not_ready;
            not_ready << "transport="
                << (impl_->transport_mode == Impl::TransportMode::d3d9ex_shared_texture
                        ? "d3d9ex_shared_texture_ring"
                        : "classic_d3d9_locked_systemmem_fallback")
                << ";status=gpu_pending"
                << ";frame_sequence=" << slot->frame_sequence
                << ";ring_depth=" << impl_->stats.ring_depth
                << ";command_flush=" << (flushed ? "true" : "false")
                << std::fixed << std::setprecision(3)
                << ";fence_poll_ms=" << MillisecondsBetween(poll_begin, poll_end)
                << ";producer_wait_ms=0.000";
            impl_->collect_description = not_ready.str();
            return false;
        }

        frame.device_id = impl_->device_id;
        frame.generation = impl_->generation;
        frame.capture_sequence = slot->frame_sequence;
        frame.render_pose_sequence = slot->render_pose_sequence;
        frame.render_hmd_pose = slot->render_hmd_pose;
        frame.capture_time = slot->capture_time;
        if (impl_->transport_mode == Impl::TransportMode::d3d9ex_shared_texture) {
            auto release_state = std::make_shared<ProducerFrameReleaseState>();
            for (std::size_t eye = 0; eye < 2; ++eye) {
                frame.shared_eyes[eye] = SharedTextureEyeFrame{
                    .shared_handle = slot->shared_handles[eye],
                    .width = impl_->source_desc.Width,
                    .height = impl_->source_desc.Height,
                    .d3d_format = static_cast<std::uint32_t>(impl_->source_desc.Format),
                };
            }
            frame.transport = StereoFrameTransport::d3d9ex_shared_texture;
            frame.producer_lease = ProducerFrameLease(release_state);
            slot->release_state = std::move(release_state);
            slot->eye_captured = {};
            slot->frame_sequence = 0;
            slot->render_pose_sequence = 0;
            slot->render_hmd_pose = {};
            slot->capture_time = {};
            slot->pending = false;
            ++impl_->stats.frames_collected;
            ++impl_->stats.shared_frames_published;
            impl_->UpdateRingDepth();

            std::ostringstream out;
            out << "transport=d3d9ex_shared_texture_ring"
                << ";frame_sequence=" << frame.capture_sequence
                << ";render_pose_sequence=" << frame.render_pose_sequence
                << ";generation=" << frame.generation
                << ";eye_surface=" << impl_->source_desc.Width << 'x'
                << impl_->source_desc.Height
                << ";format=" << static_cast<unsigned>(impl_->source_desc.Format)
                << ";shared_handles_distinct="
                << (slot->shared_handles[0] != slot->shared_handles[1] ? "true" : "false")
                << ";ring_depth=" << impl_->stats.ring_depth
                << ";ring_depth_peak=" << impl_->stats.ring_depth_peak
                << ";command_flush=" << (flushed ? "true" : "false")
                << std::fixed << std::setprecision(3)
                << ";fence_poll_ms=" << MillisecondsBetween(poll_begin, poll_end)
                << ";producer_wait_ms=0.000"
                << ";deferred_readback_ms=0.000"
                << ";cpu_copy_ms=0.000"
                << ";producer_collect_ms=" << MillisecondsBetween(
                    poll_begin, std::chrono::steady_clock::now());
            impl_->collect_description = out.str();
            return true;
        }

        auto release_state = std::make_shared<ProducerFrameReleaseState>();
        double deferred_readback_ms = 0.0;
        const auto unlock_slot = [slot]() noexcept {
            for (std::size_t eye = 0; eye < slot->system_eyes.size(); ++eye) {
                if (slot->system_eye_locked[eye] && slot->system_eyes[eye]) {
                    (void)slot->system_eyes[eye]->UnlockRect();
                    slot->system_eye_locked[eye] = false;
                }
            }
        };
        for (std::size_t eye = 0; eye < 2; ++eye) {
            const auto readback_begin = std::chrono::steady_clock::now();
            const HRESULT hr = impl_->resource_device->GetRenderTargetData(
                slot->gpu_eyes[eye].Get(), slot->system_eyes[eye].Get());
            const auto readback_end = std::chrono::steady_clock::now();
            deferred_readback_ms += MillisecondsBetween(readback_begin, readback_end);
            if (FAILED(hr)) {
                impl_->last_error = Failure("deferred GetRenderTargetData", hr);
                unlock_slot();
                slot->ResetState();
                ++impl_->stats.frames_invalidated;
                reset_metadata_keep_pixels();
                return false;
            }
            D3DLOCKED_RECT locked{};
            const HRESULT lock = slot->system_eyes[eye]->LockRect(
                &locked, nullptr, D3DLOCK_READONLY);
            const std::size_t row_bytes =
                static_cast<std::size_t>(impl_->source_desc.Width) * 4U;
            if (FAILED(lock) || !locked.pBits || locked.Pitch <= 0 ||
                static_cast<std::size_t>(locked.Pitch) < row_bytes) {
                if (SUCCEEDED(lock)) (void)slot->system_eyes[eye]->UnlockRect();
                impl_->last_error = Failure(
                    "capture ring deferred LockRect", FAILED(lock) ? lock : E_FAIL);
                unlock_slot();
                slot->ResetState();
                ++impl_->stats.frames_invalidated;
                reset_metadata_keep_pixels();
                return false;
            }
            slot->system_eye_locked[eye] = true;
            frame.eyes[eye].width = impl_->source_desc.Width;
            frame.eyes[eye].height = impl_->source_desc.Height;
            frame.eyes[eye].stride = static_cast<std::uint32_t>(locked.Pitch);
            frame.eyes[eye].format = CpuPixelFormat::bgrx8_unorm;
            frame.eyes[eye].pixels.clear();
            frame.eyes[eye].borrowed_pixels =
                static_cast<const std::uint8_t*>(locked.pBits);
        }
        frame.transport = StereoFrameTransport::classic_d3d9_locked_systemmem;
        frame.producer_lease = ProducerFrameLease(release_state);
        slot->release_state = std::move(release_state);
        slot->eye_captured = {};
        slot->frame_sequence = 0;
        slot->render_pose_sequence = 0;
        slot->render_hmd_pose = {};
        slot->capture_time = {};
        slot->pending = false;
        const auto collect_end = std::chrono::steady_clock::now();

        std::ostringstream out;
        out << "transport=classic_d3d9_locked_systemmem_fallback"
            << ";frame_sequence=" << frame.capture_sequence
            << ";render_pose_sequence=" << frame.render_pose_sequence
            << ";generation=" << frame.generation
            << ";eye_surface=" << impl_->source_desc.Width << 'x' << impl_->source_desc.Height
            << ";fence_ready_before_readback=" << (fence_ready ? "true" : "false")
            << ";systemmem_surfaces=per_slot_per_eye"
            << ";cpu_pixels=borrowed_locked_surface"
            << ";fallback_reason=" << impl_->fallback_reason
            << ";ring_depth=" << impl_->stats.ring_depth
            << ";command_flush=" << (flushed ? "true" : "false")
            << std::fixed << std::setprecision(3)
            << ";fence_poll_ms=" << MillisecondsBetween(poll_begin, poll_end)
            << ";producer_wait_ms=0.000"
            << ";deferred_readback_ms=" << deferred_readback_ms
            << ";cpu_copy_ms=0.000"
            << ";producer_collect_ms=" << MillisecondsBetween(poll_begin, collect_end);
        impl_->collect_description = out.str();

        ++impl_->stats.frames_collected;
        ++impl_->stats.cpu_fallback_frames;
        impl_->UpdateRingDepth();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 deferred stereo readback raised an exception";
        reset_metadata_keep_pixels();
        return false;
    }
}

void D3D9StereoCapture::InvalidateResources() noexcept {
    if (!impl_) return;
    impl_->ReclaimConsumerSlots();
    for (const auto& slot : impl_->slots) {
        if (slot.release_state) {
            impl_->last_error =
                "capture resource invalidation deferred while consumer owns a producer slot";
            impl_->UpdateRingDepth();
            return;
        }
    }
    impl_->ReleaseResources();
}

void D3D9StereoCapture::Shutdown() noexcept { InvalidateResources(); }

D3D9StereoCaptureStats D3D9StereoCapture::stats() const noexcept { return impl_->stats; }
bool D3D9StereoCapture::gpu_resident_active() const noexcept {
    return impl_->resources_initialized &&
        impl_->transport_mode == Impl::TransportMode::d3d9ex_shared_texture;
}
std::string_view D3D9StereoCapture::last_error() const noexcept { return impl_->last_error; }
std::string_view D3D9StereoCapture::capture_description() const noexcept {
    return impl_->capture_description;
}
std::string_view D3D9StereoCapture::collect_description() const noexcept {
    return impl_->collect_description;
}

} // namespace cojvr::backends::d3d9
