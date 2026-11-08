#include "backends/d3d9/sampled_depth_capture.hpp"
#include "backends/openvr/dlss_pair_session.hpp"
#include "backends/openvr/scene_depth_transfer.hpp"
#include "backends/openvr/d3d9_shared_texture_bridge.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include "backends/d3d9/factory_vtable_hook.hpp"
#include "games/call_of_juarez/sampled_scene_depth.hpp"
#include "games/call_of_juarez/sampled_scene_depth_export.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
using cojvr::backends::d3d9::D3D9SampledDepthCapture;
using cojvr::backends::d3d9::D3D9SampledDepthConfig;
using cojvr::backends::d3d9::D3D9SampledDepthSnapshot;

namespace {
constexpr UINT kWidth = 64, kHeight = 32;
constexpr std::uint64_t kGeneration = 7;
constexpr HRESULT kBusy = HRESULT_FROM_WIN32(ERROR_BUSY);

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Check(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        std::ostringstream out;
        out << message << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
        throw std::runtime_error(out.str());
    }
}
template<class F> void Wait(F&& poll, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    for (;;) {
        const auto hr = poll();
        if (hr == S_OK) return;
        Check(hr, message);
        Require(hr == S_FALSE && std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

constexpr D3DRENDERSTATETYPE kRenderStates[] = {
    D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_STENCILENABLE,
    D3DRS_STENCILFUNC, D3DRS_STENCILPASS, D3DRS_STENCILREF, D3DRS_STENCILMASK,
    D3DRS_STENCILWRITEMASK, D3DRS_ALPHATESTENABLE, D3DRS_ALPHAFUNC,
    D3DRS_ALPHAREF, D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND,
    D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_FOGENABLE, D3DRS_LIGHTING,
    D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_CULLMODE,
    D3DRS_FILLMODE, D3DRS_CLIPPLANEENABLE, D3DRS_COLORWRITEENABLE,
    D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3,
    D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK, D3DRS_WRAP0,
    D3DRS_DEPTHBIAS, D3DRS_SLOPESCALEDEPTHBIAS,
};
constexpr D3DSAMPLERSTATETYPE kSamplerStates[] = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_ADDRESSW,
    D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER,
    D3DSAMP_SRGBTEXTURE, D3DSAMP_MAXMIPLEVEL, D3DSAMP_MIPMAPLODBIAS,
    D3DSAMP_BORDERCOLOR, D3DSAMP_MAXANISOTROPY,
};
constexpr DWORD kTextureSlots[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    D3DDMAPSAMPLER, D3DVERTEXTEXTURESAMPLER0, D3DVERTEXTEXTURESAMPLER1,
    D3DVERTEXTEXTURESAMPLER2, D3DVERTEXTEXTURESAMPLER3};

struct NativeState {
    std::array<ComPtr<IDirect3DSurface9>, 4> targets;
    ComPtr<IDirect3DSurface9> depth;
    ComPtr<IDirect3DVertexShader9> vs;
    ComPtr<IDirect3DPixelShader9> ps;
    ComPtr<IDirect3DVertexDeclaration9> declaration;
    ComPtr<IDirect3DIndexBuffer9> indices;
    ComPtr<IDirect3DVertexBuffer9> stream;
    std::array<ComPtr<IDirect3DBaseTexture9>, std::size(kTextureSlots)> textures;
    std::array<DWORD, std::size(kRenderStates)> render;
    std::array<DWORD, std::size(kSamplerStates)> sampler;
    D3DVIEWPORT9 viewport{};
    RECT scissor{};
    DWORD fvf = 0, coord_index = 0, texture_transform = 0;
    UINT offset = 0, stride = 0, frequency = 0, mrt_count = 0;
    std::array<float, 4> vs_constant{}, ps_constant{};

    static NativeState Read(IDirect3DDevice9* d, UINT count) {
        NativeState s;
        s.mrt_count = count;
        for (UINT i = 0; i < count; ++i) {
            const auto hr = d->GetRenderTarget(i, &s.targets[i]);
            Check(i && hr == D3DERR_NOTFOUND ? S_OK : hr, "read RT");
        }
        const auto hr = d->GetDepthStencilSurface(&s.depth);
        Check(hr == D3DERR_NOTFOUND ? S_OK : hr, "read DS");
        Check(d->GetVertexShader(&s.vs), "read VS");
        Check(d->GetPixelShader(&s.ps), "read PS");
        Check(d->GetVertexDeclaration(&s.declaration), "read declaration");
        Check(d->GetIndices(&s.indices), "read index buffer");
        Check(d->GetStreamSource(0, &s.stream, &s.offset, &s.stride), "read stream");
        Check(d->GetStreamSourceFreq(0, &s.frequency), "read stream frequency");
        Check(d->GetFVF(&s.fvf), "read FVF");
        Check(d->GetViewport(&s.viewport), "read viewport");
        Check(d->GetScissorRect(&s.scissor), "read scissor");
        Check(d->GetTextureStageState(0, D3DTSS_TEXCOORDINDEX, &s.coord_index), "read coordinate index");
        Check(d->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &s.texture_transform), "read texture transform");
        Check(d->GetVertexShaderConstantF(3, s.vs_constant.data(), 1), "read VS constants");
        Check(d->GetPixelShaderConstantF(3, s.ps_constant.data(), 1), "read PS constants");
        for (std::size_t i = 0; i < std::size(kRenderStates); ++i)
            Check(d->GetRenderState(kRenderStates[i], &s.render[i]), "read renderstate");
        for (std::size_t i = 0; i < std::size(kSamplerStates); ++i)
            Check(d->GetSamplerState(0, kSamplerStates[i], &s.sampler[i]), "read sampler");
        for (std::size_t i = 0; i < std::size(kTextureSlots); ++i)
            Check(d->GetTexture(kTextureSlots[i], &s.textures[i]), "read texture");
        return s;
    }
    void Equals(const NativeState& b) const {
        for (UINT i = 0; i < mrt_count; ++i)
            Require(targets[i].Get() == b.targets[i].Get(), "RT0..3 identity was not restored");
        Require(depth.Get() == b.depth.Get(), "DS identity was not restored");
        Require(vs.Get() == b.vs.Get() && ps.Get() == b.ps.Get(), "native shaders were not restored");
        Require(declaration.Get() == b.declaration.Get() && fvf == b.fvf, "native declaration/FVF was not restored");
        Require(indices.Get() == b.indices.Get() && stream.Get() == b.stream.Get() &&
            offset == b.offset && stride == b.stride && frequency == b.frequency, "native stream/index state was not restored");
        Require(viewport.X == b.viewport.X && viewport.Y == b.viewport.Y &&
            viewport.Width == b.viewport.Width && viewport.Height == b.viewport.Height &&
            viewport.MinZ == b.viewport.MinZ && viewport.MaxZ == b.viewport.MaxZ,
            "native viewport was not restored");
        Require(EqualRect(&scissor, &b.scissor), "native scissor was not restored");
        Require(render == b.render && sampler == b.sampler, "native renderstate/sampler was not restored");
        Require(coord_index == b.coord_index && texture_transform == b.texture_transform,
            "native texture stage state was not restored");
        Require(vs_constant == b.vs_constant && ps_constant == b.ps_constant,
            "native shader constants were not restored");
        for (std::size_t i = 0; i < textures.size(); ++i)
            Require(textures[i].Get() == b.textures[i].Get(), "native texture binding was not restored");
    }
};

ComPtr<ID3DBlob> Compile(const char* source, const char* profile) {
    ComPtr<ID3DBlob> code, errors;
    Check(D3DCompile(source, std::strlen(source), "sampled_depth_fixture", nullptr,
        nullptr, "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors), "fixture shader compile");
    return code;
}

struct HostileState {
    ComPtr<IDirect3DVertexShader9> vs;
    ComPtr<IDirect3DPixelShader9> ps;
    ComPtr<IDirect3DVertexDeclaration9> declaration;
    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DVertexBuffer9> vertices;
    ComPtr<IDirect3DIndexBuffer9> indices;

    explicit HostileState(IDirect3DDevice9* d) {
        auto code = Compile("float4 main(float4 p : POSITION) : POSITION { return p; }", "vs_3_0");
        Check(d->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &vs), "create fixture VS");
        code = Compile("float4 main() : COLOR0 { return float4(1,0,1,1); }", "ps_3_0");
        Check(d->CreatePixelShader(static_cast<const DWORD*>(code->GetBufferPointer()), &ps), "create fixture PS");
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()
        };
        Check(d->CreateVertexDeclaration(elements, &declaration), "create fixture declaration");
        Check(d->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr), "create fixture texture");
        Check(d->CreateVertexBuffer(256, 0, 0, D3DPOOL_DEFAULT, &vertices, nullptr), "create fixture stream");
        Check(d->CreateIndexBuffer(32, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &indices, nullptr), "create fixture indices");
    }
    void Bind(IDirect3DDevice9* d, bool use_fvf) {
        Check(d->SetVertexShader(vs.Get()), "set fixture VS");
        Check(d->SetPixelShader(ps.Get()), "set fixture PS");
        if (use_fvf) Check(d->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1), "set native FVF");
        else Check(d->SetVertexDeclaration(declaration.Get()), "set native declaration");
        Check(d->SetStreamSource(0, vertices.Get(), 12, 24), "set native stream");
        Check(d->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | 2), "set native stream frequency");
        Check(d->SetIndices(indices.Get()), "set native indices");
        for (DWORD slot : kTextureSlots) Check(d->SetTexture(slot, texture.Get()), "set native texture");
        const D3DVIEWPORT9 vp{3, 2, kWidth - 7, kHeight - 5, 0.2F, 0.8F};
        const RECT scissor{9, 7, 21, 19};
        Check(d->SetViewport(&vp), "set hostile viewport");
        Check(d->SetScissorRect(&scissor), "set hostile scissor");
        constexpr std::pair<D3DRENDERSTATETYPE, DWORD> states[] = {
            {D3DRS_ZENABLE, TRUE}, {D3DRS_ZWRITEENABLE, TRUE}, {D3DRS_ZFUNC, D3DCMP_GREATER},
            {D3DRS_STENCILENABLE, TRUE}, {D3DRS_STENCILFUNC, D3DCMP_NEVER},
            {D3DRS_STENCILREF, 83}, {D3DRS_ALPHATESTENABLE, TRUE}, {D3DRS_ALPHAFUNC, D3DCMP_NEVER},
            {D3DRS_ALPHABLENDENABLE, TRUE}, {D3DRS_SRCBLEND, D3DBLEND_SRCALPHA},
            {D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA}, {D3DRS_SEPARATEALPHABLENDENABLE, TRUE},
            {D3DRS_FOGENABLE, TRUE}, {D3DRS_LIGHTING, TRUE}, {D3DRS_SCISSORTESTENABLE, TRUE},
            {D3DRS_SRGBWRITEENABLE, TRUE}, {D3DRS_CULLMODE, D3DCULL_CW},
            {D3DRS_FILLMODE, D3DFILL_WIREFRAME}, {D3DRS_CLIPPLANEENABLE, 1},
            {D3DRS_COLORWRITEENABLE, 1}, {D3DRS_COLORWRITEENABLE1, 2},
            {D3DRS_COLORWRITEENABLE2, 4}, {D3DRS_COLORWRITEENABLE3, 8},
            {D3DRS_MULTISAMPLEMASK, 0}, {D3DRS_WRAP0, D3DWRAP_U | D3DWRAP_V},
        };
        for (auto state : states) Check(d->SetRenderState(state.first, state.second), "set hostile renderstate");
        constexpr std::pair<D3DSAMPLERSTATETYPE, DWORD> samplers[] = {
            {D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR}, {D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP},
            {D3DSAMP_MINFILTER, D3DTEXF_LINEAR}, {D3DSAMP_MAGFILTER, D3DTEXF_LINEAR},
            {D3DSAMP_MIPFILTER, D3DTEXF_LINEAR}, {D3DSAMP_SRGBTEXTURE, TRUE},
            {D3DSAMP_MAXMIPLEVEL, 3}, {D3DSAMP_MIPMAPLODBIAS, 0x3f800000},
        };
        for (auto state : samplers) Check(d->SetSamplerState(0, state.first, state.second), "set hostile sampler");
        Check(d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 1), "set hostile coordinate index");
        Check(d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2), "set hostile texture transform");
        const float constants[] = {0.125F, -2.0F, 17.0F, 91.0F};
        Check(d->SetVertexShaderConstantF(3, constants, 1), "set native VS constants");
        Check(d->SetPixelShaderConstantF(3, constants, 1), "set native PS constants");
    }
};

