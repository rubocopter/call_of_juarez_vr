# Validation

Validation states are intentionally distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Builds, CTest, synthetic fixtures and static inspection do not promote physical gates. Acceptance below is limited to the exercised original Steam Call of Juarez (2006), PS VR2 and SteamVR contexts; it is not whole-campaign or public-release support.

## Current feature state

| Boundary | Actual state and limits |
| --- | --- |
| Native D3D9 stereo and tracked head | Headset-validated in exercised scenes: native per-eye rendering, 6DOF, recenter and room-scale movement. Broader transitions and device recovery remain open. |
| Locomotion | Native analog shaping, head-relative movement, jump parity, physical crouch and corrected 45-degree snap direction accepted in exercised contexts. |
| Body and hands | Independent tracked native hands with original torso/legs are active. Connected arms/full-body IK are not the accepted presentation. Skeletal fingers have partial live evidence; authored rest/fist retargeting is host-tested. |
| Weapons | Exercised right-hand pistol firing, visible muzzle/impact effects, water and destructible bottles accepted. Left-hand, dual and special weapons need separate coverage. |
| Menus | Automatic controller pointer, same-hand trigger selection, Cross accept, Circle back and pause accepted in exercised menus, including confirmation and mouse coexistence. |
| Sense controls | Direct controls, equipment wheel confirmation, snap rearm, haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted in exercised available contexts. Horse unavailable; kick inconclusive. |
| Gameplay HUD | Encountered health/ammo, wrist compass and subtitles accepted. Full graphical native HUD capture remains planned. |
| Death presentation | Corrected failure screen visibility accepted by operator. Retry/back and held-control recovery remain separate checks. |
| Interaction | Pickup/carry/put-down and drawers accepted through the previous HMD reference. Permanent cyan ring rejected for comfort. Latest left-controller L1 route is host-tested, awaiting headset acceptance. |
| Reload | Hybrid gesture starts native Peacemaker reload in exercised context. Cartridge-paced continuation physically rejected; corrected carrying/animation distinction is host-tested only. Full cylinder manipulation remains planned. |
| Protected-target warning | Original red warning visible on monitor, absent in visor. Red aiming-cross alternative implemented and host-tested; visor acceptance pending. |
| Whip | Exercised controls and approximate reticle alignment accepted. Broader reach, attachment, climbing and release remain open. |
| Loading/save | Exercised narrated transition after reversible LAA correction and save/load with optional thumbnail workaround accepted. Broad campaign loading and allocation pressure remain unresolved. |
| Transport | Primary D3D9Ex GPU sharing and bounded normal shutdown accepted. Sustained pacing, reset and abnormal shutdown require broader coverage. Steam recording-associated microskips remain unresolved. |
| Other platforms | OpenXR experimental. D3D10 and other games planned; neither is the active support target. |

## Current physical gate

The current candidate combines left-hand L1 selection and the cartridge-paced native recovery correction. Both changes are host-tested, not headset-validated. A new report must distinguish them from previously accepted controls.

- Aim the left Sense at a drawer's usable part and press L1. Turning the head must not retarget interaction. The cyan reference appears only while L1 is held. Check opening, pickup, carry and put-down; losing hand tracking/publication must require an available release before another action.
- Reload a partially empty Peacemaker/Frontier after one waist pickup, using fresh support-trigger presses/releases near the gun after each native cycle. Check cartridge replenishment, exactly one round per accepted cycle, reserve accounting and no support-trigger shot. This gate remains rejected until the corrected candidate passes.
- After death/failure, Back/retry must restore native gameplay cleanly. Held controls must remain suppressed until release; underfloor tracked presentation or continued dead-player interaction rejects recovery.
- Check the red aiming cross on a native protected target. This is a low-priority visor check, independent of ordinary aiming acceptance.

## Hybrid motion reload acceptance

