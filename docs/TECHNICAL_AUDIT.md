# Technical audit baseline — 2026-09-14

This document is the authoritative engineering audit for the current stabilization phase of Call of Juarez VR. It supersedes the raw audit notes as a working baseline while preserving their findings and updating them against the current repository state.

## Provenance and scope

The original audit was performed against commit `76d9bc89541b20e21827acfa12ded7bfc9225ad2` plus the then-current local working tree, which contained the D3D9/OpenVR implementation and tests before they were committed. That code was subsequently committed, and commit `22b40d90e447006ec1eda068e0396721cb2820ab` added stronger D3D9 hook-continuity diagnostics. Documentation was then refreshed without changing the implementation.

This audit remains the historical stabilization baseline, while the current repository extends it with the native-stereo camera proof and the Phase 5 capture/mailbox/presenter path described below. Before implementing any item, agents must still inspect `git status`, HEAD and the owning files rather than assuming this document alone describes every line of code.

## Evidence policy

Keep these categories distinct:

- **confirmed** — directly demonstrated by source inspection, host tests or an identified live run;
- **plausible hypothesis** — consistent with current evidence but not yet demonstrated;
- **unresolved** — evidence is insufficient to choose between competing explanations.

Do not promote a plausible hypothesis to a root cause merely because it fits the latest run.

Validation states remain:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

## Current confirmed baseline

The following facts are currently established:

- Call of Juarez (2006) DX9 can run and load gameplay with the project's forwarding path while remaining usable on the monitor.
- Classic D3D9 -> CPU readback -> D3D11 upload has completed successfully in the exact game build.
- OpenVR can initialize against manually started SteamVR, acquire a valid PSVR2 HMD pose and accept D3D11 eye submissions.
- The in-game flat bridge has completed D3D9 readback, D3D11 upload, pose wait and OpenVR submission for captured game frames.
- A later exact-build diagnostic observed exactly three project callbacks for `Present`, `BeginScene` and `EndScene`, followed by loss of integrity of the installed device-vtable entries while the game continued rendering on the monitor.
- That hook-integrity loss is a confirmed failure mode of the current interception design. Run `20260914T215121Z-9bac4e22cffd` established that all four lost device slots return exactly to their recorded original targets in `C:\WINDOWS\system32\d3d9.dll`; no foreign replacement target was observed. It still does **not** prove which component performs the restoration, why it occurs, or that hook loss is the only reason the user never sees a stable game image in the headset.
- HMD-driven game-camera rotation and distinct native eye rendering are live-observed. Run `20260916T133322Z-36c287cc43d8` produced two complete ChromeEngine eye passes with distinct real RT0 hashes and OpenVR submission. Run `20260916T153109Z-8976b8f77775` then live-observed the corrected `100` game-units-per-metre eye translation and physically validated left PS VR2 Sense Create recenter. Run `20260916T221254Z-861f3c15abd4` validated deferred-presenter scene-focus handoff and clean OpenVR/runtime shutdown. Run `20260916T224239Z-e43b46698e5c` validated explicit render-pose submission and removed the reported head-turn snap-back. Frame pacing/performance remains poor and the flat/menu path is not presented in the headset. Positional 6DOF plus the body/IK preflight are implemented: tracked Sense anchors, campaign actor reconciliation and the visible render-element writer are live-proven, while corrected arm composition remains host-tested, lower-body geometry/leg solving remains read-only and motion-controller gameplay remains unimplemented. Run `20260918T165754Z-845101e7557b` proved `RotateElementWithChildren` changes both arms through both eye renders, but physically failed because world-space solver axes were passed to an element-local native rotation. Runs `20260918T204701Z-f561e493f4ab` and `20260918T210459Z-b59448961f3c` then exercised the corrected element-local path and reached both solved targets for hundreds of frames before fail-closing at frames 416 and 614. The restore criterion was tighter than one float ULP at the live world coordinates; current host source uses separate measured restore tolerances while retaining strict mutation detection.
- The active physical gate is presentation performance/comfort. Run `20260917T161917Z-909b63e114af` measured roughly `15-25 ms` of classic-D3D9 CPU copy per stereo frame at `2560x1440`/FSAA8. The three 2026-09-18 runs all used the reversible `1920x1080`/FSAA0 profile and reduced sampled copy averages to `9.7-10.7 ms`, but the user reported visibly poor resolution. They also exposed a head-tilt comfort defect: the native camera omitted roll while OpenVR received the full HMD pose. Current source applies roll to the native camera basis and the live verifier requires a meaningful tilt sample; this is host-tested only. Resolution remains coupled to the provisional CPU-readback cost, and all three runs still reported `shutdown_complete=false`.
- Run `20260918T213453Z-a789ac61ac91` physically validated native-camera roll: the previous tilt-induced nausea was absent while telemetry covered `-27.286` to `+40.762` degrees. It also sustained 13,860 successful arm applications/restorations with no fail-close, validating the float-aware restore threshold. The body gate still failed visually. Controller front/back was reversed even though elbow/wrist targets were reached, and the wrist/hand remained twisted with `hand_orientation=natural`. Current host source flips only tracked Z at the exact-game hand-target boundary; hand-orientation ownership remains unresolved.

