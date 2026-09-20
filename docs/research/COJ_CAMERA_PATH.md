# Call of Juarez camera and stereo research

This document preserves the exact-game findings that still constrain the implementation. Superseded probe chronology and raw investigation logs were removed after the contracts below were established.

## Proven camera path

For the supported Call of Juarez build, the useful ownership chain is:

`Camera -> View/Projection -> ChromeEngine3 render-view wrapper -> active D3D9 render target`

External FOV control and restoration proved the camera-to-renderer seam before stereo work began.

The camera contract uses the game's complete source basis. The authoritative source layout is right/up/forward/position with paired inverse/view state. Do not reconstruct the first axis as `forward x up`; reflected/non-rigid bases must fail closed.

The live game also required a game-specific yaw sign correction after the native right/up/forward basis was established.

## Render-view ownership

Exact disassembly established two relevant boundaries:

- `0x00030FB0`: complete render-view wrapper;
- `0x00030E00`: core render work only.

The wrapper manages the rendered guard at `view+0xD7`, active-view state and post-core work. A candidate that used the wrapper for the left eye and core-only path for the right produced `D3DFMT_NULL` for the right-eye target. Both eyes therefore execute the complete wrapper, with the exact guard handled transactionally around the second pass.

This is an exact-build Call of Juarez contract, not a generic Chrome Engine rule.

## Eye state and units

OpenVR eye-to-head transforms are in metres. Static Call of Juarez data established movement/world values in centimetres. Early stereo used the metre baseline directly and therefore shrank physical eye separation by 100x.

The Call of Juarez adapter now performs the only metres -> centimetres conversion. Shared XR/runtime math stays in metres.

Physical run `20260916T153109Z-8976b8f77775` exercised the corrected `100` game-units/metre baseline and left-Sense Create recenter.

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

Physical run `20260916T221254Z-861f3c15abd4` validated the scene-focus/lifecycle correction. Strong head-turn ghosting remained until the exact HMD render pose and pose sequence were carried with each captured frame and submitted using OpenVR explicit-pose submission. Run `20260916T224239Z-e43b46698e5c` physically removed the previous pull/snap-back behavior.

## Flat theater

Call of Juarez uses device `Present` for the startup/menu path. When no native-stereo producer has been active recently, the device-Present hook publishes flat content so SteamVR scene ownership exists before gameplay.

The flat image is projected as a finite-depth, head-centered plane with per-eye geometry. This replaced an earlier same-centered-image path that appeared doubled between the eyes.

Flat capture is immediate and keeps no persistent default-pool resource across loading/reset. This avoids the earlier reset/load hazard.

Run `20260920T090448Z-b1f54e3cb38e` physically exercised startup flat theater through a level load into native stereo.

## Current open camera/presentation issues

The camera/stereo discovery gate is closed enough for product work. Remaining issues are operational:

- frame pacing/transport cost still affects sustained comfort;
- flat-menu controller interaction is being revalidated after a rejected physical route;
- recent body/playability runs reach outer `run_end` but still report incomplete inner presenter shutdown.

Do not reopen established camera offsets or stereo wrapper choices without contradictory physical/native evidence.