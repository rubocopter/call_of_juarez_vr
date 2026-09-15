# Audit remediation plan

This plan turns the findings in `docs/TECHNICAL_AUDIT.md` into an ordered implementation program. It is the default execution order for Codex and human work during the current stabilization phase.

Do not skip phases because a later change appears more likely to fix the headset. The purpose of this plan is to make each subsequent runtime test discriminating and reproducible.

## Phase 0 — Freeze an auditable baseline

**Goal:** make every future result traceable from source snapshot to deployed artifact and run evidence.

### Work

- Inventory tracked/untracked state before changing code.
- Record repository HEAD and dirty state.
- Add a build/run manifest containing source identity and hashes of relevant artifacts.
- Preserve existing logs instead of deleting them during staging.
- Record exact identities for `CoJ.exe`, `ChromeEngine3.dll`, proxy DLL, `openvr_api.dll` and diagnostic mode.
- Separate "known binary" from "supported integration" terminology.
- Add a run-evidence collector that can package manifest, stage state and produced logs without launching the game.

Known exact identities currently used by the project:

- `CoJ.exe`: `5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`
- inspected `ChromeEngine3.dll`: `DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`

### Acceptance

Another checkout can identify and rebuild the same source snapshot, and any manual run can be tied to one exact build/package/deployment manifest.

## Phase 1 — Repair build and host-test validity

**Goal:** make a clean host build/test result meaningful before interpreting new runtime evidence.

### Work

- Separate neutral runtime, build/game identity and OpenVR/OpenXR adapters.
- Allow OpenXR to be disabled without blocking OpenVR.
- Bootstrap every enabled dependency in CI; in particular, CI must prepare pinned OpenVR when OpenVR targets are enabled.
- Require Win32/x86 explicitly for the proxy/game-integration artifacts.
- Ensure native D3D9 tests load the system runtime intentionally and assert the loaded module path.
- Keep proxy-loading tests separate from native-D3D9 tests.
- Use isolated log/temp directories per test.
- Represent unavailable HAL/runtime prerequisites as `SKIP`, not positive evidence.
- Update proxy smoke coverage so the active `BeginScene`/`EndScene` path is actually exercised where relevant.
- Rebuild all targets from current sources before CTest.
- Add regression coverage for changing readback patterns, orientation/stride and temporal updates.

### Acceptance

- clean Debug and Release builds from a fresh checkout;
- CI can configure from zero with only enabled dependencies;
- host tests are bound to freshly built artifacts;
- no native test can accidentally use the local project proxy;
- PASS/FAIL/SKIP semantics are explicit.

## Phase 2 — Replace ad-hoc vtable patching with safe hook infrastructure

**Goal:** make hook installation, ownership, rollback and integrity deterministic.

### Components

- `VtablePatch`
- `HookRegistry`
- per-vtable/per-device records

### Patch contract

A patch operation must distinguish at least:

```text
no modification
applied
protection failure
conflict / unexpected current target
rollback incomplete
```

### Requirements

- conditional compare/exchange semantics rather than blind overwrite;
- retain original targets while any installed wrapper may still be reachable;
- roll back every entry that was actually modified;
- restore only entries still owned by this project;
- support more than one vtable/device;
- define synchronization/reentrancy policy;
- verify integrity after installation;
- never silently reinstall over another component's hook;
- ensure the module remains loaded while patched entries reference it.

Apply the same safety model to any swapchain hook that remains necessary.

### Required tests

- failure before replacement;
- failure after replacement/protection change;
- conflicting existing hook;
- second vtable/device;
- reinstall attempt;
- partial rollback;
- callback during transition;
- native HRESULT returned unchanged.

### Acceptance

No failed installation path can leave a vtable entry targeting project code without a valid retained original and ownership record.

## Phase 3 — Close factory/device discovery and COM-identity gaps

**Goal:** know which factory/device/swapchain actually owns rendering without creating split COM identity.

### Direction

Prefer preserving native D3D9 COM objects and intercepting device creation through registered native factories using the Phase 2 hook infrastructure.

A full `IDirect3DDevice9` forwarding wrapper is not the default solution. It may be considered only if evidence requires it and COM identity/lifetime behavior is explicitly validated.

### Work

