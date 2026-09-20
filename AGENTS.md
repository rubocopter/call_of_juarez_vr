# Call of Juarez VR — agent instructions

Read this file before changing code. Then read, in order:

1. `docs/TECHNICAL_AUDIT.md`
2. `docs/AUDIT_REMEDIATION_PLAN.md`
3. `docs/internal/CODEX_HANDOFF.md`
4. `docs/VALIDATION.md`
5. `ARCHITECTURE.md`
6. `ROADMAP.md`

Inspect the repository before acting. Documentation records validation state; source and fresh test results decide whether an item is still pending.

## Product and reuse boundary

Call of Juarez (2006) is the reference implementation used to discover the smallest reusable VR contracts. The supported-game target is native stereo rendering, full-body IK and interactions rebuilt for VR.

Reuse proven game-neutral policy from the Penumbra VR Framework when semantics match: tracking spaces, recenter/calibration policy, renderer-neutral eye data, logical input, haptics, validation states and separation between runtime policy and native adapters. Never copy HPL-specific layouts, addresses, hooks or source assumptions.

Do not generalize a Chrome Engine behavior until a second game independently demonstrates the same boundary.

## Ownership

- Shared runtime: game-neutral VR concepts and policy.
- Renderer backend: D3D9/D3D10 device/frame ownership, capture/transport, render targets and compositor integration.
- Game backend: exact Call of Juarez camera/player/weapon/UI/physics knowledge.
- Diagnostics/evidence: hook integrity, factory/device/generation identity, structured telemetry and source/build/deployment/run correlation.

Exact binary details must remain inside the game adapter.

## Build identity and validation

Use SHA-256 to decide whether a binary is known. Filename recognition is diagnostic only. Unknown builds fail closed for game-specific mutation.

Keep validation states distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A build, CTest pass or synthetic verifier never promotes a physical gate.

## Current checkpoint

Latest physical evidence is diagnostic run ID `20260920T214629Z-351c27f434d5`. The same staged ID was reused across three CoJ processes (PIDs 12048, 8600 and 4688) after Circle/back failures forced relaunches, so it is **multiprocess diagnostic evidence only** and cannot promote a physical gate. Intro skipping worked, but subtitles were invisible, VR pointer motion did not drive menu hover, Circle could hide/show layers and leave a frozen flat presentation, locomotion/jump remained unacceptable, physical crouch still exposed the full avatar, and arm anatomy/reload remained visibly rejected. The gameplay process recorded 55 `fire_right=true` samples and reached `run_end`, so the previous first-shot process termination was not reproduced; weapon origin/direction still failed visually. No validation state advances from this run.

The current working tree contains the next host-tested candidate and has **not** been headset-tested. It now:

- keeps the logical flat-menu cursor route through `MainMenuModule.GetGlobalCursor() -> UICursorGame.SetPos(LVector;)V -> OnMouseMove(FFI)V`, and also mirrors the projected pointer through Win32 `SetCursorPos` + `WM_MOUSEMOVE` because shipped UI hit-testing follows the real mouse path rather than the cursor sprite alone;
- maps Cross to global accept, Circle to global back through the active UI's normal Escape press/release path, and L2/R2 to ray-select;
- renders the flat-theater beam from the projected Sense-controller origin;
- keeps startup skip on the exact `IntroModule.OnInputKey` route and dispatches loading continue through `GameUILoading.OnInputKey(IZC)V`;
- resolves paused hints through shipped `LawmanModule.GetHintManager()` instead of direct inherited-field lookup, eliminating the known `m_cHintManager` `NoSuchFieldError` route;
- recenters from a gravity-level, yaw-only HMD basis so recenter cannot bake pitch/roll into tracking space;
- samples normal Body IK telemetry instead of logging successful tracking/restore every arm/frame;
- detects physical crouch from calibrated HMD height for state/telemetry without automatically pressing the native crouch action; explicit controller crouch still uses the native action;
- scopes controller-derived `Being.m_vLookFromPoint` strictly around the synchronous fire `InputDigital.Translate` call and restores the native value immediately;
- no longer creates a gameplay `LaserPointer` diagnostic object;
- samples eye-distinction hashes instead of scanning both full eye buffers every frame and recycles CPU-frame storage across the D3D9 readback mailbox.

