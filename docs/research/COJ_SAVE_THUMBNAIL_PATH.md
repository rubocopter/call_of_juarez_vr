# Quick/automatic save thumbnails — exact CoJ 2006 boundary

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
and all four changed payloads are checked, and finish restores the original
archive byte-for-byte.

Quick/automatic saves have no newly generated preview while this archive is
staged, including flat presentation. The workaround is host-tested with call
boundary/fail-closed tests, archive comparison and the shipped Java 1.4 verifier.
Successful native save creation, loadability and pickup-through-autosave must
still be physically checked under the acceptance gate in `VALIDATION.md`.
