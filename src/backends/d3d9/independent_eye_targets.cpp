#include "backends/d3d9/independent_eye_targets.hpp"

#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace cojvr::backends::d3d9 {
namespace {
using Microsoft::WRL::ComPtr;
constexpr std::size_t kSurfaceCount = 5;
constexpr UINT kMaxExtent = 8192;
constexpr std::uint64_t kMaxPayload = 1024ULL * 1024 * 1024;
bool DepthSlot(std::size_t slot) noexcept { return slot == 1 || slot == 3 || slot == 4; }
unsigned Bytes(D3DFORMAT format, bool depth) noexcept {
    if (depth) {
        switch (format) {
        case D3DFMT_D16: return 2;
        case D3DFMT_D24X8: case D3DFMT_D24S8: return 4;
        default: return 0;
        }
    }
    switch (format) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: return 4;
    case D3DFMT_A16B16G16R16F: return 8;
    default: return 0;
    }
}
bool Extent(UINT width, UINT height) noexcept {
    return width && height && width <= kMaxExtent && height <= kMaxExtent;
}
bool SameDesc(const D3DSURFACE_DESC& a, const D3DSURFACE_DESC& b) noexcept {
    return a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
        a.Type == b.Type && a.Usage == b.Usage && a.Pool == b.Pool &&
        a.MultiSampleType == b.MultiSampleType && a.MultiSampleQuality == b.MultiSampleQuality;
}
bool SameViewport(const D3DVIEWPORT9& a, const D3DVIEWPORT9& b) noexcept {
    return a.X == b.X && a.Y == b.Y && a.Width == b.Width && a.Height == b.Height &&
        a.MinZ == b.MinZ && a.MaxZ == b.MaxZ;
}
bool GetTarget(IDirect3DDevice9* device, DWORD slot, ComPtr<IDirect3DSurface9>& out) noexcept {
    const HRESULT hr = device->GetRenderTarget(slot, out.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr) || (slot && hr == D3DERR_NOTFOUND);
}
bool GetDepth(IDirect3DDevice9* device, ComPtr<IDirect3DSurface9>& out) noexcept {
    const HRESULT hr = device->GetDepthStencilSurface(out.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr) || hr == D3DERR_NOTFOUND;
}
struct Bank {
    ComPtr<IDirect3DDevice9> device;
    std::array<std::array<ComPtr<IDirect3DTexture9>, 2>, 14> textures{};
    std::array<bool, 14> histories{};
    std::array<ComPtr<IDirect3DSurface9>, kSurfaceCount> surfaces{};
    ComPtr<IDirect3DStateBlock9> state;
    std::array<ComPtr<IDirect3DSurface9>, 4> retained_targets{};
    ComPtr<IDirect3DSurface9> retained_depth;
    D3DVIEWPORT9 viewport{};
    RECT scissor{};
    DWORD owner = 0, mrt_count = 0;
    std::uint64_t generation = 0, bytes = 0;
    UINT width = 0, height = 0;
    std::uint32_t leases = 0;
    bool active = false, poisoned = false;
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
    int fail_restore = -1;
    std::uint32_t restore_attempts = 0;
#endif

