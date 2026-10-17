#include "games/call_of_juarez/independent_render_scope.hpp"
#include "games/call_of_juarez/independent_resolution_recipe.hpp"
#include "backends/d3d9/system_d3d9.hpp"

#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace cojvr::games::call_of_juarez;
using cojvr::runtime::Eye;

namespace {
static_assert(sizeof(void*) == 4, "Native COM identities require a Win32 test executable");
constexpr UINT Width = 2560, DesktopHeight = 1440, EyeHeight = 1920;
constexpr std::uint32_t DirtyMask = 0x00622000;
constexpr DWORD DesktopColor = 0xff123456;
constexpr int Divisors[]{1, 1, 4, 0, 0, 1, 1, 2, 4, 4, 8, 16, 16, 16};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void Hr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::cerr << operation << " HRESULT=0x" << std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
        throw std::runtime_error(operation);
    }
}
template<class T> std::uint32_t Id(T* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}
template<class T> T* Pointer(std::uint32_t id) {
    return reinterpret_cast<T*>(static_cast<std::uintptr_t>(id));
}
void Begun(CoJIndependentRenderScope& scope, IDirect3DDevice9* device,
           const CoJPostprocessResourceSnapshot& snapshot, Eye eye) {
    const bool result = scope.Begin(device, 7, snapshot, EyeHeight, eye);
    Require(result, "Begin: " + std::string(scope.last_error()));
}
void Ended(CoJIndependentRenderScope& scope) {
    const bool result = scope.End();
    Require(result, "End: " + std::string(scope.last_error()));
    Require(!scope.active() && !scope.restoration_pending(), "End retained an ordinary eye transaction");
}

// The synthetic addresses intentionally include the high bit. Only COM payloads
// refer to real process addresses; native roots/objects must use the injected IO.
struct Memory {
    struct Region { std::uint32_t base; std::vector<unsigned char> bytes; };
    std::vector<Region> regions;
    std::uint32_t fault_address = 0;
    bool persistent_fault = false, fault_fired = false;
    unsigned writes = 0;

    void Map(std::uint32_t base, std::size_t size) {
        Region region{base, std::vector<unsigned char>(size)};
        for (std::size_t i = 0; i < size; ++i)
            region.bytes[i] = static_cast<unsigned char>((base >> 8) + i * 17 + 31);
        for (const auto& other : regions)
            Require(std::uint64_t(base) + size <= other.base ||
                    std::uint64_t(other.base) + other.bytes.size() <= base, "overlapping sparse fixture regions");
        regions.push_back(std::move(region));
    }
    const unsigned char* Find(std::uint32_t address, std::size_t size) const noexcept {
        for (const auto& region : regions)
            if (address >= region.base && std::uint64_t(address) - region.base + size <= region.bytes.size())
                return region.bytes.data() + (address - region.base);
        return nullptr;
    }
    unsigned char* Find(std::uint32_t address, std::size_t size) noexcept {
        return const_cast<unsigned char*>(static_cast<const Memory&>(*this).Find(address, size));
    }
    static bool Read(void* context, std::uint32_t address, void* out, std::size_t size) noexcept {
        auto& memory = *static_cast<Memory*>(context);
        const auto* source = memory.Find(address, size);
        if (!source) return false;
        std::memcpy(out, source, size); return true;
    }
    static bool Write(void* context, std::uint32_t address, const void* in, std::size_t size) noexcept {
        auto& memory = *static_cast<Memory*>(context);
        ++memory.writes;
        auto* destination = memory.Find(address, size);
        if (!destination) return false;
        if (address == memory.fault_address) {
            // First attempt changes a real prefix, subsequent failed rollback
            // attempts leave it untouched until the test permits recovery.
            if (!memory.fault_fired) std::memcpy(destination, in, size / 2);
            memory.fault_fired = true;
            if (!memory.persistent_fault) memory.fault_address = 0;
            return false;
        }
        std::memcpy(destination, in, size); return true;
    }
    CoJRenderMemory Io() { return {Read, Write, this}; }
    template<class T> T At(std::uint32_t address) const {
        T value{};
        const auto* source = Find(address, sizeof(value));
        Require(source != nullptr, "unmapped fixture read");
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
    void PutBytes(std::uint32_t address, const void* data, std::size_t size) {
        auto* destination = Find(address, size);
        Require(destination != nullptr, "unmapped fixture initialization");
        std::memcpy(destination, data, size);
    }
    template<class T> void Put(std::uint32_t address, const T& value) { PutBytes(address, &value, sizeof(value)); }
    void Mutate(std::uint32_t address, std::size_t size) {
        auto* destination = Find(address, size);
        Require(destination != nullptr, "unmapped CPU-source mutation");
        for (std::size_t i = 0; i < size; ++i) destination[i] ^= 0x5a;
    }
    bool SameBytes(const Memory& expected) const {
        if (regions.size() != expected.regions.size()) return false;
        for (std::size_t i = 0; i < regions.size(); ++i) {
            const auto& a = regions[i]; const auto& b = expected.regions[i];
            if (a.base != b.base || a.bytes.size() != b.bytes.size()) return false;
            for (std::size_t j = 0; j < a.bytes.size(); ++j) if (a.bytes[j] != b.bytes[j]) {
                std::cerr << "native byte mismatch at 0x" << std::hex << a.base + j << std::dec << '\n';
                return false;
            }
        }
        return true;
    }
};

// Same observed recipe as independent_resolution_recipe.cpp, populated below
// with actual original COM resources and native object preimages.
CoJPostprocessResourceSnapshot Sample() {
    CoJPostprocessResourceSnapshot s;
    s.valid = true; s.status = CoJPostprocessObservationStatus::observed;
    s.engine_base = 0x10000000; s.renderer = 0x90000000; s.manager = 0xa0000000;
    s.renderer_vtable = s.engine_base + 0x312334; s.manager_vtable = s.engine_base + 0x314c48;
    s.effect_count = s.loaded_effect_count = 5; s.buffer_count = 14;
    auto& r = s.renderer_state;
    r.logical_width = r.viewport_clamp_width = Width;
    r.logical_height = r.viewport_clamp_height = DesktopHeight;
    const char* effects[]{"null.ppfx", "hsm.ppfx", "scene30.ppfx", "scene30simple.ppfx", "clear_z.ppfx"};
    for (unsigned i = 0; i < 5; ++i) {
        auto& e = s.effects[i]; std::strcpy(e.name.data(), effects[i]);
        e.effect = 0xc0000000 + i * 0x1000; e.vtable = s.engine_base + 0x3180ac;
        e.buffer_count = i == 2 ? 14 : 0;
    }
    const char* names[]{"1A_16", "1B_16", "4A_16", "1xScreenY_16", "1x1_16",
        "1A", "1B", "2A", "4A", "4B", "8A", "16A", "16B", "16Prev"};
    for (unsigned i = 0; i < 14; ++i) {
        auto& b = s.buffers[i]; std::strcpy(b.name.data(), names[i]);
        b.container = s.effects[2].effect; b.index = i; b.entry = b.container + 4 + i * 0x64;
        b.width_divisor = Divisors[i]; b.height_divisor = i == 3 ? 4 : Divisors[i];
        b.logical_width = b.width_divisor ? Width / b.width_divisor : 1;
        b.logical_height = b.height_divisor ? DesktopHeight / b.height_divisor : 1;
        b.temp_shareable = i != 4 && i != 13; b.internal_color = b.depth_attachment = i < 2;
        b.native_bpp = i >= 5;
        auto& t = b.texture; t.handle = 0xd0000000 + i * 0x1000;
        t.width = t.requested_width = b.logical_width; t.height = t.requested_height = b.logical_height;
        t.depth = t.requested_depth = 1;
        t.inverse_width = 1.0f / t.width; t.inverse_height = 1.0f / t.height; t.inverse_depth = 1;
        t.format = i < 5 ? 113 : 21; t.usage = 1; t.flags = i < 2 ? 0x1d : 5;
    }
    return s;
}

struct Fixture {
    CoJPostprocessResourceSnapshot snapshot = Sample();
    Memory memory;
    std::array<ComPtr<IDirect3DTexture9>, 14> textures;
    std::array<ComPtr<IDirect3DSurface9>, 5> surfaces;
    D3DFORMAT depth_format = D3DFMT_UNKNOWN;