The active historical diagnostic candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` records which `Reset`, `Present`, `BeginScene` and `EndScene` slots are replaced and resolves replacement addresses to owning modules. Preserve it as evidence/baseline; do not let its existence bypass the audit-remediation work below.

## Updated findings

### A1 — Observability previously supported conclusions stronger than the evidence

**Severity:** P0  
**Status:** live-tested observability; observation gate failed on hook ownership

The original implementation used one-shot log markers for frame boundaries, so absence of later milestones could not distinguish low callback count from hook loss, failure, blocking, device replacement or logging problems.

The Phase 0-4 candidate now emits run-bound JSONL records with PID/TID, monotonic time, factory/device/swapchain/generation IDs, callback and stage entry/exit, HRESULT/runtime result, duration, exact process and per-device counters, content hashes that ignore the undefined D3D9 X/alpha byte, and independent capture/content/upload/left-submit/right-submit sequences. A separate observer thread emits periodic per-device summaries and hook-integrity state; normal process exit emits `run_end`, while its absence is rejected as an incomplete run. Detailed high-frequency records are bounded, but counters and active-stage state are updated on every observation.

The parser/verifier has host-tested success, malformed/mixed-run and incomplete-run paths. Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` exercised this telemetry in the exact game: provenance was complete, one factory/device/swapchain generation was identified, all three observed captures completed upload and balanced-eye submission, and both runs ended normally. The verifier failed only because four device hooks lost ownership after the third frame.

**Required action:** preserve these runs as live observability evidence. The Steam-Overlay-disabled A/B remains the controlled experiment required to resolve this D3D9 finding, because the native factory `CreateDevice` entry was already owned by `gameoverlayrenderer.dll` before the project installed its factory hook. The user has deferred that repetition while the separate camera/render-boundary gate is investigated; do not infer overlay causality without the A/B run.

### A2 — The current vtable patching mechanism is not transactionally safe

**Severity:** P0  
**Status:** live-tested failure persists under remediated ownership

The audited implementation had the unsafe behavior described above. It has now been replaced by `VtablePatch` and `HookRegistry`: conditional compare/exchange, explicit no-change/applied/protection-failure/conflict/incomplete-rollback outcomes, retained originals and ownership records, per-vtable records, conflict-safe restore, synchronized callback lookup, module pinning and the same ownership model for factory/device/swapchain hooks.

