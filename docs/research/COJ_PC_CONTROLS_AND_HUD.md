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
| 12 | Lean left | Q | custom-bindable native lean; room-scale lean remains camera translation |
| 13 | Lean right | E | custom-bindable native lean; room-scale lean remains camera translation |
| 14 | Walk | Shift | Triangle + left-stick click; analog speed remains separate |
| 15 | Toggle walk | unassigned | absent |
| 16 | Crouch | left Ctrl | right-stick click and physical crouch policy |
| 17 | Toggle crouch | unassigned | absent |
| 18 | Horse gallop / multiplayer run | Caps Lock | left-stick click, named `run` |
| 19 | Toggle horse gallop / run | unassigned | absent |
| 20 | Toggle alternate fire | Z | Triangle + Cross |
| 21 | Select rifle / long weapon | 3 | utility sector 3 / custom equipment_3 |
| 22 | Select right pistol | 2 | utility sector 2 / custom equipment_2 |
| 23 | Select left pistol | 1 | utility sector 1 / custom equipment_1 |
| 24 | Select dynamite | 4 | utility sector 4 / custom equipment_4 |
| 25 | Select Bible (Ray) / whip (Billy) | 5 (manual labels whip) | utility sector 5 / custom equipment_5 |
| 26 | Select bow | 6 | utility sector 6 / custom equipment_6 |
| 27 | Discard weapon | Backspace | Triangle + L1 / utility sector 8 |
| 28 | Switch weapons, hidden from ordinary menu | not a documented PC control | absent; distinct from next/previous |
| 29 | Hands / fists (native availability gates) | 0 | Triangle + Circle / utility sector 7 |
| 30 | Contextual action / execute active trigger | F | L1, `interact` |
| 31 | Reload | R | left Square |
| 32 | Show objectives | O | Triangle + right-stick click |
| 33 | Show dialogue and hint logs | L | Triangle + R2 |
| 34 | Quick load | F8 | custom binding only; no default combat chord |
| 35 | Quick save | F5 | custom binding only; no default combat chord |
| 36 | Unnamed/developer slot | not a documented PC control | absent |
| 37 | Cheat menu/developer slot | not a documented PC control | absent |
| 38 | Bullet-time slot, hidden from ordinary menu | not a documented PC control | absent; concentration uses native weapon-state transitions |
| 39 | Kick | C | right Circle during gameplay; back only in owned UI |
| 40 | Focus / squint | X | Triangle + Square toggles native focus; VR magnification open |
| 41 | Multiplayer statistics | Tab | absent |
| 42 | Multiplayer chat | Y | absent |
| 43 | Multiplayer team chat | U | absent |
| 44 | Multiplayer team/class selection | T | absent |
| 45 | Multiplayer voice | V | absent |
| 46 | Next weapon | mouse wheel up | R1 |
| 47 | Previous weapon | mouse wheel down | Triangle + R1 |

The manifest exposes thirty gameplay actions, including the utility modifier and
custom-bindable campaign controls. The game adapter supplies thirty-three native
values: shaped movement, neutral continuous look, and the demonstrated action IDs.
Hidden/developer and multiplayer text/voice actions remain outside this campaign
layout. Native equipment actions decide whether the selected item exists; the
selector does not yet query inventory or hide unavailable sectors.

Native action IDs are not `Attack` hand indices. Exact
`PlayerController.ExecuteInput(IFLjava/lang/Object;)V` converts action 10 to
`ApplyAttack(0, pressed)` and action 9 to `ApplyAttack(1, pressed)`.
`ArmedPlayerBeing.Attack(IZ)` then first tries that hand, 0/right or 1/left,
and can retry the opposite hand. Equating action 9 with `Attack(0, ...)`
physically inverted the VR triggers. The adapter now preserves the complete
controller conversion: R2 -> action 10 -> hand 0, L2 -> action 9 -> hand 1.
For a locally tracked
firearm operated with one hand, the exact `CanAttack(II)Z` prefix now rejects
that opposite-hand retry. Own-hand attacks, native two-hand operation, tools,
missing tracking caches and forced-network attacks retain native eligibility.
Host JNI checks include this downstream conversion; corrected physical trigger
ownership remains pending acceptance.

