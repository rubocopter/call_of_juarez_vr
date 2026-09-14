# Call of Juarez VR — Agent instructions

Read this file before changing code. Then read `docs/internal/CODEX_HANDOFF.md`, `ARCHITECTURE.md`, `ROADMAP.md` and the relevant validation notes before assuming any task is still pending.

## Core rule

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable VR contracts. Do not generalize a Chrome Engine behavior until at least one second game demonstrates the same boundary.

Reuse proven game-neutral policy from the Penumbra VR Framework when the semantics match: tracking spaces, recenter/calibration policy, renderer-neutral eye data, logical input, haptics, validation states and separation between runtime policy and per-game/native adapters. Do not copy HPL-specific layouts, addresses, hooks or source assumptions.

## Ownership

The shared runtime owns game-neutral VR concepts: poses, eye views, tracking space, configuration semantics, logical input, haptics and VR runtime lifecycle.

Renderer backends own D3D9/D3D10 device and frame boundaries, transport, render targets, per-eye submission and renderer-specific resource handling.

Game backends own camera/player/weapon/UI/physics knowledge for a specific game and build. Exact binary details must never leak into shared runtime code.

## Build identity

Binary integration is exact-build first. Filename recognition is diagnostic only. Use SHA-256 to decide whether a build is known. Unknown builds must fail closed for game-specific modifications while generic diagnostics may continue safely.

## Validation states

Keep these states distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A successful build or synthetic test does not imply a live-game or headset test.

## Current scope

The current critical path is the Call of Juarez D3D9 -> D3D11 -> OpenVR flat-game proof.

Already established:

- forwarding/bootstrap and native D3D9 observation are live-tested;
- classic-D3D9 CPU readback -> D3D11 upload is live-tested on the exact game build;
- OpenVR/SteamVR initialization, valid PSVR2 HMD pose acquisition and synthetic D3D11 submission are live-tested;
- the in-game flat bridge has completed three genuine game-frame submissions through readback, upload, `WaitForHmdPose` and stereo `Submit`;
- the active failure is not a compositor/readback stall: after exactly three `Present`, `BeginScene` and `EndScene` callbacks, the installed D3D9 device-vtable hooks are overwritten while the game continues rendering normally on the monitor.

The active diagnostic identifies which `Reset`, `Present`, `BeginScene` and `EndScene` slots are replaced and resolves each replacement target to its owning loaded module. Use that evidence before choosing the next interception strategy.

If a known overlay/hook module owns the replacement, decide whether to remove that interference for validation or chain after it. If D3D9/engine-owned code restores the entries, investigate that lifecycle. If native-vtable ownership remains inherently unstable, prefer an owned `IDirect3DDevice9` forwarding wrapper over periodic re-hooking.

Do not skip ahead to camera hooks, stereo rendering, 6DOF or motion controls before sustained renderer interception is demonstrated. The flat bridge should sustain at least 300 submitted frames before promotion.

The D3D10 path for Call of Juarez remains a first-class target because it has visual improvements. D3D9 is implemented first because it is the common renderer surface shared by all three inspected games.

## Runtime direction

OpenVR -> SteamVR is the primary PSVR2 path, including both PS VR2 Sense controllers at the input milestone. Keep input abstractions logical and controller-independent.

Preserve the existing OpenXR work as an experimental/future backend. It is not the critical path for the first headset proof.

## Manual runtime validation

Do not launch a Call of Juarez game or SteamVR automatically. The user performs all game and SteamVR launches manually. Advance implementation, build validation, deployment preparation, diagnostics and log verification as far as possible before handing off a runtime test.

Do not stop merely because a game launch is the next evidence gate: prepare the exact build artifact, reversible deployment step and post-run verifier first. Stop only when the remaining evidence requires the user's manual game/SteamVR launch or physical headset/controller validation.

## Before editing

1. Inspect repository status, HEAD and the relevant owning files.
2. Read the current handoff and validation state; do not assume a planned item remains unimplemented.
3. Prefer a narrow evidence-backed change that advances the current validation gate.
4. Run the smallest meaningful host tests after each behavioral change.
5. Update status documents without promoting evidence beyond what was actually tested.
6. Do not repeat a historical live diagnostic unless a new candidate or hypothesis makes the run materially different.