// Binary16 decoding is independent of the producer shader and does not require
// a CPU instruction-set extension. Integer bytes 0..255 must survive exactly.
float Half(std::uint16_t bits) {
    const int exponent = (bits >> 10) & 31;
    const int mantissa = bits & 1023;
    if (exponent == 31) return std::nanf("");
    const float magnitude = exponent ? std::ldexp(1.0F + mantissa / 1024.0F, exponent - 15)
                                     : std::ldexp(static_cast<float>(mantissa), -24);
    return bits & 0x8000 ? -magnitude : magnitude;
}

struct Consumer {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    explicit Consumer(IDirect3D9Ex* api) {
        LUID luid{};
        Check(api->GetAdapterLUID(D3DADAPTER_DEFAULT, &luid), "D3D9 adapter LUID");
        ComPtr<IDXGIFactory1> factory;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "DXGI factory");
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0;; ++i) {
            ComPtr<IDXGIAdapter1> candidate;
            const auto hr = factory->EnumAdapters1(i, &candidate);
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            Check(hr, "DXGI adapter enumeration");
            DXGI_ADAPTER_DESC1 desc{};
            Check(candidate->GetDesc1(&desc), "DXGI adapter description");
            if (desc.AdapterLuid.HighPart == luid.HighPart && desc.AdapterLuid.LowPart == luid.LowPart) {
                adapter = std::move(candidate); break;
            }
        }
        Require(!!adapter, "D3D11 adapter did not match producer LUID");
        constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        Check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            &device, nullptr, &context), "matching D3D11 device");
    }
    void Read(const D3D9SampledDepthSnapshot& snapshot, double expected) {
        Require(snapshot.valid() && snapshot.shared_handle(), "unproven capture reached consumer");
        Require(snapshot.width() == kWidth && snapshot.height() == kHeight &&
            snapshot.generation() == kGeneration, "capture metadata changed");
        ComPtr<ID3D11Texture2D> shared;
        Check(device->OpenSharedResource(snapshot.shared_handle(), IID_PPV_ARGS(&shared)), "open private depth RGB24 image");
        D3D11_TEXTURE2D_DESC desc{};
        shared->GetDesc(&desc);
        Require(desc.Width == kWidth && desc.Height == kHeight && desc.MipLevels == 1 &&
            desc.ArraySize == 1 && desc.SampleDesc.Count == 1 && desc.SampleDesc.Quality == 0 &&
            desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT, "shared packed-depth layout mismatch");
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        Check(device->CreateTexture2D(&desc, nullptr, &staging), "consumer staging allocation");
        D3D11_QUERY_DESC query_desc{D3D11_QUERY_EVENT, 0};
        ComPtr<ID3D11Query> completion;
        Check(device->CreateQuery(&query_desc, &completion), "consumer completion query");
        context->CopyResource(staging.Get(), shared.Get());
        context->End(completion.Get());
        context->Flush();
        Wait([&] {
            BOOL completed = FALSE;
            const auto hr = context->GetData(completion.Get(), &completed, sizeof(completed), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            return hr == S_OK && !completed ? E_FAIL : hr;
        }, "consumer copy completion timeout/failure");
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "map packed-depth readback");
        bool valid = true;
        const auto expected_n = static_cast<std::uint32_t>(std::floor(expected * 16777215.0 + 0.5));
        std::uint32_t first_n = 0;
        for (UINT y = 0; y < kHeight; ++y) {
            const auto* row = reinterpret_cast<const std::uint16_t*>(
                static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch);
            for (UINT x = 0; x < kWidth; ++x) {
                const float r = Half(row[4*x]), g = Half(row[4*x+1]), b = Half(row[4*x+2]);
                const float alpha = Half(row[4*x+3]);
                if (!(std::isfinite(r) && std::isfinite(g) && std::isfinite(b) &&
                    r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 &&
                    r == std::floor(r) && g == std::floor(g) && b == std::floor(b))) {
                    valid = false; continue;
                }
                const auto n = static_cast<std::uint32_t>(r) * 65536 +
                    static_cast<std::uint32_t>(g) * 256 + static_cast<std::uint32_t>(b);
                if (!x && !y) first_n = n;
                // Native Clear takes FP32 and D24 conversion can round either
                // side of one least-significant unit. Never allow FP16-Z error.
                const auto delta = std::int64_t{n} - expected_n;
                valid = valid && delta >= -1 && delta <= 1 && n == first_n &&
                    std::isfinite(alpha) && std::abs(alpha - expected) < 0.001;
                if (expected < 1.0) valid = valid && n < 16777215;
                if (expected == 1.0) valid = valid && n == 16777215;
            }
        }
        context->Unmap(staging.Get(), 0);
        Require(valid, "decoded RGB24 does not match native device-Z clear (or far Z collapsed to 1)");
    }
};

void ClearDepth(IDirect3DDevice9* d, float z) {
    // Explicitly clear the entire own depth, independently of hostile native
    // viewport/scissor settings restored by the prior snapshot.
    const D3DVIEWPORT9 viewport{0, 0, kWidth, kHeight, 0.0F, 1.0F};
    Check(d->SetViewport(&viewport), "full-depth clear viewport");
    Check(d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE), "full-depth clear scissor");
    Check(d->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, z, 83), "native D3D9 depth clear");
}
void Ready(const D3D9SampledDepthSnapshot& snapshot) {
    Wait([&] { return snapshot.PollReady(); }, "producer event proof timeout/failure");
    Require(snapshot.shared_handle() != nullptr, "completed producer did not expose a shared handle");
}