    void Create(IDirect3D9Ex* api, IDirect3DDevice9Ex* device) {
        snapshot.device = Id(device);
        D3DDISPLAYMODE display{};
        Hr(api->GetAdapterDisplayMode(0, &display), "adapter format");
        for (const auto format : {D3DFMT_D16, D3DFMT_D24X8, D3DFMT_D24S8}) {
            if (SUCCEEDED(api->CheckDeviceFormat(0, D3DDEVTYPE_HAL, display.Format,
                    D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, format)) &&
                SUCCEEDED(api->CheckDepthStencilMatch(0, D3DDEVTYPE_HAL, display.Format, D3DFMT_A8R8G8B8, format)) &&
                SUCCEEDED(api->CheckDepthStencilMatch(0, D3DDEVTYPE_HAL, display.Format, D3DFMT_A16B16G16R16F, format))) {
                depth_format = format; break;
            }
        }
        Require(depth_format != D3DFMT_UNKNOWN, "no common real A8/HDR depth format");
        Hr(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surfaces[0]), "original desktop backbuffer");
        for (unsigned i = 1; i < 5; ++i) {
            const UINT w = i == 4 ? 2048 : Width, h = i == 4 ? 2048 : DesktopHeight;
            if (i == 2) Hr(device->CreateRenderTarget(w, h, D3DFMT_A16B16G16R16F,
                D3DMULTISAMPLE_NONE, 0, FALSE, &surfaces[i], nullptr), "original internal HDR surface");
            else Hr(device->CreateDepthStencilSurface(w, h, depth_format, D3DMULTISAMPLE_NONE,
                0, TRUE, &surfaces[i], nullptr), "original depth group");
        }
        auto& r = snapshot.renderer_state;
        r.default_color_surface = Id(surfaces[0].Get()); r.default_depth_surface = Id(surfaces[1].Get());
        r.internal_color_surface = Id(surfaces[2].Get()); r.internal_depth_surface = Id(surfaces[3].Get());
        r.additional_depth_surface = Id(surfaces[4].Get());
        memory.Map(snapshot.engine_base + 0x590074, 8);
        memory.Map(snapshot.engine_base + 0x5902ac, 4);
        memory.Map(snapshot.renderer, 0x2000); memory.Map(snapshot.manager, 0x800);
        memory.Put(snapshot.engine_base + 0x590074, snapshot.renderer);
        memory.Put(snapshot.engine_base + 0x590078, snapshot.device);
        memory.Put(snapshot.engine_base + 0x5902ac, snapshot.manager);
        memory.Put(snapshot.manager, snapshot.manager_vtable);
        memory.Put(snapshot.manager + 0x644, std::uint32_t{0});
        memory.Put(snapshot.manager + 0x650, std::uint32_t{5});
        memory.Put(snapshot.manager + 0x6b5, std::uint8_t{0});
        for (const auto& e : snapshot.effects) {
            if (!e.effect) continue;
            memory.Map(e.effect, 0x800); memory.Put(e.effect, e.vtable);
            memory.Put(e.effect + 0x644, e.buffer_count);
            memory.PutBytes(e.effect + 0x684, e.name.data(), e.name.size());
        }
        for (unsigned i = 0; i < 14; ++i) {
            auto& b = snapshot.buffers[i]; auto& t = b.texture;
            Hr(device->CreateTexture(t.width, t.height, 1, D3DUSAGE_RENDERTARGET,
                static_cast<D3DFORMAT>(t.format), D3DPOOL_DEFAULT, &textures[i], nullptr), "original PP texture");
            t.texture_2d = t.selected_interface = Id(textures[i].Get());
            memory.Map(t.handle, 0x180);
            memory.Put(t.handle, t.selected_interface); memory.Put(t.handle + 4, t.texture_2d);
            memory.Put(t.handle + 8, std::uint32_t{0}); memory.Put(t.handle + 12, std::uint32_t{0});
            memory.Put(t.handle + 0x10, t.usage); memory.Put(t.handle + 0x14, t.format);
            memory.Put(t.handle + 0x130, t.kind); memory.Put(t.handle + 0x134, t.flags);
            const std::array<std::uint32_t, 6> dimensions{t.requested_width, t.requested_height,
                t.requested_depth, t.width, t.height, t.depth};
            const std::array<float, 3> inverse{t.inverse_width, t.inverse_height, t.inverse_depth};
            memory.Put(t.handle + 0x13c, dimensions); memory.Put(t.handle + 0x154, inverse);
            memory.Put(t.handle + 0x168, std::uint32_t{11 + i});
            memory.Put(b.entry, t.handle); memory.PutBytes(b.entry + 4, b.name.data(), b.name.size());
            memory.Put(b.entry + 0x44, b.logical_width); memory.Put(b.entry + 0x48, b.logical_height);
            memory.Put(b.entry + 0x54, b.internal_color); memory.Put(b.entry + 0x55, b.depth_attachment);
            memory.Put(b.entry + 0x58, b.width_divisor); memory.Put(b.entry + 0x5c, b.height_divisor);
            memory.Put(b.entry + 0x60, b.native_bpp); memory.Put(b.entry + 0x61, b.temp_shareable);
            memory.Put(b.entry + 0x62, b.linked_borrowed);
        }
        const auto base = snapshot.renderer;
        memory.Put(base, snapshot.renderer_vtable); memory.Put(base + 0x1bc, std::uint32_t{0x80000005});
        memory.Put(base + 0x1c8, Width); memory.Put(base + 0x1cc, DesktopHeight);
        memory.Put(base + 0x15c4, Width); memory.Put(base + 0x15c8, DesktopHeight);
        memory.Put(base + 0x1c10, std::uint32_t{D3DMULTISAMPLE_NONE});
        memory.Put(base + 0x1c14, std::uint32_t{D3DFMT_A16B16G16R16F});
        memory.Put(base + 0x1c18, r.internal_color_surface); memory.Put(base + 0x1c1c, r.internal_depth_surface);
        memory.Put(base + 0x1c20, std::uint32_t{2});
        memory.Put(base + 0x1c24, Width); memory.Put(base + 0x1c28, DesktopHeight);
        memory.Put(base + 0x1c2c, r.additional_depth_surface); memory.Put(base + 0x1c30, std::uint32_t{7});
        memory.Put(base + 0x1c34, UINT{2048}); memory.Put(base + 0x1c38, UINT{2048});
        r.placeholder_texture = Id(textures[5].Get());
        memory.Put(base + 0x1c3c, r.placeholder_texture); memory.Put(base + 0x1c40, std::uint32_t{3});
        memory.Put(base + 0x1c44, Width); memory.Put(base + 0x1c48, DesktopHeight);
        const D3DVIEWPORT9 viewport{0, 0, Width, DesktopHeight, 0, 1}; memory.Put(base + 0x1c58, viewport);
        memory.Put(base + 0x1db4, std::uint32_t{0}); memory.Put(base + 0x1db8, std::uint32_t{0});
        memory.Put(base + 0x1dc0, std::uint32_t{0});
        memory.Put(base + 0x1dc4, r.default_color_surface); memory.Put(base + 0x1dc8, r.default_depth_surface);
        for (unsigned i = 0; i < 16; ++i) {
            r.pixel_samplers[i] = snapshot.buffers[i % 14].texture.handle;
            memory.Put(base + 0x1cd0 + i * 4, r.pixel_samplers[i]);
        }
        for (unsigned i = 0; i < 4; ++i) {
            r.vertex_samplers[i] = snapshot.buffers[i].texture.handle;
            memory.Put(base + 0x1d50 + i * 4, r.vertex_samplers[i]);
        }
    }
};

