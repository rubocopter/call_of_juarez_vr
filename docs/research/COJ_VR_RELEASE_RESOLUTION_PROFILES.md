# Startup resolution profiles

## Scope and status

**Experimental launcher selection and startup application implemented and
host-tested; the published prerelease remains unchanged.** Resolve Auto, 75%, 100%, 125% and Manual before
game startup through the existing native `Video.scr` path. Keep ordinary
D3D9/OpenVR stereo and the independent eye bank off. This changes the game's
menu/theater source size too. It adds no renderer, upscaler, live resizing,
in-game resizing or new visor UI.

The offline [`coj_resolution_profiles.py`](../../tools/coj_resolution_profiles.py)
implements the pixel-ratio arithmetic, bounded/strict observation parsing,
preparation/time identity checks and checked nominal payload lower bound. Every
report retains `applyAllowed=false`, unknown effective engine/eye/submit extents,
explicit unresolved native/theater/x86 gates and preservation of selected video
settings. Manual needs no runtime observation. Fresh Background recommendations
produce proposals, not generally qualified CoJ 100% values. Missing recommendations
report unavailable; malformed/foreign/stale data rejects. The launcher separately
records an explicit experimental application decision; it does not turn the
proposal's release qualification flag true. Package publication is unchanged. An explicit
`vr_test.ps1 prepare -ExperimentalResolutionProfile -ResolutionProfile ...`
development trial can apply a proposal at startup, with LAA, full Body IK,
ordinary stereo and reversible video required. Manual also requires
`-ResolutionWidth W -ResolutionHeight H`. This opt-in does not qualify the
proposal's release admission or unresolved physical gates.

Before any experimental video write, the factory-only
`cojvr_d3d9_resolution_limits` helper checks native adapter-0 HAL texture axes,
aspect/shape restrictions and the observed color/depth format families. It
creates no device or surfaces. Unsupported/unavailable caps preserve the selected
resolution; invalid requests reject before staging. Nonzero OpenVR adapter
observations also preserve the selected size: matching ordinal zero alone does
not prove cross-API adapter identity, and multi-GPU is unqualified. These are
necessary bounds, not D3D11/theater allocation, shared transport, memory or
campaign-headroom qualification. The game logs its Scene application key and
recommendation without changing application identity or bindings.

The standalone `cojvr_openvr_resolution_query` target uses only the pinned OpenVR
API and Background initialization, with bounded JSON, UTC/preparation provenance,
HMD/runtime/adapter/application identity and unconditional shutdown after successful
initialization. It acquires no compositor, creates no graphics device and neither
submits nor starts/stops runtime/game processes. Its CLI/invalid argument behavior
is host-tested. The closed-runtime check returns
`VRInitError_Init_NoServerForBackgroundApp` without starting SteamVR. A successful
HMD recommendation has subsequently been observed with the operator-started
PS VR2/SteamVR runtime, matching the prior game recommendation in that configuration
and preserving the runtime process identities. This is live-tested query behavior,
not general equivalence: the helper reports its own generated application key,
which differs from the game's identity. Equivalence under per-application/global
scaling and startup changes remains unqualified.
Example observation: run the helper with `--preparation-id TOKEN` after manually
starting SteamVR, then pass its JSON and the same token to the Python resolver's
`--observation` / `--preparation-id` arguments. The 120-second freshness limit is
an observation fence, not proof of stable headset/application settings.

