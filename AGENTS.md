# Call of Juarez VR — Agent instructions

Read this file before changing code. Then read, in order:

1. `docs/TECHNICAL_AUDIT.md`
2. `docs/AUDIT_REMEDIATION_PLAN.md`
3. `docs/internal/CODEX_HANDOFF.md`
4. `docs/VALIDATION.md`
5. `ARCHITECTURE.md`
6. `ROADMAP.md`

Do not assume a documented task is still pending without inspecting the current repository state, but do not mark an audit finding resolved merely because code exists. The acceptance criteria in the remediation plan control promotion.

## Core rule

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable VR contracts. Do not generalize a Chrome Engine behavior until at least one second game demonstrates the same boundary.

The supported-game product target is native stereo rendering, full-body IK and interactions
rebuilt for VR. Treat these as downstream milestones governed by the active validation
gates; do not reduce the end goal to a flat headset bridge or controller remapping.

Reuse proven game-neutral policy from the Penumbra VR Framework when the semantics match: tracking spaces, recenter/calibration policy, renderer-neutral eye data, logical input, haptics, validation states and separation between runtime policy and per-game/native adapters. Do not copy HPL-specific layouts, addresses, hooks or source assumptions.

## Ownership

The shared runtime owns game-neutral VR concepts and policy.

Renderer backends own D3D9/D3D10 device/frame ownership, capture/transport, render targets and compositor integration.

Game backends own camera/player/weapon/UI/physics knowledge for a specific game and build. Exact binary details must never leak into shared runtime code.

Diagnostics/evidence infrastructure owns hook integrity, factory/device/generation identity, structured run telemetry and source/build/deployment/run correlation.

## Build identity

Binary integration is exact-build first. Filename recognition is diagnostic only. Use SHA-256 to decide whether a build is known. A known binary is not automatically a supported VR integration. Unknown builds must fail closed for game-specific modifications while generic diagnostics may continue only where safe.

## Validation states

Keep these states distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A successful build or synthetic test does not imply a live-game or headset test.

## Current scope

Audit-remediation Phases 0-4 remain the established stabilization baseline. The
Call of Juarez camera-path gate has passed live validation:
`Camera -> View/Projection -> ChromeEngine3 renderer`, external FOV control and clean
restoration are proven in the exact game build. Subsequent HMD runs established the paired
`+0x44` world/source and `+0x04` inverse/view contract, the native right/up/forward basis,
stable scene visibility with the corrected right axis, and a remaining game-specific yaw
sign inversion. The current source corrects that yaw sign and also contains the first
native-stereo candidate: two ChromeEngine render-view passes per frame with distinct
eye-to-head translations, asymmetric per-eye projection and backend-neutral OpenVR eye data.
Run `20260916T113600Z-native-stereo` reached HMD-driven visual-camera motion but exposed two
candidate defects before stereo submission: OpenVR raw vertical projection signs were mapped
incorrectly into neutral `EyeFov`, and source/frustum state was restored before later scene/
visibility work in `0x00030E00`. Both were corrected. The next one-process run,
`20260916T123049Z-8d977bb5b439`, reached visible in-game headset submission after gameplay
loaded, but every sampled submitted pair had identical left/right pixel hashes. The old
transport was reading the swap-chain backbuffer from inside the render-view boundary rather
than the currently bound D3D9 render target, so it did not prove capture of the two engine eye
results. That run also stopped finalization after restoring the factory hook, before OpenVR
shutdown and `run_end`. The next active-render-target run,
`20260916T130852Z-1438628c90c6`, confirmed correct HMD yaw/pitch direction but remained flat:
left-eye RT0 was a real `D3DFMT_A8R8G8B8` backbuffer while every right-eye RT0 was
`D3DFMT_NULL`, so no stereo pair was submitted. Exact-build disassembly shows why: the failed
candidate ran the left eye through full wrapper `0x30FB0` but the right eye through core-only
`0x30E00`; `0x30FB0` also owns the `view+0xD7` rendered guard and post-core work. Run
`20260916T133322Z-36c287cc43d8` then live-tested the complete-wrapper path in one process:
both eye passes reached a real `2560x1440` `D3DFMT_A8R8G8B8` RT0, left/right hashes were
distinct, renderer-camera correlation was true for both eyes and OpenVR received the stereo
pair. The user observed binocular gameplay, correct yaw/pitch direction, head-height camera
placement and no obvious missing scene geometry, but fusion/comfort and frame pacing were poor.
The run again ended without `native_stereo_runtime: stopped`/`run_end`.

