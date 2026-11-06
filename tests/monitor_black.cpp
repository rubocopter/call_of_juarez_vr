#include "backends/d3d9/monitor_black.hpp"
#include "backends/d3d9/system_d3d9.hpp"
#include <wrl/client.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>
using namespace cojvr::backends::d3d9;
using Microsoft::WRL::ComPtr;
namespace {
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void Hr(HRESULT hr, const char* message) { Check(SUCCEEDED(hr), message); }
using FillFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, D3DCOLOR);
FillFn original_fill = nullptr;
HRESULT STDMETHODCALLTYPE PartialFailedFill(IDirect3DDevice9* device, IDirect3DSurface9* target,
    const RECT*, D3DCOLOR) {
    const RECT partial{0, 0, 2, 2};
    Hr(original_fill(device, target, &partial, 0xffeeeeee), "inject partially written failed fill");
    return E_FAIL;
}
HRESULT STDMETHODCALLTYPE FailedCopy(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*,
    IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE) { return E_FAIL; }
HRESULT STDMETHODCALLTYPE FailedAllocation(IDirect3DDevice9*, UINT, UINT, D3DFORMAT,
    D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*) { return E_OUTOFMEMORY; }
HRESULT STDMETHODCALLTYPE FailedPresent(IDirect3DDevice9*, const RECT*,const RECT*,HWND,const RGNDATA*) {
    return D3DERR_DEVICELOST;
}
struct ReplacedCall {
    void** slot; void* original;
    ReplacedCall(IDirect3DDevice9* device, unsigned index, void* replacement)
        : slot(*reinterpret_cast<void***>(device) + index) {
        DWORD protection = 0, ignored = 0;
        Check(VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection) != FALSE, "unprotect test slot");
        original = InterlockedExchangePointer(slot, replacement);
        Check(VirtualProtect(slot, sizeof(void*), protection, &ignored) != FALSE, "protect test slot");
    }
    ~ReplacedCall() {
        DWORD protection = 0, ignored = 0;
        Check(VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection) != FALSE, "unprotect original slot");
        InterlockedExchangePointer(slot, original);
        Check(VirtualProtect(slot, sizeof(void*), protection, &ignored) != FALSE, "restore original slot");
    }
};
void Policy() {
    MonitorBlackPolicy p;
    auto allow = [&](std::uint64_t now, bool capture = true, bool running = true,
        bool tracking = true, std::uint64_t submits = 1, std::uint64_t failures = 0,
        std::uintptr_t device = 100, std::uint64_t generation = 1) {
        return p.Allow(device, generation, now, capture, running, tracking, submits, failures);
    };
    Check(!allow(1000), "historical submissions cannot establish live progress");
    Check(allow(1010, true, true, true, 2), "fresh capture and progressing compositor permit black");
    Check(allow(1259, true, true, true, 2), "short compositor repeat remains usable");
    Check(!allow(1260, true, true, true, 2), "250ms stalled compositor restores desktop");
    Check(!allow(1270, false, true, true, 3), "failed capture restores desktop despite fresh submissions");
    Check(!allow(1280, true, false, true, 4), "stopped runtime restores desktop");
    Check(!allow(1290, true, true, false, 5), "invalid tracking restores desktop");
    Check(!allow(1300, true, true, true, 6), "reacquisition requires new submission progress");
    Check(allow(1310, true, true, true, 7), "recovered compositor can blank again");
    Check(!allow(1320, true, true, true, 8, 1), "new submit failure restores desktop");
    Check(allow(1330, true, true, true, 9, 1), "new successful progress recovers after a failure");
    Check(!allow(1340, true, true, true, 10, 1, 100, 2), "reset cannot inherit submission lease");
    Check(allow(1350, true, true, true, 11, 1, 100, 2), "new generation needs fresh progress");
    Check(!allow(1360, true, true, true, 12, 1, 200, 2), "replacement device cannot inherit submission lease");
    Check(!allow(1350, true, true, true, 13, 1, 200, 2), "reversed clock restores desktop");
    Check(!allow(1370, true, true, true, 0, 0, 200, 2), "restarted counters cannot borrow old progress");
    p.FlatCaptured(100, 1, 2000);
    Check(p.FlatFresh(100, 1, 2249), "throttled theater capture retains bounded lease");
    Check(!p.FlatFresh(100, 1, 2250), "theater capture expires without renewal");
    Check(!p.FlatFresh(200, 1, 2010) && !p.FlatFresh(100, 2, 2010), "theater lease checks device and generation");
    Check(!p.FlatFresh(100, 1, 1999), "theater lease rejects reversed time");
    p.FlatCaptured(0, 0, 0);
    Check(!p.FlatFresh(100, 1, 2010), "failed capture withdraws theater lease");
    std::cout << "PASS: monitor readiness, capture lease, recovery and identity\n";
}
std::uint32_t Pixel(IDirect3DDevice9* d, IDirect3DSurface9* source) {
    D3DSURFACE_DESC desc{}; Hr(source->GetDesc(&desc), "read description");
    ComPtr<IDirect3DSurface9> cpu;
    Hr(d->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
        D3DPOOL_SYSTEMMEM, &cpu, nullptr), "readback allocation");
    Hr(d->GetRenderTargetData(source, cpu.Get()), "readback");
    D3DLOCKED_RECT lock{}; Hr(cpu->LockRect(&lock, nullptr, D3DLOCK_READONLY), "lock readback");
    // Check every pixel, including outside the native viewport/scissor.
    const auto first = *static_cast<const std::uint32_t*>(lock.pBits) & 0x00ffffff;
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch);
        for (UINT x = 0; x < desc.Width; ++x)
            Check((row[x] & 0x00ffffff) == first, "surface must be uniformly filled");
    }
    Hr(cpu->UnlockRect(), "unlock readback"); return first;
}
void Gpu(bool ex, D3DSWAPEFFECT swap = D3DSWAPEFFECT_DISCARD) {
    HWND window = CreateWindowExW(0, L"STATIC", L"monitor black fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 96, 96, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create hidden fixture window");
    ComPtr<IDirect3D9> factory; ComPtr<IDirect3D9Ex> factory_ex;
    if (ex) {
        const auto create=SystemDirect3DCreate9Ex();Check(create!=nullptr,"system D3D9Ex entry");
        Hr(create(D3D_SDK_VERSION, &factory_ex), "create D3D9Ex"); factory = factory_ex;
    } else {
        const auto create=SystemDirect3DCreate9();Check(create!=nullptr,"system D3D9 entry");
        factory.Attach(create(D3D_SDK_VERSION));
    }
    Check(factory != nullptr, "create D3D9");
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.hDeviceWindow = window; pp.SwapEffect = swap;
    pp.BackBufferWidth = pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9> d; ComPtr<IDirect3DDevice9Ex> dx;
    if (ex) {
        Hr(factory_ex->CreateDeviceEx(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp, nullptr, &dx), "create Ex device"); d = dx;
    } else Hr(factory->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp, &d), "create classic device");
    ComPtr<IDirect3DSurface9> bb, eye, captured, depth;
    Hr(d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb), "get desktop");
    Hr(d->CreateRenderTarget(64, 64, pp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &eye, nullptr), "create bound eye");
    Hr(d->CreateRenderTarget(64, 64, pp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &captured, nullptr), "create captured image");
    Hr(d->CreateDepthStencilSurface(64, 64, D3DFMT_D16, D3DMULTISAMPLE_NONE, 0, TRUE, &depth, nullptr), "create depth");
    Hr(d->ColorFill(bb.Get(), nullptr, 0xff2468ac), "natural desktop color");
    Hr(d->ColorFill(eye.Get(), nullptr, 0xff884422), "native eye color");
    // Capturing BEFORE black is essential: reversing these calls loses visor content.
    Hr(d->StretchRect(bb.Get(), nullptr, captured.Get(), nullptr, D3DTEXF_NONE), "capture final natural content");
    Hr(d->SetRenderTarget(0, eye.Get()), "bind engine eye RT");
    Hr(d->SetDepthStencilSurface(depth.Get()), "bind native depth");
    const D3DVIEWPORT9 vp{3, 4, 8, 9, .2f, .8f}; const RECT scissor{5, 6, 7, 8};
    Hr(d->SetViewport(&vp), "set native viewport"); Hr(d->SetScissorRect(&scissor), "set scissor");
    Hr(d->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE), "enable native scissor");
    Hr(d->SetRenderState(D3DRS_COLORWRITEENABLE, 0), "disable native color writes");
    MonitorBlackSurface mirror;
    const auto black = mirror.Begin(d.Get(), 1);
    Check(black == (swap == D3DSWAPEFFECT_DISCARD ? S_OK : S_FALSE), "blank only disposable primary chain");
    Check(Pixel(d.Get(), bb.Get()) == (swap == D3DSWAPEFFECT_DISCARD ? 0U : 0x2468acU), "desktop black or native fallback");
    Check(Pixel(d.Get(), captured.Get()) == 0x2468ac, "visor capture survives black output");
    Check(Pixel(d.Get(), eye.Get()) == 0x884422, "bound native eye is never cleared");
    ComPtr<IDirect3DSurface9> rt, ds; D3DVIEWPORT9 actual_vp{}; RECT actual_scissor{}; DWORD mask = 1, enabled = 0;
    Hr(d->GetRenderTarget(0, &rt), "read RT"); Hr(d->GetDepthStencilSurface(&ds), "read depth");
    Hr(d->GetViewport(&actual_vp), "read viewport"); Hr(d->GetScissorRect(&actual_scissor), "read scissor");
    Hr(d->GetRenderState(D3DRS_COLORWRITEENABLE, &mask), "read color mask");
    Hr(d->GetRenderState(D3DRS_SCISSORTESTENABLE, &enabled), "read scissor enable");
    Check(rt == eye && ds == depth && mask == 0 && enabled == TRUE &&
        std::memcmp(&vp, &actual_vp, sizeof(vp)) == 0 && std::memcmp(&scissor, &actual_scissor, sizeof(scissor)) == 0,
        "native RT/depth/viewport/scissor/write mask preserved");
    Hr(d->Present(nullptr, nullptr, nullptr, nullptr), "original native Present still runs");
    Hr(mirror.End(d.Get(), 1), "restore actual preimage after Present");
    Hr(d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, bb.ReleaseAndGetAddressOf()), "get post-Present desktop");
    Check(Pixel(d.Get(), bb.Get()) == 0x2468ac, "Present must restore actual natural pixels for partial redraw/fallback");
    const RECT partial{0, 0, 1, 1};
    Hr(d->ColorFill(bb.Get(), &partial, 0xffeeeeee), "next frame only redraws one pixel");
    ComPtr<IDirect3DSurface9> partial_copy;
    Hr(d->CreateRenderTarget(63, 63, pp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &partial_copy, nullptr), "partial next-frame capture");
    const RECT untouched{1, 1, 64, 64};
    Hr(d->StretchRect(bb.Get(), &untouched, partial_copy.Get(), nullptr, D3DTEXF_NONE), "capture untouched native pixels");
    Check(Pixel(d.Get(), partial_copy.Get()) == 0x2468ac, "next natural fallback capture must not inherit desktop black");
    partial_copy.Reset();
    mirror.Reset();
    if (swap == D3DSWAPEFFECT_DISCARD) {
        Hr(d->ColorFill(bb.Get(), nullptr, 0xff2468ac), "reset native frame before faults");
        {
            ReplacedCall allocation(d.Get(), 28, reinterpret_cast<void*>(&FailedAllocation));
            Check(FAILED(mirror.Begin(d.Get(), 1)), "allocation failure rejects black before writing desktop");
        }
        Check(Pixel(d.Get(), bb.Get()) == 0x2468ac, "allocation failure keeps usable native desktop");
        mirror.Reset();
        {
            ReplacedCall copy(d.Get(), 34, reinterpret_cast<void*>(&FailedCopy));
            Check(FAILED(mirror.Begin(d.Get(), 1)), "backup failure rejects black");
        }
        Check(Pixel(d.Get(), bb.Get()) == 0x2468ac, "failed backup never clears desktop");
        mirror.Reset();
        {
            ReplacedCall fill(d.Get(), 35, reinterpret_cast<void*>(&PartialFailedFill));
            original_fill = reinterpret_cast<FillFn>(fill.original);
            Check(FAILED(mirror.Begin(d.Get(), 1)), "failed partial fill must roll back");
        }
        Check(Pixel(d.Get(), bb.Get()) == 0x2468ac, "failed fill restores actual preimage before Present");
        Check(mirror.Begin(d.Get(), 1) == S_FALSE, "fill fault disables retry until reset");
        mirror.Reset();
        Check(mirror.Begin(d.Get(), 1) == S_OK, "begin restoration failure case");
        {
            ReplacedCall copy(d.Get(), 34, reinterpret_cast<void*>(&FailedCopy));
            Check(FAILED(mirror.End(d.Get(), 1)), "restoration failure is reported");
        }
        Hr(d->ColorFill(bb.Get(), nullptr, 0xff2468ac), "native redraw after driver restoration failure");
        Check(mirror.Begin(d.Get(), 1) == S_FALSE && mirror.End(d.Get(), 1) == S_FALSE,
            "restoration fault disables further writes and stale retries");
        Check(Pixel(d.Get(), bb.Get()) == 0x2468ac, "faulted scope cannot overwrite later native redraw");
        mirror.Reset();
        Check(mirror.Begin(d.Get(), 1) == S_OK, "begin generation mismatch case");
        Hr(d->ColorFill(bb.Get(), nullptr, 0xffabcdef), "new generation owns new pixels");
        Check(FAILED(mirror.End(d.Get(), 2)), "generation mismatch rejects old preimage");
        Check(Pixel(d.Get(), bb.Get()) == 0xabcdef, "old generation never restores into new frame");
        mirror.Reset();
        Check(mirror.Begin(d.Get(), 2) == S_OK, "begin failed native Present case");
        {
            // DISCARD drivers may ignore source rectangles. Inject an actual
            // device-lost result instead of relying on undefined invalid args.
            ReplacedCall present(d.Get(),17,reinterpret_cast<void*>(&FailedPresent));
            Check(d->Present(nullptr,nullptr,nullptr,nullptr)==D3DERR_DEVICELOST,
                "fixture native Present fails");
        }
        Hr(mirror.End(d.Get(), 2), "failed native Present still restores saved pixels");
        Check(Pixel(d.Get(), bb.Get()) == 0xabcdef, "failed Present preserves actual preimage");
        mirror.Reset();
    }
    // No retained DEFAULT-pool resources may prevent reset.
    rt.Reset(); ds.Reset(); eye.Reset(); captured.Reset(); depth.Reset(); bb.Reset();
    Hr(d->SetDepthStencilSurface(nullptr), "release fixture depth");
    Hr(d->Reset(&pp), "reset after scoped black output");
    std::cout << "PASS: " << (ex ? "D3D9Ex" : "D3D9")
        << " pixels, capture ordering, partial redraw, state, failure rollback, Present/reset; swap=" << swap << '\n';
    d.Reset(); dx.Reset(); factory.Reset(); factory_ex.Reset(); DestroyWindow(window);
}
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--gpu") {
        Gpu(false); Gpu(true); Gpu(false, D3DSWAPEFFECT_COPY);
    } else Policy();
}
