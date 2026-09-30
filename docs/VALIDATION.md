# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

Per-run logs, process IDs, videos, raw telemetry and evidence packages are local working data and belong under ignored `work/`. This document records only durable acceptance state and the current physical gate.

## Current physical gate

The D3D9Ex compatibility/startup/reset path is now **live-tested** in the
production mod on the inspected Steam build and this host. The physical startup
run retained Ex factory/device identity, recorded successful Presents beyond
frame 2,970, completed a reset into generation 2 and captured flat images.
All 380 logged translated MANAGED texture creation results succeeded. Proxy hooks
restored and the outer runtime reached finalization. This does not validate
resource contents across reset, broader gameplay or native-stereo transport.

The corrected production startup gate has now **passed**. The geometry-derived
allocation for both asymmetric PS VR2 eyes is **live-tested**, and startup
videos plus the main menu on the mod's virtual screen are **headset-validated**
by the operator. The pointer was visible; this does not establish accurate
pointing or controller-only menu operation. Frames upload, compositor submission
and repeats advance with no frame rejection or submit failure. The physical eye
optics confirm that the old fixed 35% padding could not contain the source.
Shared native-stereo gameplay now supplies **live-tested** publication,
D3D11 consumer copies and successful new-frame OpenVR submission for both eyes
bound to an explicit render pose. Sampled producer timings report zero CPU
readback/copy and sampled producer/consumer waits are zero. The operator reports
no perceived slowdown; that initial impression was **observed**. A subsequent
bounded `vr-full` trace now establishes the production cadence target, and the
operator confirms correct depth and stable head turns in the visor.

The verifier's obsolete submission pattern rejected genuine telemetry because
it omitted the presenter's `presentation_mode` field. The corrected regression
uses the actual native-stereo record and rejects flat presentation, zero pose,
failed-eye submission and repeats without a successful new frame. Frozen replay
of the earlier failed run advances past submission and fails at incomplete
owning-runtime shutdown. That log ended during capture cleanup with no transport
summary or `run_end`; missing final counters remain unknown, not zero.
A fresh physical run now passes the corrected verifier through complete shutdown.
Bounded transport acceptance now also passes its measured cadence and visual
stereo/head-turn checks. Sustained pacing, tail latency and pending-frame
reset/device-loss recovery remain unproved.
A monoscopic gameplay clip
also shows avatar occlusion; body IK was disabled for this profile, so tracked
body behavior and stereo visual correctness are not promoted by this run.

**Implemented / host-tested / live-tested shutdown boundary:** finalization now executes
before the exact game's `DestroyGame` import. Binary hashes, x86 headers, loaded
call site and import/export identity are required, and the original game call
is preserved. Owner stop/join, protected import restoration and real process/DLL
exit ordering pass host regressions; pending GPU copies drain before capture
resources are reclaimed. The verifier rejects missing pre-exit completion,
abandoned/pending copies, unequal copied/completed totals or missing counters.
The new exact-build physical run completes pre-exit begin/end, import restoration,
same-owner OpenVR shutdown, capture cleanup and `run_end`. Both final summaries
agree on copied/completed totals with zero pending or abandoned copies; every
published producer lease is reclaimed. Normal-quit shutdown acceptance is closed
for this build/host. See [shutdown boundary](research/COJ_SHUTDOWN_BOUNDARY.md).

**Host-tested / live-tested telemetry follow-up:** releasing the last fenced but unpublished
producer frame reset its slot without refreshing ring-depth telemetry. A real GPU
regression reproduces the stale final depth and passes after refreshing that
counter. The next physical binary includes this correction and reports final
depth zero with every published producer lease reclaimed. The older run's trace
was disabled; cadence comes from the subsequent `vr-full` phase, not mixed
flat/native submission totals.