The hybrid gesture is **headset-validated for the exercised Peacemaker context**
by operator report: the corrected gesture successfully starts the original
reload. Mirrored hands, cancellation, reserve limits and the other exact pistol
classes remain separate gates. This report does not validate cartridge-paced
loading or physical cylinder manipulation.
With a recognized Frontier 1878 Regular, Schofield A/B or Peacemaker pistol in one hand,
the other genuinely empty, on foot and in ordinary firing mode:

- Fire one round and wait for native shot recovery; retain ammunition in reserve.
  An entirely empty gun may already auto-reload through the native game path.
- Bring the empty hand to its side of the waist with its trigger released.
  Press and hold that hand's trigger, bring it near the armed hand, then release.
  The initial zone is approximately 55 cm below the HMD, 20 cm to the empty-hand
  side and 10 cm forward, with 20 cm radius. Release within 14 cm of the armed
  grip after at least 20 cm of hand travel and 100 ms; finish within five seconds.
  These are gesture UX defaults, not native physics or a measured body fit.
- Verify one native reload, actual ammunition/reserve changes, original reload
  animation ownership and recovery of tracked hands. No trigger shot, duplicated
  reload or virtual cartridge insertion is expected from the empty hand.
- Release away from the weapon to cancel. Repeat interruption by pause (including
  keyboard Escape), dashboard, wheel, recenter, tracking loss, switching/equipping
  and Action. No interrupted gesture may replay after recovery or reload a newly
  selected weapon. A trigger that acquires a weapon while still held must remain
  suppressed until an actual available release.
- Test mirrored hands where the native inventory permits, Square fallback,
  full/empty reserve and native automatic reload. Unsupported weapons, dual guns,
  alternate firing states, carried objects and mounted/unknown owners must retain
  their original controls without starting the gesture.

Assess zone reach/readability, accidental activation, control coexistence and
frame pacing. The hybrid intentionally leaves transfer timing to the game;
full manual per-round/cylinder interaction remains planned. See
[reload ownership](research/COJ_RELOAD_OWNERSHIP.md).

## Cartridge-paced reload acceptance

The installed cartridge-paced route is **physically rejected for continuation**:
one round loads, but the support-hand cartridge does not replenish. The updated
native-recovery reader now distinguishes two-hand reload animation occupancy
from actual carrying through a separate pure predicate. That correction and
the solid cartridge remain pending a fresh combined physical gate. The exercised hybrid Peacemaker
acceptance does not accept this new native-loop change.
With a single Peacemaker or Frontier 1878 Regular, empty support hand, on foot,
ordinary state and ammunition in reserve:

- Fire at least two rounds, then perform one waist pickup and release near the
  armed grip. A small brass/copper cartridge marker should follow the empty hand
  during the held journey and disappear on release or cancellation.
- Wait for the original reload begin/body/end cycle. Exactly one round should
  enter the gun, with exactly one round removed from reserve. The gun must stop
  reloading while still partially empty. If the same gun can still continue,
  another cartridge marker should reappear in the same empty support hand only
  after that native reload interval completes. A fresh trigger press claims it;
  release near the gun requests one further round. A miss keeps that cartridge
  available for another fresh press/release attempt. Holding the trigger through
  replenishment or repeatedly releasing cannot queue additional rounds.
- Release away from the gun, pause, dashboard, recenter or switch during pickup.
  No cartridge or reload request may return after the interruption. If the native
  reload was already accepted, its single-round completion remains native-owned;
  cancellation must not turn it into a full reload loop.
- Check full gun, zero reserve, weapon replacement, death, quicksave/load and
  mirrored hands separately. Multiplayer is excluded from this route. Square,
  native automatic reload and Schofield remain ordinary native reload paths.

This candidate uses the established grip proximity gesture and original reload
animation. The cartridge is a captured stereo solid with fourteen surfaces,
controller orientation, brass/copper shading and per-eye self-occlusion. It has no
scene depth occlusion; its visibility and comfort require headset testing. It does not
provide a measured chamber/loading-port socket, gate opening, spent-case ejection
or independent occupied chambers. Those model/pose boundaries and save/load
recovery remain open before full physical cylinder loading can be accepted.

