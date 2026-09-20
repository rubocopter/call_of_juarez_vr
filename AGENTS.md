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

Run `20260919T162808Z-fb75cb34977a` is diagnostic multiprocess evidence and is no longer staged.
The user launched it twice (PIDs 28408 and 24032) and both starts left the SteamVR interface stuck
over the game. In each process the presenter initially reported `scene_focus_process_id=0` with
`dashboard_visible=true`; CoJ did not obtain scene focus until the first native-stereo frame became
presentable and the first scene submit completed. Reusing one run ID across two processes also
prevents formal promotion. The staging tool now reports `Staging: none`; no staged project D3D9 or
OpenVR files remain in the game directory. Treat the residual run manifest/log as diagnostic data.

Current host source replaces that startup contract with a Penumbra-style presentation policy adapted
to CoJ's classic-D3D9 ownership. The device `Present` hook captures the game backbuffer
as `flat_theater` content whenever no native-stereo producer has been active for 250 ms. The presenter
claims the OpenVR scene as soon as that flat content exists, repeats the latest flat frame at compositor
cadence, anchors it to a stable HMD pose and lets left-Sense Create re-anchor it. Native gameplay
automatically switches to `native_stereo`; later menus/loading screens can fall back to `flat_theater`.
The flat path deliberately permits identical eye content while native stereo retains the distinct-eye
fail-closed check. The implicit swap-chain hook remains as an explicit-swap-chain fallback. A
low-frequency ownership check safely reacquires the device slot only when it has returned to the
recorded system-D3D9 original; a foreign target is preserved and reported. Debug and Release each
pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP. This corrected
startup/fallback policy is **host-tested only** until a fresh physical run.

The flat presentation now also owns the first CoJ VR menu-interaction seam. OpenVR exposes dedicated
global `ui_select_left`/`ui_select_right` actions on L2/R2, independent from gameplay fire actions.
The presenter intersects the preferred Sense `/pose/tip` ray (with `/pose/handgrip` fallback) against
the same anchored flat-theater plane, maps the hit through the centered source rectangle, smooths it,
and composites a visible crosshair into the submitted flat texture. The exact CoJ adapter moves the
native Windows cursor over the game client and sends left-button down/up only while the flat menu is
active and CoJ owns foreground focus. Ray loss, focus loss, invalid client state and shutdown force a
release. A trigger held while returning to native stereo is neutralized until released so it cannot
become an accidental gameplay shot. The live verifier now requires an actual pointer hit plus click
down/up and the subsequent `flat_theater -> native_stereo` transition. Debug and Release each pass
24 tests plus the expected capability SKIP after these changes. This menu path is **host-tested only**.

Candidate `20260919T170916Z-d9a22d24eb0c` was never launched and has been transactionally unstaged;
its missing `cojvr.log` means it is not physical evidence and its run ID must not be reused.

Full/body run `20260919T174647Z-67b3c560acd0` is finalized/unstaged. It used clean source
`8697a816406b897fce35bcbb2b98ce12fd535216`, build-manifest ID
`96ACFF43B38E16C1FAA5A1177180F3567561B0587B72D3B39FB7622B0911F7A5`, proxy SHA-256
`4A6562851FE4CAA9845740ECBA35BCF85FF37E499FD7E3D0574C3F7C8C2D50DF`. Release preparation passed
24 tests plus the expected classic-D3D9 shared-texture capability SKIP, enabled Body IK before process
start and applied the reversible `1920x1080`/FSAA0 profile. The physical startup/menu gate failed:
the installed swap-chain hook received no game presentation callback, no `flat_theater` frame was
captured/published, and OpenVR moved directly to `native_stereo` only after gameplay loaded. The run
therefore confirms that CoJ presents through `IDirect3DDevice9::Present`; it does not promote the menu
path. Current source contains the host-tested device-Present correction described above.

