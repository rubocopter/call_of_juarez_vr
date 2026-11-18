# Architecture

## Product boundary

Call of Juarez (2006), Windows x86, is the reference implementation for native
stereo PCVR through D3D9 and OpenVR/SteamVR. The current presentation combines
tracked head and independent native hands with the game's torso and legs.
Connected-arm IK remains a rejected diagnostic presentation. The current
manual session preserves native ammunition authority with separate exact-class
unit, indexed and clip policies. New families remain host candidates pending
live and physical qualification. Launcher installation defaults
and operator opt-out are described in the distribution contract below.
D3D10, OpenXR and additional games are separate future/experimental tracks.

Only game-neutral policy may be reused from Penumbra VR Framework. HPL layouts,
addresses and hooks do not apply here. Chrome Engine contracts remain CoJ-specific
until a second game independently demonstrates them. [VALIDATION.md](VALIDATION.md)
owns acceptance state and unresolved physical gates; [ROADMAP.md](ROADMAP.md)
owns development priorities. [Documentation index](README.md) maps the
exact-game research and release boundaries.

Comparative source reviews of external VR projects are consolidated in the
ignored local `work/research/VR_REFERENCE_RESEARCH.md`. They guide agent
hypotheses, but introduce no runtime dependency, native contract or headset
acceptance. The [documentation index](README.md) explains this local-only scope;
source/deployment audit history and per-run checklists also stay in `work/`.

## Layering

Neural/DirectNR is a deferred optional experiment. Its tested inference path is
retained for explicit developer use, default Off, with no external dependencies
staged by ordinary preparation. The operator finds no worthwhile image benefit;
the live retained-frame presentation remains rejected for cadence/quality.
Core VR work does not depend on resuming this track. The implementation below
records its existing boundary, not an active development or support commitment.

Renewed operator-authorized autonomous research is limited to offline DLSS/Neural
feasibility until fresh game inputs and delivery are qualified. The image lab
can compare one-to-four consecutive NR model passes, reports effective whole-eye
coverage and separates D3D12 timestamp averages from CPU roundtrips. It does not
change the deferred/default-Off game presenter or the rejected frame-hold policy.