The explicit insertion policy is host-tested separately: the captured cartridge
tip must approach an adapter-authored socket from behind, stay aligned, enter
while held and release inside. Discontinuous tip/socket motion cancels the claim.
The active game adapter does not enable that mode yet. Named displayed drum/gate
frame observation and six measured chamber-mouth transforms are host-tested
observations only; they do not establish gate clearance or occupied chambers.
`manual_reload_geometry` records sampled current mapped frames in centimetres
with `interaction_enabled=false`. A persistent open/load/close native session
remains unresolved and is not an additional requested operator test.

## Revised Sense control acceptance

The [default Sense table](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls) is the single binding reference. Exercised direct buttons, Triangle wheel release confirmation, haptic highlights, snap rearm, on-foot crouch and ordinary R3 Focus have operator acceptance. Create held for 800 ms recentres; a short press opens objectives, with operator acceptance.

Remaining coverage: mounted transitions/gallop when a horse is available; kick once campaign permissions allow it; newly acquired weapons, bow/scoped contexts and alternate HUD rows. Unavailable or inconclusive actions are pending, not failed. Check that wheel/menu/dashboard ownership suppresses gameplay and recovery requires release without replay.

## Automatic menu-pointer acceptance

The pointer appears automatically in native menus without L1/R1. Use the same hand's L2/R2 to select; verify Cross, Circle, Options, confirmation dialogs and mouse coexistence. These exercised cases are accepted. Loading, dashboard and resource replacement recovery require broader physical coverage with no stale click or haptic replay.

## Combined menu and gameplay regression

After each change affecting ownership, exercise menu -> gameplay -> wheel -> pause -> dashboard -> gameplay, plus death/retry and save/load. Hold mapped controls across transitions: no delayed fire, interaction, reload, snap or utility action may escape on recovery. Native permission and exact-build admission remain authoritative.

## Production D3D9Ex startup/reset gate

### Bounded transport acceptance — passed

The exercised primary D3D9Ex GPU transport and normal shutdown passed their bounded physical gate. This does not accept arbitrary device resets, abnormal shutdown, sustained pacing or every campaign transition. The classic shared-texture environmental test can be skipped when the host cannot supply its device capability; that skip is not a pass for that boundary.

## Closed locomotion diagnosis

Native per-axis analog shaping and actions 4–7 remain the accepted boundary. Measured vanilla movement/jump parity closed the earlier locomotion diagnosis; no speculative physics retuning is pending. Camera/body writes must retain scoped restoration.

## PC mechanics and gameplay HUD completeness

Encountered HUD rows and ordinary controls are accepted only for observed content. Horse, broader weapons, climbing, duel/special modes and all campaign-specific indicators still require coverage. Full graphical HUD capture and full physical reload are future work, not hidden completed features.

## Image-quality profile acceptance

Use the repository's prepared profile for comparable headset checks. Record rendering/readability, aiming effects and motion stability separately. Steam recording off produced accepted head-turn stability; recording-associated microskips remain unresolved. Higher resolution or successful startup alone does not establish sustained performance.

## Evidence policy

Use the canonical reversible workflow:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart
# Start SteamVR and Call of Juarez manually; exercise the relevant gates.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

Every physical run needs a fresh local identity and source/build/deployment correlation. Never launch or terminate the game or SteamVR automatically. Keep active staging, original backups and deployment/video recovery journals until finish completes. Host recovery checks cover backup identity, interrupted journals and finish retries; they do not establish headset acceptance.

Store run IDs, logs, videos, raw telemetry and handoffs under ignored `work/`. Once conclusions are durable in source/tests/research and validation, discard consumed clips, extracted frames, duplicate binaries, obsolete frozen packages and SDK extraction/download caches. Preserve necessary unresolved inputs and concise findings. Versioned documentation records contracts and acceptance, not debugging chronology.
