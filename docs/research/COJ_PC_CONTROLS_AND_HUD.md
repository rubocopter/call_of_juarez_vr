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

## Complete native catalogue and logical VR intents

There are 48 action slots. The ordinary controls menu enables 43: all except
8, 28, 36, 37 and 38. Developer-key mode can additionally enable 36 and 37.
An unused slot, hidden action or unassigned toggle must not be mistaken for a
missing mandatory campaign button. PC keys below are official defaults; rows
outside that manual are explicitly marked.

| ID | Native action / PC meaning | PC default | VR logical intent |
| --- | --- | --- | --- |
| 0 | Look up | unassigned | tracked HMD; no digital look intent |
| 1 | Look down | unassigned | tracked HMD; no digital look intent |
| 2 | Turn left | unassigned | adapter snap; native continuous look neutral |
| 3 | Turn right | unassigned | adapter snap; native continuous look neutral |
| 4 | Forward | W | move.y positive |
| 5 | Backward | S | move.y negative |
| 6 | Strafe right | D | move.x positive |
| 7 | Strafe left | A | move.x negative |
| 8 | Show map, hidden from ordinary menu | not a documented PC control | absent; O is action 32 |
| 9 | Left mouse / left-hand weapon action | left mouse | fire_left (original LMB semantics) |
| 10 | Right mouse / right-hand weapon action | right mouse | fire_right (original RMB semantics) |
| 11 | Jump | Space | jump |
| 12 | Lean left | Q | lean_left, custom only; physical lean remains camera translation |
| 13 | Lean right | E | lean_right, custom only; physical lean remains camera translation |
| 14 | Walk | Shift | walk, custom only; default retains analog magnitude |
| 15 | Toggle walk | unassigned | absent |
| 16 | Crouch | left Ctrl | crouch: VR toggle OR physical height |
| 17 | Toggle crouch | unassigned | native persistent toggle deliberately unused by VR |
| 18 | Horse gallop / multiplayer run | Caps Lock | run: contextual shoulder while mounted |
| 19 | Toggle horse gallop / run | unassigned | absent |
| 20 | Toggle alternate fire | Z | alternate_fire |
| 21 | Select rifle / long weapon | 3 | equipment_select[2] / SelectRifle |
| 22 | Select right pistol | 2 | equipment_select[1] / SelectPistolRight |
| 23 | Select left pistol | 1 | equipment_select[0] / SelectPistolLeft |
| 24 | Select dynamite | 4 | equipment_select[3] / SelectDynamite |
| 25 | Select Bible (Ray) / whip (Billy) | 5 (manual labels whip) | equipment_select[4] / SelectContextualTool |
| 26 | Select bow | 6 | equipment_select[5] / SelectBow |
| 27 | Discard weapon | Backspace | discard_weapon / ThrowWeapon |
| 28 | Switch weapons, hidden from ordinary menu | not a documented PC control | absent; distinct from next/previous |
| 29 | Hands / fists (native availability gates) | 0 | hands |
| 30 | Contextual action / execute active trigger | F | interact |
| 31 | Reload | R | reload |
| 32 | Show objectives | O | objectives |
| 33 | Show dialogue and hint logs | L | logs, custom only |
| 34 | Quick load | F8 | quick_load, custom only |
| 35 | Quick save | F5 | quick_save, custom only |
| 36 | Unnamed/developer slot | not a documented PC control | absent |
| 37 | Cheat menu/developer slot | not a documented PC control | absent |
| 38 | Bullet-time slot, hidden from ordinary menu | not a documented PC control | absent; native weapon-state concentration |
| 39 | Kick | C | kick: contextual shoulder on foot |
| 40 | Focus / squint | X | focus: direct VR toggle; magnification observes native squint |
| 41 | Multiplayer statistics | Tab | absent |
| 42 | Multiplayer chat | Y | absent |
| 43 | Multiplayer team chat | U | absent |
| 44 | Multiplayer team/class selection | T | absent |
| 45 | Multiplayer voice | V | absent |
| 46 | Next weapon | mouse wheel up | weapon_next, custom only |
| 47 | Previous weapon | mouse wheel down | weapon_previous, custom only |

The manifest retains thirty gameplay inputs, including the weapon-radial input
and optional custom campaign actions. The adapter supplies thirty-three native
values. Native selection retains final ammunition/reload/context authority;
the wheel observes owned inventory and cached permissions without selecting
anything during highlighting. Hidden/developer and multiplayer text/voice
actions remain outside the default campaign layout.

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
Host JNI checks include this downstream conversion. Corrected right-pistol and
empty-left-trigger ownership is operator-accepted; left/dual firing and native
two-hand/tool contexts retain separate physical gates.