Run `20260919T213924Z-d5d149a5bf46` is finalized/unstaged and proves the corrected device-Present
startup path in one process. The menu entered `flat_theater`, CoJ PID 404 acquired scene focus on the
first scene submit, 1,417 flat frames were published/uploaded, 1,417 new plus 2,482 repeated scene
submissions completed with zero submit failures, the Sense pointer produced native Win32 menu clicks,
and Create re-anchored the flat screen twice. The user nevertheless saw the menu doubled between the
eyes. That artifact copied the same centered source rectangle into both runtime-eye textures and did
not encode the finite screen depth from `eye_to_head`/per-eye FOV. Loading then crashed before any
`flat_theater -> native_stereo` transition; `hs_err_pid404.log` records the fault at
`ChromeEngine3.dll+0x20d889` inside `MeshObject.LoadMesh()` during `LawmanGame.LoadLevelAfterFade()`.
The fault is a near-null destination write after the engine's D3D buffer lock-like helper. The old
flat path kept deferred-capture default-pool resources alive across menu/loading, which can make a
classic D3D9 Reset fail. Current source addresses both observed boundaries: flat content is projected
per eye onto a head-centered 1.5 m plane using runtime eye translation/FOV, and flat readback is now
immediate with no persistent default-pool resource. A host test proves classic D3D9 Reset succeeds
without explicit flat-capture invalidation and capture recovers afterward. Debug and Release each pass
24 tests plus the expected classic-D3D9 shared-texture capability SKIP. These two corrections remain
host-tested pending one fresh physical fusion/load gate. The run also observed a SteamVR dashboard
cycle while scene focus remained PID 404; the project's Sense binding contains no dashboard/system
action, so this remains a separate overlay/input observation rather than a scene-ownership failure.

Run `20260919T153546Z-705460dca03b` is now finalized/unstaged as the latest clean single-process
physical evidence. It ran from source `32e979709bbb13780cf885c82770a0e8c1649631`, build-manifest ID
`9B5DDBEED941B6C1F5A1E45D39837F2B7BB491CE4C5F6AB8934F86DE255CD671`, proxy SHA-256
`8864D3DFEE42537D0A4E0CBCC230B4E27B48AC273287BFA9FAF508FA0323B97A`. The user confirmed the new
right-stick snap turn works physically; telemetry recorded 23 exact native +/-45-degree turns
(13 left, 10 right) through `PlayerBeing.RotateHorizontally(F)`. The same process completed 8,452
arm applications and 8,452 restores with zero writer/restore failures. Two Create recenter recoveries
used `calibration_preserved=true`, and subsequent arm samples exercised
`orientation_calibration_mode=preserved_target_rebase`. Promote the exact 45-degree snap behavior to
**headset-validated** and treat the recenter target-preservation path as **live-exercised**; visual
recenter stability was not isolated as its own acceptance gesture.

The new 52.68-second video shows a substantial arm-anatomy improvement over the earlier catastrophic
twist/collapse, but Body IK still needs visual work. Reach clamping remains frequent at 3,695/8,452
applications (43.72%; left 45.41%, right 42.03%), with the same ~49.843-unit native arm chain.
Wrist/hand orientation is still visibly forced in some poses, local head/hair intrudes strongly into
the HMD view, and weapon/shot aiming remains on the unresolved native per-hand look direction/origin
boundary. Presenter inner shutdown still reports `shutdown_complete=false` despite normal outer
runtime stop/run_end. See `docs/research/evidence/20260919T153546Z-705460dca03b.json`.

Latest physical evidence for candidate `20260919T122155Z-c534d86926a9` is diagnostic only: the
same run ID contains three separate CoJ process starts, so it cannot promote a formal single-process
gate. The third process (PID 448) is the user's 87.79-second recorded pose sequence. It completed
11,406 arm applications and 11,406 restores with zero writer/restore failures, but visual anatomy
still failed. `target_clamped=true` occurred 6,072/11,406 times (53.24%); the sampled native chain
averaged 26.686 upper + 23.158 lower = 49.843 game units, quantitatively supporting the user's
observation that the arms feel short. Head/hair intrusion, weapon-aim ownership and recurring
SteamVR dashboard sticking remain open. See `docs/research/evidence/20260919T122155Z-c534d86926a9.json`.

Current source preserves the visible hand-orientation target across Create recenter by rebasing
the new controller reference instead of recalibrating from the current natural hand pose, and it
implements one exact +/-45-degree right-stick snap per deflection through
`PlayerBeing.RotateHorizontally(F)` while neutralizing the old continuous native turn actions.
It also contains the four host-tested corrections described in the staged candidate above. Fresh
Debug and Release validation are both 24 PASS plus the expected classic D3D9 shared-texture
capability SKIP.

