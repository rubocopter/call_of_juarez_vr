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
        std::uint64_t copy_ordinal = 0;
    };

    std::unordered_map<std::uintptr_t, ComPtr<ID3D11Texture2D>> opened{};
    std::deque<PendingCopy> pending{};
    D3D9SharedTextureBridgeStats stats{};
    std::string last_error{};
    bool fence_poll_failed = false;
    bool recovery_failed = false;
    bool recovery_flush_issued = false;
    ComPtr<ID3D11Query> recovery_fence;
    ComPtr<ID3D11Device> consumer_device;
    ComPtr<ID3D11DeviceContext> consumer_context;
    std::uintptr_t producer_device = 0;
    std::uint64_t producer_generation = 0;
#if defined(COJVR_GPU_COPY_FAULT_TESTING)
    bool force_next_poll_failure = false;
    bool force_fence_pending = false;
    bool force_next_recovery_poll_failure = false;
    bool force_recovery_pending = false;
#endif

    void RefreshDepth() noexcept {
        stats.pending_copy_fences = pending.size();
        stats.pending_copy_fences_peak =
            std::max(stats.pending_copy_fences_peak, pending.size());
    }

    void RetireFront() noexcept {
        const double completion_ms = MillisecondsBetween(
            pending.front().queued_at, std::chrono::steady_clock::now());
        stats.last_copy_completion_ms = completion_ms;
        stats.max_copy_completion_ms = std::max(stats.max_copy_completion_ms, completion_ms);
        ++stats.copy_fences_completed;
        pending.pop_front();
    }

    void PollRecovery(ID3D11DeviceContext* context) {
        if (recovery_failed || pending.empty()) return;
        if (!recovery_fence) {
            stats.recovery_first_copy = pending.front().copy_ordinal;
            stats.recovery_last_copy = pending.back().copy_ordinal;
            D3D11_QUERY_DESC desc{};
            desc.Query = D3D11_QUERY_EVENT;
            const HRESULT result = consumer_device->CreateQuery(&desc, &recovery_fence);
            if (FAILED(result) || !recovery_fence) {
                recovery_failed = true;
                ++stats.recovery_failures;
                last_error = "D3D11 independent recovery-query creation failed; leases retained";
                return;
            }
            // The same immediate context orders this fresh event after ALL old
            // CopyResource calls. The first GetData below allows one submission
            // check; subsequent polls never flush or wait.
            context->End(recovery_fence.Get());
            recovery_flush_issued = false;
            ++stats.recovery_queries_started;
            return;
        }
        BOOL complete = FALSE;
        HRESULT result = S_OK;
#if defined(COJVR_GPU_COPY_FAULT_TESTING)
        if (force_next_recovery_poll_failure) {
            force_next_recovery_poll_failure = false;
            result = E_FAIL;
        } else if (force_recovery_pending) {
            result = S_FALSE;
        } else
#endif
        {
            const UINT flags = recovery_flush_issued ? D3D11_ASYNC_GETDATA_DONOTFLUSH : 0;
            recovery_flush_issued = true;
            result = context->GetData(recovery_fence.Get(), &complete,
                static_cast<UINT>(sizeof(complete)), flags);
        }
        if (result == S_FALSE || (result == S_OK && complete == FALSE)) {
            ++stats.recovery_poll_pending;
            return;
        }
        if (result != S_OK) {
            recovery_failed = true;
            ++stats.recovery_failures;
            last_error = "D3D11 independent recovery-query polling failed; leases retained";
            return;
        }
        // This independent completion proves even copies whose individual
        // queries failed. Only now may the old cache and producer leases retire.
        while (!pending.empty()) RetireFront();
        recovery_fence.Reset();
        opened.clear();
        fence_poll_failed = false;
        ++stats.recoveries_completed;
        RefreshDepth();
    }
};

D3D9SharedTextureBridge::D3D9SharedTextureBridge()
    : impl_(std::make_unique<Impl>()) {}
D3D9SharedTextureBridge::~D3D9SharedTextureBridge() {
    // Releasing COM references does not prove queued copies completed. An
    // uncertain lease must never advertise consumer_done, even at teardown.
    for (auto& copy : impl_->pending) copy.lease.Abandon();
}

