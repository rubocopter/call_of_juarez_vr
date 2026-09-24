# Technical audit — current baseline

Last refreshed: 2026-09-24.

This document describes the current engineering baseline and the findings that still matter. Historical investigation details were intentionally removed once their conclusions became enforced contracts or retained evidence.

## Evidence policy

A claim advances only as far as its evidence:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Exact game integration is SHA-256 gated. One staged run ID belongs to one game process. Physical promotion requires source/build/deployment/run correlation and the acceptance gesture for that gate.

## Current baseline

The repository has moved beyond the original blank-headset/bootstrap problem. Current verified capabilities include:

- exact Call of Juarez camera -> view/projection -> ChromeEngine renderer ownership;
- complete left/right native D3D9 game renders and SteamVR submission;
- correct 100 game-units/metre stereo baseline;
- HMD orientation and positional camera offset;
- explicit render-pose submission that removed the previous head-turn pull/snap-back artifact;
- flat-theater startup/loading presentation and return to native stereo;
- left-Sense Create recenter and exact ±45° snap turn;
- live campaign-player discovery and tracked Sense poses;
- exact visible arm mutation/restoration against the live skeleton;
- physically validated local head/hair suppression for the exercised Ray/Billy path.

Subsequent physical cadence/locomotion comparison closes the earlier locomotion
ambiguity. Vanilla normal/walk movement measured `503.056/256.109 cm/s` and VR
measured approximately `489-499/248 cm/s`; vanilla jump measured about
`58.7-60.8 cm` over `0.77-0.78 s`, while VR input-off measured `62.623 cm` over
`0.781 s`. Native locomotion and jump are therefore not reduced. The decisive
performance comparison is `139.949 Hz` vanilla, `83.473 Hz` old VR full and
`138.117 Hz` with both complete eye renders active but capture readback/copy
disabled. The perceived locomotion slowdown is presentation stutter caused by
the old transport, not a movement/input/physics defect.

The current source contains a D3D9Ex shared-texture replacement that is
**host-tested only**. Win32 Release passes **30 tests + 1 expected classic-D3D9
shared-texture capability SKIP**. Host success cannot prove exact-game D3D9Ex
stability, headset image correctness, cadence, UI, anatomy, aiming or shutdown.

## Audit findings

| Finding | Current status | Remaining concern |
| --- | --- | --- |
| Provenance was too weak for promotion | Resolved baseline | Keep exact source/build/deploy/run correlation mandatory |
| Vtable/hook changes were not transactionally safe | Resolved baseline | Preserve foreign-hook detection and restoration checks |
| D3D9 factory/device identity could split or bypass hooks | Resolved baseline | Continue generation/device identity telemetry |
| Capture and OpenVR presentation were coupled in game callbacks | Resolved baseline | Maintain mailbox/presenter separation |
| Renderer resources were not tied strongly enough to device/generation | Resolved baseline | Reset/new-device recovery remains a required host test |
| Host tests did not prove the staged pipeline | Resolved baseline | `prepare` must build/test/provenance-bind the staged candidate |
| Multiple processes could reuse one run ID | Resolved in workflow | Historical multiprocess runs remain diagnostic only |
| Deployment restoration was happy-path oriented | Resolved baseline | `finish` must restore candidate and video settings transactionally |
| Source architecture and build dependencies drifted | Resolved baseline | Keep exact-game code in the game adapter |
| Runtime/lifetime recovery was incomplete | Partially open | Recent physical runs still report incomplete inner presenter shutdown |
| Neutral camera/math contracts were ambiguous | Resolved baseline | Keep right/up/forward source basis and exact unit conversion |
| Documentation accumulated chronological state and stale objectives | Addressed by current cleanup | Keep docs state-based; retain compact evidence metadata rather than narratives |

## Active technical risks

### UI ownership

