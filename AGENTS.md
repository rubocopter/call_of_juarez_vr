# Call of Juarez VR — agent instructions

Read this file before changing code. Then read, in order:

1. `docs/TECHNICAL_AUDIT.md`
2. `docs/AUDIT_REMEDIATION_PLAN.md`
3. `docs/internal/CODEX_HANDOFF.md`
4. `docs/VALIDATION.md`
5. `docs/ARCHITECTURE.md`
6. `docs/ROADMAP.md`

Inspect the repository and current evidence before acting. Documentation records durable contracts and validation state; source, staging and fresh test results decide whether an item is still pending.

## Product and reuse boundary

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable VR contracts. The target is native stereo rendering, tracked head and hands, full-body integration and interactions rebuilt around motion controllers.

Reuse proven game-neutral policy from Penumbra VR Framework when the semantics genuinely match: tracking spaces, recenter/calibration policy, renderer-neutral eye data, logical input, haptics, validation states and separation between runtime policy and native adapters. Never copy HPL-specific layouts, addresses, hooks or source assumptions.

Do not generalize a Chrome Engine behavior until a second game independently demonstrates the same boundary.

## Ownership

- Shared runtime owns game-neutral VR concepts and policy.
- Renderer backends own D3D9/D3D10 device/frame ownership, capture/transport, render targets and compositor integration.
- Game backends own exact Call of Juarez camera/player/weapon/UI/physics knowledge.
- Diagnostics/evidence owns hook integrity, factory/device/generation identity, structured telemetry and source/build/deployment/run correlation.

Exact binary details must remain inside the game adapter. Unknown executable builds fail closed for game-specific mutation; SHA-256 identity is authoritative and filenames are diagnostic only.

## Validation

Keep validation states distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A build, CTest pass, static inspection or synthetic verifier never promotes a physical gate. Do not infer headset support from bindings, code paths or host-side tests alone.

Current physical evidence, run IDs, measurements and the active implementation checkpoint belong in `docs/VALIDATION.md`, `docs/internal/CODEX_HANDOFF.md` and the retained evidence records under `docs/research/evidence/`. Do not duplicate that chronology here.

## Exact game contracts

Preserve the currently demonstrated target contracts unless new evidence disproves them:

- Camera/render path: `Camera -> View/Projection -> ChromeEngine3 renderer`.
- Full render-view wrapper: `0x00030FB0`; core-only `0x00030E00` is insufficient for the second eye.
- Camera source basis uses exact right/up/forward/position; do not reconstruct the first axis with a cross product.
- Game/world units are centimetres; XR runtime units are metres. Convert only in the Call of Juarez adapter.
- Native analog movement uses the target's per-axis shaping and actions 4-7 as floats.
- Body writes use the exact-build native body boundary and must remain scoped/restorable.
- Weapon direction/origin, UI ownership and arm hierarchy stay game-specific until independently demonstrated otherwise.

If a contract changes, update its owning source/evidence document rather than adding another parallel description here.

## Physical workflow

Use the repository workflow for physical candidates:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually.
# Exercise the acceptance gestures in docs/VALIDATION.md.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

`prepare` builds/tests, creates exact provenance and stages the candidate. `finish` verifies/collects evidence and restores staging. Never launch or terminate SteamVR or the game automatically. Every headset run must use a fresh run ID.

## Before editing

- Inspect `git status`, branch/upstream relation, current source and current staging state.
- Do not overwrite user changes or assume a documented task remains pending.
- Read the active handoff and validation documents before changing the current gate.
- Keep D3D9/OpenVR as the primary active path. OpenXR and D3D10 remain separate future/experimental tracks unless the roadmap changes.
- Preserve exact-build fail-closed behavior and transactional restoration.
- Keep temporary camera/body/weapon mutations scoped and restorable.
- Prefer measured ownership fixes over tuning arbitrary constants around visible symptoms.
- Promote validation state only from evidence satisfying the gate in `docs/VALIDATION.md`.

## Documentation policy

Keep root documentation durable and small. `README.md` is the public landing page; this file defines agent rules. Architecture, roadmap, validation, audits, research and active handoffs belong under `docs/`.

Temporary debugging chronology, one-off measurements, local logs, screenshots, videos, extracted frames and discarded hypotheses must not accumulate in root documentation. Retain only evidence still needed to reproduce an unresolved gate or a maintained verifier workflow.