enum class FinalOpticsFixture { matching, unavailable, changed };
void CheckCoJSceneBoundary(IDirect3DDevice9Ex* device, UINT width=kWidth, UINT height=kHeight,
    FinalOpticsFixture final_optics_case=FinalOpticsFixture::matching, bool full_transport=true) {
    namespace coj = cojvr::games::call_of_juarez;
    constexpr auto format = D3DFMT_A16B16G16R16F;
    constexpr DWORD flags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER;
    ComPtr<IDirect3DSurface9> scene_color, original_depth, terminal_surface, before;
    ComPtr<IDirect3DTexture9> terminal;
    Check(device->GetRenderTarget(0,&before), "save fixture color");
    Check(device->CreateRenderTarget(width,height,format,D3DMULTISAMPLE_NONE,0,FALSE,&scene_color,nullptr), "scene FP16 RT");
    Check(device->CreateDepthStencilSurface(width,height,D3DFMT_D24X8,D3DMULTISAMPLE_NONE,0,FALSE,&original_depth,nullptr), "scene D24X8 depth");
    Check(device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,format,D3DPOOL_DEFAULT,&terminal,nullptr), "terminal texture");
    Check(terminal->GetSurfaceLevel(0,&terminal_surface), "terminal surface");
    Check(device->SetRenderTarget(0,scene_color.Get()), "bind scene FP16 RT");
    Check(device->SetDepthStencilSurface(original_depth.Get()), "bind native D24X8");
    Check(device->SetRenderState(D3DRS_STENCILENABLE,FALSE), "native stencil disabled");
    coj::CoJPostprocessResourceSnapshot native{};
    const auto id = [](auto* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    native.device = id(static_cast<IDirect3DDevice9*>(device));
    native.renderer_state.internal_color_surface = id(scene_color.Get());
    native.renderer_state.internal_depth_surface = id(original_depth.Get());
    native.buffer_count = 1;
    auto& buffer = native.buffers[0];
    std::memcpy(buffer.name.data(), "1B_16", 6);
    buffer.texture.texture_2d = id(terminal.Get());
    buffer.texture.width = width;
    buffer.texture.height = height;
    const bool full_color=width>64&&full_transport;
    if(width==2804&&height==2860&&!full_transport) {
        D3D9SampledDepthCapture over_budget;
        const D3D9SampledDepthConfig full_config{width,height,format,D3DFMT_D24X8,
            D3DMULTISAMPLE_NONE,0,kGeneration,true};
        Require(over_budget.Initialize(device,full_config)==E_OUTOFMEMORY&&
            std::strcmp(over_budget.last_error(),"native capture storage exceeds the configured memory budget")==0,
            "full-size temporal capture hid its storage failure behind a driver capability verdict");
    }
    coj::SampledSceneDepth adapter(full_color);
    Require(!adapter.Begin(device,native,kGeneration,1,1,1,0,1), "unqualified game source admitted");
    Require(std::strcmp(adapter.last_error(),"native_snapshot_invalid")==0,
        "unqualified source lost its rejection reason");
    native.valid = true;
    const auto native_device=native.device;
    native.device=0;
    Require(!adapter.Begin(device,native,kGeneration,1,1,1,0,1)&&
        std::strcmp(adapter.last_error(),"native_device_mismatch")==0,
        "mismatched device lost its rejection reason");
    native.device=native_device;
    native.buffer_count=0;
    Require(!adapter.Begin(device,native,kGeneration,1,1,1,0,1)&&
        std::strcmp(adapter.last_error(),"terminal_unavailable")==0,
        "missing terminal lost its rejection reason");
    native.buffer_count=2;
    native.buffers[1]=buffer;
    Require(!adapter.Begin(device,native,kGeneration,1,1,1,0,1)&&
        std::strcmp(adapter.last_error(),"terminal_ambiguous")==0,
        "ambiguous terminal lost its rejection reason");
    native.buffer_count=1;
    Require(adapter.Begin(device,native,kGeneration,1,1,1,0,1), "qualified fixture source could not start");
    const D3DRECT partial{0,0,static_cast<LONG>(width/4),static_cast<LONG>(height/4)};
    adapter.BeforeClear(device,1,&partial,flags,1);
    Require(adapter.MapDepth(device,original_depth.Get()) == original_depth.Get(), "partial clear changed native depth");
    bool queued = true;
    Require(adapter.End(1,0,kGeneration,1,1,true,queued) && !queued, "partial clear published depth");
    Require(std::strcmp(adapter.last_error(),"scene_clear_contract")==0,
        "capture rejection lost the failed clear boundary");
    Require(adapter.Begin(device,native,kGeneration,1,2,1,0,2), "complete fixture source could not start");
    Check(device->BeginScene(), "begin native depth transaction");
    adapter.BeforeClear(device,0,nullptr,flags,1);
    const auto cleared = device->Clear(0,nullptr,flags,0xff9f4533,1.0F,0);
    adapter.AfterClear(device,cleared);
    Check(cleared,"original full scene clear");
    auto* mapped = adapter.MapDepth(device,original_depth.Get());
    Require(mapped && mapped != original_depth.Get(), "native rebind missed private depth");
    Check(device->SetDepthStencilSurface(nullptr), "simulate interposed postprocess target");
    Check(device->SetDepthStencilSurface(mapped), "resume original scene depth ownership");
    Check(device->Clear(1,&partial,D3DCLEAR_ZBUFFER,0,.25F,0), "write pattern to live depth");
    const D3DRECT far_color{static_cast<LONG>(width*3/4),static_cast<LONG>(height*3/4),
        static_cast<LONG>(width),static_cast<LONG>(height)};
    Check(device->Clear(1,&far_color,D3DCLEAR_TARGET,0xff0000ff,1,0),"write far color marker");
    const auto copy = device->StretchRect(scene_color.Get(),nullptr,terminal_surface.Get(),nullptr,D3DTEXF_NONE);
    Check(copy, "native terminal copy");
    D3DCAPS9 scene_caps{};
    Check(device->GetDeviceCaps(&scene_caps),"read scene state limits");
    const auto native_state=NativeState::Read(device,std::min<DWORD>(scene_caps.NumSimultaneousRTs,4));
    const coj::CoJNativeTemporalOptics fixture_optics{
        cojvr::runtime::TemporalIdentity(),17,.25F,100.F,true,true};
    adapter.AfterCopy(device,scene_color.Get(),nullptr,terminal_surface.Get(),nullptr,copy,
        +[](unsigned eye,std::uint64_t frame,std::uint64_t pose) noexcept {
            if (eye!=0 || frame!=2 || pose!=2) return coj::CoJNativeTemporalOptics{};
            return coj::CoJNativeTemporalOptics{
                cojvr::runtime::TemporalIdentity(),17,.25F,100.F,true,true};
        });
    native_state.Equals(NativeState::Read(device,std::min<DWORD>(scene_caps.NumSimultaneousRTs,4)));
    Check(device->EndScene(), "finish native depth transaction");
    Require(adapter.End(2,0,kGeneration,1,1,true,queued) && queued, "complete native depth transaction rejected");
    ComPtr<IDirect3DSurface9> restored;
    Check(device->GetDepthStencilSurface(&restored), "read restored native depth");
    Require(restored.Get() == original_depth.Get(), "original native depth was not restored");
    ComPtr<IDirect3DSurface9> final_color;
    Check(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,
        &final_color,nullptr),"allocate final eye fixture");
    Check(device->ColorFill(final_color.Get(),nullptr,0xff204080),"fill final eye fixture");
    Require(adapter.CaptureFinalColor(device,final_color.Get(),kGeneration,3,0,2)==S_FALSE &&
        adapter.CaptureFinalColor(device,final_color.Get(),kGeneration,2,1,2)==S_FALSE &&
        adapter.CaptureFinalColor(device,final_color.Get(),kGeneration,2,0,3)==S_FALSE,
        "final eye matched a different frame, eye or pose");
    auto final_optics=fixture_optics;
    if(final_optics_case==FinalOpticsFixture::unavailable)final_optics={};
    if(final_optics_case==FinalOpticsFixture::changed)final_optics.view_projection_metres[12]+=1;
    const bool expected_optics_match=final_optics_case==FinalOpticsFixture::matching;
    Check(adapter.CaptureFinalColor(device,final_color.Get(),kGeneration,2,0,2,final_optics),
        "copy same-frame final eye");
    Require(adapter.CaptureFinalColor(device,final_color.Get(),kGeneration,2,0,2)!=S_OK,
        "duplicate final eye replaced the snapshot");
    Check(device->ColorFill(final_color.Get(),nullptr,0xff00ff00),"overwrite final eye after capture");
    Check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff00ff00,1,0),"overwrite native color after capture");
    bool ready = false;
    D3D9SampledDepthSnapshot retained_preview;
    Wait([&] {
        const auto report=[&](auto frame, auto pose, auto owner, auto eye, const auto& image, HRESULT status,
            const coj::CoJTemporalPreviewOptics& optics={}) {
            Check(status, "sampled scene fence");
            Require(frame == 2 && pose == 2 && owner == 1 && eye == 0, "scene capture lost provenance");
            std::array<std::uint32_t,64> grid{};
            Check(image.ReadDiagnosticGrid(grid), "bounded diagnostic depth grid");
            Require(grid.front() < 16777215 && grid.back() == 16777215, "depth grid lost near/far layout");
            cojvr::backends::d3d9::D3D9SampledDepthPreview preview;
            Check(image.ReadDiagnosticPreview(preview),"read joint color/depth preview");
            Require(preview.final_color_bgra8.size()==preview.color_rgba16f.size() &&
                preview.final_color_format==D3DFMT_A8R8G8B8 &&
                preview.final_color_bgra8[0]==0x80 && preview.final_color_bgra8[1]==0x40 &&
                preview.final_color_bgra8[2]==0x20,"final eye copy lost its original color");
            retained_preview=image;
            Require(preview.width>0&&preview.height>0&&preview.width<=512&&preview.height<=512&&
                preview.color_rgba16f.size()==preview.width*preview.height*4&&
                preview.depth_rgba16f.size()==preview.color_rgba16f.size(),
                "preview dimensions or planes disagree");
            Require(std::abs(Half(preview.color_rgba16f[0])-159.F/255)<.001F&&
                std::abs(Half(preview.color_rgba16f[1])-69.F/255)<.001F&&
                std::abs(Half(preview.color_rgba16f[2])-.2F)<.001F,
                "preview did not retain the original scene color");
            const auto z=[&](std::size_t pixel) {
                return Half(preview.depth_rgba16f[pixel*4])*65536+
                    Half(preview.depth_rgba16f[pixel*4+1])*256+Half(preview.depth_rgba16f[pixel*4+2]);};
            Require(z(0)<16777215&&z(preview.width*preview.height-1)==16777215,
                "joint preview lost its aligned near/far depth pattern");
            const auto far_pixel=preview.color_rgba16f.size()-4;
            Require(Half(preview.color_rgba16f[far_pixel])==0&&
                Half(preview.color_rgba16f[far_pixel+2])==1,"far color marker did not align with far depth");
            const auto output=std::filesystem::temp_directory_path()/
                ("cojvr-scene-preview-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(width));
            const coj::CoJSceneDepthPreviewIdentity identity{frame,pose,owner,image.generation(),
                image.sequence(),eye,image.width(),image.height()};
            const auto manifest=coj::ExportCoJSceneDepthPreview(output,"fixture",identity,preview,optics);
            Require(!manifest.empty()&&std::filesystem::exists(manifest),"joint preview manifest was not committed");
            Require(coj::ExportCoJSceneDepthPreview(output,"fixture",identity,preview).empty(),
                "same preview identity overwrote published files");
            std::ifstream metadata(manifest,std::ios::binary);
            const std::string json{std::istreambuf_iterator<char>(metadata),std::istreambuf_iterator<char>()};
            Require(json.find("\"frame_sequence\":2")!=std::string::npos&&
                json.find("\"final_color_format\":\"bgra8_le\"")!=std::string::npos&&
                json.find("\"temporal_admitted\":false")!=std::string::npos,
                "preview manifest lost source identity or claimed temporal admission");
            if (full_color) Require(json.find(expected_optics_match?
                "\"final_color_match\":true":"\"final_color_match\":false")!=std::string::npos&&
                json.find(final_optics.valid?"\"final_color_optics_valid\":true":
                    "\"final_color_optics_valid\":false")!=std::string::npos&&
                json.find("\"recenter_sequence\":17")!=std::string::npos,"manifest lost independent final-eye optics status");
            metadata.close();
            std::size_t files=0;
            for (const auto& entry:std::filesystem::directory_iterator(manifest.parent_path())) {
                if (entry.path().extension()==".f16")
                    Require(entry.file_size()==preview.color_rgba16f.size()*2,"preview payload size mismatch");
                ++files;
                std::filesystem::remove(entry.path());
            }
            Require(files==4,"preview export left incomplete temporary files");
            std::filesystem::remove(manifest.parent_path());
            std::filesystem::remove(output/"runtime");
            std::filesystem::remove(output);
            ready = true;
        };
        if(full_color)adapter.PollPackets([&](const auto& packet,HRESULT result,const auto& optics) {
            Require(packet.stamp.device==reinterpret_cast<std::uintptr_t>(device)&&
                packet.stamp.generation==kGeneration&&packet.stamp.integrity==1&&packet.stamp.epoch!=0&&
                packet.snapshot.color_shared_handle(),"exact-game packet lost its admitted source identity/color");
            Require(optics.depth==fixture_optics&&optics.final_color_match==expected_optics_match,
                "packet lost same-eye native optics or admitted changed/unavailable final optics");
            report(packet.stamp.frame,packet.stamp.pose,packet.stamp.owner,packet.eye,packet.snapshot,result,optics);
        });
        else {
            adapter.PollPackets([&](const auto&,HRESULT,const auto&) {
                throw std::runtime_error("preview-only source escaped through the full-color worker path");
            });
            adapter.PollPreviews([&](const auto& observation,HRESULT result,const auto& optics) {
                Require(!observation.snapshot.color_shared_handle()&&
                    optics.final_color_match==expected_optics_match,
                    "preview-only observation gained a full color carrier or lost matched optics");
                report(observation.stamp.frame,observation.stamp.pose,observation.stamp.owner,
                    observation.eye,observation.snapshot,result,optics);
            });
        }
        return ready ? S_OK : S_FALSE;
    }, "scene capture did not complete");
    Require(adapter.Retire(), "scene capture owner retirement failed");
    cojvr::backends::d3d9::D3D9SampledDepthPreview retained_pixels;
    Check(retained_preview.ReadDiagnosticPreview(retained_pixels),"preview lease lost its retired producer");
    Require(std::abs(Half(retained_pixels.color_rgba16f[0])-159.F/255)<.001F,
        "retained preview changed after owner retirement");
    {
        coj::SampledSceneDepth unsupported;
        const auto saved_width=buffer.texture.width;
        buffer.texture.width=D3D9SampledDepthCapture::max_extent+1;
        Require(!unsupported.Begin(device,native,kGeneration,1,3,1,0,3)&&
            FAILED(unsupported.last_result())&&std::strcmp(unsupported.last_error(),"none")!=0,
            "initialization failure lost its native result or explanation");
        const auto result=unsupported.last_result();
        const std::string reason=unsupported.last_error();
        buffer.texture.width=saved_width;
        Require(!unsupported.Begin(device,native,kGeneration,1,4,1,1,4)&&
            unsupported.last_result()==result&&reason==unsupported.last_error(),
            "disabled probe replaced its original initialization failure");
    }
    Check(device->SetDepthStencilSurface(nullptr), "detach native fixture depth");
    Check(device->SetRenderTarget(0,before.Get()), "restore fixture color");
}

