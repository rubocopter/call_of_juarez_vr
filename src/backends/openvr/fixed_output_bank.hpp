#pragma once

#include "backends/openvr/fixed_output_policy.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>

namespace cojvr::backends::openvr {

// One intentionally non-destroyed record per process/module. Pinning prevents
// unload/reload from forgetting the claim. No OpenVR interfaces or callbacks
// survive here. Methods except Claim/ProcessSessionClaimed run on the presenter
// owner thread; external readers receive a separately synchronized value copy.
// Retention proves application ownership, never compositor retirement.
class FixedOutputBank final {
    using Texture = Microsoft::WRL::ComPtr<ID3D11Texture2D>;
public:
    static bool ProcessSessionClaimed() noexcept { return claimed_.load(std::memory_order_acquire); }

    [[nodiscard]] static FixedOutputBank* ClaimProcessSession() noexcept {
        bool expected = false;
        if (!claimed_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return nullptr;
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&claimed_), &module)) return nullptr;
#if defined(COJVR_FIXED_OUTPUT_FAULT_TESTING)
        if (test_fault_ == 15) return nullptr;
#endif
        // A failed heap allocation also consumes the sole process attempt.
        process_root_ = new (std::nothrow) FixedOutputBank;
        return process_root_;
    }

    void Prepare(ID3D11Device* device, ID3D11DeviceContext* context,
        const FixedOutputConfiguration& config,
        std::uint32_t theater_width, std::uint32_t theater_height) noexcept {
        if (attempted_) return;
        attempted_ = true;
        status_.state = FixedOutputState::preparing;
        config_ = config;
        status_.native_width = config.native_width; status_.native_height = config.native_height;
        status_.desktop_width = config.desktop_width; status_.desktop_height = config.desktop_height;
        status_.theater_width = theater_width; status_.theater_height = theater_height;
        const auto valid_extent = [](std::uint32_t w, std::uint32_t h) noexcept {
            return w && h && w <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION && h <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        };
        if (!config.enabled || !valid_extent(config.native_width, config.native_height) ||
            !valid_extent(config.desktop_width, config.desktop_height) ||
            !valid_extent(theater_width, theater_height) ||
            config.desktop_width > theater_width || config.desktop_height > theater_height ||
            (config.native_d3d_format != 21 && config.native_d3d_format != 22)) {
            Fail(FixedOutputFailure::invalid_configuration); return;
        }
        status_.payload_bytes = 8ULL * (static_cast<std::uint64_t>(config.native_width)*config.native_height +
            static_cast<std::uint64_t>(theater_width)*theater_height);
        if (status_.payload_bytes > config.max_output_bytes) {
            Fail(FixedOutputFailure::invalid_configuration); return;
        }
        Microsoft::WRL::ComPtr<ID3D11Device> context_device;
        if (context) context->GetDevice(&context_device);
        if (!device || !context || context_device.Get() != device ||
            context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE || FAILED(device->GetDeviceRemovedReason())) {
            Fail(FixedOutputFailure::endpoints); return;
        }
        // Keep endpoints rooted before the first allocation or GPU command.
        device_ = device; context_ = context;
        if (!PreparePair(0, theater_width, theater_height, DXGI_FORMAT_B8G8R8A8_UNORM)) return;
        status_.state = FixedOutputState::theater_ready;
        if (!PreparePair(1, config.native_width, config.native_height,
                config.native_d3d_format == 21 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM)) return;
        status_.state = FixedOutputState::ready;
    }

    [[nodiscard]] bool HealthyEndpoints(ID3D11Device* device, ID3D11DeviceContext* context) const noexcept {
        return device && context && device_.Get() == device && context_.Get() == context &&
            SUCCEEDED(device->GetDeviceRemovedReason());
    }

    // Validation/selection only: no allocation, resize, retry or release.
    [[nodiscard]] bool Select(ID3D11Device* device, ID3D11DeviceContext* context, bool theater,
        std::uint32_t source_width, std::uint32_t source_height, DXGI_FORMAT format,
        std::array<Texture, 2>& result) const noexcept {
        if (!HealthyEndpoints(device, context) || status_.state == FixedOutputState::stopped ||
            (theater ? !theater_ready_ : !native_ready_)) return false;
        if (source_width != (theater ? config_.desktop_width : config_.native_width) ||
            source_height != (theater ? config_.desktop_height : config_.native_height) ||
            format != (theater || config_.native_d3d_format == 21 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM)) return false;
        const std::size_t offset = theater ? 0U : 2U;
        result = {textures_[offset], textures_[offset+1]};
        return true;
    }

    [[nodiscard]] FixedOutputReadiness Readiness() const noexcept { return status_; }
    void Seal() noexcept { attempted_ = true; status_.state = FixedOutputState::stopped; }

