#pragma once

#include "backends/d3d9/render_resource_probe.hpp"
#include <array>
#include <atomic>
#include <mutex>

// Separate from the eye resource census: no eye index or inferred script route.
namespace cojvr::backends::d3d9::hud {
enum class Route : std::uint8_t { unknown };
enum class Stage : std::uint8_t { sprites, sprites_end, flush_begin, flush_end };
enum class Rejection : std::uint8_t {
    none, disabled, unknown_build, native_boundary_witness_missing,
    nesting_witness_missing, draw_continuity_missing, invalid_identity,
    budget_exhausted, busy
};
struct Admission {
    bool enabled = false, exact_build = false;
    // Both must be supplied by the exact native adapter; sampled callbacks
    // alone cannot prove either. No route/caller inference is permitted.
    bool native_boundary_continuous = false, nesting_observable = false;
    bool draw_hooks_continuous = false;
};
[[nodiscard]] Rejection RejectHookup(const Admission&) noexcept;
[[nodiscard]] const char* RejectionName(Rejection) noexcept;
struct Identity {
    std::uintptr_t device = 0;
    std::uint64_t native_generation = 0, continuity_epoch = 0, hook_epoch = 0;
    std::uint64_t frame = 0, present = 0;
    std::uint32_t thread = 0;
    std::uint64_t native_hook_epoch = 0;
    std::uint64_t permission_epoch = 0;
    bool operator==(const Identity&) const = default;
};
struct BoundaryWitness {
    std::uint32_t sprite_depth = 0, flush_depth = 0;
    std::uint64_t hook_epoch = 0;
    bool intact = false, faulted = true;
    bool operator==(const BoundaryWitness&) const = default;
};
struct DrawWitness {
    BoundaryWitness boundary{};
    std::uint64_t pass = 0;
    std::uintptr_t native_owner = 0;
    std::uint32_t option = 0;
    bool operator==(const DrawWitness&) const = default;
};
struct Context {
    Identity identity{};
    BoundaryWitness boundary{};
    std::uint64_t pass = 0;
    std::uintptr_t sprite_owner = 0, native_caller = 0;
    Route route = Route::unknown;
};
// Keep D3D state keys with their copied values. Success is per key as well as
// aggregate; zero is a value, never an unavailable sentinel.
inline constexpr std::array state_keys{
    D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_BLENDOP,
    D3DRS_BLENDFACTOR, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA,
    D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA, D3DRS_COLORWRITEENABLE,
    D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3,
    D3DRS_ALPHATESTENABLE, D3DRS_ALPHAREF, D3DRS_ALPHAFUNC,
    D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_DEPTHBIAS,
    D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE
};
struct ShaderInfo {
    std::uintptr_t object = 0;
    std::uint32_t bytes = 0;
    bool bound = false, valid = false;
    bool operator==(const ShaderInfo&) const = default;
};
struct Snapshot {
    D3D9RenderResourceInfo rt0{}, depth{}, texture0{};
    ShaderInfo vertex_shader{}, pixel_shader{};
    std::array<DWORD, state_keys.size()> states{};
    std::array<bool, state_keys.size()> state_valid{};
    std::array<DWORD, 4> viewport{};
    std::array<float, 2> viewport_depth{};
    std::array<LONG, 4> scissor{};
    bool viewport_valid = false, scissor_valid = false, valid = false;
    bool operator==(const Snapshot&) const = default;
};
// Only getters and transient getter references; no image/command/resource work.
[[nodiscard]] Snapshot Observe(IDirect3DDevice9*) noexcept;
using Reader = Snapshot(*)(IDirect3DDevice9*) noexcept;
enum Fault : std::uint32_t {
    nested = 1u << 0, identity_changed = 1u << 1, bad_order = 1u << 2,
    read_failed = 1u << 3, draw_failed = 1u << 4, truncated = 1u << 5,
    unterminated = 1u << 6, native_fault = 1u << 7, disabled = 1u << 8,
    contended = 1u << 9, missing_flush_draw = 1u << 10
};
struct Record {
    RenderResourceEvent event{};
    Snapshot state{};
    Stage stage = Stage::sprites;
    DrawWitness witness{};
    std::uint64_t ordinal = 0, repeat = 1;
};
struct Pass {
    Context context{};
    BoundaryWitness sprite_end{}, flush_begin{}, flush_end{};
    std::uintptr_t sprite_end_owner = 0, flush_owner = 0, flush_end_owner = 0;
    std::uint32_t flush_end_option = 0;
    std::uint32_t option = 0, faults = 0;
    std::uint64_t draws = 0, flush_draws = 0;
    std::size_t samples = 0, records = 0;
    std::array<Record, 64> values{};
    bool closed = false, complete = false;
    // This is scheduling/binding correlation, never content admission.
    bool correlation_only = true, capture_admitted = false, occlusion_admitted = false;
};

// One collector per diagnostic session, not per thread/eye. All storage is fixed
// at construction, and the six-pass budget cannot be reset. Caller supplies fresh
// identity witnesses at every boundary/draw. No lock survives a public call.
class Collector final {
public:
    static constexpr std::size_t max_passes = 6, max_records = 64;
    explicit Collector(Reader reader = &Observe) noexcept : reader_(reader) {}
    [[nodiscard]] Rejection Begin(const Context&, const Admission&) noexcept;
    void Boundary(Stage, const Identity&, const BoundaryWitness&, std::uint64_t pass,
                  std::uintptr_t owner, std::uint32_t option = 0) noexcept;
    // Bracket ALL hooked native ingress, even outside selected passes. Register
    // only under the resource diagnostic opt-in. Never hold a lock forwarding.
    void Call(IDirect3DDevice9*, bool entering) noexcept;
    // Called before original draw and after its actual return. `result` at entry
    // is only a placeholder; records receive the returned HRESULT at exit.
    void Draw(IDirect3DDevice9*, const Identity&, const DrawWitness&, const RenderResourceEvent&, bool entering) noexcept;
    // Owner must call on Reset/ResetEx attempts (including failed), permission
    // loss, Present without closure, teardown, or caught native fault.
    void Abort(Fault) noexcept;
    // Copies values only; an open scope is always incomplete/unterminated.
    [[nodiscard]] bool CopyPass(std::size_t index, Pass&) noexcept;
private:
    bool Match(const Identity&) noexcept;
    void Close(std::uint32_t faults = 0) noexcept;
    std::mutex mutex_;
    // Contention/in-flight accounting are independent of metadata locking.
    // A contention fault latches for the session rather than losing a race.
    std::atomic<std::uint32_t> asynchronous_faults_{0};
    std::atomic<std::uint64_t> in_flight_{0};
    Reader reader_;
    std::array<Pass, max_passes> passes_{};
    std::size_t selected_ = 0;
    Pass* active_ = nullptr;
    Stage stage_ = Stage::sprites;
    Record pending_{};
    bool draw_pending_ = false, pending_sampled_ = false;
};
} // namespace cojvr::backends::d3d9::hud