// Raw HDR values, negative channels and sub-unity alpha must survive without
// clamping, gamma conversion or depth-carrier reinterpretation.
constexpr std::array<std::array<std::uint16_t,4>,2> kHdrColors{{
    {0x4000,0xb800,0x3400,0x3800},{0x3c00,0x0000,0xbc00,0x3c00}}};

void ReadFullColorPair(Consumer& consumer,const D3D9SampledDepthSnapshot& image,
                      unsigned pattern,double depth) {
    Require(image.color_shared_handle()&&image.shared_handle(),
        "completed color/depth pair has no color transport");
    std::array<ComPtr<ID3D11Texture2D>,2> shared,staging;
    const HANDLE handles[]{image.color_shared_handle(),image.shared_handle()};
    Require(handles[0]!=handles[1],"color and packed depth alias");
    for(unsigned plane=0;plane<2;++plane) {
        Check(consumer.device->OpenSharedResource(handles[plane],IID_PPV_ARGS(&shared[plane])),
            "open immutable full-size capture");
        D3D11_TEXTURE2D_DESC desc{};shared[plane]->GetDesc(&desc);
        Require(desc.Width==image.width()&&desc.Height==image.height()&&
            desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT&&desc.SampleDesc.Count==1&&
            desc.MipLevels==1&&desc.ArraySize==1,"full-size capture metadata mismatch");
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;
        desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(consumer.device->CreateTexture2D(&desc,nullptr,&staging[plane]),"paired staging");
        consumer.context->CopyResource(staging[plane].Get(),shared[plane].Get());
    }
    ComPtr<ID3D11Query> completion;
    const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
    Check(consumer.device->CreateQuery(&query,&completion),"paired consumer fence");
    consumer.context->End(completion.Get());consumer.context->Flush();
    Wait([&] { BOOL done=FALSE;const auto hr=consumer.context->GetData(completion.Get(),&done,
        sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);return hr==S_OK&&!done?E_FAIL:hr; },
        "paired consumer completion");
    const auto expected=static_cast<std::int64_t>(std::floor(depth*16777215+.5));
    for(unsigned plane=0;plane<2;++plane) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(consumer.context->Map(staging[plane].Get(),0,D3D11_MAP_READ,0,&mapped),"paired map");
        bool valid=mapped.pData&&mapped.RowPitch>=image.width()*8;
        for(UINT y=0;valid&&y<image.height();++y) {
            const auto* row=reinterpret_cast<const std::uint16_t*>(
                static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch);
            for(UINT x=0;x<image.width();++x) {
                if(!plane) {
                    const auto& color=kHdrColors[(x+y+pattern)%2];
                    for(unsigned c=0;c<4;++c)valid=valid&&row[4*x+c]==color[c];
                } else {
                    const auto z=static_cast<std::int64_t>(Half(row[4*x])*65536+
                        Half(row[4*x+1])*256+Half(row[4*x+2]));
                    valid=valid&&std::abs(z-expected)<=1;
                }
            }
        }
        consumer.context->Unmap(staging[plane].Get(),0);
        Require(valid,plane?"immutable paired depth changed":"raw full-size HDR color changed");
    }
}

