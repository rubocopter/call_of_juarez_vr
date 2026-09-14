# Technical audit baseline — 2026-09-14

This document is the authoritative engineering audit for the current stabilization phase of Call of Juarez VR. It supersedes the raw audit notes as a working baseline while preserving their findings and updating them against the current repository state.

## Provenance and scope

The original audit was performed against commit `76d9bc89541b20e21827acfa12ded7bfc9225ad2` plus the then-current local working tree, which contained the D3D9/OpenVR implementation and tests before they were committed. That code was subsequently committed, and commit `22b40d90e447006ec1eda068e0396721cb2820ab` added stronger D3D9 hook-continuity diagnostics. Documentation was then refreshed without changing the implementation.

The current local and remote repositories are considered aligned for this audit baseline. Before implementing any item below, agents must still inspect `git status`, HEAD and the owning files rather than assuming this document alone describes every line of code.

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
- That hook-integrity loss is a confirmed failure mode of the current interception design. It does **not** yet prove which component replaces the entries, why the transition occurs, whether other devices/factories are involved, or that hook loss is the only reason the user never sees a stable game image in the headset.
- No functional game-camera VR, real stereo rendering, 6DOF integration or motion-controller gameplay is implemented yet.
- Headset-visible in-game presentation is not validated.

The active historical diagnostic candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` records which `Reset`, `Present`, `BeginScene` and `EndScene` slots are replaced and resolves replacement addresses to owning modules. Preserve it as evidence/baseline; do not let its existence bypass the audit-remediation work below.

## Updated findings

### A1 — Observability previously supported conclusions stronger than the evidence

**Severity:** P0  
**Status:** partially resolved

The original implementation used one-shot log markers for frame boundaries, so absence of later milestones could not distinguish low callback count from hook loss, failure, blocking, device replacement or logging problems.

The current code improves this materially: `Present`, `BeginScene`, `EndScene`, callback returns, submission milestones and installed-vtable continuity are now counted/inspected. This is what allowed the later hook-integrity loss to become confirmed evidence.

Still missing:

- per-run identity;
- PID/TID and monotonic timestamps;
- device, factory, swapchain and generation identity;
- stage entry/exit and duration for every critical operation;
- explicit separation of callback count, capture count, **new-content** count, upload count and submit count;
- periodic summaries independent of render callbacks;
- explicit incomplete-run markers when the process terminates without a final summary.

**Required action:** complete the structured run-evidence and render-telemetry model in remediation Phase 4. Do not infer unique game frames from OpenVR submission count.

### A2 — The current vtable patching mechanism is not transactionally safe

**Severity:** P0  
**Status:** open

Source inspection shows that the current patch helper can modify an entry and then report failure if protection restoration fails. Installation/rollback logic can therefore lose reliable ownership of an entry. Other issues include unconditional exchange before conflict validation, one global vtable assumption, incomplete rollback guarantees, unsafe reinstall semantics, insufficient synchronization and duplicated weaknesses in swapchain hooking.

The later hook-integrity diagnostic confirms that vtable ownership is unstable in the live process, making these defects more important, not less.

**Required action:** replace ad-hoc patching with an explicit patch/registry contract that distinguishes no-change, applied, protection failure, conflict and incomplete rollback; uses conditional replacement; records ownership per vtable/device; verifies integrity; and never blindly overwrites another component's hook.

### A3 — D3D9 factory wrapping currently creates split COM identity and bypass paths

**Severity:** P1  
**Status:** open

`Direct3D9Forwarder` returns the wrapper for selected interfaces but delegates other `QueryInterface` requests to the inner object. Devices remain native, and `GetDirect3D` can expose the native factory. That makes both wrapped and native factory paths reachable and complicates guarantees that all subsequently created devices are observed.

This is a plausible coverage problem, not yet a demonstrated cause of the live failure.

**Required action:** preserve native COM identity and close device-discovery paths deliberately. The preferred audit direction is to observe/intercept device creation on registered native factories using the safe hook infrastructure from Phase 2. Do not introduce a full `IDirect3DDevice9` wrapper merely to avoid the current hook loss unless its COM identity, `QueryInterface`, `GetDirect3D` and lifetime semantics are explicitly proven.

### A4 — Capture, VR synchronization and compositor submission are coupled inside the game callback

**Severity:** P0  
**Status:** open

The flat bridge currently performs D3D9 readback, CPU access, D3D11 upload, pose wait and both eye submissions synchronously from the render callback. This couples the engine render cadence to the VR compositor cadence and makes stage stalls hard to distinguish.

The fact that the first few calls completed does not make the architecture suitable for sustained operation.

**Required action:** split D3D9 capture from OpenVR presentation using an owned-frame handoff/mailbox. D3D9 calls remain on the game thread; D3D11/OpenVR presentation has a single explicit owner. Track capture sequence separately from submit sequence so repeated presentation of the last image never counts as new game rendering.

### A5 — Renderer resources are keyed by image description rather than device/generation ownership

**Severity:** P1  
**Status:** open

The current bridge can reuse resources when dimensions/format/MSAA match even if the underlying D3D9 device or generation has changed. Reset/device loss, reentrancy, D3D11-context ownership and teardown are not modeled strongly enough.

**Required action:** introduce explicit device/generation identity, resource invalidation on Reset/recreation, bounded ownership, RAII for mapped/locked resources and shutdown outside `DllMain`.

### A6 — Host tests do not yet prove the deployed pipeline

**Severity:** P0  
**Status:** partially resolved

The device-hook test has improved since the original audit and now exercises five `BeginScene`/`EndScene`/`Present` cycles plus continuity checks.

Remaining gaps include:

- the proxy smoke path still primarily exercises `Clear -> Present -> Reset` and does not prove the actual EndScene-driven OpenVR path;
- no registered test validates the complete flat OpenVR pipeline with controlled doubles/mocks;
- HAL-device unavailability is still represented as process success rather than a distinct test skip state;
- tests do not yet cover patch conflicts, protection failures, multiple vtables/devices, partial rollback, device recreation or stage stalls;
- test binaries can still risk loading the project proxy when the intent is native system D3D9 unless the loaded module path is asserted;
- shared log filenames can make parallel tests interfere;
- readback validation remains too narrow for orientation/stride/temporal-change guarantees.

**Required action:** complete remediation Phase 1 before interpreting future host-suite success as pipeline evidence.

### A7 — Live verification is not tied to a unique execution

**Severity:** P1  
**Status:** partially resolved

The current verifier now checks the expected staged proxy hash and staging state and extracts stronger counters than the original version.

It still lacks a durable run identity binding together:

- source snapshot/build manifest;
- deployed artifact hash and diagnostic mode;
- PID and process start time;
- device/generation identity;
- run duration and finalization state;
- unique captures versus repeated submissions;
- physical headset confirmation.

**Required action:** introduce `RunEvidence` and `run_id` through build/deployment/runtime/verifier tooling. Old logs must be incapable of validating a new artifact.

### A8 — Deployment is reversible only on the happy path, not transactional

**Severity:** P1  
**Status:** open

Current staging/unstaging does not provide a complete journal-before-mutation transaction. It can mutate files before all validation is complete, removes/rotates evidence ad hoc, does not consistently reject active game processes, and does not fully prove the architecture/mode of an arbitrary supplied proxy.

**Required action:** implement preflight, journal, temporary install, hash verification, controlled replacement, recoverable partial failure and evidence preservation as described in remediation Phase 7.

### A9 — Declared architectural separation does not match current build dependencies

**Severity:** P1  
**Status:** open

The neutral runtime target currently contains OpenXR runtime code and links `openxr_loader` publicly. The OpenVR path depends on that runtime, so configuring/building OpenVR also requires the experimental OpenXR dependency. CI bootstraps OpenXR but does not currently bootstrap the pinned OpenVR SDK before CMake configuration.

The executable identity/catalog code also remains in the runtime target rather than a clearly separated game/build-identity layer.

**Required action:** split neutral runtime, game/build identity and OpenVR/OpenXR adapters. Make backends independently selectable and prepare each enabled dependency in CI.

### A10 — Runtime/lifetime/error contracts are not yet recovery-safe

**Severity:** P1  
**Status:** open

OpenVR/OpenXR ownership, state transitions, move semantics, `noexcept` boundaries, global runtime ownership and error reporting are not modeled robustly enough for sustained/recoverable operation.

**Required action:** formalize runtime states, focus/tracking/connection state, one-process ownership, event handling, error propagation, teardown and move/lifetime rules in remediation Phase 6.

### A11 — Neutral math contracts are ambiguous before camera work begins

**Severity:** P2  
**Status:** open

`EyeView::pose` is not guaranteed to mean the same space/transform across OpenVR and OpenXR. FOV/projection conventions and matrix validity are not sufficiently proven for asymmetric stereo projection.

These issues do not explain the current flat bridge because it does not yet construct game stereo cameras, but they would become dangerous immediately afterward.

**Required action:** complete remediation Phase 8 before camera/stereo implementation is promoted.

### A12 — Documentation is improved, but provenance from source to run is still incomplete

**Severity:** P1  
**Status:** partially resolved

README, architecture, roadmap, research notes and handoff were refreshed after the original audit. The remaining problem is not prose freshness alone; it is the lack of an automatic provenance chain:

`sources -> build -> package -> deployed files -> process/run -> evidence`

Documentation must also avoid wording that turns the observed three-frame hook loss into a fully proven sole root cause of the blank headset.

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
| The project stops observing the active render path | confirmed hook-integrity loss in the current device-vtable path | factory/device/generation coverage and owner of replacement targets |
| A bridge stage ceases to progress | first frames completed; sustained operation not proven | per-stage entry/exit, timing and independent presenter progress |
| OpenVR does not receive useful new work at the required cadence | initial submissions accepted; headset image not validated | presenter timing, unique-content sequencing and compositor/runtime state |
| Captured image is not always the final useful game image | capture is tied to one chosen callback | render-sequence evidence plus changing asymmetric frame patterns |
| Focus/tracking/runtime state changes during the relevant window | runtime state exists but is weakly correlated to game events | timestamped runtime-state events bound to the same run |

There is currently no basis to blame PSVR2 hardware, abandon OpenVR, reactivate D3D9Ex as the main path or start game-camera hooks.

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