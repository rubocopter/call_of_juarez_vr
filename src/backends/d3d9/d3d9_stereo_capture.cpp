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
    struct Slot {
        std::array<ComPtr<IDirect3DSurface9>, 2> gpu_eyes{};
        ComPtr<IDirect3DQuery9> fence;
        std::array<bool, 2> eye_captured{};
        std::uint64_t frame_sequence = 0;
        std::uint64_t render_pose_sequence = 0;
        runtime::Pose render_hmd_pose{};
        std::chrono::steady_clock::time_point capture_time{};
        bool pending = false;

        void ResetState() noexcept {
            eye_captured = {};
            frame_sequence = 0;
            render_pose_sequence = 0;
            render_hmd_pose = {};
            capture_time = {};
            pending = false;
        }
    };

    std::array<Slot, kCaptureRingSize> slots{};
    ComPtr<IDirect3DSurface9> system_memory;
    D3DSURFACE_DESC source_desc{};
    IDirect3DDevice9* resource_device = nullptr;
    std::uintptr_t device_id = 0;
    std::uint64_t generation = 0;
    std::size_t current_slot = kCaptureRingSize;
    std::uint64_t current_frame = 0;
    D3D9StereoCaptureStats stats{};
    std::string last_error;
    std::string capture_description;
    std::string collect_description;
    bool resources_initialized = false;

    void ReleaseResources() noexcept {
        for (auto& slot : slots) {
            for (auto& surface : slot.gpu_eyes) surface.Reset();
            slot.fence.Reset();
            if (slot.pending) ++stats.frames_invalidated;
            slot.ResetState();
        }
        system_memory.Reset();
        source_desc = {};
        resource_device = nullptr;
        device_id = 0;
        generation = 0;
        current_slot = kCaptureRingSize;
        current_frame = 0;
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
        if (resources_initialized && resource_device == device &&
            generation == resource_generation && SameDescription(desc, source_desc)) {
            return true;
        }

        ReleaseResources();
        hr = device->CreateOffscreenPlainSurface(
            desc.Width,
            desc.Height,
            desc.Format,
            D3DPOOL_SYSTEMMEM,
            &system_memory,
            nullptr);
        if (FAILED(hr) || !system_memory) {
            last_error = Failure("capture staging system-memory surface creation", hr);
            ReleaseResources();
            return false;
        }
        for (auto& slot : slots) {
            for (std::size_t eye = 0; eye < slot.gpu_eyes.size(); ++eye) {
                hr = device->CreateRenderTarget(
                    desc.Width,
                    desc.Height,
                    desc.Format,
                    D3DMULTISAMPLE_NONE,
                    0,
                    FALSE,
                    &slot.gpu_eyes[eye],
                    nullptr);
                if (FAILED(hr) || !slot.gpu_eyes[eye]) {
                    last_error = Failure("capture ring render-target creation", hr);
                    ReleaseResources();
                    return false;
                }
            }
            hr = device->CreateQuery(D3DQUERYTYPE_EVENT, &slot.fence);
            if (FAILED(hr) || !slot.fence) {
                last_error = Failure("capture ring event-query creation", hr);
                ReleaseResources();
                return false;
            }
        }

        source_desc = desc;
        resource_device = device;
        device_id = reinterpret_cast<std::uintptr_t>(device);
        generation = resource_generation;
        resources_initialized = true;
        return true;
    }

    bool AcquireSlot(const std::uint64_t frame_sequence) noexcept {
        if (current_frame == frame_sequence && current_slot < slots.size()) return true;
        current_frame = 0;
        current_slot = slots.size();
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (!slots[index].pending && slots[index].frame_sequence == 0) {
                current_slot = index;
                current_frame = frame_sequence;
                slots[index].frame_sequence = frame_sequence;
                slots[index].capture_time = std::chrono::steady_clock::now();
                slots[index].eye_captured = {};
                return true;
            }
        }
        ++stats.frames_dropped_no_slot;
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

    bool CopyCpuEye(CpuEyeFrame& output) {
        D3DLOCKED_RECT locked{};
        HRESULT hr = system_memory->LockRect(&locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr) || !locked.pBits || locked.Pitch <= 0) {
            if (SUCCEEDED(hr)) (void)system_memory->UnlockRect();
            last_error = Failure("capture ring LockRect", FAILED(hr) ? hr : E_FAIL);
            return false;
        }
        const std::size_t row_bytes = static_cast<std::size_t>(source_desc.Width) * 4U;
        const std::size_t source_pitch = static_cast<std::size_t>(locked.Pitch);
        if (source_pitch < row_bytes) {
            (void)system_memory->UnlockRect();
            last_error = "capture ring returned a pitch smaller than the pixel row";
            return false;
        }
        const std::size_t total_bytes = row_bytes * source_desc.Height;
        output.width = source_desc.Width;
        output.height = source_desc.Height;
        output.stride = static_cast<std::uint32_t>(row_bytes);
        output.format = CpuPixelFormat::bgrx8_unorm;
        output.pixels.resize(total_bytes);
        const auto* source_row = static_cast<const std::uint8_t*>(locked.pBits);
        auto* destination_row = output.pixels.data();
        if (source_pitch == row_bytes) {
            std::memcpy(destination_row, source_row, total_bytes);
        } else {
            for (UINT row = 0; row < source_desc.Height; ++row) {
                std::memcpy(destination_row, source_row, row_bytes);
                source_row += source_pitch;
                destination_row += row_bytes;
            }
        }
        hr = system_memory->UnlockRect();
        if (FAILED(hr)) {
            last_error = Failure("capture ring UnlockRect", hr);
            return false;
        }
        return true;
    }
};