void CheckPairTransfer(IDirect3DDevice9Ex* producer,Consumer& consumer,const std::array<D3D9SampledDepthSnapshot,2>& images) {
    namespace dlss=cojvr::backends::openvr::dlss;
    using cojvr::backends::openvr::D3D9SharedTextureBridge;
    using cojvr::backends::openvr::D3D9SharedTextureCopyTiming;
    const dlss::SceneDepthStamp stamp{images[0].producer_device(),kGeneration,100,200,42,3,1};
    const UINT captured_width=images[0].width(),captured_height=images[0].height();
    std::array<dlss::SceneDepthEye,2> eyes{{{stamp,0,images[0]},{stamp,1,images[1]}}};
    for(unsigned reason=0;reason<12;++reason) {
        auto bad=eyes;
        switch(reason) {
        case 0:++bad[1].stamp.frame;break;case 1:++bad[1].stamp.pose;break;
        case 2:++bad[1].stamp.owner;break;case 3:++bad[1].stamp.generation;break;
        case 4:++bad[1].stamp.device;break;case 5:++bad[1].stamp.integrity;break;
        case 6:++bad[1].stamp.epoch;break;case 7:bad[1].eye=0;break;
        case 8:bad[1].snapshot=images[0];break;
        case 9:++bad[0].stamp.device;bad[1].stamp=bad[0].stamp;break;
        case 10:++bad[0].stamp.generation;bad[1].stamp=bad[0].stamp;break;
        case 11:bad[0].stamp.integrity=0;bad[1].stamp=bad[0].stamp;break;
        }
        dlss::SceneDepthTransfer rejected;dlss::SceneDepthWork absent;
        Require(!rejected.Queue(bad,absent)&&!absent.color&&!absent.depth,
            "foreign/mixed/aliased eye pair reached worker");
    }
    dlss::SceneDepthTransfer transfer;dlss::SceneDepthWork work;
    bool foreign_owner_queued=true;
    std::thread wrong_owner([&]{
        dlss::SceneDepthTransfer wrong;dlss::SceneDepthWork absent;
        foreign_owner_queued=wrong.Queue(eyes,absent);
    });
    wrong_owner.join();Require(!foreign_owner_queued,"non-producer thread retained D3D9 snapshots");
    Require(transfer.Queue(eyes,work)&&work.color&&work.depth,"complete stamped pair did not reach worker");
    Require(work.stamp==stamp&&work.color->render_pose_sequence==stamp.pose&&
        work.depth->capture_sequence==stamp.frame&&work.color->generation==stamp.epoch&&
        work.depth->generation==stamp.epoch,"worker packets lost source identity/cache epoch");
    dlss::SceneDepthWork busy;dlss::SceneDepthStamp completed{};
    Require(!transfer.Queue(eyes,busy)&&!busy.color&&!busy.depth,"bounded worker ticket overwritten");
    Require(transfer.Poll(stamp,completed)==dlss::SceneDepthDelivery::pending&&completed.frame==0,
        "source lease retired before worker GPU completion");
    D3D9SharedTextureBridge color,depth;
    color.ForceFencePendingForTest(true);
    std::array<ComPtr<ID3D11Texture2D>,4> destinations;
    std::exception_ptr failure;
    const auto run=[&](auto action) {
        std::thread worker([&]{try{action();}catch(...){failure=std::current_exception();}});
        worker.join();if(failure)std::rethrow_exception(failure);
    };
    run([&]{
        D3D11_TEXTURE2D_DESC desc{};desc.Width=captured_width;desc.Height=captured_height;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        for(auto& destination:destinations)
            Check(consumer.device->CreateTexture2D(&desc,nullptr,&destination),"worker input allocation");
        D3D9SharedTextureCopyTiming timing{};
        Require(color.CopyFrame(consumer.device.Get(),consumer.context.Get(),*work.color,
            {destinations[0].Get(),destinations[1].Get()},timing),"worker color copy rejected");
        Require(depth.CopyFrame(consumer.device.Get(),consumer.context.Get(),*work.depth,
            {destinations[2].Get(),destinations[3].Get()},timing),"worker depth copy rejected");
        work.AcceptCopies();
        consumer.context->Flush();
        Wait([&]{depth.Poll(consumer.context.Get());return depth.stats().pending_copy_fences?S_FALSE:S_OK;},
            "worker depth copy did not retire");
        color.Poll(consumer.context.Get());
    });
    Require(transfer.Poll(stamp,completed)==dlss::SceneDepthDelivery::pending,
        "depth completion alone released paired source leases");
    const bool invalidate_in_flight=images[0].width()>64;
    if(invalidate_in_flight) {
        transfer.Invalidate();
        Require(transfer.Poll(stamp,completed)==dlss::SceneDepthDelivery::pending,
            "in-flight invalidation released an unfinished color copy");
    }
    run([&]{
        color.ForceFencePendingForTest(false);
        Wait([&]{color.Poll(consumer.context.Get());return color.stats().pending_copy_fences?S_FALSE:S_OK;},
            "worker color copy did not retire");
        for(unsigned part=0;part<4;++part) {
            D3D11_TEXTURE2D_DESC desc{};destinations[part]->GetDesc(&desc);
            desc.Width=desc.Height=1;desc.Usage=D3D11_USAGE_STAGING;
            desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> pixel;
            Check(consumer.device->CreateTexture2D(&desc,nullptr,&pixel),"worker sample allocation");
            const D3D11_BOX box{0,0,0,1,1,1};
            consumer.context->CopySubresourceRegion(pixel.Get(),0,0,0,0,destinations[part].Get(),0,&box);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            Check(consumer.context->Map(pixel.Get(),0,D3D11_MAP_READ,0,&mapped),"read worker input sample");
            std::array<std::uint16_t,4> value{};std::memcpy(value.data(),mapped.pData,8);
            consumer.context->Unmap(pixel.Get(),0);
            if(part<2)Require(value==kHdrColors[part],"worker color eyes changed/swapped");
            else {
                const double z=(Half(value[0])*65536+Half(value[1])*256+Half(value[2]))/16777215.;
                Require(std::abs(z-(part==2?.25:.75))<1e-6,"worker depth eyes changed/swapped");
            }
        }
    });
    Require(transfer.Poll(stamp,completed)==(invalidate_in_flight?dlss::SceneDepthDelivery::discarded:
        dlss::SceneDepthDelivery::ready)&&completed==(invalidate_in_flight?dlss::SceneDepthStamp{}:stamp),
        "worker delivery ignored source authority or GPU completion");
    work={};Require(!transfer.Queue(eyes,work),"repeated source pair was replayed");
    auto replay=eyes;
    for(auto& eye:replay){++eye.stamp.frame;++eye.stamp.pose;++eye.stamp.epoch;}
    Require(!transfer.Queue(replay,work),"old capture was relabeled as a fresh frame/epoch");
    // A used right-eye snapshot must not become a later frame's left eye.
    D3D9SampledDepthCapture next_capture;
    const D3D9SampledDepthConfig next_config{captured_width,captured_height,D3DFMT_A16B16G16R16F,
        D3DFMT_D24X8,D3DMULTISAMPLE_NONE,0,kGeneration,true};
    Check(next_capture.Initialize(producer,next_config),"cross-eye replay source init");
    ComPtr<IDirect3DSurface9> saved_depth;
    const auto saved_depth_result=producer->GetDepthStencilSurface(&saved_depth);
    if(saved_depth_result!=D3DERR_NOTFOUND)Check(saved_depth_result,"retain cross-eye source depth");
    Check(producer->SetDepthStencilSurface(next_capture.native_depth_surface()),"bind next source depth");
    Check(producer->BeginScene(),"begin next source snapshot");
    Check(producer->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,.5F,0),"clear next source depth");
    D3D9SampledDepthSnapshot fresh;
    Check(next_capture.Snapshot(images[1].sequence()+1,kGeneration,fresh),"capture next source sequence");
    Check(producer->EndScene(),"end next source snapshot");
    Check(producer->SetDepthStencilSurface(saved_depth.Get()),"restore replay fixture depth");
    Ready(fresh);
    auto mixed=eyes;mixed[0].snapshot=images[1];mixed[1].snapshot=fresh;
    for(auto& eye:mixed){++eye.stamp.frame;++eye.stamp.pose;}
    dlss::SceneDepthWork cross_eye;
    Require(!transfer.Queue(mixed,cross_eye),"previous right eye was replayed as the next left eye");
    for(unsigned mode=0;mode<3;++mode) {
        dlss::SceneDepthTransfer stale;dlss::SceneDepthWork canceled;
        Require(stale.Queue(eyes,canceled),"cancellation fixture queue failed");
        if(!mode)stale.Invalidate();
        Require(stale.Poll(stamp,completed)==dlss::SceneDepthDelivery::pending,
            "invalidation granted early source release");
        canceled.color->producer_lease.Release();canceled.depth->producer_lease.Release();
        auto current=stamp;if(mode==1)++current.epoch;
        completed={};
        Require(stale.Poll(current,completed)==dlss::SceneDepthDelivery::discarded&&completed.frame==0,
            "stale worker completion was published");
        Require(stale.Poll(stamp,completed)==dlss::SceneDepthDelivery::idle,
            "older current identity resurrected a discarded result");
    }
}