struct Pixel { UINT x, y; DWORD color; };
void Pixels(IDirect3DDevice9* device, IDirect3DSurface9* source, std::initializer_list<Pixel> probes) {
    D3DSURFACE_DESC desc{}; Hr(source->GetDesc(&desc), "readback source description");
    Require(desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_A16B16G16R16F, "unexpected readback format");
    ComPtr<IDirect3DSurface9> cpu;
    Hr(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
        D3DPOOL_SYSTEMMEM, &cpu, nullptr), "readback allocation");
    Hr(device->GetRenderTargetData(source, cpu.Get()), "genuine GPU readback");
    D3DLOCKED_RECT locked{}; Hr(cpu->LockRect(&locked, nullptr, D3DLOCK_READONLY), "readback lock");
    bool equal = true;
    for (const auto& p : probes) {
        if (p.x >= desc.Width || p.y >= desc.Height) { equal = false; continue; }
        const auto* row = static_cast<const unsigned char*>(locked.pBits) + p.y * locked.Pitch;
        if (desc.Format == D3DFMT_A8R8G8B8) {
            DWORD color = 0; std::memcpy(&color, row + p.x * 4, 4);
            equal &= (color & 0xffffff) == (p.color & 0xffffff);
        } else {
            // HDR history probes use only exact zero/one channels, represented
            // by binary16 0x0000/0x3c00; alpha is deliberately irrelevant.
            std::array<std::uint16_t, 4> rgba{}; std::memcpy(rgba.data(), row + p.x * 8, 8);
            equal &= rgba[0] == ((p.color & 0xff0000) ? 0x3c00 : 0) &&
                     rgba[1] == ((p.color & 0x00ff00) ? 0x3c00 : 0) &&
                     rgba[2] == ((p.color & 0x0000ff) ? 0x3c00 : 0);
        }
    }
    Hr(cpu->UnlockRect(), "readback unlock"); Require(equal, "GPU pixel mismatch");
}
void TexturePixels(IDirect3DDevice9* device, std::uint32_t id, DWORD color) {
    ComPtr<IDirect3DSurface9> surface;
    Hr(Pointer<IDirect3DTexture9>(id)->GetSurfaceLevel(0, &surface), "history level");
    D3DSURFACE_DESC desc{}; Hr(surface->GetDesc(&desc), "history description");
    Pixels(device, surface.Get(), {{0, 0, color}, {desc.Width - 1, desc.Height - 1, color}});
}
void FillTexture(IDirect3DDevice9* device, std::uint32_t id, DWORD color) {
    ComPtr<IDirect3DSurface9> surface;
    Hr(Pointer<IDirect3DTexture9>(id)->GetSurfaceLevel(0, &surface), "history fill level");
    Hr(device->ColorFill(surface.Get(), nullptr, color), "history fill");
}

