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

**Source/archive inspection and implemented native editor.** Current source retires
the held-Options OpenVR panel. The native editor extends the existing Game
page. The original recognized `code.pak` contains these existing routes
(bytecode offsets are within the named method, not native RVAs):

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

The implemented route adds cinematic, smooth/snap, weapon-guide and nine graphics selectors to the existing
`MenuOptionsGame`, shared by both Options variants. This avoids a new UI registry
slot and retains original OK/Cancel/Defaults and return behavior. The recognized
class SHA-256 is `2FE3AA28003AA2C3F9539071EEFA040E4FAEDBDD6868FBD8C179AB0230BA6DD8`;
`Data/Menu/scr/OptionsGame.scr` is
`995388EDB69798167E318189598E451319D31C351B230455D737679608772B1D`.
The transformation clones the original selector resource at installation time,
retaining `MenuSmall`/scale, original options and buttons. Native
`UIOptionsControlSelect` supplies label, option data, selection, focus and tips.
The graphics extension compacts the whole native selector rectangles, including
their inherited label/option/arrow click extents: sixteen `.465 x .038` rows at
screen y `.15 + index*.041`, ending at `.803`. The noninteractive status occupies
`.805..855`; the enlarged decorative list ends at `.86166667`, before the original
`.87166667` separator. Original callback routes and font patterns remain native.
Host geometry checks do not qualify glyph fit or physical readability. No
original resource/class is redistributed.
Native menus retain naturally scheduled flat-theater drawing; no second-eye UI
traversal or new overlay is introduced. A Native/DLAA/DLSS Quality selector saves
the reconstruction request; explicit experimental tips and pending/native-active
status distinguish that request from an available game rendering mode. The
standalone NGX backend is not connected to current CoJ presentation.

`runtime/vr_settings` owns the three comfort preferences, configuration and atomic
publication through one versioned store. Post-.5 source removes the presenter
quick editor; the native bridge snapshots the authoritative values once per Show.
Native highlighting remains local; OK queues request 1, consumed as token 2
before application/save. Accepted completion requires that token and clears it
before original OK; Cancel/Back revoke readiness and pending requests. Failed
saves keep the page visible with session-only feedback. Revision conflicts
refresh the authoritative choices and require another deliberate OK. No native
references cross threads; this protocol is authored by the mod, not an engine API.

The shared owner preserves these application and persistence contracts:

- `%LOCALAPPDATA%/CoJVR/CallOfJuarez/settings.ini` is independent of staging and
  native editor lifetime. Missing/invalid input defaults to theater/smooth/guides off.
  Version 1 retains its cinematic choice and supplies smooth turning; version 2
  retains both choices. Both default guides off. The next accepted save writes
  strict version 3 with all values. Cinematic-only edits preserve other choices.
  Duplicate/unknown keys and
  unknown versions reject. Invalid/unreadable startup files remain protected
  for the session; an edit can publish but cannot overwrite them.
- Native synchronization uses `cojvrSyncSettings(IIII)V` for cinematic mode,
  turning, guides and language; `cojvrHolsterGuidesValue()I` reads the local choice.
  The comfort label and Off/On choices follow the game's startup `Game.ini`
  locale (English/Spanish, English fallback), using original widget fonts.
- Graphics uses a separate `cojvrSyncGraphics(IIIIIIIIII)V` and ready flag. Its
  arguments are enabled, sharpness percentage, contrast percentage, saturation
  percentage, gamma percentage, edge smoothing, reconstruction request, startup
  resolution scale, horse comfort and locale. Four numeric choices
  use bounded five-percentage-point steps; booleans/locale use 0/1. Both readiness
  flags and all twelve preference getters must qualify before a VR request is queued
  or original OK is completed. Missing/wrong widgets retain native fallback.