void CheckFullColorPair(IDirect3DDevice9Ex* device,Consumer& consumer,UINT width,UINT height) {
    D3D9SampledDepthConfig config{width,height,D3DFMT_A16B16G16R16F,D3DFMT_D24X8,
        D3DMULTISAMPLE_NONE,0,kGeneration,true};
    auto invalid=config;invalid.native_color_format=D3DFMT_A8R8G8B8;
    Require(FAILED(D3D9SampledDepthCapture::CheckSupport(device,invalid)),
        "full color capture silently converted a non-FP16 native source");
    invalid=config;invalid.width=4096;invalid.height=2048;
    Require(FAILED(D3D9SampledDepthCapture::CheckSupport(device,invalid)),
        "paired storage was excluded from the capture memory bound");
    auto capture=std::make_unique<D3D9SampledDepthCapture>();
    Check(capture->Initialize(device,config),"initialize paired full-size capture");
    ComPtr<IDirect3DSurface9> original_color,original_depth,surface;
    Check(device->GetRenderTarget(0,&original_color),"retain paired original target");
    const auto depth_result=device->GetDepthStencilSurface(&original_depth);
    if(depth_result!=D3DERR_NOTFOUND)Check(depth_result,"retain paired original depth");
    ComPtr<IDirect3DTexture9> native,upload;
    Check(device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,config.native_color_format,
        D3DPOOL_DEFAULT,&native,nullptr),"private native color fixture");
    Check(native->GetSurfaceLevel(0,&surface),"paired native surface");
    Check(device->CreateTexture(width,height,1,0,config.native_color_format,
        D3DPOOL_SYSTEMMEM,&upload,nullptr),"native HDR upload");
    Check(device->SetRenderTarget(0,surface.Get()),"bind paired native color");
    Check(device->SetDepthStencilSurface(capture->native_depth_surface()),"bind paired depth");
    std::array<D3D9SampledDepthSnapshot,2> images;
    for(unsigned frame=0;frame<2;++frame) {
        D3DLOCKED_RECT locked{};Check(upload->LockRect(0,&locked,nullptr,0),"map native HDR");
        for(UINT y=0;y<height;++y) {
            auto* row=reinterpret_cast<std::uint16_t*>(static_cast<std::uint8_t*>(locked.pBits)+y*locked.Pitch);
            for(UINT x=0;x<width;++x)std::copy(kHdrColors[(x+y+frame)%2].begin(),
                kHdrColors[(x+y+frame)%2].end(),row+4*x);
        }
        Check(upload->UnlockRect(0),"unmap native HDR");
        Check(device->UpdateTexture(upload.Get(),native.Get()),"write native HDR source");
        Check(device->BeginScene(),"begin paired scene");
        const D3DVIEWPORT9 viewport{0,0,width,height,0,1};
        Check(device->SetViewport(&viewport),"paired viewport");
        Check(device->Clear(0,nullptr,D3DCLEAR_ZBUFFER,0,frame?.75F:.25F,0),"paired native depth");
        const auto state=NativeState::Read(device,1);
        Check(capture->Snapshot(frame+1,kGeneration,images[frame]),"snapshot paired native inputs");
        Require(!images[frame].shared_handle()&&!images[frame].color_shared_handle(),
            "paired handles escaped before producer completion");
        state.Equals(NativeState::Read(device,1));
        Check(device->EndScene(),"end paired scene");
        Ready(images[frame]);
        Require(images[frame].color_shared_handle(),"completed color/depth pair has no color transport");
        cojvr::backends::d3d9::D3D9SampledDepthPoint point;
        const auto readback_state=NativeState::Read(device,1);
        Check(images[frame].ReadDiagnosticPoint(point),"exact native center readback failed");
        readback_state.Equals(NativeState::Read(device,1));
        Require(point.color==kHdrColors[(width/2+height/2+frame)%2],"native center readback changed HDR values");
        const double point_z=(Half(point.depth[0])*65536+Half(point.depth[1])*256+Half(point.depth[2]))/16777215.;
        Require(std::abs(point_z-(frame?.75:.25))<1e-6,"native center depth readback changed");
    }
    Require(images[0].color_shared_handle()!=images[1].color_shared_handle(),"paired slots alias");
    D3D9SampledDepthSnapshot blocked;
    Require(capture->Snapshot(3,kGeneration,blocked)==kBusy&&!blocked.valid(),"paired lease overwritten");
    Check(device->SetDepthStencilSurface(nullptr),"detach paired depth before retirement");
    CheckPairTransfer(device,consumer,images);
    capture.reset(); // BOTH immutable planes outlive producer retirement.
    std::exception_ptr worker_error;
    std::thread worker([&] { try {
        ReadFullColorPair(consumer,images[0],0,.25);
        ReadFullColorPair(consumer,images[1],1,.75);
    } catch(...) { worker_error=std::current_exception(); } });
    worker.join(); // consumer query completed before producer-owner final release
    if(worker_error)std::rethrow_exception(worker_error);
    images={};
    Check(device->SetRenderTarget(0,original_color.Get()),"restore paired original target");
    Check(device->SetDepthStencilSurface(original_depth.Get()),"restore paired original depth");
}

void ReconstructNativeDepthPair(IDirect3DDevice9Ex* producer, Consumer& consumer,
                                const std::filesystem::path& helper) {
    namespace dlss = cojvr::backends::openvr::dlss;
    constexpr UINT extent = 256, output_extent = 384;
    constexpr D3DFORMAT format = D3DFMT_A16B16G16R16F;
    D3D9SampledDepthCapture depth;
    const D3D9SampledDepthConfig config{extent, extent, format, D3DFMT_D24X8,
        D3DMULTISAMPLE_NONE, 0, kGeneration, true};
    const auto initialized = depth.Initialize(producer, config);
    Check(initialized, depth.last_error());
    ComPtr<IDirect3DSurface9> original_color, original_depth;
    Check(producer->GetRenderTarget(0, &original_color), "retain original color");
    const auto original_depth_result = producer->GetDepthStencilSurface(&original_depth);
    if (original_depth_result != D3DERR_NOTFOUND) Check(original_depth_result, "retain original depth");
    std::array<ComPtr<IDirect3DTexture9>,2> native_colors;
    std::array<ComPtr<IDirect3DSurface9>,2> surfaces;
    std::array<ComPtr<ID3D11Texture2D>,2> colors;
    for (unsigned eye = 0; eye < 2; ++eye) {
        Check(producer->CreateTexture(extent, extent, 1, D3DUSAGE_RENDERTARGET,
            format, D3DPOOL_DEFAULT, &native_colors[eye], nullptr), "private native FP16 color");
        Check(native_colors[eye]->GetSurfaceLevel(0, &surfaces[eye]), "native color surface");
    }
    D3D11_TEXTURE2D_DESC mask_desc{};
    mask_desc.Width = mask_desc.Height = extent;
    mask_desc.ArraySize = mask_desc.MipLevels = mask_desc.SampleDesc.Count = 1;
    mask_desc.Format = DXGI_FORMAT_R8_UNORM;
    mask_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const std::vector<std::uint8_t> mask_pixels(extent * extent, 0);
    const D3D11_SUBRESOURCE_DATA mask_data{mask_pixels.data(), extent, 0};
    ComPtr<ID3D11Texture2D> mask;
    Check(consumer.device->CreateTexture2D(&mask_desc, &mask_data, &mask), "static fixture mask");
    dlss::PairSession session;
    if (!session.Start(consumer.device.Get(), helper)) throw std::runtime_error(std::string(session.last_error()));
    for (std::uint64_t frame = 1; frame <= 3; ++frame) {
        std::array<D3D9SampledDepthSnapshot,2> snapshots;
        std::array<ComPtr<ID3D11Texture2D>,2> packed_depth;
        dlss::PairFrame pair;
        pair.identity = {{reinterpret_cast<std::uintptr_t>(consumer.device.Get()), kGeneration,
            frame, frame, 1, extent, extent, output_extent, output_extent,
            static_cast<unsigned>(dlss::Quality::quality), true, true}, 1, 1, 1};
        for (unsigned eye = 0; eye < 2; ++eye) {
            Check(producer->SetDepthStencilSurface(nullptr), "detach preceding depth");
            Check(producer->SetRenderTarget(0, surfaces[eye].Get()), "bind native scene color");
            Check(producer->SetDepthStencilSurface(depth.native_depth_surface()), "bind sampleable native D24X8 depth");
            Check(producer->SetRenderState(D3DRS_STENCILENABLE, FALSE), "preserve D24X8 stencil contract");
            Check(producer->BeginScene(), "begin native scene");
            Check(producer->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                eye == 0 ? 0xff992233 : 0xff224499, 1.0F, 0), "clear native scene and far depth");
            const LONG offset = static_cast<LONG>(frame + eye);
            const D3DRECT geometry{40 + offset, 32, 180 + offset, 192};
            Check(producer->Clear(1, &geometry, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                eye == 0 ? 0xff33aa44 : 0xffaaaa33, eye == 0 ? .75F : .9999F, 0), "native scene depth pattern");
            const auto sequence = 2 * frame - 1 + eye;
            const auto captured = depth.Snapshot(sequence, kGeneration, snapshots[eye]);
            Check(captured, depth.last_error());
            Check(producer->EndScene(), "finish native eye");
            Ready(snapshots[eye]);
            Check(consumer.device->OpenSharedResource(snapshots[eye].color_shared_handle(),
                IID_PPV_ARGS(colors[eye].ReleaseAndGetAddressOf())), "open immutable scene color in D3D11");
            Check(consumer.device->OpenSharedResource(snapshots[eye].shared_handle(),
                IID_PPV_ARGS(&packed_depth[eye])), "open native packed depth in D3D11");
            auto& input = pair.eyes[eye];
            input.identity = pair.identity;
            input.input_token = sequence;
            input.eye = eye;
            input.unjittered_view_projection[12] = static_cast<float>(frame) / 128.0F;
            input.guides.color = colors[eye].Get();
            input.guides.linear_depth_alpha = packed_depth[eye].Get();
            input.guides.current_color_mask = mask.Get();
            input.guides.near_metres = .05F;
            input.guides.far_metres = 200.0F;
            input.guides.source_qualified = true; // Fixture owns all clears, textures and view transforms.
            input.guides.depth_encoding = dlss::DepthEncoding::packed_device_z_rgb24;
        }
        if (!session.Reconstruct(consumer.context.Get(), pair)) throw std::runtime_error(std::string(session.last_error()));
        const auto output = session.Outputs(pair);
        std::array<std::array<std::uint16_t,4>,2> pixels{};
        for (unsigned eye = 0; eye < 2; ++eye) {
            Require(output[eye] != nullptr, "incomplete real NGX pair");
            D3D11_TEXTURE2D_DESC desc{};
            output[eye]->GetDesc(&desc);
            Require(desc.Width == output_extent && desc.Height == output_extent, "real NGX output extent");
            desc.Width = desc.Height = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;
            Check(consumer.device->CreateTexture2D(&desc, nullptr, &staging), "sample NGX output");
            const D3D11_BOX centre{190,190,0,191,191,1};
            consumer.context->CopySubresourceRegion(staging.Get(),0,0,0,0,output[eye],0,&centre);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            Check(consumer.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped), "read NGX output");
            std::memcpy(pixels[eye].data(), mapped.pData, sizeof(pixels[eye]));
            consumer.context->Unmap(staging.Get(),0);
            for (unsigned channel = 0; channel < 3; ++channel)
                Require(std::isfinite(Half(pixels[eye][channel])), "nonfinite NGX output");
        }
        Require(pixels[0] != pixels[1], "NGX eyes lost independent image content");
    }
    Check(producer->SetDepthStencilSurface(nullptr), "detach fixture depth");
    Check(producer->SetRenderTarget(0, original_color.Get()), "restore original color");
    Check(producer->SetDepthStencilSurface(original_depth.Get()), "restore original depth");
    std::cout << "Real NGX DLSS Quality: D3D9Ex FP16 color and packed hardware Z -> D3D11 guides -> x64 helper -> 3 stereo pairs at 384x384\n";
}

