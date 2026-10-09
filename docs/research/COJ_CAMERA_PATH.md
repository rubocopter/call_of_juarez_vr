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
