#pragma once

#include <d3d9.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

namespace cojvr::backends::d3d9 {

struct FrameLumaSample {
    std::uint64_t sequence = 0, generation = 0, pose_sequence = 0, timestamp_ns = 0;
    std::uintptr_t source = 0, device = 0;
    std::uint32_t source_width = 0, source_height = 0;
    D3DFORMAT source_format = D3DFMT_UNKNOWN;
    // Raw 0..255 channels, ignoring alpha. Extrema are per-pixel RGB averages;
    // this point-sampled grid is a content diagnostic, not photometric luminance.
    double mean_rgb = 0, mean_r = 0, mean_g = 0, mean_b = 0;
    double min_rgb = 0, max_rgb = 0, white_fraction = 0;
    double queue_ms = 0, readback_ms = 0;
};

class D3D9FrameLumaProbe final {
public:
    // Single render-thread owner. Use independent instances per eye/desktop.
    // Call Reset BEFORE native Reset/ResetEx attempts and owner teardown.
    // Counters are lifetime totals. dropped includes rejected Queue attempts and
    // pending samples discarded by Reset; skipped counts rejected Queue calls.
    HRESULT last_result = S_OK;
    std::uint64_t queued = 0, completed = 0, dropped = 0, skipped = 0;
    std::uint64_t failures = 0, pending_polls = 0;
    bool unavailable = false;

    D3D9FrameLumaProbe() noexcept = default;
    ~D3D9FrameLumaProbe() noexcept { Reset(); }
    D3D9FrameLumaProbe(const D3D9FrameLumaProbe&) = delete;
    D3D9FrameLumaProbe& operator=(const D3D9FrameLumaProbe&) = delete;

    bool Queue(IDirect3DDevice9* device, IDirect3DSurface9* source,
               std::uint64_t sequence, std::uint64_t generation,
               std::uint64_t pose_sequence) noexcept {
        const auto started = Now();
        if (unavailable) { ++skipped; ++dropped; return false; }
        if (device_ && (device_.Get() != device || source_ != source || generation_ != generation))
            Reset();
        if (!device || !source) return QueueFailure(D3DERR_INVALIDCALL);
        D3DSURFACE_DESC desc{};
        HRESULT hr = source->GetDesc(&desc);
        if (FAILED(hr)) return QueueFailure(hr);
        if (device_ && (desc.Width != desc_.Width || desc.Height != desc_.Height ||
                        desc.Format != desc_.Format)) Reset();
        if (!desc.Width || !desc.Height || desc.Type != D3DRTYPE_SURFACE ||
            desc.Pool != D3DPOOL_DEFAULT || desc.MultiSampleType != D3DMULTISAMPLE_NONE ||
            (desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8))
            return QueueFailure(D3DERR_INVALIDCALL);
        Microsoft::WRL::ComPtr<IDirect3DDevice9> owner;
        hr = source->GetDevice(&owner);
        if (FAILED(hr)) return QueueFailure(hr);
        if (owner.Get() != device) return QueueFailure(D3DERR_INVALIDCALL);
        // Surface descriptions alone do not distinguish a cube face from 2D.
        Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> container;
        hr = source->GetContainer(__uuidof(IDirect3DBaseTexture9),
                                 reinterpret_cast<void**>(container.GetAddressOf()));
        if (SUCCEEDED(hr)) {
            if (!container || container->GetType() != D3DRTYPE_TEXTURE)
                return QueueFailure(D3DERR_INVALIDCALL);
        } else if (hr != E_NOINTERFACE) return QueueFailure(hr);

        if (!device_) {
            device_ = device; source_ = source; desc_ = desc; generation_ = generation;
            for (auto& slot : slots_) {
                hr = device->CreateRenderTarget(16, 16, desc.Format, D3DMULTISAMPLE_NONE,
                                                0, FALSE, &slot.target, nullptr);
                if (FAILED(hr)) return QueueFailure(hr);
                hr = device->CreateOffscreenPlainSurface(16, 16, desc.Format,
                    D3DPOOL_SYSTEMMEM, &slot.cpu, nullptr);
                if (FAILED(hr)) return QueueFailure(hr);
                hr = device->CreateQuery(D3DQUERYTYPE_EVENT, &slot.event);
                if (FAILED(hr)) return QueueFailure(hr);
            }
        }
        if (count_ == slots_.size()) {
            ++skipped; ++dropped; last_result = S_FALSE; return false;
        }
        auto& slot = slots_[(head_ + count_) % slots_.size()];
        // No native target/state binding, source write, flush or wait. A driver
        // rejecting this operation (including inside BeginScene) disables the
        // diagnostic until explicit Reset instead of retrying it every frame.
        hr = device->StretchRect(source, nullptr, slot.target.Get(), nullptr, D3DTEXF_POINT);
        if (FAILED(hr)) return QueueFailure(hr);
        hr = slot.event->Issue(D3DISSUE_END);
        if (FAILED(hr)) return QueueFailure(hr);
        slot.sample = {};
        slot.sample.sequence = sequence; slot.sample.generation = generation;
        slot.sample.pose_sequence = pose_sequence; slot.sample.timestamp_ns = started;
        slot.sample.source = reinterpret_cast<std::uintptr_t>(source);
        slot.sample.device = reinterpret_cast<std::uintptr_t>(device);
        slot.sample.source_width = desc.Width; slot.sample.source_height = desc.Height;
        slot.sample.source_format = desc.Format;
        slot.sample.queue_ms = ElapsedMs(started);
        ++count_; ++queued; last_result = S_OK;
        return true;
    }