int Run(const std::filesystem::path& helper) {
    static_assert(std::is_copy_constructible_v<D3D9SampledDepthSnapshot>);
    static_assert(!std::is_copy_constructible_v<D3D9SampledDepthCapture>);
    D3D9SampledDepthConfig config;
    config.width = kWidth; config.height = kHeight; config.generation = kGeneration;
    Require(FAILED(D3D9SampledDepthCapture::CheckSupport(nullptr, config)), "null device admitted");
    D3D9SampledDepthSnapshot empty;
    Require(!empty.valid() && !empty.shared_handle() && !empty.color_shared_handle() &&
        FAILED(empty.PollReady()), "empty lease admitted");
    const auto create = cojvr::backends::d3d9::SystemDirect3DCreate9Ex();
    Require(create && cojvr::backends::d3d9::IsExpectedSystemD3D9Module(), "expected system D3D9Ex unavailable");
    ComPtr<IDirect3D9Ex> api;
    Check(create(D3D_SDK_VERSION, &api), "create D3D9Ex");
    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE; present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = GetDesktopWindow(); present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.BackBufferWidth = kWidth; present.BackBufferHeight = kHeight;
    ComPtr<IDirect3DDevice9Ex> d;
    auto hr = api->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, present.hDeviceWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &present, nullptr, &d);
    if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_DEVICELOST) {
        std::cout << "SKIP: D3D9Ex HAL unavailable\n"; return 77;
    }
    Check(hr, "create D3D9Ex HAL");
    hr = D3D9SampledDepthCapture::CheckSupport(d.Get(), config);
    if (hr == D3DERR_NOTAVAILABLE) {
        std::cout << "SKIP: INTZ/D24S8/FP16 sharing capability unavailable\n"; return 77;
    }
    Check(hr, "sampled-depth capability");
    {
        auto pure_present=present;
        ComPtr<IDirect3DDevice9Ex> pure_device;
        Check(api->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,pure_present.hDeviceWindow,
            D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE,&pure_present,nullptr,&pure_device),
            "reproduce CoJ pure-device creation");
        D3DDEVICE_CREATION_PARAMETERS pure_creation{};
        Check(pure_device->GetCreationParameters(&pure_creation),"read pure-device creation flags");
        Require((pure_creation.BehaviorFlags&D3DCREATE_PUREDEVICE)!=0&&
            D3D9SampledDepthCapture::CheckSupport(pure_device.Get(),config)==D3DERR_NOTAVAILABLE,
            "the native pure-device request did not reproduce sampled-depth rejection");
        std::cout<<"Reproduced game flags 0x50: PUREDEVICE rejected with D3DERR_NOTAVAILABLE\n";
    }
    for(bool prefer_ex:{false,true})for(bool queries:{false,true}) {
        namespace backend=cojvr::backends::d3d9;
        constexpr DWORD requested=D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE|D3DCREATE_FPU_PRESERVE;
        const backend::FactoryHookCallbacks callbacks{
            .prefer_ex_device=prefer_ex,
            .before_create_device=[](IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*) noexcept {},
            .require_state_queries=queries};
        Require(backend::InstallFactoryVtableHook(api.Get(),callbacks),"install query-capable factory policy");
        auto trial_present=present;
        ComPtr<IDirect3DDevice9> created;
        const auto created_result=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,trial_present.hDeviceWindow,
            requested,&trial_present,&created);
        Check(created_result,"create with query policy");
        D3DDEVICE_CREATION_PARAMETERS actual{};
        Check(created->GetCreationParameters(&actual),"read effective query policy flags");
        const DWORD expected=queries?requested&~D3DCREATE_PUREDEVICE:requested;
        Require(actual.BehaviorFlags==expected,"query policy did not preserve all flags except opted-in PUREDEVICE");
        if(prefer_ex&&queries) {
            d.Reset();
            Check(created.As(&d),"query policy lost D3D9Ex identity");
            Check(D3D9SampledDepthCapture::CheckSupport(d.Get(),config),"query-capable game device still rejected depth");
        }
    }
    Require(cojvr::backends::d3d9::RestoreFactoryVtableHook(api.Get()),
        "query policy did not restore native factory");
    for (unsigned variant = 0; variant < 8; ++variant) {
        auto invalid = config;
        switch (variant) {
        case 0: invalid.native_depth_format = D3DFMT_D16; break;
        case 1: invalid.multisample = D3DMULTISAMPLE_2_SAMPLES; break;
        case 2: invalid.multisample_quality = 1; break;
        case 3: invalid.width = 0; break;
        case 4: invalid.height = D3D9SampledDepthCapture::max_extent + 1; break;
        case 5: invalid.generation = 0; break;
        case 6: invalid.width = invalid.height = D3D9SampledDepthCapture::max_extent; break;
        case 7: invalid.native_color_format = D3DFMT_D24S8; break;
        }
        Require(FAILED(D3D9SampledDepthCapture::CheckSupport(d.Get(), invalid)), "invalid format/MSAA/bounds admitted");
        D3D9SampledDepthCapture rejected;
        Require(FAILED(rejected.Initialize(d.Get(), invalid)) && !rejected.native_depth_surface(),
            "failed initialization exposed depth storage");
    }
    D3DCAPS9 caps{};
    Check(d->GetDeviceCaps(&caps), "read caps");
    const UINT mrt_count = caps.NumSimultaneousRTs < 4 ? caps.NumSimultaneousRTs : 4;
    auto capture = std::make_unique<D3D9SampledDepthCapture>();
    const auto initial = NativeState::Read(d.Get(), mrt_count);
    const auto initialized = capture->Initialize(d.Get(), config);
    Check(initialized, capture->last_error());
    initial.Equals(NativeState::Read(d.Get(), mrt_count));
    Require(capture->native_depth_surface() != nullptr, "own INTZ surface missing");
    Require(FAILED(capture->Initialize(d.Get(), config)), "live owner was replaced by Initialize");
    std::array<ComPtr<IDirect3DSurface9>, 4> targets;
    Check(d->SetDepthStencilSurface(nullptr), "detach initial DS");
    for (UINT i = 0; i < mrt_count; ++i) {
        Check(d->CreateRenderTarget(kWidth, kHeight, config.native_color_format,
            D3DMULTISAMPLE_NONE, 0, FALSE, &targets[i], nullptr), "create native MRT");
        Check(d->SetRenderTarget(i, targets[i].Get()), "bind native MRT");
    }
    ComPtr<IDirect3DSurface9> foreign_depth;
    Check(d->CreateDepthStencilSurface(kWidth, kHeight, D3DFMT_D24S8,
        D3DMULTISAMPLE_NONE, 0, FALSE, &foreign_depth, nullptr), "create unknown native DS");
    Check(d->SetDepthStencilSurface(foreign_depth.Get()), "bind unknown native DS");
    const auto unknown = NativeState::Read(d.Get(), mrt_count);
    Require(FAILED(capture->Snapshot(1, kGeneration, empty)) && !empty.valid(), "unknown native DS was automatically replaced");
    unknown.Equals(NativeState::Read(d.Get(), mrt_count));
    Check(d->SetDepthStencilSurface(capture->native_depth_surface()), "caller explicitly binds own INTZ before clear");
    HostileState hostile(d.Get());
    Consumer consumer(api.Get());
    Check(d->BeginScene(), "begin caller scene");
    ClearDepth(d.Get(), 0.25F);
    hostile.Bind(d.Get(), false);
    auto saved = NativeState::Read(d.Get(), mrt_count);
    D3D9SampledDepthSnapshot first, second;
    Check(capture->Snapshot(1, kGeneration, first), "snapshot native clear 0.25");
    Require(first.valid() && !first.shared_handle() && !first.color_shared_handle(), "unproven handle published");
    saved.Equals(NativeState::Read(d.Get(), mrt_count));
    Require(FAILED(capture->Snapshot(2, kGeneration + 1, empty)), "stale generation admitted");
    Require(FAILED(capture->Snapshot(1, kGeneration, empty)), "repeated sequence admitted");
    Require(FAILED(capture->Snapshot(2, kGeneration, first)) && first.sequence() == 1,
        "nonempty output lease was overwritten");
    ClearDepth(d.Get(), 0.75F);
    hostile.Bind(d.Get(), true);
    saved = NativeState::Read(d.Get(), mrt_count);
    Check(capture->Snapshot(2, kGeneration, second), "snapshot native clear 0.75");
    saved.Equals(NativeState::Read(d.Get(), mrt_count));
    Require(capture->Snapshot(3, kGeneration, empty) == kBusy && !empty.valid(), "bounded immutable leases were overwritten");
    Check(d->EndScene(), "end caller scene");
    Ready(first); Ready(second);
    cojvr::backends::d3d9::D3D9SampledDepthPreview disabled_preview{1,1,{123},{456}};
    Require(FAILED(first.ReadDiagnosticPreview(disabled_preview))&&disabled_preview.width==1&&
        disabled_preview.color_rgba16f.front()==123&&disabled_preview.depth_rgba16f.front()==456,
        "default depth capture exposed a preview or changed a rejected output");
    Require(first.shared_handle() != second.shared_handle(), "distinct captures alias one output");
    // First contents must still be .25 AFTER the source was cleared to .75.
    consumer.Read(first, 0.25); consumer.Read(second, 0.75);
    HRESULT wrong_poll = S_OK, wrong_capture = S_OK;
    IDirect3DSurface9* wrong_surface = reinterpret_cast<IDirect3DSurface9*>(1);
    std::thread outsider([&] {
        wrong_poll = first.PollReady();
        wrong_capture = capture->Snapshot(3, kGeneration, empty);
        wrong_surface = capture->native_depth_surface();
    });
    outsider.join();
    Require(FAILED(wrong_poll) && FAILED(wrong_capture) && !wrong_surface && !empty.valid(),
        "foreign producer thread admitted");
    auto retained_first = first;
    first = {};
    Check(d->BeginScene(), "begin busy scene");
    Require(capture->Snapshot(3, kGeneration, empty) == kBusy, "copy of immutable lease did not retain slot");
    Check(d->EndScene(), "end busy scene");
    retained_first = {}; second = {}; // both D3D11 copies have event completion proof

    // Reject an incompatible native RT without touching native ownership/state.
    for (UINT i = 1; i < mrt_count; ++i) Check(d->SetRenderTarget(i, nullptr), "detach MRT for mismatch fixture");
    ComPtr<IDirect3DSurface9> wrong_format;
    Check(d->CreateRenderTarget(kWidth, kHeight, D3DFMT_A16B16G16R16F,
        D3DMULTISAMPLE_NONE, 0, FALSE, &wrong_format, nullptr), "create wrong native color format");
    Check(d->SetRenderTarget(0, wrong_format.Get()), "bind wrong native color format");
    const auto mismatch = NativeState::Read(d.Get(), mrt_count);
    Require(FAILED(capture->Snapshot(3, kGeneration, empty)), "wrong native color format admitted");
    mismatch.Equals(NativeState::Read(d.Get(), mrt_count));
    for (UINT i = 0; i < mrt_count; ++i) Check(d->SetRenderTarget(i, targets[i].Get()), "restore fixture MRT");

    Check(d->BeginScene(), "begin far-Z scene");
    ClearDepth(d.Get(), 0.9999F);
    hostile.Bind(d.Get(), false);
    saved = NativeState::Read(d.Get(), mrt_count);
    Check(capture->Snapshot(3, kGeneration, first), "snapshot native clear .9999");
    saved.Equals(NativeState::Read(d.Get(), mrt_count));
    ClearDepth(d.Get(), 1.0F);
    hostile.Bind(d.Get(), true);
    saved = NativeState::Read(d.Get(), mrt_count);
    Check(capture->Snapshot(4, kGeneration, second), "snapshot native clear 1");
    saved.Equals(NativeState::Read(d.Get(), mrt_count));
    Check(d->EndScene(), "end far-Z scene");
    Ready(first); Ready(second);
    consumer.Read(first, static_cast<double>(0.9999F)); consumer.Read(second, 1.0);
    first = {}; second = {};