Failure-injection host tests cover failure before and after replacement, conflicting targets/reinstall, two vtables, integrity loss, partial rollback, foreign-hook-safe restore and callback reentrancy during installation. Native hook tests preserve HRESULTs. Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` exercised the replacement infrastructure in the game and again lost `Reset`, `Present`, `BeginScene` and `EndScene` ownership on the same device vtable after exactly three callbacks. There was no install conflict or install failure. The second run recorded each `current_target` equal to the corresponding `original_target` in `C:\WINDOWS\system32\d3d9.dll`. Source inspection finds no production callsite that invokes `HookRegistry::Restore` for these hooks while the game is running.

**Required action:** determine which external component restores the native device vtable. The required controlled test remains an A/B observation with Steam Overlay disabled, verifying that the factory `CreateDevice` original target is no longer `gameoverlayrenderer.dll` before drawing a conclusion. This test is currently deferred by the user-directed camera/render-boundary gate. Do not add blind re-hooking.

### A3 — D3D9 factory wrapping currently creates split COM identity and bypass paths

**Severity:** P1  
**Status:** live-tested single factory/device/swapchain coverage; sustained callbacks blocked by hook loss

The classic path no longer returns `Direct3D9Forwarder`. It returns the native `IDirect3D9` and observes `CreateDevice` by patching each reachable native factory vtable through `HookRegistry`. The D3D9Ex forwarder remains only behind its explicit laboratory opt-in, and classic staging rejects both its environment switch and marker.

A native host test creates a device, recovers the factory through `GetDirect3D`, verifies canonical `IUnknown` identity, then creates a second observed device through that recovered factory with unchanged native HRESULTs. Stable diagnostic IDs cover factories, devices and implicit swapchains; successful Reset advances device generation and re-identifies the implicit swapchain without changing native COM identity. Hook records include original/replacement/current target modules.

Run `20260914T215121Z-9bac4e22cffd` again observed one native factory, one device, one implicit swapchain and generation 1; that device received all 3/3/3 `Present`/`BeginScene`/`EndScene` callbacks before hook loss. The factory's pre-project `CreateDevice` target belonged to `C:\Program Files (x86)\Steam\gameoverlayrenderer.dll`, while the four device slots later reverted exactly to their Windows D3D9 originals. **Required action:** use an overlay-disabled A/B run before changing the interception design. Do not introduce a full device wrapper without new evidence.

### A4 — Capture, VR synchronization and compositor submission are coupled inside the game callback

**Severity:** P0  
**Status:** Phase 5 separation and acceptance host-tested and live-exercised; performance remains open

The original flat/native proof path performed D3D9 readback, CPU access, D3D11 upload, pose wait and both eye submissions synchronously from the render callback. The current native-stereo path separates those responsibilities: `D3D9StereoCapture` queues/copies eye results on the game thread, produces owned `StereoCpuFrame` data, `FrameMailbox` keeps a bounded latest-frame handoff, and `OpenVrStereoPresenter` owns D3D11/OpenVR work on its presenter thread.

Runs `20260916T221254Z-861f3c15abd4` and `20260916T224239Z-e43b46698e5c` exercised this separated path in the headset, including separate new/repeated submission counts, scene-focus handoff, explicit render-pose submission and clean presenter/runtime teardown. The latest run also quantified the remaining cost: roughly `15-22 ms` of owned CPU copy on sampled 2560x1440 stereo frames, plus the diagnostic hashing cost that the current source has since removed from almost every frame.

The Phase 5 host acceptance matrix now includes explicit source resize, pre-Reset resource invalidation/post-Reset generation recovery, a second device with identical dimensions/format, and a controlled paused-producer presentation policy. `PresentationCadence` keeps the last valid frame presentable and labels subsequent successful submissions as repetitions until a new upload arrives; a failed submit does not consume the pending-new classification. Sustained frame pacing still remains below the product gate.

**Required action:** reduce readback/copy overhead while preserving separate capture/submit accounting, presenter ownership and the now-host-tested pause/reset/recreation contracts.

### A5 — Renderer resources are keyed by image description rather than device/generation ownership

**Severity:** P1  
**Status:** native-stereo Phase 5 ownership and reset/resize/recreation seams host-tested

The historical bridge could reuse resources when dimensions/format/MSAA matched even if the underlying D3D9 device or generation changed. The current native-stereo transport keys D3D9 capture resources by device, generation and full surface description including MSAA, carries device/generation identity in each owned frame, resets presenter textures when that identity changes and rejects stale generations/sequences.

The presenter has a single D3D11/OpenVR owner and teardown occurs outside `DllMain`. The owned D3D9 stereo-capture test now exercises a source resize on the same device/generation, explicit `InvalidateResources()` before a classic D3D9 Reset followed by recovery on a new generation, and migration to a second device with the same dimensions/format. The exact Call of Juarez proof still does not depend on a `Reset` hook; the invalidation seam exists for owners that have an explicit lifecycle signal.

**Required action:** preserve the device/generation ownership contract and only wire `InvalidateResources()` where an explicit lifecycle boundary is available; do not reintroduce a `Reset`-hook dependency into the exact CoJ proof merely to exercise this seam live.

### A6 — Host tests do not yet prove the deployed pipeline

**Severity:** P0  
**Status:** host acceptance complete through current Phase 6 components; live/runtime evidence remains separate

Phase 1 acceptance is now host-tested. Neutral runtime, build identity, OpenVR and OpenXR targets are separated; OpenVR-only and OpenXR-only configurations build/test while the disabled SDK root is deliberately absent; CI bootstraps each enabled dependency; integration artifacts require Win32. Native D3D9 tests load and assert the system DLL, proxy smoke tests run in isolated directories, unavailable HAL/capability results use CTest SKIP, scene boundaries are exercised, and classic readback validates full asymmetric frames, row pitch and temporal changes.

Phase 2-4 tests add deterministic patch failures/conflicts/multiple vtables/rollback/reentrancy, native factory recovery/two-device identity, swapchain observation, generation changes, structured stage failures and telemetry parser rejection paths. Phase 5 adds the bounded mailbox, resize/Reset/new-device owned D3D9 stereo-capture coverage and a controlled presentation-cadence pause/repeat test, while later live runs exercise the dedicated presenter in the exact game. Phase 6 now adds deterministic OpenVR state-policy coverage for focus loss, one-eye submit failure, invalid tracking, runtime disconnect and shutdown, plus process-owner conflict/release semantics, move-assignment safety and controlled D3D11 synchronization strategies. Fresh Debug and Release suites pass 23/24 CTests; the classic shared-texture capability is the single explicit SKIP on this host.

Runtime submission/state-transition evidence from the real compositor remains classified as live/headset evidence rather than host evidence. The configurable animated OpenVR probe is built but has not been physically rerun for this Phase 6 increment.

**Required action:** keep live/headset promotion separate from host results and collect fresh run-bound runtime-state/probe evidence only when a manual SteamVR/HMD gate is actually requested.

### A7 — Live verification is not tied to a unique execution

**Severity:** P1  
**Status:** live-tested run binding; overlay A/B deferred

Phase 0 now supplies a SHA-256 build manifest, staging-assigned `run_id`, run manifest, exact deployed-file identities and a run-evidence package. The proxy records the bound run/build-manifest IDs at startup, and all D3D9 live verifiers reject a log, staging state, build manifest or deployed hash that does not belong to the same run. Host tests cover complete/incomplete evidence, finalization recognition and changed deployed artifacts.

Phase 4 adds the structured runtime side: PID/TID, monotonic timeline, factory/device/swapchain/generation identity, exact global and per-device counters, stage results/durations, content-change sequencing, periodic summaries and `run_end`. The parser rejects malformed records, foreign run/build IDs and missing finalization unless explicitly asked for an incomplete diagnostic report. The evidence collector recognizes the structured final event.

Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` exercised that provenance chain in the exact game. Both produced a unique `run_end`, matched their run/build/staging/deployment identities and were collected into run-specific evidence packages. Physical headset confirmation is intentionally outside provenance acceptance.

