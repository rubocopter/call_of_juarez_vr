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

The current code change replaces native-stereo CPU readback with a host-tested
D3D9Ex shared-texture ring. The next headset run should validate only that
transport boundary before combining it with unrelated product work. It must
preserve all baseline contracts and demonstrate a stable D3D9Ex game device,
distinct paired eyes, exact pose ownership, nonblocking producer/consumer
handoff, increased cadence and normal shutdown.

After that gate, remaining product boundaries are:

1. replace or isolate the current flat-menu pointer ownership model; the sole Win32 `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE` route is now physically rejected for control quality;
2. Cross accept, Circle back through normal Escape semantics from both menu and gameplay, and L2/R2 ray-select without layer loss, freeze or flat-mode lockup;
3. reliable startup skip, paused-hint handling without JNI exceptions and `GameUILoading.OnInputKey` continuation from Sense;
4. visible subtitles after enabling the shipped setting; latest evidence already established `Settings.bSubtitles=false`;
5. level horizon after recenter and no pitch/roll baked into tracking reference;
6. retain horizontal-only visual-body compensation while making physical horizontal displacement drive the same class of visual walk animation as stick locomotion;
7. make arm ownership continuous across ordinary tracked motion instead of visibly snapping to native/default poses whenever safety denies a write;
8. restore a temporary controller-tip diagnostic ray, compare it directly with visible weapon/barrel geometry, and trace the remaining muzzle/ballistic mismatch; local `-Z` is already strongly confirmed by physical geometry;
9. flat/native presentation transitions and shutdown/finalization.

The superseded 1920x1080-per-eye classic-D3D9 CPU path measured about 7-7.5 ms median and 9-9.5 ms p95 readback/copy cost. Physical comparison then showed `139.949 Hz` vanilla, `83.473 Hz` with the old full VR transport and `138.117 Hz` with both eyes active but readback/copy disabled. This closes locomotion/input/physics investigation and makes the transport gate causal rather than speculative. OFXR-Bridge remains future OpenXR/frame-generation research and is not a remediation for the active OpenVR path.

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