A standalone shipped-Java fixture executes the unchanged `ExecuteInput`,
`ApplyAttack` and `ApplyAttackState` bytecode against inert engine/player stubs.
The formerly inverted request fails the right-hand assertion; action 10 reaches
hand 0 and action 9 reaches hand 1. This establishes controller routing only,
without game initialization or physical firing acceptance.

`Data/InputActions.def` incorrectly labels quick save and quick load with the
same ID 33. The shipped class identifies 33 as logs, 34 as quick load and 35 as
quick save. Do not build new mappings from that duplicate definition.

Options has a separate global pause action. Circle is back in flat menus or a
valid frozen native UI, and kick in ordinary gameplay. Unknown timer ownership
cannot authorize Circle navigation. Explicit Options can request the native
Escape route to open pause. Create/recenter, Cross/accept and L2/R2 ray selection
retain their existing routes.

### Secondary Sense layer

Hold left Triangle for secondary functions. L1 otherwise performs contextual F.
Normal fire, jump, reload, kick, gallop/run and stick shaping retain their native
semantics. While the modifier is active, firing and snap turning are suppressed.

| With Triangle held | Intent |
| --- | --- |
| Square | toggle native focus; repeat to exit |
| Cross | alternate fire mode |
| Circle | put weapons away / hands |
| L1 | discard weapon |
| R1 | previous weapon |
| R2 | open dialogue/hint logs |
| Left-stick click | slow walk while held |
| Right-stick click | objectives |

The right stick provides eight gesture sectors, clockwise from up: left pistol,
right pistol, rifle, dynamite, Bible/whip, bow, hands and discard. Start from a
neutral stick, deflect to choose, and return to neutral before choosing again.
Crossing sectors while deflected retains the first choice. This is quick selection,
not a visible inventory wheel; item icons, available-item filtering and tracked
wheel presentation remain planned.

Unavailable controls cannot count as observed releases. Native blocking UI and
presenter context generations reset the layer without relying on texture uploads.
Inputs held before entering the layer must be released before becoming secondary
actions. Closing it cannot turn a held logs trigger into a shot or a held selection
stick into a snap. Focus persists after leaving the layer so ordinary aiming/fire
can continue, but resets on focus/menu/pose loss. Explicit custom focus remains a
held action. Save/load and native Q/E lean are available to deliberate custom
SteamVR bindings; no save/load chord is assigned during combat. Actual XR zoom
magnification is still open, despite native focus-state delivery.

Triangle alone only opens the layer; it does not request focus or hands. Hold
Triangle, then freshly press Square for focus or Circle for hands. Release any
button held before layer entry first. A layer-active record without that chord
or a right-stick selection is not evidence that a secondary mechanic was tried.

## Mechanics that a button list does not cover

The native controller rejects digital one-shots while
`LockApplyControllerState` is active. `ApplyState` does not replay F, reload,
weapon selection, jump or kick. The adapter therefore commits only shaped
analog actions 4-7 under that lock and dispatches digital transitions after
unlock/apply; an analog failure leaves their edges pending for retry.

`BeingTriggered.ExecuteTrigger` calls `CheckTriggers` on the press transition.
Its original look origin/direction getters follow the character, independently
of temporary stereo-camera and per-hand weapon overrides. The exact class patch
substitutes only those two calls with private helpers selecting a complete
nullable HMD gaze pair. The original 148-byte selection method retains all
branch offsets, native trigger range and target/permission tests. Missing either
vector delegates to the original getter. Publication uses the central render
camera in centimetres, with physical translation/height and actor-yaw ownership;
it clears on lost tracking, blocking UI and player changes. The patch and JNI
failure cleanup are host-tested. The operator confirms L1 box pickup/carry and
pistol pickup. A subsequent L1 press releases the carried box; that put-down
gesture is also operator-confirmed. Device use and mounting remain physical gates.