Latest evidence supersedes the historical arm-chain assumptions below. Run
`20260919T085408Z-327dd354bc4f` is finalized/unstaged and visually failed despite 5,050 successful
arm applications/restores. Replaying 116 logged axes proves FORETWIST inherits upper but NOT
forearm swing, while hand inherits forearm but NOT FORETWIST roll. EBones ordering is not parentage.
Current source explicitly swings the FORETWIST sibling and shares axial roll with the independent
hand (`hand_rotation_mode=sibling_shared_roll`), verifies both complete bases and restores all
elements. See `docs/research/COJ_ARM_SKINNING_AND_AIM.md`. Physical anatomy remains unpromoted.
The user has additionally authorized investigating/fixing aiming where feasible. Exact bytecode
shows shots use per-hand look direction and look origin, not render-time barrel transforms;
the current host-tested source writes controller `/pose/tip` direction into the per-hand native look
direction after the game update and before rendering, while preserving native spread/accuracy and
native fire origin. Physical firing evidence is still required before promotion.

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
telemetry only and constructs contiguous owned eye buffers directly from the locked D3D9 bytes,
avoiding a redundant full-vector initialization before overwrite. Phase 5 host acceptance also
covers resize, explicit pre-Reset resource invalidation/recovery, identical-format new-device
ownership and controlled paused-producer repeat classification. Those performance corrections are
host-tested only. Positional/body work has host coverage. The Session-based actor route is empty in
campaign, but shipped bytecode provides the exact single-player path
`LawmanGame.sm_cActiveGameModule -> LawmanModuleSingle.GetMainPlayer()`; run
`20260917T172007Z-e6232c4778d2` live-proved that route, actor reconciliation, valid changing Sense
poses and successful left/right arm writer calls. The user also observed that both arms moved with
the controllers, but they contorted sharply behind/over the body and produced severe graphical
corruption. Treat that run as a failed arm-composition gate, not as body-IK promotion. Presentation
comfort and clean presenter finalization also remain active physical concerns.
The proof remains game/build-specific and must not be generalized to another Chrome Engine
title without independent evidence.

Current host-tested body work reconciles horizontal HMD translation into the native actor, keeps
vertical translation camera/body-owned, carries both Sense poses in the same OpenVR sample/recenter
space, reads the exact CoJ skeleton through the existing JVM and contains a measured two-bone arm
overlay using the native `FromUpForwardPosElementWorld` method. Exact binary inspection after the
failed physical arm run established that this writer consumes a complete element world transform.
Current source therefore reads each upper-arm/forearm element's natural world position/up/+X frame,
reconstructs forward from the paired +X/up axes, and shortest-arc rotates that complete frame around
the measured shoulder/elbow pivot while preserving the native element-origin offset. The lower-body preflight also reads
pelvis/thigh/shin/foot geometry, derives a locomotion-rooted pelvis anchor and solves both measured
leg chains with reach clamping and the current animated knee plane, but remains read-only.
`bodyIkEnabled` is a runtime gate; the corrected arm composition is host-tested and the next
physical run must prove both arms before any pelvis/leg
writer is promoted. This remains exact-build Call of Juarez integration and must not be generalized
as Chrome Engine policy.

Run `20260917T222204Z-28ac69c31c56` physically confirmed that both arm writers now follow the Sense
controllers without the previous catastrophic element-origin corruption, but exposed a different
target-space bug. The user observed forward/back motion inverted. Telemetry showed why: recentered
controller positions were converted to centimetres around `(0,0,0)`, while the animated skeleton
was around `(39700,3600,29400)`. The solver was therefore clamping both arms toward the global origin.
Current source anchors each hand to live head bone 5 and maps `(tracked_hand - tracked_head)` through
the exact CoJ camera basis. Debug/Release host suites pass 24 tests plus the expected capability SKIP.
This world-anchor correction requires one fresh `-BodyIkAtStart` physical run before body promotion.

Run `20260917T223157Z-8061a216a065` physically exercised that world-anchor correction. Telemetry
confirmed that hand targets now stay in the live skeleton neighborhood and preserve left/right plus
front/back controller motion, but the physical arm gate still failed: in a T-pose both rendered arms
continued to point backward/contort. The user also observed the SteamVR interface stuck over the game
again. Treat the remaining arm defect as native pose-application failure, not target-space failure,
and treat the dashboard recurrence as a separate presentation/focus failure.

Exact-build inspection after that run rejected `SetBoneOrientation` and initially selected
`BoneRotate(BLVector;FZ)V`. Run `20260917T230423Z-e63b9146cea7` then recorded thousands of successful
left/right JNI applications and inverse restores without clear Sense-driven body motion. The later
run `20260918T160300Z-cd4137a48fca` resolved that ambiguity: all 96 sampled immediate
`body_arm_write_probe` records and all 192 per-eye `body_arm_render_probe` records stayed equal to
the natural geometry. `BoneRotate` is therefore rejected as the active visible-mesh writer for this
build unless new native evidence contradicts those measurements.

