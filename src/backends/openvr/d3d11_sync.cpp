#include "backends/openvr/d3d11_sync.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <thread>

namespace cojvr::backends::openvr {
namespace {

double MillisecondsBetween(
    const std::chrono::steady_clock::time_point begin,
    const std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

const char* D3D11SyncStrategyName(const D3D11SyncStrategy strategy) noexcept {
    switch (strategy) {
    case D3D11SyncStrategy::none: return "none";
    case D3D11SyncStrategy::flush: return "flush";
    case D3D11SyncStrategy::event_query: return "event_query";
    }
    return "unknown";
}

bool SynchronizeD3D11(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const D3D11SyncStrategy strategy,
    const std::uint32_t timeout_ms,
    D3D11SyncResult& result,
    std::string& error) noexcept {
    result = {};
    error.clear();
    try {
        if (!device || !context) {
            error = "D3D11 synchronization requires a device and immediate context";
            result.hresult = E_INVALIDARG;
            return false;
        }

        const auto begin = std::chrono::steady_clock::now();
        if (strategy == D3D11SyncStrategy::none) {
            result.succeeded = true;
            result.elapsed_ms = MillisecondsBetween(begin, std::chrono::steady_clock::now());
            return true;
        }

        if (strategy == D3D11SyncStrategy::flush) {
            context->Flush();
            result.succeeded = true;
            result.elapsed_ms = MillisecondsBetween(begin, std::chrono::steady_clock::now());
            return true;
        }

        Microsoft::WRL::ComPtr<ID3D11Query> query;
        D3D11_QUERY_DESC desc{};
        desc.Query = D3D11_QUERY_EVENT;
        const HRESULT create_result = device->CreateQuery(&desc, &query);
        if (FAILED(create_result) || !query) {
            result.hresult = create_result;
            error = "D3D11 event-query creation failed";
            return false;
        }

        context->End(query.Get());
        // One explicit flush starts the bounded diagnostic fence. Polling then
        // uses DONOTFLUSH so the wait cannot turn into repeated global flushes.
        context->Flush();
        const auto deadline = begin + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            BOOL completed = FALSE;
            const HRESULT get_result = context->GetData(
                query.Get(), &completed, sizeof(completed), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            ++result.polls;
            result.hresult = get_result;
            if (get_result == S_OK && completed != FALSE) {
                result.succeeded = true;
                result.elapsed_ms = MillisecondsBetween(begin, std::chrono::steady_clock::now());
                return true;
            }
            if (FAILED(get_result)) {
                result.elapsed_ms = MillisecondsBetween(begin, std::chrono::steady_clock::now());
                error = "D3D11 event-query synchronization failed";
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                result.timed_out = true;
                result.elapsed_ms = MillisecondsBetween(begin, std::chrono::steady_clock::now());
                error = "D3D11 event-query synchronization timed out";
                return false;
            }
            std::this_thread::yield();
        }
    } catch (...) {
        try {
            error = "D3D11 synchronization raised an exception";
        } catch (...) {
        }
        return false;
    }
}

} // namespace cojvr::backends::openvr
