# Exact CoJ PC controls and gameplay HUD

These findings apply to the reference Steam Call of Juarez (2006) PC build.
Executable SHA-256:
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`.
Original `code.pak` SHA-256:
`F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12`.

## Authoritative sources

The shipped official `Manual.pdf`, printed pages 10-14 (PDF pages 12-16),
describes PC defaults, HUD and character-specific mechanics. The shipped
`InputSettings.GetActionName`, `GetDefaultSetting`, `IsActionEnabled` and
`m_aSettingsForMenu` establish the executable action catalogue. The saved
`settings.dat` establishes user bindings; `Controller.scr` only records mouse
sensitivity in the inspected configuration. User bindings can differ from the
manual and are not a new game contract.

The current VR path is independently established by `GameplayInputState`,
`assets/openvr/actions.json`, the Sense binding, OpenVR action polling and
`BuildCoJGameplayActionValues`. Third-party Steam Input layouts are not PC
defaults or evidence that an action exists in this build.

## Complete native catalogue and present Sense coverage

There are 48 action slots. The ordinary controls menu enables 43: all except
8, 28, 36, 37 and 38. Developer-key mode can additionally enable 36 and 37.
An unused slot, hidden action or unassigned toggle must not be mistaken for a
missing mandatory campaign button. PC keys below are official defaults; rows
outside that manual are explicitly marked.

| ID | Native action / PC meaning | PC default | Current VR route |
| --- | --- | --- | --- |
| 0 | Look up | unassigned | tracked HMD; no digital look binding |
| 1 | Look down | unassigned | tracked HMD; no digital look binding |
| 2 | Turn left | unassigned | right-stick snap; native continuous look kept neutral |
| 3 | Turn right | unassigned | right-stick snap; native continuous look kept neutral |
| 4 | Forward | W | left stick |
| 5 | Backward | S | left stick |
| 6 | Strafe right | D | left stick |
| 7 | Strafe left | A | left stick |
| 8 | Show map, hidden from ordinary menu | not a documented PC control | absent; do not assign O to this slot |
| 9 | Left mouse / left-hand weapon action | left mouse | L2 |
| 10 | Right mouse / right-hand weapon action | right mouse | R2 |
| 11 | Jump | Space | right Cross |
| 12 | Lean left | Q | no native action binding; room-scale lean is camera translation |
| 13 | Lean right | E | no native action binding; room-scale lean is camera translation |
| 14 | Walk | Shift | no explicit walk action; analog speed is a separate contract |
| 15 | Toggle walk | unassigned | absent |
| 16 | Crouch | left Ctrl | right-stick click and physical crouch policy |
| 17 | Toggle crouch | unassigned | absent |
| 18 | Horse gallop / multiplayer run | Caps Lock | left-stick click, named `run` |
| 19 | Toggle horse gallop / run | unassigned | absent |
| 20 | Toggle alternate fire | Z | absent |
| 21 | Select rifle / long weapon | 3 | no direct selection |
| 22 | Select right pistol | 2 | no direct selection |
| 23 | Select left pistol | 1 | no direct selection |
| 24 | Select dynamite | 4 | no direct selection |
| 25 | Select Bible (Ray) / whip (Billy) | 5 (manual labels whip) | no direct selection |
| 26 | Select bow | 6 | no direct selection |
| 27 | Discard weapon | Backspace | absent |
| 28 | Switch weapons, hidden from ordinary menu | not a documented PC control | absent; distinct from next/previous |
| 29 | Hands / put weapons away | 0 | absent |
| 30 | Contextual action / execute active trigger | F | left Triangle, `interact` |
| 31 | Reload | R | left Square |
| 32 | Show objectives | O | absent |
| 33 | Show dialogue and hint logs | L | absent |
| 34 | Quick load | F8 | absent |
| 35 | Quick save | F5 | absent |
| 36 | Unnamed/developer slot | not a documented PC control | absent |
| 37 | Cheat menu/developer slot | not a documented PC control | absent |
| 38 | Bullet-time slot, hidden from ordinary menu | not a documented PC control | absent; concentration uses native weapon-state transitions |
| 39 | Kick | C | right Circle; conflicts with global Escape/back |
| 40 | Focus / squint | X | absent |
| 41 | Multiplayer statistics | Tab | absent |
| 42 | Multiplayer chat | Y | absent |
| 43 | Multiplayer team chat | U | absent |
| 44 | Multiplayer team/class selection | T | absent |
| 45 | Multiplayer voice | V | absent |
| 46 | Next weapon | mouse wheel up | R1 |
| 47 | Previous weapon | mouse wheel down | L1 |

The manifest has twelve gameplay actions. Movement supplies four native values,
and continuous look supplies two neutral values, producing sixteen entries in
the native dispatch array. This is not complete PC gameplay coverage.

`Data/InputActions.def` incorrectly labels quick save and quick load with the
same ID 33. The shipped class identifies 33 as logs, 34 as quick load and 35 as
quick save. Do not build new mappings from that duplicate definition.

Global VR controls are Create/recenter, Cross/accept, Circle/back and L2/R2 ray
selection. Circle/back is dispatched inside the native-stereo gameplay path,
not just the flat menus; its simultaneous kick binding is an ownership conflict.
Options has no binding in the current Sense asset. A proposed layout must give
pause its own Options action and scope back/accept/select to the owning UI.

## Mechanics that a button list does not cover

- **Contextual F:** the manual assigns pickup, put-down, devices and mounting to
  the same action. `HUDActiveTrigger.UpdateText` reads
  `BeingTriggered.GetActiveTrigger().GetTip()`. An interaction button without
  that cue loses information about both availability and outcome. Existing F
  dispatch is not proof of tracked-hand object selection or physical grabbing.
- **Mouse buttons:** right mouse aims two-handed weapons. With Billy's whip,
  the two buttons attack/grab, adjust hanging length, and together detach;
  jump can also release it. The manual gives contradictory shortening/lengthening
  directions in its overview and detailed whip sections. Retain native per-button
  semantics and verify the actual whip consumer rather than choosing from that
  contradiction. These are contextual actions, not two unconditional gun shots.
- **Ray concentration:** the manual says to put guns away and press a mouse
  button to quick-draw when concentration is available. It uses two moving
  crosshairs and a cooldown indication. Mapping hidden action 38 as a new generic
  slow-motion button would not reproduce this mechanism. Hands action 29 and
  the special aiming/availability UI are required.
- **Duels:** drawing becomes available when the countdown reaches zero or the
  opponent reaches for his gun. The manual specifies downward mouse movement
  to draw, then upward mouse movement to raise/aim, and allows Q/E leaning. Ordinary
  HMD look and neutral native actions 2/3 do not establish a VR duel contract.
  The quick-draw input owner and special crosshairs remain unresolved.
- **Billy climbing and stealth:** forward plus jump initiates climbing;
  crouching, slow movement, cover and light level affect stealth. Physical
  camera lean is not proof that native lean/cover state matches Q/E. The player
  icon communicates stance/stealth; losing it removes useful mechanical feedback.
- **Horse:** F mounts by selecting the saddle; Caps Lock requests gallop.
  Strafe controls steer while mounted. Horse health/stamina are necessary
  feedback. Ordinary on-foot locomotion acceptance does not accept horse input.
- **Focus:** `PlayerBeing.Squint`, `CanSquint` and `UpdateSquint` own state and
  smoothing. `CanRun` rejects running during focus; `GetBeingZoom` multiplies
  native zoom by squint zoom, and `BeingCamera.UpdateZoom` changes native FOV.
  A key injection alone cannot reproduce zoom through the independently owned
  asymmetric XR frusta. Native focus state and VR magnification require separate
  policies; do not silently replace the headset projection with flat-screen FOV.

## HUD presentation boundary

The official HUD supplies compass/objective direction, player stance/stealth,
concentration availability, health, weapons, ammunition, contextual action and
horse condition. Shipped HUD components additionally own hints, objectives,
logs, direction/damage indicators, countdown and boss feedback. Subtitle setting
observation is separate from actually displaying subtitle text in the headset.

The current native-stereo pair is captured in the camera render-view hook.
`CameraStereoReticleOverlay` carries only activation, frame sequence and two
projected points; the presenter draws a fixed black/white cross. It carries no
native HUD sprites/text, target identity, no-shoot state or weapon availability.
The operator reports missing gameplay HUD. This demonstrates incomplete
presentation, not that every native HUD object has stopped updating. The exact
native sprite draw/capture ordering remains to be measured before selecting a
HUD capture hook; camera scene capture alone is not evidence of complete UI.

The native red no-shoot indication is separate from reticle alignment:
`HUDCrosshairHand.CanShowDontShot` delegates to `Being.CanShowDontShot(hand)`;
`CanFireAtTarget` and `AimingAtValidTarget` are separate predicates. The eyes-only
crosshair returns false for no-shoot. Reusing the wrong owner or coloring the VR
cross merely by hit distance would give incorrect feedback. Verify that native
target selection corresponds to the current tracked weapon ray before attaching
the native warning to that ray. Whip climb/grab helpers also use native target
predicates and must remain distinguishable from ordinary weapon alignment.

## Planned adaptation and acceptance boundary

1. Recover readable hints/tutorials, subtitles, contextual action, health/ammo,
   native no-shoot feedback and special countdown/aiming states. Each must retain
   native availability and dismissal rules, including paused hints. Choose the
   UI render/capture owner from measured ordering and keep UI work outside the
   second world-render simulation/update path.
2. Separate Options/pause from Circle/kick and provide focus, alternate fire,
   put-away/discard, direct weapon choice, objectives and logs. Explicit slow
   walk and accessibility crouch must remain available. Keep quick load/save
   accessible without accidental activation during combat.
3. Replace shoulder weapon cycling with a tracked radial selector backed by
   native inventory and actions 21-27/29 as appropriate. Show only available
   items; respect per-character, per-hand and native reload/attack ownership.
   Opening/selecting the wheel must suppress conflicting snap/fire actions and
   release them safely on focus loss. Keep throw distinct from selecting hands.
4. Move compass/objective direction and compact status to a wrist presentation
   once reliable native feedback exists. Retain essential tutorials/subtitles
   where they remain readable without looking at the wrist. A wrist compass
   must preserve waypoints, not merely reproduce north.
5. Establish separate physical gates for contextual carry/put-down, two-handed
   aim, whip, bow, dynamite, Bible, concentration, duels, climbing and horses.

These adaptations are **planned**. The catalogue and presentation limitations
are source/manual findings, not host-tested implementations or physical
acceptance. [VALIDATION.md](../VALIDATION.md#pc-mechanics-and-gameplay-hud-completeness)
owns their acceptance gate; [ROADMAP.md](../ROADMAP.md#milestone-4--controller-ui-and-interactions)
owns priority. Local settings snapshots and extracted manual/class evidence
belong under ignored `work/`.