Run `20260918T165754Z-845101e7557b` physically proved that exact-build
`RotateElementWithChildren(ILVector;F)V` at RVA
`0x0009A070`. Disassembly shows that it composes a relative rotation onto the element's existing
world transform and refreshes descendants/attached children, preserving the animated element origin
and base frame rather than rebuilding an absolute transform. Both arms visibly followed the Sense
controllers for a short interval and telemetry proved natural -> changed -> changed during both eye
renders. The physical gate nevertheless failed: the user saw wrong orientation/deformation, and at
frame 470 the right inverse calls succeeded but exact geometry restoration failed. Frame 471 then
fail-closed both writers, explaining the observed return to the game's fixed native arm pose. The
game was already out of foreground focus at frame 450 while arm writes continued, so focus loss was
not itself the reset mechanism. Evidence package SHA-256:
`02A6D8EA4DBEF452A01142AC66100F88B982C5B4D4BFB2022F86A2466360DFE4`.

Exact helper disassembly at RVA `0x001F5700` shows why orientation was wrong: the handler
post-multiplies the current element matrix and consumes an element-local axis, while the failed
candidate supplied the solver's world-space axis directly. Current host source converts the upper
axis through the live upper-element frame, applies it, re-reads the parent-adjusted forearm frame
and converts the child axis there. It now requires elbow/wrist agreement with the solved targets
within `0.5` game units, not merely a geometry change. Inverse restoration remains child-before-
parent; if floating-point drift misses the captured natural sample, the bridge reapplies the exact
complete natural element frames and verifies them. Failure of both paths disables further writes.
The live verifier requires local-axis telemetry, target agreement, both-eye persistence and exact
natural restoration for both arms.

Runs `20260918T204701Z-f561e493f4ab` and `20260918T210459Z-b59448961f3c` physically exercised the
element-local correction. They recorded 832 and 1228 successful left/right arm applications
respectively, with `targets_reached=true`, before one restore comparison failed at frame 416/right
and frame 614/left and fail-closed all later writes. The user consequently saw only the native fixed
arms during normal gameplay. Both inverse JNI calls and exact-frame fallback had reported success;
the actual defect was the validator's `0.001`-unit threshold, smaller than one float ULP (about
`0.0039` units) around the live ~39,700-unit skeleton coordinates. Current source keeps strict
mutation detection but validates restoration separately at `0.02` game units for positions and
`0.001` for unit axes, emitting each maximum error. Run `20260918T210713Z-99d292bd2c94` used the
performance profile without `-BodyIkAtStart`; its manifest correctly set `requireBodyIk=false` and
no arm write was attempted. All three runs are finalized; do not reuse their IDs.

The user also reported severe discomfort specifically on physical head tilt. Those artifacts
rendered the CoJ camera without roll while submitting the full raw HMD pose through
`Submit_TextureWithPose`, so image orientation and compositor render-pose semantics diverged.
Current host source extracts physical roll and applies it to the exact native right/up/forward basis
with the sign required by tracking `-Z` -> CoJ `+Z`; the verifier requires meaningful roll telemetry.
This comfort correction needs physical validation. The reported poor resolution is expected from
the reversible `1920x1080`/FSAA0 performance profile used by all three runs; higher resolution is
still constrained by classic-D3D9 CPU readback cost.

The first `prepare -BodyIkAtStart` attempt with SteamVR still active was rejected before
manifest/deployment because eight classic-D3D9 device-creation tests returned
`D3DERR_NOTAVAILABLE`; status remained `Staging: none` with no video-profile state. After the user
closed SteamVR, the complete Release suite passed 24 tests plus the expected capability SKIP and a
fresh full/body candidate was staged as run `20260918T213453Z-a789ac61ac91`, build-manifest ID
`C2FB43FC65A9CD0F0A848A64584322D1D5C70A49F3E3B4E10BE958F1A7B11C90`, proxy SHA-256
`ACF278F40C2C959BD40A8BE016E4F8B73C4DDA8BCC3DAC51845F61191DEDA5FC`. Body IK and tracking are
enabled from process start, D3D9Ex is disabled and the reversible `1920x1080`/FSAA0 profile was
active. The run is now finalized. It recorded 13,860 successful arm applications and 13,860 clean
restorations with zero writer/restore failures; measured restore maxima were `0.00390625` game
units for joints/elements and `4.05355e-7` for axes. Physical roll ranged from `-27.286` to
`+40.762` degrees and the user reported that the previous head-tilt nausea was gone, promoting the
native-camera roll correction. The arms remained visibly malformed and front/back was reversed:
moving both hands physically behind the head made the arms visible in front. This physically
rejects the old hand-target mapping despite its valid target/recovery telemetry. Current source
therefore maps tracking-space `-Z` forward to negative native camera-forward while preserving the
live-working X/Y mappings. Hand orientation is still natural/uncontrolled and the photographed
wrist/hand twist remains a separate unpromoted problem. Evidence package SHA-256:
`A8CE0C3BDDAD405FDC8EADA5CDF3C27E8D5B76AE8E2FDDFE1329ECB145E1B477`.