The dual-owner Java+Win32 route could select but was chaotic. The next candidate removed Java ownership and left only `SetCursorPos` + absolute `SendInput` + `WM_MOUSEMOVE`; run `20260921T192639Z-1d70905cb4f8` still found the pointer uncontrollable and required the physical mouse. Pointer instability therefore is not explained solely by competing owners. Treat the current projection/input ownership model as unresolved rather than stacking another cursor route on top.

Circle used normal Escape only while a current menu UI existed. Native-stereo gameplay had no current UI and therefore no usable exit path. Current host source retains `GameUserInterface.CallOnInputKeyGlobal(1, ...)` for existing UI and falls back to `LawmanGame.sm_cActiveGameModule.OnInputKey(1, ...)` so the shipped module creates the pause menu. Cross remains accept, L2/R2 remain ray-select, and `GameUILoading.OnInputKey(IZC)V` owns the blocking post-load continue path.

The same run produced 202 `java.lang.NoSuchFieldError: m_cHintManager` exceptions from direct inherited-field lookup in the paused-hint helper. Current source uses the shipped `LawmanModule.GetHintManager()` accessor before `HintManager.DisableCurrentHint`, avoiding that known exception route. These UI changes are host-tested only.

This remains an open UI architecture/debugging problem rather than a host-only gate.

### Subtitles

Retained diagnostic run `20260920T235112Z-6b2d91cda4a5` recorded 376 subtitle states with `Settings.bSubtitles=false`. `DialogSubtitle.Show(String)` exits when that setting is false, so that run explains its subtitle absence without evidence of a layering/capture defect. Enable subtitles through the shipped setting before judging presentation; do not force-enable or relocate them in the adapter.

### D3D9 presentation transport

The old complete-process samples measured approximately 7-7.5 ms median,
9-9.5 ms p95 and 10-12 ms maximum readback/copy cost at 1920x1080 per eye. A
120 Hz frame is about 8.33 ms, so that boundary could consume the whole budget.
The readback-off physical run restored approximately vanilla cadence with both
eye renders still active, proving causality.

The candidate now requests a D3D9Ex device, captures each eye with `StretchRect`
into separate shared DEFAULT-pool render-target textures, publishes handles plus
the exact pose through the mailbox, opens them on the presenter's matching D3D11
adapter and queues `CopyResource` into stable OpenVR textures. During normal
presentation D3D9 and D3D11 EVENT queries are polled without waiting; three
producer slots plus a consumer lease prevent overwrite and preserve
left/right/pose pairing. Presenter shutdown uses one bounded D3D11 completion
barrier only when copies remain pending, and retains leases on drain failure
rather than releasing producer resources early. The host-tested GPU candidate
contains no `GetRenderTargetData`, SYSTEMMEM surface, CPU pixel copy,
diagnostic eye hash or `UpdateSubresource`.

Classic D3D9 cannot supply the DXGI-shareable DEFAULT resources required by
this interop, so the full GPU path requires D3D9Ex. The factory falls back to
classic device creation if `CreateDeviceEx` fails; capture then reports and uses
the old deferred CPU route. That fallback is functional but fails the current
performance gate. Immediate flat-theater capture still uses transient
`GetRenderTargetData` because it owns CPU-side UI/pointer composition and reset
safety; native stereo does not use it while D3D9Ex sharing is active.

The exact-game D3D9Ex candidate remains physically blocked. Two candidates
created an Ex device and completed three Presents, then stopped before videos
or shared transport. The second exposed a factory COM identity mismatch and
the current source hooks `GetDirect3D` to return the game's factory; that fix
is host-tested but not physically verified.

A separate classic-D3D9 observation run loaded DX9 gameplay and logged 4,311
rendered frames. It recorded two successful `D3DPOOL_MANAGED` creations, a 2D
texture and a cube texture, proving that verbatim device substitution is
semantically incompatible. However, only three resource creations total were
captured. The observed 66.7% MANAGED share is not representative, and the full
resource types and emulation requirements remain unknown. The immediate gate
is a complete resource census and evidence for any managed-resource emulation
before another D3D9Ex transport attempt. The run report and raw capture remain
under ignored `work/evidence/d3d9_pool_probe/`.

