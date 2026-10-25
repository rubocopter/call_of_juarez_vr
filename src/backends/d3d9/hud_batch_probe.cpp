#include "backends/d3d9/hud_batch_probe.hpp"
#include <windows.h>
#include <wrl/client.h>

namespace cojvr::backends::d3d9::hud {
namespace {
using Microsoft::WRL::ComPtr;
struct LastErrorScope {
    DWORD value = GetLastError();
    ~LastErrorScope() { SetLastError(value); }
};
template<class T> std::uintptr_t Address(T* object) noexcept {
    return reinterpret_cast<std::uintptr_t>(object);
}
bool IsDraw(RenderResourceOperation op) noexcept {
    return op == RenderResourceOperation::draw || op == RenderResourceOperation::draw_indexed ||
        op == RenderResourceOperation::draw_up || op == RenderResourceOperation::draw_indexed_up;
}
D3D9RenderResourceInfo Surface(IDirect3DSurface9* surface, HRESULT hr) noexcept {
    D3D9RenderResourceInfo out{};
    // Getter interface addresses are borrowed observations, not canonical
    // IUnknown/parent identities or a lifetime/aliasing proof.
    out.object = Address(surface); out.surface = out.object; out.bound = surface != nullptr;
    if (!surface) { out.valid = SUCCEEDED(hr) || hr == D3DERR_NOTFOUND; return out; }
    D3DSURFACE_DESC desc{};
    out.valid = SUCCEEDED(hr) && SUCCEEDED(surface->GetDesc(&desc));
    out.width = desc.Width; out.height = desc.Height; out.format = desc.Format;
    out.usage = desc.Usage; out.pool = desc.Pool; out.type = desc.Type;
    out.samples = desc.MultiSampleType; out.quality = desc.MultiSampleQuality;
    return out;
}
D3D9RenderResourceInfo Texture(IDirect3DBaseTexture9* texture, HRESULT hr) noexcept {
    D3D9RenderResourceInfo out{};
    out.object = Address(texture); out.bound = texture != nullptr;
    if (!texture) { out.valid = SUCCEEDED(hr); return out; }
    out.type = texture->GetType(); out.levels = texture->GetLevelCount();
    D3DSURFACE_DESC desc{};
    if (out.type == D3DRTYPE_TEXTURE)
        out.valid = SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0, &desc));
    else if (out.type == D3DRTYPE_CUBETEXTURE)
        out.valid = SUCCEEDED(static_cast<IDirect3DCubeTexture9*>(texture)->GetLevelDesc(0, &desc));
    else if (out.type == D3DRTYPE_VOLUMETEXTURE) {
        D3DVOLUME_DESC volume{};
        out.valid = SUCCEEDED(hr) &&
            SUCCEEDED(static_cast<IDirect3DVolumeTexture9*>(texture)->GetLevelDesc(0, &volume));
        out.width = volume.Width; out.height = volume.Height; out.depth = volume.Depth;
        out.format = volume.Format; out.usage = volume.Usage; out.pool = volume.Pool;
        return out;
    }
    out.valid = out.valid && SUCCEEDED(hr);
    out.width = desc.Width; out.height = desc.Height; out.format = desc.Format;
    out.usage = desc.Usage; out.pool = desc.Pool;
    return out;
}
template<class T> ShaderInfo Shader(T* shader, HRESULT hr) noexcept {
    ShaderInfo out{}; out.object = Address(shader); out.bound = shader != nullptr;
    out.valid = SUCCEEDED(hr);
    if (shader) {
        UINT bytes = 0;
        out.valid = SUCCEEDED(shader->GetFunction(nullptr, &bytes)) && out.valid;
        out.bytes = bytes;
    }
    return out;
}
}
Rejection RejectHookup(const Admission& a) noexcept {
    if (!a.enabled) return Rejection::disabled;
    if (!a.exact_build) return Rejection::unknown_build;
    if (!a.native_boundary_continuous) return Rejection::native_boundary_witness_missing;
    if (!a.nesting_observable) return Rejection::nesting_witness_missing;
    if (!a.draw_hooks_continuous) return Rejection::draw_continuity_missing;
    return Rejection::none;
}
const char* RejectionName(Rejection r) noexcept {
    switch (r) {
    case Rejection::none: return "none";
    case Rejection::disabled: return "disabled";
    case Rejection::unknown_build: return "unknown_build";
    case Rejection::native_boundary_witness_missing: return "native_boundary_continuity_unavailable";
    case Rejection::nesting_witness_missing: return "native_boundary_nesting_unobservable";
    case Rejection::draw_continuity_missing: return "draw_hook_continuity_unavailable";
    case Rejection::invalid_identity: return "invalid_identity";
    case Rejection::budget_exhausted: return "session_budget_exhausted";
    case Rejection::busy: return "busy_or_nested";
    }
    return "unknown";
}
Snapshot Observe(IDirect3DDevice9* device) noexcept {
    LastErrorScope error;
    Snapshot out{}; if (!device) return out;
    // Every getter reference is released synchronously before forwarding the
    // native draw. No GetContainer/QueryInterface walk or resource is retained.
    {
        ComPtr<IDirect3DSurface9> rt, depth;
        const auto rt_hr = device->GetRenderTarget(0, &rt);
        const auto ds_hr = device->GetDepthStencilSurface(&depth);
        out.rt0 = Surface(rt.Get(), rt_hr); out.depth = Surface(depth.Get(), ds_hr);
        ComPtr<IDirect3DBaseTexture9> texture;
        const auto tex_hr = device->GetTexture(0, &texture);
        out.texture0 = Texture(texture.Get(), tex_hr);
        ComPtr<IDirect3DVertexShader9> vertex; ComPtr<IDirect3DPixelShader9> pixel;
        const auto vs_hr = device->GetVertexShader(&vertex);
        const auto ps_hr = device->GetPixelShader(&pixel);
        out.vertex_shader = Shader(vertex.Get(), vs_hr); out.pixel_shader = Shader(pixel.Get(), ps_hr);
    }
    D3DVIEWPORT9 viewport{}; RECT scissor{};
    out.viewport_valid = SUCCEEDED(device->GetViewport(&viewport));
    out.viewport = {viewport.X, viewport.Y, viewport.Width, viewport.Height};
    out.viewport_depth = {viewport.MinZ, viewport.MaxZ};
    out.scissor_valid = SUCCEEDED(device->GetScissorRect(&scissor));
    out.scissor = {scissor.left, scissor.top, scissor.right, scissor.bottom};
    out.valid = out.rt0.valid && out.rt0.bound && out.depth.valid && out.texture0.valid &&
        out.vertex_shader.valid && out.pixel_shader.valid && out.viewport_valid && out.scissor_valid;
    for (std::size_t i = 0; i < state_keys.size(); ++i) {
        out.state_valid[i] = SUCCEEDED(device->GetRenderState(state_keys[i], &out.states[i]));
        out.valid = out.valid && out.state_valid[i];
    }
    return out;
}