Fresh Debug and Release suites from the corrected-Z source each pass 24 tests plus the expected
capability SKIP. Full/body run `20260918T215118Z-c46320012ff0`, build-manifest ID
`C3679E1866C80B388E557C8B5B76B6E144945469E8154DB16C5BECE3DB5476B4` and proxy SHA-256
`337EB29BE4000E4C85B0B2EAD30FC9843B84ED35E2DF1691BFAB3645EF824C34`, is now finalized and
unstaged. The corrected tracking-Z mapping is physically validated: the user confirmed that moving
both Sense controllers forward now moves the arms forward. Telemetry recorded 9,296 successful arm
applications and 9,296 successful restores, zero restore failures, and the corrected
`tracking_forward=-z_to_negative_native_forward` marker on every application. The body gate still
fails visual acceptance because both arms remain severely deformed/twisted. Controller orientation
was still intentionally absent in that artifact (`hand_orientation=natural`), so that run narrowed
the next arm task to controller/hand orientation plus forearm/wrist roll/twist composition, not
another positional-axis change. Evidence package SHA-256:
`CA3F758168AAE782E727EFCF0A24B6F46E821297538BB1A78883AA2CBA93DA74`.

Run `20260919T011421Z-bef5076cd07e` physically exercised the explicit PS VR2 Sense
`/pose/handgrip` path. It completed 14,968 left/right arm applications and 14,968 restores with no
restore failure, plus seven safe recenter recoveries, so the writer/restoration and handgrip pose
source remained stable. Visual anatomy still failed. In the sampled palms-up pose FORETWIST was
already approximately symmetric between sides (~89/~96 degrees average), while the additional hand
residual was still very large (~98/~122 degrees average). This isolates the next experiment to the
residual wrist/hand rotation rather than positional IK, tracked-Z, FORETWIST ownership or the native
writer.

Current host source therefore keeps computing the post-FORETWIST hand residual as diagnostic
telemetry but does **not** apply it. The visible transaction is upper arm -> forearm -> FORETWIST;
the hand keeps its native child relation to FORETWIST. Positional elbow/wrist target agreement and
complete natural-frame restoration remain mandatory, while calibrated full-hand orientation is no
longer an acceptance requirement for this candidate. Telemetry identifies the experiment with
`hand_orientation=calibrated_controller_delta_foretwist_only`,
`hand_residual_source=post_foretwist_observed_basis_diagnostic` and
`hand_rotation_mode=foretwist_only`; `hand_rotation_no_op=true` is required. Fresh Debug and Release
host suites pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP. This
twist-only composition still requires a fresh physical visual gate before Body IK promotion.

Clean candidate `20260919T085408Z-327dd354bc4f` is staged for that gate from source commit
`18057534b960522613e30b8aa2dc20e02d35eb1e` with `dirty=false`, build-manifest ID
`79EE4829D5076EB59A96F71609ACCFB37642E42788BDA30A9A28B6F4A5801213` and proxy SHA-256
`3BD0B476810B15FBD935FA96F539E7303439B372F6BC45DA57BB40595CCE693F`. Preparation passed 24 tests
plus the expected classic-D3D9 shared-texture capability SKIP, enabled Body IK before process start
and applied the reversible `1920x1080`/FSAA0 profile. Earlier preparation
`20260919T085249Z-0caf8c569979` was unstaged before launch because its manifest was dirty and must not
be treated as physical evidence.

Audit-remediation Phase 7 now has complete host transaction acceptance. The isolated
`tools/test_deployment_transactions.ps1` matrix recovers 15 staging failure checkpoints, 10
unstaging failure checkpoints and two repeated full stage/unstage cycles; `provenance_tools` also
proves active-`CoJ.exe` rejection before mutation. Phase 8's neutral math/semantic contract is also
host-tested: `EyeView::eye_to_head`, `LocatedEyeView::tracking_from_eye` and
`EyeRenderRecommendation` have distinct meanings; axes/units/handedness/composition are explicit;
asymmetric reference projection and invalid/non-finite inputs are covered. Do not infer from Phase 8
that the experimental OpenXR runtime/session lifetime is promoted; that remains a separate A10 task.

