#include "backends/openvr/d3d9_shared_texture_bridge.hpp"
#include "backends/openvr/d3d11_sync.hpp"

#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>

namespace cojvr::backends::openvr {
namespace {

using Microsoft::WRL::ComPtr;

double MillisecondsBetween(
    const std::chrono::steady_clock::time_point begin,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

bool SameCopyDescription(
    const D3D11_TEXTURE2D_DESC& source,
    const D3D11_TEXTURE2D_DESC& destination) noexcept {
    return source.Width == destination.Width &&
        source.Height == destination.Height &&
        source.MipLevels == destination.MipLevels &&
        source.ArraySize == destination.ArraySize &&
        source.Format == destination.Format &&
        source.SampleDesc.Count == destination.SampleDesc.Count &&
        source.SampleDesc.Quality == destination.SampleDesc.Quality;
}

} // namespace

struct D3D9SharedTextureBridge::Impl {
    struct PendingCopy {
        ComPtr<ID3D11Query> fence;
        std::array<ComPtr<ID3D11Texture2D>, 2> sources{};
        d3d9::ProducerFrameLease lease{};
        std::chrono::steady_clock::time_point queued_at{};
    };

    std::unordered_map<std::uintptr_t, ComPtr<ID3D11Texture2D>> opened{};
    std::deque<PendingCopy> pending{};
    D3D9SharedTextureBridgeStats stats{};
    std::string last_error{};