Static game data now closes a previously unproven unit contract:
`Data/Player/PlayerProperties.def` documents movement in `cm/s` and acceleration in `cm/s^2`,
while the shared XR runtime is in metres. The live candidate had therefore applied OpenVR
eye-to-head metres directly as Chrome Engine centimetres, shrinking the physical stereo
baseline by 100x. Current source performs the metres->centimetres conversion only in the Call
of Juarez adapter, records applied eye positions, frustum and D3D9 viewport state, and emits
shutdown stage markers around readback/OpenVR teardown. Fresh Debug and Release host suites
pass 18 tests plus one expected capability SKIP. A minimal game-neutral OpenVR global action
seam is also host-tested: left PS VR2 Sense Create maps to `/actions/global/in/recenter`, and
the logical press edge feeds the existing XR-neutral `RelativePoseTracker` recenter path. The
action manifest and binding are provenance-bound staged artifacts. Run
`20260916T153109Z-8976b8f77775` subsequently live-observed the corrected `100` units/metre eye
baseline and physically validated left-Sense-Create recenter. Visual quality/frame pacing was
still very poor, the flat game menu was not presented in-headset, and shutdown again stopped at
`native_stereo_shutdown: stage=runtime_begin`. Later host-only hardening changes remain
host-tested until another physical run exercises them.
The first deferred-presenter observation reused run `20260916T215746Z-362752fe6362` across two
process starts and therefore cannot promote the gate, but it showed sustained distinct-eye OpenVR
submission, clean runtime shutdown/run_end and substantially improved perceived gameplay once the
user toggled the CoJ Escape menu. SteamVR's dashboard remained stuck over the scene. SteamVR logs
showed the presenter captured scene focus through `WaitGetPoses` tens of seconds before the first
scene texture existed. Current source keeps pre-scene tracking on non-blocking OpenVR system poses,
enters compositor pacing only after a real stereo frame is ready, calls `PostPresentHandoff`
explicitly and records scene/dashboard focus telemetry. Fresh formal run
`20260916T221254Z-861f3c15abd4` physically validated that lifecycle correction: scene focus moved
to the CoJ PID on first submit, the SteamVR dashboard no longer remained stuck over gameplay, and
shutdown reached `native_stereo_runtime: status=stopped` plus `run_end`. The user reported improved
perceived performance, but strong head-turn ghosting/elastic reprojection remained.

Current source addresses that remaining defect by carrying the exact HMD pose and pose sequence
used to render each stereo frame through the D3D9 ring/mailbox and submitting both new and repeated
textures with OpenVR `VRTextureWithPose_t` / `Submit_TextureWithPose`. Telemetry records
`render_pose_sequence` and `pose_mode=explicit_render_pose`, and frames lacking a valid exact render
pose fail closed before submission. Run `20260916T224239Z-e43b46698e5c` physically validated that
path: slow/fast head turns and mouse rotation no longer produced the previous backward pull or
snap-back, and perceived comfort improved substantially. Remaining work is frame pacing. That run
showed roughly `15-22 ms` of CPU copy plus `11-14 ms` of diagnostic hashing on sampled new frames.
Current source keeps RGB eye-distinction fail-closed on every frame, moves full hashes to sampled
telemetry only and uses a bulk memcpy for contiguous D3D9 locks. Those performance corrections are
host-tested only; positional 6DOF remains downstream of this presentation gate.
The proof remains game/build-specific and must not be generalized to another Chrome Engine
title without independent evidence.

Already established evidence includes:

- forwarding/bootstrap and native D3D9 observation have worked in the exact game build;
- classic-D3D9 CPU readback -> D3D11 upload has worked in the exact game build;
- OpenVR/SteamVR initialization, valid PSVR2 HMD pose acquisition and synthetic D3D11 submission have worked;
- the in-game flat bridge has completed genuine captured-frame submissions;
- one later diagnostic observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks followed by loss of integrity of the installed D3D9 device-vtable entries while monitor rendering continued.

Treat that last point as a confirmed failure mode of the historical frame-hook interception design, not as a complete root-cause explanation for the blank headset. Audit-remediation Phases 0-4 now provide host-tested run provenance, device/generation coverage, safe hook ownership and structured render telemetry. Two run-bound manual observations reproduced the three-frame device-hook loss; the second proved that all four lost slots return to their recorded Windows D3D9 originals. Phase 5 capture/presentation decoupling is implemented, host-tested and exercised by later live native-stereo runs. Its remaining plan-level work is acceptance coverage for reset/resize/new-device and controlled paused-producer behavior, plus the active frame-pacing gate.

## Required execution order

Follow `docs/AUDIT_REMEDIATION_PLAN.md`.

Critical path before another manual game-observation run:

1. Phase 0 — auditable source/build/deployment/run provenance.
2. Phase 1 — build, CI and host-test validity.
3. Phase 2 — safe vtable patching and `HookRegistry` ownership.
4. Phase 3 — native factory/device discovery without split COM identity.
5. Phase 4 — structured run/render telemetry.

Do not ask for another headset test to validate Phases 0-4 or the camera-boundary proof.
The two post-remediation observations and the unresolved Steam Overlay A/B remain valid
evidence/work, but the user has explicitly deferred that repeat run. The exact-build
camera-control probe described in `docs/research/COJ_CAMERA_PATH.md` has passed its manual
DX9 gameplay gate with matching run-bound telemetry.

