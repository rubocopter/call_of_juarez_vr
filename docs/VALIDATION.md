# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

Per-run logs, process IDs, videos, raw telemetry and evidence packages are local working data and belong under ignored `work/`. This document records only durable acceptance state and the current physical gate.

## Current physical gate

The current backend is physically rejected for four playability areas:

- flat-menu pointer control is not yet reliable enough to replace the physical mouse;
- locomotion and jump behavior remain unacceptable even though native input reaches the game;
- tracked arm ownership is not continuous enough and can fall back visibly to native poses;
- weapon origin/direction is not yet aligned reliably with the visible weapon barrel.

Independent stereo, tracking, recenter and snap-turn results remain valid. A rejected playability gate does not invalidate unrelated capabilities that were already physically accepted.

## Next physical acceptance gate

One fresh `prepare`, one Call of Juarez process, one `finish`. The candidate should exercise:

| Area | Required observation |
| --- | --- |
| Startup/presentation | flat theater visible, stable scene ownership, startup skip works |
| Menu pointer | Sense-driven hover is controllable without touching the physical mouse |
| Menu actions | Cross accept, Circle normal Escape/back, L2/R2 ray-select |
| Blocking UI | post-load continue responds through the native loading-input path |
| Subtitles | shipped subtitle setting enabled first; visible text verified separately from capture/presentation |
| Recenter/horizon | recenter leaves the world level even if the HMD is tilted at the instant of recenter |
| Locomotion | walk/run speed is acceptable and jump has useful native-scale amplitude |
| Room scale | horizontal physical displacement preserves native collision/grounding and drives plausible visual walking |
| Crouch | physical HMD drop remains visually coherent while explicit controller crouch still works |
| Body IK | arms track continuously without unsafe deformation or repeated fallback to native poses |
| Weapon | tracked direction, visible barrel and final shot origin/direction agree |
| Mode transition | flat theater and native stereo switch safely where game state requires it |
| Shutdown | outer runtime and inner presenter finalization both complete cleanly |

Failure in one area does not promote that area, but independent observations may still be recorded.

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
| Classic-D3D9 presentation transport | open performance blocker | CPU readback remains structurally expensive; prefer a lower-copy/GPU-resident path before major resolution increases |
| Inner presenter finalization | open | recent physical testing exposed incomplete shutdown behavior |
| Native analog locomotion | live-exercised / rejected for playability | native routing is present; further blind remapping is not justified |
| Native jump action | live-exercised / rejected | input reaches gameplay but useful jump amplitude is not achieved |
| Room-scale visual body compensation | live-exercised technically | vertical pelvis drift was removed; visual walking parity remains open |
| Physical-walk visual animation | planned/open | should reuse native locomotion animation semantics without surrendering collision ownership |
| Sense tracking in game space | live-tested | left/right controller transforms reach the backend |
| Visible arm writer/restoration | live-tested | geometry changes and restoration are proven |
| Body IK continuity/anatomy | live-exercised / rejected | safety must not produce repeated visible fallback to default animation |
| Native reload ownership | host-tested | VR writes yield during native reload state |
| Flat-menu pointer | live-exercised / rejected | current ownership model remains unusable in-headset |
| Cross/Circle/L2/R2 UI actions | host-tested follow-up | physical acceptance still pending |
| Loading continuation | host-tested | native loading input route is mapped |
| Controller-origin UI beam | host-tested | physical usability still pending |
| Controller-owned weapon direction/visual origin | host-tested | final physical firing alignment still pending |
| Sense tip direction convention | live-tested diagnostically | local `-Z` is the demonstrated pointing direction |
| Temporary controller alignment ray | planned diagnostic | diagnostic only; must not become production ballistics |
| Physical gun origin/direction | pending/rejected | production shots must originate from the visible weapon/barrel |
| Supported end-to-end VR release | planned | project remains pre-alpha |

## Evidence policy

Only conclusions that remain useful across sessions belong here. Raw headset-run manifests, hashes, local video paths, telemetry counts, process IDs and agent handoffs stay under ignored `work/` and may be discarded once their conclusions are represented by code, tests or the durable documents above.
