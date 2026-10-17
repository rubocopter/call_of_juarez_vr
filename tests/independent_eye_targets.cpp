#include "backends/d3d9/independent_eye_targets.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>
#include <wrl/client.h>

#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

using Microsoft::WRL::ComPtr;
using namespace cojvr::backends::d3d9;

namespace {
int Fail(const char* text, const IndependentEyeTargets* bank = nullptr) {
    std::cerr << "RED: " << text;
    if (bank) std::cerr << ": " << bank->last_error();
    std::cerr << '\n'; return 1;
}
D3DSURFACE_DESC SurfaceDesc(UINT w, UINT h, bool depth, D3DFORMAT format = D3DFMT_UNKNOWN) {
    return {format == D3DFMT_UNKNOWN ? (depth ? D3DFMT_D24S8 : D3DFMT_A8R8G8B8) : format,
        D3DRTYPE_SURFACE, static_cast<DWORD>(depth ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET),
        D3DPOOL_DEFAULT, D3DMULTISAMPLE_NONE, 0, w, h};
}
IndependentEyeTargetConfig Config(UINT w, UINT h) {
    IndependentEyeTargetConfig c;
    c.width = w; c.height = h; c.max_payload_bytes = 32 * 1024 * 1024;
    for (std::size_t i = 0; i < c.textures.size(); ++i) {
        auto& t = c.textures[i];
        t.width = i == 13 ? 1 : w / (1 + static_cast<UINT>(i % 3));
        t.height = i == 13 ? 1 : h / (1 + static_cast<UINT>(i % 3));
        t.format = i >= 10 ? D3DFMT_A16B16G16R16F : D3DFMT_A8R8G8B8;
        t.history = i >= 12;
    }
    c.surfaces = {SurfaceDesc(w, h, false), SurfaceDesc(w, h, true),
        SurfaceDesc(w, h, false, D3DFMT_A16B16G16R16F), SurfaceDesc(w, h, true),
        SurfaceDesc(w + 8, h + 8, true, D3DFMT_D16)};
    return c;
}
bool Pixels(IDirect3DDevice9* device, IDirect3DSurface9* source,
            UINT x, UINT y, DWORD expected) {
    D3DSURFACE_DESC desc{};
    ComPtr<IDirect3DSurface9> cpu;
    if (FAILED(source->GetDesc(&desc)) || x >= desc.Width || y >= desc.Height ||
        FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
            desc.Format, D3DPOOL_SYSTEMMEM, &cpu, nullptr)) ||
        FAILED(device->GetRenderTargetData(source, cpu.Get()))) return false;
    D3DLOCKED_RECT locked{};
    if (FAILED(cpu->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
    const auto* row = reinterpret_cast<const DWORD*>(static_cast<const char*>(locked.pBits) + y * locked.Pitch);
    const bool same = (row[x] & 0x00ffffff) == (expected & 0x00ffffff);
    return SUCCEEDED(cpu->UnlockRect()) && same;
}
bool HistoryPixels(IDirect3DDevice9* device, IDirect3DTexture9* texture, bool expect_black = true) {
    ComPtr<IDirect3DSurface9> source, cpu;
    D3DSURFACE_DESC desc{};
    if (FAILED(texture->GetSurfaceLevel(0, &source)) || FAILED(source->GetDesc(&desc)) ||
        FAILED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
            D3DPOOL_SYSTEMMEM, &cpu, nullptr)) || FAILED(device->GetRenderTargetData(source.Get(), cpu.Get()))) return false;
    D3DLOCKED_RECT locked{};
    if (FAILED(cpu->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
    bool black = true;
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = static_cast<const unsigned char*>(locked.pBits) + y * locked.Pitch;
        for (UINT x = 0; x < desc.Width * 8; ++x) black &= row[x] == 0;
    }
    return SUCCEEDED(cpu->UnlockRect()) && black == expect_black;
}
bool Draw(IDirect3DDevice9* device, UINT w, UINT h, DWORD color) {
    struct Vertex { float x, y, z, rhw; DWORD color; };
    const float right = static_cast<float>(w) - .5F, bottom = static_cast<float>(h) - .5F;
    const std::array<Vertex, 4> vertices{{{-.5F, -.5F, .5F, 1, color},
        {right, -.5F, .5F, 1, color}, {-.5F, bottom, .5F, 1, color}, {right, bottom, .5F, 1, color}}};
    if (FAILED(device->SetVertexShader(nullptr)) || FAILED(device->SetPixelShader(nullptr)) ||
        FAILED(device->SetTexture(0, nullptr)) || FAILED(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE)) ||
        FAILED(device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE)) ||
        FAILED(device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE)) ||
        FAILED(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS)) ||
        FAILED(device->SetRenderState(D3DRS_STENCILENABLE, FALSE)) ||
        FAILED(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE)) ||
        FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)) ||
        FAILED(device->SetRenderState(D3DRS_COLORWRITEENABLE, 15)) ||
        FAILED(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)) ||
        FAILED(device->SetRenderState(D3DRS_LIGHTING, FALSE)) ||
        FAILED(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1)) ||
        FAILED(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE)) ||
        FAILED(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE)) ||
        FAILED(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1, 0)) ||
        FAILED(device->BeginScene())) return false;
    const HRESULT drawn = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(Vertex));
    const HRESULT ended = device->EndScene();
    return SUCCEEDED(drawn) && SUCCEEDED(ended);
}