The same host source adds the first exact-CoJ gameplay-input profile. OpenVR exposes a neutral
`/actions/gameplay` set for move, turn, left/right fire, jump, reload, run, crouch, interact, weapon
next/previous and kick. The PS VR2 Sense binding maps sticks/buttons/triggers into that semantic
state, while the exact game adapter feeds the game's existing
`GameInputController.InputAction.Translate` objects rather than synthesizing Windows keyboard or
mouse input. Input is neutralized whenever scene focus/tracking is unavailable so held actions cannot
stick. This path is also **host-tested only**; left-Sense-Create recenter remains the separately
headset-validated global action.

The same source adds non-invasive `post_load_liveness` telemetry for the separately reported
post-load freeze: it observes real input changes, foreground/focus/GUI-thread state, pending
keyboard/mouse input through `GetInputState`, natural camera/frame progress and the inherited
read-only `Module.IsTimerFreezed()` state on the already-proven active `LawmanModuleSingle`.
It does not consume Win32 messages or synthesize input. Before the physical run, Debug and Release
each passed all 25 CTest outcomes with 24 PASS plus the expected classic-D3D9 shared-texture
capability SKIP. The earlier
candidate `20260918T164932Z-1daf676be48e` predates the timer-state probe and must not be used for the
next physical run. Full/body run `20260918T165754Z-845101e7557b` is finalized and unstaged; do not
reuse it. Its build-manifest ID was
`51EE466E23FFE5DC16DA502DF5078FFCAC9B3F7C56ED9EBF23CC6997E72B8670`, proxy SHA-256
`15AFB4220800DE7C0AF8F5743CB2C291D58A520926AF169525D076214929287E`, tracking enabled and Body IK
enabled from process start. The corrected local-axis source is host-tested: fresh Debug and Release
suites each pass 24 tests plus the expected classic-D3D9 shared-texture capability SKIP. A fresh
`-BodyIkAtStart` candidate remains required.

Prepare run `20260918T204452Z-d196ba97e34c` was rejected before launch and unstaged because
`vr_test.ps1` built `build-win32` but selected the stale DLL from `build/win32-debug`; it also
recreated the prohibited D3D9Ex marker. Current tooling binds build, manifest and staging to the
same `build-win32/Release` artifact, keeps the marker absent, and tests both contracts. Do not use
or reuse that run ID.

Corrected candidate `20260918T204701Z-f561e493f4ab` is finalized and unstaged; do not reuse it.
Its build-manifest ID was `E7AE926955FEFDC24A66B77ECC719FD35D3E02E4EA4F3C9729EF48C84ED8D697`
and proxy SHA-256 was `561CE9FFDCB97E174440FD265F56088AA23EA5383D2A0822A4B128EE4F12ADE3`.

Run `20260917T153051Z-ac6a4be37d85` is diagnostic only. The live `body-enable` control was accepted,
but after the user's Alt+Tab the game stopped producing new capture frames and the presenter only
repeated the last frame; the body verifier therefore had no valid arm-application sample. The same
run recorded dashboard open/close transitions but the user reported the SteamVR interface was not
usable, and presenter shutdown did not reach the `shutdown_complete` state. Do not use that run to
promote body, dashboard usability or shutdown. For the next body gate, use
`tools/vr_test.ps1 prepare -BodyIkAtStart` so the guarded body writer is enabled before SteamVR and
CoJ are launched; do not require Alt+Tab merely to enable body IK.

Run `20260917T154759Z-211723af9dc8` is finalized diagnostic evidence. Body IK was enabled before
launch and stereo frames continued, but every sampled body update failed at player discovery with
`local player is unavailable`; no skeleton/arm writer was reached. Shipped `Session.class` shows
`sm_LocalPlayer` is assigned only when `NetPlayer.GetNetIsOwner()` is true, while `sm_Players`
contains every created player. Current source keeps `sm_LocalPlayer` primary and uses the sole
`sm_Players` entry only when the vector contains exactly one player; ambiguous sessions fail closed.
The next physical candidate must be built from this fallback and must record valid left/right
`body_tracking_input` before body/arm evidence can promote. Controller gameplay actions are still
outside the current gate; only the global left-Sense-Create recenter action is implemented.

