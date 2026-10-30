#pragma once
#include <array>
#include <cstdint>
#include <d3d11_1.h>
#include <memory>
#include <span>
#include <vector>

namespace cojvr::backends::openvr::neural {
// Presenter-owned immediate-context component; no model, history or frame admission.
// Pair bytes are contiguous left then right, encoded RGBA8, tightly packed.
// One owner/thread must serialize calls with the renderer's context ownership.
class ImageProcessor final {
    friend struct ImageProcessorTestAccess;
    using ReadinessProbe=HRESULT (*)(ID3D11DeviceContext*,ID3D11Query*) noexcept;
    // Narrow query-adapter seam: tests can withhold readiness without GPU sleeps.
    // Null uses the actual nonflushing GetData result; no runtime delay injection.
    ReadinessProbe readiness_probe_=nullptr;
    HRESULT Ready(ID3D11DeviceContext* context,ID3D11Query* query) noexcept;
    struct Resources;
    std::unique_ptr<Resources> resources_;
    HRESULT result_=S_OK;
    double readback_ms_=0;
public:
    ImageProcessor() noexcept;
    ~ImageProcessor();
    ImageProcessor(const ImageProcessor&)=delete;
    ImageProcessor& operator=(const ImageProcessor&)=delete;
    // Retains private full originals until resolve, discard or reset. Begin queues
    // once and flushes once; Poll never restarts work, flushes, spins or waits.
    // busy includes the captured/inference interval. Any new BeginCapture while
    // busy fails E_PENDING with unchanged extents. Parent owns frame/pose expiry.
    [[nodiscard]] bool BeginCapture(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& eyes,unsigned requested_height,
        unsigned& stage_width,unsigned& stage_height) noexcept;
    [[nodiscard]] bool PollCapture(ID3D11Device* device,ID3D11DeviceContext* context,
        std::vector<std::uint8_t>& pair) noexcept;
    // Uses BeginCapture's originals, never current destination contents. Both
    // destinations are retained by COM identity and validated again before commit.
    [[nodiscard]] bool BeginResolve(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& destinations,std::span<const std::uint8_t> input,
        std::span<const std::uint8_t> output,unsigned stage_width,unsigned stage_height) noexcept;
    [[nodiscard]] bool PollResolve(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& destinations) noexcept;
    // Cancellation never releases/reuses in-flight resources. PollCapture drains
    // either discarded operation (false/E_ABORT, busy=false); Reset on context loss.
    // Cancelled PollResolve can drain with an empty destination pair.
    void Discard() noexcept;
    [[nodiscard]] bool busy() const noexcept;
    // Aspect-preserving proxy, <=1280 on either axis and >=256 on both.
    // Legacy bounded wrappers for host probes. Pending work is cancelled and
    // must drain before another capture; game callers use Begin/Poll instead.
    // False leaves pair/extents untouched. Only the wrapper wait is bounded to
    // 2ms; last_readback_ms measures the most recent PollCapture CPU cost.
    [[nodiscard]] bool Capture(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& eyes,unsigned stage_height,
        std::vector<std::uint8_t>& pair,unsigned& stage_width,unsigned& stage_height_out) noexcept;
    // Legacy Resolve can snapshot current images when no capture is retained.
    // CopyResource has no device-loss-atomic pair primitive. True means both
    // copies were submitted after the final healthy-device check.
    [[nodiscard]] bool Resolve(ID3D11Device* device,ID3D11DeviceContext* context,
        const std::array<ID3D11Texture2D*,2>& eyes,std::span<const std::uint8_t> input,
        std::span<const std::uint8_t> output,unsigned stage_width,unsigned stage_height) noexcept;
    [[nodiscard]] HRESULT last_result() const noexcept {return result_;}
    [[nodiscard]] double last_readback_ms() const noexcept {return readback_ms_;}
    void Reset() noexcept;
};
}