void DrawPattern(IDirect3DDevice9* d, DWORD top, DWORD bottom) {
    struct Vertex { float x, y, z, rhw; DWORD color; };
    const auto strip = [&](float y0, float y1, DWORD color) {
        const std::array<Vertex, 4> vertices{{{-.5f, y0 - .5f, .5f, 1, color},
            {Width - .5f, y0 - .5f, .5f, 1, color}, {-.5f, y1 - .5f, .5f, 1, color},
            {Width - .5f, y1 - .5f, .5f, 1, color}}};
        return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(Vertex));
    };
    Hr(d->SetVertexShader(nullptr), "draw VS"); Hr(d->SetPixelShader(nullptr), "draw PS");
    Hr(d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "draw FVF");
    Hr(d->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE), "draw depth");
    Hr(d->SetRenderState(D3DRS_ZWRITEENABLE, TRUE), "draw depth write");
    Hr(d->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS), "draw depth function");
    for (const auto state : {D3DRS_STENCILENABLE, D3DRS_ALPHATESTENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_LIGHTING})
        Hr(d->SetRenderState(state, FALSE), "draw disable state");
    Hr(d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "draw cull");
    Hr(d->SetRenderState(D3DRS_COLORWRITEENABLE, 15), "draw color mask");
    Hr(d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "draw color operation");
    Hr(d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE), "draw diffuse");
    Hr(d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "draw stage one");
    // No stencil clear: D16 and D24X8 are intentional supported depth fixtures.
    Hr(d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff010203, 1, 0), "pattern clear");
    Hr(d->BeginScene(), "pattern BeginScene");
    const HRESULT first = strip(0, static_cast<float>(DesktopHeight), top);
    const HRESULT second = strip(static_cast<float>(DesktopHeight), static_cast<float>(EyeHeight), bottom);
    const HRESULT ended = d->EndScene();
    Hr(first, "top geometry"); Hr(second, "geometry in new rows 1440..1919"); Hr(ended, "pattern EndScene");
}