The combined HMD/native-stereo implementation may proceed through exact candidate build,
host tests, transactional staging and verifier preparation without launching the game or
SteamVR. The next manual gate must use one fresh run ID for exactly one game-process start;
provenance rejects reused run IDs. It is one `d3d9_native_stereo` run proving valid HMD pose,
corrected continuous yaw/pitch motion, stable scene/model placement, two complete `0x30FB0`
render-view passes with restored `view+0xD7`, distinct left/right ChromeEngine eye renders
captured from real color render targets with asymmetric projection, compositor submission,
disable-to-natural passthrough and clean hook/runtime shutdown. Distinct-eye rendering, corrected
scale, left-Sense-Create recenter, scene-focus handoff, explicit render-pose submission and clean
finalization already have live evidence; the remaining gate is usable presentation/frame pacing
without regressing those contracts. A NULL capture target or an
identical left/right pair must fail closed before OpenVR submission. The source matrix contract is right/up/forward
at `+0x44`; never reconstruct that first axis as `forward x up` (left). Run
`20260916T104036Z-24b3e3010d4c` proved that the corrected right-handed basis fixes the
mirrored character/scene failure but same-sign HMD yaw still moves the visible camera in the
opposite horizontal direction. The exact game adapter therefore negates physical yaw while
keeping the live-confirmed pitch direction; roll remains excluded. Stereo passes keep source
world/view, frustum and derived camera state active for the complete render-view pass and
restore the complete natural snapshot transactionally after each eye. The first proof may use
the provisional classic-D3D9 CPU readback -> two D3D11 textures transport. The candidate
must not depend on `Present`, `BeginScene`, `EndScene` or `Reset` hooks.

## Explicit prohibitions during stabilization

- Camera hooks, HMD orientation injection and the current native-stereo render-view proof
  must remain in the exact Call of Juarez game integration. Do not extend the current gate
  into positional 6DOF, UI adaptation, full-body IK or motion controls yet.
- Do not reactivate D3D9Ex as the primary game path without new evidence.
- Do not use a blind periodic re-hook loop as the default fix.
- Do not introduce a full `IDirect3DDevice9` wrapper solely to avoid current hook replacement unless COM identity, `QueryInterface`, `GetDirect3D`, lifetime and discovery semantics are explicitly validated and evidence justifies the design.
- Do not equate OpenVR submit count with unique game-frame count.
- Do not interpret historical logs as evidence for a new artifact/run.
- Do not preserve an existing abstraction if the audit demonstrates that its ownership or testability is fundamentally wrong.

The historical `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` diagnostic remains useful baseline evidence because it can report replacement-slot ownership. Preserve it, but do not let it bypass the remediation order.

The D3D10 path for Call of Juarez remains a first-class later target because it has visible rendering improvements. D3D9 is first because it is the common renderer surface shared by all three inspected games.

## Runtime direction

OpenVR -> SteamVR is the primary PSVR2 path. A minimal left-Sense-Create global recenter action
is headset-validated for the current camera gate; full tracked-controller/gameplay input remains
the later input milestone. Keep input abstractions logical and controller-independent.

Preserve OpenXR as an experimental/future backend, but it must be independently selectable and must not be required merely to configure/build the OpenVR path.

## Manual runtime validation

Never launch Call of Juarez or SteamVR automatically. The user performs all game and SteamVR launches manually.

For repeated HMD-camera testing, `tools/vr_test.ps1` is the user-facing front door. The
user may run `prepare`, `recenter`, `disable`, `status` and `finish` without waiting for an
agent. Normal in-headset recenter uses left PS VR2 Sense Create; the terminal `recenter` action
is a diagnostic fallback. `prepare` must still build/test, create exact provenance and stage the current
candidate; it never launches SteamVR or the game.

Do not stop merely because a game launch is eventually required. Advance implementation, host tests, exact artifact build, deployment preparation and verifier work as far as the current gate allows.

Before requesting any manual run:

1. identify the exact source/build manifest;
2. build the exact candidate from current sources;
3. pass the phase-specific host acceptance criteria;
4. prepare reversible/transactional staging appropriate to the current phase;
5. prepare the post-run verifier and expected evidence;
6. record the candidate/run identity.

Stop only when the remaining evidence genuinely requires the user's manual game/SteamVR launch or physical headset/controller confirmation.

## Before editing

1. Inspect `git status`, HEAD and the relevant owning files.
2. Read the current audit, remediation phase and handoff.
3. Make the smallest coherent change that advances the active phase.
4. Add/strengthen success and failure-path tests.
5. Build the relevant Debug/Release targets from current sources.
6. Update `docs/TECHNICAL_AUDIT.md` only when evidence changes a finding status.
7. Update `docs/VALIDATION.md` with actual evidence, never intended behavior.
8. Update `docs/internal/CODEX_HANDOFF.md` before stopping.