**Required action:** preserve the run-bound verifier guarantees for every staged candidate. If the deferred overlay-disabled A/B resumes, old logs must remain incapable of validating it, and its result must be rejected unless the run manifest, deployed hashes and factory-target evidence all belong to the same `run_id`.

### A8 — Deployment is reversible only on the happy path, not transactional

**Severity:** P1  
**Status:** substantially remediated at host level; full script-level interruption matrix open

Current staging/unstaging writes a recovery journal before mutating managed game assets and
automatically resolves an interrupted journal before a later stage/unstage attempt. Staging rejects
a running `CoJ.exe`, rejects a non-Win32 build manifest, validates x86 PE machine type for the exact
game/proxy/OpenVR/ChromeEngine artifacts, prepares managed files/directories under deterministic
temporary names, verifies SHA-256/manifests there, and only then moves them to their final names.
Recovery restores a proven original or removes only recognized staged/temporary content; changed
destinations, backups or temporary artifacts fail closed and preserve the journal for diagnosis.
Run-bound manifests and verifiers prevent stale logs from validating a new native-stereo candidate.

Host coverage now includes clean/no-original recovery, a pre-existing original DLL, verified
temporary file installation, interrupted temporary-file cleanup, partial temporary-directory
cleanup, interrupted managed-directory restoration, missing-backup rejection, external-change
rejection and idempotent no-journal recovery. The remaining gap is the full end-to-end script matrix:
failure injection after every staging/unstaging mutation, repeated real stage/unstage cycles and an
active-process integration case.

