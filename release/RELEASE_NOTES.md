# v0.1.0-prealpha.2 — Pre-alpha / Experimental Community Release

Cumulative changes since **v0.1.0-prealpha.1** for the exact original Steam
Call of Juarez (2006), D3D9/OpenVR build. Back up saves: campaign coverage is
incomplete and crashes can block progress. No feature is promoted to `supported`.
Only exercised PS VR2 Sense scenarios have physical acceptance; other hardware
and packaged bindings remain experimental.

## Highlights

- **Higher native per-eye resolution with saved startup profiles**, integrated
  into the player launcher. New installations default to Preserve.
- **Improved main-menu presentation and performance at 2804×2860**:
  the accepted GPU presentation retains the agreed angular reference, proportions,
  pointer and native coordinates. This targeted correction requires verified
  main-menu ownership and exactly that source size; other sizes retain the
  established presentation.
- **Fixed stereo recovery after cinematics**, including the previously reported
  Hope transition. The operator confirms headset acceptance across multiple
  exercised sequences, entry/exit, hot presentation-mode changes during sequences,
  different equipped weapons, headset recentering and continued gameplay.
- **Improved Ray body and coat presentation**, with the accepted 8 cm horizontal
  visual setback and scoped coat partition. Camera, collisions, hand targets and
  Billy's fit retain their established behavior.

## Fixes and improvements

- Recover unfinished stereo capture slots when presentation ownership changes
  before a pair completes, preserving pending GPU work and consumer leases.
  This prevents interrupted captures from permanently exhausting the ring.
- Preserve all 32 bits of native weapon/object identity across the Java/native
  boundary, including high-bit identities, for reload and physical Hands combat.
  This corrects erroneous signed-identity rejection; it does not establish general
  high-address compatibility for every native/JVM path.
- Correct Sense Cross and trigger selection on loading screens to deliver the
  native key press needed to continue. Host tests cover both routes and cleanup;
  their fresh headset continuation check remains separate from cinematic acceptance.
- Keep main-menu pointer mapping consistent with its source size and presentation.
  Ordinary native stereo remains the active render path.
- Add persistent profile selection, bounded SteamVR query and D3D9 limits helpers,
  and layered session-video recovery to Install/update and Prepare session.
  Preparation restores the previous override before applying the next; update
  preserves ManualReload and resolution intent. Uninstall restores the session
  layer before the installation originals and preserves conflicting edits.
- Extend package inventories, build manifests and offline verification to include
  resolution helpers and tools. The public menu retains its seven operations;
  no separate development selector is needed.
- Accept the UTF-8 marker that Windows PowerShell 5.1 can add to recommendation
  input, preventing valid Auto/percentage profiles from being rejected on those
  consoles. Strict JSON and size validation remain in place.
- Add opt-in render-resource and frame diagnostics with explicit generation
  correlation. These are internal investigation tools, disabled by default.
  The separate independent-render-target prototype remains off and unaccepted;
  it is not the resolution mechanism offered by this release.

## Resolution profiles

Extract the complete **player ZIP** and run **CoJ-VR.cmd**. With the game closed,
choose **7 — Resolution profiles**, then **6 — Prepare session** to apply a changed
choice. Installation already prepares a session using the saved profile: no
second preparation is needed unless you change it afterward. On subsequent
sessions, close the game, optionally change the profile, and choose 6.

- **Preserve (default):** retain the game's selected native resolution; no
  SteamVR recommendation query.
- **Auto / 100%:** use the current helper recommendation. Start SteamVR manually
  before Install/update or Prepare session needs to query it.
- **75% / 125%:** scale the recommendation's total pixel count, approximately
  preserving aspect ratio (each axis scales by the square root of the percentage).
- **Manual:** enter width and height per eye; no SteamVR recommendation is needed.

**Manual 2804×2860 per eye is the accepted local PS VR2 reference.** Prior
instrumented evidence establishes actual raster/capture/output dimensions. The
operator reports repeated satisfactory stereo, sharper detail and no perceptible
performance problems during the exercised tests. This is declared physical
acceptance, not a new telemetry measurement, universal default or sustained
performance guarantee. No repeat of these accepted tests is required for release.

Explicit profiles remain experimental. The helper's application identity is not
universally proven equivalent to the game's SteamVR recommendation. Query or
compatibility failures retain the selected native size through the established
Preserve fallback. Inspect the reported applied size. Higher dimensions increase
GPU and 32-bit address-space pressure; caps checks do not qualify total memory.
Profiles change native rendering, menu and theater sources at startup, never a
running game. There is no new upscaler or independent render-target bank.

## Installation and update

Use **1 — Install/update**, including when updating from prealpha.1. Extract a new
complete ZIP, close the game and retain all original backups/recovery data.
ManualReload remains optional and persists unless explicitly changed; resolution
intent also persists. **2 — Uninstall/recover** restores originals. Interrupted
preparations report recovery instructions; normal successive sessions need no
separate finish command. **3 — Diagnostics**, **4 — Status** and **5 — Verify
package** remain available. Reports stay local until manually shared.

Windows PowerShell 5.1, bundled isolated Python and the VC++ x86 redistributable
provide offline installation without Git, a compiler, global Python or PowerShell 7.
Users supply the exact game, SteamVR and GPU/headset drivers. Start and close the
game and SteamVR manually. Avoid switching desktop focus while playing: the
original game can close unexpectedly when it loses focus.

## Known limitations

- **Campaign:** full campaign and all-cinematic coverage remain incomplete;
  the accepted Hope/multiple-sequence recovery is no longer an unresolved gate.
  Mounted contexts, other weapons, special optics, duels, climbing and complete
  graphical HUD coverage remain unfinished or require broader acceptance.
- **Crash:** the chest/possible-scroll interaction crash remains unresolved.
  Diagnostics do not constitute a fix. Keep save backups.
- **Movies:** theater remains the default. Immersive subtitle transport is
  unfinished. Earlier intermittent full-scene flashes have not recurred in later
  exercised sessions, but their cause remains unresolved.
- **Recovery:** broader tracking/dashboard/held-input, death/retry, save/load,
  abnormal-exit and device-loss recovery remain open. Accepted cinematic mode
  changes and normal shutdown do not qualify these exceptional cases.
- **Interactions/reload:** the latest left-hand interaction ray still awaits
  physical acceptance. Convincing cartridge contact/occlusion, precise chamber
  loading, ejection, additional reload profiles and broad interruption coverage
  remain unfinished; manual reload is still an optional experimental feature.
- **Performance/hardware:** sustained pacing, recording-associated microskips,
  general allocation pressure, other profile sizes and other headsets/PCs need
  independent evidence. Positive local PS VR2 feedback does not close these gates.
- **Other tracks:** OpenXR, D3D10, Bound in Blood and Gunslinger are outside this
  player release. The independent-render-target experiment remains disabled.

Inspect local diagnostics for private information before manually attaching them
to an issue. Include package version, checkpoint, hardware and reproduction steps.

No project-code license has been selected. `THIRD_PARTY_NOTICES.txt` retains the
bundled dependency sources and license notices; it does not license project code.