    bool Owner() const noexcept { return device && owner == GetCurrentThreadId(); }
    bool Save() noexcept {
        if (!Owner() || active || poisoned) return false;
        if (FAILED(state->Capture())) return false;
        bool ok = true;
        for (DWORD slot = 0; slot < mrt_count; ++slot)
            ok = GetTarget(device.Get(), slot, retained_targets[slot]) && ok;
        ok = GetDepth(device.Get(), retained_depth) && ok;
        ok = SUCCEEDED(device->GetViewport(&viewport)) && ok;
        ok = SUCCEEDED(device->GetScissorRect(&scissor)) && ok;
        if (!ok) { ReleaseSaved(); return false; }
        active = true;
        return true;
    }
    void ReleaseSaved() noexcept {
        for (auto& target : retained_targets) target.Reset();
        retained_depth.Reset();
    }
    bool Restore() noexcept {
        if (!Owner() || !active) return false;
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
        restore_attempts = 0;
#endif
        auto step = [&, ordinal = 0](auto&& operation) mutable {
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
            restore_attempts |= std::uint32_t{1} << ordinal;
            if (ordinal++ == fail_restore) return false;
#endif
            return operation();
        };
        // Attempt every restoration even after a failure. Detach incompatible
        // depth/MRTs before restoring native RT0 (which resets the viewport).
        bool ok = step([&] { return SUCCEEDED(device->SetDepthStencilSurface(nullptr)); });
        for (DWORD slot = 1; slot < mrt_count; ++slot)
            ok = step([&] { return SUCCEEDED(device->SetRenderTarget(slot, nullptr)); }) && ok;
        for (DWORD slot = 0; slot < mrt_count; ++slot)
            ok = step([&] { return SUCCEEDED(device->SetRenderTarget(slot, retained_targets[slot].Get())); }) && ok;
        ok = step([&] { return SUCCEEDED(device->SetDepthStencilSurface(retained_depth.Get())); }) && ok;
        ok = step([&] { return SUCCEEDED(state->Apply()); }) && ok;
        ok = step([&] { return SUCCEEDED(device->SetViewport(&viewport)); }) && ok;
        ok = step([&] { return SUCCEEDED(device->SetScissorRect(&scissor)); }) && ok;
        for (DWORD slot = 0; slot < mrt_count; ++slot) {
            ComPtr<IDirect3DSurface9> current;
            ok = step([&] { return GetTarget(device.Get(), slot, current) &&
                  current.Get() == retained_targets[slot].Get(); }) && ok;
        }
        ComPtr<IDirect3DSurface9> depth;
        D3DVIEWPORT9 current_viewport{}; RECT current_scissor{};
        ok = step([&] { return GetDepth(device.Get(), depth) && depth.Get() == retained_depth.Get(); }) && ok;
        ok = step([&] { return SUCCEEDED(device->GetViewport(&current_viewport)) && SameViewport(viewport, current_viewport); }) && ok;
        ok = step([&] { return SUCCEEDED(device->GetScissorRect(&current_scissor)) && EqualRect(&scissor, &current_scissor); }) && ok;
        // Keep the original capture and every retained reference until ALL
        // restores and readbacks succeed. Neither a retry nor a rejected Begin
        // may recapture the partially restored device state.
        active = !ok;
        poisoned = !ok;
        if (ok) ReleaseSaved();
        return ok;
    }
};
} // namespace

struct IndependentEyeTargets::Impl {
    Bank bank;
    const char* error = "";
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
    int fail_allocation = -1;
#endif
    bool Fail(const char* why) noexcept { error = why; return false; }
};

IndependentEyeTargets::IndependentEyeTargets() : impl_(std::make_shared<Impl>()) {}
IndependentEyeTargets::~IndependentEyeTargets() {
    // Lifetime belongs to the D3D owner thread. End is still attempted on an
    // abandoned scope; native fields remain the separate game adapter's duty.
    if (impl_->bank.active) (void)impl_->bank.Restore();
}

bool IndependentEyeTargets::EstimatePayload(const IndependentEyeTargetConfig& c,
                                           std::uint64_t& bytes) noexcept {
    bytes = 0;
    if (!Extent(c.width, c.height) || !c.max_payload_bytes || c.max_payload_bytes > kMaxPayload) return false;
    std::uint64_t total = 0;
    auto add = [&](UINT w, UINT h, unsigned bpp, unsigned copies) {
        if (!Extent(w, h) || !bpp) return false;
        const auto size = std::uint64_t(w) * h * bpp * copies;
        if (size > c.max_payload_bytes || total > c.max_payload_bytes - size) return false;
        total += size;
        return true;
    };
    unsigned histories = 0;
    for (const auto& t : c.textures) {
        if (t.usage != D3DUSAGE_RENDERTARGET || !add(t.width, t.height, Bytes(t.format, false), t.history ? 2 : 1)) return false;
        histories += t.history ? 1 : 0;
    }
    if (histories != 2) return false;
    for (std::size_t i = 0; i < c.surfaces.size(); ++i) {
        const auto& d = c.surfaces[i];
        if (d.Type != D3DRTYPE_SURFACE || d.Pool != D3DPOOL_DEFAULT ||
            d.Usage != static_cast<DWORD>(DepthSlot(i) ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET) ||
            d.MultiSampleType != D3DMULTISAMPLE_NONE || d.MultiSampleQuality ||
            !add(d.Width, d.Height, Bytes(d.Format, DepthSlot(i)), 1)) return false;
    }
    if (c.surfaces[0].Width != c.width || c.surfaces[0].Height != c.height ||
        c.surfaces[1].Width != c.width || c.surfaces[1].Height != c.height ||
        c.surfaces[2].Width != c.surfaces[3].Width || c.surfaces[2].Height != c.surfaces[3].Height) return false;
    bytes = total;
    return true;
}