// Independent observations across the state families changed by the wrapper.
struct GpuState {
    std::array<ComPtr<IDirect3DSurface9>, 4> targets;
    ComPtr<IDirect3DSurface9> depth;
    std::array<ComPtr<IDirect3DBaseTexture9>, 16> textures;
    ComPtr<IDirect3DVertexBuffer9> stream;
    ComPtr<IDirect3DIndexBuffer9> indices;
    D3DVIEWPORT9 viewport{}; RECT scissor{}; D3DMATRIX projection{};
    DWORD fvf = 0, zfunc = 0, blend = 0, scissor_enabled = 0, sampler = 0, stage = 0;
    UINT offset = 0, stride = 0;
    float vs[4]{}, ps[4]{};
    void Read(IDirect3DDevice9* d) {
        D3DCAPS9 caps{}; Hr(d->GetDeviceCaps(&caps), "state capabilities");
        for (DWORD i = 0; i < caps.NumSimultaneousRTs; ++i) {
            const auto hr = d->GetRenderTarget(i, targets[i].ReleaseAndGetAddressOf());
            Require(SUCCEEDED(hr) || (i && hr == D3DERR_NOTFOUND), "state MRT capture");
        }
        Hr(d->GetDepthStencilSurface(depth.ReleaseAndGetAddressOf()), "state depth");
        for (DWORD i = 0; i < textures.size(); ++i) Hr(d->GetTexture(i, textures[i].ReleaseAndGetAddressOf()), "state texture");
        Hr(d->GetStreamSource(0, stream.ReleaseAndGetAddressOf(), &offset, &stride), "state stream");
        Hr(d->GetIndices(indices.ReleaseAndGetAddressOf()), "state indices");
        Hr(d->GetViewport(&viewport), "state viewport"); Hr(d->GetScissorRect(&scissor), "state scissor");
        Hr(d->GetTransform(D3DTS_PROJECTION, &projection), "state projection");
        Hr(d->GetFVF(&fvf), "state FVF"); Hr(d->GetRenderState(D3DRS_ZFUNC, &zfunc), "state Z function");
        Hr(d->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend), "state blend");
        Hr(d->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor_enabled), "state scissor enable");
        Hr(d->GetSamplerState(0, D3DSAMP_ADDRESSU, &sampler), "state sampler");
        Hr(d->GetTextureStageState(0, D3DTSS_COLOROP, &stage), "state texture stage");
        Hr(d->GetVertexShaderConstantF(7, vs, 1), "state VS constants");
        Hr(d->GetPixelShaderConstantF(9, ps, 1), "state PS constants");
    }
    void Check(IDirect3DDevice9* d) const {
        GpuState now; now.Read(d);
        bool equal = now.depth == depth && now.stream == stream && now.indices == indices &&
            now.offset == offset && now.stride == stride && now.fvf == fvf && now.zfunc == zfunc &&
            now.blend == blend && now.scissor_enabled == scissor_enabled && now.sampler == sampler && now.stage == stage &&
            !std::memcmp(&now.viewport, &viewport, sizeof(viewport)) && EqualRect(&now.scissor, &scissor) &&
            !std::memcmp(&now.projection, &projection, sizeof(projection)) &&
            !std::memcmp(now.vs, vs, sizeof(vs)) && !std::memcmp(now.ps, ps, sizeof(ps));
        for (unsigned i = 0; i < targets.size(); ++i) equal &= now.targets[i] == targets[i];
        for (unsigned i = 0; i < textures.size(); ++i) equal &= now.textures[i] == textures[i];
        Require(equal, "original GPU state was not restored");
    }
};

void OriginalObjects(Fixture& f, const Memory& preimage) {
    for (unsigned i = 0; i < 14; ++i) {
        const auto& b = f.snapshot.buffers[i]; const auto& t = b.texture;
        Require(f.memory.At<std::uint32_t>(b.entry) == t.handle &&
            f.memory.At<std::uint32_t>(t.handle + 0x168) == 11 + i, "native handle registration/count changed");
        D3DSURFACE_DESC desc{}; Hr(f.textures[i]->GetLevelDesc(0, &desc), "original PP descriptor");
        Require(desc.Width == t.width && desc.Height == t.height && desc.Format == static_cast<D3DFORMAT>(t.format) &&
                desc.Pool == D3DPOOL_DEFAULT && desc.Usage == D3DUSAGE_RENDERTARGET &&
                desc.MultiSampleType == D3DMULTISAMPLE_NONE, "original COM texture resized/replaced");
        // Identity, kind, native allocation flags/depth and reference counts must
        // survive even while the selected COM payload is intentionally private.
        for (UINT offset : {8U, 0xcU, 0x10U, 0x14U, 0x130U, 0x134U, 0x138U, 0x144U, 0x150U, 0x15cU, 0x168U}) {
            const auto* expected = preimage.Find(t.handle + offset, 4);
            Require(expected && !std::memcmp(f.memory.Find(t.handle + offset, 4), expected, 4), "native allocation identity mutated");
        }
    }
    const auto base = f.snapshot.renderer;
    for (const UINT offset : {0x1c10U, 0x1c14U, 0x1c20U, 0x1c30U, 0x1c3cU, 0x1c40U, 0x1c44U, 0x1c48U})
        Require(f.memory.At<std::uint32_t>(base + offset) == preimage.At<std::uint32_t>(base + offset),
                "native MSAA/format/count/placeholder ownership changed");
    for (unsigned i = 0; i < 5; ++i) {
        D3DSURFACE_DESC desc{}; Hr(f.surfaces[i]->GetDesc(&desc), "original surface descriptor");
        Require(desc.Width == (i == 4 ? 2048 : Width) && desc.Height == (i == 4 ? 2048 : DesktopHeight),
                "original surface dimensions changed");
    }
}

