# Validation

Validation states are distinct:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

Builds, CTest, synthetic fixtures and static inspection do not promote physical
gates. Acceptance here is limited to exercised original Steam Call of Juarez
(2006), PS VR2 Sense and SteamVR contexts. It is not whole-campaign or public
release support. This document owns acceptance; [Architecture](ARCHITECTURE.md)
owns contracts and [Roadmap](ROADMAP.md) owns priorities.

## Current feature state

| Boundary | Confirmed state | Remaining limits |
| --- | --- | --- |
| Native D3D9 stereo and head tracking | Headset-validated native per-eye rendering, 6DOF, recenter and room-scale movement in exercised scenes. | Broad loading/device transitions and sustained pacing. |
| Locomotion | Exercised native analog movement, head-relative direction, jump parity, physical crouch and corrected snap turning accepted. | Mounted contexts and campaign-specific movement. |
| Body and hands | Independent tracked native hands/weapons with original torso/legs accepted in exercised contexts; head and connected arms hidden. | Left/dual/special weapons, broader finger and recovery coverage. Connected-arm IK is a diagnostic, not the accepted presentation. |
| Weapons and whip | Right-hand pistol firing, muzzle/impact effects, water and destructible bottles accepted. Exercised whip use and approximate reticle alignment accepted. | Broader weapons, ballistic birth vectors, whip reach/climbing and special optics. |
| Menus and Sense controls | Automatic hand pointer, same-hand trigger selection, Cross/Circle, pause, confirmation dialogs and mouse coexistence accepted. Wheel selection/haptics, on-foot crouch, ordinary Focus and Create tap/hold accepted where exercised. | Dashboard/loading/held-input recovery, mounted routing and unavailable mechanics. Kick is inconclusive. |
| HUD and subtitles | Encountered native health/ammo, compass, equipment wheel, essential text and subtitles accepted. Optional status rows accepted only where encountered. | Horse and special HUD contexts, newly translated prompts, complete graphical HUD capture. |
| Interaction | Pickup/carry/put-down and drawers accepted through the previous HMD reference. | Latest left-Sense L1 ray and held-only cyan reference are host-tested, awaiting physical acceptance. The permanent HMD ring was rejected for comfort. |
| Persistent manual reload | Opt-in `-ManualReload` is scoped headset-validated: Square opens/closes without auto-loading, hands stay tracked, successive free-trigger cartridges load, insertion-only haptics work, weapon/menu recovery works where exercised. Native unit transfers and restoration have correlated live evidence. | Exact port clearance/chamber insertion, tracking/save/load/death/disable recovery, held-control edges, mirrored/Frontier use and prolonged comfort. |
| Cartridge visual and pinch | Textured CC0 .357 visibility has scoped headset acceptance. Revised .7/.7 pinch and skin-derived anchor have host tests and bounded live delivery evidence. | Latest close-up still appears superimposed rather than convincingly held. Contact, axis, scale, occlusion and comfort are unaccepted; scene depth and native lighting are unavailable. |
| Death and save/load | Corrected failure-screen visibility accepted. Exercised narrated loading after reversible LAA correction and save/load with the thumbnail workaround accepted. | Retry/back/held-input recovery, manual-session save roundtrip and broad allocation pressure. |
| Transport and shutdown | Primary D3D9Ex GPU sharing and normal shutdown have bounded headset/live acceptance. | Sustained pacing, abnormal exit, arbitrary reset/device loss and Steam recording-associated microskips. |
| Protected-target warning | Red aiming-cross alternative implemented and host-tested. | Visor visibility/target transitions; original warning was absent in visor. |
| Other platforms | OpenXR experimental; D3D10 and additional games planned. | No physical support claim outside the active D3D9/OpenVR target. |

## Current physical gate

Do not repeat an accepted baseline unless a change affects it. Batch related
checks into one meaningful candidate, recording unavailable cases as pending.

- **Interaction:** point the left Sense at a drawer's usable part and press L1.
  Head turning must not retarget it. The cyan reference must exist only while
  L1 is held. Check opening, pickup/carry/put-down and release after tracking loss.