    // Outside BeginScene, at the next natural frame boundary. Exactly one
    // flags=0 query check per call, oldest first. Output is untouched on false.
    // Natural Present must submit commands; no progress is forced here.
    bool Poll(FrameLumaSample& output) noexcept {
        if (unavailable) return false;
        if (!count_) { last_result = S_FALSE; return false; }
        auto& slot = slots_[head_];
        const HRESULT ready = slot.event->GetData(nullptr, 0, 0);
        last_result = ready;
        if (ready == S_FALSE) { ++pending_polls; return false; }
        if (ready != S_OK) return PollFailure(ready);
        const auto started = Now();
        HRESULT hr = device_->GetRenderTargetData(slot.target.Get(), slot.cpu.Get());
        if (FAILED(hr)) return PollFailure(hr);
        D3DLOCKED_RECT lock{};
        hr = slot.cpu->LockRect(&lock, nullptr, D3DLOCK_READONLY);
        if (FAILED(hr)) return PollFailure(hr);
        FrameLumaSample sample = slot.sample;
        sample.min_rgb = 255;
        unsigned sum_r = 0, sum_g = 0, sum_b = 0, white = 0;
        const bool valid_lock = lock.pBits && lock.Pitch >= 16 * 4;
        if (valid_lock) {
            for (unsigned y = 0; y < 16; ++y) {
                const auto* row = static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch;
                for (unsigned x = 0; x < 16; ++x) {
                    const unsigned b = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2];
                    sum_r += r; sum_g += g; sum_b += b;
                    if (r >= 240 && g >= 240 && b >= 240) ++white;
                    const double rgb = (r + g + b) / 3.0;
                    if (rgb < sample.min_rgb) sample.min_rgb = rgb;
                    if (rgb > sample.max_rgb) sample.max_rgb = rgb;
                }
            }
        }
        hr = slot.cpu->UnlockRect();
        if (FAILED(hr)) return PollFailure(hr);
        if (!valid_lock) return PollFailure(D3DERR_INVALIDCALL);
        sample.mean_r = sum_r / 256.0; sample.mean_g = sum_g / 256.0; sample.mean_b = sum_b / 256.0;
        sample.mean_rgb = (sum_r + sum_g + sum_b) / (256.0 * 3);
        sample.white_fraction = white / 256.0;
        sample.readback_ms = ElapsedMs(started);
        output = sample;
        head_ = (head_ + 1) % slots_.size(); --count_; ++completed;
        last_result = S_OK;
        return true;
    }

    void Reset() noexcept {
        // Only private read-only work was submitted. Releasing it does not
        // authorize reuse/mutation of any native resource or signal retirement.
        dropped += count_; count_ = 0; head_ = 0;
        for (auto& slot : slots_) {
            slot.event.Reset(); slot.cpu.Reset(); slot.target.Reset(); slot.sample = {};
        }
        device_.Reset(); source_ = nullptr; desc_ = {}; generation_ = 0;
        unavailable = false; last_result = S_OK;
    }

private:
    struct Slot {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target, cpu;
        Microsoft::WRL::ComPtr<IDirect3DQuery9> event;
        FrameLumaSample sample{};
    };
    std::array<Slot, 3> slots_{};
    Microsoft::WRL::ComPtr<IDirect3DDevice9> device_;
    // Diagnostic identity only, never dereferenced or AddRef'd after Queue.
    IDirect3DSurface9* source_ = nullptr;
    D3DSURFACE_DESC desc_{};
    std::uint64_t generation_ = 0;
    std::size_t head_ = 0, count_ = 0;

    static std::uint64_t Now() noexcept {
        static const std::uint64_t frequency = []() noexcept {
            LARGE_INTEGER value{}; QueryPerformanceFrequency(&value);
            return static_cast<std::uint64_t>(value.QuadPart);
        }();
        LARGE_INTEGER value{}; QueryPerformanceCounter(&value);
        if (!frequency || value.QuadPart < 0) return 0;
        const auto ticks = static_cast<std::uint64_t>(value.QuadPart);
        return (ticks / frequency) * 1000000000ULL + static_cast<std::uint64_t>(
            (static_cast<long double>(ticks % frequency) * 1000000000.0L) / frequency);
    }
    static double ElapsedMs(std::uint64_t started) noexcept {
        const auto ended = Now();
        return ended >= started ? static_cast<double>(ended - started) / 1000000.0 : 0;
    }
    bool PollFailure(HRESULT hr) noexcept {
        Reset(); ++failures; unavailable = true; last_result = hr; return false;
    }
    bool QueueFailure(HRESULT hr) noexcept {
        ++skipped; ++dropped; return PollFailure(hr);
    }
};

} // namespace cojvr::backends::d3d9