std::array<std::uint32_t, 14> ActivePayload(Fixture& f, IDirect3DDevice9* device, const Memory& preimage) {
    auto& m = f.memory; const auto base = f.snapshot.renderer;
    OriginalObjects(f, preimage);
    std::array<std::uint32_t, 14> identities{};
    std::set<std::uint32_t> unique;
    for (unsigned i = 0; i < 14; ++i) {
        const auto& b = f.snapshot.buffers[i]; const auto handle = b.texture.handle;
        const UINT w = Divisors[i] ? Width / Divisors[i] : 1;
        const UINT divisor_y = i == 3 ? 4 : Divisors[i], h = divisor_y ? EyeHeight / divisor_y : 1;
        const auto id = m.At<std::uint32_t>(handle); identities[i] = id; unique.insert(id);
        Require(id && id != Id(f.textures[i].Get()) && m.At<std::uint32_t>(handle + 4) == id, "private payload aliases missing");
        for (const UINT offset : {0x13cU, 0x148U}) Require(m.At<UINT>(handle + offset) == w, "private physical/requested width");
        for (const UINT offset : {0x140U, 0x14cU}) Require(m.At<UINT>(handle + offset) == h, "private physical/requested height");
        Require(m.At<float>(handle + 0x154) == 1.0f / w && m.At<float>(handle + 0x158) == 1.0f / h &&
                m.At<UINT>(b.entry + 0x44) == w && m.At<UINT>(b.entry + 0x48) == h, "logical dimensions/texel inverses disagree");
        D3DSURFACE_DESC desc{}; Hr(Pointer<IDirect3DTexture9>(id)->GetLevelDesc(0, &desc), "private PP descriptor");
        Require(desc.Width == w && desc.Height == h && desc.Format == static_cast<D3DFORMAT>(b.texture.format), "private COM extent mismatch");
    }
    Require(unique.size() == 14, "independent PP descriptors unexpectedly alias");
    for (UINT offset : {0x1c8U, 0x15c4U, 0x1c24U})
        Require(m.At<UINT>(base + offset) == Width && m.At<UINT>(base + offset + 4) == EyeHeight, "native render extent not independent");
    Require(m.At<UINT>(base + 0x1c34) == Width && m.At<UINT>(base + 0x1c38) == 2048, "additional depth maximum extent lost");
    constexpr UINT surface_offsets[]{0x1dc4, 0x1dc8, 0x1c18, 0x1c1c, 0x1c2c};
    std::set<std::uint32_t> surface_ids;
    for (unsigned i = 0; i < 5; ++i) {
        const auto id = m.At<std::uint32_t>(base + surface_offsets[i]); surface_ids.insert(id);
        for (const auto& original : f.surfaces) Require(id != Id(original.Get()), "private surface borrowed an original");
        D3DSURFACE_DESC desc{}; Hr(Pointer<IDirect3DSurface9>(id)->GetDesc(&desc), "private surface descriptor");
        Require(desc.Width == Width && desc.Height == (i == 4 ? 2048 : EyeHeight) &&
            desc.Format == (i == 0 ? D3DFMT_A8R8G8B8 : i == 2 ? D3DFMT_A16B16G16R16F : f.depth_format), "private native surface group mismatch");
    }
    Require(surface_ids.size() == 5, "native surface groups incorrectly deduplicated");
    D3DVIEWPORT9 viewport{}; Hr(device->GetViewport(&viewport), "active viewport");
    Require(viewport.X == 0 && viewport.Y == 0 && viewport.Width == Width && viewport.Height == EyeHeight &&
        viewport.MinZ == 0 && viewport.MaxZ == 1 && !std::memcmp(&viewport, m.Find(base + 0x1c58, sizeof(viewport)), sizeof(viewport)),
        "native/GPU viewport does not cover 2560x1920");
    RECT scissor{}; Hr(device->GetScissorRect(&scissor), "active scissor");
    Require(scissor.left == 0 && scissor.top == 0 && scissor.right == Width && scissor.bottom == EyeHeight, "active scissor crops new rows");
    ComPtr<IDirect3DSurface9> rt, depth;
    Hr(device->GetRenderTarget(0, &rt), "active RT0"); Hr(device->GetDepthStencilSurface(&depth), "active depth");
    Require(Id(rt.Get()) == m.At<std::uint32_t>(base + 0x1dc4) && Id(depth.Get()) == m.At<std::uint32_t>(base + 0x1dc8),
            "GPU and native default-target payload disagree");
    for (DWORD i = 0; i < 16; ++i) {
        ComPtr<IDirect3DBaseTexture9> texture; Hr(device->GetTexture(i, &texture), "active sampler");
        Require(!texture && m.At<std::uint32_t>(base + 0x1cd0 + i * 4) == 0, "pixel sampler was not invalidated/unbound");
    }
    for (UINT i = 0; i < 4; ++i) Require(m.At<std::uint32_t>(base + 0x1d50 + i * 4) == 0, "vertex sampler cache not invalidated");
    Require(!std::memcmp(m.Find(base + 0x1d10, 0x40), preimage.Find(base + 0x1d10, 0x40), 0x40),
            "intervening sampler-state words were treated as texture pointers");
    return identities;
}