### Native character and tutorial eligibility

`gameplay_input result=applied` establishes adapter dispatch without a JNI
exception. The bridge discards `InputDigital.Translate`'s boolean return; that
return itself only indicates invocation of an eligible target callback. Native
consumer acceptance, animation and visible feedback remain separate evidence.
`BeingController.CanApplyState` requires an unlocked controller and permitted
input. `PlayerController.CanExecuteInput` additionally rejects kick while riding
a horse and permits only its listed actions while using a cannon.

Kick action 39 calls `CreatureController.ApplyKick -> PlayerBeing.Kick`.
`m_bKickingEnabled` must be true; inherited `Creature.CanKick` also requires a
living, movable being, mode state 0, and neither step nor jump speed state. Both
`PlayerBeingBilly` and `PlayerBeingRay` inherit this path. Their constructors do
not establish a permanent character-specific kick ban: `PlayerBeing` initially
enables kicking, punching and weapon changes, and campaign actions/save state
can change those permissions.

The shipped `Data1.pak` Home01 and Home02 maps explicitly include an
`act_PlayerControl` in their level `cStartAction` lists with
`ePlayerKicking=_DISABLE` and `ePlayerPunching=_DISABLE`. These are deliberate
opening-campaign restrictions, not missing controller bindings. The same action
class resolves the main player and calls `SetKickingEnabled` and
`SetPunchingEnabled`. Home01 also contains scripted weapon-change disable/enable
and holster/restore transitions. A live player/level/permission readback is
required before attributing an individual rejected press to one of these gates.

Hands action 29 calls `ArmedPlayerBeing.WeaponHandsToggle`, which attempts nearby
weapon replacement, then toggles inventory slot 7. The toggle requires weapon
changes enabled, permits carrying only when `CanCarryWithWeapon` allows it,
and needs an available usable inventory candidate; selecting `WeaponHands` is
rejected when punching is disabled. It is therefore not an unconditional hide
command. The campaign's `HolsterWeapons/RestoreHolsteredWeapons` methods instead
own the separate `m_bWeaponsHolstered` flag. Preserve both meanings and native
tutorial permissions when presenting put-away controls.

Native focus action 40 sets `PlayerController.m_bSquint` and applies
`PlayerBeing.Squint`. The player must be movable to accept the setter;
`IsSquint` additionally requires `CanSquint`: alive, movable, not looking at a
scripted target, and neither kick nor jump speed state. Neither character class
overrides these checks. Native zoom smoothing and its `_SQUINT` game event do
not establish magnification in the independently supplied XR eye frusta.

The operator specifically confirms pause, L1 box pickup/carry/put-down and gun
pickup, reload, jump, crouch and Triangle + R2 dialogue/hint logs. Focus and hands
intents reached the native dispatcher. Focus now has operator-confirmed blur
without perceived magnification; toggle-off/fire recovery and put-away remain
unaccepted. Mounting, kick and equipment selection retain separate gates; only
one pistol was available. Run details remain in ignored local evidence, and the
project validation document owns formal acceptance state.

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
presentation, not that every native HUD object has stopped updating.

### Essential text owners

The essential text route is **implemented / host-tested**, with live/headset
acceptance pending. `HUDManager.sm_cMainHUDManager.m_Being` must identify the
bridge's current player. The shipped 23-element `m_aHudComponents` array assigns
index 9 to `HUD_ACTIVE_TRIGGER` and 11 to `HUD_HINT`; component instance types
are checked before reading. Interaction text comes from
`HUDActiveTrigger.m_cIcon.m_sLocalizedText`. Tutorial text follows
`HUDHint.m_cMainWindow -> UIWindowInfo.m_cInfo`, whose
`UIWindowInfoElement` inherits `UIStatic.m_sLocalizedText`. Native `SetText`
already performs localization/expression processing; observation never calls it.
The manager, components and concrete text owners must be actually visible.

