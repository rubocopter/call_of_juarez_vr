# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, D3D9
native stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Architecture](ARCHITECTURE.md) owns contracts; [Validation](VALIDATION.md) owns
acceptance. Implementation, host tests and publication do not establish physical
acceptance or supported hardware.

## Current gate

The published package remains `v0.1.0-prealpha.5`. Current post-.5 changes are host-tested; no new headset test has been declared. Candidate particulars remain under ignored `work/` and the canonical deployment journal.

Preserve scoped acceptance already recorded for native stereo, movement and
continuous body following, independent hands, manual revolver unit loading,
Remington bimanual grasp, exercised bow use, holsters/guides, native options,
duel posture, physical Hands combat and cinematic recovery. Manual 2804x2860 is
an accepted local PS VR2 reference, **not evidence for the untested candidate**.
Do not request repeats merely to reconfirm these accepted contexts.

The post-.5 batch implements ammunition assistance, binding-aware hints/coaching,
quick-panel removal and immersive skip input, mirrored rifle trigger protection,
indexed reload analysis, diagnostic opt-in cleanup, mounted reins/reference
corrections and manual reserve/JNI/close-hint hardening. These are host-tested;
their live presentation and recovery remain unqualified. Native Options > Game
owns preferences; the held-Options quick editor is retired.

## Development backlog

Priorities below distinguish remaining implementation from physical checks of
existing code. Continue independent engineering while eligible campaign scenes
are unavailable; collect related physical checks in one meaningful candidate.
No headset test is requested as part of documentation cleanup.

1. **Convincing cartridge contact.** Preserve native unit loading and the accepted
   cartridge/port visibility. Judge the host-tested displayed-finger tangent and
   native-confirmed thumb gesture before further changes. Offline overlap does
   not justify another arbitrary anchor offset. Qualify depth contents and hand
   coverage before implementing scene/hand occlusion.
2. **Measured loading mouth.** Establish a phase-bound port frame and clearance
   before connecting tip/trajectory insertion. The current 14 cm grip zone stays
   active; pivot rankings and drum phase observations do not admit a socket,
   chamber occupancy, ejection or an independent mechanical writer.
3. **Campaign correctness.** Observe the conditional secret-scroll render-level
   correction at a naturally encountered chest. Validate mounted reins and rider
   free look without changing accepted on-foot body following. The new immersive
   skip route is a separate pending input check; accepted cinematic stereo
   recovery remains closed in its exercised scope.
4. **Recovery and pacing.** Broaden held-input, ownership, tracking, save/death and
   device-loss coverage. Attribute recording microskips and community issue #5
   stutters before optimizing. Diagnostic gating fixes remove known unwanted
   work but establish no measured performance gain or crash resolution.
5. **Complete presentation.** Qualify native graphical HUD capture, remaining
   weapon geometry/optics and event-free physical-walk/turn leg animation.
   Bindings, static research and passive observers do not implement a physical
   mechanic.

### Weapon implementation inventory

Exact capacities, classes, reserve types and callback ownership belong in
[reload research](research/COJ_RELOAD_OWNERSHIP.md).

| Family | Current boundary | Remaining development/evidence |
| --- | --- | --- |
| Peacemaker / admitted Frontier1878 | Persistent native unit-loading route with scoped acceptance. | Contact/occlusion, precise mouth, sound, mechanics and broader interruption recovery. |
| Additional Frontier, Walker, Cogswell | Manual profiles host-tested; Cogswell uses rifle reserve. | Per-model physical unit transfer, geometry, ammunition appearance and recovery. Existing acceptance does not promote these extensions. |
| Winchester | Support and passive reload observer host-tested; Square reload remains native. | Correlate native phases, HUD/ammo/reserve and support motion, then qualify a port and implement a separate manual session. |
| Remington / SawOff | Remington grasp accepted; SawOff support host-tested. Two-index observation/classifier host-tested. | Index-specific single-shell transfer and per-barrel ownership before manual loading; untested fit, firing/cycling and recovery. |
| Scoped Winchester | Native fallback; excluded from support/manual admission. | Exact optic/class/ownership and separate support/reload integration. |
| Volcanic | Native fallback; static per-round contract identified. | Native phase/readback, hand/port qualification and own manual profile. |
| Derringer | Native whole-clip/two-index fallback; passive indexed observer. | Selective chamber transaction before representing one cartridge. |
| Lemant / LeMat | Native fallback; typed reserve-vector observer/classifier. | Selective index ownership and separate ammo/geometry. Native unit reload stops at the first eligible index, rifle first; it is not caller-selectable. |
| Schofield base/A/B | Native/hybrid whole-clip behavior. | Deliberate clip UX or proven selective transfer; outside persistent manual admission. |
| Bow | Physical grab/pull/release has scoped functional acceptance. | Mesh/string/arrow/reticle, haptics, concentration optics and interruption evidence. Preserve native trajectory, ammo and concentration lifecycle. |
| Hands / Bible / whip / dynamite / mounted weapons | Scoped Hands and Ray Bible/on-foot kick acceptance; other native routes retained. | Fist volume/guard/AI anticipation and exact permissions/contact for additional physical mechanics. |

### Other implementation inventory

