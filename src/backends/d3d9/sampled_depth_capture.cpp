#include "backends/d3d9/sampled_depth_capture.hpp"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <new>
#include <utility>

namespace cojvr::backends::d3d9 {
namespace {
using Microsoft::WRL::ComPtr;
constexpr auto kIntz = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
constexpr HRESULT kBusy = HRESULT_FROM_WIN32(ERROR_BUSY);

void FirstFailure(HRESULT candidate, HRESULT& result) noexcept {
    if (FAILED(candidate) && SUCCEEDED(result)) result = candidate;
}
bool SameViewport(const D3DVIEWPORT9& a, const D3DVIEWPORT9& b) noexcept {
    return a.X == b.X && a.Y == b.Y && a.Width == b.Width && a.Height == b.Height &&
        a.MinZ == b.MinZ && a.MaxZ == b.MaxZ;
}
HRESULT GetTarget(IDirect3DDevice9* d, DWORD slot, ComPtr<IDirect3DSurface9>& p) noexcept {
    const auto hr = d->GetRenderTarget(slot, p.ReleaseAndGetAddressOf());
    return slot && hr == D3DERR_NOTFOUND ? S_OK : hr;
}
HRESULT GetDepth(IDirect3DDevice9* d, ComPtr<IDirect3DSurface9>& p) noexcept {
    const auto hr = d->GetDepthStencilSurface(p.ReleaseAndGetAddressOf());
    return hr == D3DERR_NOTFOUND ? S_OK : hr;
}

struct SavedState {
    ComPtr<IDirect3DStateBlock9> block;
    std::array<ComPtr<IDirect3DSurface9>, 4> targets;
    ComPtr<IDirect3DSurface9> depth;
    D3DVIEWPORT9 viewport{};
    RECT scissor{};
    DWORD mrt_count = 0;
    bool active = false;
#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
    int fail_operation = -1;
#endif

    HRESULT Save(IDirect3DDevice9* d, DWORD count) noexcept {
        if (active) return D3DERR_INVALIDCALL;
        mrt_count = count;
        HRESULT hr = d->CreateStateBlock(D3DSBT_ALL, block.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return hr;
        hr = block->Capture();
        for (DWORD i = 0; i < mrt_count; ++i) FirstFailure(GetTarget(d, i, targets[i]), hr);
        FirstFailure(GetDepth(d, depth), hr);
        FirstFailure(d->GetViewport(&viewport), hr);
        FirstFailure(d->GetScissorRect(&scissor), hr);
        active = SUCCEEDED(hr);
        if (!active) Release();
        return hr;
    }
    void Release() noexcept {
        block.Reset();
        for (auto& t : targets) t.Reset();
        depth.Reset();
    }
    HRESULT Restore(IDirect3DDevice9* d) noexcept {
        if (!active) return S_OK;
        HRESULT hr = S_OK;
        unsigned ordinal = 0;
        auto attempt = [&](auto operation) {
#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
            if (static_cast<int>(ordinal++) == fail_operation) {
                FirstFailure(E_FAIL, hr);
                return;
            }
#else
            (void)ordinal;
#endif
            FirstFailure(operation(), hr);
        };
        // Detach our sampling binding BEFORE restoring native DS. Apply the
        // native texture state before DS as well; RT0 changes reset the viewport.
        attempt([&] { return d->SetTexture(0, nullptr); });
        attempt([&] { return d->SetDepthStencilSurface(nullptr); });
        for (DWORD i = 1; i < mrt_count; ++i)
            attempt([&] { return d->SetRenderTarget(i, nullptr); });
        for (DWORD i = 0; i < mrt_count; ++i)
            attempt([&] { return d->SetRenderTarget(i, targets[i].Get()); });
        attempt([&] { return block->Apply(); });
        attempt([&] { return d->SetDepthStencilSurface(depth.Get()); });
        attempt([&] { return d->SetViewport(&viewport); });
        attempt([&] { return d->SetScissorRect(&scissor); });
        // Check the explicit state as well as every API result. The ALL block
        // owns shaders, declaration/FVF, textures/samplers, streams and renderstate.
        for (DWORD i = 0; i < mrt_count; ++i) {
            ComPtr<IDirect3DSurface9> current;
            FirstFailure(GetTarget(d, i, current), hr);
            if (current.Get() != targets[i].Get()) FirstFailure(E_FAIL, hr);
        }
        ComPtr<IDirect3DSurface9> current_depth;
        D3DVIEWPORT9 current_viewport{};
        RECT current_scissor{};
        FirstFailure(GetDepth(d, current_depth), hr);
        if (current_depth.Get() != depth.Get()) FirstFailure(E_FAIL, hr);
        FirstFailure(d->GetViewport(&current_viewport), hr);
        FirstFailure(d->GetScissorRect(&current_scissor), hr);
        if (!SameViewport(current_viewport, viewport) || !EqualRect(&scissor, &current_scissor))
            FirstFailure(E_FAIL, hr);
#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
        fail_operation = -1;
#endif
        if (SUCCEEDED(hr)) { active = false; Release(); }
        return hr;
    }
};

struct Slot {
    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DSurface9> surface;
    ComPtr<IDirect3DTexture9> color_texture;
    ComPtr<IDirect3DSurface9> color_surface;
    ComPtr<IDirect3DQuery9> query;
    ComPtr<IDirect3DSurface9> preview_color, preview_depth;
    HANDLE handle = nullptr;
    HANDLE color_handle = nullptr;
    std::weak_ptr<void> lease;
    bool pending = false, flushed = false, poisoned = false;
};
std::uint64_t CaptureBytes(const D3D9SampledDepthConfig& config) noexcept {
    return std::uint64_t{config.width}*config.height*
        (4+(config.capture_color?16:8)*D3D9SampledDepthCapture::slot_count);
}
struct Pool {
    ComPtr<IDirect3DDevice9Ex> device;
    ComPtr<IDirect3DTexture9> depth_texture;
    ComPtr<IDirect3DSurface9> depth_surface;
    ComPtr<IDirect3DPixelShader9> shader;
    std::array<Slot, D3D9SampledDepthCapture::slot_count> slots;
    SavedState saved;
    D3D9SampledDepthConfig config;
    DWORD thread = GetCurrentThreadId(), mrt_count = 0;
    std::uint64_t last_sequence = 0;
    bool poisoned = false;