**Required action:** preserve the current journal/verified-temporary/recovery contract and complete the remaining end-to-end failure/repetition/process test matrix described in remediation Phase 7.

### A9 — Declared architectural separation does not match current build dependencies

**Severity:** P1  
**Status:** resolved at host/build level

The neutral runtime now contains only neutral math. Build/game identity, diagnostics, OpenVR runtime/backend and OpenXR runtime/backend are separate targets. `COJVR_ENABLE_OPENVR` and `COJVR_ENABLE_OPENXR` independently control dependency checks and targets; CI prepares only enabled pinned dependencies. Both single-backend configurations build and pass the host suite with the disabled SDK root intentionally absent.

**Required action:** preserve this dependency boundary as later runtime ownership work proceeds.

### A10 — Runtime/lifetime/error contracts are not yet recovery-safe

**Severity:** P1  
**Status:** OpenVR host contract substantially resolved; OpenXR/general lifetime work remains open

The OpenVR path now has explicit initialized/connected/focused/tracking-valid/presenting/shutdown state, drains relevant runtime events, fails presentation closed when tracking/connection is invalid, and records state transitions in the presenter. A process-owner gate rejects competing `OpenVrRuntime` owners, teardown releases ownership outside `DllMain`, and move-assignment now shuts down an existing owned runtime before taking another `Impl`. Host tests cover owner conflict/release and moved-from runtime queries. Allocation-capable reporting paths inside `noexcept` runtime methods are contained by failure guards so error formatting/allocation failure cannot escape those boundaries and terminate the process.

Per-eye submit outcomes feed the runtime state model; the presenter already records sampled left/right compositor results, pose wait and submit timing. The D3D11 handoff order is explicit as `UpdateSubresource -> no forced GPU wait -> Submit_TextureWithPose -> PostPresentHandoff`. A bounded diagnostic seam compares `none`, `Flush` and event-query synchronization on the host without changing the production default to a global wait. The visible probe now supports a configurable frame count and recognizable animated left/right pattern plus selectable diagnostic GPU-sync strategy.

This does not resolve the equivalent OpenXR ownership/lifetime contract, nor does host simulation prove SteamVR disconnect/focus behavior in a physical session.

**Required action:** preserve the OpenVR ownership/state contract, collect live state-transition/probe evidence when physically exercised, and address OpenXR lifetime/state semantics independently before promoting that backend.

### A11 — Neutral math contracts are ambiguous before camera work begins

**Severity:** P2  
**Status:** partially host-tested for the exact CoJ/OpenVR stereo candidate; general neutral contract remains open

`EyeView::pose` is not guaranteed to mean the same space/transform across OpenVR and OpenXR. The exact CoJ/OpenVR candidate now defines OpenVR `EyeView::pose` as eye-to-head, maps its translation into the native right/up/forward camera basis and maps asymmetric per-eye FOV into the engine's off-center frustum builder. Host tests cover that mapping and reject invalid frusta/bases. OpenXR equivalence and a backend-independent proof across runtimes remain open.

These issues predate native stereo and no longer describe the current renderer state. The exact CoJ/OpenVR path now constructs and has live-tested distinct game stereo cameras, which makes the remaining neutral-contract work a portability and hardening requirement rather than a blocker to proving that game-specific path.