D3D9StereoCapture::D3D9StereoCapture() : impl_(std::make_unique<Impl>()) {}
D3D9StereoCapture::~D3D9StereoCapture() = default;

bool D3D9StereoCapture::CaptureEye(
    IDirect3DDevice9* device,
    const runtime::Eye eye,
    const std::uint64_t frame_sequence,
    const std::uint64_t generation) noexcept {
    if (!device || frame_sequence == 0 || generation == 0) {
        impl_->last_error = "D3D9 stereo capture requires device, frame and generation";
        return false;
    }
    try {
        const auto begin = std::chrono::steady_clock::now();
        impl_->last_error.clear();
        ComPtr<IDirect3DSurface9> source;
        HRESULT hr = device->GetRenderTarget(0, &source);
        if (FAILED(hr) || !source) {
            impl_->last_error = Failure("GetRenderTarget(0)", FAILED(hr) ? hr : E_FAIL);
            return false;
        }
        if (!impl_->EnsureResources(device, source.Get(), generation) ||
            !impl_->AcquireSlot(frame_sequence)) {
            return false;
        }

        ComPtr<IDirect3DSurface9> back_buffer;
        const HRESULT back_buffer_result =
            device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
        const bool source_is_backbuffer = SUCCEEDED(back_buffer_result) && back_buffer &&
            source.Get() == back_buffer.Get();
        D3DVIEWPORT9 viewport{};
        const bool viewport_valid = SUCCEEDED(device->GetViewport(&viewport));

        Impl::Slot& slot = impl_->slots[impl_->current_slot];
        const std::size_t eye_index = EyeIndex(eye);
        hr = device->StretchRect(
            source.Get(), nullptr, slot.gpu_eyes[eye_index].Get(), nullptr, D3DTEXF_NONE);
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
        out << "transport=deferred_d3d9_ring_cpu_mailbox"
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
            << ";source_msaa=" << static_cast<unsigned>(impl_->source_desc.MultiSampleType)
            << ";ring_slots=" << impl_->slots.size()
            << std::fixed << std::setprecision(3)
            << ";gpu_copy_queue_ms=" << MillisecondsBetween(begin, end);
        impl_->capture_description = out.str();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 stereo eye capture raised an exception";
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
        impl_->current_frame = 0;
        impl_->current_slot = impl_->slots.size();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 stereo frame finalization raised an exception";
        return false;
    }
}

