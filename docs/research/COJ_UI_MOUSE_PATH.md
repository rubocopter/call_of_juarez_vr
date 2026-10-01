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
`m_bYesNoDlgVisible` does not. The first modal fix incorrectly made that missing
visibility field mandatory, leaving a pending JNI exception and disabling the
otherwise proven laser/controller route. Modal discovery now treats a non-null
`m_cYesNoDlg` as optional root metadata and clears its own observation failure
before falling back to `FindUI(m_nCurUI)`. During gameplay pause, the global
`MainMenuModule` current UI is the preferred root; the active-game menu remains a
fallback. Both corrected rules are covered by JNI fixtures and are **implemented /
host-tested**.

A failed delivery/readback cancels only the matching pending click, freeing its
target for further automatic pointing. Hand/claim/click ownership still guards
stale completion. The full-UI verifier requires native-consumer telemetry and
rejects sprite-only echo even when its coordinates match.

## Evidence limits

**Host-tested:** an independent local x86 probe maps the identified DLL without
running its initialization and calls its actual `0xC8F00` code against synthetic
root/button sprites. Coordinate writes alone produce zero events; dispatch
produces child enter, move and leave, and honors a disabled root. JNI fixtures
separately exercise the production bridge, input-versus-sprite discrimination,
dispatch failures and reference cleanup. These probes do not execute the live
Java menu's focus/selection behavior.

**Headset-validated:** first-level main-menu hover/highlight, selection and
physical mouse takeover/resume through the native event route.

**Experiment-pending:** the corrected Yes/No dialog and gameplay pause menu. The
latest combined headset candidate did not exercise them because the invalid Java
field lookup disabled all menu/controller UI delivery. Loading through the mouse
then hung before gameplay/progress, so that transition must also be retested after
the JNI correction. Operator confirmation remains required even when transport and
native input telemetry pass. Exact native boundaries must not be generalized to
another Chrome Engine build or another game.