| Area | Remaining work |
| --- | --- |
| Body | Event-free lower-body walk/turn sampling or complete reversible transaction. Native root/SetFrame also advances notifications/footsteps and cannot be a simple legs-only writer. |
| Holsters | Measured socket/grasp and body alignment; attached meshes remain future work. Accepted ergonomic zones/guides remain usable. |
| HUD / hints | Complete graphical capture and device/binding-aware visual qualification; current text and host-tested coaching retain native visibility/priority. |
| Compatibility | Real OneDrive hydration/synchronization, high-address native owners, broader allocation pressure, campaign and hardware coverage. |
| Deferred | DLC adaptation and bHaptics provider/game-event producers by operator priority; existing neutral haptic QA policy is host-tested only. |

## Parallel engineering tracks

Reload G1 precedes G2. G3–G5 can proceed independently. Rendering research is
separate from ordinary stereo and must preserve usable menus/controls on failure.
Independent resolution remains default-off and rejected; live structural binding
correlation and synthetic GPU tests do not qualify resizing or output retirement.
See [resolution research](research/COJ_VR_RENDER_RESOLUTION.md) and the
[independent-resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md).

## Next implementation gates

| Gate | Actual state | Next prerequisite / exit boundary |
| --- | --- | --- |
| G1 — cartridge contact and occlusion | Revised tangent/thumb and depth observer host-tested; convincing contact unaccepted; occlusion planned. | Judge contact/axis/scale/roll and native-confirmed gesture. Qualify exact per-eye depth contents, coverage, format, lifetime and pose/generation before renderer-owned occlusion. Preserve single native transfer and scoped recovery. |
| G2 — port and precise insertion | Port appearance accepted; pivots/phase are diagnostics; generic socket policy unwired. | Measured phase-bound mouth/clearance, then deliberate aligned release with one native loaded +1/reserve -1. Reject stale owners, closed gate, wrong approaches and misses. No chamber/ejection promotion. |
| G3 — complete graphical HUD | Text/countdown/notices/directions accepted where exercised; sprite/flush observer host-tested; graphical capture planned. | Qualify naturally scheduled batch/flush target and alpha/coverage. Observe once, never replay traversal for the second eye; preserve native visibility and both-eye lifecycle. |
| G4 — transport recovery and pacing | Normal transport/quit accepted in scope; independent consumer-query recovery host-tested. | Qualify native reset/device-loss and teardown/recreation when retirement proof fails. Retain uncertain leases, reject stale generations; measure sustained frame-age/recording tails. |
| G5 — persisted verdicts | Host-tested; rejection persistence live-tested through collection/restoration. | Exercise successful full-profile collection during a relevant candidate. Preserve input hashes, ordered run/build intervals and stale-input rejection. Collection cannot accept missing gestures or physical gates. |

## Next acceptance gates for implemented changes

Use [the pending checklist](VALIDATION.md#pending-headset-checks) as the single
physical checklist. It groups reload contact/feedback/reserve/recovery, hints and
pickup guides, left-hand interaction, mounted controls, immersive skip/loading
continuation, mirrored rifle triggers, HUD styling and global ownership checks.
Unavailable campaign scenes remain pending/inconclusive; do not split them into
repeated small sessions. These changes have no new physical acceptance.

## Stabilization baseline

Preserve SHA-256 exact-build rejection, native inventory/permissions, scoped
camera/body/weapon writes and transactional restoration. Production presentation
is independent hands with native torso/legs and hidden head/connected arms.
Connected-arm IK remains rejected diagnostics. Keep D3D9/OpenVR primary, probes
and independent eye bank off by default, and operator-controlled game/SteamVR
lifecycle. Active originals/journals are not disposable evidence caches.

## Milestone 1 — native stereo and HMD

Scoped stereo, tracking, recenter, room-scale and exercised cinematic recovery
are achieved. Broader device/loading transitions and sustained pacing remain.

## Milestone 2 — player body and comfort

Scoped native locomotion, jump, crouch, smooth/snap turn and continuous body
follow are achieved. Mounted correction/recovery is unqualified; physical-walk
animation remains planned.

## Milestone 3 — tracked hands and body IK

Independent hands/native body are active and accepted where exercised. Broaden
weapon/finger/recovery coverage; connected-arm reconstruction is not a production
requirement.

## Milestone 4 — controller UI and interactions

Scoped menus, native settings, wheel, HUD/text, combat, duel posture, bow,
holsters and manual revolver loading are accepted. Remaining mechanics and
visual/recovery gates are listed above, not implied complete by these milestones.

## Milestone 5 — additional renderers and games

OpenXR is experimental; D3D10, Bound in Blood and Gunslinger are future tracks.
Reuse neutral policy only; a second game must independently demonstrate a
Chrome Engine boundary before generalization.

## Release direction

Keep published .5 artifacts and verdicts unchanged. Future distributions retain
reversible installation, saved resolution/reload choices, local-only diagnostics,
original-asset exclusion, GPL-3.0-or-later corresponding source and notices.
Windows 10/11 x64 packaging retains the x86 game/runtime and PowerShell 5.1
launcher. No automatic updater is planned. Release notes own package history;
publication cannot promote hardware, campaign, rendering or recovery acceptance.
