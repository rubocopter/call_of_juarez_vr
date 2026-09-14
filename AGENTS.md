# Call of Juarez VR — Agent instructions

Read this file before changing code. Then read `docs/internal/CODEX_HANDOFF.md`,
`ARCHITECTURE.md` and `ROADMAP.md`.

## Core rule

Call of Juarez (2006) is the reference implementation used to discover the
smallest reusable VR contracts. Do not generalize a Chrome Engine behavior until
at least one second game demonstrates the same boundary.

Reuse proven game-neutral policy from the Penumbra VR Framework when the
semantics match: tracking spaces, recenter/calibration policy, renderer-neutral
eye data, logical input, haptics, validation states and separation between
runtime policy and per-game/native adapters. Do not copy HPL-specific layouts,
addresses, hooks or source assumptions.

## Ownership

The shared runtime owns game-neutral VR concepts: poses, eye views, tracking
space, configuration semantics, logical input, haptics and VR runtime lifecycle.

Renderer backends own D3D9/D3D10 device and frame boundaries, render targets,
per-eye submission and renderer-specific resource handling.

Game backends own camera/player/weapon/UI/physics knowledge for a specific game
and build. Exact binary details must never leak into shared runtime code.

## Build identity

Binary integration is exact-build first. Filename recognition is diagnostic only.
Use SHA-256 to decide whether a build is known. Unknown builds must fail closed
for game-specific modifications while generic diagnostics may continue safely.

## Validation states

Keep these states distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` ->
`headset-validated` -> `supported`

A successful build or synthetic test does not imply a live-game or headset test.

## Current scope

Milestone 1's forwarding-only D3D9 bootstrap and native `Present`/`Reset`
observation are live-tested on the exact Call of Juarez build. The current gate is
the smallest OpenVR/SteamVR runtime plus a justified D3D9-to-D3D11 texture
transport boundary. Camera work comes only after those renderer/runtime boundaries
have evidence.

The D3D10 path for Call of Juarez remains a first-class target because it has
visual improvements. D3D9 is implemented first because it is the common renderer
surface shared by all three installed games.

## Manual runtime validation

Do not launch a Call of Juarez game or SteamVR automatically. The user performs
all game and SteamVR launches manually. Advance implementation, build validation,
deployment preparation, diagnostics and log verification as far as possible before
handing off a runtime test.

Do not stop merely because a game launch is the next evidence gate: prepare the
exact build artifact, reversible deployment step and post-run verifier first. Stop
only when the remaining evidence requires the user's manual game/SteamVR launch or
physical headset/controller validation.

The primary headset path is PlayStation VR2 on PC through OpenVR -> SteamVR,
including both PS VR2 Sense controllers. Keep input abstractions logical and
controller-independent, then provide PS VR2 Sense bindings at the controller
milestone. Preserve the existing OpenXR work as an experimental/future backend;
it is not the critical path for the first headset proof.

## Before editing

1. Inspect `git status`, HEAD and relevant owning files.
2. Read the current handoff and do not assume a planned item remains unimplemented.
3. Prefer a narrow evidence-backed change that advances the current validation gate.
4. Run the smallest meaningful host tests after each behavioral change.
5. Update status documents without promoting evidence beyond what was actually tested.