The HMD/native-stereo gate has now narrowed the OpenVR subset without closing this finding:
the neutral HMD pose convention is explicitly right-handed `+X` right, `+Y` up, `-Z`
forward with `(x,y,z,w)` quaternions, and base-relative orientation/recenter is host-tested.
The OpenVR global-action edge feeding that policy is additionally headset-validated for left
PS VR2 Sense Create by run `20260916T153109Z-8976b8f77775`.
OpenVR supplies HMD orientation through a backend-neutral `PoseSource`, while
`CameraStereoRuntimeCallbacks` supplies renderer-neutral per-eye data to the exact game adapter.
OpenVR eye-to-head semantics and the CoJ asymmetric frustum mapping are host-tested. The first
physical native-stereo attempt exposed one OpenVR-specific vertical-sign conversion defect:
`GetProjectionRaw` supplied negative top/positive bottom while neutral `EyeFov` requires
positive up/negative down. That adapter conversion is now explicit and host-tested. OpenXR
equivalence and general neutral stereo-matrix validation remain open. Rigid-transform conversion
now also rejects non-finite values, scaled/non-orthogonal bases and reflected rotations in host
tests; that hardening does not by itself close the broader cross-runtime contract.

**Required action:** complete remediation Phase 8 before camera/stereo implementation is promoted.

### A12 — Documentation and source-to-run provenance were incomplete

**Severity:** P1  
**Status:** live-tested provenance; documentation synchronized to current gate

README, architecture, roadmap, research notes and handoff were refreshed after the original audit. The Phase 0 tooling establishes this chain for staged diagnostics:

`sources -> build -> package -> deployed files -> process/run -> evidence`

Clean source snapshots are identified by commit/tree, dirty manifests are explicitly non-commit-reproducible and include status/diff/untracked hashes, built artifacts are SHA-256 identified, staging records game/engine/deployment identities, and collection produces a run-specific evidence package without launching the game. Phase 4 adds structured process/device/event/finalization evidence and a run-bound parser. Runs `20260914T214318Z-fe71b222b664` and `20260914T215121Z-9bac4e22cffd` live-tested the complete provenance chain. Documentation continues to treat the observed three-frame hook loss as a confirmed failure mode, not a proven sole root cause of the blank headset.

**Required action:** keep this audit and the remediation plan authoritative, update finding status only with evidence, and make run provenance part of the tooling rather than a manual narrative.

## Causal model

The stabilization work is driven by this chain:

```text
incomplete artifact/run provenance
        -> insufficient telemetry
        -> overconfident attribution to one frame boundary/hook
        -> design changes before the previous hypothesis is discriminated
        -> another physical run that still cannot isolate the failure
```

The audit goal is to break this cycle before adding more VR features.

## Hypotheses that remain open for the blank headset

The following are still legitimate until stronger evidence eliminates them:

| Hypothesis | Evidence currently available | Evidence still required |
| --- | --- | --- |
| The project stops observing the active render path | run-bound live evidence identifies the active factory/device/swapchain/generation and proves all four lost device slots return to their recorded Windows D3D9 originals | overlay-disabled A/B evidence that identifies whether the restoration still occurs after `gameoverlayrenderer.dll` is absent from the factory target |
| A bridge stage ceases to progress | first frames completed; active-stage/counter/duration telemetry is host-tested | sustained game evidence and, after Phase 5, independent presenter progress |
| OpenVR does not receive useful new work at the required cadence | initial submissions accepted; separate capture/content/upload/eye-submit sequences are host-tested | run-bound cadence evidence and later headset-visible confirmation |
| Captured image is not always the final useful game image | capture remains tied to EndScene; RGB content hashing distinguishes repeats | run-bound changing-content evidence and later visual capture validation |
| Focus/tracking/runtime state changes during the relevant window | timestamped initialization/wait/submit results are now emitted | live correlation plus Phase 6 focus/tracking lifecycle states |

There is currently no basis to blame PSVR2 hardware, abandon OpenVR or reactivate D3D9Ex as the main path. A narrowly scoped game-camera diagnostic is now authorized to establish the ChromeEngine3 camera/render boundary; it does not change the status of the blank-headset hypotheses above.

