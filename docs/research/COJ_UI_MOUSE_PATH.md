# Exact CoJ menu mouse boundary

These findings apply only to the inspected Steam Call of Juarez (2006) x86
build. The executable SHA-256 is
`5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`,
ChromeEngine3.dll is
`DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8`,
and shipped code.pak is
`F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12`.
The inspected JVM is Java 1.4.2_10. No game Java classes or third-party mod
implementation are redistributed here.

## Visual position is not hover input

**Verified:** shipped `UICursor.SetPos(Vector)` invokes the native
`GameObject.SetCursorPos` and moves the cursor-point sprite. `OnMouseMove(FFI)`
only moves that sprite, and `UICursor.GetPos(Vector)` reads its visual position.
`GameUserInterface.SetProcessMouse()` calls `ProcessMouse(true)`, which enables
a flag rather than dispatching a mouse event.

**Observed / physically rejected:** automatic laser targets can match that
sprite readback without highlighting or selecting the underlying menu option.
A queued click which never becomes deliverable can also hold its target until
navigation cancels it. Successful JNI calls and an intervening Present do not
prove native hover processing.

## Native VR settings integration boundary

**Source/archive inspection; planned integration.** The provisional OpenVR panel
is a development surface, not an extension of the native menu tree. The original
recognized `code.pak` contains these existing routes (bytecode offsets are within
the named method, not native RVAs):

| Caller | Existing options route |
| --- | --- |
| `MenuMain.OnUICommand` | Offset 83 recognizes `ID_OPTIONS`; `OnOptions` offset 15 calls `ShowUI(1, Integer(GetUIID()))`. |
| `MenuInGame.OnUICommand` | Offset 134 recognizes `ID_OPTIONS`; `OnOptions` offset 16 calls `ShowUI(6, Integer(GetUIID()))`. |
| `MainMenuModule.GetUIClassName` | Explicit mapping: 0 `MenuMain`, 1 `MenuOptions`, 6 `MenuOptionsInGame`, 9 `MenuInGame`. |
| `MenuOptions.Show(Object)` / `OnBack` | Stores `m_nCaller`; `OnBack` offset 7 returns through `ShowUI(m_nCaller)`. |

The corresponding original `Data0.pak` members are
`Data/Menu/scr/Main.scr` (Options button, line 206), `InGame.scr` (119),
`Options.scr` (categories, 117), `OptionsInGame.scr` (categories, 72), and
`Data/Menu/UIDef/MenuButton.def` (command/focus/input properties, 21).
These are locations inside original archive members, not redistributed assets.
Native widgets already provide style, focus, selection and command notification.
The current hand pointer/trigger, accept/back and modal routing can supply input
to those widgets; sprite visibility or cursor echo alone cannot prove activation.

Adding a script button alone cannot implement a new entry: the Java consumers
recognize explicit command IDs. `MainMenuModule` also constructs a 56-entry UI
array (constructor offsets 35–41), with an explicit class-name table. There is
no demonstrated open-ended page-registration API. A new page must qualify
registry/capacity changes and every affected consumer; never reuse an apparently
empty index without checking its other owners.

The realistic route is a VR entry in both existing Options variants, reachable
from main and pause, leading to one native editor that preserves the caller and
back/modal behavior. Reuse native widgets and naturally scheduled sprite drawing;
do not replay UI traversal for the second eye. Native menus retain their existing
flat-theater presentation. Rendering, comfort/locomotion, controls/interaction
and HUD/cinematics are future groups, not permission to expose unimplemented
options. Initially only implemented preferences should appear.

`runtime/vr_settings` already supplies `CinematicsMode`, bounded validation,
version-1 load/save and atomic publication. The remaining coordination is in
`OpenVrStereoPresenter::Impl::Worker`: it initializes the panel's editable copy, checks panel
publication, publishes a changed value and saves it. External calls to
`PublishCinematicMode` or `SaveVrSettings` alone would not synchronize that copy.
There is no current multi-editor failure because the presenter is the only
coordinator. Preserve this working route now; extract one preference owner before
introducing the second UI. A value-only request/result bridge and synchronized
snapshot are a proposal, not existing JNI engine APIs.

Preserve these application/persistence contracts during that extraction:

- `%LOCALAPPDATA%/CoJVR/CallOfJuarez/settings.ini` is independent of staging and
  panel lifetime. Missing/invalid input defaults to theater; version 1 permits
  only its recognized keys and rejects duplicates/unknown versions. There is
  currently no migration. Extending the file requires explicit version-1
  migration that retains saved cinematic values; do not silently add keys that
  the strict old reader would reject or erase unknown future versions.