void Run(IDirect3D9Ex* api, IDirect3DDevice9Ex* device, D3DPRESENT_PARAMETERS& pp) {
    Fixture f; f.Create(api, device);
    const Memory original = f.memory;
    Memory restored = original;
    const auto base = f.snapshot.renderer;
    restored.Put(base + 0x1bc, restored.At<std::uint32_t>(base + 0x1bc) | DirtyMask);
    Hr(device->SetRenderTarget(0, f.surfaces[0].Get()), "native RT0");
    Hr(device->SetDepthStencilSurface(f.surfaces[1].Get()), "native depth");
    D3DCAPS9 caps{}; Hr(device->GetDeviceCaps(&caps), "GPU caps");
    ComPtr<IDirect3DSurface9> mrt;
    if (caps.NumSimultaneousRTs > 1) {
        Hr(device->CreateRenderTarget(Width, DesktopHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
            0, FALSE, &mrt, nullptr), "native MRT fixture");
        Hr(device->SetRenderTarget(1, mrt.Get()), "native MRT bind");
    }
    ComPtr<IDirect3DVertexBuffer9> stream; ComPtr<IDirect3DIndexBuffer9> indices;
    Hr(device->CreateVertexBuffer(96, D3DUSAGE_WRITEONLY, D3DFVF_XYZ, D3DPOOL_DEFAULT, &stream, nullptr), "native stream");
    Hr(device->CreateIndexBuffer(24, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &indices, nullptr), "native indices");
    Hr(device->SetStreamSource(0, stream.Get(), 12, 12), "native stream bind"); Hr(device->SetIndices(indices.Get()), "native indices bind");
    Hr(device->SetFVF(D3DFVF_XYZ), "native FVF");
    Hr(device->SetTexture(0, f.textures[5].Get()), "native sampler zero");
    Hr(device->SetTexture(7, f.textures[13].Get()), "native sampler seven");
    const D3DVIEWPORT9 viewport{13, 17, 2300, 1300, .15f, .85f}; const RECT scissor{23, 29, 2100, 1200};
    Hr(device->SetViewport(&viewport), "native viewport"); Hr(device->SetScissorRect(&scissor), "native scissor");
    D3DMATRIX projection{}; projection._11 = 1.25f; projection._22 = 2.5f; projection._33 = .75f; projection._44 = 1;
    const float constants[]{.25f, .5f, .75f, 1};
    Hr(device->SetTransform(D3DTS_PROJECTION, &projection), "native projection");
    Hr(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER), "native depth state");
    Hr(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE), "native blend state");
    Hr(device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE), "native scissor state");
    Hr(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP), "native sampler state");
    Hr(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE), "native stage state");
    Hr(device->SetVertexShaderConstantF(7, constants, 1), "native VS constants");
    Hr(device->SetPixelShaderConstantF(9, constants, 1), "native PS constants");
    Hr(device->ColorFill(f.surfaces[0].Get(), nullptr, DesktopColor), "desktop sentinel");
    GpuState gpu; gpu.Read(device);
    CoJIndependentRenderScope scope(f.memory.Io());
    auto bad = f.snapshot; bad.renderer_vtable ^= 4;
    Require(!scope.Begin(device, 7, bad, EyeHeight, Eye::left) && !f.memory.writes &&
        !scope.restoration_pending() && f.memory.SameBytes(original), "unqualified native class was admitted/mutated");
    gpu.Check(device);
    std::array<std::array<std::uint32_t, 14>, 2> payloads{};
    const DWORD top[]{0xffb02040, 0xff20b060}, bottom[]{0xff2040e0, 0xffe08020};
    for (unsigned eye = 0; eye < 2; ++eye) {
        Begun(scope, device, f.snapshot, eye ? Eye::right : Eye::left);
        Require(scope.active() && scope.restoration_pending() && scope.payload_bytes() > 0, "active scope has no retained transaction/bank");
        Require(!scope.Begin(device, 7, f.snapshot, EyeHeight, Eye::left) && !scope.ClearHistories() && !scope.Mirror(),
                "active eye admitted nesting/history clearing/mirroring");
        payloads[eye] = ActivePayload(f, device, original);
        for (unsigned index : {4U, 13U}) {
            TexturePixels(device, payloads[eye][index], 0);
            FillTexture(device, payloads[eye][index], eye ? 0xff00ff00 : 0xffff0000);
        }
        DrawPattern(device, top[eye], bottom[eye]);
        Pixels(device, Pointer<IDirect3DSurface9>(f.memory.At<std::uint32_t>(base + 0x1dc4)),
            {{1280, 12, top[eye]}, {1280, 1438, top[eye]}, {1280, 1442, bottom[eye]},
             {1, 1918, bottom[eye]}, {2558, 1918, bottom[eye]}});
        // Emulate all native CPU matrices, vectors, scalar/force flags and
        // shader-input sources changing during the full render-view wrapper.
        f.memory.Mutate(base + 8, 0x180); f.memory.Mutate(base + 0x188, 0x34);
        f.memory.Mutate(base + 0x1f1, 1); f.memory.Mutate(base + 0x3ac, 1);
        f.memory.Mutate(base + 0x1c71, 1); f.memory.Mutate(base + 0x1c74, 0x18);
        f.memory.Mutate(base + 0x404, 0x1080); f.memory.Mutate(base + 0x1cd0, 0xe4);
        f.memory.Put(base + 0x1bc, std::uint32_t{0});
        projection._11 = 9; const float changed[]{9, 8, 7, 6};
        Hr(device->SetTransform(D3DTS_PROJECTION, &projection), "eye projection mutation");
        Hr(device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP), "eye sampler mutation");
        Hr(device->SetVertexShaderConstantF(7, changed, 1), "eye VS mutation");
        Hr(device->SetPixelShaderConstantF(9, changed, 1), "eye PS mutation");
        Ended(scope); gpu.Check(device);
        Require(f.memory.SameBytes(restored), "End did not restore every native byte except dirty OR 0x00622000");
        OriginalObjects(f, original);
        Pixels(device, f.surfaces[0].Get(), {{0, 0, DesktopColor}, {1280, 720, DesktopColor}, {2559, 1439, DesktopColor}});
        if (!eye) Require(!scope.Mirror(), "left-only frame admitted desktop mirror");
    }
    std::set<std::uint32_t> all_payloads;
    for (unsigned i = 0; i < 14; ++i) {
        const bool history = i == 4 || i == 13;
        Require((payloads[0][i] != payloads[1][i]) == history, "per-eye history/shared scratch identity contract failed");
        all_payloads.insert(payloads[0][i]); all_payloads.insert(payloads[1][i]);
        if (history) {
            TexturePixels(device, payloads[0][i], 0xffff0000);
            TexturePixels(device, payloads[1][i], 0xff00ff00);
        }
    }
    Require(all_payloads.size() == 16, "bank does not own fourteen scratch/history roles with sixteen unique textures");
    Require(scope.Mirror(), "completed right-eye pair could not mirror"); gpu.Check(device);
    Require(f.memory.SameBytes(restored) && !scope.Mirror(), "mirror changed native fields or reused the completed pair");
    // 4:3 eye fitted into 16:9 desktop: exactly 320 black pixels on each
    // side, a 1920x1440 image, and the lower stripe begins at desktop y=1080.
    Pixels(device, f.surfaces[0].Get(), {{0, 720, 0}, {319, 720, 0}, {320, 720, top[1]},
        {2239, 720, top[1]}, {2240, 720, 0}, {2559, 720, 0}, {1280, 1078, top[1]},
        {1280, 1082, bottom[1]}, {1280, 1438, bottom[1]}});
    Require(scope.ClearHistories(), "inactive history clearing failed"); gpu.Check(device);
    for (unsigned eye = 0; eye < 2; ++eye)
        for (unsigned index : {4U, 13U}) TexturePixels(device, payloads[eye][index], 0);

    // A checked write may fail after changing a prefix. Successful automatic
    // rollback must restore native AND GPU state and allow a fresh eye.
    f.memory.fault_address = f.snapshot.buffers[6].texture.handle + 0x13c;
    Require(!scope.Begin(device, 7, f.snapshot, EyeHeight, Eye::left) && f.memory.fault_fired &&
            !scope.active() && !scope.restoration_pending(), "partial checked write did not roll back automatically");
    gpu.Check(device); Require(f.memory.SameBytes(restored), "partial checked-write rollback changed native preimage");
    Begun(scope, device, f.snapshot, Eye::left); Ended(scope); gpu.Check(device);

    // Persistent failure retains its original preimage and resources; neither
    // reset retirement nor another scope may discard recovery bookkeeping.
    f.memory.fault_address = f.snapshot.buffers[6].texture.handle + 0x13c;
    f.memory.persistent_fault = true; f.memory.fault_fired = false;
    Require(!scope.Begin(device, 7, f.snapshot, EyeHeight, Eye::left) && f.memory.fault_fired &&
            scope.active() && scope.restoration_pending(), "failed native rollback did not retain its transaction");
    gpu.Check(device);
    Require(f.memory.At<UINT>(f.snapshot.buffers[6].texture.handle + 0x140) == EyeHeight,
            "persistent fault fixture did not leave an actual partially written native dimension");
    Require(!scope.Begin(device, 7, f.snapshot, EyeHeight, Eye::right) && !scope.Mirror() &&
            !scope.ClearHistories() && !scope.InvalidateBeforeReset() && scope.restoration_pending(),
            "pending rollback allowed reuse or discarded retirement proof");
    f.memory.fault_address = 0; f.memory.persistent_fault = false;
    Ended(scope); gpu.Check(device);
    Require(f.memory.SameBytes(restored), "explicit native recovery lost the original preimage");
    Require(scope.InvalidateBeforeReset() && !scope.payload_bytes(), "recovered bank did not retire before reset");
    Begun(scope, device, f.snapshot, Eye::left); Ended(scope); gpu.Check(device);
    Require(scope.InvalidateBeforeReset() && !scope.active() && !scope.restoration_pending() && !scope.payload_bytes(),
            "final retirement retained DEFAULT bank resources");
    Require(f.memory.SameBytes(restored), "retirement changed native bytes");
    // Release every test-owned DEFAULT resource and observer reference too.
    // The facade stays alive across both resets to catch retained bank state.
    gpu = {}; mrt.Reset(); stream.Reset(); indices.Reset();
    for (DWORD i = 0; i < 16; ++i) Hr(device->SetTexture(i, nullptr), "reset sampler release");
    Hr(device->SetStreamSource(0, nullptr, 0, 0), "reset stream release"); Hr(device->SetIndices(nullptr), "reset indices release");
    for (DWORD i = 1; i < caps.NumSimultaneousRTs; ++i) Hr(device->SetRenderTarget(i, nullptr), "reset MRT release");
    Hr(device->SetDepthStencilSurface(nullptr), "reset depth release");
    f.textures = {}; f.surfaces = {};
    Hr(device->ResetEx(&pp, nullptr), "ResetEx after complete DEFAULT retirement");
    Hr(device->Reset(&pp), "Reset after complete DEFAULT retirement");
    ComPtr<IDirect3DSurface9> desktop; Hr(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &desktop), "post-reset desktop");
    D3DSURFACE_DESC desc{}; Hr(desktop->GetDesc(&desc), "post-reset desktop description");
    Require(desc.Width == Width && desc.Height == DesktopHeight, "eye rendering changed desktop presentation dimensions");
}
} // namespace