bool D3D9StereoCapture::TryCollectReady(StereoCpuFrame& frame) noexcept {
    frame = {};
    try {
        if (!impl_->resources_initialized || !impl_->resource_device) return false;
        Impl::Slot* slot = impl_->OldestPending();
        if (!slot) return false;
        const auto poll_begin = std::chrono::steady_clock::now();
        BOOL event_complete = FALSE;
        const HRESULT ready = slot->fence->GetData(
            &event_complete, static_cast<DWORD>(sizeof(event_complete)), 0);
        const auto poll_end = std::chrono::steady_clock::now();
        if (FAILED(ready)) {
            impl_->last_error = Failure("capture ring fence GetData", ready);
            slot->ResetState();
            ++impl_->stats.frames_invalidated;
            return false;
        }
        const bool fence_ready = ready == S_OK && event_complete != FALSE;

        frame.device_id = impl_->device_id;
        frame.generation = impl_->generation;
        frame.capture_sequence = slot->frame_sequence;
        frame.render_pose_sequence = slot->render_pose_sequence;
        frame.render_hmd_pose = slot->render_hmd_pose;
        frame.capture_time = slot->capture_time;
        double deferred_readback_ms = 0.0;
        double cpu_copy_ms = 0.0;
        for (std::size_t eye = 0; eye < 2; ++eye) {
            const auto readback_begin = std::chrono::steady_clock::now();
            const HRESULT hr = impl_->resource_device->GetRenderTargetData(
                slot->gpu_eyes[eye].Get(), impl_->system_memory.Get());
            const auto readback_end = std::chrono::steady_clock::now();
            deferred_readback_ms += MillisecondsBetween(readback_begin, readback_end);
            if (FAILED(hr)) {
                impl_->last_error = Failure("deferred GetRenderTargetData", hr);
                slot->ResetState();
                ++impl_->stats.frames_invalidated;
                frame = {};
                return false;
            }
            const auto copy_begin = std::chrono::steady_clock::now();
            if (!impl_->CopyCpuEye(frame.eyes[eye])) {
                slot->ResetState();
                ++impl_->stats.frames_invalidated;
                frame = {};
                return false;
            }
            const auto copy_end = std::chrono::steady_clock::now();
            cpu_copy_ms += MillisecondsBetween(copy_begin, copy_end);
        }
        const auto collect_end = std::chrono::steady_clock::now();

        std::ostringstream out;
        out << "transport=deferred_d3d9_ring_cpu_mailbox"
            << ";frame_sequence=" << frame.capture_sequence
            << ";render_pose_sequence=" << frame.render_pose_sequence
            << ";generation=" << frame.generation
            << ";eye_surface=" << impl_->source_desc.Width << 'x' << impl_->source_desc.Height
            << ";fence_ready_before_readback=" << (fence_ready ? "true" : "false")
            << std::fixed << std::setprecision(3)
            << ";fence_poll_ms=" << MillisecondsBetween(poll_begin, poll_end)
            << ";deferred_readback_ms=" << deferred_readback_ms
            << ";cpu_copy_ms=" << cpu_copy_ms
            << ";producer_collect_ms=" << MillisecondsBetween(poll_begin, collect_end);
        impl_->collect_description = out.str();

        slot->ResetState();
        ++impl_->stats.frames_collected;
        return true;
    } catch (...) {
        impl_->last_error = "D3D9 deferred stereo readback raised an exception";
        frame = {};
        return false;
    }
}

void D3D9StereoCapture::Shutdown() noexcept {
    if (!impl_) return;
    impl_->ReleaseResources();
}

D3D9StereoCaptureStats D3D9StereoCapture::stats() const noexcept { return impl_->stats; }
std::string_view D3D9StereoCapture::last_error() const noexcept { return impl_->last_error; }
std::string_view D3D9StereoCapture::capture_description() const noexcept {
    return impl_->capture_description;
}
std::string_view D3D9StereoCapture::collect_description() const noexcept {
    return impl_->collect_description;
}

} // namespace cojvr::backends::d3d9