- Atomic temporary-file replacement retains the previous file on failed save.
  A session-applied value and its persisted status are distinct. The existing
  panel rolls back an invisible failed proposal before publication/persistence;
  preserve that safeguard without making preferences depend on successful panel
  rendering when a different valid editor submits a change.
- Runtime movie admission remains game-owned: immersive selection does not
  bypass pause, death, mission-end or unknown-owner fallback.

Native input requires an explicit owner split. The current
`SettingsInputCaptured()` suppresses both gameplay and injected native pointer
delivery. Reusing it unchanged for a native page would block editing. Preserve
gameplay suppression and available-release barriers while admitting pointer,
accept/back only to the verified native editor. Bind requests to its menu,
caller/modal and lifetime; the panel's player-generation/presentation token does
not identify a particular native page. JNI references stay on the game thread;
the renderer receives only values. Closing, modal replacement and context loss
must cancel pending requests without stale clicks or resumed held actions.

This is larger than a safe script edit. `patch_coj_weapon_consumers.py` currently
patches enumerated classes and verifies other archive members remain unchanged;
it supplies no native-settings bridge. The deployment already journals exact
`code.pak` and `Data0.pak` derivatives, so extend those verified transformations
and hash/inventory/restoration tests instead of overwriting game resources or
shipping original classes/scripts. Preserve their unrelated weapon/mesh patches.

Incremental exits: first prove the shared preference owner with existing panel
tests plus two-editor synchronization, version-1 migration and save-failure tests;
then qualify the native command/registry/resource bridge with main/paused callers,
modal/back, unknown-build rejection and byte-identical rollback fixtures. Only
then add the native page and batch its controller navigation/held-input checks
into a relevant canonical headset candidate. Keep the provisional panel usable
throughout; no duplicate preference file, new overlay or placeholder setting is
needed for this investigation.

## Native consumer and coordinates

The following are RVAs in the identified ChromeEngine image (size `0x60C000`):

| Boundary | Verified contract |
| --- | --- |
| `0xB21D0`, JNI GameObject.SetCursorPos | writes owning module mouse X/Y at `+0x12C/+0x130`; does not dispatch hover |
| `0xBF870`, native Java-object lookup | reads shipped `ThisID`; dereferences handle `+4`, then callers obtain native object from binding `+4` |
| `0xC6A70`, JNI Sprite.GetMousePos | casts GameObject to Sprite using `0x29CF4E`, RTTI `0x37E72C -> 0x37E75C`; reads input context `Sprite+0x180`, or owning module `+0xF4` |
| Input context | mouse X/Y at `+0x38/+0x3C`, button state at `+0x40`, capture at `+0x48` |
| Java GetMousePos output | X is native mouse X; Z is native mouse Y; Java Y is zero |
| `0xC63E0`, JNI Sprite.ProcessMouse | enables/disables native sprite byte `+0xCC` |
| `0xC8F00`, recursive sprite mouse movement | x86 thiscall: sprite, previous two-float point, new two-float point, button state; checks bounds/capture and dispatches enter/move/leave through the real sprite tree |
| `0xCC420`, native mouse-button dispatcher | x86 thiscall on the active input context: `(buttonIndex, pressed)`; left button is index `0` and production pointer selection sends press then release |

**Implemented / host-tested:** the adapter resolves the current UI, enables
mouse processing, obtains its live native handle, writes the actual input
context and invokes the shipped recursive movement dispatcher before the
visual cursor setter can overwrite the previous position. Reads use the
current UI's native `GetMousePos`, preserving the X/Z convention. Native calls
are available only after exact executable/engine identity and camera profile
validation; invalid handles, sprite type, context or coordinates fail closed.
No native object is retained across calls or menu transitions. Pointer lookup
uses non-creating `FindUI(m_nCurUI)`; observations read the existing `m_cCursor`
field. `GetCurrentUI -> GetUI` can load a missing UI, and `GetGlobalCursor` can
create a cursor, so neither factory belongs in passive observation.

Shipped menu ownership has two additional roots that matter for live input.
Inspection of the real `MainMenuModule.class` confirms `m_cYesNoDlg` exists and
`m_bYesNoDlgVisible` does not. `OnCreate` allocates the reusable dialog even while
it is hidden, so object existence cannot identify the modal input root. Modal
discovery now calls the dialog's shipped `IsActuallyVisible()` method and uses it
only when true; optional lookup failures are cleared before falling back to
`FindUI(m_nCurUI)`. During gameplay pause, the global `MainMenuModule` current UI
is the preferred root; the active-game menu remains a fallback. These rules are
covered by JNI fixtures and are **implemented / host-tested**.

