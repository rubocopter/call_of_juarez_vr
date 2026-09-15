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

The current objective is **audit-driven stabilization**, not new VR feature work.

Already established evidence includes:

- forwarding/bootstrap and native D3D9 observation have worked in the exact game build;
- classic-D3D9 CPU readback -> D3D11 upload has worked in the exact game build;
- OpenVR/SteamVR initialization, valid PSVR2 HMD pose acquisition and synthetic D3D11 submission have worked;
- the in-game flat bridge has completed genuine captured-frame submissions;
- one later diagnostic observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks followed by loss of integrity of the installed D3D9 device-vtable entries while monitor rendering continued.

Treat that last point as a confirmed failure mode of the current interception design, not as a complete root-cause explanation for the blank headset. Audit-remediation Phases 0-4 now provide host-tested run provenance, device/generation coverage, safe hook ownership and structured render telemetry. Two run-bound manual observations reproduced the three-frame device-hook loss; the second proved that all four lost slots return to their recorded Windows D3D9 originals. Capture/presentation decoupling remains open for Phase 5.

## Required execution order

Follow `docs/AUDIT_REMEDIATION_PLAN.md`.

Critical path before another manual game-observation run:

1. Phase 0 — auditable source/build/deployment/run provenance.
2. Phase 1 — build, CI and host-test validity.
3. Phase 2 — safe vtable patching and `HookRegistry` ownership.
4. Phase 3 — native factory/device discovery without split COM identity.
5. Phase 4 — structured run/render telemetry.

Do not ask for another headset test to validate these phases. The first post-remediation observation gate has already run. The next manual gate is an otherwise identical A/B observation with Steam Overlay disabled, and its telemetry must first prove that `gameoverlayrenderer.dll` no longer owns the pre-project factory `CreateDevice` target. A headset is unnecessary for that gate.

After that evidence, continue with capture/presenter separation, OpenVR lifecycle/synchronization, transactional deployment and neutral math contracts as defined in the remediation plan.

## Explicit prohibitions during stabilization

- Do not skip to camera hooks, stereo cameras, 6DOF, UI adaptation or motion controls.
- Do not reactivate D3D9Ex as the primary game path without new evidence.
- Do not use a blind periodic re-hook loop as the default fix.
- Do not introduce a full `IDirect3DDevice9` wrapper solely to avoid current hook replacement unless COM identity, `QueryInterface`, `GetDirect3D`, lifetime and discovery semantics are explicitly validated and evidence justifies the design.
- Do not equate OpenVR submit count with unique game-frame count.
- Do not interpret historical logs as evidence for a new artifact/run.
- Do not preserve an existing abstraction if the audit demonstrates that its ownership or testability is fundamentally wrong.

The historical `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` diagnostic remains useful baseline evidence because it can report replacement-slot ownership. Preserve it, but do not let it bypass the remediation order.

The D3D10 path for Call of Juarez remains a first-class later target because it has visible rendering improvements. D3D9 is first because it is the common renderer surface shared by all three inspected games.

## Runtime direction

OpenVR -> SteamVR is the primary PSVR2 path, including both PS VR2 Sense controllers at the later input milestone. Keep input abstractions logical and controller-independent.

Preserve OpenXR as an experimental/future backend, but it must be independently selectable and must not be required merely to configure/build the OpenVR path.

## Manual runtime validation

Never launch Call of Juarez or SteamVR automatically. The user performs all game and SteamVR launches manually.

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