A standalone shipped-Java fixture executes the unchanged `ExecuteInput`,
`ApplyAttack` and `ApplyAttackState` bytecode against inert engine/player stubs.
The formerly inverted request fails the right-hand assertion; action 10 reaches
hand 0 and action 9 reaches hand 1. This establishes controller routing only,
without game initialization or physical firing acceptance.

`Data/InputActions.def` incorrectly labels quick save and quick load with the
same ID 33. The shipped class identifies 33 as logs, 34 as quick load and 35 as
quick save. Do not build new mappings from that duplicate definition.

### Default Sense controls

This is the sole user-facing default control table. The OpenVR binding supplies
physical state; GameplayControlMapper resolves wheel ownership and direct
toggles on the game owner. Only the CoJ adapter translates logical intents to
native IDs. No keyboard settings or OS-global key/mouse injection is introduced.

| Sense | Gameplay |
| --- | --- |
| Left stick | head-relative movement; small deflection gives native analog slow movement |
| L3 | on-foot crouch toggle, merged with physical crouch |
| Right stick | 45-degree snap turn |
| R3 | Focus toggle; XR magnification follows real native squint |
| L2 | original LMB / left-hand action |
| R2 | original RMB / right-hand action |
| L1 | Action / contextual F from the left Sense aim ray; reference ring only while held |
| R1 | fresh Kick press on foot; hold Horse run / gallop when mounted |
| Square | Reload; opens/closes an admitted Peacemaker/Frontier session with `-ManualReload` |
| Cross | Jump |
| Circle | Toggle alternate fire |
| Triangle | hold weapon/equipment radial; release to confirm or cancel |
| Create tap | Objectives, decided on release before 800 ms |
| Create hold | Recenter VR once at 800 ms; release cannot also send Objectives |
| Options | Pause / native Escape |

Triangle has no modifier or tap action. Hands and Throw are wheel sectors only
by default. With `-ManualReload`, fresh Square prepares an admitted single
Peacemaker/Frontier with empty support. The free-hand **trigger**, not Grip,
picks up a cartridge at waist and releases it near the weapon to insert one
native round. Square again or armed trigger closes; release before firing or
reopening. Scoped headset acceptance covers sustained waiting, tracked hands,
successive loading, insertion-only haptics and exercised menu/weapon recovery.
Cartridge contact/occlusion, precise chamber loading, mirrored/Frontier contexts
and broader recovery remain open.

