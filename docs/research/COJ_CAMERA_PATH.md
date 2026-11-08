# Call of Juarez camera and stereo research

This document preserves the exact-game findings that still constrain the implementation. Superseded probe chronology and raw investigation logs were removed after the contracts below were established.

## Proven camera path

For the supported Call of Juarez build, the useful ownership chain is:

`Camera -> View/Projection -> ChromeEngine3 render-view wrapper -> active D3D9 render target`

External FOV control and restoration proved the camera-to-renderer seam before stereo work began.

The camera contract uses the game's complete source basis. The authoritative source layout is right/up/forward/position with paired inverse/view state. Do not reconstruct the first axis as `forward x up`; reflected/non-rigid bases must fail closed.

The live game also required a game-specific yaw sign correction after the native right/up/forward basis was established.

Camera translation and skeleton/controller targets share the demonstrated
tracking-Z sign and leveled reference. The source axis named forward is the
view's backward axis in the gameplay-reticle convention. Actor-owned yaw is
removed exactly once. Earlier reflected tracking-Z or camera-only translation
exposed the avatar/inverted physical motion; the corrected room-scale direction,
body visibility and physical-crouch regression are operator-accepted for the
exercised context. Source layouts and stereo wrappers are unchanged.

## Render-view ownership

Exact disassembly established two relevant boundaries:

- `0x00030FB0`: complete render-view wrapper;
- `0x00030E00`: core render work only.

The wrapper manages the rendered guard at `view+0xD7`, active-view state and post-core work. A candidate that used the wrapper for the left eye and core-only path for the right produced `D3DFMT_NULL` for the right-eye target. Both eyes therefore execute the complete wrapper, with the exact guard handled transactionally around the second pass.

This is an exact-build Call of Juarez contract, not a generic Chrome Engine rule.

### Existing per-eye culling state

The active adapter updates both paired source transforms before the original
camera render update. The native update propagates them into the culling/world
and rendered-view paths. `HookSetFov` then installs the runtime eye's asymmetric
frustum and invokes the exact native projection builder. These source/frustum
fields remain scoped to the complete eye wrapper; restoring them immediately
after camera update would let later visibility work see the natural mouse camera.

The exact projection builder also rewrites the inverse projection, projection
mode, and world-space corners, planes and bounds. The eye transaction now
restores their native preimage with the source/view/projection matrices.
An isolated native replay rejected a test object with stale eye planes after
the previous partial restore, and admitted it again with the original planes.
Both eyes rebuild their planes before the wrapper. Whether an intervening
native consumer explains the reported tree popping remains unqualified.

Consequently, a future culling optimization must measure additional visibility or
draw work at this existing boundary. A different mod advertising VR culling is
not evidence that CoJ currently culls from a flat-camera pose, nor justification
for shrinking either eye frustum or sharing visibility between eyes. Correct
coverage, dynamic objects and native effects remain prerequisites for any such
performance experiment.

### Native audio-listener ownership

Static inspection of the pinned engine identifies
`Module.ListenerOnViewActive(IZ)V` at RVA `0x00039CF0`. Its native helper at
`0x00039A00` updates the view's listener-enabled flag and the active-listener
count. Original `LawmanModule` bytecode selects listener ownership when changing
between normal and movie views. The flag is ownership policy, not a listener-pose
setter, and must not be toggled independently of native view transitions.

The engine contains both DirectSound3D and dynamically loaded OpenAL mixers.
The OpenAL update at RVA `0x00104810` reads its retained listener record, scales
position with the mixer's native factor and submits position/orientation through
`alListenerfv`. It negates the record's direction and preserves its up vector.
These are exact static observations, not a generic mapping from OpenVR vectors.

The native active-view loop calls camera slot 7 (`0x001C4F20`) before collecting
listener data. The listener collection at `0x00039B50` reads derived camera
position at `+0xF4`, direction from the negated forward row at `+0xE4`, and
up from the negated up row at `+0xD4`. It computes velocity from the view's
previous position and submits to the common mixer record builder at
`0x000F65D0`. The stereo wrapper does not call this collection.

Slot 7 returns to RVA `0x00028F58` before a separate active-view loop.
That loop reads derived camera position at `0x0002900D`, constructs a spatial
query around it and calls the query at `0x000290C3` before listener collection
at `0x000290CE`. The same position is also copied into a native per-view
position list. The earlier experimental hook restored the source matrices
but left the modified derived camera visible to these consumers. That hook
has been retired.