Dialogue is independent: `LawmanGame.sm_cSettings.bSubtitles`,
`Dialog.cPlayingDialog.m_bCurrentLineVisible` and checked
`nCurrentLine/aLines` resolve `DialogLine.cCharacter`. The actor's native
`AreSubtitlesVisible` and actual `m_cSubtitle` visibility gate `sSubtitleText`.
`Text.Get(String)` resolves this shipped text identifier without replaying
`ShowSubtitle`, dialogue progression or sprite drawing. Missing owners and JNI
exceptions clear affected text and release all local/pinned references.

The game owner samples at most once per 100 ms. Three bounded UTF-16 values
(1023 code units each, surrogate-safe ellipsis on truncation) and exact eye
optics accompany the captured frame; no JNI pointer crosses threads. The
presenter caches Unicode rasters and draws finite-depth tutorial, interaction
and subtitle panels onto its own D3D11 textures after fresh world copy. Full
eye transforms/FOV retain stereo disparity and perspective under eye cant.
This recovers essential text independently of native sprite alpha/coverage.
Long text is wrapped with a bounded font/height fit; visual layout, timing and
performance still require live acceptance. Isolated LMB/RMB/SPACE BAR/SPACEBAR
tokens in hint copies map to L2/R2/Cross; dialogue and native strings are unchanged. Broader PC-key adaptation remains open, and
native fading/animation styling is not reproduced. Pausing hints and other
blocking UIs retain the existing flat UI/dismissal route.

### Read-only health and ammunition presentation

HUDManager's exact 23-component array supplies HUDPlayer at index 0,
HUDAmmoCounters at 1 and HUDWeapons at 7. Each component's `m_Being` and
`m_cHUDManager` must match the current player/manager; HUDPlayer's `m_cPlayer`
is checked separately. Actual manager/component/child visibility is mandatory.
HUDPlayer's `m_cHealth.m_sLocalizedText` already holds the game's normalized,
padded health percentage (including the original nonzero minimum and invalid
game-state blanking); no health calculation or UpdateHealth call is needed.

HUDWeapons has six `m_tAmmoCounters` and matching `m_tSlotsWeaponInfo` caches.
The shipped slot hierarchy is 2,1,0,4,3,5: left pistol, right pistol, long weapon,
dynamite, Bible/whip, bow. `m_bShowAmmo` controls whether a counter is relevant;
`m_bActiveAmmo` identifies active/actual slots. The compact card includes only
those active/actual slot totals, not the entire inactive weapon list. A slot
total is native presentation data, not a newly inferred magazine capacity.

HUDAmmoCounters has three `m_cAmmoCountersTotal` entries: rifle, shotgun, pistol.
Big/small bullet icon visibility and counter `m_fCurTextAlpha` gate their cached
localized inventory-reserve strings. Native updates own counts, fading and
permission changes. The reader copies bounded numeric text only; unexpected
layout/text or JNI errors clear status independently of compass/inventory.
No `UpdateWeapons`, `UpdateAmmoCounters`, weapon parameter factory or selection
is invoked. Full native graphical styling and protected-target feedback remain
separate boundaries. Host tests do not establish visor readability or live counts.

### Native inventory and compass observation

The inventory reader observes the exact game's `PawnInventory.m_aInvSlots`
(`java.util.ArrayList`) and `m_aInvSlotsObjects` (`InvObject[]`). Each
`InventorySlot` supplies `m_nType`, `m_nIndex` and `m_cPawn`; the indexed item's
`cOwner` must identify the current player. A slot index is not a slot type.
The six selection channels use slot types `2, 1, 0, 4, 3, 5`: left pistol,
right pistol, long weapon, dynamite, Bible/whip and bow. Hands use type `7`.
Native slots `11` and `12` hold objects only in the right/left hand and are not
additional numbered equipment channels. A `WeaponWhip` instance distinguishes
whip from Bible in the shared native slot; the reader never creates equipment.

