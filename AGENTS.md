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

Newest physical evidence is complete single-process diagnostic run `20260921T192639Z-1d70905cb4f8` (PID 24736), correlated with `C:\Users\onita\Videos\clip_1.790.022.783.959.mp4`. It has matching `run_start`/`run_end`, intact deployment provenance and a collected evidence package, but the user still rejects the playability gate. The sole Win32 menu-pointer route remains uncontrollable and required the physical mouse. Walking remains terrible despite full `0.999992` movement magnitude and the shipped analog lock/dispatch/unlock/apply transaction; jump input reaches the game but the user sees only about a 2 cm hop. Moving the Sense controllers now frequently returns arms to the shipped pose because safety gates deny unsafe VR writes. Shots remain visibly wrong even though grip-to-tip geometry strongly confirms local `-Z` as the Sense `/pose/tip` pointing axis. No playability state advances.

The current code candidate has now been physically exercised and rejected for playability. Technical behavior observed in the run includes:

- the real Windows mouse path is the sole flat-menu pointer-motion owner (`SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE`), but physical evidence still finds the resulting pointer uncontrollable;
- maps Cross to global accept, Circle to global back through the active UI's normal Escape press/release path, and falls back to `LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)` when gameplay has no current UI; L2/R2 remain ray-select;
- renders the flat-theater beam from the projected Sense-controller origin;
- keeps startup skip on the exact `IntroModule.OnInputKey` route and dispatches loading continue through `GameUILoading.OnInputKey(IZC)V`;
- resolves paused hints through shipped `LawmanModule.GetHintManager()` instead of direct inherited-field lookup, eliminating the known `m_cHintManager` `NoSuchFieldError` route;
- recenters from a gravity-level, yaw-only HMD basis so recenter cannot bake pitch/roll into tracking space;
- samples normal Body IK telemetry instead of logging successful tracking/restore every arm/frame;
- detects physical crouch from calibrated HMD height for state/telemetry without automatically pressing the native crouch action; explicit controller crouch still uses the native action;
- horizontal-only visual-pelvis room-scale compensation is physically exercised: all 64 sampled writes had `world_offset.y=0` and restored after stereo capture; this fixes the previous vertical-pelvis error but still lacks stick-equivalent locomotion animation when the player walks physically;
- actions 4-7 use the exact target category, native 0.04 shaping and shipped `LockApplyControllerState -> dispatch/Translate -> UnlockApplyControllerState -> ApplyControllerState`; 126 sampled transactions were observed, yet locomotion remains physically rejected;
- arm safety rejects unsafe reach/orientation writes; 43/128 sampled arm updates were denied and the user sees the resulting fallback to native animation as repeated arm resets, so fail-closed safety is working while Body IK usability is not;
- observes `Settings.bSubtitles`, `Dialog.cPlayingDialog`, `m_bCurrentLineVisible` and `AreSubtitlesVisible()` so the next physical run can distinguish disabled subtitles from a presentation/capture failure;
- publishes controller-derived `Being.m_vLookFromPoint` on the fire press transition and records aim geometry/fire-transition evidence; 120 axis samples place grip-to-tip against local `-Z` at `0.939388..0.939389`, so the simple Sense tip-axis question is effectively resolved while muzzle/ballistic ownership remains wrong in-headset;
- no longer creates a gameplay `LaserPointer` diagnostic object;
- samples eye-distinction hashes instead of scanning both full eye buffers every frame and recycles CPU-frame storage across the D3D9 readback mailbox.

Open playability work is now explicit: replace the unstable menu pointer ownership model; isolate locomotion speed/jump amplitude from frame pacing instead of changing more input routing blindly; make physical horizontal displacement drive the same visual walking animation as stick movement without surrendering native collision ownership; solve continuous tracked-arm ownership instead of repeatedly falling back to the default animation; and trace weapon direction/origin to the visible barrel. Reintroduce a temporary controller-tip gameplay ray for alignment evidence only. The latest run measured classic-D3D9 CPU copy at 7.112 ms median and 9.056 ms p95 at 1920x1080 per eye, and inner presenter finalization still reports `shutdown_complete=false`.

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
- `20260920T214629Z-351c27f434d5`: earlier multiprocess diagnostic rejection; confirms intro skip, persistent menu/subtitle/locomotion/crouch/body failures and poor image/transport cost, while repeated firing no longer terminated the gameplay process. It is not promotable because the staged run ID was reused.
- `20260920T235112Z-6b2d91cda4a5`: earlier complete single-process diagnostic rejection; proves subtitles were disabled, movement input reached full range, synthetic cursor position/messages still needed physical mouse motion, gameplay lacked a usable back route, room-scale motion exposed the avatar, and extreme reach/orientation contributed to right-arm deformation.
- `20260921T163309Z-481defca3401`: newest non-promotable reused-run diagnostic; the second process proves the combined pointer route could select but was chaotic, full-range locomotion/jump still failed, full-XYZ visual-pelvis compensation exposed the body, aiming remained unusable and the right arm still accepted extreme rotations.
- `20260921T192639Z-1d70905cb4f8`: latest complete single-process rejection; single-owner Win32 pointer remains uncontrollable, native analog transaction does not fix locomotion/jump, horizontal-only body compensation removes vertical pelvis motion but lacks physical-walk animation, arm guards cause frequent native-pose fallback, and `-Z` tip geometry is confirmed while weapon muzzle/ballistics remain wrong.

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