    void RefreshDepth() noexcept {
        stats.pending_copy_fences = pending.size();
        stats.pending_copy_fences_peak =
            std::max(stats.pending_copy_fences_peak, pending.size());
    }
};

D3D9SharedTextureBridge::D3D9SharedTextureBridge()
    : impl_(std::make_unique<Impl>()) {}
D3D9SharedTextureBridge::~D3D9SharedTextureBridge() = default;

bool D3D9SharedTextureBridge::CopyFrame(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    d3d9::StereoCpuFrame& frame,
    const std::array<ID3D11Texture2D*, 2>& destination,
    D3D9SharedTextureCopyTiming& timing) noexcept {
    timing = {};
    if (!device || !context || !destination[0] || !destination[1] ||
        frame.transport != d3d9::StereoFrameTransport::d3d9ex_shared_texture ||
        !frame.producer_lease.valid()) {
        impl_->last_error = "shared-texture copy requires D3D11 endpoints and a leased GPU frame";
        ++impl_->stats.copy_failures;
        return false;
    }

    try {
        Poll(context);
        const auto open_begin = std::chrono::steady_clock::now();
        std::array<ComPtr<ID3D11Texture2D>, 2> sources{};
        for (std::size_t eye = 0; eye < sources.size(); ++eye) {
            const auto handle_value = frame.shared_eyes[eye].shared_handle;
            if (handle_value == 0) {
                impl_->last_error = "shared-texture frame contains a null eye handle";
                ++impl_->stats.open_failures;
                return false;
            }
            const auto cached = impl_->opened.find(handle_value);
            if (cached != impl_->opened.end()) {
                sources[eye] = cached->second;
            } else {
                ComPtr<ID3D11Texture2D> opened;
                const HRESULT open_result = device->OpenSharedResource(
                    reinterpret_cast<HANDLE>(handle_value), IID_PPV_ARGS(&opened));
                if (FAILED(open_result) || !opened) {
                    impl_->last_error = "D3D11 OpenSharedResource failed for D3D9Ex eye texture";
                    ++impl_->stats.open_failures;
                    return false;
                }
                impl_->opened.emplace(handle_value, opened);
                sources[eye] = std::move(opened);
                ++impl_->stats.resources_opened;
            }

            D3D11_TEXTURE2D_DESC source_desc{};
            D3D11_TEXTURE2D_DESC destination_desc{};
            sources[eye]->GetDesc(&source_desc);
            destination[eye]->GetDesc(&destination_desc);
            if (!SameCopyDescription(source_desc, destination_desc)) {
                impl_->last_error = "D3D9Ex/D3D11 shared source does not match presenter texture";
                ++impl_->stats.copy_failures;
                return false;
            }
        }
        const auto open_end = std::chrono::steady_clock::now();
        timing.open_ms = MillisecondsBetween(open_begin, open_end);

        D3D11_QUERY_DESC query_desc{};
        query_desc.Query = D3D11_QUERY_EVENT;
        ComPtr<ID3D11Query> fence;
        const HRESULT query_result = device->CreateQuery(&query_desc, &fence);
        if (FAILED(query_result) || !fence) {
            impl_->last_error = "D3D11 shared-copy event-query creation failed";
            ++impl_->stats.copy_failures;
            return false;
        }

        const auto copy_begin = std::chrono::steady_clock::now();
        for (std::size_t eye = 0; eye < sources.size(); ++eye) {
            context->CopyResource(destination[eye], sources[eye].Get());
        }
        context->End(fence.Get());
        // Flush submits the GPU work but does not wait for it. The event query
        // below is polled with DONOTFLUSH on later presenter iterations.
        context->Flush();
        const auto copy_end = std::chrono::steady_clock::now();
        timing.copy_queue_ms = MillisecondsBetween(copy_begin, copy_end);
        timing.consumer_wait_ms = 0.0;

        Impl::PendingCopy pending{};
        pending.fence = std::move(fence);
        pending.sources = std::move(sources);
        pending.lease = std::move(frame.producer_lease);
        pending.queued_at = copy_begin;
        impl_->pending.push_back(std::move(pending));
        ++impl_->stats.frames_copied;
        impl_->RefreshDepth();
        timing.pending_copy_fences = impl_->pending.size();
        return true;
    } catch (...) {
        impl_->last_error = "D3D9Ex/D3D11 shared-texture copy raised an exception";
        ++impl_->stats.copy_failures;
        return false;
    }
}

void D3D9SharedTextureBridge::Poll(ID3D11DeviceContext* context) noexcept {
    if (!context) return;
    try {
        while (!impl_->pending.empty()) {
            BOOL complete = FALSE;
            const HRESULT result = context->GetData(
                impl_->pending.front().fence.Get(),
                &complete,
                static_cast<UINT>(sizeof(complete)),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (result == S_FALSE || (result == S_OK && complete == FALSE)) {
                ++impl_->stats.copy_fence_poll_pending;
                break;
            }
            if (FAILED(result)) {
                impl_->last_error = "D3D11 shared-copy event-query polling failed";
                ++impl_->stats.copy_failures;
            } else {
                const double completion_ms = MillisecondsBetween(
                    impl_->pending.front().queued_at,
                    std::chrono::steady_clock::now());
                impl_->stats.last_copy_completion_ms = completion_ms;
                impl_->stats.max_copy_completion_ms =
                    std::max(impl_->stats.max_copy_completion_ms, completion_ms);
                ++impl_->stats.copy_fences_completed;
            }
            impl_->pending.pop_front();
        }
        impl_->RefreshDepth();
    } catch (...) {
        impl_->last_error = "D3D11 shared-copy polling raised an exception";
    }
}

void D3D9SharedTextureBridge::ResetOpenedResources() noexcept {
    try {
        impl_->opened.clear();
    } catch (...) {
    }
}

void D3D9SharedTextureBridge::Shutdown(ID3D11DeviceContext* context) noexcept {
    try {
        Poll(context);
        if (context && !impl_->pending.empty()) {
            ComPtr<ID3D11Device> device;
            context->GetDevice(&device);
            D3D11SyncResult sync{};
            std::string sync_error;
            constexpr std::uint32_t kShutdownDrainTimeoutMs = 1000;
            if (device && SynchronizeD3D11(
                    device.Get(), context, D3D11SyncStrategy::event_query,
                    kShutdownDrainTimeoutMs, sync, sync_error)) {
                // The shutdown query is queued after every pending CopyResource.
                // Once it retires, all earlier per-frame event queries are also
                // complete and Poll can release their producer leases safely.
                Poll(context);
            } else {
                impl_->last_error = sync_error.empty()
                    ? "D3D11 shutdown drain could not acquire the immediate device"
                    : "D3D11 shutdown drain failed: " + sync_error;
            }
        }
        if (!impl_->pending.empty()) {
            // Fail closed: keep producer leases alive until this bridge is
            // destroyed after the D3D11 session has shut down. Releasing them
            // here could let D3D9 recycle/free shared textures still referenced
            // by in-flight GPU work.
            impl_->stats.abandoned_on_shutdown += impl_->pending.size();
        }
        impl_->opened.clear();
        impl_->RefreshDepth();
    } catch (...) {
    }
}

D3D9SharedTextureBridgeStats D3D9SharedTextureBridge::stats() const noexcept {
    return impl_->stats;
}

std::string_view D3D9SharedTextureBridge::last_error() const noexcept {
    return impl_->last_error;
}

} // namespace cojvr::backends::openvr