Rejection Collector::Begin(const Context& c, const Admission& admission) noexcept {
    LastErrorScope error;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended); return Rejection::busy; }
    if (active_) { active_->faults |= nested; return Rejection::busy; }
    const auto rejection = RejectHookup(admission);
    if (rejection != Rejection::none) return rejection;
    const auto& id = c.identity;
    if (!reader_ || !id.device || !id.native_generation || !id.continuity_epoch || !id.hook_epoch ||
        !id.frame || !id.present || id.thread != GetCurrentThreadId() || !c.pass || !c.sprite_owner ||
        !id.native_hook_epoch || !id.permission_epoch || c.boundary.hook_epoch != id.native_hook_epoch ||
        !c.boundary.intact || c.boundary.faulted || c.boundary.sprite_depth != 1 || c.boundary.flush_depth != 0 ||
        c.native_caller != 0 || c.route != Route::unknown) return Rejection::invalid_identity;
    if (in_flight_.load()) return Rejection::busy;
    if (selected_ == max_passes) return Rejection::budget_exhausted;
    active_ = &passes_[selected_++]; active_->context = c;
    stage_ = Stage::sprites; draw_pending_ = pending_sampled_ = false;
    return Rejection::none;
}
bool Collector::Match(const Identity& id) noexcept {
    if (active_ && (active_->context.identity != id || id.thread != GetCurrentThreadId())) {
        active_->faults |= identity_changed; return false;
    }
    return active_ != nullptr;
}
void Collector::Close(std::uint32_t faults) noexcept {
    if (!active_) return;
    active_->faults |= faults | asynchronous_faults_.load();
    if (draw_pending_ || in_flight_.load()) active_->faults |= unterminated;
    if (stage_ == Stage::flush_end && !active_->flush_draws) active_->faults |= missing_flush_draw;
    active_->closed = true;
    active_->complete = !active_->faults && active_->draws != 0 && stage_ == Stage::flush_end;
    active_ = nullptr; draw_pending_ = pending_sampled_ = false;
}
void Collector::Boundary(Stage stage, const Identity& id, const BoundaryWitness& witness, std::uint64_t pass,
                         std::uintptr_t owner, std::uint32_t option) noexcept {
    LastErrorScope error;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended); return; }
    if (!active_) return;
    if (stage == Stage::sprites_end) { active_->sprite_end = witness; active_->sprite_end_owner = owner; }
    if (stage == Stage::flush_begin) active_->flush_begin = witness;
    if (stage == Stage::flush_end) {
        active_->flush_end = witness; active_->flush_end_owner = owner; active_->flush_end_option = option;
    }
    if (!Match(id) || pass != active_->context.pass || !owner) {
        Close(identity_changed); return;
    }
    if (!witness.intact || witness.hook_epoch != active_->context.identity.native_hook_epoch) {
        Close(identity_changed); return;
    }
    const bool depths_match = stage == Stage::sprites_end
        ? witness.sprite_depth == 1 && witness.flush_depth == 0
        : witness.sprite_depth == 0 && witness.flush_depth == 1;
    if (!depths_match || witness.faulted) { Close(nested | native_fault); return; }
    if (draw_pending_ || in_flight_.load()) { Close(nested); return; }
    if (stage == Stage::sprites_end && stage_ == Stage::sprites && owner == active_->context.sprite_owner)
        stage_ = stage;
    else if (stage == Stage::flush_begin && stage_ == Stage::sprites_end) {
        active_->flush_owner = owner; active_->option = option; stage_ = stage;
    } else if (stage == Stage::flush_end && stage_ == Stage::flush_begin &&
               owner == active_->flush_owner && option == active_->option) {
        stage_ = stage; Close();
    } else Close(bad_order);
}
void Collector::Call(IDirect3DDevice9* device, bool entering) noexcept {
    LastErrorScope error;
    const auto previous = entering ? in_flight_.fetch_add(1) : in_flight_.fetch_sub(1);
    if (!entering && previous == 0) {
        // Malformed pairing is terminal; do not turn unsigned underflow into
        // an apparently idle coordinator.
        asynchronous_faults_.fetch_or(bad_order);
    }
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended); return; }
    if (!active_) return;
    if (active_->context.identity.device != Address(device) ||
        active_->context.identity.thread != GetCurrentThreadId()) active_->faults |= identity_changed;
    if (entering && previous != 0) active_->faults |= nested;
}
void Collector::Draw(IDirect3DDevice9* device, const Identity& id,
                     const DrawWitness& witness, const RenderResourceEvent& event, bool entering) noexcept {
    LastErrorScope error;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended); return; }
    if (!active_) return;
    if (entering && (active_->faults & ~std::uint32_t(truncated))) { Close(); return; }
    if (!Match(id) || id.device != Address(device)) { active_->faults |= identity_changed; return; }
    if (!witness.boundary.intact || witness.boundary.hook_epoch != id.native_hook_epoch ||
        witness.pass != active_->context.pass) { active_->faults |= identity_changed; return; }
    const auto& b = witness.boundary;
    const bool depths = stage_ == Stage::sprites ? b.sprite_depth == 1 && b.flush_depth == 0 :
        stage_ == Stage::sprites_end ? b.sprite_depth == 0 && b.flush_depth == 0 :
        stage_ == Stage::flush_begin && b.sprite_depth == 0 && b.flush_depth == 1;
    if (!depths || b.faulted) { active_->faults |= nested | native_fault; return; }
    const auto owner = stage_ == Stage::sprites ? active_->context.sprite_owner :
        stage_ == Stage::flush_begin ? active_->flush_owner : 0;
    if (witness.native_owner != owner || (stage_ == Stage::flush_begin && witness.option != active_->option)) {
        active_->faults |= identity_changed; return;
    }
    if (!IsDraw(event.operation) || in_flight_.load() != 1) { active_->faults |= bad_order; return; }
    if (entering) {
        if (draw_pending_) { active_->faults |= nested; return; }
        ++active_->draws;
        if (stage_ == Stage::flush_begin) ++active_->flush_draws;
        draw_pending_ = true;
        pending_sampled_ = active_->samples < max_records;
        if (!pending_sampled_) { active_->faults |= truncated; return; }
        ++active_->samples;
        pending_ = {}; pending_.event = event; pending_.stage = stage_; pending_.ordinal = active_->draws;
        pending_.witness = witness;
        pending_.state = reader_(device);
        if (!pending_.state.valid) active_->faults |= read_failed;
    } else {
        if (!draw_pending_) { active_->faults |= bad_order; return; }
        draw_pending_ = false;
        if (FAILED(event.result)) active_->faults |= draw_failed;
        if (!pending_sampled_) return;
        auto expected = pending_.event; expected.result = event.result;
        if (expected != event || pending_.witness != witness) { active_->faults |= bad_order; return; }
        pending_.event.result = event.result;
        auto& count = active_->records;
        if (count && active_->values[count - 1].event == pending_.event &&
            active_->values[count - 1].stage == pending_.stage &&
            active_->values[count - 1].witness == pending_.witness &&
            active_->values[count - 1].state == pending_.state) ++active_->values[count - 1].repeat;
        else if (count < max_records) active_->values[count++] = pending_;
        else active_->faults |= truncated;
    }
}
void Collector::Abort(Fault reason) noexcept {
    LastErrorScope error;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended | reason); return; }
    Close(reason | unterminated);
}
bool Collector::CopyPass(std::size_t index, Pass& out) noexcept {
    LastErrorScope error;
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) { asynchronous_faults_.fetch_or(contended); return false; }
    if (index >= selected_) return false;
    out = passes_[index];
    if (!out.closed) { out.complete = false; out.faults |= unterminated | asynchronous_faults_.load(); }
    return true;
}
} // namespace cojvr::backends::d3d9::hud