A future candidate must intercept only listener collection at `0x00039B50`
or the pose inputs to the common mixer record builder at `0x000F65D0`.
Both known collection callers (`0x00028C09`, `0x000290CE`) require explicit
ownership analysis. Preserve native view selection, environment and scale,
and qualify position, orientation and velocity together before a headset run.
Both collection paths converge on the call at `0x00039CC7`. Its relative
operand is aligned on x86. The collector passes position, direction, up,
velocity and native context to the builder, then stores the natural position
as velocity history at view `+0x18`. A builder interception would need its own
qualified view identity and head-offset history. Offline emulation of the
actual collector and builder confirms that replacing those four vectors for
an admitted view updates the pending record without changing the camera or
natural velocity history. A foreign view forwards the original arguments.
Disabled listener and full record bank cases preserve native behavior.
Patch ownership, safe install/restore and live mixer output remain unverified.

## Eye state and units

OpenVR eye-to-head transforms are in metres. Static Call of Juarez data established movement/world values in centimetres. Early stereo used the metre baseline directly and therefore shrank physical eye separation by 100x.

The Call of Juarez adapter now performs the only metres -> centimetres conversion. Shared XR/runtime math stays in metres.

The corrected 100 game-units/metre eye baseline and recenter have physical acceptance for the exercised path.

## OpenVR projection conversion

The OpenVR adapter maps `GetProjectionRaw(left, right, top, bottom)` to neutral
eye tangents `(left, right, up=bottom, down=top)`. This preserves the vertical
scale `2/(bottom-top)` and center `(bottom+top)/(bottom-top)` of Valve's
[ComposeProjection reference](https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetProjectionRaw).
Negating both raw vertical values instead reverses the center for vertically
asymmetric optics; symmetric vertical optics cannot expose that error.
The exact native projection builder at RVA `0x0022BC50` propagates the supplied
frustum's vertical center rather than cancelling that difference. Gameplay and
flat-theater menu placement consume the same converted eye data. Keep this
conversion in the OpenVR adapter; no headset-specific FOV or menu offset is needed.
Physical acceptance remains owned by [Validation](../VALIDATION.md).

### Sampled temporal optics

The pinned projection builder at `0x0022BC50` writes a right-handed, off-center
D3D9 perspective matrix at camera `+0x184`. The default far plane is infinite
for device depth, even when the camera exposes a finite far limit. The
alternate branch uses that limit for finite depth. Offline execution of both
native branches and the product routine at `0x001F4F30` verifies the
coefficients and row-vector order `view × projection`.

The sampled depth probe reads native view, projection and their product at
the qualified scene copy. It requires the active eye, frame, pose, renderer
camera and recenter sequence to agree. It rejects inconsistent products and
unsupported projection layouts. The adapter scales the spatial rows once
for world metres. A second read at final native color capture can attest
matching optics for that eye. The manifest stores the view-projection and
its device-depth convention. Temporal admission remains false.

Final-color observation has a different renderer-camera boundary from the scene
copy. The pinned native path restores the previous camera from render owner
`+0xB58` into renderer `+0x1AC/+0x1B0` at `0x00030753..0x00030784`
(also `0x0002F8C6..0x0002F8F1`). Requiring that transient renderer reference
after the full wrapper can therefore reject an intact eye camera. The completed
eye read instead requires the still-active exact eye/frame/pose scope and its
positive scene-camera witness. It independently validates the native matrices
again, and the source adapter still requires exact equality with the retained
depth optics. It does not synthesize optics from the earlier read or accept a
changed product, recenter, projection or eye. Scene-copy observation retains its
live renderer-camera requirement. Host hook tests reproduce restoration and
reject stale/foreign scopes and changed optics. The preview records final-read
validity separately from equality; older captures without that distinction
cannot establish which check failed or acquire retrospective admission.

## Capture boundary

Reading the swap-chain backbuffer from inside the render-view path produced identical left/right hashes even though two game eye passes ran. Capturing the currently bound D3D9 render target exposed the real eye results.

The valid native-stereo path therefore requires:

- real color RT0 for each full eye pass;
- distinct eye content;
- renderer-camera correlation for both eyes;
- fail-closed behavior when the right-eye target is null/invalid.

## Presentation and render pose

The presenter is decoupled from the game render callback. New stereo frames enter a ring/mailbox; OpenVR presentation runs at compositor cadence and may repeat the latest valid frame.