- `runtime/graphics_settings` owns strict version-4 `graphics.ini` (complete versions
  1/2 migrate with missing choices defaulted; v3 scales 0..3 retain meaning and
  removed Manual4 becomes Auto). Native scale options are Auto/75/100/125/150/200%
  pixel counts relative to a fresh HMD recommendation at next preparation. It owns per-store
  revisions and atomic replacement. Both owners' revisions are checked before
  applying a native proposal; separate file replacements are not a cross-file
  transaction. A failure of either save retains the editor and session choices
  for a deliberate retry. Unchanged quantized controls preserve precise legacy
  file floats. Native Cancel/default staging do not publish. A separate short
  publication lock supplies coherent graphics values at fresh capture boundaries,
  without renderer disk reads or the persistence lock. Repeats/theater/private
  bank remain outside image processing; terminal consumer loss cannot be revived.
- Atomic temporary-file replacement retains the previous file on failed save.
  A session-applied value and its persisted status are distinct. Native save
  failure keeps the editor visible with session-only feedback; no renderer
  overlay needs publication to apply a valid native edit.
- Runtime movie admission remains game-owned: immersive selection does not
  bypass pause, death, mission-end or unknown-owner fallback.

Native input retains an explicit owner split. Post-.5 `SettingsInputCaptured()`
is only the presenter's startup/focus/owner available-release fence. It suppresses
gameplay and pointer injection while controls are unavailable or awaiting neutral
input; it is not set just because the native options page is open. Native menu
ownership separately blocks gameplay and admits pointer/accept/back to its verified
editor. Closing, modal replacement and context loss cancel pending requests.

The original `LawmanModuleSingle.OnInputKey(IZC)V` checks key-down at PC0 and
Escape at PC5. `CanSkipMovie` combines `CanLevelBreakAndSkipMovie` (mission end,
blocking hint and precache guards) with `IsPlayingSkipableMovie`; the Escape
consumer calls `OnSkipableMovieSkip` only for that native scene. Post-.5 immersive
input forwards a fresh permitted request to this consumer, never directly invokes
skip/fade callbacks. The active movie and module must remain stable before dispatch;
the camera-only path must not swallow the request or grant gameplay authority.

The canonical code patcher now includes the hash-pinned menu class. The Data0
transformer adds its script through the same compressed-record-preserving
transaction as player partitioning; native stereo without independent hands
uses a menu-only derivative. The original code/Data0 hashes remain authoritative,
and the existing journal restores both originals byte-for-byte. Unrelated
weapon/mesh transformations remain included and independently checked.

Host fixtures execute transformed menu bytecode, original OK/back dispatch and
the JNI request bridge separately; they do not initialize the native widget
renderer. Native option usability and exercised guide assistance on/off have
scoped operator acceptance. Unreported persistence, save-failure feedback and
held-input recovery remain separate gates in [Validation](../VALIDATION.md).
The post-.5 panel retirement and immersive skip route are host-tested only;
accepted cinematic stereo recovery does not establish skip acceptance.

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

Main-menu presentation admission additionally requires current global `MenuMain`
type, its inherited `IsActuallyVisible()` getter returning true and a valid native
`GetThisID()`. Existence of `FindUI(0)` alone is insufficient. Exceptions, hidden
objects, active-game owners and modals retain the existing layout; native identity
changes cancel pending presentation pointer claims. No native menu layout or
mouse dispatcher is modified by this presentation-only observation.

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

Direct `OnInputKey` dispatch bypassed exclusive registration and could schedule
callbacks before readiness or repeatedly restart the native fade. The hash-pinned
loading derivative now exposes `cojvrContinueLoading()Z`: it requires native
`IsRegisteredKeyRequestExclusive()` readiness and an unset queued flag, sets that
flag before the original positive key handler, and refuses subsequent delivery.
Preserved `WaitForUserInput`, `Hide` and `ShowCommon` bodies reset this local flag
through wrappers; native continuation/fade/timer/mission callbacks are unchanged.
`cojvrLoadingState()I` distinguishes not ready, ready and already queued for JNI
diagnostics. Both global/Cross and pointer/trigger use this boolean helper.
A loading-only request rejects
ordinary menus; paused hints, mission-end and ordinary menu activation retain
their own routes. This direct UI handler does not set global held-key state, and
one readiness epoch schedules at most one continuation callback. JNI host fixtures cover
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