Hover and activation are separate native operations. The laser already reached
the current UI through the `0xC8F00` movement dispatcher, but the old ray-select
path activated widgets by calling Java Enter. The current candidate obtains the
same live UI/native object, enables mouse processing, resolves its current input
context and calls `0xCC420` with left-button press then release. L2/R2 ray select
uses this native click path. Cross/global accept retains Java Enter; loading UI,
intro skip and pausing-hint routes retain their special handling. The button path
does not retain a native context or object across calls and remains exact-build
gated. It is physically accepted for exercised ordinary menus, visible Yes/No dialogs
and gameplay pause; broader navigation remains a separate gate.

Selection has a separate paused-hint precheck before resolving the current UI.
The shipped hierarchy is `LawmanModule -> Module -> GameObject`, while
`GameMode -> GameObject` declares `GetHintManager()LHintManager;`.
`LawmanModule` exposes `m_GameMode:LGameMode;`; it has no `GetHintManager` method.
Looking up that getter on the active module rejects selection once a campaign
module exists, even while pointer hover/readback succeeds. The adapter now resolves
the field on `LawmanModule` and the getter on `GameMode`, then follows the existing
hint manager. Null game-mode/manager objects permit ordinary selection; a pausing
hint consumes it through `DisableCurrentHint`. JNI host fixtures reproduce the
wrong-owner failure and verify corrected Enter delivery, modal delivery, hint
ownership and exception cleanup. The correction is **implemented / host-tested**.

A failed delivery/readback cancels only the matching pending click, freeing its
target for further automatic pointing. Hand/claim/click ownership still guards
stale completion. The full-UI verifier requires native-consumer telemetry and
rejects sprite-only echo even when its coordinates match.

The pointer mailbox now records source-image dimensions as part of its geometry
ownership. A changed width or height cancels pending selection and old hover
without changing native UI indices or mouse-priority policy. New coordinates
remain available for automatic pointing, but the first resized packet cannot
queue a click. A local generation also rejects an unlocked native cursor delivery
that returns after resize or neutralization; hand/claim/click alone cannot detect
this when both idle click IDs are zero. Direct proxy fixtures cover both size axes,
held/edge packets, stale selection, new hover/selection and the idle interleaving.
This correction is implemented/host-tested; physical resize recovery is pending.

## Loading continuation

The exact shipped `GameUILoading.OnInputKey(IZC)V` examines only the pressed
boolean: a true value schedules `OnUserInput` through `CallOnFrameEnd`; key-up
returns without scheduling it. The key code/character are not filtered. Its
mouse-button handler similarly requires a positive second argument.
`OnUserInput` unregisters exclusive input and starts the fade; the later fade
callback resumes the timer and starts the mission. Successful JNI delivery alone
therefore does not prove that loading continued.

Both global/Cross accept and pointer/trigger accept now send one direct native
key-down to an identified current loading UI. A loading-only request rejects
ordinary menus; paused hints, mission-end and ordinary menu activation retain
their own routes. This direct UI handler does not set global held-key state, and
one call schedules at most one continuation callback. JNI host fixtures cover
both callers, loading-only admission, single positive delivery, exception cleanup
and absence of generic Enter/mouse fallback. This correction is host-tested;
controller-only continuation with Cross/triggers still needs a physical check in
the next relevant combined candidate. The operator reports that Circle/Options
continue the exercised loading screens through their Escape press/release route.

## Evidence limits

**Host-tested:** an independent local x86 probe maps the identified DLL without
running its initialization and calls its actual `0xC8F00` code against synthetic
root/button sprites. Coordinate writes alone produce zero events; dispatch
produces child enter, move and leave, and honors a disabled root. JNI fixtures
separately exercise the production bridge, input-versus-sprite discrimination,
dispatch failures and reference cleanup. These probes do not execute the live
Java menu's focus/selection behavior.

**Headset-validated:** first-level main-menu hover/highlight, selection and
physical mouse takeover/resume through the native event route. The corrected
GameMode hint precheck and native `0xCC420` pointer-click route are also physically
accepted in the exercised ordinary menus, visible Yes/No dialog and gameplay
pause menu.

Loading reaches gameplay in exercised sessions; Circle/Options have an operator
continuation report, while the corrected Cross/trigger route remains a separate
acceptance boundary. Operator confirmation remains required even when
transport and native input telemetry pass. Exact native boundaries must not be
generalized to another Chrome Engine build or another game.