// Representative checks across every state family touched by this test; the
// bank also independently verifies all explicit MRT/depth/viewport/scissor binds.
struct State {
    ComPtr<IDirect3DSurface9> target, depth, mrt;
    ComPtr<IDirect3DBaseTexture9> texture;
    ComPtr<IDirect3DVertexBuffer9> stream;
    ComPtr<IDirect3DIndexBuffer9> indices;
    D3DVIEWPORT9 viewport{}; RECT scissor{}; D3DMATRIX projection{};
    DWORD z = 0, blend = 0, sampler = 0, stage = 0, fvf = 0, scissor_enable = 0;
    UINT offset = 0, stride = 0;
    float vs[4]{}, ps[4]{};
    bool Read(IDirect3DDevice9* d) {
        const HRESULT mrt_hr = d->GetRenderTarget(1, mrt.ReleaseAndGetAddressOf());
        return (SUCCEEDED(mrt_hr) || mrt_hr == D3DERR_NOTFOUND) &&
            SUCCEEDED(d->GetRenderTarget(0, target.ReleaseAndGetAddressOf())) &&
            SUCCEEDED(d->GetDepthStencilSurface(depth.ReleaseAndGetAddressOf())) &&
            SUCCEEDED(d->GetTexture(0, texture.ReleaseAndGetAddressOf())) &&
            SUCCEEDED(d->GetStreamSource(0, stream.ReleaseAndGetAddressOf(), &offset, &stride)) &&
            SUCCEEDED(d->GetIndices(indices.ReleaseAndGetAddressOf())) &&
            SUCCEEDED(d->GetViewport(&viewport)) && SUCCEEDED(d->GetScissorRect(&scissor)) &&
            SUCCEEDED(d->GetTransform(D3DTS_PROJECTION, &projection)) &&
            SUCCEEDED(d->GetRenderState(D3DRS_ZFUNC, &z)) &&
            SUCCEEDED(d->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend)) &&
            SUCCEEDED(d->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor_enable)) &&
            SUCCEEDED(d->GetSamplerState(0, D3DSAMP_ADDRESSU, &sampler)) &&
            SUCCEEDED(d->GetTextureStageState(0, D3DTSS_COLOROP, &stage)) &&
            SUCCEEDED(d->GetFVF(&fvf)) && SUCCEEDED(d->GetVertexShaderConstantF(7, vs, 1)) &&
            SUCCEEDED(d->GetPixelShaderConstantF(9, ps, 1));
    }
    bool Matches(IDirect3DDevice9* d) const {
        State now;
        return now.Read(d) && now.target == target && now.depth == depth && now.mrt == mrt &&
            now.texture == texture && now.stream == stream && now.indices == indices &&
            now.offset == offset && now.stride == stride && now.z == z && now.blend == blend &&
            now.scissor_enable == scissor_enable && now.sampler == sampler && now.stage == stage && now.fvf == fvf &&
            std::memcmp(&now.viewport, &viewport, sizeof(viewport)) == 0 && EqualRect(&now.scissor, &scissor) &&
            std::memcmp(&now.projection, &projection, sizeof(projection)) == 0 &&
            std::memcmp(now.vs, vs, sizeof(vs)) == 0 && std::memcmp(now.ps, ps, sizeof(ps)) == 0;
    }
};
} // namespace