- **Cartridge grip:** the current anchor has already been observed in runtime;
  repeating it solely to reconfirm visibility adds no evidence. After a material
  contact/axis/occlusion correction, combine empty-hand waiting, trigger pickup,
  slow wrist roll, release away, successive insertion, closing/firing and
  menu/weapon recovery. Include both hand assignments when available.
- **Manual mechanics:** compare the actual visible loading port in natural
  `READY` and sustained `MANUAL_LOAD` at a comparable angle. Measured gate
  rotation alone does not prove clearance or an insertable socket.
- **Recovery:** qualify tracking loss/recovery, held controls across ownership
  changes, death/retry, save/load and disable during loading when available.
  Require no stale round, delayed action, extra transfer or permanent lock.
- **Campaign coverage:** mounted controls, broader weapons/special optics, whip
  climbing and optional HUD content need eligible scenes. The protected-target
  red aiming cross remains a separate low-priority check.

## Persistent manual reload candidate

Use one eligible single Peacemaker/Frontier with an empty support hand, missing
rounds and reserve. Prepare with `-BodyIkAtStart -LargeAddressAware -ManualReload`.
The support **trigger**, not Grip, carries/releases the cartridge.

The exercised flow is `READY -> OPENING -> MANUAL_LOAD -> CLOSING -> READY`.
Fresh left Square prepares; native opening enters a supervised wait without
automatic transfer. Weapon and independent hands retain controller ownership.
Pick up at waist with the free trigger, carry to the existing weapon-grip zone
and release. Each accepted insertion calls native `WeaponReload` once; native
inventory/capacity/reserve remain authoritative. A fresh pickup/claim can repeat
the process. Square again or the armed trigger closes early; that closing trigger
press must not fire. Release and press again to shoot. Full/no reserve closes
without another round. Unsupported positively identified contexts retain native
reload; observation failures do not authorize that fallback.

Operator acceptance covers Square preparation/closure, no automatic loading,
tracked hands, successive loading, insertion-only haptics and the exercised
weapon/menu interruptions. Complete inventoried right-hand Peacemaker captures
correlate accepted insertions with `loaded +1 / reserve -1`, sustained waiting
beyond the original bounded probe interval, coherent closing and scoped recovery.
The offline analyzer found no unexplained transfer in the reviewed captures.
These findings do not accept every weapon, interruption or comfort condition.

The active insertion zone is the existing approximately 14 cm grip-proximity
zone. It is not an individual chamber or measured loading port. The read-only
root/barrel/gate/drum observer has bounded live evidence of changed native gate
pose and independent drum motion. The six-mouth pivot-ranking diagnostic has
bounded live observations, but the gate element origin is an unverified pivot:
`gate_open_confirmed=false` and `socket_admission=false` remain mandatory.
No independent gate/drum writer, spent-case simulation or chamber occupancy is
implemented. See [exact reload research](research/COJ_RELOAD_OWNERSHIP.md).

Optional acceptance haptics require coherent native unit readbacks and fresh
capture/tracking/gameplay ownership. Host tests cover duplicate/stale captures,
replacement, release barriers and output faults; live submission records and
operator feedback accept insertion-only delivery in the exercised context.
Zone-entry feedback and insertion-specific sound remain pending.

`manual_reload_pinch` and `manual_reload_cartridge` have bounded applied/visible
and idle/hidden observations. The optional `manual_reload_finger_reference`
reader has bounded live snapshots of native/displayed distal fingers; its
`weapon_id=0` diagnostic field cannot independently prove weapon ownership.
Joint observations do not measure skin contact. The current compositor lacks
scene-depth occlusion; improved anchor position alone does not qualify the grip.

## Hybrid motion reload acceptance

This is the retained legacy path, separate from `-ManualReload`. A deliberate
free-trigger waist-to-gun journey requests ordinary native reload. Corrected
initial admission has scoped operator acceptance on the exercised Peacemaker.
Native animation still owns the original two-hand presentation.

## Cartridge-paced reload acceptance

The legacy Peacemaker/Frontier extension limits each natural completion to one
native round. Corrected recovery has bounded live evidence of replenishment and
unit count/reserve changes. It still takes the hands during each native cycle;
it is not the persistent manual flow and has no accepted physical chamber UX.
Keep it as a fallback/compatibility contract, not the next physical development
target. Schofield retains whole-clip native behavior.

