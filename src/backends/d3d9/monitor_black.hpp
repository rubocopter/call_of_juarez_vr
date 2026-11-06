#pragma once
#include <d3d9.h>
#include <wrl/client.h>
#include <cstdint>

namespace cojvr::backends::d3d9 {
class MonitorBlackPolicy final {
public:
    // Only successful final natural captures renew this lease. Skipped theater
    // readbacks keep their existing cadence without alternating desktop flashes.
    void FlatCaptured(std::uintptr_t device, std::uint64_t generation, std::uint64_t now) noexcept {
        flat_device_ = device; flat_generation_ = generation; flat_tick_ = now;
    }
    bool FlatFresh(std::uintptr_t device, std::uint64_t generation, std::uint64_t now) const noexcept {
        return device && generation && device == flat_device_ && generation == flat_generation_ &&
            Fresh(flat_tick_, now);
    }
    // Call on the render/Present thread. Historical totals alone cannot prove
    // that the compositor still works. A reset, stop, tracking loss, counter or
    // time regression requires a new baseline and then successful progress.
    bool Allow(std::uintptr_t device, std::uint64_t generation, std::uint64_t now,
        bool captured, bool running, bool tracking, std::uint64_t submissions,
        std::uint64_t failures) noexcept {
        if (!device || !generation || !running || !tracking) {
            observed_ = false; progress_tick_ = 0;
            return false;
        }
        const bool baseline = !observed_ || device != device_ || generation != generation_ ||
            now < observed_tick_ || submissions < submissions_ || failures < failures_;
        if (baseline || failures != failures_) progress_tick_ = 0;
        else if (submissions > submissions_) progress_tick_ = now;
        observed_ = true; device_ = device; generation_ = generation; observed_tick_ = now;
        submissions_ = submissions; failures_ = failures;
        return captured && Fresh(progress_tick_, now);
    }
private:
    static bool Fresh(std::uint64_t tick, std::uint64_t now) noexcept {
        return tick && now >= tick && now - tick < 250;
    }
    std::uintptr_t device_ = 0, flat_device_ = 0;
    std::uint64_t generation_ = 0, flat_generation_ = 0, flat_tick_ = 0;
    std::uint64_t observed_tick_ = 0, progress_tick_ = 0, submissions_ = 0, failures_ = 0;
    bool observed_ = false;
};

// The saved surface is a pixel preimage, not a state block. Every mutation is
// bracketed by the original Present, including failed native Present calls.
// Calls stay on the render/Present thread; Reset runs before Reset/ResetEx and
// device replacement. No render target/depth/viewport/shader binding is changed.
class MonitorBlackSurface final {
public:
    HRESULT Begin(IDirect3DDevice9* device, std::uint64_t generation) noexcept {
        if (!device || !generation || active_ || failed_) return S_FALSE;
        if (device->GetNumberOfSwapChains() != 1) return S_FALSE;
        if (saved_ && (device_ != device || generation_ != generation)) Reset();
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain;
        HRESULT hr = device->GetSwapChain(0, &chain);
        if (FAILED(hr)) return Fail(hr);
        if (!chain) return S_FALSE;
        D3DPRESENT_PARAMETERS pp{};
        hr = chain->GetPresentParameters(&pp);
        if (FAILED(hr)) return Fail(hr);
        // Other swap modes and multiple chains need separate ownership proof.
        if (pp.SwapEffect != D3DSWAPEFFECT_DISCARD) return S_FALSE;
        Microsoft::WRL::ComPtr<IDirect3DSurface9> backbuffer;
        hr = chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
        if (FAILED(hr)) return Fail(hr);
        if (!backbuffer) return S_FALSE;
        D3DSURFACE_DESC desc{};
        hr = backbuffer->GetDesc(&desc);
        if (FAILED(hr)) return Fail(hr);
        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) return S_FALSE;
        if (saved_ && !SameLayout(desc, layout_)) saved_.Reset();
        if (!saved_) {
            hr = device->CreateRenderTarget(desc.Width, desc.Height, desc.Format,
                D3DMULTISAMPLE_NONE, 0, FALSE, &saved_, nullptr);
            if (FAILED(hr)) return Fail(hr);
        }
        // No desktop write is permitted until the entire preimage is saved.
        hr = device->StretchRect(backbuffer.Get(), nullptr, saved_.Get(), nullptr, D3DTEXF_NONE);
        if (FAILED(hr)) return Fail(hr);
        device_ = device; generation_ = generation; layout_ = desc;
        chain_ = chain; active_ = true;
        hr = device->ColorFill(backbuffer.Get(), nullptr, D3DCOLOR_ARGB(255, 0, 0, 0));
        if (FAILED(hr)) {
            // A failed fill is not evidence of no writes. Roll back before
            // returning to original Present, then disable further attempts.
            const HRESULT restore = End(device, generation);
            return Fail(FAILED(restore) ? restore : hr);
        }
        return S_OK;
    }
    HRESULT End(IDirect3DDevice9* device, std::uint64_t generation) noexcept {
        if (!active_) return S_FALSE;
        if (device != device_ || generation != generation_) return Fail(D3DERR_INVALIDCALL);
        Microsoft::WRL::ComPtr<IDirect3DSwapChain9> current;
        HRESULT hr = device->GetSwapChain(0, &current);
        if (FAILED(hr)) return Fail(hr);
        if (current.Get() != chain_.Get()) return Fail(D3DERR_INVALIDCALL);
        // DISCARD can rotate the underlying buffers. Restore the saved pixels
        // into this same chain's CURRENT backbuffer, never a retained old RT0.
        Microsoft::WRL::ComPtr<IDirect3DSurface9> backbuffer;
        hr = current->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
        if (FAILED(hr)) return Fail(hr);
        if (!backbuffer) return Fail(D3DERR_INVALIDCALL);
        D3DSURFACE_DESC desc{};
        hr = backbuffer->GetDesc(&desc);
        if (FAILED(hr)) return Fail(hr);
        if (!SameLayout(desc, layout_)) return Fail(D3DERR_INVALIDCALL);
        hr = device->StretchRect(saved_.Get(), nullptr, backbuffer.Get(), nullptr, D3DTEXF_NONE);
        if (FAILED(hr)) return Fail(hr);
        active_ = false; chain_.Reset();
        return S_OK;
    }
    void Reset() noexcept {
        active_ = false; failed_ = false; chain_.Reset(); saved_.Reset();
        device_ = nullptr; generation_ = 0; layout_ = {};
    }
private:
    static bool SameLayout(const D3DSURFACE_DESC& a, const D3DSURFACE_DESC& b) noexcept {
        return a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
            a.MultiSampleType == b.MultiSampleType && a.MultiSampleQuality == b.MultiSampleQuality;
    }
    HRESULT Fail(HRESULT hr) noexcept {
        Reset(); failed_ = true; // No retries or stale preimage writes after uncertainty.
        return hr;
    }
    Microsoft::WRL::ComPtr<IDirect3DSurface9> saved_;
    Microsoft::WRL::ComPtr<IDirect3DSwapChain9> chain_;
    IDirect3DDevice9* device_ = nullptr; // Identity only; resources retain the owner.
    std::uint64_t generation_ = 0;
    D3DSURFACE_DESC layout_{};
    bool active_ = false, failed_ = false;
};
}