Run `20260917T161917Z-909b63e114af` proved that both PS VR2 Sense poses are valid and change with
physical controller motion, so controller tracking is not the body blocker. The campaign still
exposes neither `sm_LocalPlayer` nor any `sm_Players` entry: every sampled body update stopped at
`local player is unavailable and the session player list is empty`, so no skeleton/arm writer could
run. The user also reported that small physical head translation visibly left the native body
behind and that presentation remained uncomfortable. Sampled producer timings still spent roughly
`15-25 ms` per stereo frame in the classic-D3D9 CPU copy before accounting for the two engine eye
passes. Treat actor discovery and transport/frame pacing as separate problems.

Run `20260917T163732Z-03df947b8d50` then exercised the performance profile at temporary
`1920x1080`/FSAA0 with body IK deliberately disabled. The user confirmed that physical movement no
longer leaves the body behind; telemetry proves this is the intended unresolved-actor fallback,
`positional_6dof=false;translation_mode=rotation_only_actor_unresolved`, rather than successful
actor reconciliation. Both Sense poses remained valid/changing. The run reduced sampled CPU copy
to `10.246 ms` average, `9.982 ms` p50 and `12.937 ms` p95, with zero ring drops/submit failures,
but presenter shutdown still reported `shutdown_complete=false`. Evidence package SHA-256:
`C16C889C155F9B1A7C09D7D06DEA6FF9D3B600A97C3D65C49BE46F026935C4CE`.

Run `20260917T220704Z-b57a36497e54` is the latest performance-profile observation from the current
tree. Its run manifest explicitly sets `requireBodyIk=false` and `requirePositional6Dof=false`; arm
telemetry remained observation-only with `write_enabled=false`, so it carries no body-IK promotion.
At `1920x1080`/FSAA0 it collected 4,471 frames, submitted 4,469 new plus 2,075 repeated frames, and
recorded zero capture-ring drops or submit failures. Sampled CPU copy improved to `9.412 ms`
average / `8.888 ms` p50, while p95 rose to `14.808 ms` (max `17.313 ms`). Distinct-eye content,
explicit render-pose submission, recenter and an active focused/tracking-valid presenting state were
observed. The run is diagnostic only: the only dashboard open/close events occurred before active
presentation, no explicit disable-to-natural-passthrough command was recorded, and presenter
shutdown again emitted `shutdown_complete=false` despite outer `native_stereo_runtime: status=stopped`
and `run_end`. Evidence package SHA-256:
`0078E0700F7177A361A2D8C95A051741B7D099AA9EA535E85D68D542B865B04A`.

Static inspection after that run found the campaign player outside `Session`: `LawmanGame.class`
holds static `sm_cActiveGameModule : LawmanModule`, and `LawmanModuleSingle.class` exposes
`GetMainPlayer() : Being` backed by `MainPlayer : PlayerBeing`. Current source keeps the Session
routes primary, then uses this campaign route only when the Session player vector is empty and only
after JNI `IsInstanceOf` proves the module is `LawmanModuleSingle`; ambiguous/non-single-player
states fail closed. Fresh Debug and Release builds each pass 24 tests plus the one expected
classic-D3D9 shared-texture capability SKIP out of 25. Run `20260917T172007Z-e6232c4778d2`
subsequently live-proved this actor route; the remaining body blocker is corrected arm-element
composition, not player discovery.

Current host source therefore fails closed to HMD rotation plus native stereo eye offsets whenever
the actor cannot be proven/reconciled: room-scale head translation is suppressed instead of letting
the camera leave the body, and telemetry reports `positional_6dof=false` with an explicit
`translation_mode`. Fresh Release builds and the full suite pass 24 tests plus the expected classic
D3D9 shared-texture capability SKIP out of 25. The next `vr_test prepare` defaults to a performance
validation profile that does not require body IK or positional-6DOF promotion and applies a
reversible `1920x1080`, `FSAA(0)` `Video.scr` profile; `finish` restores the exact original file.

Already established evidence includes:

- forwarding/bootstrap and native D3D9 observation have worked in the exact game build;
- classic-D3D9 CPU readback -> D3D11 upload has worked in the exact game build;
- OpenVR/SteamVR initialization, valid PSVR2 HMD pose acquisition and synthetic D3D11 submission have worked;
- the in-game flat bridge has completed genuine captured-frame submissions;
- one later diagnostic observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks followed by loss of integrity of the installed D3D9 device-vtable entries while monitor rendering continued.