- observe device creation from every registered/reachable factory;
- verify `GetDirect3D` round-trips and factory identity;
- assign stable diagnostic IDs to factory, device and swapchain instances;
- record creation thread and device generation;
- record module ownership of intercepted/replacement function targets;
- detect new devices and device generations after Reset/recreation;
- isolate the D3D9Ex substitution as a laboratory artifact with no accidental activation path in the classic runtime.

### Acceptance

Creating a device through a factory recovered from another native D3D9 object remains observable, and COM identity relationships match native behavior.

Do not add hooks for unused interfaces without evidence that they are part of the live render path.

## Phase 4 — Build structured render/run observability

**Goal:** a single run must be sufficient to distinguish hook loss, stage failure, stage stall, device change and lack of new content.

### Minimum events

```text
run_start
build/deployment_identified
device_created
generation_changed
hook_installed
hook_conflict
hook_integrity_lost
callback_enter
callback_exit
capture_begin
capture_end
frame_published
upload_begin
upload_end
wait_poses_begin
wait_poses_end
submit_left
submit_right
runtime_state_changed
periodic_summary
run_end
```

### Minimum fields

- `run_id`
- source/build manifest identity
- PID/TID
- monotonic timestamp
- factory/device/swapchain ID
- device generation
- callback/capture/content/upload/submit sequence
- stage duration
- HRESULT/runtime result

### Rules

- exact counters, with detailed per-event sampling bounded to avoid excessive logging;
- periodic summary generated outside critical draw calls where possible;
- every blocking stage records entry before work and exit after work;
- absence of final summary marks the run incomplete;
- `Present`, `EndScene` and swapchain activity are measured before any is discarded;
- `BeginScene`/`Clear`/draw probes may establish activity but must not perform VR submission per draw;
- a repeated OpenVR texture increments submit sequence but not capture/content sequence.

### Acceptance

One run log can answer: which device rendered, whether hooks remained owned, how many callbacks occurred, how many new frames were captured, where progress stopped, and whether OpenVR continued presenting after capture stopped.

Only after this phase should another manual game observation run be requested.

## Phase 5 — Separate D3D9 capture from OpenVR presentation

**Goal:** decouple the Chrome Engine render clock from the SteamVR compositor clock while preserving correct D3D9 thread ownership.

### Components

- `D3D9Capture`
- `FrameMailbox`
- `OpenVrPresenter`

### Frame contract

```text
Frame
  device_id
  generation
  capture_sequence
  capture_time
  width / height
  stride
  explicit pixel format
  owned pixel storage
```

### Requirements

- all D3D9 calls remain on the correct game/render thread;
- resources are keyed by device and generation;
- no D3D9 COM resource crosses to the presenter thread;
- locked/mapped resources use RAII;
- use a canonical explicit pixel format and alpha/X-channel policy;
- bounded small mailbox; stale pending frames may be replaced rather than blocking the producer indefinitely;
- one owner for D3D11 immediate context and OpenVR;
- Reset/recreation invalidates the old generation;
- presenter rejects stale frames;
- repeated last-frame presentation is identified as repetition;
- teardown occurs outside `DllMain` and cannot race callbacks/presenter work.

### Required tests

- asymmetric image patterns;
- content changing every frame;
- nontrivial stride;
- resize;
- Reset;
- new device with identical dimensions/format;
- stopped producer with active presenter;
- slow consumer;
- bounded memory use.

### Acceptance

The presenter can remain alive and measurable when capture pauses, while logs clearly show that no new game frame was produced.

## Phase 6 — Formalize OpenVR ownership, state and synchronization

**Goal:** make sustained compositor behavior and failure recovery observable and deterministic.

### Work

- separate runtime initialized/connected/focused/tracking-valid/presenting/shutdown states;
- process relevant runtime events;
- define one OpenVR initialization owner per process;
- record per-eye submit results and available compositor timing/timeout information;
- review move semantics and ownership of native handles;
- review `noexcept` boundaries so allocation/reporting failures cannot unexpectedly terminate the process;
- define explicit D3D11 upload/synchronization/handoff order;
- compare GPU synchronization strategies through controlled tests instead of adding unconditional global waits;
- make the visible OpenVR probe duration configurable or manually stoppable and use a recognizable animated pattern/counter.

### Acceptance

Host/simulated tests cover focus loss, one-eye submit failure, invalid tracking, runtime disconnect and shutdown; the physical probe produces interpretable run evidence when manually executed.

## Phase 7 — Make deployment and verification transactional