Without this opt-in, the retained hybrid trigger journey requests native reload;
the Peacemaker/Frontier paced extension limits each natural completion to one
round and has bounded live continuation evidence. Native animation still takes
the hands in that legacy route. Schofield retains whole-clip behavior.
No VR ammunition replica is maintained in either route. See
[manual acceptance](../VALIDATION.md#persistent-manual-reload-candidate) and
[exact reload ownership](COJ_RELOAD_OWNERSHIP.md).

Next/previous weapon, logs, quick save/load, digital lean and explicit
Walk retain native keyboard and optional custom logical routes, with no default
Sense binding. Quick Load has no default controller gesture.

| Direction | Logical action | CoJ native route |
| --- | --- | --- |
| Up | SelectRifle | action 21 / PC 3 / long weapon |
| Up-right | SelectBow | action 26 / PC 6 |
| Right | SelectPistolRight | action 22 / PC 2 |
| Down-right | SelectDynamite | action 24 / PC 4 |
| Down | Hands | action 29 / PC 0; native fists/holster semantics |
| Down-left | ThrowWeapon | action 27 / Backspace; distinct from Hands |
| Left | SelectPistolLeft | action 23 / PC 1 |
| Up-left | SelectContextualTool | action 25 / PC 5; Bible for Ray / whip for Billy |

The kWeaponRadialActions table is shared by intent resolution and presentation
ordering. Native inventory stays in semantic channel order; the adapter remaps
labels, ownership and permissions into clockwise spatial order for the renderer.
Slot 5 is demonstrated by the shipped action catalogue and inventory type 3.
The reader distinguishes a WeaponWhip instance from the Bible in that same
slot. No separate invented Bible action exists.

Triangle opens the wheel on its first eligible held sample. Right-stick
highlighting emits no native command, including while crossing Throw. A single
logical equipment command is emitted on an available Triangle release with
a valid highlighted sector. Center or unavailable/invalid stick cancels.
The retained 0.65 deflection threshold engages selection; 0.55 disengages it,
providing 0.10 radial hysteresis. A selected sector retains its highlight until
the angle exceeds its 22.5-degree half-width by 5 degrees. Unobserved inventory
shows disabled sectors; owned/permitted selection still requires a valid native
snapshot. A denied highlight cannot execute merely because permission recovers;
center or another sector starts a fresh gesture.

While open, the wheel consumes digital gameplay buttons and all right-stick
turning. It retains established manual crouch/Focus state and left-stick movement.
On closing, held buttons must release before gameplay resumes. Snap stays blocked
through the closing sample and subsequent deflected samples until an available
finite right stick returns inside radius 0.25. This barrier is additional to the
existing native snap thresholds, 0.70 engage and 0.35 horizontal release.

Menu/UI owns shared buttons before the wheel: Cross Accept, Circle Back,
L2/R2 tracked-hand pointer selection and Options native pause/navigation.
Gameplay actions and Triangle radial are suppressed in a native blocking UI.
Unavailable controls do not count as releases. Menu/loading/dashboard/tracking
loss and presenter context generations clear held gameplay and manual toggles;
reacquisition requires an actual available release. Native special states retain
their own eligibility after these context gates.

Create resolves at the presenter sample rate with a monotonic clock. A short
gesture becomes an Objectives event only on release; a hold becomes one Recenter
event at 800 ms, or on release if the threshold fell between samples. Events
survive faster presenter polling through the tracking mailbox. Gameplay owns
Objectives delivery: menus and the open wheel consume it. Input/focus/dashboard/
pose loss cancels the gesture and any undelivered mailbox tap, and requires
release before restarting. The game-side retry queue also cancels generated
Objectives when the Create source becomes unavailable; optional held Objectives
retains edge semantics.

L3 keeps a neutral manual toggle and merges it with physical crouch through
native action 16. Action 17 is not suitable here: ToggleDuck flips persistent
m_bToggleDuck, clears toggle-run/walk, and has no release undo; physical
Duck(true) clears that latch. VR action-16 ownership remains releasable at
context loss, and explicit crouch keeps the established camera-height policy.

Synthetic manual and physical Duck require a fresh known on-foot owner. Native
action 16 shares the category-1 target chain with HorseController, whose inherited
Duck can make Horse.CanRun reject gallop. Mounted or unobserved player state
therefore neutralizes VR Duck and suppresses physical crouch-height compensation;
ordinary tracked HMD translation remains active. Mount/dismount resets the
manual latch through the existing controller-owner boundary. Desktop/native
input keeps its original route. This is separate from proving horse behavior
or view comfort during a manual campaign run.

R3 changes only native action 40. Optical magnification remains derived from
the real cached m_fSquintFactor and m_fCurrentMaxSquintZoom, including native
decay after Focus release; a rejected logical request cannot invent a zoom
factor. There is no second R3-controlled magnification latch. Desired/current
bows and scoped Winchester retain their existing special-zoom exclusion.

Default slow movement needs no synthesized Shift: the native float path preserves
the shipped independent 0.04-per-axis deadzone and remapping to actions 4-7.
For example, forward deflection 0.20 becomes magnitude 1/6. Head-relative rotation,
speed policy and native physics are unchanged. Billy stealth comfort/noise remains
a physical acceptance check; no new Walk threshold or hysteresis is introduced.
Physical room-scale lean remains camera translation; Q/E are retained natively
and unbound on Sense. Duel/special-camera lean still needs eligible live evidence.

The revised layout is operator-accepted for exercised available contexts:
release-confirmed wheel selection, snap rearm, wheel haptics, on-foot crouch,
ordinary R3 Focus, Create hold recenter/tap Objectives and encountered HUD rows.
Kick remains inconclusive under campaign eligibility; no horse was available.
Other weapons, mounted routing and special mechanics remain pending rather than
failed. The replacement left-hand L1 route and held-only ring are host-tested,
not physically accepted. [VALIDATION.md](../VALIDATION.md#revised-sense-control-acceptance)
owns the acceptance gates.

## Mechanics that a button list does not cover

The native controller rejects digital one-shots while
`LockApplyControllerState` is active. `ApplyState` does not replay F, reload,
weapon selection, jump or kick. The adapter therefore commits only shaped
analog actions 4-7 under that lock and dispatches digital transitions after
unlock/apply; an analog failure leaves generated radial/Objectives/Kick pulses
pending for retry inside the same context. Delivery is acknowledged per native
action, so a later digital failure cannot replay an earlier successful command.
A prior unreleased pulse is neutralized before a new confirmation; its held
history cannot acknowledge the new command. Inventory denial, a new wheel or
context/owner loss cancels pending commands.

`BeingTriggered.ExecuteTrigger` calls `CheckTriggers` on the press transition.
Its original look origin/direction getters follow the character, independently
of temporary stereo-camera and per-hand weapon overrides. The exact class patch
substitutes only those two calls with private helpers selecting a complete
nullable tracked interaction pair. The original 148-byte selection method retains all
branch offsets, native trigger range and target/permission tests. Missing either
vector delegates to the original getter. Publication now uses the left Sense aim
origin and local -Z in centimetres, with shared render translation/height correction
and actor-yaw ownership. L1 dispatch is suppressed when that hand ray is unavailable,
so its fallback cannot turn a held hand action into head-gaze selection;
it clears on lost tracking, blocking UI and player changes. The patch and JNI
failure cleanup are host-tested. Box pickup/carry, pistol pickup, box put-down
and small-drawer opening were operator-confirmed through the earlier HMD route.
They do not validate the replacement left-hand route. Device use and mounting
remain physical gates.

A small cyan hand-directed ring identifies the ray used by L1 Action independently
of the weapon aiming cross, only while L1 is held with a published valid left aim
in live gameplay. It hides on release, equipment wheel and blocking presentation.
The earlier permanent HMD ring made drawers openable by operator report but was
uncomfortable; that acceptance does not promote the replacement hand route.
Its finite binocular depth is presentation geometry, not native interaction range
or proof of a usable target. Small drawers still require the original native
collision element, reach and door/permission state; those predicates are unchanged.
Fresh Action dispatch diagnostics read the cached active/executing trigger IDs
and active element before/after dispatch without replaying selection or a trace.
Dispatch completion is not proof that the selected native action succeeded.

### Contextual shoulder ownership

The fresh original-class audit establishes HumanBeing.IsRidingHorse()Z as
a pure m_cHorse != null read inherited by Ray/Billy. The bridge resolves this
predicate on the verified current local player every game input frame; validity
is separate from its value. CoJContextualShoulderState uses player generation,
observed mount state and available releases. Unknown state, owner replacement,
mount/dismount and context loss release the shoulder and require a fresh gesture.

On foot the default shoulder pulses action 39 once per fresh press. Mounted it
holds action 18 until release. The combined fallback is deliberately rejected:
PlayerController.CanExecuteInput rejects rider kick, but InputDigital.Translate
walks the target list, and HorseController admits action 39 through its inherited
kick consumer. The horse can reach Creature.PerformKick / WpnAttackHorseKick.
Player rejection therefore does not prove C harmless mounted. On-foot single
player CanRun normally denies run through m_bAllowRunInSingleGame == false,
but action 18 still changes run state and scripts may alter that permission.
F is delivered before the shoulder phase, then the predicate is read again:
F can mount/dismount synchronously, invalidating the earlier observation.
Player/mount changes clear manual crouch/Focus, queued commands and owned native
input history, requiring release before a fresh gesture. Failed neutral cleanup
keeps gameplay disabled until it succeeds. No gameplay permission is overridden.
JNI fault/recovery, partial delivery, generation transitions,
exclusive routes and releases are host-tested; campaign horse behavior remains
unaccepted physically.

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
overrides these checks. `GetSquintZoom` linearly interpolates from 1 to cached
`m_fCurrentMaxSquintZoom` using `m_fSquintFactor`; the native default maximum is
2. `BeingCamera.UpdateZoom` divides its tangent FOV by native zoom before atan.
The VR adapter copies those two finite bounded caches once per stereo pair,
scaling every asymmetric render-eye tangent by the same native factor. It keeps
runtime/UI optics and eye transforms unchanged and retains native entry/decay
smoothing. `GetBeingZoom` may invoke `CalculateZoom` and is not an observation
API. Missing/invalid caches or inactive gameplay use physical optics. Other
scope/cinematic zoom is deliberately outside this squint contract.
`ArmedPlayerBeing.GetDestinyMaxSquintZoom` averages ordinary weapon zoom values
into the maximum cache; the reader preserves that native result. Its
`GetSquintFactor` additionally combines bow concentration, and `GetSquintZoom`
switches to a separate scope result when `IsScopeVisible`. Therefore desired
and current `WeaponBow`/`WeaponRifleWinchesterScope` objects exclude this ordinary
focus projection. The two hand getters only read native state; no parameter
factory or scope getter is called. Lookup/owner ambiguity uses physical optics.
Reader and projection tests are host-tested. Ordinary R3 Focus is
operator-accepted for exercised use; bow/scoped optics and broader alignment,
comfort and recovery retain their own gates. The `_SQUINT` event or blur alone
does not establish magnification acceptance.

Exercised pause, pickup/carry/put-down, gun firing, hybrid reload, jump,
on-foot crouch, ordinary Focus and available wheel switching retain their scoped
operator acceptance. Former-layout logs remain a baseline observation, with no
default Sense binding now. Mounting, kick eligibility and broader equipment
contexts remain separate gates; dispatch alone does not validate them.

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
Gameplay HUD recovery covers essential text and selected native wrist feedback.
It does not reproduce the complete native graphical HUD.

### Essential text owners

The essential text route is **implemented / host-tested**; floating dialogue
and encountered HUD elements have operator acceptance, while unencountered
tutorial/context transitions and broader readability remain pending. `HUDManager.sm_cMainHUDManager.m_Being` must identify the
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
performance still require live acceptance. Hint/interaction copies adapt isolated
LMB/RMB/SPACE BAR/SPACEBAR and explicitly delimited default campaign keys to Sense
labels, including complete Triangle hold/right-stick direction/release gestures.
The original templates use `%KEY(_ACTION_...)`; expanded text no longer carries
action identity. English key names and independently observed Spanish DirectInput
aliases are supported. Native strings, dialogue, bare letters/digits and unmapped
keys remain unchanged. Expansion preserves complete fitting gestures and UTF-16
pairs within the 1023-unit bound. Custom rebindings and other locale names remain
ambiguous/unadapted; native fading/animation styling is not reproduced. The copied
labels are host-tested, with live readability pending. Pausing hints and other
blocking UIs retain the existing flat UI/dismissal route.

### Read-only health and ammunition presentation

HUDManager's exact 23-component array supplies HUDPlayer at index 0,
HUDAmmoCounters at 1 and HUDWeapons at 7. Each component's `m_Being` and
`m_cHUDManager` must match the current player/manager; HUDPlayer's `m_cPlayer`
is checked separately. Actual manager/component/child visibility is mandatory.
HUDPlayer's `m_cHealth.m_sLocalizedText` already holds the game's normalized,
padded health percentage (including the original nonzero minimum and invalid
game-state blanking); no health calculation or UpdateHealth call is needed.

The optional wrist meter parses only this already validated visible numeric
cache, accepting up to three digits in 0–100. Shipped `HUDPlayer.UpdateHealth(FF)V`
calls `Tools.NormalizeValue`, multiplies by 100 and truncates, preserves one for
a positive fractional value that would truncate to zero, then pads the native
text. The meter adds no native health access or mutation. Percentages outside
the supported range retain numeric text without a meter; absent/hidden health
cannot borrow an ammunition or optional row. Green above 50, amber at 26–50
and red at 0–25 are VR presentation thresholds, not claims about native
`SetHealthState`, damage events or death. Reader/raster behavior is host-tested;
the added meter and text colors have no live/headset validation yet.

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
separate boundaries. Exercised card readability, ammunition and damage-driven
health updates are operator-accepted; broader transitions remain pending.

### Read-only stance and shadow presentation

The same owned HUDPlayer supplies exactly two `m_aPoses` windows: standing at
index 0 and crouched at 1. `UpdateData` shows one and hides the other, then caches
`m_bLastStanding`. Riding/using a cannon uses the native standing icon; this is
HUD posture feedback, not an independent classification of physical HMD height.
`m_bLastHidden` means `IsInShadow` with native recognition feedback enabled.
The game applies its cached `m_fAlpha` to both pose icons. Shadow does not prove
enemy invisibility, and false does not prove detection or even that recognition
feedback is enabled.

The wrist card adds a bounded posture line, with `En sombra` only for the positive
native shadow cache. Both windows must exist with the exact UIWindow type,
exactly one must be actually visible, and the visible index must match the cache.
Pending `m_bForceUpdate`, nonfinite/transparent/out-of-range alpha, unexpected
layout or JNI failure suppress this optional line without hiding health/ammo.
No pose, shadow, recognition or HUD update methods execute. Health/ammo retains
priority within the ten-row raster limit. Host ownership/fault tests and raster
composition establish host behavior only. Encountered HUD rows have operator
acceptance; unexercised posture/shadow transitions retain physical gates.

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
exercised wrist direction, readability, attachment and snap/recenter behavior
have operator acceptance. Broader objective contexts and native marker timing
retain separate live/physical gates.

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
presentation bounds, not gameplay range/permission tuning.
The sampled warning assessment records the rejection category, origin drift in
centimetres and normalized direction cosine. Unavailable/hidden native warnings,
wrong-hand ownership, unsupported reason, stale age and invalid geometry remain
distinct from origin/angular drift. These diagnostics do not relax either bound
or force a native trace.
The renderer draws an outlined red X at each matching captured eye point, preserving surrounding
world pixels. Unknown original class hashes still fail closed. The reader,
bytecode/JVM verification, geometry rejection, pixels and captured flag transport
are host-tested; native warning appearance/target transitions remain a visor gate.
The operator reports that the warning appears on the monitor but not in the
visor. The native-signal-driven outlined red weapon aiming cross is implemented and
host-tested in the captured-eye compositor. Its visor visibility and protected
target transitions remain pending; the earlier warning presentation was
physically rejected. No physical visibility fix is claimed.
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
JNI lifecycle, ray selection and labels are host-tested; physical whip use and
approximately aligned reticle are accepted for exercised contexts. Exact
clutch, length/release, collision reach and broader climbing remain open.

## Optional native wrist feedback

The fixed HUDManager component array supplies concentration at index 10,
countdown at 17 and horse at 18. Each read checks actual visibility and matching
inherited player/manager owners independently of health/ammo.
`HUDBulletTime.m_nMode` is committed after native icon selection: 1 running,
2 cooldown, 3/4 available and 5 player not ready; modes -1/0 hide the icon.
Copy only a visible typed icon with positive cached texture alpha and matching
`m_cPlayer`; do not invoke eligibility or time queries.

`HUDCountdownTimer.UpdateText` populates exactly one of bottom/center numerical
text owners, clearing the other. It presents `int(time_to_finish)+1`; copy the
existing digits, never recalculate seconds or infer drawing permission.
`LawmanModuleSingle.OnDuelReady` starts this timer with an empty title, selecting
the center owner; duel start stops it. Require the owned timer/window, exclusive
visible numerical text and valid text alpha; malformed or ambiguous text hides
only this optional row.

`HUDHorse.UpdateHorseData` caches fatigue in `m_fLastTiredness` on the native
0-100 scale. This is fatigue, not remaining stamina. Its inherited cached
`m_nHealthLevel` preserves native percentage/feedback lag; skip pending
`m_bUpdateHealthLevel` and reset values. Require the visible typed horse/fill
icons and positive bounded texture alpha. No horse update/getter is called.
Health/ammo retain ten-row priority, followed by countdown, horse,
concentration and posture/shadow. JNI exceptions clear independently. Reader
faults and combined raster are host-tested; live transitions/readability and
concentration/duel/horse input mechanics retain distinct gates.

## Current acceptance and remaining work

Essential hints/interactions/subtitles use read-only native owners and finite-depth
stereo presentation; encountered HUD elements and floating dialogue are
operator-accepted. Health/ammo, equipment wheel and objective wrist compass are
accepted in exercised contexts. Posture/shadow, concentration and countdown have
host tests and scoped acceptance only where encountered. Horse feedback and
special campaign transitions remain pending. Complete graphical HUD capture is
still planned; passive sprite/flush hooks do not establish usable alpha or live
capture coverage.

Remaining gates are the left-hand L1 route and held-only ring; convincing
manual-cartridge grip, exact loading-port clearance and broader manual recovery;
protected-target red weapon cross visibility;
retry/held-input recovery after the accepted death-screen visibility; broader
inventory permissions, other weapons and campaign mechanics. Full cylinder
loading, bow, scoped optics, dynamite, Bible, concentration, duels, climbing and
horses require their own native ownership and physical evidence. Kick is
inconclusive, not a demonstrated binding failure.

[VALIDATION.md](../VALIDATION.md#pc-mechanics-and-gameplay-hud-completeness)
owns acceptance; [ROADMAP.md](../ROADMAP.md#milestone-4--controller-ui-and-interactions)
owns priority. Source/manual findings and host tests do not promote physical
gates. Local settings, extracted classes/manuals and run evidence belong under
ignored `work/`.