The optional `cojvr_neural_lab` library and separate x64 `tools/neural_host`
companion adapt Penumbra DirectNR over a versioned CPU-pixel pair protocol.
Standalone probes and an explicitly opted-in D3D9/OpenVR candidate consume it;
it is not a Super Resolution consumer. Pair identity includes frame/pose/device/generation,
settings revision and history epoch. Pending input remains immutable until
completion; warmup is asynchronous and disposable, and late output is drained
and discarded before fresh history. GPU resources are process-private. The D3D11
fixture composes the proxy residual onto an untouched full-resolution original,
preserving encoded-color semantics and source alpha. See the
[laboratory instructions](../tools/neural_host/README.md) and
[validation boundary](VALIDATION.md#standalone-neural-laboratory).

The candidate presenter reads `cojvr-neural.ini` once at startup; absent/invalid
configuration keeps Neural unavailable. A separate persisted native Options > Game
Neural Off/On request defaults Off and does not change Super Resolution requests.
The visible status/count reports actual startup, warmup, processing, commits and
fallback, independently of saved intent. Graphics preferences migrate through v5
with legacy files retaining Neural Off. F8 is no longer the operator control.
Only fresh native stereo with valid captured HMD/physical-eye optics and current
tracking/focus is eligible; theater, scopes and the independent bank bypass it.
HMD and eye rotations compose in runtime coordinates and FOV stays in radians.
Recenter, requested artificial turn, metadata gaps and source changes reset history.
Model startup/warmup keeps original stereo. The renderer owns aspect-preserving
proxy capture with nonblocking maps and one retained query per captured pair.
Begin/Poll capture, inference and resolve preserve the original private colors,
pose and value-only HUD metadata until completion or explicit discard; polling
never restarts the work. Pending work holds the already displayed native pair
within the existing 100ms capture-age fence; the native mailbox can drop newer
same-context packets, releasing their producer leases. A different generation,
mode, scope, input context or settings revision cancels publication. A completed
residual commits only to that same displayed frame, after which comfort/overlays
are recomposed once using its own metadata; no native source copy, haptic event
or frame-upload event repeats. No D3D9 lease or borrowed source pointer is retained.
Warmup output remains disposable and does not hold native presentation.
Cancellation drains GPU work before private images/queries are reused. A late
response is never painted onto a newer capture; repeated failures disable
processing until an explicit Off/On retry. Device removal revokes submission rather than admitting a
partial pair. Neural runs before native comfort/scope and mod HUD composition;
game HUD already baked into source color remains part of the experiment.
This retained-frame candidate is physically rejected for image cadence and
perceived quality. Artificial turn cancels its pending work and releases the
hold, so stick-dependent smoothness is not evidence of successful Neural pacing.
The current code documents an experimental implementation, not an accepted
presentation contract. A replacement must preserve fresh native delivery and
exact source/result identity together; an old residual cannot be attached to a
new frame merely to remove the hold.
Preparation journals/restores the UTF-16 developer configuration and records
external helper/model/forwarder hashes per run. They are not public package assets.

The separate `CoJNeuralImageLab` executable consumes supplied RGBA crop pairs
offline with explicit model scale and effective settings. It has no presenter or
game hooks. Human/head selection belongs in the exact-game adapter; visible-pixel
masks, native-density crop transport and same-source completion remain independent
requirements. See [human targeting](research/COJ_NEURAL_HUMAN_TARGETING.md) for the
shipped NPC query evidence and unresolved visibility boundary. A model's automatic
skin control does not provide that game-owned mask or eliminate full-scene cost.

| Layer | Owns | Must not own |
| --- | --- | --- |
| Shared runtime | tracking/recenter, metre-space eye data, logical actions, haptics, gesture policy | exact game addresses/classes |
| Renderer backend | device/frame ownership, capture/transport, render targets, compositor | player/weapon/UI semantics |
| Game backend | exact camera, actor, skeleton, UI, input, weapon and physics seams | reusable compositor policy |
| Diagnostics/evidence | source/build/deployment correlation, hook/device identity, telemetry | gameplay policy |

## Safety and provenance invariants

Game-specific mutation requires recognized executable/engine/archive SHA-256
identity, not filenames. Unknown builds, ambiguous ownership, invalid bases/poses
and failed restoration fail closed. Hooks retain object/generation identity,
preserve foreign replacements and undo only their own changes.
Cleanup also recognizes a slot already pointing to its exact retained original,
without rewriting it. A foreign destination or incomplete memory-protection
restoration still reports failure. Original targets remain available to in-flight
callbacks after retirement. This cleanup case has deterministic host coverage.

Camera-owned pelvis and diagnostic arm transactions also retain the bridge's
original actor reference. Player replacement, clear and reset wait until every
body restore verifies and releases its lease. A failed or partial write keeps its
preimage and element indices for retry; entry restoration precedes Refresh and
tracking-disabled exits. Generation mismatch never redirects a restore to the
current replacement actor. Pelvis and arm restores rebase their captured world
positions by the retained actor's translation. Failed pelvis rollback also stops
inner hand/weapon writes, fire and eye rendering in the same invocation; the next
entry retries restoration before forwarding native work.

Exact-game object identities retain all 32 bits across JNI's signed `jint`,
including LAA high-bit values; zero/all-ones remain invalid. Reload ticket and
physical Hands ownership use equality and native context rather than signed
positivity. General JVM/native LAA compatibility remains separate from the scoped
functional recovery accepted in [Validation](VALIDATION.md); host regressions do
not establish physical coverage of every identity range.

Each physical candidate binds source state, build manifest, deployed hashes and
one fresh run ID to one game process. `tools/vr_test.ps1 prepare` builds, checks
and stages; `finish` collects evidence and restores original assets transactionally.
Prepare/finish hold an exclusive local workflow lease for their full duration,
preventing overlapping verification, collection and restoration.
Canonical native telemetry checks use only the interval from one exact run/build
start to its matching later end; duplicate, foreign or reversed markers reject.
Collection preserves that rejection and cannot mark an invalid interval complete.
The operator starts and closes the game and SteamVR. Retained inventory and
backup identities are checked before any recovery mutation; missing or damaged
backups preserve the journal instead of permitting partial restoration.

Directory recovery inventories hidden/system files and rejects unrecorded
contents before deletion. File-only manifests authorize ancestor directories,
not additional empty subdirectories. Fresh staging rejects an original directory
that cannot be represented by this recovery inventory before moving any assets;
existing ambiguous journals retain their originals and recovery evidence.

The optional Large Address Aware derivative changes only the PE flag of the
recognized x86 executable. Original and derivative hashes are pinned separately;
code/RVAs remain identical and the executable is journaled/restored. This improves
user address capacity, not VRAM. Exact identities and allocation boundaries live
in [the resource census](research/COJ_D3D9_RESOURCE_CENSUS.md).

## Exact-build integration

The active renderer uses D3D9Ex shared textures for D3D11/OpenVR transport;
classic D3D9 remains the compatibility reference. The exact observed MANAGED
texture/cube requests translate to DEFAULT|DYNAMIC; WRITEONLY VB/IB requests
translate to DEFAULT while preserving usage, format/FVF and size. This is bounded
compatibility, not generic MANAGED emulation. Successful BeginStateBlock returns
immediately reacquire owned hook slots because the engine can rewrite its vtable.
A periodic watchdog alone cannot protect the next engine call.

The optional hardware-depth and render-resource candidates require queryable
D3D9 device state. Their startup factory policy removes `D3DCREATE_PUREDEVICE` while retaining all
other requested behavior flags through classic creation and the Ex bridge or
fallback. The pre-create callback records requested flags and the post-create
callback receives effective flags. Ordinary factory policy preserves flags.
The same developer probe can retain point-sampled color/packed-depth previews
at the admitted terminal scene copy. Private paired FP16 images share the depth
lease's producer fence and ownership. A 512-pixel maximum axis bounds diagnostic
storage, readback and run-local export. The evidence collector inventories
committed and partial files. A manifest published after both planes supplies
frame/pose/eye/device-generation provenance and never grants temporal admission.

The renderer utility separately offers default-off `capture_color`: a private
full-size FP16 color copy per depth slot. It admits only native FP16 input,
includes both planes in the bounded storage budget, restores native state and
publishes both borrowed handles only after their common producer query completes.
No native color texture is shared. A retained immutable lease covers worker
access to both planes until consumer GPU completion; final producer release
remains on the D3D9 owner thread. Ordinary play keeps this option off; the
explicit `SampledSceneDepth` developer probe enables full-size color and a bounded
worker copy audit. The separate `TemporalSequence` mode retains reduced color,
depth and final-eye previews with the depth carrier, keeping full-size color
disabled. Preview observations cannot enter the full-color worker packet route.
The unchanged 256 MiB capture budget admits this mode at 2804×2860, whereas
the two-slot full-color mode exceeds it. The adapter exports source packets containing the
admitted frame, pose, owner, integrity, native generation and capture-resource
epoch. That epoch is not an input/settings/history authority.

`SceneDepthTransfer` retains one exact matching eye pair on the producer thread.
The worker receives only value metadata, shared handles and separate existing
producer-completion tokens for color and depth. Both copy acceptance and both
GPU retirement proofs are required for delivery; invalidation withdraws delivery
without granting early resource release. Stale completions are consumed once,
and ordering fences reject replay, including relabelled snapshots. Worker bridge
generation uses the monotonic capture-resource epoch so recreated pools retire
opened-resource caches even under an unchanged native device. Unproven teardown
quarantines the retained pair for process lifetime; callers must not recreate a
failed owner to bypass that backpressure. No game-source qualification, temporal
history admission or presenter-thread inference follows from this transport.

`SceneDepthWorker` owns its D3D11 device/context and optional NGX pair session on
one persistent thread. Startup is asynchronous and one outstanding job bounds
backpressure. Producer polling releases native sources after both GPU copies
retire, independently of slower inference on private worker textures. Explicit
shutdown joins outside render/presenter hot paths; unproven copies remain
quarantined. A stopped worker cannot silently restart its authority domain.
Copy-only jobs stop the optional reconstruction session and release upload
masks before source allocation. Returning to reconstruction restarts the helper
on the same worker thread with fresh history; restart failure faults the worker.
Blocking lifecycle work is followed by fresh authority and capture-age checks.
Existing unproven-GPU retirement quarantine remains authoritative.

Capture timestamps originate at scene entry and travel with each eye. Requests
must retain the oldest source time and supply an explicit age bound. Settings,
input and history authority are separate from resource epochs; any change or
unknown available authority revokes delivery. Restoring earlier values does not
restore an old result. Passive OpenVR input-authority reads do not consume
buttons. Worker reports contain only identified diagnostic samples, not image
handles for presentation. Reconstruction requires explicit qualified guide
metadata and per-eye current-color masks. The game audit exports diagnostic
observations and grants no reconstruction admission.
History also resets when native source identity changes or a reconstructed
result is rejected at delivery. The guide rejects reprojected samples outside
the previous clip volume before emitting motion or allowing history.

The developer probe sends matched source packets to the worker in copy-only
mode. Exact center pixels from both private FP16 source planes are compared
against worker color/depth samples, with native scope identity and independent
producer/worker thread IDs. The bounded schedule and preview exports remain
diagnostic. An optional final BGRA8 preview uses the same RT0 surface as the
successful native eye capture and attaches only to its matching frame, pose,
eye and device generation. HDR, packed depth and final color share the
diagnostic lease and its producer completion proof. Pause, tracking, settings
and native owner observations fence the
audit; reset/exit retire the worker before the source adapter. CPU wall-clock
timings are not GPU timestamps or game-performance evidence. Ordinary game
output and the compositor remain native.

The sampled source can export verified per-eye native optics with the
preview. The depth and final-color boundaries independently read the same
renderer's row-vector view-projection. The exact game adapter handles metre
conversion and finite versus infinite far depth. Missing or mismatched
optics remain unqualified diagnostic input.
The completed-eye read retains the active exact eye scope and its positive
scene-camera witness after native renderer-camera restoration; the scene copy
still requires a current renderer-camera match. Final-read validity and exact
depth-optics equality are distinct diagnostic fields. No earlier matrix is
substituted for an unavailable final read.
The startup-only temporal sequence variant uses two bursts of four adjacent
frame attempts under the same eight-frame budget. It exports preview sources
directly and releases their leases after readback. The worker copy audit has
its own startup mode. Pending GPU work can leave gaps in a burst, and the
evidence analyzer requires consecutive completed stereo sources with matching
optics before qualifying that capture.

Ordinary OpenVR runs sample elapsed CPU wall time around complete eye wrappers.
Frame, native owner, device generation, observation epoch and thread bind each
pair. Reset or an incomplete scope prevents mixed samples. Timing is bounded
and summarized at shutdown. The observer excludes capture and submission and
does not measure draw counts or GPU execution.

The native mixer owns listener selection and output. A future center-head
listener must supply pose at an audio-only boundary. Derived camera state
also drives native spatial queries before collection.
See the [camera research](research/COJ_CAMERA_PATH.md#native-audio-listener-ownership).

Successful texture-allocation census callbacks, per-resource destruction hooks
and periodic address-space scans require the startup resource-probe opt-in.
Ordinary compatibility retains failed-result diagnostics and their address-space
snapshot, while omitting the successful-event formatting, file writes and
diagnostic resource registry. Translation, BeginStateBlock reacquisition and
owned restoration are independent of that choice.

The journaled `code.pak` patches target verified original classes only and retain
native fallback paths. The `Data0.pak` player-mesh partition is similarly hash
pinned. All original archives restore byte-for-byte.
World FX definition creation/release uses null tests rather than signed positive
checks in the exact FXEmitter derivative, preserving LAA heap-pointer bits.
Runtime emitter handles and damage/timer behavior retain native semantics;
[world-fire research](research/COJ_WORLD_FIRE_DEFINITIONS.md) owns the boundary
and its unproven attribution to the invisible campfire report.
Optional quick/automatic save-preview capture is suppressed at its exact invocation while ordinary save
serialization remains native; see [save thumbnails](research/COJ_SAVE_THUMBNAIL_PATH.md).
Pause entry also suppresses both optional capture/ForceRender branches of the
exact `TakeScreenshotsOnLevelBreak` helper. Its HUD restoration, input cleanup
and delayed native pause scheduling remain intact. The existing null-background
path releases/hides the pause screenshot rather than loading a missing/stale
image; manual saving removes any stale destination preview without copying the
omitted pause image. Native save serialization remains unchanged. This closes
an observed pause screenshot allocation path in source; physical recovery remains
a separate gate.
Full `vr_test prepare -BodyIkAtStart` defaults to the reversible exact-build LAA
transaction, matching the established gameplay recipe. Explicit
`-LargeAddressAware:$false` retains an intentional original-image diagnostic;
startup-only/non-body preparation keeps its existing default. The executable
SHA-256, backup and restoration contracts stay authoritative. This increases
the available x86 VA range on the host; it does not qualify arbitrary allocation
pressure or general high-address JVM/native behavior.

## Camera and stereo

`Camera -> View/Projection -> full ChromeEngine3 render-view wrapper -> D3D9 target -> presenter -> SteamVR`

The natural camera remains authoritative. Each eye uses a scoped transient
camera/view transaction and restores it afterward. The exact source basis is
right/up/forward/position; no cross-product reconstruction of its first axis.
Reflected/non-rigid bases fail closed. XR data stays in metres; only the CoJ
adapter converts to centimetres.

Both eyes execute full wrapper `0x00030FB0`, including its guard and post-core
work. Core-only `0x00030E00` cannot supply a valid second-eye result. Capture
reads each currently bound color RT0, not a swap-chain backbuffer that may echo
both eyes. Frame identity includes the exact HMD render pose/sequence submitted
through OpenVR explicit-pose submission. See [camera contracts](research/COJ_CAMERA_PATH.md).

The .3 OpenVR projection correction follows the owning
[projection contract](research/COJ_CAMERA_PATH.md#openvr-projection-conversion).
Complete-matrix and forward-menu host regressions cover asymmetric vertical
optics and symmetric PS VR2 controls. Community physical feedback confirms the
corrected Beyond gameplay view; a Quest report is positive without a specified
model. These observations do not qualify broad headset/controller support or
menu gaze alignment.

Scene target allocation remains engine-owned. Capture allocates from the current RT0
description, and native-stereo D3D11 eyes preserve that extent through equal-size
copies. OpenVR's cached recommended extent does not resize ChromeEngine targets.
Independent eye resolution must qualify native color/depth/intermediate ownership
before the full render-view passes, retaining desktop/theater and lease/reset
boundaries. [Resolution research](research/COJ_VR_RENDER_RESOLUTION.md) owns the
source trace, memory constraints and incremental implementation criteria.

An explicit native-resolution trial may instead change the engine video setting
at startup, including menu/theater source dimensions, with byte-exact restoration
through `finish`. It retains ordinary stereo/transport and HMD optics, keeps the
independent bank off, and requires measured native draw/capture/output extents;
configuration or window size alone does not qualify rendering resolution.
The physical workflow additionally accepts explicitly opted-in startup
Auto/pixel-percentage/Manual proposals through the same video transaction.
The development workflow preserves resolution unless explicitly opted in; the
public launcher uses the saved preference, or Auto when none is saved.
Factory-only native caps checks reject
unsupported dimensions before application; unavailable queries preserve selected
size. This experimental route does not qualify Background/Scene recommendation
identity, theater allocations or x86 memory headroom for public releases.
See [profile contracts](research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md).
The existing launcher owns a separate bounded `resolution.json` preference;
the runtime's cinematic `settings.ini` and its saver stay unchanged. Selection
persists intent only. Installation/PrepareSession resolve and apply it while
closed, with a session video journal layered over the install-time FSAA journal.
Restore the session layer before replacing it or restoring installation originals.
Interrupted metadata publication retains a pending run identity for recovery.
The .3 product default is Auto only for a missing resolution preference; every
explicit saved choice, including Manual and Preserve, survives updates. Auto
uses the helper's recommended per-eye extent, not panel-native pixels. The
operator must start SteamVR before Install/update or PrepareSession for Auto or
percentages. Unavailable helper queries or incompatible caps preserve the selected
game size with a visible explanation, retaining intent for a fresh retry at the
next preparation. Explicit Preserve never queries a headset. This experimental
default does not qualify Background/Scene identity, memory or other headsets.
The exercised PS VR2 reference uses the operator's accepted Manual 2804x2860
preference as its local default. This does not impose that size on new users or
change the meaning of SteamVR-relative profiles.

An experimental startup-only private bank now implements fixed 2560x1440
isolation and 2560x1920 eye rendering with the original 2560x1440 desktop.
The exact adapter borrows registered CTexture payload fields only during the
full wrapper and capture, restores renderer matrices/material sources/cache
values and fog, and never writes native reference counts or queue containers.
Natural queue producer/playback witnesses qualify completed CPU command
consumption; GPU capture/consumer fences remain separate. Two eye histories
are private; other admitted scratch resources are sequential. The transport
prepares shared-only resources before native mutation. Four D3D11 output
textures reserve separate native/theater pairs for process lifetime; retained
ownership does not prove compositor retirement. Failed restoration retains
its preimage and resources, and vetoes Reset/ResetEx until retirement succeeds.
Actual native rendering, image quality and continuity remain physical gates in
[Validation](VALIDATION.md); host GPU fixtures do not qualify the game or visor.
A startup-only, bounded read-only D3D9 binding probe brackets the exact full eye
wrappers and correlates device/generation/frame/owner identity. Reset/ResetEx
invalidation and conservative offline analysis do not establish allocation
ownership, programmable projections or independent-size admission. Disable the
opt-in probe for performance qualification.

The exact-build [PP target ownership map](research/COJ_PPFX_TARGET_OWNERSHIP.md)
identifies alias-aware native buffer ownership and additional renderer color,
depth, default-target and viewport caches. Native reconstruction destroys the
old family before allocating; it is not an independent per-eye transaction.
Logical dimensions also drive screen quads, with different rounding from buffer
dimensions. These static contracts constrain a future private bank but do not
admit it without runtime correlation, effect/constant and retirement qualification.

The same default-off diagnostic now copies bounded native PP descriptors and
renderer cache identities through an exact-build game adapter and checked
own-process memory reads. No copied COM identity is called or retained. Begin/end
structural snapshots correlate with D3D9 bindings, preserving alias names and
unknowns without claiming atomic lifetime or independent resource ownership.
Two pair attempts are reserved per gameplay/settings/movie route within the
existing six-pair limit; absent routes remain unobserved.

## Physical hand contact

Physical Hands contact uses game-neutral per-hand input/motion admission with
game-owned collision geometry. The CoJ adapter reconstructs original knuckle
skin witnesses from current displayed bones/weights and supplies independent
current-pose movement segments. A scoped native contact query precedes one
eligible original attack; no completed historical curve or animation deadline
delays collision. Native permissions, cooldown, reactions and damage remain
authoritative. Query endpoints are private copies because the native trace
writes the collision endpoint. A contact latch prevents held/static repetition;
ownership/tracking loss retains available-release barriers.

## Presentation modes

`native_stereo` contains two game eye renders. `flat_theater` projects native
Present content onto a finite-depth head-anchored plane for startup, menus,
loading, scripted in-game movies by default and blocking native UI. Per-eye geometry and asymmetric optics determine
texture extents; identical centered images or fixed padding are insufficient.

A qualified immersive movie can render passive stereo while the bridge retains
a pending release from gameplay. Admission checks the current movie epoch after
cleanup against the captured sample. Camera, body and eye-resource restoration
remain publication prerequisites. Native skip and resumption of gameplay still
require cleanup. A skip edge blocked by pending release is consumed once. Missing
native movie ownership retains theater.

At the explicit 2804x2860 native trial size, positively observed existing global
`MenuMain` frames may carry a 2560x1440 presentation reference, only with verified
`IsActuallyVisible()` and valid native `GetThisID()` identity. The presenter fits
the captured aspect into the reference angular box inside the existing output
textures. D3D11 samples one source SRV texture onto those targets; no full-image
CPU resampling runs on the submission thread. Original capture pixels and native
cursor coordinates remain unchanged. Angular changes never change the allocation
key. Source capture and gameplay remain 2804x2860. Other flat routes keep their
existing layout. Reference/owner changes cancel pending UI pointer claims and
retain the existing release barrier; absent exact menu ownership uses defaults.
The menu compositor owns no output targets or retained output views. Its source
texture is keyed by actual source dimensions/device and retired with the presenter.
A failed menu preparation/draw disables this adjustment for the session and clears
its pointer geometry metadata, preserving the ordinary usable menu presentation.

Game callbacks produce captured frames; the presenter owns compositor cadence,
scene focus, repeat behavior and submission. Immediate flat capture reserves a
mono SYSTEMMEM ring, with both eyes borrowing one producer lease. Consumer
completion permits owner-thread unlock/reuse. Matching same-device Reset retains
the reservation with fresh generation metadata; incompatible replacements defer
until leases release. No persistent DEFAULT resource or duplicate CPU eye image
is retained for this path.

## Flat UI ownership

The active exact `LawmanModuleSingle` supplies `IsTimerFreezed`,
`IsMainPlayerAlive`, `IsMenuMissionEndVisible` (menu 38) and
`IsAnyMoviePlaying`. The last getter reads the playing movie or visible owned
black belts; it never starts/updates a movie. A stopped timer, dead player,
visible mission-end UI, scripted movie or unavailable observation yields native
stereo/body/gameplay ownership and captures native Present in flat theater.
A post-input check aborts the frame before body/eye writes if dispatch acquired
blocking UI. Death and movie ownership cannot be inferred from timer state alone.
Movies can run with a live timer, scripted camera and module subtitle sprite.
Yielding before stereo/body writes keeps the complete naturally rendered native
Present, including those subtitles, on the existing flat screen. The script moves
its image inside that screen; it does not drive the tracked viewpoint. End-of-movie
recovery retains the existing available-release barriers and scoped restoration.

The persisted VR cinematic preference can opt a verified live scripted movie
into camera-only native stereo. This route uses the natural scripted camera,
tracked head and the full wrapper for each eye, restoring camera state after
each pass. It cancels gameplay/reload/punch ownership and performs no tracked
body, hand or weapon writes. Invalid/dead/mission-end/frozen owners retain flat
theater; a frozen movie cannot safely be distinguished from pause by these
observations. Native visible black belts are also a positive movie-presentation
owner when `m_cMoviePlaying` is null. An exact current skippable sequence can
then own the camera-only route and subtitle cache. If both targets are absent,
the observation remains valid but retains theater to preserve the complete
native subtitle pass. No case grants gameplay. Native observation/read/lease failures
remain fail-closed. Transient capture pressure retains the last valid stereo image
within the existing 250 ms freshness window; admission alone does not extend
that window in passive movie/settings routes. A presentation owner or mode
change retires outstanding captures, and a newly queued fence cannot declare
the previous mode's displayed image current. Flat recovery waits for a newly
published complete pair. Sustained staleness falls back to native Present;
death, pause, ownership loss and failed native restoration invalidate immediately.
Immersive movies copy subtitles from the exact module's naturally scheduled
`OnShowSubtitle` writer. The hash-pinned archive transformation retains its
normalized key, ID, current movie and (only for the single-player subtype) its
current skippable sequence in transient fields. Wrong-ID hides preserve
newer lines; accepted hides clear the cache, and movie start/end/destruction clear
before original callbacks. Original native sprite/timing behavior stays intact.
The game-thread reader requires the same active module, movie or sequence, native/cache IDs,
visible subtitle parent and native subtitle setting. It localizes with `Text.Get`,
rechecks ownership and copies bounded UTF-16 directly into that captured pair.
A real movie takes precedence over a sequence. A cached movie line cannot be
reinterpreted as sequence text after its movie disappears, even with a reused
subtitle ID. The exact writer and shipped JVM verifier cover the additional
sequence cache; no native subtitle timer or sprite callback is replayed.
The immersive movie route has its own text cache. Theater preserves the
naturally rendered pass. For a local opt-in diagnostic, the presenter can
retain the lower half of both uploaded native eyes and the corresponding
composed crops from an accepted fresh submit. The observer matches a bounded
set of caption fingerprints against the same capture, pose and owner. Staging,
nonblocking readback and image export have independent lifetime bounds. The
observer does not change subtitle placement or native sprite visibility.

VR preferences are edited in native Options > Game. Post-.5 source removes
the held-Options quick editor, its OpenVR overlay, shared texture and redundant
cinematic writer. Options now forwards one fresh native pause press, with no
hold timer or release action. The versioned preference store, native OK/Cancel/
Defaults and saved cinematic/turning/weapon-guide choices remain authoritative.

The presenter retains a dedicated neutral-input release gate across startup,
focus/tracking/poll loss and native owner changes. Missing channels do not
fabricate releases of previously held controls; never-bound optional actions
do not block neutral input. `SettingsInputCaptured` now reports this availability
fence, not an open VR quick panel. It temporarily suppresses gameplay and native
pointer delivery until available buttons are released and sticks centered.

Immersive movies retain the natural-body camera-only route and block ordinary
gameplay/body/weapon mutation. Their proxy copies a separate fresh skip intent
before clearing UI/gameplay fields. Options/Circle/Cross and either trigger's
selection edge request the same native Escape consumer, with no repeated request
from the same tracking sequence. Pending edges carry their polled native owner
token; a gameplay edge cannot be combined with a newly observed movie owner.
The token also includes a bridge-owned movie epoch, established by JNI equality
against retained module/target references. The target is the current `Movie`,
or `m_CurrentSkipableMovieStart` only when no `Movie` exists. A known absence of
both retires the skip lease while preserving native black-belt presentation.
Replacing a target without changing
presentation flags invalidates the old input owner. Only that epoch leaves the
game adapter; no JNI reference crosses to the presenter. The two observation
references retire on movie exit/reset, with no native lifecycle mutation.
If an immersive capture temporarily falls back to natural theater output, its
controls retain this strict movie route instead of generic UI-back/pointer retries.
The game thread rechecks its active movie,
native `CanSkipMovie` and retained module/target identity before calling original
`OnInputKey(1,true,0)` and its release. Native restrictions, hint/loading and
mission-end ownership remain authoritative; no direct movie-end callback runs.
Scoped render-level restoration is required, and any attempted skip returns to
natural rendering rather than replaying the old camera snapshot into both eyes.
Refusal before entering native OnInputKey is distinct from an attempted native
key call, including a throwing call. A harmless refusal keeps camera-only stereo
only after reobserving the same admissible movie epoch; blocked or changed owners
retain natural fallback. Diagnostics distinguish native permission denial,
absence of a skippable sequence, bridge rejection and native-call failure.
Delivery does not prove a fade or skip completed. Theater keeps its existing route.

The natural-camera fallback has a renderer-only scope: its camera updater
forwards directly to the retained native updater without enabling/disabling the
relative pose tracker. A transient capture failure must not discard the VR base
or recenter hands on the next pose. Settings/camera capture backpressure may repeat
the last successfully queued pair within its ordinary freshness window without
renewing it. Failed native restoration invalidates it immediately.

The presenter intersects tracked `/pose/tip` with the flat screen automatically,
retains a valid hand owner and lets a fresh L2/R2 select that hand. No shoulder
activation is required. A valid tip missing the screen does not switch rays.
A hand/claim/click mailbox carries value-only coordinates to the game thread.

The presenter reuses bounded CPU scratch storage for flat-menu pointer uploads;
this changes allocation ownership only. It does not add native input authority
or establish a measured frame-pacing improvement.

The exact native sprite-tree dispatcher `0xC8F00` receives previous/new source
positions before the visual cursor is moved. `GetMousePos` observes native input
in X/Z, normalized to source X/Y; visual cursor echo is not proof of hover.
The global MainMenuModule UI is preferred for pause; an actually visible
`m_cYesNoDlg` is the modal root. Object existence alone is insufficient.
Passive discovery uses `FindUI`/existing fields rather than creating UI factories.

L2/R2 select through native input-context mouse press/release at `0xCC420`.
Cross retains global accept, Circle UI-back and Options pause. Back uses native
Escape routing, not `ShowPrevUI`. Intro/loading and pausing hints use their own
shipped consumers; HintManager belongs to GameMode through `LawmanModule.m_GameMode`.
Native input readback must agree within one pixel. Physical mouse activity has
priority for 1.5 seconds; observation continues while the VR beam is hidden.

Visible exact `MenuMissionEnd` (38) is a separate native input owner. Its static
text advertises Space to continue and Escape to quit; it has no Enter/mouse
consumer. A verified visible `LawmanModuleSingle` receives those press/release
events through its original `OnInputKey`, preserving its save/load/exit decisions.
Ordinary menus retain their existing UI consumers. Delivery alone does not prove
a retry or menu transition.

Queued clicks retain matching hand/claim/index identity through a confirming
Present. Failed delivery, owner/focus loss, navigation or UI-index change cancels
only the matching click. Held gameplay shoulders cannot cross into gameplay
until an available released sample. [Exact UI seams](research/COJ_UI_MOUSE_PATH.md)
record binary contracts and rejected sprite-only/Windows-input alternatives.

Source-image width/height changes also cancel the pointer's click and hover
confirmation, while retaining automatic aim. A local geometry generation rejects
an in-flight old cursor result, including idle deliveries whose click ID is zero.
The first resized packet cannot enqueue a select edge; a subsequent deliberate
press needs fresh native hover confirmation. Mouse-priority policy is preserved.

Global OpenVR button edges require an available release after startup, action
loss or explicit invalidation. A failed/partial input poll clears all global
edges and partial output, even when earlier reads succeeded. Presenter focus,
dashboard and HMD tracking loss invalidate these edges and pending UI actions.
Stereo presentation state likewise discards partial eye confirmation across
tracking, connection and lifecycle loss; this is state policy, not a claim about
rendered image alignment or frame-pacing improvement.

## Shared preferences and native settings

`runtime/vr_settings` owns one synchronized preference store for cinematic mode,
smooth/snap turning, optional weapon guides and manual reload intent. The native Options > Game editor
reads versioned snapshots and submits values through that owner. An older
editor cannot overwrite a newer edit. Runtime reads use atomic publication;
native references remain on the game thread. Version 1 retains its cinematic
choice and supplies smooth turning; version 1/2 files default guides off.
Accepted saves write version 4; versions 1 through 3 retain install-default manual reload.
Missing/invalid files use theater/smooth/guides off;
unrecognized or unreadable startup files are preserved, with session-only changes
and explicit save-failure feedback. Atomic replacement preserves the previous
file when persistence fails. The held-Options presenter editor is retired.

The hash-pinned native editor extends the existing `MenuOptionsGame` class and
`OptionsGame.scr`, shared by main/pause Options, with four comfort and ten graphics
selectors plus save and Neural status labels. Graphics exposes enable, sharpness, contrast,
saturation, gamma, spatial edge smoothing, a requested reconstruction mode,
startup render scale, mounted peripheral comfort and a separate Neural request in English/Spanish. Native,
DLAA and DLSS Quality are selectable; the latter two explicitly report pending
qualification and retained native rendering. A saved request does not enable NGX
against unqualified game images. Numeric selectors
use five-percentage-point steps within each implemented filter's bounds; unchanged
rounded selectors preserve a more precise existing file value. Whole selector
rectangles use measured native layout and child click geometry; host nonoverlap
does not establish physical readability. It adds no UI index or class registry
slot. Original native
options, defaults, fonts, pointer processing and return behavior remain owned by
the game. Highlighting and Cancel do not apply VR choices. OK queues one request;
the bridge consumes it before publication/persistence and completes original OK
only after a successful save. Failed saves retain the visible page and distinguish
the session choice. A conflicting edit refreshes values and requires another OK.

Exact current UI type/visibility, native object identity and per-Show serial bind
the editor. Modal replacement and focus/input loss cancel its owner; reacquisition
clears an old request before synchronization. Ordinary native menu input stays
active; the presenter's neutral-input fence is separate from editor ownership.
Existing pause/gameplay release barriers still apply.
The four visible comfort preferences share `%LOCALAPPDATA%/CoJVR/CallOfJuarez/settings.ini`.
Version 4 adds `manual_reload` with
`install_default`, `off` and `on`. Versions 1 through 3 retain the install
default. The manual selector exposes Install default/Off/On and resets to Install
default under Defaults without publishing before OK. On is stored intent and
cannot bypass verified installed capability. Cinematic-only edits preserve the
other choices. Native labels
follow the game's startup locale (English/Spanish, English fallback), without a
separate VR language selector.
`runtime/graphics_settings` owns the ten graphics values in its separate strict
version-5 `graphics.ini`, accepting complete version-1 through version-4 files with the
missing choices defaulted to native reconstruction, automatic scale, no mask and Neural Off,
with a versioned editor store and coherent live
publication. The bridge validates all selections and both captured revisions
before editing either owner. Each file has atomic replacement; this is not a
cross-file transaction. A failure saving either file retains the visible menu,
session values and explicit retry feedback. Invalid/unreadable startup files
remain protected. The renderer takes one coherent published graphics tuple at a
fresh capture boundary through a short publication lock separate from disk I/O;
no per-frame file polling or re-filtering of repeated images is introduced.
Launcher resolution profiles retain their independent startup-only transaction.
Full LAA candidate preparation without an explicit resolution override reads the
saved menu scale through `cojvr_graphics_startup`: Auto/75%/100%/125%/150%/200%
query the current background OpenVR recommendation. Percentages multiply total
pixel count; each axis scales by its square root before rounding. The personal
2804x2860 menu entry is removed; explicit Manual CLI/launcher behavior remains.
V3 Manual index 4 migrates to Auto, rather than being reinterpreted as 150%.
Existing caps, unavailable-query fallback and profile restoration remain active.
This applies during the next preparation, not by resizing a running scene;
the public release launcher is a separate integration boundary.
Native selector usability and exercised holster assistance on/off have scoped
physical acceptance. Unreported comfort, persistence and interruption cases
remain separate gates in Validation.
See [UI research](research/COJ_UI_MOUSE_PATH.md#native-vr-settings-integration-boundary).

## Expanded campaign input

The runtime exposes semantic actions; only the CoJ adapter maps native IDs.
Shaped analog actions 4–7 execute under the shipped controller-state lock,
then unlock/apply before digital one-shots. Native F/reload/jump/kick/equipment
reject locked calls and are not replayed by ApplyState. Partial delivery retains
per-action history and unsent edges without replaying successful commands.

The control mapper owns release-confirmed radial commands, Focus/manual crouch
toggles and held-button barriers. The wheel consumes right-stick/digital gameplay
input, retains existing movement/toggles and blocks snap until a centered sample.
Native permission remains final; denied sectors require center/another sector
before becoming eligible again. Unavailable actions never count as releases.

Fresh verified `IsRidingHorse` observation selects on-foot kick or mounted gallop;
unknown/changed owner requires release. F can mount synchronously, so mount/context
is rechecked before later shoulder/crouch/body/fire consumers. On-foot native
action 16 merges manual and physical crouch; persistent toggle action 17 is unused.
Mounted/unknown owners suppress synthetic crouch and its camera compensation.
They also suppress gaze-relative locomotion and automatic HMD actor-yaw commits.
Mounted forward/backward stays in the horse frame; lateral stick input remains
native reins and the right stick remains rider free look. The last committed
yaw reference stays shared by camera/hands while mounted or observation is
unavailable; recenter adopts the current native heading. A queued on-foot yaw
commit requires a fresh unmounted observation and the same player generation
after stereo restoration. Native horse/squint callbacks are not intercepted.
An opt-in mounted-motion observer reads the reciprocal rider/horse references,
native input requests, gait, turn rate and cached physics velocities after input
dispatch and before the next simulation update. It retains local references for
one call, brackets value/identity reads and verifies the bridge generation before
publication. `movementTraceEnabled` admits at most one attempt per 100ms and 2048
attempts per enabled interval; failed reads consume the budget. No controller,
simulation callback or global horse reference is retained. Cached values do not
prove immediate input response, physical displacement or rider-camera comfort.
Create hold recenters once at 800 ms; a short released tap requests Objectives.
[The controls document](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)
owns the sole default layout and exact action mappings.

Prepared duels have a separate exact native input owner. The adapter verifies
the local player's module, duel participant and native action identity, then
forwards right-stick axes through the original mouse-axis consumer with shipped
pad shaping and instance gains. The empty-hand downward gesture additionally
requests one native minimum draw displacement using current native real delta
and sensitivity conversion; default pad gain alone cannot draw at typical VR
update rates. No native threshold, clock or equip state is modified.
The VR entry additionally requires a fresh tracked right-hand aim tip: point
at least 45 degrees down and center the stick before pulling down to draw.
After an admitted draw, a positive stick cannot raise the native gun until the
physical tip returns above -30 degrees. These are gesture UX thresholds, not a
measured holster or body socket. Pre-aiming plus stick-only draw/raise is rejected.
Pose/input/owner loss, stale or reversed samples and recenter require a fresh
lowered/centered baseline. Correct physical draw/raise and rejection of exercised
incorrect postures have scoped operator acceptance; broader timing/retry remains
separate in Validation.
Native draw/raise/timing and trigger permissions
remain final. A fresh centered sample is required on ownership/input loss and
each native game clock receives at most one delivery, even if VR renders faster.
Ordinary smooth turn and body-follow yaw yield to this owner, because the same
native RotateHorizontally method becomes duel aim. Actor-yaw commits recheck
ownership after callbacks before a nonzero write. The concentration scene is
independent; no concentration or weapon-equipping logic is replaced.

Weapon reticles retain two independent hand slots, each with a complete pair
of captured-eye projections. Every currently verified muzzle is displayed,
independent of trigger state or last shot; a missing/reloading weapon cannot
borrow the other hand's ray. The right procedural whip retains its existing
admitted ray. Native no-shoot warnings are assessed separately for each hand,
and frame identity, native gameplay ownership and stereo restoration still
gate publication. The operator confirms the combined candidate's exercised
dual-reticle presentation; precise ballistic agreement, protected-target warning
appearance and broader recovery remain separate gates.

Optional wheel haptics describe fresh highlighting, not command success: right
hand, 12 ms, amplitude 0.18, 120 Hz, minimum 80 ms cadence and maximum 100 ms capture
age. Initial/reacquired/context/resource baselines are silent; stale/centered
captures and output failures never queue/retry. Per-hand output faults are isolated.

`runtime/body_haptics` is a separate, host-tested admission foundation for optional
body feedback. Value-only session/owner/input/availability epochs, monotonic
event sequences and injected time provide silent acquisition, 100 ms freshness
and consume-before-delivery behavior. The only pattern is a QA vest-test intent;
recording/fault sinks exercise it without any SDK, device or gameplay producer.
Commands are for immediate consumption; an asynchronous provider must add its
own bounded freshness and lifecycle contract. This policy owns no OpenVR hand
output, SDK loader, worker, vest model or physical-support state.

## Equipment wheel and wrist compass

Basic virtual holsters reuse the existing native equipment toggles through
`CoJVirtualHolsterState`: left hip/Action grip selects the left pistol, right
hip/Kick grip the right pistol, right shoulder/Kick grip the long weapon, and
left shoulder/Action grip the bow.
These are ergonomic tracking-metre gesture zones relative to the HMD position
and horizontal heading, independent of native body sockets or visible holstered
meshes. Looking down does not pitch the zones. Earlier exercised holsters/guides
and assistance on/off retain scoped acceptance; broader fit/recovery remains open.
Current hip zones use 11 cm radii and shoulder zones retain 24 cm; visual half-extents
share those actual interaction radii. Thin antialiased 128x128 rings retain clear
centres and existing availability colors, without changing other ammunition or
support glyphs. A positively observed native world selection preserves fresh
left-grip Action near the hips over holster intent; ordinary front selection does
not block a valid behind-shoulder gesture. Native nearby-weapon/ready guards
remain authoritative. Prior consumed holds still require release.
These refinements have host gesture/pixel coverage and require physical checking.
The exact adapter requires ordinary on-foot gameplay, known non-duel ownership,
stable idle/empty native hand tuples, no carry/death/reload or nearby world
weapon, no manual reload or
active bow draw, and current native slot ownership/permission. It issues one
existing selection edge; native toggle/equip/hide and animation remain authoritative.
Conflicting wheel/fire/equipment input rejects. A captured grip suppresses its
Action/Kick/Gallop meaning and shoulder availability through physical release,
including zone exit and context/tracking loss. Missing or unavailable input is
not a release; failed selection is never queued for a later owner. An unclaimed
other-hand Action/Kick cancels selection, and an admitted slot edge cancels any
older pending shoulder pulse before native delivery. Precise socket
contact, model attachment and duel drawing remain separate contracts.

A known zone press is reserved before conflict/freshness/arming admission, so a
blocked holster attempt cannot fall through to a kick or Action, including after
leaving the zone while held. Selection retains all original readiness checks.
During a manual reload session the zone observer still consumes such grips but
has no native selection readiness. Ammunition pickup/carry/release uses the free
trigger (L2/R2), whereas holsters use grip (L1/R1); an empty support hand and an
admitted weapon remain necessary. No model/socket attachment is inferred from
either gesture. Guides report conflicting trigger/reload/wheel input as unavailable.

The native comfort option enables colored holster guides, off by default. The
adapter uses the same canonical centers/radii as grip admission and captures
head-relative billboard corners with the gameplay pair. Available, hovered and
unavailable states have distinct colors. The renderer uses bounded immutable
translucent textures; invalid projection hides a zone in both eyes, and optional
guide failures preserve essential HUD text. Blocking UI and wheel ownership hide
the guides. These hints have no scene depth or native contact authority.

Post-.5 development adds a separate ammunition pickup marker to that same
default-off preference. The reload gesture publishes its exact source and
shared pickup predicate, including the fresh-trigger-release barrier; guide
presentation cannot grant a cartridge or native reload request. Only a supervised,
admitted manual waiting phase with positive native reserve advertises pickup.
A small cartridge icon distinguishes ammunition from grip-based holsters:
gold locates the source, green marks a nearby released hand and grey requests
trigger release. Carry/replenishment, closing, cancellation, context/tracking loss
and invalid geometry hide it. Manual sessions hide holster rings while their
hands are owned, preserving the existing grip-consumption policy. The captured
source is transformed into that frame's head space; no anatomical pouch is inferred.
Queued ammunition markers additionally require the current reload-owner epoch
at composition, valid projection in both eyes and the current guide preference.
Distinct immutable textures remain optional and preserve essential HUD on failure.
This has host coverage only; visual fit/readability remain in Validation.

The wheel uses procedural leather, brass, stitching and amber highlights while
preserving native localized labels, sector order, permissions and input policy.
Its styling has host raster coverage only; visor readability remains unqualified.

Read-only game-thread readers copy native inventory permissions, actual rotor
IDs, visible waypoint/map orientation and owned HUD caches into bounded neutral
snapshots. No eligibility factory, timer, HUD or gameplay update is invoked.
JNI references never cross threads. The compass follows the captured left grip
and native guidance; invalid pose, back-facing geometry and blocking UI hide it.

The presenter caches transparent wheel/compass rasters and a separate wrist
status card. Native health/ammunition have priority within ten copied rows;
optional countdown, horse, concentration and posture/shadow rows fail independently.
The visible owned health row optionally supplies a bounded 0–100 percentage
parsed from its native numeric cache. Its wrist meter and low-health text colors
are presentation only; missing/out-of-range metadata preserves ordinary text.
Only that row changes, with no new health reads, gameplay calls or authority.
Optional rows also carry bounded renderer-neutral decorations from the same
admitted caches: paired percentages with warning direction and a discrete state
badge. The CoJ adapter maps horse health/fatigue to separate half-row meters and
committed concentration mode to ready/active/cooldown/unavailable. Fatigue fills
up as native tiredness increases; no remaining-stamina conversion or cooldown
progress clock is introduced. Decorations follow actual appended rows and cannot
displace essential rows. Missing/invalid/overlapping meter metadata retains text;
the existing health meter has priority. Context/tracking loss clears the whole
captured status, and unchanged content reuses pixels. These semantic graphics
are host-tested; they do not capture native HUD sprites or establish visor fit.
Fatigue is not remaining stamina; shadow does not guarantee enemy invisibility.
Pixel rebuilds and bounded game-owner observation run at up to 10 Hz while each
captured frame retains its own geometry, optics and resource identity.

Ordinary Focus reads cached squint factor/maximum and applies
`1 + (maximum - 1) * factor` to copied asymmetric render-eye tangents. The copied
scene-eye baseline is divided by magnification about its existing binocular
centre; otherwise narrowed FOV multiplies displayed disparity. Physical runtime
IPD/tracking, eye cant, UI optics and restore preimages remain unchanged. It never calls CalculateZoom via
GetBeingZoom. Bow concentration combines squint with the existing module
`BulletTime` current/target caches using the original clamped proportional rule
and `squint + concentration - squint * concentration`. Actual/desired bow
identities and module/BulletTime ownership must remain stable; no native update
or parameter factory is called. Invalid caches/context fall back to physical
optics. The exact scoped Winchester additionally reads the already-existing
module TreeMap parameter and cached default-camera FOV, bracketing parameter,
camera, module, state and weapon identities. Its native scope-visibility predicate
and camera-normalized magnification govern a binocular scene-scope fallback;
no parameter factory or native camera/zoom update executes. While native scope
visibility is active, a virtual scene yaw/pitch follows the verified rifle ray in
the compensated tracking reference. HMD orientation remains validated and is
submitted physically to the compositor, but does not steer the scope image.
This angular policy also suspends HMD-driven actor/upper-body yaw and
gaze-relative movement while the exact cached native scope owner is visible.
The last actor-owned yaw remains the camera/hand reference. A fresh post-input
scope observation prevents a queued actor turn on the frame that Focus enters;
scope exit resumes ordinary body following. Recenter and player-generation
invalidation retain their existing reset boundaries. Native scoped camera yaw
need not follow a written actor yaw one-to-one, so replacing only the final
render quaternion is insufficient to isolate the controller's upstream ray.
Head translation retains its existing mapping. Scope pose and eye frusta commit
together; failure or scope exit restores ordinary head-driven view. One
binocular-centre angular shift aims both parallel frusta toward the verified
muzzle ray's existing reticle reference plane in that scene orientation.
Each original asymmetric principal point is preserved; independent UV=.5
convergence is rejected because it changes the displayed binocular alignment.
Normal camera transactions restore shifted frusta/scene offsets. Losing
verified ownership uses physical optics. The complete current right-reticle pair
authorizes per-eye lens centres/radii projected from one shared angular aperture,
with black exterior and a thin post-filter sniper rim/cross, even
when optional colour processing is off; stale/partial pairs cannot activate it.
The original FPPCamera mask update is suppressed only for the locally tracked
stable exact rifle; ordinary updates retain its complete native object lifecycle.
This is a scene-scope fallback, not a telescope texture. Validation owns scoped
acceptance and the head-input defect; the controller-only orientation correction
alone failed physical head-yaw isolation. The subsequent body-yaw policy has
host regression coverage only, independently of bow cache acceptance.
The admitted current native protected-target flag colors the scope sight red
after scene filtering; it cannot be discarded by suppressing the old reticle.

## Essential gameplay text

Bounded readers preserve owned native visibility/alpha, localized hint/interaction
text and subtitle settings/dialogue ownership. UTF-16 snapshots travel with exact
capture identity and eye optics. The D3D11 presenter caches finite-depth Unicode
panels after copying fresh world content; empty snapshots remove old panels.

Authored VR wrist labels, equipment sectors/confirmation and controller-gesture
wording follow the startup `Game.ini` `Locale("En")`/`Locale("Es")` setting.
The game adapter reads the bounded configuration once on exact-build installation;
missing, unknown or ambiguous settings use English. Native copied dialogue,
notices and waypoint text remain unchanged UTF-16. Renderers receive copied
labels, including wheel confirmation text, without game-language parsing.
Production and host targets compile UTF-8 sources consistently; test-only UTF-8
previously masked corrupted authored accents in the production DLL.

Copied prompts translate only explicitly delimited default campaign keys and
observed aliases. Native strings/dialogue/bare letters and digits remain unchanged;
custom PC keybindings and unobserved locale names remain ambiguous. Post-.5 source
can replace mapped gestures with the connected OpenVR origin's hand/component
name. The input owner copies bounded UTF-8 names into owned UTF-16 snapshots;
missing or multiple distinct origins use localized semantic action names rather
than an arbitrary controller. Hold/tap and wheel direction/release remain explicit.
Without a runtime metadata source, the existing Sense presentation is preserved.
An invalidated source retains a nonzero epoch with empty labels and uses semantic
fallback until fresh metadata arrives; it cannot create an unfenced Sense label.
This does not describe arbitrary binding chords. Optional graphical glyphs require
one qualified connected origin and an exact inspected controller/component/input
tuple; unknown devices, chords and ambiguous sources retain the text fallback.
Glyph metadata has a separate bounded lifetime and binding epoch.

Origin metadata is cached until binding/device/input invalidation; empty or failed
lookups retry at most once per second without querying settled actions again.
Optional name errors cannot fail an input poll.
Routine focus/settings edge resets retire the presentation epoch without
requerying settled binding metadata every frame; failed polls and actual
binding/device changes also invalidate the source cache.
Invalidation clears published names, and publication rechecks generation under
the event lock. Generations remain distinct across runtime replacements within
the process. Captured translated text carries its binding generation; the
compositor hides stale control hints while preserving native subtitles. No native
string, input binding or gameplay permission is changed. Text/status recovery is
separate from graphical native HUD capture.

The ordinary OpenVR route keeps native desktop pixels at Present for monitor
viewing and screenshots. VR capture and luma diagnostics run before the
original Present. The retired blackout rendered the same frame, then added a
GPU save, clear and restore around Present without a measured performance gain.
`LatestTracking` drains pending pause, UI, recenter and objectives edges and
belongs to the input consumer.

Native hint, mission-notice and timer panels share a reading anchor captured when
their visible interval starts. Hint/notice changes or game-owner changes acquire
a new anchor; timer ticks retain it. Their complete stack fits both captured eye
projections, including a hint without a timer. Composition transforms the anchor
through the producing frame's HMD pose rather than the latest runtime pose.
Recenter, flat presentation and device-generation changes retire the anchor.
Subtitles, interaction coaching and tracked hand/wrist surfaces retain their own
placement policy. Flat menus remain world anchored; a changed main-menu reference
acquires its new anchor only when the corresponding frame upload commits.
These are presentation and input-ownership contracts, not physical fit acceptance.

The same default-off weapon-guide preference also enables short localized manual
reload instructions. They follow the supervised session's pickup/release/carry/
insertion/continuation state and correct support-hand action, never infer ammo or
grant an insertion. Native hint/interaction text takes priority. Each stage lasts
at most three seconds; stale samples, changed player/weapon/input/binding identity,
closing or invalid tracking hide it. Queued instructions additionally require the
current reload-owner epoch, unexpired deadline and enabled guide preference at
composition. Text deadlines share the presenter's GetTickCount64 clock; the
independent high-resolution gesture clock never authors presentation deadlines.
The presenter also retires expired controlled text before repeated
eye submissions: at most four small renderer-owned background rectangles preserve
the world and other HUD underneath. Their total backing is capped at 16 MiB,
reused by extent/format and released on reset; whole-eye preimages are excluded.
Controlled text is drawn last, after both eye backups are prepared; unavailable
backup suppresses that controlled text rather than painting an unretirable label.
Device/context/target mismatches discard identity without restoring elsewhere.
Optional button-art expiry alone restores those controlled layers and redraws
their still-current semantic text, using one owned capture for the original
layout. Other HUD is not drawn twice. The refresh cannot extend the instruction
deadline or survive binding, owner, guide-preference or device/target invalidation.
This prevents an expired icon from blanking a valid sentence until a fresh world
frame; physical stability remains a separate gate.
A new successful world upload discards the previous preimage, while a rejected
no-write upload keeps its retirement fence. Fixed-output writes retain frame
authority. Physical readability, resource pressure and gesture fit remain separate
validation gates.

Critical alerts have independent captured head-space presentation: the visible
owned countdown title/exclusive number and objective/journal notices do not
depend on wrist tracking, orientation or ten-row status capacity. Read-only
HUDManager slots 12–17 supply naturally localized text and active threat caches;
each domain clears independently on hidden/foreign/invalid owners. The native
timer and fade clocks remain authoritative. Native absolute threat bearings are
resolved against CoJ's camera-back convention into viewer-to-source directions,
converted through the captured tracking basis and HMD orientation, then composed
as green direction/red damage chevrons with observed native alpha. Immutable
glyph textures are reused across bearing/fade changes. Full per-eye projection
and capture identity apply; invalid second-eye projection suppresses both eyes.
This semantic recovery is host-tested and has scoped operator headset acceptance
for the recovered countdown, encountered notices and corrected attack directions.
Broader campaign/owner-loss coverage remains separate; it does not establish
native sprite capture or complete HUD coverage.

## Tracking, locomotion and body ownership

Native actor position, collision, grounding and locomotion remain authoritative.
Analog movement preserves the shipped per-axis 0.04 shaping and float actions 4–7.
Controller axes receive native shaping before residual HMD yaw rotates the
result into the actor frame. Shaping after rotation erased small lateral
components during careful gaze-directed movement. Native float dispatch uses
a 0.00001 noise tolerance so slow gaze changes are not swallowed by its cache.
Zero/nonzero transitions always dispatch, including sub-tolerance values:
native horse reins interpret the sign as a full turn-rate target. A successful
analog batch caches only the axes actually delivered; another axis's update
cannot acknowledge a skipped delta or erase accumulated movement/release.
A failed native analog batch leaves its state uncertain, since a callback can
write before throwing. Recovery resends all four axes and remains pending until
lock/delivery/unlock/apply complete; a fire/shoulder-only phase cannot acknowledge
that recovery. Neutral cleanup must not trust the last acknowledged zero values
to describe a partially mutated native controller.
The existing movement-trace opt-in also observes actual JNI dispatch entry,
duration, phase and turn inhibition/rearm, with distinct frame/pose/native-context
and player/input generations. Native action and yaw acknowledgements retain
partial failures; they do not establish displacement. Unknown input-sample or
device identities stay zero. The bounded sink never adds native dispatches;
formatting/clock/log work is absent once disabled or capped. Summary p99 retains
the existing percentile convention. These diagnostics attribute observations,
not a measured pacing improvement or a resolved external stutter report.
Native speed coefficients and physics remain authoritative. Verified on-foot body
yaw follows every HMD yaw change, without the former 35-degree free-look cone.
The previous committed actor yaw is removed once from tracked camera, hand and
weapon mapping; the new target commits only after stereo restoration. Duel,
movie, settings and native ownership guards still apply. Every mapping consumer
qualifies that committed reference against the available player and generation;
a replacement cannot leave hands/pelvis using yaw already rejected by the camera.
This continuous-follow change has host coverage and scoped operator headset
acceptance. Ordinary controller turning
defaults to continuous
yaw: a 0.25 stick deadzone, proportional speed up to 90 degrees/second and
monotonic elapsed time, mapped to the native yaw sign by the game adapter.
Unavailable/context-changed input, regressing time or gaps over 100 ms reset
authority until a fresh centered sample. Duplicate timestamps and later input
phases never replay rotation. The native settings selector can request smooth
turning or one 45-degree snap per deliberate stick deflection. Mode changes also
require a fresh centered sample; native sign/permissions stay in the adapter.

Room-scale camera and controller targets share tracking-Z/yaw conventions.
Only horizontal room-scale displacement enters the render-only pelvis overlay;
vertical actor position stays native. Physical crouch height uses a calibrated
hysteresis and compensates duplicate native camera descent without altering
collision. Recenter/player/mount changes invalidate the relevant references.
Physical-walk animation remains unresolved; root updates affect child animation
and gameplay events. The offline ANM sampler evaluates bounded original cubic
channels, native rotation modes and local-times-parent matrices without invoking
the engine. Hash-bound Ray/Billy profiles recompose eight leg elements below an
explicit pelvis anchor; authored root/pelvis motion remains unapplied. No runtime
leg writer, grounded eligibility or native restore/lifetime transaction follows
from this mathematical sampler. See [animation ownership](research/COJ_ARM_SKINNING_AND_AIM.md#physical-walk-animation-ownership).

## Body IK

The active `-BodyIkAtStart` presentation uses independent native hands with the
original torso/legs and hides the partitioned connected arm mesh only while
those hands own presentation. Twenty named forearm/twist/hand/finger/socket
frames per hand receive one rigid map. Armed hands share the verified weapon
map; unarmed sockets use absolute grip orientation. Connected-arm span fitting
and IK remain rejected diagnostics, with observation gated by movement tracing.
The [arm research](research/COJ_ARM_SKINNING_AND_AIM.md) owns mesh hashes,
hierarchy, exact writers, measured grip points and rejected anatomy findings.

Parent hand/socket maps apply before the child weapon. Restore returns the
parent before its captured child, then the outer pelvis, rebasing positions by
actor translation. Removing an outer pelvis offset adjusts retained inner
captures exactly once. Failed restoration retains references/preimages, blocks
further mutation and prevents native closing callbacks or watchdog renewal.
Player replacement never redirects cleanup to a new actor.

Native animation owns conventional reload. The persistent manual route keeps
scoped weapon/independent-hand presentation only under positively verified native
reload ownership. Its exact policy is `READY -> OPENING -> MANUAL_LOAD -> CLOSING
-> READY`: fresh Square prepares/closes, free-hand trigger pickup/journey/release
requests one synchronous operation, and armed-trigger closing cannot shoot.
Exact profiles distinguish one native unit, a selected indexed unit and one
native whole-clip operation. Native ammunition, reserve, capacity and mechanical animation remain
authoritative. Invalid/unknown ownership cannot fall through to conventional
reload; only positively identified unsupported contexts use that fallback.
The [reload owner](research/COJ_RELOAD_OWNERSHIP.md#persistent-manual-candidate--scoped-headset-acceptance)
retains helper signatures, native state/clock boundaries, restoration, legacy
hybrid/paced/wait contracts and their evidence limits.

Post-.5 reserve and JNI hardening is host-tested only. Positive current reserve
is required to offer/carry a cartridge; reserve loss withdraws continuation but
retains native waiting/close supervision and physical release claims. Replenished
reserve cannot rearm a held trigger. Optional zero-reserve coaching uses the
current reload binding without issuing a close command. Helper success requires
the original player/generation after lookup and callback, with no JNI exception.
Rejected/faulted insertion stops presentation/supervision before publication and
never retries. Completion, failed preparation and fresh sessions clear old tokens.
The gesture's final consumed release sample cannot recreate the session's
persistent trigger claim. Replacement context/input/player tuples retire old
gesture clocks and establish a neutral baseline while retaining physical holds;
only a later fresh available release in that current source discharges them.
Retired tuples cannot reclaim authority. This permits ordinary-fire recovery
after cancellation/disable without replaying an insertion or borrowing an old
source's release, including when the replacement sequence restarts at one.

The optional active reload target is a value-only receipt emitted by the same
manual gesture update that owns insertion. Its current tracking-space centre,
player/context/weapon/hand, input generation and sample clock bind the guide;
capture identity, native wait/reserve, restored mutations and renderer reload
owner gate delivery. All exact manual profiles, including Schofield, share this
channel. Readiness comes from gesture policy. The existing generic grip centre
is still provisional, not a mechanical loading mouth. The earlier measured
Winchester/Scope and Remington/SawOff locators are withdrawn from production HUD
after physical rejection of guide/insertion agreement. Their bounded geometry
observers and host fixtures remain research; no prior-frame post-overlay sample
authorizes a transfer or replaces the current input-space target.

The exact-game adapter also predicts those measured mouths and the textured
cartridge nose from current input before transfer. It brackets native reads
with the same weapon object and waiting phase, borrows resolved render readers
and keeps all sampled frames local. The diagnostic records held and released
poses, including misses, for correlation with the independent rendered sample.
The input policy continues to use the grip receipt while that correlation and
mechanical clearance await live validation.

The duel coach is read-only presentation from the existing native input capture:
phase, observed hand occupancy, physical pitch and successful axis delivery.
It neither invokes native input again nor changes native duel timing. Fresh
pose/native-clock/owner fences and native HUD priority gate optional EN/ES text.
An independent presentation epoch and 500 ms presenter-clock deadline retire
retained coaching on owner/phase/instruction replacement, guide disablement or
expiry, including repeated frames. No reload token is borrowed.
The original paused EN tutorial has its own pinned-resource presentation seam;
unknown resources and localizations keep original text and release conditions.

The public launcher enables manual reload on fresh installs and preserves the
installed choice on updates; Advanced > Manual reload changes it through
Install/update while the game is closed. The legacy numbered launcher retains
its equivalent choice. Developer candidates explicitly select `-ManualReload`.
The runtime checks the staged executable identity and the declared derivative
archive hash against the installed files. It also resolves all four manual JNI
endpoints on the actual retained player's immutable Java class. The capability
cache belongs to that VM, player and generation and retires with the player.
The persisted preference can disable that route without changing the archive.
Disablement cancels manual ownership and fences Square and both physical fire
channels until native retirement and a fresh release for each channel. A void
cancel return alone cannot retire ownership. The disable path keeps retrying
until repeated same-player reads show a cleared probe, absent probe weapon and
machine, and native reload inactive. A request that never entered the native
probe can retire from status zero only with the explicit pending-request token
and the same repeated empty/inactive proof. An admitted wait cannot use that
exception. The native menu edits this stored choice through the same revision,
save-retry and owner protocol as the other preferences. Live menu disablement
and the expanded layout remain pending physical checks.
Current source includes sixteen exact manual firearm profiles; archived
prealpha.5 retains its earlier admission. Winchester and scoped Winchester restart the authored body21 cycle
after one verified native unit; Volcanic uses the same policy with pistol reserve.
The pinned constructor-only `WeaponHenry1863` alias shares Winchester's 12-round,
rifle-reserve, right-owned body21 transaction. Archive identity, exact class,
capacity and ammunition type remain mandatory; this admits no alias geometry.
The pending/consumed ticket, veto owner, watchdog, retained callback references,
fault quarantine and native20/21/22 closing remain shared. A replay withdraws
cartridge presentation until native body completion reaches waiting again.
Remington/SawOff and Derringer retain two one-round indices, while Lemant retains
one rifle round and eight pistol rounds. The lowest eligible index is proposed
at pickup and frozen across carry/release; becoming full/reserve-empty withdraws
the carry instead of redirecting it. The guarded indexed composite calls original
count/reserve setters for that one index, verifies both loaded counts and all
distinct reserve pools, and publishes one event. It is explicitly separate from
the original ascending native reload loop. After a verified selected unit, the
indexed route replays its guarded native body21 when another unit remains and
withdraws readiness until a new native completion reaches waiting. The original
Started callback keeps its zero-unit per-index skin/audio calls; all count/pool/
counter identities and maxima remain checked around it. Only the retained
same-hand/machine/body self-Clear is suppressed. Typed feedback accepts the
resulting Status1 only with matching exact-profile cycle flags and one verified
selected unit; no extra carry or native transfer is queued. A same-ID replacement
body rejects replay/close authority; a separate exact-body completion lease fences
late ammunition transfer on the retained hand/machine until natural retirement.
Indexed empty-trigger preparation is
disabled until native firing-index ownership is proven; Square prepares normally.
The verified reserve family selects the ammunition shown at the existing
finger-pad anchor. Rifle rounds use an elongated textured mesh and shotgun
rounds use a red hull with a brass base. The held index fixes the choice on
LeMat. The measured-mouth observers use the nose offset of the rendered mesh.
After a confirmed native insertion, rifle and shell profiles use distinct
pulses on the loading hand through the existing event and ownership guards.
Schofield base/A/B use one original whole-clip authority call with exact
`min(6-loaded,pistol reserve)` readback and immediate retained native close. A
bundle depicts that many rounds using repeated copies of the imported textured
cartridge. Capture retains one mesh and at most six head-space instance offsets;
the renderer globally orders their faces and uploads bounded chunks without
exceeding its original constant-buffer limit. The bundle is a provisional count
presentation, not an exact-calibre speedloader. Single-cartridge proximity
diagnostics do not qualify bundle contact and are withheld for bundle samples.
The optional development Schofield motion candidate scopes a direct native
opening request to the exact base model and a fresh manual ticket. Both hands
must pass the single-hand idle check. After the request the armed desired state
21 must carry the original two-handed animation flag, with the actual weapon
and empty support hand still verified. The
retained wait accepts a downward hand sweep or wrist snap with tilted aim.
A snap in an already empty drum clears the guide outlines for that wait.
One verified bundle insertion retains the native wait until a rearward flick or the
existing close request. The native watchdog and ownership retirement still
control the animation lifecycle. A visual guide projects the six measured
radial positions onto the positive-Z drum face and shows the aggregate live
count as brass heads in a non-spinning rear view. Unfilled outlines clear on
an empty shake or a confirmed transfer to reserve. This guide carries no
contact or ammunition authority.
Shared transaction guards prevent cross-family callback reentry.
All new paths remain source/host candidates with generic grip-zone interaction;
per-model ports, shell/calibre fidelity, native mechanical playback and physical acceptance
remain separate gates. Exact profile admission belongs to the
[reload profiles](research/COJ_RELOAD_OWNERSHIP.md#exact-revolver-profiles), not a
model-name prefix or inherited pistol class. Scoped accepted Peacemaker flow and
port visibility do not qualify new profiles, cartridge contact, precise loading
mouths, mechanical writers, ejection or broad interruption recovery.

Two-hand support connects `runtime/two_hand_grip` to exact right-owned Winchester,
Remington and SawOff transactions with an empty left native slot. Fresh nearby L1
acquires a measured wooden fore-end point on the retained live weapon element;
the player's detached left_hand socket supplies anatomy/orientation only. One
rigid map preserves weapon scale and authored roll across both hands, muzzle and
ray. Acquired weapon-local contact persists through admitted native fire/cycle;
reload/equip, invalid tracking, excessive separation or changed ownership cancels.
Both parent hands commit before the child weapon and every eye verifies the
retained object/epoch. L1/L2 claims precede Action, holsters and ordinary fire and
remain consumed through physical release. Numeric IDs cannot retain support.
The [measured-contact owner](research/COJ_ARM_SKINNING_AND_AIM.md#measured-long-gun-contact)
retains original mesh coordinates/hashes and exact-class constraints. Corrected
Remington grasp has scoped acceptance; other fit and unreported sustained-cycle/
recovery remain open. No manual rifle admission follows from support acceptance.

Optional finger curls preserve authored translations, lengths and Hand/socket
frames in the same transaction. Ordinary empty-hand eligibility, the verified
idle-rifle support exception and manual support-hand recovery remain game-owned.
A physically held cartridge claims pinch; an available continuation token does
not. Native-confirmed unit loading alone starts the short support-thumb feedback;
closing and ownership/tracking loss suppress it. Exact poses, timing, sampled
skin measurements and limitations belong to
[reload contact research](research/COJ_RELOAD_OWNERSHIP.md#finger-contact-boundary--visibility-accepted-grip-unresolved).

The loose CC0 cartridge is renderer-owned geometry/material captured with each
eye pair, with no runtime asset parser or native environmental-light sampling.
Its midpoint and tangent derive from verified displayed distal finger frames;
invalid frames or failed hand/weapon restoration hide it. Visibility acceptance
does not establish convincing contact, revised-axis acceptance or scene occlusion.
The [asset and contact owner](research/COJ_RELOAD_OWNERSHIP.md#installed-ammunition-resources--pickup-meshes-not-loose-cartridges)
retains provenance, rejected visuals and measured skin/mesh findings. Sparse
contact/depth diagnostics are read-only; matching depth dimensions or offline
overlap cannot qualify eye/hand contents, penetration or occlusion.

Read-only Winchester and indexed reload observers copy native states, clocks,
loaded indices and typed reserves under exact-class/same-object proofs. Their
offline analyzers reject incomplete intervals and invalid identity/count/clock
claims; sparse samples or numeric IDs never prove a transfer or cross-poll object
identity. The synchronous vector classifier requires a caller-held identity
lease. These implemented/host-tested tools add no manual weapon or mechanical
writer. [Reload ownership](research/COJ_RELOAD_OWNERSHIP.md#additional-families-original-archive-reload-contracts)
owns storage/pool mappings and remaining native correlation prerequisites.

The bounded Winchester loading-geometry observer separately copies exact
ordinary/scoped natural and displayed GunBody/GunLoader/Holder/barrel frames
under retained JNI ownership. It transforms the measured receiver polygon and,
only with a current held pinch, the actual textured cartridge nose/axis. Native
phase observations must agree around each read. Same-frame/context/generation,
stereo capture and successful restoration gate publication. Its projected
edge/plane measurements do not authorize insertion, clearance or a mechanical
writer; the existing grip-zone policy remains unchanged. Host verification and
remaining live/physical prerequisites belong to the
[measured-mouth contract](research/COJ_RELOAD_OWNERSHIP.md#measured-winchester-receiver-lip--observation-only).

## Weapon ownership

Tracked grip anchors the authored holding socket; tip -Z supplies direction.
`runtime/two_hand_grip` supplies a host-tested rigid support-span/roll solver and
fresh-grip acquisition/cancellation policy. It takes measured metre-space frames
and returns one rigid map with explicit support-spacing error; it never stretches
the weapon or substitutes the hand-to-hand vector for the barrel axis. One policy
instance owns the same physical support channels through interruption and release.
The exact Winchester, Remington and SawOff adapter now connects the solver to
scoped native rifle transactions, with host-tested ownership and restoration.
Named `Holder` may resolve to element zero (observed on native Remington); zero
is a valid root socket, while a negative result rejects. Keep exact-class,
retained object, natural frame and hand-state checks even for a root holder.
The adapter also rejects a support controller crossing the perpendicular plane
through the dominant grip relative to the ordinary right-hand mapped mesh
contact. Equal hand spacing alone could otherwise admit a reversed rifle;
the same hemisphere gate applies at acquisition, retention and render refresh,
with physical L1 release required before reacquisition.
The corrected Remington grasp has scoped operator acceptance. Other model fit,
complete fire/cycle lifetime and broader recovery remain open headset gates.
Existing idle-finger eligibility alone does not qualify contact.
The bridge retains a Java object reference and opaque epoch independently of
each restored hand/weapon transaction. A same-ID replacement between frames
changes that epoch and cancels the physical claim until release/regrip. Missing
JNI cleanup revokes authority but keeps the reference for a later cleanup retry;
player clear/reset also retire it. Numeric IDs alone cannot retain support.
An expected support transaction requires both hands and the right weapon at
every eye and final verification. Failed or unexpectedly inactive maps veto
later eye work, mirroring and publication of the entire pair, while restoration
still completes. Successful rollback cannot qualify images already invalidated.
An opt-in read-only observer admits the exact ordinary Winchester, Remington
and sawed-off Remington classes with coherent right active/actual/desired
ownership, empty left equipment, ordinary idle/fire states and native two-hand
flags. It brackets naturally animated root/Holder/left_hand and named mesh-anchor
reads with repeated
owner/state/identity checks, before scoped body/hand/weapon writes. Retained or
faulted overlays reject observation. Sparse periodic references and bounded
trigger-intent bursts record native centimetres, local support frames and actual
sample gaps; neither trigger intent nor an idle return qualifies a complete
native cycle, contact or attachment. The offline evidence analyzer preserves
those limits and compares only coherent same-owner sample histories.
The adapter rigidly maps all measured weapon elements through
`FromUpForwardPosElementWorld`, preserving internal animation/socket-to-muzzle
geometry. The general object setter is unsafe because it reaches recursive
`+0x82F00` notification. Readback verifies the complete muzzle basis through both
eyes before ray publication. Shared two-hand weapons have one visual owner.
Exact `WeaponBow` has no Barrel; its authored grip is DBowHolder and its native
fallback ray is root position/+Z. Only a stable actual/desired native hand owner
with that exact class/holder gains this exception. An ActiveWeapon alias in the
other slot gains no second map or ray. Bow pose, native per-hand shot cache and
existing verified reticle use the same scoped map; pitch is not flattened.
Other missing barrels fail closed.
The bow owner proof is bound to the transaction's retained Java object and holder
before its first commit write and during ray/eye verification. A different bow
at the same pose cannot inherit that transaction; its retained preimage remains
owned for restoration after rejection.

Physical bow drawing qualifies only the native left-held exact bow. An available
fresh right trigger near its authored DArrow point captures the string; current
right-hand retraction admits release. DArrow's rest point is retained in bow-local
space during the grab so authored animation cannot manufacture physical tension.
Continuous slack/off-axis movement relaxes current shot tension without dropping
the captured R2 hold. Invalid alignment cannot fire; hard owner/tracking/time or
reach faults still cancel and retain release barriers.
Game-neutral metre-space gesture policy owns continuity/release barriers; the
CoJ adapter owns mapped geometry and native ALT_MOVE/CanAttack/GetAttackState
admission. R2 release internally requests one original attack channel while
preserving its native alternate prerequisite until the observed attack entry or
a bounded abort. No physical L2 requirement, direct projectile creation, native
ammunition/power writer or animation-time writer is introduced. Original authored
tension animation and trajectory remain native. Admission additionally publishes
a transient exact bow reference while drawing or awaiting the one scheduled
native attack. Only that same current actual/desired left bow and native held
right, outside network play, guard the original concentration-expiry call to
turn off alternate fire. Original right-release cancellation and concentration
duration/zoom/slomo remain native. Failed publication or cleanup invalidates the
gesture; actor replacement retains the old owner until cleanup succeeds. Every
external reset records its cause. Controller grab/tension/release
feedback has its own owner epoch and captured-frame freshness gate, separate
from manual reload and the deferred optional vest track. Host verification does
not establish an arrow spawn, string/arrow alignment or physical usability.
GPU capture readiness does not own the captured gesture. Ring backpressure can
drop the left capture and skip the right eye while current input, tracking,
weapon ownership and attempted native/resource restoration remain valid. That
frame supplies no new captured feedback, but it does not release the string.
Only attempted eyes participate in restoration checks; an unattempted right
eye's false status is not a failed rollback. Pending resource quarantine and
failed native/weapon verification still cancel, as do gameplay/tracking loss.
The consumed physical R2 hold survives cancellation and ambiguous bow-owner
observations. Only a fresh available release from the current input source
discharges it, and that release sample remains consumed. A replacement source
establishes its own baseline; repeated or retired observations cannot resume
ordinary native fire or release the old claim.
Stationary Cannon ownership is independent of retained hand inventory. The
adapter brackets the current player's native IsUsingCannon/GetCannon and the
Cannon/controller identities, then routes physical R2 to action9/Attack(1) through
the normal native dispatcher. It does not call StartFire directly. Inventory
rifle/punch/bow filters cannot own that contextual shot; explicit loading,
recenter and restoration safety still override it. Cancellation invalidates
arming without releasing consumed holds; unknown-owner reads consume held
channels until fresh available physical releases. Native exit owns Cannon stop.
Grab/fire now have scoped operator acceptance; broader recovery remains open.
Stationary aim owns both right-stick axes continuously through the existing
SmoothTurnState timing/shaping policy, with a neutral baseline on entry/recovery.
The game adapter dispatches the resulting angles through native OnInputMouseAxis,
normalizing the common mouse sensitivity once. Native target eligibility,
Controller state, inversion and angular constraints remain authoritative; no
Cannon-angle fields are overwritten. Ordinary yaw and HMD body-follow commits
yield to Cannon ownership. Skipped render/input frames invalidate aim policy.
The reticle observes the current controller's mesh/barrel element with retained
local references and the existing rigid +X/up frame reader, brackets ownership,
and projects its world ray through both applied eye frusta after restoration.
It does not call the randomized GetFireDir or reuse a hand-inventory reticle.
This is a directional reference; actual alignment/precision and impact prediction
are separate, and the new X/Y/reticle route is host-tested only.
The read-only evidence analyzer preserves bow requests, valid counter maxima and
separate gesture/dispatch/reset reasons within the inventoried run interval.
Neither a request nor native attack entry proves an arrow spawn; absent events
remain unobserved and the analyzer cannot grant headset acceptance.

Exact-class nullable per-instance vectors feed native ballistic/visual origin
and direction consumers after current muzzle publication. Null caches, unknown
hands and network-forced attacks retain original methods; native spread remains
downstream. Invalid/menu/generation boundaries clear ownership. Persistent FX
use detached configured emitters, full native barrel frames and FXDetach commit;
a starting transform alone is insufficient. Native counters demonstrate consumption,
not visible hits/effects. [Weapon research](research/COJ_ARM_SKINNING_AND_AIM.md)
retains exact consumers, emitter layout and authored rearward-trail interpretation.

Contextual F now uses the left Sense aim ray through exact local-player
CheckTriggers, retaining native range, collision-element and permission decisions.
Missing publication suppresses Action until an available release; there is no HMD
fallback. The small cyan reference follows that ray only while L1 is held in eligible
gameplay, outside the wheel. Its nominal 1.5 m depth is presentation geometry,
not a collision hit or native range. Cached active/executing trigger diagnostics
are passive, bounded and sampled before/after fresh F edges; they never trace or
select. This replacement's physical gate is separate from the accepted older
HMD-based drawer interaction.

The legacy motion-reload route is a hybrid request into native action 31,
retaining conventional Square and native ammo/reserve/state/animation authority.
The default public persistent route uses Square preparation and native unit insertion
as described above; the following paced/probe contracts are legacy alternatives.
Exact passive admission supports eight concrete single-pistol/empty-support
classes; the paced single-round extension uses the five qualified profiles in
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md#exact-revolver-profiles).
A waist pickup, held journey and release near
the grip requests one native round. Following an observed reload interval, ordinary
eligible ownership must recover before the presentation-only solid token replenishes.
Fresh trigger press/release can then request another round without returning to waist.
Both initial and continuing grip carries reject discontinuous translations of
either hand on fresh held or release samples. The current gesture UX bounds are
12 cm per sample and 3 m/s, shared with explicit insertion's translation check;
grip-only carries do not inherit socket-axis constraints. A rejected press hides
its token and cannot insert or emit entry feedback, while its original physical
claim remains consumed through an available release. Duplicate observations do
not advance motion baselines. This policy is host-tested, not physically accepted.

An explicitly selected archive probe can veto the owned native body-state exit
before `Clear`, wait for a bounded native-clock deadline and close through native
callbacks while skipping the ammunition transfer. This logical boundary has
bounded Peacemaker runtime evidence. It retains native reload presentation;
held logical state does not establish a fixed mechanical pose or tracked hands.
The legacy paced path continues to transfer one native round per admitted gesture.

A separate passive recovery reader preserves an accepted continuation through
verified native reload/two-hand animation. It proves no carried object via
IsCarrying, rather than mistaking HasSomethingInHand's animation fallback for a
world object. Held support trigger is consumed through verified reload until an
available release, preventing cross-hand attack. No VR ammo counter or retry queue
exists. Generic cartridge-tip/socket policy is implemented and synthetic-tested,
but CoJ supplies no qualified socket or cartridge-tip input to enable it.
Displayed drum/gate observation is a separate read-only game diagnostic. Precise
chamber/ejection, independently controlled mechanical pose and
manual-session engine save/load recovery remain unresolved.
The manual policy uses `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`.
It consumes rejected Square admission until release, preserving native fallback
only for positively identified unsupported contexts. Missing observation cannot
start conventional reload. The exact base Schofield shot tail can hold one
Square press for up to 450 ms. The same owner must pass ordinary idle admission
before the manual request is dispatched. Invalid owner/context/poses/input availability,
recenter, stale samples, incompatible actions or presentation failure close the
session. Native-clock watchdog expiry is a recovery fallback, not a fixed mesh
frame. JNI cleanup retains the player on failed cancellation for retry. See
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md).

Optional insertion feedback is qualified on the game thread by a completed,
accepted native call and coherent same-weapon/hand readbacks of loaded +1 and
reserve -1. The adapter copies one value-only event into that capture's HUD
snapshot; native references and ammunition state remain game-owned. The OpenVR
presenter can pulse the free hand once (18 ms, amplitude 0.25, 120 Hz) only on a
fresh accepted stereo upload with matching capture/input/native context epochs,
valid tracking and current gameplay focus. Maximum capture age is 100 ms and
minimum pulse cadence is 80 ms.
An opaque game-owned epoch also binds the latest observed player/weapon/armed
hand identity; replacement, cancellation or missing observation invalidates
queued events even if gameplay availability remains unchanged. The presenter
rechecks this atomic epoch immediately before optional output. No native
reference or game layout crosses that boundary.
Loss/reacquisition and resource replacement
establish silent baselines. Dropped captures, rejected insertions and output
faults never queue or retry feedback and cannot affect native loading. This
policy is host-tested, with live submission records and operator acceptance of
insertion-only delivery in the exercised manual context. Broader recovery and
prolonged comfort remain pending; Validation owns those acceptance limits.

The host-tested manual candidate additionally emits a distinct qualified
zone-entry cue (9 ms, amplitude 0.10, 90 Hz), without a native transfer. First
pickup requires the same completed journey/time as insertion; replenished rounds
require a fresh held trigger claim. Entry and release use the same manual
18 cm grip zone, withdrawal beyond 22 cm rearms the cue, and 180 ms bounds its
output cadence.
The dormant explicit-socket policy also requires its qualified trajectory.
Entry and acceptance use independent cadences so entry cannot suppress immediate
acceptance. Both use the same freshness/ownership fences; neither event queues.
This extension has no live or headset acceptance yet.

Protected-target feedback can color the per-eye aiming cross red only from coherent
native warning/trace-owner/ray observations. Invalid observations clear it. Source
implementation and host checks do not establish visor visibility. Billy's whip
uses its authored holding-socket cache and naturally scheduled AdditionalSynchro;
no rigid whole-rope transform or forced simulation is introduced.

## D3D9 native-stereo transport

`RT0 -> StretchRect shared D3D9Ex DEFAULT ring -> handle/pose/lease mailbox -> D3D11 OpenSharedResource/CopyResource -> owned OpenVR textures`

Three slots hold separate non-MSAA eye textures and a D3D9 event query. One
bounded FLUSH poll can submit commands, but the producer never waits. Ready pairs
publish atomically. A D3D11 completion query retains the producer lease until
both consumer copies complete. Ring exhaustion skips production; replacement or
failure releases leases only when safe. A failed D3D11 completion-query poll
quarantines all pending GPU leases and rejects further copies; those leases remain
held until a separate completion proof; terminal abandonment never grants reuse.
On the same immediate D3D11 context, the bridge queues one fresh EVENT query
after every old copy. Its first GetData allows one submission check; later polls
use DONOTFLUSH without waiting. Only S_OK with completed=true retires the entire
quarantined batch and clears the opened-resource cache for subsequent copies.
A failed recovery query stays quarantined without repeated recreation or lease
release. Bridge destruction abandons unproven leases without setting
consumer_done: the producer still rejects invalidation/reuse of those slots.
Dropping session/COM references alone is not retirement evidence. Safe resource
teardown/recreation after device removal needs its own qualified boundary.
Original failure counters remain nonzero after successful recovery.
If ownership cancels a pair after eye copies but before `EndFrame`, the next
capture reuses only that unfinished, unfenced, unpublished slot. Subsequent
copies on the same D3D9 device follow its earlier commands in order. Pending
producer fences, failed-query quarantine and consumer-owned slots remain
ineligible; a movie/menu transition cannot permanently strand an unfinished slot.
Context/device mismatches and stale/unknown producer epochs reject; source or
consumer replacement defers while old copies remain pending. Rebinding and cache
reopening require retirement first. Rebuild/invalidation defers while a consumer
owns a slot. OpenSharedResource itself does not copy. These are renderer-owned
contracts; they do not establish exact-game device-loss or reset recovery.
If a D3D9 producer event-query poll fails, its pending slot is quarantined from
collection and reuse; other slots can still publish completed frames. A
quarantined producer frame is counted as invalidated once, including when its
resources are later torn down. Producer resource-generation transitions remain
subject to the D3D9 device/reset boundary and need exact-game loss/reset coverage.

This removes native-stereo CPU readback/LockRect/UpdateSubresource; immediate flat
capture remains separate. The classic CPU stereo path is diagnostic compatibility,
not the active performance path. Telemetry separates production, new submissions,
repeats, drops, frame age and query retirement from compositor refresh.
Per-frame producer/consumer lease event logs sample the first eight frames and
each 90th frame; copy ordinals preserve sampled lease-to-fence association,
including retirement during shutdown drain. Aggregate counters remain unsampled.
Up to three completed D3D11 event queries are reused on the same immediate
context. Only a query that proves its own copy completed enters the cache.
Recovery retires its quarantined copies through separate proof and discards
their queries. A consumer device or context change clears the completed cache.
Independent recovery emits begun/completed/failed events with the affected copy
ordinal range and retained failure counts. The final GPU summary and local
performance summary include recovery, identity rejection and transition deferral
counters. A recovered error does not satisfy the normal zero-failure verifier.

The game producer cap follows the HMD display-frequency property. One bounded
cancellation-aware CPU schedule covers stereo and flat modes, rebases at rate/reset/
pause changes and samples latest tracking afterward. Valid HMD timing selects
IMMEDIATE presentation to avoid monitor-vsync double wait; desktop mode remains
native. Failed Create/Reset restores caller parameters and does not commit cadence.
Missing timing retains the last valid cap; late timing can require native Reset.
The cap is not compositor phase lock; WaitGetPoses owns presenter cadence.

The primary D3D9/OpenVR route preserves the game's native desktop pixels at
Present after the stereo/theater capture. A former opt-in black-backbuffer
transaction was retired after the operator reported black screenshots and
desktop flicker and telemetry demonstrated native/black alternation. The
restoration changes only the desktop presentation; game eye captures, SteamVR
submission, pacing, ownership and the original native Present semantics remain
independent. Current-source screenshots and desktop behavior still require a
normal game-session check. The discarded black-surface approach is retained only
in Git history, not an active selectable feature.

Refresh-rate policy does not scale native movement or jump by the HMD frequency.
The adapter retains shaped native movement magnitudes, digital jump edges and
elapsed-time smooth yaw. Host fixtures cover 72/80/90/120/144 Hz input/pacing;
they do not replace the original simulation clock/physics or establish real
distance, jump height or wall-clock progression under lower native frame rates.
Per-device optics use OpenVR projection and eye transforms; default bindings
share logical actions while retaining device-specific physical routes. Physical
qualification remains separate for each controller/runtime combination.

Native sprite/flush boundary hooks are optional resource diagnostics, separate
from HUD presentation and text capture. Register their observer only with the
startup `renderResourceProbeEnabled` opt-in, alongside the render-pass observer.
A retained HUD diagnostic callback also checks current diagnostic permission
before forwarding to D3D state queries/logging. Ordinary community sessions
must not install these diagnostic hooks while the resource probe is disabled.
Removing this work does not establish a performance improvement on a headset.

The separate HUD batch collector uses actual natural boundary/depth/owner and
hook-epoch witnesses, with six passes and 64 per-pass state queries/records.
Existing draw ingress supplies pre-call state and the original returned result;
no diagnostic lock or getter reference crosses forwarding. Per-draw permission
is a cached owner read with an independent epoch, not file polling. Native device
generation, continuity, frame/Present/thread and hook identities stay distinct.
Faults, nesting, reset, changed ownership and missing/overflowed closure retain
incomplete metadata. This source implementation and authored C++ fixtures await
compiled verification. It observes no pixels and cannot qualify alpha/coverage,
depth contents, resource lifetime, native HUD capture or compositor retirement.

Periodic native FX-global/emitter walks and duplicate shot/barrel comparisons
also require that resource-probe opt-in. Repeated skeleton element discovery is
restricted to `movementTraceEnabled`; it is not proof of body ownership. These
integration observations are separate from functional bow shot counters,
muzzle/hand verification, exact-build admission, restoration and fault reporting,
which retain their ordinary safety/behavior roles.

## Evidence and lifecycle

The startup-only `frameLumaProbeEnabled` diagnostic is renderer-owned and off by
default. It reads native per-eye RT0 and desktop source points into private tiny
surfaces without rebinding native render state. Correlated raw RGB statistics
are evidence of sampled content, not a flash cause, photometric measurement or
performance acceptance. Queries poll without flush/spin; unsupported readback
disables only the diagnostic stream. Its resources release before Reset/ResetEx
attempts, device replacement and shutdown. It does not change rendering effects
or the ordinary stereo/interaction paths. Binding-census collection is a separate
diagnostic and is not required for this pixel investigation.

The exact pre-DestroyGame import boundary stops producers/restores hooks, joins the
presenter, drains pending GPU copies, releases capture resources and forwards
native destruction once. Atexit is too late for worker cleanup and only reports
incomplete shutdown without COM/GPU/thread work. [Shutdown contracts](research/COJ_SHUTDOWN_BOUNDARY.md)
retain exact hashes/RVAs, ownership and final-counter requirements.

Steady-state GPU queries do not wait. Shutdown alone permits a bounded completion
barrier; failed drains retain quarantine without signaling completion, including
after bridge/session destruction. Final GPU/
proxy summaries must be unique and agree on copied/completed totals, zero pending
copies/abandoned leases and full published-lease reclamation.

A thin exact-game render-hook SEH filter optionally records the first escaping
access/in-page fault: run/build/process/frame identity, actual exception and
access addresses, module/RVA, registers and a bounded stack. Fixed storage and
direct file writes avoid telemetry locks and renderer/JNI mutations. It always
continues native exception search; no process-wide handler, suppression or
recovery is installed. This diagnostic boundary is host-tested, not a crash fix.
Collection admits only a bounded report matching the owned native start/run/build
and process. Recollection preserves previous reports outside the current package
before rebuilding its hash inventory; rejected/absent reports cannot leave an
uninventoried ZIP member. An unstarted run needs no runtime folder. Ordinary
collection, native failure and operator/visor acceptance remain separate facts.

The exact adapter preserves the scheduler's ambient current-level binding around
injected UI navigation and pre-shoulder gameplay dispatch. Before returning from
that dispatch, a cleared binding can resume only the captured level under the same
game singleton and fresh active-list membership. Foreign contexts, removed levels
and failed checked writes reject before this frame's body/eye transactions; neither
menu ownership nor inventory is changed. Native render continuations stay unchanged.
The [camera research](research/COJ_CAMERA_PATH.md#ambient-current-level-context)
records the native null-dereference boundary and host-only verification limits.

Raw videos/logs/dumps and run metadata belong under ignored local evidence and may
be removed once their durable conclusions are captured in source, tests and focused
research. Active staging/recovery journals and their original backups must remain
intact. Cleanup cannot substitute for finish or promote a physical gate.

The diagnostics-owned `analysis/validation-result.json` persists the canonical
native-stereo verifier outcome (`pass`, `fail`, `inconclusive`), rejection reason,
source/build/run identity, validation profile and requirements. Input hashes bind
the exact log, run/staging/build manifests, deployed files and verifier tools.
The verifier writes its result on success, early startup return and rejection;
collection rejects an executed result if its inputs changed. Collection without
verification records `inconclusive`, never a telemetry pass.

The evidence manifest inventories the result and build manifest and exposes the
telemetry outcome separately from runtime completion. Optional run-local
`operator-observations.json` is independently inventoried and cannot alter that
outcome. Successful collection/restoration preserves canonical rejection;
`headsetAcceptance=not_evaluated` prevents automatic promotion of a physical gate.
A retained unstage journal routes `finish` directly to restoration recovery,
even if stage state remains; retries preserve the original verdict and package
instead of revalidating partially restored inputs.

## Primary validation hardware

PS VR2 with Sense controllers through SteamVR is the development reference.
Bindings belong to runtime assets; game logic consumes logical actions/poses.
Compatibility with other devices requires independent acceptance.

## Public pre-alpha distribution

`tools/package_release.ps1` builds/tests the Win32 OpenVR path and packages an
explicit file allowlist with a complete SHA-256 inventory. Dependency identities
live in `release/dependencies.lock.json`: portable isolated Python, the x86
OpenVR DLL and the official Microsoft-signed x86 VC++ redistributable. The player
uses Windows PowerShell 5.1 on x64 Windows; no compiler, Git, system Python or
dependency download is required. Game files are patched locally and never shipped.

`release/cojvr.ps1` owns the public install/session journal and calls the existing
exact-build transactional stage/unstage boundaries. Updates restore originals
before deploying a replacement; failure retains recovery data or restores the
original game. Default Install/update enables manual reload on fresh installs
and preserves the installed choice on upgrades. `release/cojvr-gui.ps1` provides
a Windows PowerShell 5.1/WinForms graphical front end: Steam AppID 3020
discovery through registered Steam roots, `libraryfolders.vdf` and
`appmanifest_3020.acf` (`installdir`), including moved libraries and renamed
install directories. GOG discovery uses the original product's registered and
default locations; custom/offline locations remain selectable with Browse.
The shared exact-executable catalog owns edition and matching Data0 hashes;
engine and code originals must also match before mutation. The installer derives
the explicit experimental GOG staging option from that admitted identity. Both
editions use the same payload and transactional restoration; unknown or mixed
originals fail closed. This is experimental compatibility of the tested GOG build,
not a second fully supported campaign/hardware target.
Discovery checks for `CoJ.exe` and `ChromeEngine3.dll`,
deduplicates detected paths and allows choosing between multiple installs,
explicit rescanning, editing the game path or browsing. Detection is read-only;
the install transaction still requires exact SHA-256 game identity. The GUI
also provides package verification, install/update, session preparation, status, local
diagnostic ZIP and uninstall/recovery. It runs a bounded, encoded invocation of
the existing `cojvr.ps1` actions in a child PowerShell process and displays the
output without blocking the window. `CoJ-VR.cmd` opens the GUI with no arguments;
explicit arguments continue to use the scripted launcher. The GUI preserves
the SHA-256 game admission, journal, backup/restore and manual game/SteamVR
lifecycle because these remain owned by the CLI transaction implementation.
When opened from a recognized source checkout, GUI actions use the repository's
`tools/cojvr_source_action.ps1` bridge. It prepares a local player package from
the existing provenance-checked native Release build, verifies its inventory
and invokes the same packaged backend. A SHA-256 cache key covers current
installer/patcher inputs, build manifest and native artifact bytes; every reuse
revalidates the player inventory. Package creation has an exclusive lease and
rejects linked or incomplete destinations. The original build provenance is
retained; local dirty packages are not public releases. A missing native build
requires the development build workflow. Incomplete player ZIPs never gain a
source fallback. Preference-only saves need no package generation.
Child streams are formatted to UTF-8 text before redirection, with explicit
exit-code forwarding and a retained process handle. Built-in utility/security
modules are selected from the Windows PowerShell host, independent of an
inherited PowerShell 7 module path.
The window groups the six actions separately from startup resolution and the
advanced reload installation choice. Resolution offers Preserve, Auto, 75%,
100%, 125% and explicit manual per-eye dimensions, persisted through the
existing `ConfigureResolution` backend; changing the selector alone has no
effect on a running game. Its Western presentation uses parchment/leather/copper
colors and an original, runtime-drawn GDI+ frontier scene (sun, mesas, windmill,
cactus), with distinct Install and Uninstall treatments. No copyrighted game
image is packaged. Host integration covers all backend actions against an
isolated exact-game copy, including updates, resolution persistence, recovery
and byte-exact uninstall. WinForms inspection and operator use remain distinct
from physical gameplay validation.
The GUI's advanced On/Off choice changes manual reload only through the same
transactional Install/update path while the game is closed;
explicit noninteractive `-ManualReload:$false` remains available. All explicitly saved resolution
preferences survive updates. Input manifests bind
every referenced controller JSON, preserving legacy Sense-only restoration.
Additional bindings are experimental, independent of physical acceptance.

Install enables ordinary stereo, independent hands, native torso/legs and physical
Hands combat alongside manual reload, and prepares the first session. PrepareSession
retains that installed feature set without repatching archives, rotates prior outputs
into local evidence and assigns a fresh correlated run/stage identity. For queried
profiles the user starts SteamVR manually before either preparation, then starts
the game manually afterward. Launcher option 9 exposes startup and per-profile
pause/save-menu controls. Only video settings paths may admit recognized Cloud
Files provider reparse tags; junctions, symlinks and unknown tags still reject,
including ancestors of missing destinations and journaled recovery targets.
Game/package/backup paths retain the strict rule. This is host-tested path
compatibility, not physical OneDrive synchronization acceptance.
Manual reload retains its On/Off installation capability and a separate persisted
Options > Game intent. The runtime selector and safe active-ownership cancellation
have host coverage; live switching, recovery and native widget readability remain
separate physical gates.
Theater stays the cinematic default; the rejected
independent eye RT prototype and investigation probes remain off. Community
reports use a bounded text
allowlist and redact common local paths; they are inspected and shared manually.
They do not replace canonical `vr_test.ps1 prepare`/`finish` acceptance evidence.
The GitHub release workflow produces a draft prerelease for explicit publication.

## Physical boxing

The shared `MotionPunchContact` policy admits current closed-fist poses per hand
in tracking and head-relative spaces. A fresh moving pose queries measured skin
contact immediately; no completed historical curve, minimum travel, timed guard
or long-stroke rearm delays the attack. Real grip and head-relative translation
must meet the existing motion threshold, so a stationary or rotation-only fist
cannot attack. Contact latches until a later clear query, preventing repeated
attempts against the same surface. Owner, player/input/context generation,
tracking, time and recenter changes cancel pending contact and retain available
release barriers. Duplicate delivery does not advance the geometric baseline.

The CoJ adapter admits only the exact `WeaponHands` with stable native ownership
and hand states. It maps every point through the same current camera/body
frame in centimetres, after verifying the scoped tracked-hand transaction.
`cojvrPhysicalPunch` rechecks the native owner, campaign/presentation/carry/death
and attack permissions, then calls native `Weapon.Attack` synchronously. Native
logic selects the physical hand's attack mode and retains cooldown, collision,
damage and effects. Temporarily owned world segments replace the exact melee
loop's trace count and bypass its authored local-coordinate transformation.
The original collision/hit loop consumes the current independent segment bundle,
with one native attack and native hit de-duplication across those rows. The first
tangent supplies only the auxiliary origin/basis. JNI derives the row type from an actual array of the
admitted Vector class, without a separate class-loader lookup, and releases all
payload references after the attempt. No shared attack parameters or health are
written. Scope cleanup also rethrows Java failures unchanged. Synchronous boxing
reaction reads the native attack's damage type through the same transient scope.
Tracked fist rendering uses the authored hand socket, without a firearm barrel.
Attempt telemetry is not hit confirmation. Four sampled skin witnesses do not
qualify complete fist volume, guard/blocking or native AI anticipation of a
controller wind-up. Firearm input and dual reload retain their existing routes;
the exercised melee acceptance belongs to [Validation](VALIDATION.md).

## Physical-candidate image quality

An experimental, opt-in D3D11 spatial processor runs once on each fresh ordinary
native-stereo pair, after source upload and before VR reticles/HUD/handheld mesh
composition. It provides contrast-adaptive local sharpening, spatial smoothing
of diagonal stair steps and isolated high-contrast features, and
contrast/saturation/gamma controls. It is not AMD CAS, FXAA,
DLAA/DLSS, temporal reconstruction or supersampling; it retains native eye extent,
pose and generation. Native HUD already baked into captured color cannot be
excluded by this ordering. Theater/menu captures, repeated frames and the
independent-eye private bank bypass it.

The shared graphics owner initializes
`%LOCALAPPDATA%/CoJVR/CallOfJuarez/graphics.ini` once per process, separately from
cinematic `settings.ini` and launcher `resolution.json`. The strict version-5
file requires all eleven keys exactly once (complete seven-key version 1,
eight-key version 2 and ten-key versions 3/4 remain readable); missing, unreadable, oversized or
invalid files initialize processing off and log the reason. Native Options > Game
edits and saves the ten controls, applying the spatial tuple to the next fresh
native pair without restarting. Cancel/highlighting/default staging do not apply
graphics changes. Disabled or enabled-neutral controls are an exact bypass without
processor GPU resources. The optional
[`source example`](../assets/graphics.ini.example) remains available for manual
startup configuration; external file edits require a game restart. Its proposed
sharpening value is an experiment, not an accepted visual preset. `sharpness` accepts 0..1, `contrast`
0.5..1.5, `saturation` 0..2 and `gamma` 0.5..2; color neutrality is 1, sharpening
neutrality is 0 and `edge_smoothing` is a lowercase boolean.
`reconstruction` is `native`, `dlaa` or `dlss_quality`; it records intent only until
the exact-game temporal source and compositor path are qualified.
`render_scale` selects Auto, 75%, 100%, 125%, 150% or 200% via values 0..5;
`horse_comfort` selects off/mild/strong via values 0..2. A positive mounted-motion
input observation publishes a 150 ms lease; unavailable input, loading, frozen
time or expired lease disables the mask. The compositor dims the periphery of
fresh native pairs before overlays, independently of color-filter enable. It
changes neither horse speed nor camera/FOV.
The mask uses one head-forward angular cone at infinity, projected through each
physical eye's asymmetric FOV and complete eye-to-head rotation. Its reference
radius is the largest such cone inside both physical frusta; eye translation
does not introduce finite-depth disparity. Equal head-space rays receive equal
attenuation even when their texture UVs differ. Missing/invalid optical pairs
reject before GPU allocation or either-eye writes. Scope masking retains
precedence and Off/expired-motion captures retain exact native pixels.
Mild uses clear/full-dimming fractions .42/.78 of that shared tangent radius;
Strong uses .28/.60, retaining .55/.85 maximum dimming. The head-forward centre
and alpha remain unchanged. WARP pixel tests cover asymmetric/canted eye rays,
both strengths, invalid pairs, non-square/actual eye extents and fresh exit.
The previous equal-UV circles had a demonstrated binocular geometry mismatch;
these host checks do not qualify the revised mask's physical appearance.
Comfort and physical visibility of the expanded native editor remain unqualified.

The renderer validates both same-device, distinct, equal-size private BGRA/BGRX
UNORM outputs before writing. Shared/misc-flagged endpoints are rejected because
different COM objects can alias one shared allocation. One reused source scratch texture processes the eyes
sequentially, capped at 128 MiB (32,077,760 bytes at 2804x2860). Private D3D11.1
context state isolates and restores caller bindings. Unsupported devices/formats
or allocation/setup failures disable effects until a deliberate new options
acceptance permits another setup attempt, retaining the native pair. A detected
removed device terminally revokes old/new
submission cadence and stops the presenter through common cleanup. A later CPU
upload cannot revive that consumer; no automatic recovery is claimed. Sampled
telemetry correlates capture/device/generation and reports CPU command-queue
time and scratch allocations, not GPU timing or a performance improvement.
Spatial output quality, headset comfort, sustained pacing and allocation/device
recovery remain physical gates in [Validation](VALIDATION.md).

An original experimental `cojvr_dlss_client` library and separately built x64
`tools/dlss_host` implement the helper pattern researched in FarCry2VR. The helper
uses official NVIDIA NGX D3D11, the consumer adapter LUID, separate features per
eye and private NT-shared RGBA16F color/output, R32F standard depth, RG16F motion
and R8 current-color bias. A fixed-width versioned pipe protocol validates both
process identities and bounds. Idle waits monitor parent/pipe lifetime without
expiring a healthy paused session; message completion has a bounded deadline.
Failed startup drains pending connection I/O before rolling back the owned child.
The owned hidden helper has kill-on-close lifetime;
it never owns the game or SteamVR. CPU-completed shared fences precede
acknowledgement/output admission; timeout/exit is not GPU completion. Uncertain
client allocations are quarantined rather than recycled. Missing helpers,
runtime, capabilities and incomplete guides reject reconstruction.

Protocol 2 sends NGX into a single private FP16 scratch texture. A compute pass
copies reconstructed pixels into the current eye's shared output and substitutes
bilinear current color where the nearest R8 mask texel is fully marked. The mask
and color sample the same jittered location. The wire offset describes the
rendered image shift in input pixels, with positive X right and positive Y down.
The helper fences NGX and the resolve together before acknowledging the eye.
Both eyes reuse scratch after retirement and keep distinct shared outputs. A
failed completion check requires independent retirement proof or observed
device removal for orderly NGX teardown.

The separate experimental `DepthHistoryMask` pass compares current R32 device Z
with the reprojected previous eye depth entirely on D3D11. It requires adjacent
frame/pose identities, the same device/generation/owner/extent and unchanged
settings, history epoch and input context, plus distinct nonzero capture tokens.
Current and previous jitter map through the unjittered camera transform with
opposite sampling offsets. Explicit previous finite/infinite optics convert
depth to axial metres; a caller-supplied relative tolerance determines agreement.
A bounded zero-to-two-pixel max filter expands current-color exclusions and
rejects out-of-image neighborhoods. Native partial exclusions remain partial.
Both eyes validate before writes; failure withdraws both published masks and
caller pipeline state is restored. Inputs remain immutable through retirement;
the pass does not retain or commit history. Its private R8 storage is four input
pixel planes. Depth agreement cannot identify same-depth or transparent motion, so
native dynamic/material exclusions remain an independent requirement. Explicit
tolerance/halo experiments are not a qualified game preset.

An opt-in `DepthMaskHistory` owner now connects this pass to `PairSession` and
the persistent worker. The active game adapter leaves the option disabled.
First/reset pairs use fully current-color masks. Later pairs use private R32
depth from the last complete stereo reconstruction and value-only optics,
matrix and capture-token metadata. The session prepares masks before inference
and commits depth only after both eye outputs are available. Copies and later
reads remain ordered on the serialized immediate context; no native capture
lease is retained. Any failure/cancellation withdraws publication and mask
history. Policy changes reset both eyes; source settings/epoch/context fences
still apply. Unsupported executors reject enabled masking before evaluation.
Infinite-far masking requires standard or packed device Z; the existing axial
guide conversion has no infinite-projection contract. History and seed masks
add ten input-pixel bytes to the four-plane mask pass, fourteen in total. The
worker accounts for this optional storage in its guide/output allowance;
reset/disablement retires cached mask allocations before new feature/guide
allocation. This is not a bound on whole-process or driver residency. Ordinary
supplied-mask reconstruction and the copy-only native audit retain their defaults.

`runtime/temporal_reconstruction` provides unjittered row-vector reprojection and
history admission/reset policy. `temporal_guides` privately generates paired
depth/camera motion/current-color guides on D3D11, rejects unqualified sources,
restores caller state and exposes no stale guides after rejection. This library
is exercised by host fixtures, not linked into the active presenter. Its source
qualification bit is an adapter obligation, not a depth-content detector.
CoJ still transports presentation color without a qualified sampled geometry-depth,
motion/jitter or temporal-input lease contract. The optional existing resource
probe can now sample 16x16 HDR-alpha values from a currently COM-owned pixel texture
binding only when it matches the exact-build `1B_16` descriptor. The bounded
observer now attempts copying at the matching native draw, while the COM-owned
binding is still present, and reports missing consumers at scope end. Actual
registers are queried across 0..15: material user sampler 10 is a logical name,
while collected consumption used registers 0/2. The earlier fixed-register
observer produced no pixel samples even in the long exercised session.
Samples have bounded copied lifetime and explicit
`temporalAdmitted:false`; they are diagnostic, not depth transport. The
extended observer has live RGB/alpha delivery for three consumers per eye in
stable gameplay windows. Its optional scope-local write-call witnesses count
successful RT0 draw/Clear and typed native StretchRect destination-copy intents, retain
alpha write masks/last-writer shader identity, and expose read failures. Counts
are not proof that fragments modified pixels or that all MRT/copy paths were
covered; live copy rectangles retain partial/full extent explicitly. Borrowed
surface arguments remain scoped to the synchronous hook callback; copied numeric
addresses never authorize COM access. Diagnostic copies are excluded from their
own ledger. Scope-end totals
can expose writes after a sampled consumer. No source qualification follows.
The same frozen copy-producer snapshot now retains up to four distinct vertex
programs from its first 64 draws and the first draw's 256 float constant registers
per program. Program bytes and raw constant bits are immutable copied values;
COM getter references retire in the callback. Omitted draws, read failures and
unavailable constants remain explicit. This is a bounded material-investigation
witness, not per-instance motion, fragment coverage or temporal admission.
The [postprocess ownership findings](research/COJ_PPFX_TARGET_OWNERSHIP.md#hdr-alpha-depth-candidate)
own material semantics and remaining coverage gates.
Live copy witnesses now separate pre-copy consumer contents from the subsequent
equal-extent FP16 scene copy and stable later samples. The bounded collector
requires that observed complete copy within the current eye scope before
selecting a phase; partial/scaled/self/non-FP16 copies, later observed source
writes, reset and read uncertainty withdraw readiness. This is observational
readiness with explicit coverage limits, not source/eye/depth qualification.
Consumer ordinals remain local and are renumbered after filtering. Copied grids
stay 16x16 and cannot become a full-size temporal-input lease.
The filter's subsequent delivered pairs retain an unresolved split-alpha pattern
despite differing RGB. Bounded upstream RT0 clear/draw, alpha-write-mask and blend
call witnesses now freeze at native copy time; absent/unreadable producers remain
unknown. Their counts do not establish fragment writes, depth or material coverage.
The first blend-enabled alpha draw per bounded producer additionally freezes
effective alpha blend factors/operation and up to4KiB of its owned shader program.
No logged address is revisited. Incomplete/oversize/failed bytecode reads remain
unknown, with one attempt per producer and no new GPU allocations. Offline alpha
algebra/program digests do not qualify material coverage or temporal input.
Camera-only motion reconstruction does not describe independently moving
hands/NPCs/foliage. Actual CoJ capture, scoped jitter, dynamic masks, per-eye
presentation/history commitment and reduced native raster work remain pending.
No saved DLAA/DLSS menu request launches the helper or changes game rendering.
The official SDK/runtime is an ignored, pinned external dependency prepared by
`tools/bootstrap_dlss.ps1`; the x64 helper and NVIDIA binaries are excluded from
normal game staging/releases. Local host experiments do not qualify distribution.
Shared sun shadows also
need independently demonstrated Chrome light/cascade/caster and atlas lifetime
equivalence. Neither technique is enabled by this spatial processor. The external
FarCry2VR comparison is retained in ignored local `work/research/VR_REFERENCE_RESEARCH.md`.
Native video options already expose anisotropic trilinear filtering; its native
mode is not evidence of a forced 16x factor. No sampler override is introduced.

`prepare` preserves the selected game resolution and stages FSAA 0 unless the
operator elects to keep video settings. Original video configuration is journaled
and restored by `finish`. Intervening video edits are retained as verified,
journaled conflict copies before original restoration; conflicts or concurrent
edits that cannot be preserved retain recovery state. Source resolution, eye target extents and runtime optics
retain explicit provenance; no silent supersampling/performance preset replaces
native ownership. Frame-generation experiments remain separate from the active
stereo path. Sustained pacing, recording compatibility, resource loss and broader
campaign behavior retain independent gates.