## Camera/render boundary evidence — 2026-09-15

The current exact `ChromeEngine3.dll` has now been statically inspected far enough to
justify a separate functional camera gate. This evidence does not change A1/A2 status:

- `LawmanModule.SetViewFullscreen(Camera)` reaches native `Module.SetViewCamera` at RVA
  `0x00039710`, which installs the resolved native camera on the active level view.
- MSVC RTTI identifies the primary `CBaseCamera` vtable at RVA `0x0030AE1C`.
- vtable slot `+0x24` targets RVA `0x001C5BA0`. That function invokes the FOV/frustum
  virtual at `+0x28`, calls matrix update RVA `0x0022BB10`, updates projection-related
  globals and finally writes the same camera pointer to renderer offsets `+0x1AC` and
  `+0x1B0` through global renderer pointer RVA `0x00590074`.
- slot `+0x28` targets RVA `0x001C58E0`; the Java `Camera.SetFOV` path ultimately reaches
  this camera projection boundary.
- the native camera keeps a paired transform: world/camera matrix at `+0x44` and its
  inverse/view source at `+0x04`. Native setters `0x0022C4D0`/`0x0022C510` update `+0x44`
  then invoke inverse RVA `0x001F3F80` into `+0x04`.
- RVA `0x0022BB10` copies `+0x44 -> +0xC4` and `+0x04 -> +0x84`, derives effective view
  at `+0x104`, world/culling transform at `+0x144`, projection at `+0x184`, and combined
  view-projection at `+0x204`.

The dedicated `d3d9_camera_probe` therefore hooks only the exact `CBaseCamera` render
update and FOV slots. Transient orientation is now applied to the source basis before the
render update and restored immediately afterward; the FOV hook is diagnostic FOV only.
Ordinary system-D3D9 forwarding is used solely as bootstrap. No OpenVR initialization or
D3D9 frame-vtable interception is involved.
See `docs/research/COJ_CAMERA_PATH.md` for the complete evidence and live acceptance gate.

The first `d3d9_hmd_camera` live attempt showed that a derived `+0xC4` write was overwritten.
The second attempt, run `20260915T211240Z-7d1c65d07a43`, moved `+0x44` successfully but
still left the visual camera fixed; the user instead observed HMD-dependent disappearance
of distant world geometry. This identifies `+0x44/+0xC4/+0x144` as insufficient alone and
is consistent with a world/frustum-culling path. The current implementation now mirrors the
native setter contract by recomputing `+0x04 = inverse(+0x44)` before the original render
update, so `+0x104` view and `+0x204` view-projection can change coherently. Debug/Release
builds and both full host suites pass after CoJ closed, so this revision is **host-tested**.
The second failed run is preserved as package SHA-256
`8E015C2B39326EE2E9C52804E9C6E2E03262088B1B3FFB05E5BC92747B45A5D2`. The proxy still
has no D3D9 frame hooks.
This does not change A1/A2 and does not close A11.