bool D3D9SharedTextureBridge::CopyFrame(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    d3d9::StereoCpuFrame& frame,
    const std::array<ID3D11Texture2D*, 2>& destination,
    D3D9SharedTextureCopyTiming& timing) noexcept {
    timing = {};
    if (!device || !context || !destination[0] || !destination[1] ||
        frame.transport != d3d9::StereoFrameTransport::d3d9ex_shared_texture ||
        !frame.producer_lease.valid() || frame.device_id == 0 || frame.generation == 0) {
        impl_->last_error = "shared-texture copy requires D3D11 endpoints and a leased GPU frame";
        ++impl_->stats.copy_failures;
        return false;
    }

    try {
        ComPtr<ID3D11Device> context_device;
        context->GetDevice(&context_device);
        if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE || context_device.Get() != device ||
            (impl_->producer_generation != 0 && (frame.generation < impl_->producer_generation ||
                (frame.generation == impl_->producer_generation && frame.device_id != impl_->producer_device)))) {
            impl_->last_error = "shared-texture copy rejected an unknown/stale producer or mismatched immediate consumer";
            ++impl_->stats.identity_rejections;
            return false;
        }
        for (auto* texture : destination) {
            ComPtr<ID3D11Device> texture_device;
            texture->GetDevice(&texture_device);
            if (texture_device.Get() != device) {
                impl_->last_error = "shared-texture destination belongs to another consumer device";
                ++impl_->stats.identity_rejections;
                return false;
            }
        }
        const bool consumer_changed = impl_->consumer_device.Get() != device ||
            impl_->consumer_context.Get() != context;
        if (consumer_changed && !impl_->pending.empty()) {
            impl_->last_error = "consumer replacement deferred until old GPU copies retire";
            ++impl_->stats.transition_deferrals;
            return false;
        }
        if (consumer_changed) {
            impl_->opened.clear();
            impl_->consumer_device = device;
            impl_->consumer_context = context;
        }
        Poll(context);
        if (impl_->fence_poll_failed) {
            // A failed query does not prove that its GPU copy has retired.
            // Keep all pending producer leases and stop queueing new copies.
            return false;
        }
        if (frame.device_id != impl_->producer_device || frame.generation != impl_->producer_generation) {
            if (!impl_->pending.empty()) {
                impl_->last_error = "producer replacement deferred until old GPU copies retire";
                ++impl_->stats.transition_deferrals;
                return false;
            }
            impl_->opened.clear();
        }
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

        // Acquire deque storage and transfer the producer lease before issuing
        // GPU commands. A failed allocation must not release an in-flight lease.
        const auto copy_begin = std::chrono::steady_clock::now();
        Impl::PendingCopy pending{};
        pending.fence = std::move(fence);
        pending.sources = std::move(sources);
        pending.lease = std::move(frame.producer_lease);
        pending.queued_at = copy_begin;
        pending.copy_ordinal = impl_->stats.frames_copied + 1;
        impl_->pending.push_back(std::move(pending));
        for (std::size_t eye = 0; eye < destination.size(); ++eye) {
            context->CopyResource(destination[eye], impl_->pending.back().sources[eye].Get());
        }
        context->End(impl_->pending.back().fence.Get());
        // Flush submits GPU work but does not wait; Poll checks completion later.
        context->Flush();
        const auto copy_end = std::chrono::steady_clock::now();
        timing.copy_queue_ms = MillisecondsBetween(copy_begin, copy_end);
        timing.consumer_wait_ms = 0.0;
        ++impl_->stats.frames_copied;
        impl_->producer_device = frame.device_id;
        impl_->producer_generation = frame.generation;
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
    if (!context || context != impl_->consumer_context.Get()) return;
    try {
        if (impl_->fence_poll_failed) {
            impl_->PollRecovery(context);
            return;
        }
        while (!impl_->pending.empty()) {
            BOOL complete = FALSE;
            HRESULT result = S_OK;
#if defined(COJVR_GPU_COPY_FAULT_TESTING)
            if (impl_->force_next_poll_failure) {
                impl_->force_next_poll_failure = false;
                result = E_FAIL;
            } else if (impl_->force_fence_pending) {
                result = S_FALSE;
            } else
#endif
            {
                result = context->GetData(
                    impl_->pending.front().fence.Get(),
                    &complete,
                    static_cast<UINT>(sizeof(complete)),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH);
            }
            if (result == S_FALSE || (result == S_OK && complete == FALSE)) {
                ++impl_->stats.copy_fence_poll_pending;
                break;
            }
            if (result != S_OK) {
                impl_->last_error = "D3D11 shared-copy event-query polling failed";
                ++impl_->stats.copy_failures;
                impl_->fence_poll_failed = true;
                break;
            } else {
                impl_->RetireFront();
            }
        }
        impl_->RefreshDepth();
    } catch (...) {
        if (impl_->fence_poll_failed && !impl_->recovery_failed) {
            impl_->recovery_failed = true;
            ++impl_->stats.recovery_failures;
        }
        impl_->fence_poll_failed = true;
        impl_->last_error = "D3D11 shared-copy polling raised an exception";
    }
}

#if defined(COJVR_GPU_COPY_FAULT_TESTING)
void D3D9SharedTextureBridge::ForceNextFencePollFailureForTest() noexcept {
    impl_->force_next_poll_failure = true;
}
void D3D9SharedTextureBridge::ForceFencePendingForTest(const bool pending) noexcept {
    impl_->force_fence_pending = pending;
}
void D3D9SharedTextureBridge::ForceNextRecoveryFencePollFailureForTest() noexcept {
    impl_->force_next_recovery_poll_failure = true;
}
void D3D9SharedTextureBridge::ForceRecoveryFencePendingForTest(const bool pending) noexcept {
    impl_->force_recovery_pending = pending;
}
#endif

void D3D9SharedTextureBridge::ResetOpenedResources() noexcept {
    try {
        impl_->opened.clear();
    } catch (...) {
    }
}

void D3D9SharedTextureBridge::Shutdown(ID3D11DeviceContext* context) noexcept {
    try {
        Poll(context);
        if (context && context == impl_->consumer_context.Get() &&
            !impl_->fence_poll_failed && !impl_->pending.empty()) {
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
            // Fail closed: retain the pending leases here. Destruction abandons
            // their consumer state without granting producer reuse; dropping
            // session references alone is not GPU retirement evidence.
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