[Validation](../VALIDATION.md#image-quality-profile-acceptance) remains the
acceptance authority. The
[native-resolution trial](COJ_VR_RENDER_RESOLUTION.md#explicit-native-resolution-trial)
demonstrates actual 2804x2860 native raster/capture/output extents with scoped
repeated positive operator feedback. The operator accepts it as the local Manual
default and requests no repeat physical test. It is not a universal headset
recommendation, safe maximum or new-user release default. Other profiles,
sustained timings, memory limits and broader continuity remain separate gates.

## Profile contract

Let `Wr,Hr` be the fresh OpenVR recommended per-eye extent and `p` the requested
pixel-count ratio. Compute `s = sqrt(p)`, then
`W = floor(Wr*s + 0.5), H = floor(Hr*s + 0.5)` (nearest integer, positive ties up).
Preserve the recommended aspect to integer-pixel rounding error; never
substitute monitor aspect, double width for stereo, or multiply both axes by `p`.
This aspect rule applies to automatic percentage profiles; Manual preserves its
explicit width/height aspect after compatibility checks.

| Profile | Proposed startup request |
| --- | --- |
| Auto | Explicit experimental `p=1` with preserve fallback on unavailable query or incompatible necessary bounds. A generally qualified automatic default remains pending; no speculative GPU scoring or dynamic adjustment. |
| 75% | `p=0.75`; approximately 0.8660 times each recommended dimension. |
| 100% | `p=1`; recommended dimensions. |
| 125% | `p=1.25`; approximately 1.1180 times each recommended dimension. |
| Manual | Explicit positive integer width and height, with the same necessary native/implementation bounds and unresolved memory qualification. Preserve the requested aspect; do not silently replace or clamp either axis. |

These percentages are relative to the queried recommendation, not panel pixels
or an additional SteamVR slider transformation. Record requested percentage,
recommendation and rounded result; do not assume which runtime settings produced
the recommendation. Auto and 100% share a nominal target, but Auto expresses
conditional default policy. Neither guarantees frame rate. Never silently lower
a requested profile, clamp axes independently, distort aspect or enlarge only
transport textures. An inadmissible request falls back to the original selected
native settings with a visible diagnostic. Manual dimensions and release pixel/byte
ceilings require measured qualification before broad support or safe defaults can be claimed.

## Selecting and applying a profile

The existing player launcher's option **7 — Resolution profiles** saves intent;
option **6 — Prepare session** applies it with the game closed. Noninteractive
`ConfigureResolution` accepts `ResolutionProfile`, `ResolutionWidth` and
`ResolutionHeight`; Manual alone accepts nonzero axes. Default is Preserve.
Auto/percentages require an operator-started SteamVR runtime; Auto is an explicit
experimental nominal 100% choice with preserve fallback, not automatic GPU scoring
or a qualified release default. No game/runtime lifecycle is automated.

`tools/coj_resolution_settings.ps1` owns the bounded, strict, atomic launcher
preference `%LOCALAPPDATA%/CoJVR/CallOfJuarez/resolution.json`. It never writes the
runtime's cinematic `settings.ini`. The local developer entry point
`tools/CoJ-VR-Resolution.cmd` uses the same preference and canonical prepare/finish
through `-UseSavedResolutionProfile`. Changing selection never changes a running
game; no targets, tracking, hands or stereo passes change here.

`coj_resolution_session.ps1` reuses the strict Python proposal calculation and
query/caps executables with bounded subprocess waits. Beyond native caps it
rejects the existing 16384-axis and 128 MiB menu-source implementation limits and
a nominal payload reaching the x86 LAA address ceiling. Those necessary bounds
are not a total-memory budget, theater allocation test or support qualification.
No universal 2804x2860 ceiling is introduced. Record requested/applied/observed
extents separately; applied configuration never counts as raster evidence.

## Existing seams and future interventions

All links below are repository-relative; function names identify the seams.

| Existing source | Reuse / intervention |
| --- | --- |
| [Video helper](../../tools/coj_video_profile.ps1), `Apply-CoJVrVideoProfile` / `Restore-CoJVrVideoProfile` | Reuses parsing, original-byte backup, SHA-256 journal-before-write, apply verification and retryable restoration. Separate experimental width/height accept checked positive native integers from the workflow; legacy preserve/2804x2860 remains. Unrelated quality settings are preserved. Low-level parsing is not hardware or memory admission. |
| [Physical workflow](../../tools/vr_test.ps1), `Apply-VrVideoProfile` / `finish` | Retain canonical provenance and restoration. Current native trial requires LAA, full body candidate and bank off, and enables bounded resolution evidence. It is a validation route, not the public profile interface. |
| [Release launcher](../../release/cojvr.ps1), `Invoke-PublicAction`, `Start-PublicSession`, `Restore-PublicInstall` | Install retains its FSAA-0 baseline; the separate session journal applies the selected experimental profile. PrepareSession restores the preceding session before reapplying and publishes fresh provenance. Recovery/update/uninstall restores session first, then install baseline; pending run identity survives interrupted metadata publication. |
| [Runtime](../../src/runtime/openvr_runtime.cpp), `Initialize`, `ReadEyeConfiguration`, `Shutdown` | Query is cached once after Scene initialization, copied into eye data, cleared at shutdown. Preserve its ownership and optics lifecycle; no filesystem mutation belongs here. |
| [Game startup](../../src/games/call_of_juarez/native_stereo_proxy.cpp), `EnsureStarted` / `StartStereoRuntime` | Presenter starts from the proxy's `Direct3DCreate9` entry. This is already inside the running game; no qualified boundary guarantees a config write here precedes native settings parsing. Do not add a startup hook or call native ApplyVideoSettings to bridge it. |
| [Capture](../../src/backends/d3d9/d3d9_stereo_capture.cpp), `EnsureResources` / `PrepareResources` | Actual bound RT0 `GetDesc` supplies the three-slot/two-eye capture extent. Whole-surface `StretchRect` uses `D3DTEXF_NONE`; retain this source-sized path and lease/reset protection. |
| [Presenter](../../src/backends/openvr/stereo_presenter.cpp), `EnsureTextures`, and [shared bridge](../../src/backends/openvr/d3d9_shared_texture_bridge.cpp), `SameCopyDescription` | Native outputs follow frame dimensions, not recommendation. Equal-description `CopyResource` preserves size. Flat theater computes separate optical padding, potentially larger than source; include that allocation in preflight. |

## Recommendation before game startup

The new observation bridge is a small external, query-only OpenVR helper, invoked during
closed-game preparation after the operator has started SteamVR. It must finish
and release its client session before the operator launches CoJ. The existing
[OpenVR probe](../../tools/openvr_probe.cpp) is a reference, not a suitable
drop-in: it initializes the Scene runtime and D3D11, with optional submission.
The [build](../../CMakeLists.txt) has a probe target, but the
[release packager](../../tools/package_release.ps1) now includes both query/caps
executables, the resolver and sibling pinned OpenVR DLL in the verified inventory.
Helper bytes are bound to build provenance even for SkipBuild. A missing helper
leaves native dimensions unchanged; package-integrity failures reject first.

The locally pinned SDK header (`.cache/deps/openvr-2.15.6/headers/openvr.h`,
materialized dependency) describes `GetRecommendedRenderTargetSize` as a suggested
distortion intermediate extent. It describes `VRApplication_Background` as not
starting SteamVR or keeping it alive; Utility initializes without hardware
drivers. Therefore a bounded Background client is the candidate to investigate,
not a claim of working recommendation equivalence. Pair successful init with
`VR_Shutdown`, invalidate interface pointers afterward, and neither submit frames
nor start/terminate SteamVR or the game. Runtime/HMD presence checks alone do not
provide a recommendation.

**Unvalidated blocker:** a separate Background process may have a different
application identity/effective SteamVR recommendation from CoJ's Scene client.
The current `VR_Init` third argument does not establish a shared application key;
the header documents it as reserved startup information. `IdentifyApplication`
requires a known application manifest and does not itself prove matching scale.
Compare helper results with the game's existing `native_stereo_eye_optics`
telemetry under controlled global/per-app scale changes, HMD replacement and
runtime restart. Until equivalence is established, keep automatic default sizing
disabled and label explicitly selected percentages relative to the helper query,
with game-application equivalence unqualified; do not claim supported CoJ 100%.

Pass only a bounded, versioned value record: recommendation, runtime/HMD/adapter
and application identity, observation time and preparation identity. Reject
failed/zero/stale/mismatched observations. No pointers or retained OpenVR session
cross the process boundary. A cached previous run is not a fresh recommendation.
Recheck identity/recommendation in-game for diagnostics; a mismatch requires a
later closed-game preparation, never an in-session rewrite.

## Bounds, memory and failure policy

Before writing, require recognized game/build identity, game closed, unambiguous
video syntax, valid backup ownership and no unresolved deployment/video journal.
Use checked wide arithmetic for rounding, dimensions, area, pitch and byte sums;
reject noninteger/nonpositive Manual dimensions and overflow before narrowing to the
native integer settings. Check actual-adapter D3D9 texture/viewport/aspect and
format constraints, D3D11 feature-level limits, shared transport and the larger
theater outputs. The presenter's D3D11 maximum-dimension check alone is not a
complete native-mode or memory preflight. Windowed startup and FSAA 0 follow the
existing explicit trial; native/client clamping remains a failure to meet the
request, not success.

Use a qualified total-memory envelope, not GPU model or available VRAM alone.
Six four-byte capture textures plus two native presenter outputs have nominal
payload `32*W*H` bytes. Shared handles do not duplicate that payload. Native HDR
color/depth/effects, mono theater SYSTEMMEM reservation, padded theater outputs,
CPU fallback, driver storage and overlapping lifetimes add separate costs.
Native intermediate rounding, allocation thresholds and transitions make total
memory and frame time nonlinear; 75% pixels does not promise 25% less total memory
or time. See the [resource census](COJ_D3D9_RESOURCE_CENSUS.md) for observed native
allocation failures and VA pressure.

CoJ remains x86: original user VA capacity is 2 GiB; the exact-hash LAA derivative
on 64-bit Windows allows 4 GiB, without guaranteeing contiguous space, commit,
VRAM or general JVM high-address safety. Preflight must account for measured VA,
largest free region, commit and loading peaks; an external helper's free VA is
not the game's budget. Caps and a successful sample allocation cannot establish
campaign headroom. Until a conservative envelope is qualified, retain Preserve
as the default and keep selected profiles explicitly experimental rather than
inventing a safe universal cap or promoting automatic default application.

On query/preflight failure, leave selected resolution/fullscreen untouched.
Existing journaled FSAA-0 install policy remains a separate explicit policy.
On partial apply failure, roll back exact original bytes. On failure discovered
after launch, report it and recover only after normal close; do not retry smaller
dimensions, hot-reset or promise crash-proof native allocation fallback.

## Restoration, persistence and acceptance

Keep requested profile/Manual width and height separate from applied `Video.scr` and
recovery state. The existing [cinematic preference owner](../../src/runtime/vr_settings.cpp)
keeps its bounded version-1 configuration unchanged. The separate launcher-owned
file avoids concurrent writers/schema loss. Retain preference intent across
uninstall, but never persist a native override as the original video selection.

The installation video journal spans install to uninstall/update. A separate
session video journal restores the preceding override before applying the next;
its original hash must match the install baseline's applied hash. Never back up
an already overridden size as the installation original. Preserve original bytes,
resolution, fullscreen and FSAA, verify hashes and retain recovery evidence on
failure. Restoration now compares the current/applied/original hashes. Intervening
game/user bytes are copied to a unique adjacent conflict file, hash-verified and
journaled before restoring the original. Missing/corrupt conflict copies or
concurrent changes fail closed with recovery state retained. An already restored
original permits idempotent cleanup. Host fixtures cover interruption/retry;
public update/session integration must preserve the same original baseline.
Restoration holds file ownership through hash checks, preservation, write,
verification and journal commit. A missing settings file is recreated with
CreateNew only after verifying the backup; a concurrent replacement is retained.

Arithmetic/aspect/bounds parsing, preference isolation and interrupted
apply/reapply/restore/conflict handling have host coverage. General recommendation
identity, complete native/theater compatibility and x86 memory envelopes remain
open before broad support or automatic defaults. Reuse the
[video fixtures](../../tests/vr_video_profile.ps1) and
[native-resolution analyzer](../../tools/analyze_coj_native_resolution.py).
For a new size, physical evidence must correlate full-size native color/depth/viewport draws in
both eyes, equal capture extents, distinct equal-size D3D11 outputs and successful
matched-pose submissions. Then assess detail, menu/pointer layout, theater/movie
transitions, loading memory peaks and sustained pacing without probe overhead.
Source review or helper output cannot promote these physical gates. This design
does not change [roadmap priorities](../ROADMAP.md#independent-resolution-and-native-settings-preparation)
or promote any validation state.