**Performance-validated for bounded production cadence:** the new trace measures
136.151 game updates/s and 135.527 rendered stereo pairs/s over approximately
72 seconds of render observation. This is about 63%/62% above the old CPU path,
respectively, and within 1.4%/1.6% of the readback-off reference. Update Hz uses
native simulation duration; pair Hz uses the render wall interval. These are
production rates, not headset refresh or compositor submission rates. Sampled
native CPU readback/copy and producer/consumer waits remain zero, with no fallback
or open/copy/submit failure. Ring drops and frame-age outliers remain observed;
full pacing and tail-latency performance are not promoted by this short run.

**Headset-validated for this bounded stereo path:** the operator confirms correct
depth and a stable image during head turns, without perceived dragging or jerks.
Body IK was disabled. Body/UI/weapon and abnormal-lifecycle acceptance remain
separate from the completed short transport gesture.

Independent LTR research on the same exact game build is materially stronger
than those old production attempts. Its semantic MANAGED adapter maps observed
usage-0 2D/cube textures to `DEFAULT | DYNAMIC` and observed MANAGED WRITEONLY
VB/IB requests to DEFAULT, survives 39 `BeginStateBlock` refreshes, services the
observed CPU-write locks and reaches 118 EndScenes / 120 Presents with zero
exception markers. A subsequent real reset keeps all nine tracked adapted
resources alive; four directly hashable textures retain byte-identical contents
and a generation-1 texture remains actively bound in generation 2. The research
two-slot transport then cancels a deliberately stalled generation at reset and
completes 12/12 submissions in generation 2 with ready=done=12, 12 D3D12 copies
and zero sampled mismatches. These are **live-tested research observations**;
they do not promote this mod's startup, transport, visual, performance or headset
state. See [resource census](research/COJ_D3D9_RESOURCE_CENSUS.md).

**Host-tested production compatibility:** the focused Ex compatibility test
reproduced `D3DERR_INVALIDCALL` for Chrome's observed 262,144-byte MANAGED
WRITEONLY vertex buffer and 131,072-byte MANAGED WRITEONLY INDEX16 index
buffer before production adaptation. The compatibility layer now maps those
observed profiles to DEFAULT while preserving their usage and buffer metadata;
both allocations and Lock/Unlock succeed in the test. A second regression
case reproduced loss of the creation hook after BeginStateBlock and now passes
with immediate HookRegistry reacquisition before the next engine call. Vtable
slots 26, 27 and 60 are compile-time checked. These allocation/Lock tests remain
host evidence; the physical run above adds bounded production startup/reset
evidence. The corrected presenter passes startup and normal-quit shutdown;
bounded cadence and stereo visual pairing also pass. Preserve any concrete production-only
divergence without restarting a broad resource census.

Locomotion diagnosis is closed. Physical measurements established that native
normal movement, the walk modifier and jump are not materially reduced in VR.
The decisive comparison was `139.949 Hz` vanilla versus `138.117 Hz` with both
VR eye renders active and only readback/copy disabled. The old full transport
ran at about `83.473 Hz`. This causally localizes the perceived slowdown to the
old capture/presentation boundary; movement, input and physics are not an active
remediation target.

Independent stereo, tracking, recenter, snap-turn and locomotion/jump parity
remain valid. The separate menu-pointer, arm-continuity and weapon-alignment
product gates remain open. Body, UI and weapon acceptance are
outside this transport run; normal-quit shutdown is now live-tested.

## Production D3D9Ex startup/reset gate

**Passed for the exercised production build/host:** Ex startup, sustained
Present, flat capture/presentation and outer finalization are live-tested;
videos/menu visibility in PS VR2 is headset-validated. The earlier production
run separately established a successful reset. Shared stereo and inner shutdown
remain outside this completed startup acceptance.