int main() {
    std::uint64_t estimated = 99;
    auto config = Config(96, 64);
    if (!IndependentEyeTargets::EstimatePayload(config, estimated)) return Fail("valid payload rejected");
    std::uint64_t expected = 0;
    for (const auto& t : config.textures)
        expected += std::uint64_t(t.width) * t.height * (t.format == D3DFMT_A16B16G16R16F ? 8 : 4) * (t.history ? 2 : 1);
    expected += std::uint64_t(96) * 64 * (4 + 4 + 8 + 4) + std::uint64_t(104) * 72 * 2;
    if (estimated != expected) return Fail("payload must count 16 texture payloads and 5 surfaces exactly");
    for (int bad = 0; bad < 9; ++bad) {
        auto invalid = config;
        switch (bad) {
        case 0: invalid.width = 0; break;
        case 1: invalid.textures[0].width = UINT_MAX; break;
        case 2: invalid.textures[0].format = D3DFMT_UNKNOWN; break;
        case 3: invalid.textures[0].usage |= D3DUSAGE_DYNAMIC; break;
        case 4: invalid.surfaces[0].MultiSampleType = D3DMULTISAMPLE_2_SAMPLES; break;
        case 5: invalid.surfaces[1].Format = D3DFMT_UNKNOWN; break;
        case 6: invalid.max_payload_bytes = expected - 1; break;
        case 7: invalid.textures[13].history = false; break;
        case 8: invalid.surfaces[0].Pool = D3DPOOL_MANAGED; break;
        }
        if (IndependentEyeTargets::EstimatePayload(invalid, estimated) || estimated)
            return Fail("invalid/overflowing layout must reject with zero payload");
    }
    const auto create = SystemDirect3DCreate9Ex();
    ComPtr<IDirect3D9Ex> api;
    if (!create || FAILED(create(D3D_SDK_VERSION, &api))) return Fail("system D3D9Ex unavailable");
    HWND window = CreateWindowExW(0, L"STATIC", L"Independent eye bank GPU test", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return Fail("window creation failed");
    struct WindowCleanup { HWND window; ~WindowCleanup() { DestroyWindow(window); } } cleanup{window};
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 160; pp.BackBufferHeight = 90; pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9Ex> device;
    HRESULT hr = api->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &pp, nullptr, &device);
    if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) {
        std::cout << "D3D9Ex HAL unavailable; skipped\n"; return 77;
    }
    if (FAILED(hr)) return Fail("CreateDeviceEx failed");
    ComPtr<IDirect3DSurface9> original, depth, mrt;
    ComPtr<IDirect3DTexture9> placeholder;
    ComPtr<IDirect3DVertexBuffer9> stream;
    ComPtr<IDirect3DIndexBuffer9> indices;
    if (FAILED(device->GetRenderTarget(0, &original)) ||
        FAILED(device->CreateDepthStencilSurface(160, 90, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depth, nullptr)) ||
        FAILED(device->CreateRenderTarget(160, 90, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &mrt, nullptr)) ||
        FAILED(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &placeholder, nullptr)) ||
        FAILED(device->CreateVertexBuffer(64, 0, D3DFVF_XYZ, D3DPOOL_DEFAULT, &stream, nullptr)) ||
        FAILED(device->CreateIndexBuffer(12, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &indices, nullptr)))
        return Fail("native state fixtures allocation failed");
    D3DMATRIX projection{};
    projection._11 = 2; projection._22 = 3; projection._33 = 4; projection._44 = 1; projection._31 = .2F;
    const D3DVIEWPORT9 viewport{7, 8, 130, 72, .1F, .9F};
    const RECT scissor{9, 10, 100, 70};
    const float constants[4]{1, 2, 3, 4};
    if (FAILED(device->SetDepthStencilSurface(depth.Get())) || FAILED(device->SetRenderTarget(1, mrt.Get())) ||
        FAILED(device->SetViewport(&viewport)) || FAILED(device->SetScissorRect(&scissor)) ||
        FAILED(device->SetTransform(D3DTS_PROJECTION, &projection)) ||
        FAILED(device->SetTexture(0, placeholder.Get())) || FAILED(device->SetStreamSource(0, stream.Get(), 0, 12)) ||
        FAILED(device->SetIndices(indices.Get())) || FAILED(device->SetFVF(D3DFVF_XYZ)) ||
        FAILED(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER)) ||
        FAILED(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)) ||
        FAILED(device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE)) ||
        FAILED(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)) ||
        FAILED(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE)) ||
        FAILED(device->SetVertexShaderConstantF(7, constants, 1)) ||
        FAILED(device->SetPixelShaderConstantF(9, constants, 1)) ||
        FAILED(device->ColorFill(original.Get(), nullptr, 0xff123456))) return Fail("native state setup failed");
    State before;
    if (!before.Read(device.Get())) return Fail("native state capture failed");
    IndependentEyeTargets bank;
    if (!bank.Create(device.Get(), 7, config) || !before.Matches(device.Get()))
        return Fail("bank preparation changed native state", &bank);
    ComPtr<IDirect3DDevice9Ex> other;
    if (FAILED(api->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, nullptr, &other))) return Fail("wrong-device fixture failed");
    if (bank.Begin(other.Get(), 7) || bank.Begin(device.Get(), 8) || !before.Matches(device.Get()))
        return Fail("foreign device/generation admitted", &bank);
    bool foreign_thread_rejected = false;
    std::thread foreign([&] { foreign_thread_rejected = !bank.Begin(device.Get(), 7) && !bank.InvalidateBeforeReset(); });
    foreign.join();
    if (!foreign_thread_rejected || !before.Matches(device.Get())) return Fail("foreign owner thread admitted", &bank);
    auto lease = bank.AcquireLease();
    if (!lease || bank.InvalidateBeforeReset() || bank.Create(device.Get(), 7, config))
        return Fail("live source lease did not prevent retirement/replacement", &bank);
    lease.Reset();
    int restore_steps = 0;