An early deferred-presenter version acquired compositor timing before a valid scene frame existed and could leave the SteamVR dashboard over the game. Later lifecycle work delays compositor pacing until a scene frame is ready and records focus/dashboard state.

Scene-focus/lifecycle ownership and explicit render-pose submission removed the
previous head-turn pull/snap-back behavior. Render pose/sequence must remain bound
to the captured frame rather than substituted from a newer tracking poll.

## Flat theater

Call of Juarez uses device `Present` for the startup/menu path. When no native-stereo producer has been active recently, the device-Present hook publishes flat content so SteamVR scene ownership exists before gameplay.

The flat image is projected as a finite-depth, head-centered plane with per-eye geometry. This replaced an earlier same-centered-image path that appeared doubled between the eyes.

Flat capture is immediate and keeps no persistent default-pool resource across loading/reset. This avoids the earlier reset/load hazard.

Startup flat theater through level load into native stereo is physically exercised.
Death/mission-end ownership also yields to native flat presentation even while
the timer runs: pure module getters observe alive state and menu 38, and a
post-input check prevents body/eye writes after terminal UI acquisition. Failure-
screen visibility is operator-accepted; retry/held-control recovery remains separate.

## Current open camera/presentation issues

The native camera/stereo seam, exercised room-scale direction/physical crouch,
independent-hand ownership, corrected snap direction and ordinary flat menus
have scoped acceptance. OpenVR positive stick X maps to native -45-degree yaw
(right), negative X to +45 degrees (left); native yaw has the opposite sign.
Normal-quit cleanup has bounded live evidence through the exact pre-DestroyGame
boundary; see [shutdown contracts](COJ_SHUTDOWN_BOUNDARY.md).

Open gates are sustained pacing/frame age, Steam recording-associated microskips,
arbitrary reset/device loss/abnormal exit, dashboard/loading ownership and
death/retry/held-control recovery. The bounded coherence trace observes poses
and both-eye bases; it does not rotate an actor or force render/FX updates.

Do not reopen established camera offsets or stereo wrapper choices without contradictory physical/native evidence.

## Ambient current-level context

**Observed fault / static exact-build evidence:** a community secret-scroll pickup
faults at ChromeEngine3 RVA `0x4609E`, `mov eax,[edx+0x378]`, with EDX zero and a
read of `0x378`. The preceding instructions load the game singleton from RVA
`0x542F58` and its current level at `+0x668`. This is mesh rendering, not an
identified `GetSurfaceFlagName` call; the game's symbol-only crash text is not
sufficient to name the actual failing function.

The native scheduler binds `CGame+0x668` before level rendering (`0x1D1965`).
The active-level vector spans `+0x65C` to `+0x660`; native level removal at
`0x1D27D0` clears the ambient slot (`0x1D2838`). The full view wrapper is still
`0x30FB0`, and its caller continues at `0x31073`. Original archive inspection
connects `SecretItem.InvokeUseSecretItem` through `SecretsManager.OnSecretUsed`
to `LawmanModule.ShowNewSecretMenu`, opening menu 47 synchronously. The report
ends game-thread production on that interaction and its natural fallback, with
no normal runtime end. It establishes the missing render context; it does not
identify the exact nested callback that cleared it or prove broader lifetime
safety during loading/unloading.

**Implemented / host-tested:** injected pre-shoulder gameplay and UI Back/Select
dispatches capture a nonnull current level equal to the native render owner before
calling the original native action once. At the dispatch's return, the singleton
and bounded active list are revalidated. A null current slot may be conditionally
restored to that captured live level. An unchanged slot requires no write; nonnull
foreign ownership, removed levels, invalid lists or failed write admission reject.
Repair authority is single-use and lexical to that dispatch. The binding is never
synthesized from a null entry, persisted across frames or copied to another singleton.

Repair at the input boundary precedes later body/render consumers and leaves the
full-eye, camera-only, natural fallback and restoration routes unchanged. A rejected
dispatch invalidates the pair and yields before new body/eye transactions, without
native cancellation against a lost owner. Native secret credit, menu creation,
timer ownership and input permissions remain native. The precise fault reporter
still propagates exceptions. Checked-memory regressions execute the production
dispatch helper followed by native fallback, covering null repair, foreign/removed
owners, replaced singleton, list reallocation/malformed bounds, read-only memory,
single-use authority, nested dispatch and original action success/failure. They do
not reproduce the complete campaign pickup or accept native menu/visor recovery.