bool IndependentEyeTargets::Create(IDirect3DDevice9* device, std::uint64_t generation,
                                   const IndependentEyeTargetConfig& config) noexcept {
    auto& old = impl_->bank;
    if (!device || !generation || old.active || old.leases || (old.device && !old.Owner()))
        return impl_->Fail("bank creation rejected: device/generation/thread/scope/lease");
    std::uint64_t payload = 0;
    if (!EstimatePayload(config, payload)) return impl_->Fail("bank layout or payload ceiling rejected");
    ComPtr<IDirect3DDevice9Ex> ex;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&ex))) || !ex)
        return impl_->Fail("independent targets require D3D9Ex");
    D3DCAPS9 caps{};
    D3DDEVICE_CREATION_PARAMETERS creation{};
    ComPtr<IDirect3D9> api;
    D3DDISPLAYMODE display{};
    if (FAILED(device->GetDeviceCaps(&caps)) || !caps.NumSimultaneousRTs || caps.NumSimultaneousRTs > 4 ||
        FAILED(device->GetCreationParameters(&creation)) || FAILED(device->GetDirect3D(&api)) ||
        FAILED(api->GetAdapterDisplayMode(creation.AdapterOrdinal, &display)))
        return impl_->Fail("device capabilities unavailable");
    auto format_ok = [&](UINT w, UINT h, D3DFORMAT format, DWORD usage, D3DRESOURCETYPE type) {
        return w <= caps.MaxTextureWidth && h <= caps.MaxTextureHeight &&
            SUCCEEDED(api->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType,
                                            display.Format, usage, type, format));
    };
    for (const auto& t : config.textures)
        if (!format_ok(t.width, t.height, t.format, t.usage, D3DRTYPE_TEXTURE))
            return impl_->Fail("texture dimensions/format unsupported");
    for (std::size_t i = 0; i < config.surfaces.size(); ++i) {
        const auto& d = config.surfaces[i];
        if (!format_ok(d.Width, d.Height, d.Format, d.Usage, D3DRTYPE_SURFACE))
            return impl_->Fail("surface dimensions/format unsupported");
        if (DepthSlot(i)) {
            const auto color = config.surfaces[i == 1 ? 0 : 2].Format;
            if (FAILED(api->CheckDepthStencilMatch(creation.AdapterOrdinal, creation.DeviceType,
                                                 display.Format, color, d.Format)))
                return impl_->Fail("depth/color compatibility rejected");
        }
    }
    Bank next;
    next.device = device; next.owner = GetCurrentThreadId(); next.generation = generation;
    next.bytes = payload; next.width = config.width; next.height = config.height;
    next.mrt_count = caps.NumSimultaneousRTs;
    int ordinal = 0;
    auto allocate = [&]() {
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
        return ordinal++ != impl_->fail_allocation;
#else
        ++ordinal; return true;
#endif
    };
    for (std::size_t i = 0; i < config.textures.size(); ++i) {
        const auto& t = config.textures[i];
        next.histories[i] = t.history;
        for (std::size_t eye = 0; eye < (t.history ? 2U : 1U); ++eye) {
            auto& texture = next.textures[i][eye];
            if (!allocate() || FAILED(device->CreateTexture(t.width, t.height, 1, t.usage,
                    t.format, D3DPOOL_DEFAULT, &texture, nullptr)) || !texture)
                return impl_->Fail("private texture allocation failed");
            D3DSURFACE_DESC desc{};
            const D3DSURFACE_DESC expected{t.format, D3DRTYPE_SURFACE, t.usage,
                D3DPOOL_DEFAULT, D3DMULTISAMPLE_NONE, 0, t.width, t.height};
            ComPtr<IDirect3DSurface9> level;
            D3DSURFACE_DESC level_desc{};
            if (texture->GetLevelCount() != 1 || FAILED(texture->GetLevelDesc(0, &desc)) ||
                !SameDesc(desc, expected) || FAILED(texture->GetSurfaceLevel(0, &level)) || !level ||
                FAILED(level->GetDesc(&level_desc)) || !SameDesc(level_desc, expected))
                return impl_->Fail("private texture description mismatch");
            if (t.history && FAILED(device->ColorFill(level.Get(), nullptr, 0)))
                return impl_->Fail("private history initialization failed");
        }
        if (!t.history) next.textures[i][1] = next.textures[i][0];
    }
    for (std::size_t i = 0; i < config.surfaces.size(); ++i) {
        const auto& d = config.surfaces[i];
        if (!allocate()) return impl_->Fail("private surface allocation fault");
        const HRESULT hr = DepthSlot(i)
            ? device->CreateDepthStencilSurface(d.Width, d.Height, d.Format,
                D3DMULTISAMPLE_NONE, 0, TRUE, &next.surfaces[i], nullptr)
            : device->CreateRenderTarget(d.Width, d.Height, d.Format,
                D3DMULTISAMPLE_NONE, 0, FALSE, &next.surfaces[i], nullptr);
        D3DSURFACE_DESC actual{};
        if (FAILED(hr) || !next.surfaces[i] || FAILED(next.surfaces[i]->GetDesc(&actual)) || !SameDesc(d, actual))
            return impl_->Fail("private surface allocation/description failed");
    }
    if (!allocate() || FAILED(device->CreateStateBlock(D3DSBT_ALL, &next.state)) || !next.state)
        return impl_->Fail("full D3D9 state block allocation failed");
    old = std::move(next);
    impl_->error = "";
    return true;
}