Still-open root causes are explicit: subtitle visibility (`Settings.bSubtitles` versus UI/presentation loss), first-person local-mesh ownership during physical crouch, locomotion/jump semantics separate from renderer stutter, hand orientation/reload animation conflict, and the remaining weapon trajectory/origin error. The gameplay process still rendered/captured 1920x1080 per eye while SteamVR recommended 3400x3468; its classic-D3D9 CPU copy measured 7.432 ms median and 8.994 ms p95, so do not raise eye resolution substantially while this CPU readback boundary remains.

Fresh Debug and Release suites each pass all 25 outcomes: **24 PASS + the expected classic-D3D9 shared-texture capability SKIP**.

Staging is local runtime state; inspect it with `tools/vr_test.ps1 status` before acting. Every headset run must use a fresh run ID.

## Physical evidence that remains authoritative

- `20260916T153109Z-8976b8f77775`: correct 100 game-units/metre eye baseline and left-Sense Create recenter exercised physically.
- `20260916T221254Z-861f3c15abd4`: compositor lifecycle/focus correction reached scene focus and clean outer runtime stop/run end.
- `20260916T224239Z-e43b46698e5c`: explicit render-pose submission removed the previous head-turn pull/snap-back artifact.
- `20260919T153546Z-705460dca03b`: exact ±45° snap turn physically validated; body IK visibly improved but remained rejected.
- `20260920T090448Z-b1f54e3cb38e`: startup flat theater, load into native stereo, recenter and local head/hair suppression exercised successfully.
- `20260920T152316Z-48dab54366d5` and `20260920T161307Z-474f0b054338`: earlier diagnostic rejections that drove UI, locomotion instrumentation, body ownership and firing work.
- `20260920T204507Z-6db13f3107e0`: single-process rejection that exposed menu/loading, locomotion, crouch, horizon and first-shot stability failures.
- `20260920T214629Z-351c27f434d5`: latest multiprocess diagnostic rejection; confirms intro skip, persistent menu/subtitle/locomotion/crouch/body failures and poor image/transport cost, while repeated firing no longer terminated the gameplay process. It is not promotable because the staged run ID was reused.

Compact metadata for retained formal runs is under `docs/research/evidence/`. Large logs, extracted video frames and local evidence packages are disposable once their conclusions are incorporated into source and these records.

## Exact game contracts currently in use

- Camera/render path: `Camera -> View/Projection -> ChromeEngine3 renderer`.
- Full render-view wrapper: `0x00030FB0`; core-only `0x00030E00` is insufficient for the second eye.
- Camera source basis: exact right/up/forward/position; do not reconstruct the first axis with a cross product.
- Game/world units: centimetres; XR runtime units: metres. Convert only in the Call of Juarez adapter.
- Native analog movement: per-axis 0.04 deadzone/saturation, actions 4-7 as floats.
- Body writer: exact-build `RotateElementWithChildren(ILVector;F)V` with element-local axes and verified restoration.
- Effective arm hierarchy: FORETWIST behaves as a sibling of forearm beneath upper; hand follows forearm and does not inherit FORETWIST roll.
- Weapon direction: native per-hand look direction; visual origin: `m_avAimFromPoint[hand]`; ordinary ballistic origin: `Being.m_vLookFromPoint`.

## Manual physical workflow

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually.
# Test the acceptance gestures in docs/VALIDATION.md.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

`prepare` builds/tests, creates exact provenance and stages the candidate. `finish` verifies/collects evidence and restores staging. Never launch or terminate SteamVR or the game automatically.

## Before editing

- Inspect `git status`, current source and current staging state.
- Do not overwrite user changes or assume a documented task remains pending.
- Keep D3D9/OpenVR as the primary active path. OpenXR and D3D10 remain separate future/experimental tracks.
- Preserve exact-build fail-closed behavior and transactional restoration.
- Keep temporary camera/body/weapon mutations scoped and restorable.
- Promote a validation state only from evidence that satisfies the gate in `docs/VALIDATION.md`.
