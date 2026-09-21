# Call of Juarez VR — agent instructions

Read this file before changing code. Then read, in order:

1. `docs/TECHNICAL_AUDIT.md`
2. `docs/AUDIT_REMEDIATION_PLAN.md`
3. `docs/VALIDATION.md`
4. `docs/ARCHITECTURE.md`
5. `docs/ROADMAP.md`

Inspect the repository before acting. Documentation records durable contracts and validation state; source, local staging and fresh test results decide whether an item is still pending.

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

`docs/VALIDATION.md` owns durable acceptance state and current unresolved gates. Individual run IDs, process IDs, videos, raw telemetry, evidence packages and active agent handoffs belong under ignored `work/` and are not versioned.

## Exact game contracts

Preserve demonstrated target contracts unless new evidence disproves them:

- Camera/render path: `Camera -> View/Projection -> ChromeEngine3 renderer`.
- Full render-view wrapper: `0x00030FB0`; core-only `0x00030E00` is insufficient for the second eye.
- Camera source basis uses exact right/up/forward/position; do not reconstruct the first axis with a cross product.
- Game/world units are centimetres; XR runtime units are metres. Convert only in the Call of Juarez adapter.
- Native analog movement uses the target's per-axis shaping and actions 4-7 as floats.
- Body writes use the exact-build native body boundary and must remain scoped/restorable.
- Weapon direction/origin, UI ownership and arm hierarchy stay game-specific until independently demonstrated otherwise.

If a contract changes, update its owning source or durable research document rather than adding another parallel description.

## Physical workflow

Use the repository workflow for physical candidates:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually.
# Exercise the acceptance gestures in docs/VALIDATION.md.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

`prepare` builds/tests, creates local provenance and stages the candidate. `finish` verifies/collects local evidence and restores staging. Never launch or terminate SteamVR or the game automatically. Every headset run must use a fresh local run ID.

## Before editing

- Inspect `git status`, branch/upstream relation, current source and current staging state.
- Do not overwrite user changes or assume a documented task remains pending.
- Inspect the current local handoff/evidence under `work/` when continuing an active physical gate.
- Keep D3D9/OpenVR as the primary active path. OpenXR and D3D10 remain separate future/experimental tracks unless the roadmap changes.
- Preserve exact-build fail-closed behavior and transactional restoration.
- Keep temporary camera/body/weapon mutations scoped and restorable.
- Prefer measured ownership fixes over tuning arbitrary constants around visible symptoms.
- Promote validation state only from evidence satisfying the gate in `docs/VALIDATION.md`.

## Documentation policy

Versioned documentation is limited to durable architecture, research conclusions, roadmap state, compatibility and validation contracts.

Temporary debugging chronology, one-off measurements, raw run metadata, local paths, screenshots, videos, extracted frames, prompts, agent continuity notes and discarded hypotheses belong under ignored `work/`. `docs/internal/` and `docs/research/evidence/` are legacy local-only paths and are ignored defensively if recreated.
