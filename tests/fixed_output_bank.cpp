#include "backends/openvr/fixed_output_bank.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace cojvr::backends::openvr;
using Microsoft::WRL::ComPtr;

namespace {
bool Check(bool value, const char* message) {
    if (!value) std::cerr << message << '\n';
    return value;
}

// A private-data sentinel reports actual destruction, without interpreting
// AddRef/Release counts as GPU or compositor completion.
struct Sentinel final : IUnknown {
    ULONG refs = 1;
    unsigned* destroyed;
    explicit Sentinel(unsigned& count) : destroyed(&count) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown)) return E_NOINTERFACE;
        *out = this; AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --refs;
        if (!remaining) { ++*destroyed; delete this; }
        return remaining;
    }
};
constexpr GUID sentinel_id{0x8162210b,0xd73b,0x4187,{0xa2,0xaa,0x70,0x27,0x67,0x91,0x88,0x13}};
}

int main(int argc, char** argv) {
    const int fault = argc > 1 ? std::atoi(argv[1]) : 0;
    if (fault < 0 || fault > 19) return 1;
    FixedOutputConfiguration config{};
    config.enabled = true;
    config.native_width = 32; config.native_height = 24;
    config.desktop_width = 16; config.desktop_height = 8;
    config.max_output_bytes = 12288; // 8*(32*24 + 32*24)
    FixedOutputBank::SetFaultForTesting(fault);
    auto* bank = FixedOutputBank::ClaimProcessSession();
    if (fault == 15) {
        return Check(!bank && FixedOutputBank::ProcessSessionClaimed() &&
            !FixedOutputBank::ClaimProcessSession(), "record allocation failure must seal the process") ? 0 : 1;
    }
    if (!Check(bank && !FixedOutputBank::ClaimProcessSession(), "only one process session may claim outputs")) return 1;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature{};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &device, &feature, &context))) return 1;
    if (fault == 16) config.max_output_bytes = 12287;
    if (fault == 17) config.native_d3d_format = 113;
    if (fault == 18) config.native_width = 0;
    if (fault == 19) {
        ComPtr<ID3D11DeviceContext> deferred;
        if (FAILED(device->CreateDeferredContext(0, &deferred))) return 1;
        bank->Prepare(device.Get(), deferred.Get(), config, 32, 24);
    } else {
        bank->Prepare(device.Get(), context.Get(), config, 32, 24);
    }
    const auto ready = bank->Readiness();
    const std::array<unsigned, 20> retained{4,0,1,2,3,1,2,3,4,2,4,2,4,2,4,0,0,0,0,0};
    if (!Check(ready.retained_textures == retained[static_cast<std::size_t>(fault)], "partial allocation count differs")) return 1;
    if (fault != 0) {
        if (!Check(ready.failure != FixedOutputFailure::none &&
                ready.state != FixedOutputState::ready, "fault published native readiness")) return 1;
        std::array<ComPtr<ID3D11Texture2D>, 2> selected{};
        if (!Check(!bank->Select(device.Get(), context.Get(), false, 32, 24,
                DXGI_FORMAT_B8G8R8A8_UNORM, selected), "partial native pair became selectable")) return 1;
        bank->Prepare(device.Get(), context.Get(), config, 32, 24);
        if (!Check(bank->Readiness().retained_textures == ready.retained_textures &&
                bank->Readiness().failure == ready.failure, "failed preparation retried allocation")) return 1;
        // Even when native preparation fails, only a fully initialized theater
        // pair may remain usable. It still needs new capture pixels to submit.
        const bool theater = bank->Select(device.Get(), context.Get(), true, 16, 8,
            DXGI_FORMAT_B8G8R8A8_UNORM, selected);
        const bool expected_theater = fault == 3 || fault == 4 || fault == 7 || fault == 8 ||
            fault == 10 || fault == 12 || fault == 14;
        return Check(theater == expected_theater, "incorrect theater fallback readiness") ? 0 : 1;
    }
    if (!Check(ready.state == FixedOutputState::ready && ready.payload_bytes == 12288 &&
            !ready.theater_capture_initialized,
            "storage initialization must not claim a fresh natural theater capture")) return 1;
    std::array<ComPtr<ID3D11Texture2D>, 2> native{}, theater{}, again{};
    if (!bank->Select(device.Get(), context.Get(), false, 32, 24, DXGI_FORMAT_B8G8R8A8_UNORM, native) ||
        !bank->Select(device.Get(), context.Get(), true, 16, 8, DXGI_FORMAT_B8G8R8A8_UNORM, theater)) return 1;
    const std::array<ID3D11Texture2D*,4> identities{native[0].Get(),native[1].Get(),theater[0].Get(),theater[1].Get()};
    for (std::size_t i=0; i<identities.size(); ++i) for (std::size_t j=i+1; j<identities.size(); ++j)
        if (!Check(identities[i] != identities[j], "mode outputs must not alias even with equal extents")) return 1;
    FixedOutputFrameAuthority authority;
    const auto first_capture = authority.Epoch();
    for (unsigned i = 0; i < 1000; ++i) {
        // Failed/repeated native resets and ownership loss revoke frame
        // authorization, never the retained output allocation/readiness.
        authority.Invalidate();
        const bool flat = (i & 1) != 0;
        if (!bank->Select(device.Get(), context.Get(), flat, flat ? 16U : 32U,
                flat ? 8U : 24U, DXGI_FORMAT_B8G8R8A8_UNORM, again)) return 1;
        if (!Check(again == (flat ? theater : native), "mode transition replaced output identities")) return 1;
    }
    if (!Check(!authority.Allows(first_capture) && bank->Readiness().state == FixedOutputState::ready &&
            bank->Readiness().retained_textures == 4 && !bank->Readiness().theater_capture_initialized,
            "frame invalidation changed storage readiness or fabricated theater capture")) return 1;
    if (!Check(!bank->Select(device.Get(), context.Get(), false, 16, 8,
            DXGI_FORMAT_B8G8R8A8_UNORM, again), "native desktop-size fallback must reject instead of upscale")) return 1;
    if (!Check(!bank->Select(device.Get(), context.Get(), false, 32, 24,
            DXGI_FORMAT_B8G8R8X8_UNORM, again), "format changes must reject instead of recreate")) return 1;
    ComPtr<ID3D11Device> other_device;
    ComPtr<ID3D11DeviceContext> other_context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &other_device, &feature, &other_context))) return 1;
    if (!Check(!bank->Select(other_device.Get(), other_context.Get(), false, 32, 24,
            DXGI_FORMAT_B8G8R8A8_UNORM, again), "foreign consumer admitted")) return 1;

    // Inspect initialized padding and real equal-size CPU updates via WARP.
    std::array<unsigned, 16*8> pixels{};
    pixels.fill(0xff345678);
    const D3D11_BOX box{8, 8, 0, 24, 16, 1};
    context->UpdateSubresource(theater[0].Get(), 0, &box, pixels.data(), 16*4, 0);
    D3D11_TEXTURE2D_DESC desc{};
    theater[0]->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return 1;
    context->CopyResource(staging.Get(), theater[0].Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return 1;
    for (unsigned y=0; y<24; ++y) for (unsigned x=0; x<32; ++x) {
        const auto* row = reinterpret_cast<const unsigned*>(static_cast<const char*>(mapped.pData)+y*mapped.RowPitch);
        if (!Check(row[x] == ((x>=8 && x<24 && y>=8 && y<16) ? 0xff345678U : 0xff000000U),
                "theater padding/current pixels differ")) return 1;
    }
    context->Unmap(staging.Get(), 0);
    unsigned destroyed = 0;
    for (auto* texture : {native[0].Get(),native[1].Get(),theater[0].Get(),theater[1].Get()}) {
        auto* sentinel = new Sentinel(destroyed);
        if (FAILED(texture->SetPrivateDataInterface(sentinel_id, sentinel))) return 1;
        sentinel->Release();
    }
    context->ClearState(); context->Flush();
    native = {}; theater = {}; again = {}; staging.Reset();
    context.Reset(); device.Reset();
    if (!Check(destroyed == 0 && bank->Readiness().retained_textures == 4 &&
            !FixedOutputBank::ClaimProcessSession(), "local teardown released roots or rearmed process claim")) return 1;
    bank->Seal();
    return Check(bank->Readiness().state == FixedOutputState::stopped && destroyed == 0,
        "session stop must retain four roots") ? 0 : 1;
}