**Goal:** guarantee recoverability and prevent stale evidence from validating a new build.

### Work

- complete preflight before mutating the game directory;
- reject active game processes;
- validate executable build, proxy architecture/mode/hash and dependency hashes;
- write a recovery journal before the first mutation;
- install temporary files and verify them before controlled replacement;
- mark staging complete only after every operation succeeds;
- recover interrupted staging/unstaging from the journal;
- preserve logs/evidence under run-specific paths;
- bind post-run verification to `run_id`, manifest and deployed hashes.

### Required tests

- clean installation;
- pre-existing original DLL;
- missing backup;
- externally changed file;
- failure after each transaction step;
- repeated stage/unstage;
- active game process.

### Acceptance

Any interruption either leaves originals intact or provides an unambiguous recovery path; an old log cannot pass verification for a new candidate.

## Phase 8 — Lock neutral math contracts before real stereo cameras

**Goal:** ensure game-neutral VR transforms mean the same thing across runtime backends before camera integration.

### Work

- distinguish eye-to-head transform from an eye pose in tracking/reference space;
- document meters, coordinate axes, handedness and transform-composition convention;
- verify asymmetric FOV conversion against reference projection matrices;
- reject non-finite/invalid transforms;
- keep OpenXR isolated until its handle/lifetime ownership is corrected.

### Acceptance

The same neutral field has one documented semantic meaning for every producer and consumer, with tests covering asymmetric stereo projection.

## Execution order

Default critical path:

```text
Phase 0
  -> Phase 1
  -> Phase 2
  -> Phase 3
  -> Phase 4
  -> manual game observation gate
  -> Phases 5 and 6
  -> Phase 7
  -> flat integration validation
  -> camera/stereo work
```

Phase 8 may progress through host tests in parallel but must be complete before real stereo camera work is promoted.

### Camera/render boundary proof — live-tested

The user explicitly deferred another Steam-Overlay-disabled repetition of the Phase 0-4
observation gate in favor of a more discriminating engine-boundary experiment. Call of
Juarez (2006) has now passed this narrow exact-build diagnostic gate:

```text
exact CoJ.exe + exact ChromeEngine3.dll
  -> CBaseCamera render update / FOV boundary
  -> view + projection matrix update
  -> ChromeEngine3 renderer camera ownership
  -> externally commanded FOV/yaw/pitch proof
```

Run `20260915T150554Z-7e0d7da45949` proved visible external FOV/yaw/pitch control,
renderer-camera correlation, natural-basis restoration, disabled passthrough and clean
hook restoration. The follow-on HMD-rotation implementation now host-tests an explicit
neutral pose/recenter boundary and an OpenVR-backed `d3d9_hmd_camera` candidate, but it
does not promote stereo, positional 6DOF or motion controls. The Steam Overlay A/B remains unresolved evidence for the D3D9 interception
finding and may be resumed later if presentation work returns to that boundary. Exact
static/live evidence and constraints are recorded in `docs/research/COJ_CAMERA_PATH.md`.

## Validation gates

| Gate | Required evidence | User action |
| --- | --- | --- |
| Host | hooks/resources/states/transfer pass controlled and failure-injection tests | none |
| Camera boundary | exact engine profile, camera-vtable ownership, external FOV/orientation command, renderer-camera correlation and clean restore | one manual game launch; headset/SteamVR unnecessary |
| Monocular HMD rotation | valid HMD pose, recenter, continuous 1:1 yaw/pitch monitor-camera motion, no accumulation, disable passthrough, renderer-camera correlation and clean restore | manual SteamVR + game + physical HMD motion |
| Game observation | sustained callback/device/generation coverage with structured evidence | manual game launch; headset unnecessary |
| Game capture | changing images, correct orientation/stride/content | manual game launch; headset unnecessary |
| Isolated compositor | sustained timing/submission and recognizable visible pattern | manual SteamVR + physical headset confirmation |
| Flat integration | changing gameplay visible in headset across menus/loading/gameplay | physical test |
| Stereo/tracking | game-camera projections and tracking ownership validated | later physical phase |

## Stop conditions for agents

An agent should stop and hand off only when the next evidence genuinely requires a user-launched game, SteamVR or physical headset/controller validation. Before stopping, it must prepare the exact artifact, staging path, verifier and evidence expectations.

Do not ask for another headset test merely because code changed. Reach the relevant gate first.