    bool Owner() const noexcept { return thread == GetCurrentThreadId(); }
    HRESULT Poll(std::size_t index) noexcept {
        if (!Owner()) return D3DERR_INVALIDCALL;
        auto& s = slots[index];
        if (s.poisoned) return E_FAIL;
        if (!s.pending) return S_OK;
        const DWORD flags = s.flushed ? 0 : D3DGETDATA_FLUSH;
        s.flushed = true;
        const auto hr = s.query->GetData(nullptr, 0, flags);
        if (hr == S_OK) s.pending = false;
        else if (hr != S_FALSE) { s.poisoned = true; return FAILED(hr) ? hr : E_FAIL; }
        return hr;
    }
};

HRESULT ConfigureAndDraw(Pool& p, Slot& s) noexcept {
    auto* d = p.device.Get();
    HRESULT hr = d->SetDepthStencilSurface(nullptr); // no DS sampling feedback
    for (DWORD i = 1; SUCCEEDED(hr) && i < p.mrt_count; ++i)
        hr = d->SetRenderTarget(i, nullptr);
    if (SUCCEEDED(hr)) hr = d->SetRenderTarget(0, s.surface.Get());
    if (FAILED(hr)) return hr;
    const D3DVIEWPORT9 viewport{0, 0, p.config.width, p.config.height, 0.0F, 1.0F};
    hr = d->SetViewport(&viewport);
    if (SUCCEEDED(hr)) hr = d->SetVertexShader(nullptr);
    if (SUCCEEDED(hr)) hr = d->SetPixelShader(p.shader.Get());
    if (SUCCEEDED(hr)) hr = d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    if (SUCCEEDED(hr)) hr = d->SetStreamSourceFreq(0, 1);
    if (SUCCEEDED(hr)) hr = d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    if (SUCCEEDED(hr)) hr = d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    // Remove native states that could clip, discard, blend or mask the snapshot.
    constexpr std::pair<D3DRENDERSTATETYPE, DWORD> states[] = {
        {D3DRS_ZENABLE, FALSE}, {D3DRS_ZWRITEENABLE, FALSE},
        {D3DRS_STENCILENABLE, FALSE}, {D3DRS_ALPHATESTENABLE, FALSE},
        {D3DRS_ALPHABLENDENABLE, FALSE}, {D3DRS_SEPARATEALPHABLENDENABLE, FALSE},
        {D3DRS_FOGENABLE, FALSE}, {D3DRS_LIGHTING, FALSE},
        {D3DRS_SCISSORTESTENABLE, FALSE}, {D3DRS_SRGBWRITEENABLE, FALSE},
        {D3DRS_CULLMODE, D3DCULL_NONE}, {D3DRS_FILLMODE, D3DFILL_SOLID},
        {D3DRS_CLIPPLANEENABLE, 0}, {D3DRS_COLORWRITEENABLE, 15},
        {D3DRS_MULTISAMPLEANTIALIAS, FALSE}, {D3DRS_MULTISAMPLEMASK, 0xffffffff},
        {D3DRS_WRAP0, 0}, {D3DRS_DEPTHBIAS, 0}, {D3DRS_SLOPESCALEDEPTHBIAS, 0},
    };
    for (const auto& state : states) {
        if (FAILED(hr)) break;
        hr = d->SetRenderState(state.first, state.second);
    }
    constexpr std::pair<D3DSAMPLERSTATETYPE, DWORD> samplers[] = {
        {D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP},
        {D3DSAMP_MINFILTER, D3DTEXF_POINT}, {D3DSAMP_MAGFILTER, D3DTEXF_POINT},
        {D3DSAMP_MIPFILTER, D3DTEXF_NONE}, {D3DSAMP_SRGBTEXTURE, FALSE},
        {D3DSAMP_MAXMIPLEVEL, 0}, {D3DSAMP_MIPMAPLODBIAS, 0},
    };
    for (const auto& state : samplers) {
        if (FAILED(hr)) break;
        hr = d->SetSamplerState(0, state.first, state.second);
    }
    if (SUCCEEDED(hr)) hr = d->SetTexture(0, p.depth_texture.Get());
    if (FAILED(hr)) return hr;
    struct Vertex { float x, y, z, rhw, u, v; };
    const float right = static_cast<float>(p.config.width) - 0.5F;
    const float bottom = static_cast<float>(p.config.height) - 0.5F;
    // D3D9 half-pixel convention: integer pixel centers sample texel centers.
    const Vertex vertices[] = {
        {-0.5F, -0.5F, 0, 1, 0, 0}, {right, -0.5F, 0, 1, 1, 0},
        {-0.5F, bottom, 0, 1, 0, 1}, {right, bottom, 0, 1, 1, 1},
    };
    return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
}
} // namespace

struct D3D9SampledDepthSnapshot::Impl {
    std::shared_ptr<Pool> pool;
    std::size_t slot = 0;
    std::uint64_t sequence = 0;
    std::atomic<HRESULT> ready{S_FALSE};
    UINT preview_width = 0, preview_height = 0;
};
bool D3D9SampledDepthSnapshot::valid() const noexcept { return !!impl_; }
HRESULT D3D9SampledDepthSnapshot::PollReady() const noexcept {
    if (!impl_ || !impl_->pool->Owner()) return D3DERR_INVALIDCALL;
    auto hr = impl_->ready.load();
    if (hr != S_FALSE) return hr;
    hr = impl_->pool->Poll(impl_->slot);
    impl_->ready.store(hr);
    return hr;
}
HANDLE D3D9SampledDepthSnapshot::shared_handle() const noexcept {
    return impl_ && impl_->ready.load() == S_OK ? impl_->pool->slots[impl_->slot].handle : nullptr;
}
HANDLE D3D9SampledDepthSnapshot::color_shared_handle() const noexcept {
    return impl_ && impl_->ready.load() == S_OK ? impl_->pool->slots[impl_->slot].color_handle : nullptr;
}
std::uintptr_t D3D9SampledDepthSnapshot::producer_device() const noexcept {
    return impl_ ? reinterpret_cast<std::uintptr_t>(static_cast<IDirect3DDevice9*>(impl_->pool->device.Get())) : 0;
}
std::uint32_t D3D9SampledDepthSnapshot::producer_thread() const noexcept {
    return impl_ ? impl_->pool->thread : 0;
}
HRESULT D3D9SampledDepthSnapshot::ReadDiagnosticPreview(D3D9SampledDepthPreview& output) const noexcept {
    if (!impl_ || !impl_->pool->Owner() || !shared_handle() || !impl_->preview_width)
        return D3DERR_INVALIDCALL;
    try {
        D3D9SampledDepthPreview result;
        result.width = impl_->preview_width; result.height = impl_->preview_height;
        const auto read = [&](IDirect3DSurface9* source, std::vector<std::uint16_t>& pixels) {
            ComPtr<IDirect3DSurface9> system;
            auto* device = impl_->pool->device.Get();
            auto hr = device->CreateOffscreenPlainSurface(result.width,result.height,format,
                D3DPOOL_SYSTEMMEM,&system,nullptr);
            if (SUCCEEDED(hr)) hr = device->GetRenderTargetData(source,system.Get());
            if (FAILED(hr)) return hr;
            pixels.resize(static_cast<std::size_t>(result.width)*result.height*4);
            D3DLOCKED_RECT locked{};
            hr = system->LockRect(&locked,nullptr,D3DLOCK_READONLY);
            if (FAILED(hr)) return hr;
            const auto row_bytes = static_cast<std::size_t>(result.width)*8;
            if (!locked.pBits || locked.Pitch < static_cast<INT>(row_bytes)) hr = E_FAIL;
            for (UINT y=0; SUCCEEDED(hr) && y<result.height; ++y)
                std::memcpy(pixels.data()+static_cast<std::size_t>(y)*result.width*4,
                    static_cast<const std::uint8_t*>(locked.pBits)+static_cast<std::size_t>(y)*locked.Pitch,row_bytes);
            const auto unlocked = system->UnlockRect();
            return FAILED(hr) ? hr : unlocked;
        };
        const auto& slot = impl_->pool->slots[impl_->slot];
        auto hr = read(slot.preview_color.Get(),result.color_rgba16f);
        if (SUCCEEDED(hr)) hr = read(slot.preview_depth.Get(),result.depth_rgba16f);
        if (FAILED(hr)) return hr;
        output = std::move(result);
        return S_OK;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
HRESULT D3D9SampledDepthSnapshot::ReadDiagnosticPoint(D3D9SampledDepthPoint& output) const noexcept {
    if(!impl_||!impl_->pool->Owner()||!shared_handle()||!color_shared_handle())return D3DERR_INVALIDCALL;
    auto* device=impl_->pool->device.Get();
    ComPtr<IDirect3DSurface9> target,system;
    auto hr=device->CreateRenderTarget(1,1,format,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
    if(SUCCEEDED(hr))hr=device->CreateOffscreenPlainSurface(1,1,format,D3DPOOL_SYSTEMMEM,&system,nullptr);
    if(FAILED(hr))return hr;
    const RECT point{static_cast<LONG>(width()/2),static_cast<LONG>(height()/2),
        static_cast<LONG>(width()/2+1),static_cast<LONG>(height()/2+1)};
    const auto& slot=impl_->pool->slots[impl_->slot];
    IDirect3DSurface9* sources[]{slot.color_surface.Get(),slot.surface.Get()};
    D3D9SampledDepthPoint result;
    for(unsigned plane=0;plane<2;++plane){
        hr=device->StretchRect(sources[plane],&point,target.Get(),nullptr,D3DTEXF_NONE);
        if(SUCCEEDED(hr))hr=device->GetRenderTargetData(target.Get(),system.Get());
        D3DLOCKED_RECT locked{};
        if(SUCCEEDED(hr))hr=system->LockRect(&locked,nullptr,D3DLOCK_READONLY);
        if(FAILED(hr))return hr;
        if(locked.pBits&&locked.Pitch>=8)
            std::memcpy((plane?result.depth:result.color).data(),locked.pBits,8);
        else hr=E_FAIL;
        const auto unlocked=system->UnlockRect();
        if(FAILED(hr)||FAILED(unlocked))return FAILED(hr)?hr:unlocked;
    }
    output=result;return S_OK;
}
HRESULT D3D9SampledDepthSnapshot::ReadDiagnosticGrid(std::array<std::uint32_t,64>& output) const noexcept {
    if (!impl_ || !impl_->pool->Owner() || !shared_handle()) return D3DERR_INVALIDCALL;
    auto& pool = *impl_->pool;
    ComPtr<IDirect3DSurface9> target, system;
    auto hr = pool.device->CreateRenderTarget(8,8,format,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
    if (SUCCEEDED(hr)) hr = pool.device->CreateOffscreenPlainSurface(8,8,format,D3DPOOL_SYSTEMMEM,&system,nullptr);
    if (SUCCEEDED(hr)) hr = pool.device->StretchRect(pool.slots[impl_->slot].surface.Get(),nullptr,target.Get(),nullptr,D3DTEXF_POINT);
    if (SUCCEEDED(hr)) hr = pool.device->GetRenderTargetData(target.Get(),system.Get());
    D3DLOCKED_RECT locked{};
    if (SUCCEEDED(hr)) hr = system->LockRect(&locked,nullptr,D3DLOCK_READONLY);
    if (FAILED(hr)) return hr;
    std::array<std::uint32_t,64> samples{};
    const auto byte = [](std::uint16_t half) noexcept -> int {
        if (half == 0) return 0;
        if (half & 0x8000) return -1;
        const int exponent = (half >> 10) & 31;
        const int mantissa = half & 1023;
        if (!exponent || exponent > 22) return -1;
        const float value = std::ldexp(static_cast<float>(1024 + mantissa), exponent - 25);
        return value <= 255 && value == std::floor(value) ? static_cast<int>(value) : -1;
    };
    if (!locked.pBits || locked.Pitch < 64) hr = E_FAIL;
    for (unsigned y = 0; SUCCEEDED(hr) && y < 8; ++y) {
        const auto* row = reinterpret_cast<const std::uint16_t*>(
            static_cast<const std::uint8_t*>(locked.pBits) + y * locked.Pitch);
        for (unsigned x = 0; x < 8; ++x) {
            const auto r = byte(row[4*x]), g = byte(row[4*x+1]), b = byte(row[4*x+2]);
            if (r < 0 || g < 0 || b < 0) { hr = E_FAIL; break; }
            samples[y*8+x] = static_cast<std::uint32_t>(r*65536 + g*256 + b);
        }
    }
    const auto unlock = system->UnlockRect();
    if (FAILED(hr)) return hr;
    if (FAILED(unlock)) return unlock;
    output = samples;
    return S_OK;
}
UINT D3D9SampledDepthSnapshot::width() const noexcept { return impl_ ? impl_->pool->config.width : 0; }
UINT D3D9SampledDepthSnapshot::height() const noexcept { return impl_ ? impl_->pool->config.height : 0; }
std::uint64_t D3D9SampledDepthSnapshot::sequence() const noexcept { return impl_ ? impl_->sequence : 0; }
std::uint64_t D3D9SampledDepthSnapshot::generation() const noexcept {
    return impl_ ? impl_->pool->config.generation : 0;
}

struct D3D9SampledDepthCapture::Impl {
    std::shared_ptr<Pool> pool;
    const char* error = "";
    HRESULT Fail(const char* message, HRESULT hr) noexcept { error = message; return hr; }
};
D3D9SampledDepthCapture::D3D9SampledDepthCapture() : impl_(std::make_unique<Impl>()) {}
D3D9SampledDepthCapture::~D3D9SampledDepthCapture() {
    if (impl_->pool && impl_->pool->Owner() && impl_->pool->saved.active)
        (void)impl_->pool->saved.Restore(impl_->pool->device.Get());
}

HRESULT D3D9SampledDepthCapture::CheckSupport(
    IDirect3DDevice9* device, const D3D9SampledDepthConfig& config) noexcept {
    if (!device || !config.width || !config.height || !config.generation ||
        config.width > max_extent || config.height > max_extent ||
        CaptureBytes(config) > max_payload_bytes ||
        (config.capture_color && config.native_color_format != D3D9SampledDepthSnapshot::format) ||
        (config.native_depth_format != D3DFMT_D24S8 && config.native_depth_format != D3DFMT_D24X8) ||
        config.multisample != D3DMULTISAMPLE_NONE || config.multisample_quality)
        return D3DERR_INVALIDCALL;
    ComPtr<IDirect3DDevice9Ex> ex;
    auto hr = device->QueryInterface(IID_PPV_ARGS(&ex));
    if (FAILED(hr)) return hr;
    D3DDEVICE_CREATION_PARAMETERS creation{};
    D3DCAPS9 caps{};
    ComPtr<IDirect3D9> api;
    hr = device->GetCreationParameters(&creation);
    if (SUCCEEDED(hr)) hr = device->GetDeviceCaps(&caps);
    if (SUCCEEDED(hr)) hr = device->GetDirect3D(&api);
    if (FAILED(hr)) return hr;
    if ((creation.BehaviorFlags & D3DCREATE_PUREDEVICE) ||
        caps.PixelShaderVersion < D3DPS_VERSION(3, 0) || !caps.NumSimultaneousRTs ||
        config.width > caps.MaxTextureWidth || config.height > caps.MaxTextureHeight)
        return D3DERR_NOTAVAILABLE;
    D3DDISPLAYMODE mode{};
    hr = api->GetAdapterDisplayMode(creation.AdapterOrdinal, &mode);
    if (FAILED(hr)) return hr;
    const auto check_format = [&](DWORD usage, D3DFORMAT format, D3DRESOURCETYPE type) {
        return api->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType,
            mode.Format, usage, type, format);
    };
    hr = check_format(D3DUSAGE_RENDERTARGET, config.native_color_format, D3DRTYPE_SURFACE);
    if (SUCCEEDED(hr)) hr = check_format(D3DUSAGE_DEPTHSTENCIL, config.native_depth_format, D3DRTYPE_SURFACE);
    if (SUCCEEDED(hr)) hr = check_format(D3DUSAGE_DEPTHSTENCIL, kIntz, D3DRTYPE_TEXTURE);
    if (SUCCEEDED(hr)) hr = check_format(D3DUSAGE_RENDERTARGET,
        D3D9SampledDepthSnapshot::format, D3DRTYPE_TEXTURE);
    for (auto format : {config.native_depth_format, kIntz}) {
        if (FAILED(hr)) break;
        hr = api->CheckDepthStencilMatch(creation.AdapterOrdinal, creation.DeviceType,
            mode.Format, config.native_color_format, format);
    }
    if (SUCCEEDED(hr)) hr = device->CreateQuery(D3DQUERYTYPE_EVENT, nullptr);
    return hr;
}

HRESULT D3D9SampledDepthCapture::Initialize(
    IDirect3DDevice9* device, const D3D9SampledDepthConfig& config) noexcept {
    if (impl_->pool) return impl_->Fail("already initialized; use a new owner for a new generation", D3DERR_INVALIDCALL);
    auto hr = CheckSupport(device, config);
    if (FAILED(hr)) return impl_->Fail("INTZ/non-MSAA/D24S8/D3D9Ex capability rejected", hr);
    try {
        auto p = std::make_shared<Pool>();
        p->config = config;
        hr = device->QueryInterface(IID_PPV_ARGS(&p->device));
        D3DCAPS9 caps{};
        if (SUCCEEDED(hr)) hr = device->GetDeviceCaps(&caps);
        if (FAILED(hr)) return impl_->Fail("D3D9Ex device capabilities failed", hr);
        p->mrt_count = caps.NumSimultaneousRTs < 4 ? caps.NumSimultaneousRTs : 4;
        if (SUCCEEDED(hr)) hr = device->CreateTexture(config.width, config.height, 1,
            D3DUSAGE_DEPTHSTENCIL, kIntz, D3DPOOL_DEFAULT, &p->depth_texture, nullptr);
        if (SUCCEEDED(hr)) hr = p->depth_texture->GetSurfaceLevel(0, &p->depth_surface);
        if (FAILED(hr)) return impl_->Fail("INTZ texture or depth surface creation failed", hr);
        for (auto& slot : p->slots) {
            if (FAILED(hr)) break;
            hr = device->CreateTexture(config.width, config.height, 1, D3DUSAGE_RENDERTARGET,
                D3D9SampledDepthSnapshot::format, D3DPOOL_DEFAULT, &slot.texture, &slot.handle);
            if (FAILED(hr) || !slot.handle)
                return impl_->Fail("shared depth carrier creation failed", FAILED(hr) ? hr : E_FAIL);
            if (SUCCEEDED(hr)) hr = slot.texture->GetSurfaceLevel(0, &slot.surface);
            if (SUCCEEDED(hr) && config.capture_color) {
                hr = device->CreateTexture(config.width,config.height,1,D3DUSAGE_RENDERTARGET,
                    D3D9SampledDepthSnapshot::format,D3DPOOL_DEFAULT,&slot.color_texture,&slot.color_handle);
                if (FAILED(hr) || !slot.color_handle)
                    return impl_->Fail("shared immutable color carrier creation failed",FAILED(hr)?hr:E_FAIL);
                hr = slot.color_texture->GetSurfaceLevel(0,&slot.color_surface);
            }
            if (SUCCEEDED(hr)) hr = device->CreateQuery(D3DQUERYTYPE_EVENT, &slot.query);
            if (FAILED(hr)) return impl_->Fail("shared depth surface or query creation failed", hr);
        }
        // INTZ .r is raw device Z, not hardware shadow comparison. Store exact
        // integer bytes in FP16 RGB. Evaluate round(z*16777215) in split words:
        // adding .5 to a large FP32 integer directly would round odd integers up
        // before floor(), losing a D24 bit. The low-word addition avoids that.
        constexpr char shader[] =
            "sampler2D depth : register(s0);"
            "float4 main(float2 uv : TEXCOORD0) : COLOR0 {"
            "float z = saturate(tex2D(depth, uv).r);"
            "float scaled = z * 16777215.0;"
            "float high = floor(scaled / 65536.0);"
            "float low = floor((scaled - high * 65536.0) + 0.5);"
            "float carry = floor(low / 65536.0);"
            "high += carry; low -= carry * 65536.0;"
            "float green = floor(low / 256.0);"
            "return float4(high, green, low - green * 256.0, z); }";
        ComPtr<ID3DBlob> code, errors;
        if (SUCCEEDED(hr)) hr = D3DCompile(shader, sizeof(shader) - 1, "sampled_device_z",
            nullptr, nullptr, "main", "ps_3_0", 0, 0, &code, &errors);
        if (FAILED(hr)) return impl_->Fail("device Z pixel shader compilation failed", hr);
        if (SUCCEEDED(hr)) hr = device->CreatePixelShader(
            static_cast<const DWORD*>(code->GetBufferPointer()), &p->shader);
        if (FAILED(hr)) return impl_->Fail("device Z pixel shader creation failed", hr);
        impl_->pool = std::move(p);
        impl_->error = "";
        return S_OK;
    } catch (const std::bad_alloc&) { return impl_->Fail("allocation failed", E_OUTOFMEMORY); }
    catch (...) { return impl_->Fail("unexpected initialization failure", E_FAIL); }
}

IDirect3DSurface9* D3D9SampledDepthCapture::native_depth_surface() const noexcept {
    const auto& p = impl_->pool;
    return p && p->Owner() && !p->poisoned && !p->saved.active ? p->depth_surface.Get() : nullptr;
}

HRESULT D3D9SampledDepthCapture::Snapshot(std::uint64_t sequence,
    std::uint64_t generation, D3D9SampledDepthSnapshot& output, bool diagnostic_preview) noexcept {
    const auto p = impl_->pool;
    if (!p || !p->Owner() || p->poisoned || p->saved.active || output.valid() ||
        !sequence || sequence <= p->last_sequence || generation != p->config.generation)
        return impl_->Fail("invalid owner/identity/output or unresolved restoration", D3DERR_INVALIDCALL);
    ComPtr<IDirect3DSurface9> bound_depth, bound_color;
    auto hr = GetDepth(p->device.Get(), bound_depth);
    if (SUCCEEDED(hr)) hr = GetTarget(p->device.Get(), 0, bound_color);
    D3DSURFACE_DESC color{};
    if (SUCCEEDED(hr) && bound_color) hr = bound_color->GetDesc(&color);
    if (FAILED(hr) || bound_depth.Get() != p->depth_surface.Get() || !bound_color ||
        color.Width != p->config.width || color.Height != p->config.height ||
        color.Format != p->config.native_color_format || color.MultiSampleType != D3DMULTISAMPLE_NONE ||
        color.MultiSampleQuality)
        return impl_->Fail("caller must bind our depth and a compatible native RT before rendering", FAILED(hr) ? hr : D3DERR_INVALIDCALL);
    // Reject a pre-existing DS/sampler feedback loop; restoration must never
    // recreate such a loop. Check every pixel and vertex texture binding.
    constexpr DWORD texture_slots[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        D3DDMAPSAMPLER, D3DVERTEXTEXTURESAMPLER0, D3DVERTEXTEXTURESAMPLER1,
        D3DVERTEXTEXTURESAMPLER2, D3DVERTEXTEXTURESAMPLER3};
    for (DWORD slot : texture_slots) {
        ComPtr<IDirect3DBaseTexture9> texture;
        hr = p->device->GetTexture(slot, &texture);
        if (FAILED(hr) || texture.Get() == p->depth_texture.Get())
            return impl_->Fail("invalid native texture state or depth sampling feedback", FAILED(hr) ? hr : D3DERR_INVALIDCALL);
    }
    std::size_t index = slot_count;
    for (std::size_t i = 0; i < slot_count; ++i) {
        auto& slot = p->slots[i];
        if (slot.lease.expired() && !slot.poisoned && p->Poll(i) == S_OK) { index = i; break; }
    }
    if (index == slot_count) return impl_->Fail("bounded capture slots are leased, pending or quarantined", kBusy);
    try {
        auto capture = std::make_shared<D3D9SampledDepthSnapshot::Impl>();
        capture->pool = p; capture->slot = index; capture->sequence = sequence;
        auto& slot = p->slots[index];
        if (diagnostic_preview) {
            if (color.Format != D3D9SampledDepthSnapshot::format)
                return impl_->Fail("joint preview requires native FP16 color",D3DERR_INVALIDCALL);
            const auto extent = std::max(color.Width,color.Height);
            const auto preview_extent = std::min<UINT>(extent,D3D9SampledDepthPreview::max_extent);
            capture->preview_width = std::max<UINT>(1,color.Width*preview_extent/extent);
            capture->preview_height = std::max<UINT>(1,color.Height*preview_extent/extent);
            const auto native_bytes=CaptureBytes(p->config);
            const auto preview_bytes=std::uint64_t{capture->preview_width}*capture->preview_height*16*slot_count;
            if (native_bytes+preview_bytes>max_payload_bytes)
                return impl_->Fail("joint preview exceeds bounded capture storage",E_OUTOFMEMORY);
            for (auto* surface : {std::addressof(slot.preview_color),std::addressof(slot.preview_depth)}) {
                if (surface->Get()) continue;
                hr = p->device->CreateRenderTarget(capture->preview_width,capture->preview_height,
                    D3D9SampledDepthSnapshot::format,D3DMULTISAMPLE_NONE,0,FALSE,
                    surface->ReleaseAndGetAddressOf(),nullptr);
                if (FAILED(hr)) return impl_->Fail("joint preview allocation failed",hr);
            }
        }
        hr = p->saved.Save(p->device.Get(), p->mrt_count);
        if (FAILED(hr)) return impl_->Fail("native state capture failed before mutation", hr);
        HRESULT draw_hr = S_OK;
        if (p->config.capture_color)
            draw_hr = p->device->StretchRect(bound_color.Get(),nullptr,slot.color_surface.Get(),nullptr,D3DTEXF_NONE);
        if (SUCCEEDED(draw_hr) && diagnostic_preview)
            draw_hr = p->device->StretchRect(bound_color.Get(),nullptr,slot.preview_color.Get(),nullptr,D3DTEXF_POINT);
        if (SUCCEEDED(draw_hr)) draw_hr = ConfigureAndDraw(*p, slot);
        if (SUCCEEDED(draw_hr) && diagnostic_preview)
            draw_hr = p->device->StretchRect(slot.surface.Get(),nullptr,slot.preview_depth.Get(),nullptr,D3DTEXF_POINT);
        // Fence even a failed draw/setup: any submitted private work must retire
        // before reuse. No handle is published until both restore and query pass.
        const auto fence_hr = slot.query->Issue(D3DISSUE_END);
        slot.pending = SUCCEEDED(fence_hr); slot.flushed = false;
        slot.poisoned = FAILED(fence_hr);
        const auto restore_hr = p->saved.Restore(p->device.Get());
        if (FAILED(restore_hr)) return impl_->Fail("native state restoration failed; RetryRestore required", restore_hr);
        if (FAILED(fence_hr)) return impl_->Fail("producer completion fence failed; slot quarantined", fence_hr);
        if (FAILED(draw_hr)) return impl_->Fail("device-Z snapshot draw failed", draw_hr);
        slot.lease = capture;
        output.impl_ = std::move(capture);
        p->last_sequence = sequence;
        impl_->error = "";
        return S_OK;
    } catch (const std::bad_alloc&) { return impl_->Fail("capture lease allocation failed", E_OUTOFMEMORY); }
    catch (...) { return impl_->Fail("unexpected capture failure", E_FAIL); }
}

HRESULT D3D9SampledDepthCapture::RetryRestore() noexcept {
    const auto& p = impl_->pool;
    if (!p || !p->Owner()) return impl_->Fail("restoration requires the original owner thread", D3DERR_INVALIDCALL);
    const auto hr = p->saved.Restore(p->device.Get());
    impl_->error = FAILED(hr) ? "native restoration remains unresolved" : "";
    return hr;
}
const char* D3D9SampledDepthCapture::last_error() const noexcept { return impl_->error; }
#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
void D3D9SampledDepthCapture::FailNextRestoreForTest(unsigned operation) noexcept {
    if (impl_->pool && impl_->pool->Owner() && !impl_->pool->saved.active)
        impl_->pool->saved.fail_operation = static_cast<int>(operation);
}
#endif

} // namespace cojvr::backends::d3d9