Static inspection has also identified the exact engine render-view boundary used by the first
native-stereo candidate: vtable target RVA `0x00030FB0` and core RVA `0x00030E00`. The
first implementation rendered the left eye through the original view method and the right eye
through the core, applying per-eye translation/asymmetric projection at the proven
`CBaseCamera` boundary. Run `20260916T130852Z-1438628c90c6` later proved that core-only right
pass insufficient: right RT0 was `D3DFMT_NULL` on every sampled attempt, while the left full
wrapper pass ended on a color backbuffer. Exact-build disassembly confirms `0x30FB0` owns a
`view+0xD7` rendered guard and post-core work. Current source therefore replays full `0x30FB0`
for both eyes and restores the guard transactionally.
It now keeps the complete per-eye source world/view, frustum and derived state alive across
the whole render-view pass, then restores source `+0x04/+0x44`, frustum fields and derived
camera matrices at `+0x84`, `+0xC4`, `+0x104`, `+0x144`, `+0x184` and `+0x204` after each eye.
The provisional transport captures each eye from classic D3D9, uploads into separate D3D11
textures and submits them to OpenVR. Run `20260916T113600Z-native-stereo` reached HMD camera
motion but failed before the first capture because the OpenVR vertical FOV signs were wrong;
the same observation exposed premature restoration of culling state. After those fixes, run
`20260916T123049Z-8d977bb5b439` executed both eye camera/projection passes, captured and
submitted frames, and produced visible gameplay in the headset. Every sampled left/right pair
had the same RGB hash, however. The transport was reading the swap-chain backbuffer from
inside ChromeEngine's render-view call, so successful submission did not demonstrate capture
of distinct engine eye outputs. The run also ended after factory-hook restoration without
OpenVR shutdown or `run_end`. Current source captures active render target 0, records whether
it aliases the backbuffer, refuses identical-eye submission, records renderer-camera identity
at each eye update and performs runtime shutdown before abandoning process-lifetime D3D9
references. Run `20260916T133322Z-36c287cc43d8` then proved the complete-wrapper capture path:
both eyes captured real `2560x1440` `D3DFMT_A8R8G8B8` RT0 content, sampled hashes were distinct,
renderer-camera correlation was true for both eyes and OpenVR submission succeeded. The user
observed binocular gameplay but reported poor fusion/comfort and mediocre performance; shutdown
again stopped before `native_stereo_runtime: stopped`/`run_end`.

Subsequent static data inspection identified a concrete stereo-scale defect in that live
candidate. `Data/Player/PlayerProperties.def` defines movement in `cm/s` and acceleration in
`cm/s^2`; the runtime pose contract is metres. The candidate had applied eye-to-head metres
directly as ChromeEngine units, shrinking a 65 mm IPD by 100x. Current source converts metres
to centimetres in the CoJ adapter, records applied eye positions/frustum and D3D9 viewport,
and brackets readback/runtime shutdown with stage telemetry. Debug and Release suites pass
18 tests plus the expected classic-D3D9 shared-texture capability SKIP. Run
`20260916T153109Z-8976b8f77775` subsequently live-observed the corrected scale in submitted
stereo frames: OpenVR eye X offsets of `-0.032/+0.032` m became approximately `-3.2/+3.2`
game units at the neutral camera before world orientation, with distinct real-color eye hashes
and valid asymmetric frusta. The user still reported very poor visual quality/frame pacing and
discomfort, so usable fusion/comfort remains unvalidated. The same run again stopped at
`native_stereo_shutdown: stage=runtime_begin` without `runtime_end`/`run_end`.

## Target component boundaries

The remediation should converge on explicit components with narrow ownership:

| Component | Responsibility |
| --- | --- |
| `BuildIdentity` / per-game catalog | known-build recognition and authorized capabilities |
| `VtablePatch` / `HookRegistry` | safe conditional patching, originals, ownership and integrity per vtable |
| `DeviceContext` | device/factory/swapchain/thread/generation identity |
| `RenderTelemetry` | lightweight callback/stage counters and timing |
| `D3D9Capture` | game-thread capture producing owned CPU frames |
| `FrameMailbox` | bounded transfer of complete frames between producer and presenter |
| `OpenVrPresenter` | exclusive OpenVR + D3D11 ownership and presentation cadence |
| `RunEvidence` | source/build/deployment/process/evidence correlation |

The flat diagnostic path should ultimately be:

```text
verified D3D9 callback
  -> capture owned frame on game thread
  -> publish bounded frame
  -> OpenVR/D3D11 presenter
  -> compositor statistics/evidence
```

A presenter may repeat the last frame for diagnostic continuity, but `capture_sequence` and `submit_sequence` must remain separate.

## Authority and change control

During the stabilization phase:

1. `docs/TECHNICAL_AUDIT.md` defines the open engineering findings.
2. `docs/AUDIT_REMEDIATION_PLAN.md` defines execution order and acceptance gates.
3. `docs/VALIDATION.md` records evidence actually obtained.
4. `docs/internal/CODEX_HANDOFF.md` records the immediate continuation checkpoint.
5. `ROADMAP.md` remains the product-level milestone view.

When implementation materially changes one of the findings, update its status here in the same change or immediately after host validation. Do not silently treat an audit item as resolved.