## Bounded native wait/zero-transfer probe

The explicitly selected `-ReloadWaitProbe` derivative is host-tested and bounded
live-tested on a right-hand Peacemaker. It uses a roughly 1.5-native-clock wait,
grants zero rounds and closes through native callbacks; Square stays conventional
and presentation remains native-owned. It cannot combine with `-ManualReload`.
It established the logical veto/cancel seam used by the later manual candidate,
not mechanical clearance or persistent tracked-hand acceptance. No repeat probe
is needed merely to reconfirm that discovery.

## Revised Sense control acceptance

The [default Sense table](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)
is the sole binding reference. Exercised direct controls, wheel confirmation,
haptic highlights, snap rearm, on-foot crouch, ordinary R3 Focus and Create
timing have scoped operator acceptance. Mounted contexts, kick eligibility,
broader weapons and bow/scoped optics retain independent gates.

## Automatic menu-pointer acceptance

The automatic pointer uses same-hand L2/R2 selection; Cross accepts, Circle backs
out and Options pauses. Exercised ordinary menus, confirmation dialogs and mouse
coexistence are accepted. Dashboard, loading, resource replacement and held-click
recovery require broader coverage without stale clicks or haptic replay.

## Combined menu and gameplay regression

Changes to ownership need menu -> gameplay -> wheel -> pause -> dashboard ->
gameplay coverage plus available death/retry and save/load. Hold mapped controls
across transitions: no delayed fire, interaction, reload, snap or utility action
may escape on recovery. Native permissions remain final.

## Production D3D9Ex startup/reset gate

Primary GPU transport and normal shutdown passed their bounded physical gate.
Arbitrary reset, abnormal shutdown, sustained pacing and campaign transitions
remain open. A skipped classic shared-texture environmental test is neither a
pass nor physical validation. Transient host D3D9 CreateDevice failures have also
been observed; their cause has not been established.

## Closed locomotion diagnosis

Native per-axis shaping and float actions 4–7 remain the accepted movement seam.
Measured vanilla movement/jump parity closed the earlier diagnosis. Preserve
camera/body restoration; no speculative physics retuning is pending.

## PC mechanics and gameplay HUD completeness

Accept only observed content. Horse, broader weapons, climbing, duel/special
modes and campaign indicators need coverage. Full graphical HUD capture and
complete physical cylinder reload are planned.

## Image-quality profile acceptance

Canonical preparation preserves selected resolution and stages FSAA 0 unless
requested otherwise. Judge readability, aiming and motion stability separately.
Steam recording off produced accepted head-turn stability; recording-associated
microskips and sustained performance remain unresolved.

## Evidence policy

Use the canonical reversible workflow:

```powershell
pwsh -File tools/vr_test.ps1 prepare -GameDirectory "C:\path\to\Call of Juarez" -BodyIkAtStart -LargeAddressAware -ManualReload
# Start SteamVR and Call of Juarez manually; exercise relevant gates.
# Close the game normally.
pwsh -File tools/vr_test.ps1 finish
```

Each physical run needs fresh source/build/deployment/run identity. Never launch
or terminate the game or SteamVR automatically. Preserve staging/recovery
journals and original backups until `finish` completes. A historical run manifest
marked `staged` is not evidence of an active deployment; inspect canonical status
and the current journal. Host recovery tests do not establish headset acceptance.

After `finish`, inspect collected reload evidence without modifying it:

```powershell
python tools/analyze_coj_manual_reload.py "<collected run directory>" --output "work/manual-reload-analysis.json"
```

Use an output outside the evidence directory. The analyzer verifies inventoried
hashes, deployment coverage and ordered native markers; it separates correlated
unit readbacks from rejection/inconclusive observations. It never accepts a
physical gate or replaces the canonical verifier.

Version durable conclusions and acceptance limits, not run chronology. Keep only
compact unresolved inputs and necessary local provenance under ignored `work/`.
Once consumed, remove duplicate videos/frames/logs/dumps, frozen game archives,
obsolete builds and SDK extraction/download caches. Keep configured dependency
roots, the pinned offline bootstrap-test archive and useful current build output;
these caches are not shipped. Never remove
active recovery data or user assets as evidence cleanup.