OFXR-Bridge is not a direct fix for this path. It is an experimental OpenXR
optical-flow layer and cannot replace D3D9Ex/OpenVR resource interop or recover
source detail that was never rendered.

### Recenter level reference

Earlier evidence visibly showed a rolled horizon. The old recenter tracker captured the complete HMD basis, so pitch/roll present during recenter could become part of relative tracking space. Current host source derives the recenter reference from gravity plus HMD yaw only; a tilted-recenter regression test covers this boundary.

### Body anatomy

The exact visible writer is proven live, but current safety ownership is not usable. In the latest run, 43/128 sampled arm updates denied VR writes and 32 were reach-unsafe; 80 samples rejected hand residuals. The user sees this as the arms repeatedly snapping back to the default game pose while moving the Sense controllers. The safety guard is doing its fail-closed job, but the remaining anatomy problem is now continuity/ownership as much as raw rotation limits.

### Weapon origin and direction

Static bytecode and host tests establish distinct direction/visual-origin/ballistic-origin ownership. `InputDigital.Translate` selects the hand/fire state and the actual native attack later follows `OnHandStateStarted_Attack -> WeaponAttack -> Weapon.Attack`, where `GetFireOriginForWeapon` reads `Being.m_vLookFromPoint`. Full method-order inspection further establishes that `ArmedPlayerBeing.OnBeingsFrame()` executes `UpdateHandStates` before `OnPostBeingsUpdate()` later executes `UpdateLookAndAimDirs` and `UpdateLookAndAimPoints`; the native post-update recomputation therefore cannot erase the published fire transition before `WeaponAttack`. Translation failure rolls back the captured native value.

The newest run resolves the simple Sense-axis question: 120 grip-to-tip comparisons put local `-Z` at `0.939388..0.939389`, and 21 fire transitions successfully publish varying controller directions/origins. Shots are still visibly wrong, so a guessed controller-axis offset is no longer justified. Reintroduce a temporary controller-tip ray only as a visual diagnostic, then compare that ray with the visible weapon/barrel and trace the mismatch through weapon visual transform, muzzle origin and ballistic ownership. Production shots must originate from the visible barrel/muzzle.

### Physical crouch

The previous full-XYZ pelvis error is now technically corrected: 64 sampled room-scale writes in the latest run all had zero vertical world offset and restored after both eyes. The new gap is animation ownership. Physical horizontal HMD displacement moves the visual body anchor but does not trigger the walking animation expected from stick movement. Add this as a distinct body requirement while keeping native actor position, grounding and collision authoritative.

### Presenter finalization

Several earlier runs achieved clean outer runtime stop and `run_end`, but recent body/playability runs still report `shutdown_complete=false` for the inner presenter. Do not collapse these signals into a single "clean shutdown" claim.

## Exact game findings that remain authoritative

- Full render-view wrapper: `0x00030FB0`; core `0x00030E00` alone does not produce a valid second-eye render.
- Camera source layout: right/up/forward/position with paired source/view state.
- World/player units are centimetres; XR tracking is metres.
- Call of Juarez `InputAnalog` uses 0.04 per-axis shaping for the native movement route.
- `RotateElementWithChildren(ILVector;F)V` composes element-local rotation and updates descendants/attachments.
- FORETWIST is effectively a sibling of forearm for the observed model; hand follows forearm and ignores FORETWIST roll.
- Ordinary local ballistic origin passes through `Being.m_vLookFromPoint`; per-hand visualization uses `m_avAimFromPoint`; per-hand direction uses the native look-direction array.

See `docs/research/COJ_CAMERA_PATH.md` and `docs/research/COJ_ARM_SKINNING_AND_AIM.md` for the compact research record.

## Current acceptance boundary

The repository is no longer blocked on discovering whether native stereo or
native locomotion is correct. Its immediate gate is exact-game/headset
validation of the D3D9Ex GPU-resident transport. Remaining product work is
controller UI, subtitles, first-person body ownership, weapon origin/direction,
arm/reload anatomy and lifecycle finalization.
