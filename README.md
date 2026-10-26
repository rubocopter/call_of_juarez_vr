<p align="center">
  <img alt="Call of Juarez VR" src="assets/CoJ-VR.png" width="100%">
</p>

<p align="center">
  <a href="https://github.com/rubocopter/call_of_juarez_vr/releases"><img alt="Latest release, including pre-releases" src="https://img.shields.io/github/v/release/rubocopter/call_of_juarez_vr?include_prereleases&amp;style=flat-square&amp;label=release"></a>
  <img alt="Platform: Windows" src="https://img.shields.io/badge/platform-Windows-0078D6?style=flat-square">
  <img alt="VR runtime: SteamVR" src="https://img.shields.io/badge/VR-SteamVR-1a9fff?style=flat-square">
  <a href="LICENSE"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support development on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

<p align="center">
  <strong>Step into the Wild West.</strong><br>
  Experience the original <em>Call of Juarez (2006)</em> from inside the world,<br>
  with stereoscopic vision, head tracking, motion-controlled weapons and physical interactions.
</p>

<p align="center">
  <a href="https://github.com/rubocopter/call_of_juarez_vr/releases"><strong>Download the VR mod →</strong></a>
  &nbsp;·&nbsp;
  <a href="#download-and-play">Install</a>
  &nbsp;·&nbsp;
  <a href="docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls">Controls</a>
  &nbsp;·&nbsp;
  <a href="#known-limitations">Limitations</a>
  &nbsp;·&nbsp;
  <a href="#documentation">Technical docs</a>
</p>

> [!IMPORTANT]
> **Experimental pre-alpha, intended for community testing.** The full campaign is not verified; crashes may interrupt progress. PS VR2 Sense through SteamVR is the only physically tested reference, in specific scenes. Other headset and controller profiles remain experimental.

## The experience

The goal is to make **Call of Juarez feel like a VR game** while keeping the original story, characters and world. Look and lean naturally, aim and fire with either hand, and interact with the world using motion controllers. Your in-game torso and legs remain visible beneath independent tracked hands.

| Experience | What it means |
| --- | --- |
| **A world with depth** | Two native game-rendered views, one per eye, with head tracking through SteamVR. |
| **Weapons in your hands** | Independent aiming, dual-wielded pistols, holstering and experimental manual firearm reloads. |
| **More than shooting** | Physical bow drawing and melee, picking up objects, an equipment wheel, readable HUD and controller-operated menus. |
| **The original adventure** | The 2006 PC game, with its story, characters, cinematics and original body presentation. |