bool IndependentEyeTargets::Begin(IDirect3DDevice9* device, std::uint64_t generation) noexcept {
    auto& b = impl_->bank;
    if (device != b.device.Get() || generation != b.generation || !b.Save())
        return impl_->Fail("D3D9 scope capture/identity rejected");
    bool ok = SUCCEEDED(device->SetDepthStencilSurface(nullptr));
    for (DWORD slot = 1; slot < b.mrt_count; ++slot)
        ok = SUCCEEDED(device->SetRenderTarget(slot, nullptr)) && ok;
    ok = SUCCEEDED(device->SetRenderTarget(0, b.surfaces[0].Get())) && ok;
    ok = SUCCEEDED(device->SetDepthStencilSurface(b.surfaces[1].Get())) && ok;
    const D3DVIEWPORT9 viewport{0, 0, b.width, b.height, 0, 1};
    const RECT scissor{0, 0, static_cast<LONG>(b.width), static_cast<LONG>(b.height)};
    ok = SUCCEEDED(device->SetViewport(&viewport)) && ok;
    ok = SUCCEEDED(device->SetScissorRect(&scissor)) && ok;
    if (!ok) {
        const bool restored = b.Restore();
        return impl_->Fail(restored ? "private target binding failed; restored" : "private target binding and restoration failed");
    }
    impl_->error = "";
    return true;
}

