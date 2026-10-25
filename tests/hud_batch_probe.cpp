// Authored offline; not compiled or executed in the authoring session.
#include "backends/d3d9/hud_batch_probe.hpp"
#include "backends/d3d9/device_vtable_hook.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <type_traits>

using namespace cojvr::backends::d3d9;
namespace h = cojvr::backends::d3d9::hud;
namespace {
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
IDirect3DDevice9* Device() { return reinterpret_cast<IDirect3DDevice9*>(0x1234); }
h::Admission Qualified() { return {true, true, true, true, true}; }
h::Context Context() {
    h::Context c{};
    c.identity = {reinterpret_cast<std::uintptr_t>(Device()), 2, 7, 11,
        40, 9, GetCurrentThreadId()};
    c.identity.native_hook_epoch = 13;
    c.identity.permission_epoch = 17;
    c.boundary = {1, 0, 13, true, false};
    c.pass = 3; c.sprite_owner = 0x9010;
    return c;
}
unsigned reads = 0;
bool read_ok = true;
h::Snapshot Read(IDirect3DDevice9*) noexcept {
    ++reads;
    h::Snapshot s{}; s.valid = read_ok;
    s.rt0.object = 0x3010; s.rt0.bound = true; s.rt0.valid = read_ok;
    s.rt0.width = 640; s.rt0.height = 480;
    s.states.fill(42); s.state_valid.fill(read_ok);
    return s;
}
RenderResourceEvent Event() { return {RenderResourceOperation::draw_up, 4, S_OK, 0x80, 0, 0}; }
h::BoundaryWitness Witness(h::Stage stage, const h::Identity& identity) {
    return {stage == h::Stage::sprites || stage == h::Stage::sprites_end ? 1u : 0u,
        stage == h::Stage::flush_begin || stage == h::Stage::flush_end ? 1u : 0u,
        identity.native_hook_epoch, true, false};
}
void Draw(h::Collector& p, const h::Identity& identity, HRESULT result = S_OK,
          bool flushing = false, std::uint64_t pass = 3) {
    auto e = Event();
    h::DrawWitness witness{Witness(flushing ? h::Stage::flush_begin : h::Stage::sprites, identity),
        pass, flushing ? 0x9020u : 0x9010u, flushing ? 0xFFFFFFFFu : 0u};
    p.Call(Device(), true);
    p.Draw(Device(), identity, witness, e, true);
    e.result = result;
    p.Draw(Device(), identity, witness, e, false);
    p.Call(Device(), false);
}
void Finish(h::Collector& p, const h::Context& c) {
    p.Boundary(h::Stage::sprites_end, c.identity, Witness(h::Stage::sprites_end, c.identity), c.pass, c.sprite_owner);
    p.Boundary(h::Stage::flush_begin, c.identity, Witness(h::Stage::flush_begin, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
    p.Boundary(h::Stage::flush_end, c.identity, Witness(h::Stage::flush_end, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
}
h::Pass Copy(h::Collector& p, std::size_t index = 0) {
    h::Pass out{}; Check(p.CopyPass(index, out), "missing stored pass"); return out;
}

// Synthetic native vtable: no GPU/device/resource creation. Calls go through the
// production hook and original functions, including pre/post state distinction.
struct FakeDevice { void** vtable; };
unsigned forwards = 0, before = 0, after = 0, calls = 0;
bool forwarded = false;
HRESULT STDMETHODCALLTYPE NativeDraw(IDirect3DDevice9*, D3DPRIMITIVETYPE type,
                                     UINT start, UINT count) {
    Check(type == D3DPT_TRIANGLELIST && start == 7 && count == 2, "draw args changed");
    ++forwards; forwarded = true; SetLastError(0x7331); return E_FAIL;
}
HRESULT STDMETHODCALLTYPE NativeOther(IDirect3DDevice9*) { return S_OK; }
HRESULT STDMETHODCALLTYPE NativeQuery(IDirect3DDevice9*, REFIID, void** out) {
    if (out) *out = nullptr;
    return E_NOINTERFACE;
}
HRESULT STDMETHODCALLTYPE NativeIndexed(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, INT base,
    UINT min, UINT vertices, UINT start, UINT count) {
    Check(base == -2 && min == 1 && vertices == 8, "indexed args changed");
    return NativeDraw(d, t, start, count);
}
HRESULT STDMETHODCALLTYPE NativeUp(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, UINT count, const void* data, UINT stride) {
    Check(data == reinterpret_cast<void*>(0x1000) && stride == 24, "UP args changed");
    return NativeDraw(d, t, 7, count);
}
HRESULT STDMETHODCALLTYPE NativeIndexedUp(IDirect3DDevice9* d, D3DPRIMITIVETYPE t, UINT min,
    UINT vertices, UINT count, const void* indices, D3DFORMAT format, const void* data, UINT stride) {
    Check(min == 1 && vertices == 8 && indices == reinterpret_cast<void*>(0x2000) && format == D3DFMT_INDEX16,
        "indexed UP args changed");
    return NativeUp(d, t, count, data, stride);
}
void DrawObserver(IDirect3DDevice9*, const RenderResourceEvent& e, bool entering) noexcept {
    if (entering) { ++before; Check(!forwarded, "sample was after original draw"); }
    else { ++after; Check(forwarded && e.result == E_FAIL, "original HRESULT lost"); }
    SetLastError(0xBAD);
}
void CallObserver(IDirect3DDevice9*, bool entering) noexcept {
    if (entering) ++calls; else --calls;
    SetLastError(0xBAD);
}
void HookForwarding() {
    std::array<void*, 119> vtable{};
    vtable.fill(reinterpret_cast<void*>(&NativeOther));
    vtable[0] = reinterpret_cast<void*>(&NativeQuery);
    vtable[81] = reinterpret_cast<void*>(&NativeDraw);
    vtable[82] = reinterpret_cast<void*>(&NativeIndexed);
    vtable[83] = reinterpret_cast<void*>(&NativeUp);
    vtable[84] = reinterpret_cast<void*>(&NativeIndexedUp);
    FakeDevice fake{vtable.data()};
    auto* device = reinterpret_cast<IDirect3DDevice9*>(&fake);
    DeviceHookCallbacks cb{}; cb.hud_batch_draw = &DrawObserver; cb.hud_batch_call = &CallObserver;
    Check(InstallDeviceVtableHook(device, cb), "synthetic ingress install failed");
    Check(TryInspectHudBatchHookContinuity(device).intact, "installed ingress continuity was not witnessed");
    SetLastError(0x7111);
    Check(device->DrawPrimitive(D3DPT_TRIANGLELIST, 7, 2) == E_FAIL, "draw result changed");
    Check(forwards == 1 && before == 1 && after == 1 && !calls, "forwarding not exactly once");
    Check(GetLastError() == 0x7331, "observer changed native last-error");
    forwarded = false;
    Check(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, -2, 1, 8, 7, 2) == E_FAIL, "indexed result changed");
    forwarded = false;
    Check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, reinterpret_cast<void*>(0x1000), 24) == E_FAIL, "UP result changed");
    forwarded = false;
    Check(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 1, 8, 2, reinterpret_cast<void*>(0x2000),
        D3DFMT_INDEX16, reinterpret_cast<void*>(0x1000), 24) == E_FAIL, "indexed UP result changed");
    Check(forwards == 4 && before == 4 && after == 4 && !calls, "four draw ingresses lost calls");
    Check(RestoreAllDeviceVtableHooks(), "synthetic hooks failed restoration");
    Check(vtable[81] == reinterpret_cast<void*>(&NativeDraw), "original slot not restored");
}

// Values reader fixture: only the actual getter/release slots are populated;
// any accidental setter/draw/create/readback call cannot silently succeed.
struct FakeResource { void** vtable; unsigned references = 0; };
FakeResource rt{}, ds{}, texture{}, vs{}, ps{};
bool fail_query = false, unbound = false;
unsigned released = 0;
ULONG STDMETHODCALLTYPE ReleaseResource(FakeResource* r) { ++released; return --r->references; }
template<class T> HRESULT Give(FakeResource& r, T** out) {
    ++r.references; *out = reinterpret_cast<T*>(&r); return S_OK;
}
HRESULT STDMETHODCALLTYPE GetRt(IDirect3DDevice9*, DWORD slot, IDirect3DSurface9** out) {
    Check(slot == 0, "reader queried another RT"); return Give(rt, out);
}
HRESULT STDMETHODCALLTYPE GetDs(IDirect3DDevice9*, IDirect3DSurface9** out) {
    if (unbound) { *out = nullptr; return D3DERR_NOTFOUND; } return Give(ds, out);
}
HRESULT STDMETHODCALLTYPE GetTex(IDirect3DDevice9*, DWORD slot, IDirect3DBaseTexture9** out) {
    Check(slot == 0, "reader queried another texture");
    if (unbound) { *out = nullptr; return S_OK; } return Give(texture, out);
}
HRESULT STDMETHODCALLTYPE GetVs(IDirect3DDevice9*, IDirect3DVertexShader9** out) {
    if (unbound) { *out = nullptr; return S_OK; } return Give(vs, out);
}
HRESULT STDMETHODCALLTYPE GetPs(IDirect3DDevice9*, IDirect3DPixelShader9** out) {
    if (unbound) { *out = nullptr; return S_OK; } return Give(ps, out);
}
HRESULT STDMETHODCALLTYPE Desc(FakeResource*, D3DSURFACE_DESC* out) {
    *out = {}; out->Width = 640; out->Height = 480; out->Format = D3DFMT_A8R8G8B8;
    out->Type = D3DRTYPE_SURFACE; out->Pool = D3DPOOL_DEFAULT;
    return fail_query ? E_FAIL : S_OK;
}
D3DRESOURCETYPE STDMETHODCALLTYPE Type(FakeResource*) { return D3DRTYPE_TEXTURE; }
DWORD STDMETHODCALLTYPE Levels(FakeResource*) { return 3; }
HRESULT STDMETHODCALLTYPE LevelDesc(FakeResource* r, UINT level, D3DSURFACE_DESC* out) {
    Check(level == 0, "reader guessed another mip"); return Desc(r, out);
}
HRESULT STDMETHODCALLTYPE Function(FakeResource*, void* data, UINT* bytes) {
    Check(data == nullptr, "reader copied shader bytecode"); *bytes = 48; return S_OK;
}
HRESULT STDMETHODCALLTYPE Viewport(IDirect3DDevice9*, D3DVIEWPORT9* out) {
    *out = {2, 3, 620, 460, .125F, .875F}; return S_OK;
}
HRESULT STDMETHODCALLTYPE Scissor(IDirect3DDevice9*, RECT* out) { *out = {4, 5, 600, 450}; return S_OK; }
HRESULT STDMETHODCALLTYPE State(IDirect3DDevice9*, D3DRENDERSTATETYPE key, DWORD* value) {
    *value = static_cast<DWORD>(key) * 17 + 1;
    SetLastError(0xBAD);
    return fail_query && key == D3DRS_BLENDOPALPHA ? E_FAIL : S_OK;
}
void ReaderValues() {
    std::array<void*, 119> device_slots{};
    std::array<void*, 18> surface_slots{}, texture_slots{}, shader_slots{};
    surface_slots[2] = texture_slots[2] = shader_slots[2] = reinterpret_cast<void*>(&ReleaseResource);
    surface_slots[12] = reinterpret_cast<void*>(&Desc);
    texture_slots[10] = reinterpret_cast<void*>(&Type); texture_slots[13] = reinterpret_cast<void*>(&Levels);
    texture_slots[17] = reinterpret_cast<void*>(&LevelDesc); shader_slots[4] = reinterpret_cast<void*>(&Function);
    rt = {surface_slots.data()}; ds = {surface_slots.data()}; texture = {texture_slots.data()};
    vs = {shader_slots.data()}; ps = {shader_slots.data()};
    device_slots[38] = reinterpret_cast<void*>(&GetRt); device_slots[40] = reinterpret_cast<void*>(&GetDs);
    device_slots[64] = reinterpret_cast<void*>(&GetTex); device_slots[93] = reinterpret_cast<void*>(&GetVs);
    device_slots[108] = reinterpret_cast<void*>(&GetPs); device_slots[48] = reinterpret_cast<void*>(&Viewport);
    device_slots[76] = reinterpret_cast<void*>(&Scissor); device_slots[58] = reinterpret_cast<void*>(&State);
    FakeDevice fake{device_slots.data()}; auto* device = reinterpret_cast<IDirect3DDevice9*>(&fake);
    SetLastError(0x7111); const auto s = h::Observe(device);
    Check(s.valid && s.rt0.width == 640 && s.texture0.levels == 3 && s.vertex_shader.bytes == 48 &&
        s.viewport == std::array<DWORD, 4>{2, 3, 620, 460} && s.scissor == std::array<LONG, 4>{4, 5, 600, 450},
        "real getter scope lost descriptions");
    for (std::size_t i = 0; i < h::state_keys.size(); ++i)
        Check(s.state_valid[i] && s.states[i] == DWORD(h::state_keys[i]) * 17 + 1, "blend/depth key omitted or remapped");
    Check(released == 5 && !rt.references && !ds.references && !texture.references && !vs.references && !ps.references &&
        GetLastError() == 0x7111, "getter COM reference or last-error escaped");
    fail_query = true; Check(!h::Observe(device).valid, "failed query fabricated complete state"); fail_query = false;
    unbound = true; const auto empty = h::Observe(device); unbound = false;
    Check(empty.valid && !empty.depth.bound && !empty.texture0.bound && !empty.vertex_shader.bound &&
        !empty.pixel_shader.bound && !rt.references, "unbound was confused with query failure");
}
}
int main() {
    static_assert(std::is_trivially_copyable_v<h::Pass>);
    static_assert(h::Collector::max_passes == 6 && h::Collector::max_records == 64);
    Check(!h::Observe(nullptr).valid, "null query fabricated valid state");
    ReaderValues();
    auto a = Qualified(); a.enabled = false;
    Check(h::RejectHookup(a) == h::Rejection::disabled, "disabled admission lost");
    a = Qualified(); a.native_boundary_continuous = false; a.nesting_observable = false;
    Check(h::RejectHookup(a) == h::Rejection::native_boundary_witness_missing,
        "sampled natural callbacks were treated as complete witnesses");
    a = Qualified(); a.nesting_observable = false;
    Check(h::RejectHookup(a) == h::Rejection::nesting_witness_missing, "nesting inferred");
    a = Qualified(); a.exact_build = false;
    Check(h::RejectHookup(a) == h::Rejection::unknown_build, "unknown build admitted");
    a = Qualified(); a.draw_hooks_continuous = false;
    Check(h::RejectHookup(a) == h::Rejection::draw_continuity_missing, "foreign hooks admitted");

    {
        h::Collector p(&Read); const auto c = Context();
        Check(p.Begin(c, a) != h::Rejection::none && reads == 0, "rejected scope queried device");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "qualified scope rejected");
        Draw(p, c.identity); Draw(p, c.identity);
        p.Boundary(h::Stage::sprites_end, c.identity, Witness(h::Stage::sprites_end, c.identity), c.pass, c.sprite_owner);
        p.Boundary(h::Stage::flush_begin, c.identity, Witness(h::Stage::flush_begin, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
        Draw(p, c.identity, S_OK, true);
        p.Boundary(h::Stage::flush_end, c.identity, Witness(h::Stage::flush_end, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
        auto out = Copy(p);
        Check(out.complete && out.draws == 3 && out.records == 2 && out.values[0].repeat == 2,
            "phase-aware coalescing/completion lost draws");
        Check(out.context.identity.native_generation == 2 && out.context.identity.continuity_epoch == 7 &&
            out.context.identity.hook_epoch == 11 && out.context.native_caller == 0 &&
            out.context.route == h::Route::unknown, "identity/unknown namespaces invented");
        Check(out.flush_owner == 0x9020 && out.option == 0xFFFFFFFF, "distinct flush owner/option lost");
    }
    {
        h::Collector p(&Read); auto c = Context(); reads = 0;
        for (unsigned pass = 0; pass < 6; ++pass) {
            ++c.pass; Check(p.Begin(c, Qualified()) == h::Rejection::none, "six-pass budget lost");
            for (unsigned draw = 0; draw < 65; ++draw) Draw(p, c.identity, S_OK, false, c.pass);
            Finish(p, c); auto out = Copy(p, pass);
            Check(!out.complete && out.samples == 64 && out.draws == 65 &&
                (out.faults & h::truncated), "repeated draws bypassed hard sample bound");
        }
        Check(reads == 6 * 64 && p.Begin(c, Qualified()) == h::Rejection::budget_exhausted,
            "session/sample budget exceeded or replenished");
    }
    {
        h::Collector p(&Read); auto c = Context();
        for (unsigned i = 0; i < 6; ++i) {
            Check(p.Begin(c, Qualified()) == h::Rejection::none, "identity test Begin failed");
            auto changed = c.identity;
            if (i == 0) ++changed.device;
            if (i == 1) ++changed.native_generation;
            if (i == 2) ++changed.continuity_epoch;
            if (i == 3) ++changed.hook_epoch;
            if (i == 4) ++changed.frame;
            if (i == 5) ++changed.present;
            Draw(p, changed); Finish(p, c);
            Check(!Copy(p, i).complete && (Copy(p, i).faults & h::identity_changed),
                "changed identity retained completion");
        }
    }
    {
        h::Collector p(&Read); const auto c = Context();
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "partial Begin failed");
        read_ok = false; Draw(p, c.identity); read_ok = true; Finish(p, c);
        Check(!Copy(p).complete && (Copy(p).faults & h::read_failed), "partial query accepted");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "failed draw Begin failed");
        Draw(p, c.identity, E_FAIL); Finish(p, c);
        Check(!Copy(p, 1).complete && (Copy(p, 1).faults & h::draw_failed), "failed draw accepted");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "nested Begin failed");
        Check(p.Begin(c, Qualified()) == h::Rejection::busy, "nested begin overwrote pass");
        Finish(p, c); Check(!Copy(p, 2).complete && (Copy(p, 2).faults & h::nested), "nested pass accepted");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "unterminated Begin failed");
        Check(!Copy(p, 3).complete && (Copy(p, 3).faults & h::unterminated), "open pass complete");
        p.Abort(h::native_fault);
        Check(!Copy(p, 3).complete && (Copy(p, 3).faults & h::native_fault), "fault discarded");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "cross-thread Begin failed");
        std::thread foreign([&] { p.Call(Device(), true); p.Call(Device(), false); }); foreign.join();
        Finish(p, c); Check(!Copy(p, 4).complete && (Copy(p, 4).faults & h::identity_changed),
            "foreign thread ingress retained completion");
    }
    {
        h::Collector p(&Read); const auto c = Context();
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "ordering Begin failed");
        p.Boundary(h::Stage::flush_end, c.identity, Witness(h::Stage::flush_end, c.identity), c.pass, 0x9020, 0);
        Check(!Copy(p).complete && (Copy(p).faults & h::bad_order), "orphan flush accepted");
    }
    {
        h::Collector p(&Read); const auto c = Context();
        auto missing = c; missing.identity.permission_epoch = 0;
        Check(p.Begin(missing, Qualified()) == h::Rejection::invalid_identity, "missing cached-source epoch admitted");
        missing = c; missing.boundary = {};
        Check(p.Begin(missing, Qualified()) == h::Rejection::invalid_identity, "missing boundary witness admitted");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "epoch test Begin failed");
        auto changed = c.identity; ++changed.native_hook_epoch;
        Draw(p, changed); Finish(p, c);
        Check(!Copy(p).complete && (Copy(p).faults & h::identity_changed), "native hook epoch changed unnoticed");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "permission epoch test Begin failed");
        changed = c.identity; ++changed.permission_epoch;
        Draw(p, changed); Finish(p, c);
        Check(!Copy(p, 1).complete && (Copy(p, 1).faults & h::identity_changed), "cached permission source changed unnoticed");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "depth witness Begin failed");
        auto depth = Witness(h::Stage::sprites_end, c.identity); ++depth.sprite_depth;
        p.Boundary(h::Stage::sprites_end, c.identity, depth, c.pass, c.sprite_owner);
        const auto invalid = Copy(p, 2);
        Check(!invalid.complete && (invalid.faults & h::nested) && invalid.sprite_end.sprite_depth == 2,
            "nested depth was accepted or discarded");
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "empty pass Begin failed");
        Finish(p, c);
        Check(!Copy(p, 3).complete && (Copy(p, 3).faults & h::missing_flush_draw), "no-fault empty scope claimed success");
    }
    {
        h::Collector p(&Read); const auto c = Context();
        Check(p.Begin(c, Qualified()) == h::Rejection::none, "unique budget Begin failed");
        p.Boundary(h::Stage::sprites_end, c.identity, Witness(h::Stage::sprites_end, c.identity), c.pass, c.sprite_owner);
        p.Boundary(h::Stage::flush_begin, c.identity, Witness(h::Stage::flush_begin, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
        h::DrawWitness witness{Witness(h::Stage::flush_begin, c.identity), c.pass, 0x9020, 0xFFFFFFFF};
        for (unsigned i = 0; i < 65; ++i) {
            auto e = Event(); e.selector = i;
            p.Call(Device(), true); p.Draw(Device(), c.identity, witness, e, true);
            p.Draw(Device(), c.identity, witness, e, false); p.Call(Device(), false);
        }
        p.Boundary(h::Stage::flush_end, c.identity, Witness(h::Stage::flush_end, c.identity), c.pass, 0x9020, 0xFFFFFFFF);
        const auto out = Copy(p);
        Check(!out.complete && out.records == 64 && out.samples == 64 && out.flush_draws == 65 &&
            (out.faults & h::truncated), "unique records exceeded fixed buffer");
    }
    HookForwarding();
}