The mod currently targets **only the first Call of Juarez (2006)**. It is a work in progress, with more interactions and campaign scenarios still being validated. See [what has been exercised](#features-in-the-headset) and [what remains unfinished](#known-limitations).

## Download and play

**[Download Call of Juarez VR v0.1.0-prealpha.6 for Windows](https://github.com/rubocopter/call_of_juarez_vr/releases/download/v0.1.0-prealpha.6/CoJ-VR-0.1.0-prealpha.6-windows-x64.zip)** · [Release notes & checksums](https://github.com/rubocopter/call_of_juarez_vr/releases/tag/v0.1.0-prealpha.6)

You need the **original Steam PC game**, Windows 10/11 x64, SteamVR, and a PCVR headset. The mod does not include the game.

**Virtual Desktop:** use its SteamVR route for this OpenVR build. Selecting
VDXR does not switch the mod's backend; a direct OpenXR/VDXR path is not implemented.

1. Back up your saves, launch the original game in **Direct3D 9** once, then close it.
2. Extract the **player ZIP** (not GitHub's automatic source ZIP) and open **`CoJ-VR.cmd`**.
3. Choose **1 — Install/update**, point to the game directory and follow the prompts. Start SteamVR manually first if using an Auto or percentage resolution profile.
4. Launch the game manually in **D3D9 mode**. For later sessions, run **6 — Prepare session** from the mod launcher before playing.

No compiler or development tools are required. The launcher includes resolution profiles, controller help, diagnostics and recovery. Full instructions are below for those who need them.

For testing, use the [current validation scope and focused checklist](docs/VALIDATION.md#pending-headset-checks). The [roadmap](docs/ROADMAP.md#development-backlog) separates implemented work from remaining development.

<details>
<summary><strong>Detailed installation, updates and resolution settings</strong></summary>

<br>

**v0.1.0-prealpha.6** is an incremental community-testing release, cumulative from .5. It expands experimental manual loading, adds scoped Winchester support and revised optics, binding-aware hints and reload assistance, and optional graphics/horse comfort controls. It also revises pause/movie input, rifle triggers, mounted steering and recovery. Physical bow drawing, holsters, duels and continuous body following are retained from .5. See the [complete changes and validation limits](release/RELEASE_NOTES.md).

**Experimental manual reload:** the option now admits 15 exact firearm classes, including long guns, selected-chamber loading and Schofield whole-clip loading. New paths have host tests, with only limited revolver headset evidence; this is not acceptance of every model. Use launcher **8 — Manual reload on/off** for conventional reload.

### Detailed quickstart

You need **Windows 10/11 x64**, the **original Steam Call of Juarez (2006), D3D9 mode**, SteamVR and GPU/headset drivers. Unknown game builds are rejected. OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

1. **Back up your saves.** Run the original game in D3D9 mode once, then close it.
2. Download and extract the complete **Windows player ZIP** above. Keep its folders together; GitHub's automatic source-code ZIP is for development.
3. Start **SteamVR manually before Install/update** for Auto or percentage profiles. **Auto** is used when no resolution preference is saved.
4. Double-click **CoJ-VR.cmd**, choose **1 — Install/update** and select the game folder containing `CoJ.exe`. Stereo, independent hands, native torso/legs and physical Hands combat are enabled. Experimental manual reload defaults on for a fresh installation; **8 — Manual reload on/off** changes it while the game is closed.
5. Installation prepares the first session using the saved profile. Resolution selection is optional: choose **7 — Resolution profiles**, then **6 — Prepare session** only if you change it afterward. Start the **D3D9 game manually**.
6. Before **every subsequent play session**, close the game, start SteamVR manually for Auto/percentages, optionally change the profile with **7**, then choose **6 — Prepare session** for fresh provenance. It retains installed features. Start the game manually; the launcher never starts, closes or changes its focus.

No compiler, Git, system Python or PowerShell 7 is needed. The package bundles Python and the VC++ x86 runtime for offline installation and uses Windows PowerShell 5.1.

To update, close the game, extract a newer player ZIP, start SteamVR manually for Auto/percentages and choose **1 — Install/update**. Explicit resolution preferences and the installed manual-reload on/off choice persist. **8** changes reload transactionally; **9 — Controls and startup help** explains launch, pause and save-menu controls for each packaged profile. Installation prepares the updated session. Use **2 — Uninstall/recover** to restore original files and keep backups/recovery data until it succeeds. For **Access denied** in a protected game folder, run **CoJ-VR.cmd** as administrator and retry. **5 — Verify package** checks package files; it does not prepare a session. [Full package instructions](release/README.txt).

### Image quality

**Image resolution:** The player package includes [saved startup profiles](docs/research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md): **Preserve, Auto, 75%, 100%, 125% and Manual** width/height per eye. With the game closed, use **CoJ-VR.cmd → 7**, then **6** to apply a change. Missing preferences default to **Auto**; all explicit saved choices persist. Auto and 100% use the **SteamVR helper's recommended per-eye extent, not panel-native pixels**. Percentages scale total pixel count while approximately preserving aspect ratio. Start SteamVR manually before Install/update or Prepare session queries it. If the helper is unavailable or caps are incompatible, preparation keeps the selected game size and explains why; it retains profile intent and retries on the next preparation. Background/Scene recommendation identity remains unqualified.

**Validated local reference:** Manual **2804×2860 per eye** has measured native raster/capture/output dimensions and repeated positive PS VR2 operator feedback. That exercised profile needs no repeat acceptance test; a later candidate may use a different startup size. Auto does not impose that size on other users. Other profiles remain experimental. Higher sizes can increase GPU and x86 memory pressure; recommendation equivalence, sustained timings and broad recovery remain separate [validation limits](docs/VALIDATION.md#image-quality-profile-acceptance).

Profiles change native rendering at startup, including menu/theater sources, and never resize a running game. There is no new upscaler. At **2804×2860**, the accepted main-menu GPU presentation preserves its angular reference, proportions and native pointer coordinates; this targeted correction is not enabled for other sizes. SteamVR's slider alone does not enlarge the game's native render targets.

**Optional image controls:** native **Options > Game** adds sharpening, diagonal-edge smoothing, contrast, saturation, gamma and mounted peripheral comfort. Effects default off and have host tests only; visual quality, comfort and sustained cost still need headset checks. The DLAA/DLSS selections save a request but leave game rendering native. For this player package, use launcher **7**, then **6** for startup resolution; the native menu's saved render-scale request does not replace the launcher's profile.

</details>

## Features in the headset

Already exercised on PS VR2 Sense in specific scenes:

- **Native stereo and natural movement:** tracked head, physical crouch, smooth/snap turning and a visible native body with independent tracked hands.
- **Weapons you handle:** independent pistol aiming, physical bow drawing, melee, virtual holsters and the corrected two-hand Remington grip.
- **Manual revolver loading:** open with Square, pick up ammunition with the free trigger, carry it to the weapon and release. Earlier single-round loading and base Schofield whole-clip loading have scoped functional feedback; native ammunition remains authoritative.
- **The original adventure in VR:** controller menus, equipment wheel, encountered HUD/subtitles and exercised cinematic recovery, including Hope. Native VR options select cinematic mode, turning and guides.

Acceptance is limited to exercised scenes. Cartridge contact/chamber mechanics, other long-gun states and full campaign coverage remain unfinished.

**Additional scoped feedback since .5:** scoped Winchester two-hand grip, R2 firing and the large scope/reticle with controller aiming have been accepted on PS VR2 Sense in exercised scenes. Mounted Gatling firing and right-stick X/Y traversal are also functional in one section, though precise aiming remains difficult. Neither result establishes broader campaign or headset support.

**Manual reload in .6:** Winchester/scoped Winchester/Volcanic load individual rounds with a native animation cycle; Remington/SawOff/Derringer/Lemant load one selected chamber at a time; Schofield base/A/B use deliberate whole-clip loading. Base Schofield has functional operator feedback for loading the missing amount. Its new textured cartridge bundle and opening-trigger correction remain host-tested only, as do the other new model paths. Finger contact, physical ports, ammunition appearance and interruption recovery are provisional. [Exact profiles and instructions](release/README.txt).

**Still awaiting headset checks:** render-only scope changes did not eliminate inverted head-yaw response. The new correction also suspends head-driven body yaw while scoped so angular aim follows the rifle; it is host-tested only. The ordinary Winchester's corrected R2 fire route was not retested in the latest session. Manual rifle loading, horse steering/recovery, ammunition assistance, reload coaching and immersive skip retain their own pending checks. [Current status and remaining checks](docs/VALIDATION.md#pending-headset-checks).

<details>
<summary><strong>PS VR2 Sense controls, manual reload and special actions</strong></summary>

<br>

<p align="center">
  <img alt="PS VR2 Sense controller control reference" src="assets/CoJ_sensevr.png" width="85%">
</p>

### Controller guide

Current PS VR2 Sense defaults:

| Control | Action |
| --- | --- |
| Left stick / L3 | Move / crouch toggle |
| Right stick / R3 | Smooth turn (duel: draw/raise/aim; Gatling: X/Y aim) / Focus zoom |
| L2 / R2 | Left/right native hand action; manual reload uses the free-hand trigger |
| L1 / R1 | Interact along left-hand ray / kick or mounted gallop |
| Square / Cross / Circle | Reload / jump / alternate fire |
| Hold Triangle | Equipment wheel; select with right stick and release to confirm |
| Create tap / hold 800 ms | Objectives / recenter |
| Options | Pause on a fresh press; native movie skip when permitted |

PS VR2 Sense defaults for .6. Options pauses on a fresh press; holding it adds no quick panel. For manual reload with an admitted firearm and empty support hand: **press Square to open → press/hold the free-hand trigger at your hip to grab ammunition → bring it to the gun and release → repeat if applicable → press Square to close**. Release L1 support before loading and freshly regrip afterward. Schofield inserts the missing clip amount with one bundle; indexed weapons require Square preparation. Release triggers between actions, including after an empty-trigger opening. For exact model limits and other experimental controller profiles, see the [package guide](release/README.txt).

**Options > Game** exposes cinematic presentation, smooth/snap turning, weapon guides and experimental graphics/comfort choices, preserving saved preferences. Guides default off and also enable ammunition pickup markers and brief reload coaching. Fresh Options/Circle/Cross or trigger selection reaches the game's own skip consumer during immersive cinematics; native skip restrictions remain authoritative and physical verification is pending.

**Bow (retained):** aim with the left hand, bring the right hand to the string, freshly hold R2, draw back and release R2 to shoot. **Bimanual long guns:** with an idle right-held Remington, Winchester, scoped Winchester or SawOff and an empty left hand, freshly hold L1 near the wooden fore-end; release L1 to detach. Enable guides to locate contact. Square cancels support and prepares the selected reload mode; manual loading requires an empty support hand. Scoped grip/fire and the large R3 scope/reticle have scoped acceptance. Head-independent scope aiming, corrected ordinary Winchester R2 and manual rifle loading still need their applicable checks.

**Virtual holsters:** freshly press R1 at the right hip or L1 at the left hip for pistols; R1 above/behind the right shoulder selects/stows the long weapon, L1 above/behind the left shoulder the bow. Release between gestures. These zones use native inventory permissions and operate on foot outside duels; no visible holstered weapon models are added.

**Duels:** center the right stick on entry; place the right hand at the hip and pull down to draw when the game permits, then physically raise the hand and push up to raise the weapon. Aim with your tracked hand and use the triggers to fire. **Ray concentration:** holster both weapons and wait for native readiness, then make a fresh L2/R2 press. R3 is Focus/zoom; selecting fists is not holstering. With both pistols drawn, hold Triangle, highlight the right sector with the right stick and release to holster the right pistol; center and repeat left for the left pistol. Let both animations finish. **Blocking hints:** Cross dismisses; release before the next action. A PC Z prompt means Circle/alternate fire once gameplay resumes. Native tutorial windows still display PC keys.

PS VR2 with Sense through SteamVR is the only physically validated reference, limited to exercised scenes. Packaged Touch, Index, Vive wand and WMR bindings are experimental and do not establish support for another headset or PC. Custom SteamVR bindings are allowed. Adapted floating prompts use reported bindings where unambiguous and semantic fallback otherwise; native blocking tutorials still show PC keys. Binding-aware coaching/readability remains unqualified in the headset. [Detailed Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

</details>

## Known limitations

- A chest/possible-scroll interaction has caused a campaign crash. A native render-context mitigation is host-tested; the campaign fix and recovery remain unconfirmed. Progress can be lost.
- Full campaign cinematic coverage remains open. Immersive subtitle transport is implemented, with broader mode/timing checks pending; theater remains the default. Earlier intermittent full-scene flashes have not recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-control recovery, broader death/retry and save/load, device loss and abnormal exit remain incompletely tested. Normal quit acceptance does not establish crash recovery.
- Other weapons, mounted controls, duels, climbing, special optics, sustained performance and recording compatibility need broader campaign coverage.
- The large scoped Winchester R3 view/reticle and controller aiming have scoped operator acceptance. Render-only head-yaw isolation still failed; the new body-yaw freeze is host-tested only. Precise shot alignment, manual rifle reload and scope-exit recovery remain pending. Ordinary Winchester R2 firing is corrected after a fire-to-zoom report but was not retested in the latest session. Other weapon zoom sharpness is unresolved.
- DLAA/DLSS selections save a request but leave native game rendering active. There is no integrated game reconstruction or demonstrated performance gain. Optional spatial filters, expanded graphics menus and mounted comfort have host tests only.
- Manual reload has scoped single-round revolver and base Schofield whole-clip feedback. New model paths, the textured bundle, convincing cartridge grip, precise chamber loading and broader interruption recovery remain unqualified. Ammunition visuals are provisional, with no per-barrel port or indexed per-unit animation claim. Use launcher option 8 to turn manual reload off. The latest hand-directed interaction also awaits physical acceptance.
- Floating reload hints have a host-tested correction for icon expiry erasing a still-current instruction. Physical stability/readability still need confirmation; weapon guides/coaching can be disabled in Options > Game.
- Native tutorial windows still show PC keys. The long-gun support-hand correction applies in normal weapon states; firing/alternate/reload/transitions can retain native fingers.
- Community feedback confirms the retained .3 projection correction on Beyond and reports a good Quest view without specifying its model. Menu gaze alignment and broader headset/controller support remain unqualified. The rejected independent eye RT prototype and investigation probes remain off.

**v0.1.0-prealpha.6** is cumulative from .5 and earlier releases. See [release notes](release/RELEASE_NOTES.md) for changes since .5 and [Validation](docs/VALIDATION.md) for acceptance limits. The Pico 4/Virtual Desktop rhythmic-stutter report remains unresolved; reduced diagnostic work does not establish a performance fix. Archived packages and canonical verifier outcomes remain unchanged.

## Feedback

- [Reproducible bug](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=bug_report.yml): what happened, where and how to repeat it.
- [Compatibility, image quality or performance](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=compatibility_report.yml): your setup and what you observed, including successful tests.
- [Suggestion](https://github.com/rubocopter/call_of_juarez_vr/issues/new?template=suggestion.yml): what would improve your experience.

Logs are **optional**. **3 — Diagnostics** creates a local report for you to inspect and manually share; there is no automatic upload or remote telemetry. Remove private information before attaching reports, screenshots or clips. Do not share unreviewed saves or memory dumps.

## Documentation

[Documentation index](docs/README.md) ·
[Installation and player package](release/README.txt) ·
[Validation and limits](docs/VALIDATION.md) ·
[Roadmap](docs/ROADMAP.md) ·
[Architecture](docs/ARCHITECTURE.md) ·
[Release history](release/RELEASE_NOTES.md) ·
[Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls)

<details>
<summary><strong>Development</strong></summary>

Windows/x86 requires Visual Studio 2022 or Build Tools with C++ support and CMake 3.25+. Run [OpenVR bootstrap](tools/bootstrap_openvr.ps1) before configuring a fresh checkout; [OpenXR bootstrap](tools/bootstrap_openxr.ps1) supplies the optional experimental loader.

```powershell
cmake --preset win32-debug
cmake --build --preset release
ctest --preset release
```

Physical candidates use `tools/vr_test.ps1 prepare` and `finish`; add `-BodyIkAtStart -LargeAddressAware -ManualReload` for the manual-reload candidate. The operator starts/closes SteamVR and the game. Read [AGENTS.md](AGENTS.md) and the [validation workflow](docs/VALIDATION.md#evidence-policy) before integration work.

The distribution build uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -Version 0.1.0-prealpha.6` from clean committed source. It builds/tests the isolated OpenVR Release tree and emits the player ZIP plus SHA-256 checksum. The GitHub release workflow accepts a Version through `workflow_dispatch` and prepares a draft prerelease with separate developer symbols; review it and select Publish when ready. Update `release/RELEASE_NOTES.md` for each version. Package checks do not establish additional hardware validation.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).

Original project code is licensed under **[GPL-3.0-or-later](LICENSE)**. See [copyright, reuse and provenance](COPYRIGHT_AND_REUSE.md) for scope, attribution and third-party exclusions. Bundled Python, OpenVR and Microsoft VC++ runtime notices are listed in [THIRD_PARTY_NOTICES.txt](release/THIRD_PARTY_NOTICES.txt) and retain their own terms.