int main() {
    try {
        const auto* image = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        Require((pe->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0,
                "test requires /LARGEADDRESSAWARE: production VA-headroom admission must remain enabled");
        const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
        ComPtr<IDirect3D9Ex> api;
        Require(create != nullptr, "system D3D9Ex factory unavailable"); Hr(create(D3D_SDK_VERSION, &api), "system D3D9Ex");
        const HWND window = CreateWindowExW(0, L"STATIC", L"CoJ independent scope integration test", WS_OVERLAPPEDWINDOW,
            0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Require(window != nullptr, "GPU fixture window creation failed");
        struct Cleanup { HWND window; ~Cleanup() { DestroyWindow(window); } } cleanup{window};
        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
        pp.BackBufferWidth = Width; pp.BackBufferHeight = DesktopHeight; pp.BackBufferFormat = D3DFMT_A8R8G8B8;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        ComPtr<IDirect3DDevice9Ex> device;
        const auto hr = api->CreateDeviceEx(0, D3DDEVTYPE_HAL, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, nullptr, &device);
        if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) {
            std::cout << "SKIP: D3D9Ex HAL unavailable\n"; return 77;
        }
        Hr(hr, "real D3D9Ex HAL device");
        Run(api.Get(), device.Get(), pp);
        std::cout << "GREEN: real 2560x1920 D3D9Ex geometry/new-row readback, synthetic exact native family, "
            "per-eye histories, CPU/GPU restoration, 2560x1440 mirror, checked-write recovery and Reset/ResetEx\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "RED: " << error.what() << '\n'; return 1;
    }
}