#if defined(COJVR_SAMPLED_DEPTH_FAULT_TESTING)
    // Every restore operation is independently failed. A rejected new capture
    // cannot recapture a partial preimage; RetryRestore must restore the original.
    for (unsigned operation = 0; operation < 2 * mrt_count + 5; ++operation) {
        Check(d->BeginScene(), "begin restore-fault scene");
        ClearDepth(d.Get(), 0.25F);
        hostile.Bind(d.Get(), operation % 2 == 0);
        saved = NativeState::Read(d.Get(), mrt_count);
        capture->FailNextRestoreForTest(operation);
        const auto sequence = 5ULL + operation;
        Require(FAILED(capture->Snapshot(sequence, kGeneration, empty)) && !empty.valid(), "failed restore published a capture");
        const auto partial = NativeState::Read(d.Get(), mrt_count);
        Require(FAILED(capture->Snapshot(sequence + 1, kGeneration, empty)), "unresolved restore admitted another capture");
        partial.Equals(NativeState::Read(d.Get(), mrt_count));
        Check(capture->RetryRestore(), "retry original native preimage");
        saved.Equals(NativeState::Read(d.Get(), mrt_count));
        Check(d->EndScene(), "end restore-fault scene");
        // Give the internal failed-draw/fault slots a bounded event drain through
        // fresh snapshots; no sleep-only assumption grants reuse.
        Wait([&] {
            Check(d->BeginScene(), "begin fault drain scene");
            const auto result = capture->Snapshot(sequence, kGeneration, empty);
            Check(d->EndScene(), "end fault drain scene");
            return result == kBusy ? S_FALSE : result;
        }, "fault slot did not retire");
        Ready(empty); consumer.Read(empty, 0.25); empty = {};
    }
#endif

    // A retained capture keeps the private producer allocation/device alive even
    // after the utility owner dies. The consumer never receives own INTZ itself.
    Check(d->BeginScene(), "begin lifetime scene");
    ClearDepth(d.Get(), 0.75F);
    hostile.Bind(d.Get(), false);
    Check(capture->Snapshot(100, kGeneration, first), "snapshot before producer owner destruction");
    Check(d->EndScene(), "end lifetime scene");
    Check(d->SetDepthStencilSurface(nullptr), "detach borrowed source before owner destruction");
    capture.reset();
    Ready(first);
    consumer.Read(first, 0.75);
    Require(!first.color_shared_handle(),"default-off capture allocated or exposed color");
    first = {};
    for (DWORD slot : kTextureSlots) Check(d->SetTexture(slot, nullptr), "detach native test textures");
    for (DWORD slot = 1; slot < mrt_count; ++slot)
        Check(d->SetRenderTarget(slot, nullptr), "detach MRT before scene boundary test");
    CheckFullColorPair(d.Get(),consumer,kWidth,kHeight);
    CheckFullColorPair(d.Get(),consumer,2560,1440);
    CheckCoJSceneBoundary(d.Get());
    CheckCoJSceneBoundary(d.Get(),2560,1440);
    CheckCoJSceneBoundary(d.Get(),2804,2860,FinalOpticsFixture::matching,false);
    CheckCoJSceneBoundary(d.Get(),128,72,FinalOpticsFixture::unavailable);
    CheckCoJSceneBoundary(d.Get(),128,72,FinalOpticsFixture::changed);
    if (!helper.empty()) ReconstructNativeDepthPair(d.Get(), consumer, helper);
    std::cout << "Own INTZ D24S8 device-Z -> shared RGBA16F RGB24 fixture passed: "
        ".25/.75/.9999/1, event proof, immutable bounded leases and native state restoration. "
        "No CoJ source/build/material/eye or headset qualification.\n";
    return 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc > 2) return 2;
        return Run(argc == 2 ? std::filesystem::absolute(argv[1]) : std::filesystem::path{});
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