Run one fresh `prepare -StartupOnly`, one Call of Juarez process, one `finish`.
Observe startup videos and the main menu, then close the game normally. Do not
enter gameplay in this run. `StartupOnly` disables native stereo and camera
tracking: the expected image is a flat virtual screen **inside the visor**.
Seeing only the desktop or Steam's generic theater does not establish the mod's
presentation. The startup verifier requires the
game-visible Ex factory/device identity, at least 90 successful Presents, flat content
capture and proxy finalization. It does not promote shared transport or headset
state. If startup succeeds, the separate transport acceptance gesture below
becomes the next gate.

### Bounded transport acceptance — passed

Shared publication/copy and explicit-pose new-frame submission have been
live-tested in production gameplay, including resource drain and complete
normal-quit finalization. Measured update/stereo cadence and operator-confirmed
stereo depth/head-turn stability now pass for this exercised build/host.
The procedure below remains the regression gesture; another identical run is
not required to advance. Sustained pacing and pending-frame reset/device loss
remain separate renderer gates. Body/UI/weapon product work can proceed without
reopening the completed locomotion/input/physics diagnosis.

Use one fresh `prepare` without `-StartupOnly` or `-BodyIkAtStart`, load a save,
perform the short acceptance below, close normally and run `finish`. Body/UI
product acceptance and the locomotion comparison battery are outside this run.

For a future cadence regression, prepare normally, then enable the existing
`vr-full` trace before launching the game manually:

```powershell
pwsh -File E:\call_of_juarez_vr\tools\vr_test.ps1 prepare
pwsh -File E:\call_of_juarez_vr\tools\set_movement_diagnostic.ps1 -GameDirectory 'C:\Program Files (x86)\Steam\steamapps\common\Call of Juarez' -Mode vr-full
```

Load a save and spend 30 seconds in ordinary gameplay, checking each eye for a
current, distinct image and doing slow/fast head turns. Quit normally, then run
`pwsh -File E:\call_of_juarez_vr\tools\vr_test.ps1 finish`. The retained movement
summary must contain the `vr-full` phase with update/stereo-pair cadence; an empty
phase list cannot establish this gate. Body IK stays disabled for this profile.

| Area | Required observation |
| --- | --- |
| Startup | process reaches flat theater without a D3D9Ex/device crash |
| Native stereo | both eyes show distinct, correctly paired current images |
| Tracking/pose | normal head rotation/translation remains stable with no pull/snap-back |
| Minimal gameplay | a few seconds of ordinary movement remain responsive; do not repeat the locomotion battery |
| Cadence | update/stereo-pair rate clearly exceeds the old approximately 83 Hz path and approaches the 138 Hz readback-off reference |
| Transport | shared frames and D3D11 copies advance; producer/consumer wait stay zero; no classic fallback, readback, eye mismatch or ring exhaustion |
| Shutdown | close through the game's normal quit route; pre-exit hook installs/completes/restores, presenter reports `shutdown_complete=true`, pending GPU copies drain without abandoned leases, and transport/presenter summaries plus `run_end` are emitted |

Failure in one area does not promote that area, but independent observations may still be recorded.

## Closed locomotion diagnosis

The retained diagnostic tools may be used to report update/stereo cadence for a
transport run, but the comparative locomotion protocol is complete and must not
be repeated as an input-tuning exercise. Durable physical results are:

| Mode | Update Hz | Stereo-pair Hz | Normal / walk cm/s | Jump |
| --- | ---: | ---: | ---: | --- |
| Vanilla | 139.949 | n/a | 503.056 / 256.109 | 58.7-60.8 cm, 0.77-0.78 s |
| VR full, old CPU transport | 83.473 | 83.502 | 498.451 / 248.325 | 3 observed |
| VR input-off | 70.692 | 70.726 | 489.064 / 247.893 | 62.623 cm average, 0.781 s average |
| VR readback-off | 138.117 | 137.705 | 499.113 / 248.360 | 3 observed |

These measurements rule out native movement speed, native jump amplitude,
duplicated gameplay-input ownership and the second eye render as the primary
cause. The old per-frame GPU-to-CPU transport is the demonstrated cause.