Treat that last point as a confirmed failure mode of the historical frame-hook interception design, not as a complete root-cause explanation for the blank headset. Audit-remediation Phases 0-4 now provide host-tested run provenance, device/generation coverage, safe hook ownership and structured render telemetry. Two run-bound manual observations reproduced the three-frame device-hook loss; the second proved that all four lost slots return to their recorded Windows D3D9 originals. Phase 5 capture/presentation decoupling and its host acceptance matrix are now host-tested and the separated path is exercised by later live native-stereo runs. Phase 6 OpenVR state/ownership/failure simulation and controlled D3D11 synchronization are also host-tested; its animated physical probe has not been rerun, so no new live/headset promotion follows from that work. The remaining active gate is sustained frame pacing/performance; the exact CoJ proof still must not depend on `Present`, `BeginScene`, `EndScene` or `Reset` hooks.

The next fresh `d3d9_native_stereo` physical body run uses `tools/vr_test.ps1 prepare -BodyIkAtStart`.
Do not require a SteamVR dashboard cycle in that body-composition run: dashboard/focus remains a
separate performance-profile gate because the system overlay can make CoJ non-interactive and
contaminate the IK test. Keep the stereo/recenter/shutdown checks and require valid
left/right `body_tracking_input`, `player_source=lawman_module_single_main_player`, successful
skeleton discovery and left/right `body_arm_tracking result=applied` with the element-frame
telemetry before promoting body/6DOF. Visual confirmation must show the arms following the Sense
controllers without the previous reverse/overhead contortion or mesh corruption.
Do not request separate headset runs for checks that can be collected in this combined run.

Candidate `20260917T163732Z-03df947b8d50` is finalized and unstaged. Its performance evidence is
described above; do not reuse its run ID. The next candidate must be built from the current
campaign-module actor fallback and receive a fresh run ID.

Run `20260917T172007Z-e6232c4778d2` is finalized and unstaged. Its evidence package SHA-256 is
`86CC35F6EAE2D8EB0C88C97D24723ACE10EE637D93A4368F55F18D09A7339EFB`. It live-proved the campaign
actor route and native arm write path, but physically failed arm composition because the previous
implementation replaced each mesh element's real world origin with the bone joint position.

Corrected candidate `20260917T175054Z-386939a46734` was prepared with build-manifest ID
`FFF7B749D0DCF1147CB1951F533973D3E7E7B6D92DEA13B24DD339FEB11511C8` and proxy SHA-256
`9BEFBFB2A7E91BF43ECC7E6635FBA21FC56A9F7C4D0C210112591D882BE678B5`, but it never produced a
`cojvr.log` and was later unstaged. The original `Video.scr` was restored. Do not reuse that run ID.
The next full/body candidate must be prepared fresh from the current tree with `-BodyIkAtStart`,
the full validation profile and a new run ID.

Run `20260917T230423Z-e63b9146cea7` is finalized and unstaged; evidence package SHA-256
`1371671F7D9F5310A430D583D769AD91EEDBCF7B9896FC3274CBDD86BABC9D95`. Do not reuse that run ID.

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
keeping the live-confirmed pitch direction. Current source also applies physical roll to the native
camera basis so rendered orientation matches the full HMD pose submitted for reprojection; this
tilt-comfort correction is host-tested only. Stereo passes keep source
world/view, frustum and derived camera state active for the complete render-view pass and
restore the complete natural snapshot transactionally after each eye. The first proof may use
the provisional classic-D3D9 CPU readback -> two D3D11 textures transport. The candidate
must not depend on `Present`, `BeginScene`, `EndScene` or `Reset` hooks.

## Explicit prohibitions during stabilization

- Camera hooks, HMD orientation injection, positional/body reconciliation and the current
  native-stereo render-view proof must remain in the exact Call of Juarez game integration.
  The explicitly authorized 6DOF/body preflight may advance through host tests and one gated
  physical arm-composition run; do not generalize it or extend it into UI rebuilding, weapon/aim
  ownership or pelvis/leg writes before that evidence exists. The exact-game semantic gameplay-input
  profile is an explicitly authorized parallel slice; keep it separate from Body IK promotion and do
  not generalize its CoJ action IDs or JVM route.
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
is headset-validated for the current camera gate. A broader neutral gameplay-action seam plus a
PS VR2 Sense profile is now implemented and host-tested; the exact CoJ adapter translates those
semantic actions through the game's configured `GameInputController` path. Physical gameplay-input
validation remains pending. Keep shared input abstractions logical and controller-independent, and
keep exact action IDs/game JVM details inside the CoJ integration.

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
