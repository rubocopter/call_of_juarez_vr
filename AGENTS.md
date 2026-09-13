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
space, configuration semantics, logical input and future OpenXR lifecycle.

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

Milestone 0 is repository/bootstrap research: Win32 build, build catalog, D3D9
availability probe and documentation. Milestone 1 is a forwarding-only D3D9
bootstrap for Call of Juarez with no rendering changes. OpenXR initialization and
camera work come only after forwarding is proven stable.

The D3D10 path for Call of Juarez remains a first-class target because it has
visual improvements. D3D9 is implemented first because it is the common renderer
surface shared by all three installed games.

DLSS/DLAA is a later renderer research track. Do not make it a prerequisite for
the initial VR path.

## Before editing

1. Inspect `git status`, HEAD and relevant owning files.
2. Read the current handoff and do not assume a planned item remains unimplemented.
3. Prefer a narrow evidence-backed change that advances the current validation gate.
4. Run the smallest meaningful host tests after each behavioral change.
5. Update status documents without promoting evidence beyond what was actually tested.
