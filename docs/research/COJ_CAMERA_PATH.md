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