#if defined(COJVR_INDEPENDENT_EYE_FAULT_TESTING)
    auto* original_bank_color = bank.surface(IndependentEyeSurface::final_color);
    for (int ordinal = 0; ordinal < 22; ++ordinal) {
        bank.FailAllocationForTest(ordinal);
        if (bank.Create(device.Get(), 8, Config(96, 80)) || !bank.ready() || bank.generation() != 7 ||
            bank.surface(IndependentEyeSurface::final_color) != original_bank_color ||
            bank.payload_bytes() != expected || !before.Matches(device.Get()) ||
            !Pixels(device.Get(), original.Get(), 80, 45, 0xff123456))
            return Fail("allocation fault failed transactional rollback", &bank);
    }
    bank.FailAllocationForTest(-1);
    D3DCAPS9 caps{};
    if (FAILED(device->GetDeviceCaps(&caps))) return Fail("restore fault capabilities unavailable");
    // Every explicit restore call and verification readback, including ALL-state
    // Apply. A failed attempt must preserve the ORIGINAL capture across retries.
    restore_steps = 3 * static_cast<int>(caps.NumSimultaneousRTs) + 7;
    const auto all_restore_steps = (std::uint32_t{1} << restore_steps) - 1;
    for (int ordinal = 0; ordinal < restore_steps; ++ordinal) {
        if (!bank.Begin(device.Get(), 7) || !Draw(device.Get(), 96, 64, 0xffabcdef))
            return Fail("restore fault scope setup failed", &bank);
        bank.FailRestoreForTest(ordinal);
        if (bank.End() || !bank.active() || bank.ready() || bank.last_error().empty() ||
            bank.RestoreAttemptsForTest() != all_restore_steps)
            return Fail("failed restoration discarded recovery bookkeeping", &bank);
        if (bank.Begin(device.Get(), 7) || bank.Create(device.Get(), 8, config) ||
            bank.InvalidateBeforeReset() || bank.AcquireLease() ||
            bank.texture(0, 0) || bank.surface(IndependentEyeSurface::final_color) ||
            bank.MirrorRightTo(original.Get()) || bank.ClearHistories())
            return Fail("failed restoration allowed bank mutation or reuse", &bank);
        bool foreign_restore_rejected = false;
        std::thread retry([&] { foreign_restore_rejected = !bank.RestoreAll(); });
        retry.join();
        if (!foreign_restore_rejected || !bank.active() || bank.RestoreAll() || !bank.active() ||
            bank.RestoreAttemptsForTest() != all_restore_steps)
            return Fail("failed/foreign retry discarded original capture", &bank);
        bank.FailRestoreForTest(-1);
        if (!(ordinal % 2 ? bank.End() : bank.RestoreAll()) || bank.active() ||
            !bank.ready() || !bank.last_error().empty() || !before.Matches(device.Get()) ||
            bank.generation() != 7 || bank.payload_bytes() != expected ||
            bank.surface(IndependentEyeSurface::final_color) != original_bank_color ||
            !Pixels(device.Get(), original.Get(), 80, 45, 0xff123456))
            return Fail("explicit retry failed to restore original GPU state", &bank);
        if (!bank.Begin(device.Get(), 7) || !bank.End() || !before.Matches(device.Get()))
            return Fail("recovered bank cannot start a fresh scope", &bank);
    }