Ownership and observed permission are separate masks. Selection respects
`PlayerBeing.m_bWeaponChangeEnabled`, the single-player carry permission
`HumanBeing.m_bCanCarryWithWeaponInSingleGame` when `m_cCarriedObject` is non-null,
and `m_bPunchingEnabled` for hands. Discard uses the separate change/throw gates,
including `ArmedPlayerBeing.m_bWeaponThrowEnabled`; it is distinct from selecting
hands. The existing native input consumers remain authoritative for ammunition,
reload/attack transitions, nearby replacement, holstering and contextual execution.
An owned weapon with zero ammunition is not automatically hidden or denied.
These observed masks are not proof that a selection executed.

`ArmedPlayerBeing.CanShowWeapon` must not be invoked as an observation predicate:
its `Weapon.CanNoAmmoUse -> GetLogicParams -> GetParams` chain reaches
`LawmanModule.GetWeaponParam`, which calls `CreateWeaponParam` if its cache has no
entry. Likewise, inventory getters returning arrays allocate Java arrays.
The observation path instead bounds and reads the existing slot collection,
checks identities and reads the demonstrated permission fields. JNI exceptions
clear the affected snapshot; all local references and pinned strings are released.
No selection/update method runs, and no native reference crosses threads.

Native objective direction belongs to `HUDManager.sm_cMainHUDManager`, whose
23-component array contains `HUDCompass` at index `19`. The manager and compass
must both identify the current player, the compass's `m_cHUDManager` must identify
that manager, and the native owners must be actually visible. `HUDCompass`
provides `m_fMapAngle`, `m_vPlayerPos`, `m_vPlayerForward` and the misleadingly
named `m_vPlayerRight`: `CalculateMapAngle` fills the latter through
`BeingCamera.GetLeftVector`; `CalculateWaypointPos` subsequently flattens it to
`(-forward.z, 0, forward.x)`. Preserve this observed native left axis.

`m_cWaypointsManager.m_cWaypoints` is a `java.util.Vector` of
`HUDWaypointsManager$HUDWaypoint`. Each entry has `m_sName`, `m_vPos`,
`m_bActive`, `m_nRotorIdx` and optional `m_cLocation`. `m_nRotorIdx` is the
stable `UIRotorText$RotorTextElement.m_nIndex` identity, assigned from the
monotonic `UIRotorText._idx`; it is not a vector index. Removing a location
compacts the rotor collection without reassigning these identities. Resolve the
bounded ID-to-index relation before selecting an element. Location-backed `m_vPos`
aliases `DefLocation.m_vWorldPos`; the reader must not replay
`DefLocation.UpdateObjectPos`. The rotor collection
`m_cRotorText.m_cElements` supplies `UIRotorText$RotorTextElement.m_fAngle` and
the concrete `m_cElement`, `m_cPoint` and `m_cObjectPoint` visibility. An active
waypoint is shown only when at least one corresponding native sprite is actually
visible. Labels use the existing text owner's `m_sLocalizedText`, with the
cached waypoint name only when that text owner is absent. Bounds/type checks
precede all array/collection accesses. Observation retains up to sixteen visible
markers from a bounded collection and clears invalid/non-finite guidance.

