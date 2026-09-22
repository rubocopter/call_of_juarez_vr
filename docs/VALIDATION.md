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

## Locomotion divergence diagnostic protocol

This protocol measures the current rejected locomotion/jump gate. It does not
promote gameplay support and must not be used to tune movement constants. Use
one instrumented non-VR baseline process, then one VR candidate/process for all
VR phases. The probe stops a trace automatically after 180 seconds.

Build and host-test once:

```powershell
cmake --build build-win32 --config Release
ctest --test-dir build-win32 --output-on-failure -C Release
```

For the instrumented vanilla baseline, create and stage the read-only camera
probe. It loads no XR runtime and owns no gameplay input:

```powershell
pwsh -File tools/new_build_manifest.ps1 -RepositoryRoot . -Configuration Release -DiagnosticMode d3d9_camera_probe -ProxyPath build-win32/Release/d3d9_camera_probe.dll -OutputPath build-win32/Release/d3d9_camera_probe.build-manifest.json -AllowDirty
pwsh -File tools/stage_d3d9_proxy.ps1 -GameDirectory "C:\path\to\Call of Juarez" -ProxyPath build-win32/Release/d3d9_camera_probe.dll -BuildManifestPath build-win32/Release/d3d9_camera_probe.build-manifest.json
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode vanilla
```

Start the game manually, load the same save and stand on the same flat ground.
Wait still for 3 seconds, hold `W` for 5 seconds, release it for 2 seconds,
hold `Shift+W` for 5 seconds, release it for 2 seconds, then perform three
separate standing jumps and let each one land. Disable the trace, close the game
normally, summarize, collect and restore staging:

```powershell
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode off
pwsh -File tools/summarize_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez"
pwsh -File tools/collect_run_evidence.ps1 -GameDirectory "C:\path\to\Call of Juarez"
pwsh -File tools/unstage_d3d9_proxy.ps1 -GameDirectory "C:\path\to\Call of Juarez"
```

Prepare one fresh VR candidate using the normal workflow. Start SteamVR and the
game manually. With every key and controller control released, select
`vr-full`, repeat the exact sequence above, then select `vr-input-off` and
repeat it. `vr-input-off` makes the game the exclusive owner of keyboard and
gamepad input: VR does not apply or neutralize any `InputAction` during that
phase.

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez"
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode vr-full
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode vr-input-off
```

Only if native-exclusive input does not explain the divergence, select
`vr-readback-off` and repeat. This keeps both eye renders and camera tracking,
discards pending capture surfaces, bypasses `GetRenderTargetData` and CPU pixel
copy, and lets the presenter repeat its last frame. Only if that is still
inconclusive, select `vr-single-eye` and repeat; this executes only the first
full render wrapper and also bypasses capture transport. Neither optional mode
is intended to be usable in the headset.

```powershell
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode vr-readback-off
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode vr-single-eye
pwsh -File tools/set_movement_diagnostic.ps1 -GameDirectory "C:\path\to\Call of Juarez" -Mode off
```

Close the game normally and run `tools/vr_test.ps1 finish`. The evidence package
contains `analysis/movement-summary.json`. Each phase reports duration,
distance, mean/max horizontal speed, walk/sprint speed and ratio, jump apex and
duration, update Hz, eye/pair cadence, presenter new/repeated submissions, and
readback/copy p50/p95/max.

Interpret the comparison as follows:

- Input ownership is causal if `vr-input-off` returns walk, sprint and jump near
  the vanilla baseline while `vr-full` diverges and update cadence is otherwise
  comparable.
- Simulation cadence is causal if VR game-update Hz falls or gameplay delta
  rises against vanilla, with proportional loss of distance or jump evolution,
  including in `vr-input-off`.
- Readback is causal if `vr-readback-off` restores update cadence and physical
  movement while the two eye-render counts remain active.
- The second render is causal if recovery appears only in `vr-single-eye`.
- The remaining fault is presentation stutter if actor speed, sprint ratio,
  jump apex/duration and update cadence agree with vanilla while stereo-pair or
  new-submission cadence is lower and repeated presenter submissions rise.

Jump sampling is driven by changes in native game time on a dedicated read-only
JNI observer, not render frames. The exact-build `ODEWalk` state and
`IsJumping` transition identify the physical jump interval. `PerformJump` and
successful `ODEWalk_Jump` are reported as inferred at the accepted
ground-to-air transition; the diagnostic deliberately does not detour or call
either mutating method. Requested normal jump height uses side-effect-free
`PropGetJumpHeight - ODEWalk_GetStairHeight`; `GetJumpHeight` is not called
because it can consume the one-shot big-jump flag.

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
| Locomotion divergence instrumentation | host-tested | native-update trace, input/readback/second-render toggles and phase summarizer pass host tests; physical comparison is pending |
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
