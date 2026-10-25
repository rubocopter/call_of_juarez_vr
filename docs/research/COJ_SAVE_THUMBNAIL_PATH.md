# Save and pause thumbnails — exact CoJ 2006 boundary

Applies only to the recognized original archive and ChromeEngine3 build pinned
by the adapter. Original `LawmanModuleSingle.class` SHA-256 is
`ded02549df857e99f25d47c34c5ec9c23f0718cad0dd46079db6818d0dcd3c47`.

`QuickSaveAuto` reaches `QuickSave(I)V`. That 270-byte method mutes sound,
renders, hides screenshot HUD sprites when requested, calls
`TakeScreenshot(String;II)` with 512x256, restores sound/sprites and serializes
through `SGController.Save`. The screenshot call is bytecode offset 102.

The 61-byte screenshot helper constructs the save-path `.tga` filename, removes
an existing preview, queues `Game.TakeScreenshot(String;II)` and calls
`GameObject.ForceRender`. Native handlers are ChromeEngine3 `0x1CFB90` and
`0xB16F0`. The screenshot branch enters tiled/readback rendering and allocation
through `0x1CEE90` and `0x27F73B`. An observed pickup-triggered autosave crashed
in this path with the C++ allocation exception thrown by MSVCP71 `_Nomemory`.
This identifies the failing optional capture path; it does not establish what
caused the resource pressure or accept broader renderer stability.

The staged exact-class patch redirects only that three-byte invocation to a
private helper. It retains the original screenshot helper's first 47 bytes
(filename construction and stale-preview deletion) followed by return.
QuickSave length, branches, exception/debug metadata and every other instruction
remain unchanged. Original screenshot methods are untouched. Archive identity
and the exact allowed changed payloads are checked, and finish restores the original
archive byte-for-byte.

Quick/automatic saves have no newly generated preview while this archive is
staged, including flat presentation. The workaround is host-tested with call
boundary/fail-closed tests, archive comparison and the shipped Java 1.4 verifier.
Exercised save/load with the optional thumbnail workaround has operator
acceptance; broader autosave-trigger and campaign coverage remain open. This
does not accept saving/loading during an owned manual reload or arbitrary
allocation pressure. [Validation](../VALIDATION.md) owns those limits.

## Pause entry uses a separate capture path

The later observed Options-button crash enters `LawmanModuleSingle.LevelBreak`,
then `TakeScreenshotsOnLevelBreak`, inherited `LawmanModule.TakeScreenShotInGame`
and `GameObject.ForceRender`. Its native frames again reach ChromeEngine3
`0x27F72E` and MSVCP71; the recorded throw-info resolves to `std::bad_alloc`.
The save-only workaround did not cover this producer. This identifies the
failing optional allocation path; it does not establish why address space or
allocation capacity was exhausted.

The exact 110-byte `TakeScreenshotsOnLevelBreak` helper has two paths: the
in-game screenshot call at 30..33 and the fallback 512x256 capture/ForceRender at
63..80. Both are replaced with equal-length NOP spans, retaining the existing
profile calls, HUD hide/restore conditions, ClearInputQueue, branches and metadata.
`LevelBreak`, `DelayedLevelBreak` and `DelayedLevelBreakOnFrameEnd` are unchanged.

Two consumers must accompany that suppression. The exact
`MenuBackgroundModule.class` SHA-256 is
`020db4802269c6ea1329cd234f1e94dc821181592e4af7b534d305ac79835acb`.
Its no-argument `ShowScreenInGame` passes null to the unchanged native/Java
background cleanup path, releasing/hiding the optional screenshot without
constructing `UITexture` for `Out/save.tga`. Other overloads remain unchanged.
The exact `MenuSaveLoad.class` SHA-256 is
`110587e751d32f9127bc66767fff69b023d1a899cb709119e3cdf1e448f43e71`.
Its `SaveScreenshot(String)` retains destination filename construction and stale
preview deletion, but omits only the final CopyFile of the unavailable pause
image. Native serialization, save/load selection and overwrite confirmation
remain unchanged. All edits fail closed and restore with the archive transaction.

The resulting staged behavior has no captured pause backdrop or newly generated
manual-save preview. Existing quick/automatic-save acceptance does not qualify
this pause/manual-preview correction. Exact-byte preservation, both pause modes,
HUD/input cleanup and shipped JVM verification are host-tested; fresh pause,
resume, options entry, saving/loading and visor appearance remain physical gates.