The native bearing convention is forward `90` degrees, left `180`, right `0`.
For a bearing `a`, its horizontal world direction is
`sin(a) * nativeForward - cos(a) * nativeLeft`. Native map rotation is
`-m_fMapAngle`; that angle is
`degrees(atan2(cameraLeft.z, cameraLeft.x)) + 90 + levelSunAngle`.
The shipped `Compass` texture has `N` authored at the top; `HUD.scr` keeps
`ID_COMPASS_MAIN` visible and `ID_COMPASS_NEEDLE` initially hidden. The native
north direction follows `sin(mapAngle) * nativeLeft + cos(mapAngle) * nativeForward`.
Consequently, a wrist presentation must preserve the level's native compass
rotation and every observed objective marker, rather than infer north from XR
yaw alone. Native world coordinates remain centimetres in the game adapter;
renderer-facing dial directions and tracked surface geometry contain value data.

The game-owner observation cache is bounded to a 100 ms sampling interval and
invalidates with player generation/context loss. Reader fixtures establish
ownership, permissions, visibility, bounds, Unicode and cleanup on the host;
tracked wrist direction, native marker timing and in-headset readability remain
separate live/physical gates.

### Native graphical capture

Exact native inspection identifies `CLevel` sprite traversal at RVA `0x2E680`
(vtable entry `0x2E1C98`) and `CD3DRenderer` batch flush at `0x247990`
(entry `0x3123B8`). Both take the native owner in ECX; flush additionally takes
one 32-bit stack option. The combined callback `0x1D0A30` renders world views,
then sprites, then flushes queued geometry. The present eye captures inside
`0x30FB0` therefore precede HUD drawing on that route. The sprite-only callback
`0x1D0BE0` is separate; the actual live script route is not yet accepted.

Sprite traversal is not pure: AVI servicing, callbacks and child traversal can
change state. Never recover HUD by replaying it for the second eye. A future
capture must observe the naturally scheduled pass once and remain active through
batch flush. Subtitles register independent level sprites and cannot be assumed
to be HUDManager descendants.

The candidate installs a passive, exact-build guarded observation of those two
vtable entries. Sampled begin/end events correlate sprite pass, native stereo
frame, Present sequence, thread, device generation, current render target,
viewport and blending state. Device queries require the observed native render
thread and never wait for its device lock. Original calls execute once, with
unchanged arguments; restoration preserves foreign hooks. No render target is
redirected and no sprite pass is replayed. Host tests cover forwarding and owned
restoration; live scheduling, target ownership and usable alpha/coverage still
require evidence before implementing HUD capture and composition.

The native red no-shoot indication is separate from reticle alignment:
`HUDCrosshairHand.CanShowDontShot` delegates to `Being.CanShowDontShot(hand)`;
`CanFireAtTarget` and `AimingAtValidTarget` are separate predicates. The eyes-only
crosshair returns false for no-shoot. Reusing the wrong owner or coloring the VR
cross merely by hit distance would give incorrect feedback. Verify that native
target selection corresponds to the current tracked weapon ray before attaching
the native warning to that ray. The implemented consumer reads HUDManager
components 3/4, validates HUDCrosshairHand/player/manager/hand identity and the
actual visibility of its `dontShoot` Sprite. It also requires native
`m_abCanFireAtTarget[hand] == false` and protected reasons 1 (friendly human) or
3 (friendly LawmanActor) from `m_anCantFireReason`. Generic valid-target and
empty-ammo states are not protection warnings.

The original `CheckIfAimingAtFriendlyTarget(int, boolean)` owns the natural
budgeted trace. Immediately after committing both `m_vLCStart`/`m_vLCEnd` vectors,
a bounded exact-class patch tags its existing local Weapon into one nullable
owner field per hand. Original instructions/branch destinations and native
trace cadence are preserved; no trace, gameplay update or draw is replayed.
The read-only consumer matches that tag to `GetActualWeaponNotEmpty(hand)`,
current in-hands firearm ownership, a non-null cached collision, age at most
0.25 seconds and the selected verified muzzle ray. Origin drift over 3 cm or
angular drift over 3 degrees suppresses the flag; these are conservative stale
presentation bounds, not gameplay range/permission tuning. The renderer draws
an outlined red X at each matching captured eye point, preserving surrounding
world pixels. Unknown original class hashes still fail closed. The reader,
bytecode/JVM verification, geometry rejection, pixels and captured flag transport
are host-tested; native warning appearance/target transitions remain a visor gate.
Whip climb/grab helpers remain separate from firearm protection warnings.