#if defined(COJVR_FIXED_OUTPUT_FAULT_TESTING)
    static void SetFaultForTesting(int stage) noexcept { test_fault_ = stage; }
#endif

private:
    FixedOutputBank() = default;
    ~FixedOutputBank() = default; // Deliberately inaccessible; no shutdown release API.
    inline static std::atomic_bool claimed_{false};
    inline static FixedOutputBank* process_root_ = nullptr; // Trivial pointer: no static COM destructor.
#if defined(COJVR_FIXED_OUTPUT_FAULT_TESTING)
    inline static int test_fault_ = 0;
#endif
    static bool Fault(int stage) noexcept {
#if defined(COJVR_FIXED_OUTPUT_FAULT_TESTING)
        return test_fault_ == stage;
#else
        (void)stage; return false;
#endif
    }
    void Fail(FixedOutputFailure failure) noexcept {
        status_.failure = failure;
        status_.state = theater_ready_ ? FixedOutputState::theater_ready : FixedOutputState::failed;
    }
    bool PreparePair(std::size_t pair, UINT width, UINT height, DXGI_FORMAT format) noexcept {
        UINT support = 0;
        constexpr UINT required = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_RENDER_TARGET |
            D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
        if (FAILED(device_->CheckFormatSupport(format, &support)) || (support & required) != required) {
            Fail(FixedOutputFailure::invalid_configuration); return false;
        }
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height; desc.MipLevels = desc.ArraySize = 1;
        desc.Format = format; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        for (std::size_t eye=0; eye<2; ++eye) {
            const auto index = pair*2+eye;
            if (Fault(static_cast<int>(index+1)) ||
                FAILED(device_->CreateTexture2D(&desc, nullptr, &textures_[index])) || !textures_[index]) {
                Fail(FixedOutputFailure::texture); return false;
            }
            ++status_.retained_textures;
            if (Fault(static_cast<int>(index+5)) ||
                FAILED(device_->CreateRenderTargetView(textures_[index].Get(), nullptr, &views_[index])) || !views_[index]) {
                Fail(FixedOutputFailure::view); return false;
            }
            constexpr float black[4]{0,0,0,1};
            context_->ClearRenderTargetView(views_[index].Get(), black);
        }
        D3D11_QUERY_DESC query{}; query.Query = D3D11_QUERY_EVENT;
        if (Fault(static_cast<int>(pair+9)) || FAILED(device_->CreateQuery(&query, &queries_[pair])) || !queries_[pair]) {
            Fail(FixedOutputFailure::initialization_query); return false;
        }
        context_->End(queries_[pair].Get()); context_->Flush();
        const auto deadline = std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
        for (;;) {
            BOOL complete = FALSE;
            const HRESULT hr = Fault(static_cast<int>(pair+11)) ? E_FAIL :
                context_->GetData(queries_[pair].Get(), &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (FAILED(hr) || FAILED(device_->GetDeviceRemovedReason())) {
                Fail(FixedOutputFailure::initialization_poll); return false;
            }
            if (Fault(static_cast<int>(pair+13))) { Fail(FixedOutputFailure::initialization_timeout); return false; }
            if (hr == S_OK && complete) break;
            if (std::chrono::steady_clock::now() >= deadline) { Fail(FixedOutputFailure::initialization_timeout); return false; }
            std::this_thread::yield();
        }
        // This event qualifies private initialization only. It is never used
        // to retire a scene texture or to fabricate a producer completion.
        if (pair == 0) theater_ready_ = true; else native_ready_ = true;
        return true;
    }
    bool attempted_ = false, theater_ready_ = false, native_ready_ = false;
    FixedOutputConfiguration config_{};
    FixedOutputReadiness status_{};
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    std::array<Texture, 4> textures_{};
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>,4> views_{};
    std::array<Microsoft::WRL::ComPtr<ID3D11Query>,2> queries_{};
};

} // namespace cojvr::backends::openvr