#else
    return Fail("test must compile backend and test with COJVR_INDEPENDENT_EYE_FAULT_TESTING");
#endif
    for (UINT height : {64U, 80U}) {
        config = Config(96, height);
        if (!bank.Create(device.Get(), 7, config)) return Fail("bank allocation failed", &bank);
        std::set<IDirect3DTexture9*> identities;
        for (std::size_t i = 0; i < 14; ++i) {
            auto* left = bank.texture(i, 0); auto* right = bank.texture(i, 1);
            if (!left || !right || (config.textures[i].history ? left == right : left != right))
                return Fail("scratch/history alias isolation failed", &bank);
            identities.insert(left); identities.insert(right);
            if (config.textures[i].history && (!HistoryPixels(device.Get(), left) || !HistoryPixels(device.Get(), right)))
                return Fail("per-eye HDR histories were not initialized black", &bank);
        }
        if (identities.size() != 16) return Fail("bank does not own sixteen unique textures");
        std::set<IDirect3DSurface9*> surface_identities;
        for (std::size_t i = 0; i < 5; ++i) surface_identities.insert(bank.surface(static_cast<IndependentEyeSurface>(i)));
        if (surface_identities.size() != 5 || surface_identities.contains(original.Get()))
            return Fail("private surfaces must not deduplicate unknown native aliases");
        ComPtr<IDirect3DSurface9> scratch;
        if (FAILED(bank.texture(0, 0)->GetSurfaceLevel(0, &scratch)) ||
            FAILED(device->ColorFill(scratch.Get(), nullptr, 0xff987654)))
            return Fail("history isolation scratch fixture failed");
        auto histories_nonblack = [&] {
            for (std::size_t i = 0; i < config.textures.size(); ++i)
                if (config.textures[i].history)
                    for (std::size_t eye = 0; eye < 2; ++eye)
                        if (!HistoryPixels(device.Get(), bank.texture(i, eye), false)) return false;
            return true;
        };
        for (std::size_t i = 0; i < config.textures.size(); ++i) {
            if (!config.textures[i].history) continue;
            for (std::size_t eye = 0; eye < 2; ++eye) {
                ComPtr<IDirect3DSurface9> history;
                if (FAILED(bank.texture(i, eye)->GetSurfaceLevel(0, &history)) ||
                    FAILED(device->ColorFill(history.Get(), nullptr, eye ? 0xff00ff00 : 0xffff0000)))
                    return Fail("history content fixture failed");
            }
        }
        if (!histories_nonblack()) return Fail("history fixture remained black");
        bool foreign_clear_rejected = false;
        std::thread clear([&] { foreign_clear_rejected = !bank.ClearHistories(); });
        clear.join();
        if (!foreign_clear_rejected || !histories_nonblack() || !before.Matches(device.Get()))
            return Fail("history clearing admitted a foreign owner thread", &bank);
        ComPtr<IDirect3DSurface9> retained_left;
        if (FAILED(device->CreateRenderTarget(96, height, D3DFMT_A8R8G8B8,
                D3DMULTISAMPLE_NONE, 0, FALSE, &retained_left, nullptr))) return Fail("left copy fixture failed");
        for (std::size_t eye = 0; eye < 2; ++eye) {
            if (!bank.Begin(device.Get(), 7) || bank.Begin(device.Get(), 7) || bank.InvalidateBeforeReset())
                return Fail("scope identity/nesting/retirement failed", &bank);
            if (bank.ClearHistories() || !histories_nonblack())
                return Fail("active scope admitted history clearing", &bank);
            D3DVIEWPORT9 effective{}; RECT effective_scissor{};
            if (FAILED(device->GetViewport(&effective)) || effective.Width != 96 || effective.Height != height ||
                effective.X || effective.Y || FAILED(device->GetScissorRect(&effective_scissor)) ||
                effective_scissor.right != 96 || effective_scissor.bottom != static_cast<LONG>(height))
                return Fail("eye viewport/scissor did not cover independently sized target");
            const float changed[4]{9, 8, 7, 6};
            D3DMATRIX changed_projection = projection; changed_projection._11 = 10;
            if (!Draw(device.Get(), 96, height, eye ? 0xff00ff00 : 0xffff0000) ||
                FAILED(device->SetTransform(D3DTS_PROJECTION, &changed_projection)) ||
                FAILED(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP)) ||
                FAILED(device->SetVertexShaderConstantF(7, changed, 1)) ||
                FAILED(device->SetPixelShaderConstantF(9, changed, 1))) return Fail("native-sized independent geometry draw failed");
            if (!eye && FAILED(device->StretchRect(bank.surface(IndependentEyeSurface::final_color), nullptr,
                    retained_left.Get(), nullptr, D3DTEXF_NONE))) return Fail("ordered same-size left copy failed");
            if (!bank.RestoreAll() || !before.Matches(device.Get())) return Fail("full state did not restore", &bank);
            if (!histories_nonblack()) return Fail("ordinary eye scope cleared histories");
        }
        if (!bank.ClearHistories() || !bank.ready() || bank.active() ||
            !before.Matches(device.Get()) || !Pixels(device.Get(), scratch.Get(), 0, 0, 0xff987654))
            return Fail("explicit history clearing changed state or scratch contents", &bank);
        for (std::size_t i = 0; i < config.textures.size(); ++i)
            if (config.textures[i].history && (!HistoryPixels(device.Get(), bank.texture(i, 0)) ||
                    !HistoryPixels(device.Get(), bank.texture(i, 1))))
                return Fail("explicit invalidation did not zero both histories for both eyes", &bank);
        if (!bank.ClearHistories()) return Fail("repeated explicit invalidation failed", &bank);
        if (!Pixels(device.Get(), retained_left.Get(), 94, height - 2, 0xffff0000) ||
            !Pixels(device.Get(), bank.surface(IndependentEyeSurface::final_color), 94, height - 2, 0xff00ff00) ||
            !Pixels(device.Get(), original.Get(), 80, 45, 0xff123456))
            return Fail("distinct eye geometry or original native surface contents changed", &bank);
        if (!bank.MirrorRightTo(original.Get()) || !before.Matches(device.Get()) ||
            !Pixels(device.Get(), original.Get(), 80, 45, 0xff00ff00) ||
            !Pixels(device.Get(), original.Get(), 0, 45, 0) ||
            !Pixels(device.Get(), original.Get(), 159, 45, 0))
            return Fail("proportional right-eye mirror/state restoration failed", &bank);
        if (FAILED(device->ColorFill(original.Get(), nullptr, 0xff123456))) return Fail("sentinel reset failed");
    }
    // Classic devices are a separate, rejected path.
    ComPtr<IDirect3D9> classic_api;
    const auto create_classic = SystemDirect3DCreate9();
    if (!create_classic) return Fail("classic factory missing");
    classic_api.Attach(create_classic(D3D_SDK_VERSION));
    ComPtr<IDirect3DDevice9> classic;
    if (!classic_api || FAILED(classic_api->CreateDevice(0, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &classic))) return Fail("classic rejection fixture failed");
    IndependentEyeTargets rejected;
    if (rejected.Create(classic.Get(), 1, config) || rejected.ready()) return Fail("classic device admitted");
    // Source token owns resource lifetime even if its facade is destroyed.
    auto facade = std::make_unique<IndependentEyeTargets>();
    if (!facade->Create(device.Get(), 9, config)) return Fail("lease lifetime fixture failed", facade.get());
    auto retained = facade->AcquireLease();
    auto* retained_source = facade->surface(IndependentEyeSurface::final_color);
    facade.reset();
    D3DSURFACE_DESC still_live{};
    if (!retained || FAILED(retained_source->GetDesc(&still_live)) || still_live.Height != 80)
        return Fail("source lease did not retain source lifetime");
    retained.Reset();
    if (!bank.InvalidateBeforeReset() || bank.ready() || bank.surface(IndependentEyeSurface::final_color) || bank.ClearHistories())
        return Fail("pre-Reset retirement failed", &bank);
    // Release the independent test's native fixtures too; the bank has no hidden
    // DEFAULT resources/stateblock references left to obstruct Reset/ResetEx.
    before = {}; original.Reset(); depth.Reset(); mrt.Reset(); placeholder.Reset(); stream.Reset(); indices.Reset();
    (void)device->SetTexture(0, nullptr); (void)device->SetStreamSource(0, nullptr, 0, 0);
    (void)device->SetIndices(nullptr); (void)device->SetRenderTarget(1, nullptr); (void)device->SetDepthStencilSurface(nullptr);
    if (FAILED(device->ResetEx(&pp, nullptr))) return Fail("ResetEx failed after bank retirement");
    if (bank.Begin(device.Get(), 7) || !bank.Create(device.Get(), 10, Config(96, 80)) ||
        !bank.Begin(device.Get(), 10) || !bank.RestoreAll() || !bank.InvalidateBeforeReset())
        return Fail("fresh generation did not recover after ResetEx", &bank);
    if (FAILED(device->Reset(&pp))) return Fail("Reset failed after bank retirement");
    std::cout << "GREEN: independent D3D9Ex geometry at 96x64 and 96x80, explicit history clearing, allocation rollback, "
        << restore_steps << " restore faults/retries, full state, mirror and Reset/ResetEx\n";
    return 0;
}