## Exact procedural whip contract

`WeaponWhip.class` SHA-256
`0a7ca8d2059582320251646fe2a992bf43ecf04470986128d99ff503366f5746`
extends `WeaponFire`, but does not provide the gun barrel required by rigid
weapon-element mapping. Treating it as an unverified firearm prevented the
independent hand and controller reticle from being published.

The shipped `InputSettings` maps action 9 to LMB and action 10 to RMB. The Sense
routes are therefore L2/LMB and R2/RMB, irrespective of which hand holds the
tool. `WpnLogicWhip.GetAttackMode` selects normal whip fire for input type 1
(L2), alternate clutch fire for type 0 (R2), and respectively `DragOut`/`DragIn`
when already clutched. Native drag removes/adds seven parts; L2 lengthens and
R2 shortens. The native combined-button timing rule unclutches, as does jump
while hanging. Preserve native availability, timing and permissions rather than
converting these into two gun shots. The tracked one-hand firearm retry guard
must specifically exclude `WeaponWhip`.

`ComputeWhipPosition` uses the owner's right holding socket. Its native position
offset is `socket + Z*m_fPositionLeft + X*m_fPositionUp + Y*m_fPositionForward`;
the whip's up/forward are socket X/Y. Its loop shaping also consumes socket Z
as `s_vLeft`, completed by `Vector.CalcCross(up,forward)` for the rigid tracked
basis. Read the actual instance floats. A bounded
bridge-owned three-vector cache supplies only those position/up/forward values
for the recognized owned in-hands whip, with the original method retained for
missing pose, other owners and network-forced attacks. The original
`AdditionalSynchro` stays byte-for-byte unchanged: it places the root and first
cloth point, retains loop shaping, segments, clutch world anchor and physics.
Do not rotate the entire cloth, force an update/draw, change damage or guess new
attachment offsets. Cache publication and clearing check object identity and
readback; ambiguity retains the restore owner. Reuse vectors across frames.

The reticle uses the controller right-tip direction and tracked holding origin
only after hand/cache verification; it is a tool alignment aid, not a measured
collision endpoint or proof of a successful clutch. Isolated mouse-button and
space-bar names in hint/interaction presentation copies are translated to
L2/R2/Cross. Native strings and subtitle dialogue are not edited. Bytecode,
JNI lifecycle, ray selection and labels are host-tested; physical whip attack,
clutch, length/release and visible hand cohesion remain open.

## Planned adaptation and acceptance boundary

1. Recover readable hints/tutorials, subtitles, contextual action, health/ammo,
   native no-shoot feedback and special countdown/aiming states. Each must retain
   native availability and dismissal rules, including paused hints. Choose the
   UI render/capture owner from measured ordering and keep UI work outside the
   second world-render simulation/update path.
2. Physically validate Options/pause versus Circle/kick, focus, alternate fire,
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

The input expansion, essential text presentation and passive HUD observation are **host-tested**. Complete
HUD capture and XR focus magnification remain **planned/open**. The visible
inventory wheel and objective wrist compass have operator acceptance for
exercised contexts, with the smaller revised presentation still pending.
New controls and special mechanics still require live
and headset acceptance; source/manual findings are not physical acceptance. [VALIDATION.md](../VALIDATION.md#pc-mechanics-and-gameplay-hud-completeness)
owns their acceptance gate; [ROADMAP.md](../ROADMAP.md#milestone-4--controller-ui-and-interactions)
owns priority. Local settings snapshots and extracted manual/class evidence
belong under ignored `work/`.
