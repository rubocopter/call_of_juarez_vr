# Audit remediation plan — maintained baseline

The original audit remediation phases are no longer an implementation backlog. Their accepted results form the stabilization baseline for current product work. This file keeps the acceptance contracts agents must preserve.

## Phase status

| Phase | Contract | State |
| --- | --- | --- |
| 0 | auditable source/build/deploy/run provenance | host-tested and used by physical workflow |
| 1 | trustworthy Win32 build and host-test baseline | host-tested; current Debug/Release suites green |
| 2 | safe hook ownership/restoration | host-tested and physically exercised |
| 3 | factory/device/generation identity | host-tested and physically exercised |
| 4 | structured run/render telemetry | host-tested and physically exercised |
| 5 | capture/presenter separation | host-tested and physically exercised |
| 6 | OpenVR ownership/state/synchronization | physically exercised; inner presenter finalization remains open |
| 7 | transactional stage/verify/restore workflow | host-tested and used for physical runs |
| 8 | neutral VR math/unit contracts | host-tested and physically exercised |

## Non-regression acceptance

### Provenance

Every physical candidate must bind:

- source commit and dirty state;
- exact supported-game binary identity;
- build manifest;
- deployed proxy hash;
- unique run ID;
- runtime log/evidence result;
- staging/restoration state.

A run ID reused by a second game process is diagnostic only.

### Hook/device ownership

Hooks must know the object/generation they own, preserve foreign hooks, restore only their own mutations and fail closed on ambiguous ownership. Reset/new-device paths must invalidate/recreate resources safely.

### Capture/presentation

Game render callbacks produce/capture frames. The presenter owns compositor cadence, repeat behavior, scene focus and OpenVR submission. Repeated frames carry the exact render pose associated with their image; frames without valid pose ownership fail closed.

Classic D3D9 reset must not depend on persistent default-pool flat-capture resources.

### Camera/math

The game camera is authoritative. VR state is applied transiently around the render path and restored. Use the exact source right/up/forward basis. Convert XR metres to Call of Juarez centimetres only in the exact-game adapter.

### Deployment

`tools/vr_test.ps1 prepare` is the physical-test front door. It must build/test, create provenance and stage a candidate transactionally. `finish` must collect/verify available evidence and restore staging/video settings even when the physical gate fails.

SteamVR and Call of Juarez are launched and closed manually.

## Current product-remediation gate

The next candidate must preserve all baseline contracts while physically validating the follow-up work after multiprocess diagnostic rejection `20260920T214629Z-351c27f434d5`:

1. controller-origin flat-menu beam plus real hover through the Win32 mouse path, while the logical `UICursorGame` remains synchronized;
2. Cross accept, Circle back through normal Escape semantics and L2/R2 ray-select without layer loss, freeze or flat-mode lockup;
3. reliable startup skip, paused-hint handling without JNI exceptions and `GameUILoading.OnInputKey` continuation from Sense;
4. visible subtitles, after distinguishing `Settings.bSubtitles` state from UI/presentation loss;
5. level horizon after recenter and no pitch/roll baked into tracking reference;
6. smooth locomotion/jump while separating native movement semantics from measured classic-D3D9 CPU-readback stutter;
7. physical crouch with first-person local-mesh ownership preserved even though physical crouch does not press the native crouch action;
8. stable body-arm writer/restoration with plausible hand orientation and reload behavior; do not lengthen bones without measured justification;
9. repeated-fire stability plus controller-owned visual/ballistic origin and direction checks;
10. flat/native presentation transitions and shutdown/finalization.

The active 1920x1080-per-eye classic-D3D9 CPU path measured 7.432 ms median and 8.994 ms p95 copy time in the latest gameplay process while SteamVR recommended 3400x3468. Removing/reducing GPU->CPU transport cost takes priority over a large resolution increase. OFXR-Bridge remains future OpenXR/frame-generation research and is not a remediation for the active OpenVR readback path.

Passing host tests is required before this gate, but cannot promote it.

## Promotion rules

- `implemented`: code exists.
- `host-tested`: relevant host tests/builds pass.
- `live-tested`: exact game/runtime path executed with correlated evidence.
- `headset-validated`: user-observed HMD/controller behavior satisfies the acceptance gesture.
- `supported`: feature is intentionally shipped and covered by the supported product contract.

Do not promote from documentation, static bytecode/disassembly, synthetic tests or an invalid multiprocess run alone.

## Stop conditions

Fail closed and diagnose instead of continuing game-specific mutation when exact build identity, object/generation identity, camera basis, tracked pose validity, restoration or provenance is ambiguous.