bool IndependentEyeTargets::RestoreAll() noexcept {
    if (!impl_->bank.Restore()) return impl_->Fail("D3D9 state restoration/verification failed");
    impl_->error = ""; return true;
}
bool IndependentEyeTargets::End() noexcept { return RestoreAll(); }
bool IndependentEyeTargets::ClearHistories() noexcept {
    auto& b = impl_->bank;
    if (!b.Owner() || b.active || b.poisoned)
        return impl_->Fail("history clearing rejected: thread/scope/recovery");
    for (std::size_t i = 0; i < b.textures.size(); ++i) {
        if (!b.histories[i]) continue;
        for (const auto& texture : b.textures[i]) {
            ComPtr<IDirect3DSurface9> level;
            if (FAILED(texture->GetSurfaceLevel(0, &level)) || !level ||
                FAILED(b.device->ColorFill(level.Get(), nullptr, 0)))
                return impl_->Fail("private history clearing failed");
        }
    }
    impl_->error = "";
    return true;
}
IDirect3DTexture9* IndependentEyeTargets::texture(std::size_t index, std::size_t eye) const noexcept {
    const auto& b = impl_->bank;
    return b.Owner() && !b.poisoned && index < 14 && eye < 2 ? b.textures[index][eye].Get() : nullptr;
}
IDirect3DSurface9* IndependentEyeTargets::surface(IndependentEyeSurface slot) const noexcept {
    const auto& b = impl_->bank;
    const auto i = static_cast<std::size_t>(slot);
    return b.Owner() && !b.poisoned && i < kSurfaceCount ? b.surfaces[i].Get() : nullptr;
}
bool IndependentEyeTargets::MirrorRightTo(IDirect3DSurface9* destination) noexcept {
    auto& b = impl_->bank;
    if (!b.Owner() || b.active || b.poisoned || !destination || destination == b.surfaces[0].Get())
        return impl_->Fail("mirror ownership/scope rejected");
    ComPtr<IDirect3DDevice9> destination_device;
    D3DSURFACE_DESC d{}, source{};
    if (FAILED(destination->GetDevice(&destination_device)) || destination_device.Get() != b.device.Get() ||
        FAILED(destination->GetDesc(&d)) || FAILED(b.surfaces[0]->GetDesc(&source)) ||
        !d.Width || !d.Height || d.Pool != D3DPOOL_DEFAULT || !(d.Usage & D3DUSAGE_RENDERTARGET) ||
        d.MultiSampleType != D3DMULTISAMPLE_NONE || d.MultiSampleQuality || !Bytes(d.Format, false) ||
        d.Width > kMaxExtent || d.Height > kMaxExtent || !b.Save())
        return impl_->Fail("mirror destination/capture rejected");
    UINT width = d.Width, height = d.Height;
    if (std::uint64_t(d.Width) * source.Height > std::uint64_t(d.Height) * source.Width)
        width = static_cast<UINT>(std::uint64_t(d.Height) * source.Width / source.Height);
    else height = static_cast<UINT>(std::uint64_t(d.Width) * source.Height / source.Width);
    const LONG x = static_cast<LONG>((d.Width - width) / 2), y = static_cast<LONG>((d.Height - height) / 2);
    const RECT rect{x, y, x + static_cast<LONG>(width), y + static_cast<LONG>(height)};
    const bool copied = width && height && SUCCEEDED(b.device->ColorFill(destination, nullptr, 0)) &&
        SUCCEEDED(b.device->StretchRect(b.surfaces[0].Get(), nullptr, destination, &rect, D3DTEXF_LINEAR));
    const bool restored = b.Restore();
    if (!copied || !restored) return impl_->Fail(restored ? "mirror copy failed" : "mirror state restoration failed");
    impl_->error = ""; return true;
}
IndependentEyeTargets::Lease IndependentEyeTargets::AcquireLease() noexcept {
    auto& b = impl_->bank;
    Lease lease;
    if (!b.Owner() || b.poisoned || b.leases == (std::numeric_limits<std::uint32_t>::max)()) {
        (void)impl_->Fail("source lease rejected"); return lease;
    }
    lease.impl_ = impl_;
    ++b.leases; return lease;
}
IndependentEyeTargets::Lease::~Lease() { Reset(); }
IndependentEyeTargets::Lease::Lease(Lease&& other) noexcept : impl_(std::move(other.impl_)) {}
IndependentEyeTargets::Lease& IndependentEyeTargets::Lease::operator=(Lease&& other) noexcept {
    if (this != &other) { Reset(); impl_ = std::move(other.impl_); }
    return *this;
}
void IndependentEyeTargets::Lease::Reset() noexcept {
    if (impl_) { --impl_->bank.leases; impl_.reset(); }
}
bool IndependentEyeTargets::InvalidateBeforeReset() noexcept {
    auto& b = impl_->bank;
    if (b.active || b.leases || (b.device && !b.Owner())) return impl_->Fail("bank retirement deferred: scope/lease/thread");
    b = Bank{}; impl_->error = ""; return true;
}
bool IndependentEyeTargets::ready() const noexcept { return impl_->bank.Owner() && !impl_->bank.poisoned; }
bool IndependentEyeTargets::active() const noexcept { return impl_->bank.active; }
std::uint64_t IndependentEyeTargets::payload_bytes() const noexcept { return impl_->bank.bytes; }
std::uint64_t IndependentEyeTargets::generation() const noexcept { return impl_->bank.generation; }
std::string_view IndependentEyeTargets::last_error() const noexcept { return impl_->error; }
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
void IndependentEyeTargets::FailAllocationForTest(int ordinal) noexcept { impl_->fail_allocation = ordinal; }
void IndependentEyeTargets::FailRestoreForTest(int ordinal) noexcept { impl_->bank.fail_restore = ordinal; }
std::uint32_t IndependentEyeTargets::RestoreAttemptsForTest() const noexcept { return impl_->bank.restore_attempts; }
#endif
} // namespace cojvr::backends::d3d9