## Current feature state

| Contract | State | Durable limit |
| --- | --- | --- |
| Exact camera -> view/projection -> renderer path | live-tested | camera path reaches the renderer used for physical stereo |
| Complete two-eye ChromeEngine render | live-tested | distinct eye rendering and SteamVR submission observed |
| Physical eye scale | headset-validated | validated at the game/XR unit boundary |
| HMD yaw/pitch/roll and positional offset | headset-validated | exercised repeatedly in stereo gameplay |
| Explicit render-pose submission | headset-validated | removed the previous head-turn pull/snap-back artifact |
| Recenter | headset-validated | controller recenter exercised physically |
| Exact snap turn | headset-validated | controller turn exercised physically |
| Flat-theater startup/load -> native stereo | headset-validated for exercised path | startup and level transition reached gameplay safely |
| Local head/hair suppression | headset-validated for HMD view | shadow behavior remains separate |
| D3D9Ex GPU-resident native-stereo transport | bounded acceptance passed; measured cadence and headset stereo validated | shared frames submit with explicit pose and drain on normal quit; production cadence approaches readback-off reference; sustained pacing/tail latency and pending-frame reset/device loss remain unproved |
| Classic-D3D9 resource pool census | live-tested research / bounded startup coverage | independent startup census records 1,552 successful creations and eleven MANAGED calls across texture/cube/VB/IB profiles with immediate hook restoration; broader gameplay/device-loss coverage remains open |
| Classic-D3D9 CPU transport fallback | implemented / unpromoted | retained only when D3D9Ex/device sharing is unavailable; any use during the physical gate is a failure |
| Inner presenter finalization | live-tested normal quit | exact-build pre-DestroyGame owner stop/join, complete GPU drain, capture cleanup, hook restoration and final summaries verified; abnormal exit/device loss remain unproved |
| Native analog locomotion | headset-validated diagnostically | normal/walk speed matches vanilla within the measured transport runs; do not retune input or movement |
| Native jump action | headset-validated diagnostically | measured apex/duration matches vanilla; do not retune jump or physics |
| Locomotion divergence instrumentation | host-tested and physically completed | retained for cadence reporting; causal investigation is closed |
| Room-scale visual body compensation | live-exercised technically | vertical pelvis drift was removed; visual walking parity remains open |
| Physical-walk visual animation | planned/open | should reuse native locomotion animation semantics without surrendering collision ownership |
| Sense tracking in game space | live-tested | left/right controller transforms reach the backend |
| Visible arm writer/restoration | live-tested | geometry changes and restoration are proven |
| Body IK continuity/anatomy | live-exercised / rejected | safety must not produce repeated visible fallback to default animation |
| Native reload ownership | host-tested | VR writes yield during native reload state |
| Flat-menu pointer | live-exercised / rejected | current ownership model remains unusable in-headset |
| Cross/Circle/L2/R2 UI actions | host-tested follow-up | physical acceptance still pending |
| Loading continuation | host-tested | native loading input route is mapped |
| Controller-origin UI beam | live-exercised / visible in headset | pointer visibility is observed; origin/alignment and accurate controller-only UI operation remain pending |
| Controller-owned weapon direction/visual origin | host-tested | final physical firing alignment still pending |
| Sense tip direction convention | live-tested diagnostically | local `-Z` is the demonstrated pointing direction |
| Temporary controller alignment ray | planned diagnostic | diagnostic only; must not become production ballistics |
| Physical gun origin/direction | pending/rejected | production shots must originate from the visible weapon/barrel |
| Supported end-to-end VR release | planned | project remains pre-alpha |

## Evidence policy

Only conclusions that remain useful across sessions belong here. Raw headset-run manifests, hashes, local video paths, telemetry counts, process IDs and agent handoffs stay under ignored `work/` and may be discarded once their conclusions are represented by code, tests or the durable documents above.
