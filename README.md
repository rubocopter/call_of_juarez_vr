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
| **Weapons in your hands** | Independent aiming, dual-wielded pistols, holstering and experimental physical revolver reloads. |
| **More than shooting** | Physical bow drawing and melee, picking up objects, an equipment wheel, readable HUD and controller-operated menus. |
| **The original adventure** | The 2006 PC game, with its story, characters, cinematics and original body presentation. |

The mod currently targets **only the first Call of Juarez (2006)**. It is a work in progress, with more interactions and campaign scenarios still being validated. See [what has been exercised](#features-in-the-headset) and [what remains unfinished](#known-limitations).

## Download and play

**[Download Call of Juarez VR v0.1.0-prealpha.5 for Windows](https://github.com/rubocopter/call_of_juarez_vr/releases/download/v0.1.0-prealpha.5/CoJ-VR-0.1.0-prealpha.5-windows-x64.zip)** · [Release notes & checksums](https://github.com/rubocopter/call_of_juarez_vr/releases/tag/v0.1.0-prealpha.5)

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

The latest public version is **v0.1.0-prealpha.5**, cumulative from .4. It adds physical bow drawing, bimanual rifle support, virtual holsters, optional weapon guides, native VR options, physical duel posture checks and continuous body following. It also corrects input ownership, restoration and unwanted diagnostic work. See the [complete changes and validation limits](release/RELEASE_NOTES.md).

**Revolvers only:** long-gun manual reload is not available. Rifles and shotguns use the game's automatic reload animation.

### Detailed quickstart

You need **Windows 10/11 x64**, the **original Steam Call of Juarez (2006), D3D9 mode**, SteamVR and GPU/headset drivers. Unknown game builds are rejected. OpenXR, D3D10, Bound in Blood and Gunslinger are outside this prerelease.

1. **Back up your saves.** Run the original game in D3D9 mode once, then close it.
2. Download and extract the complete **Windows player ZIP** above. Keep its folders together; GitHub's automatic source-code ZIP is for development.
3. Start **SteamVR manually before Install/update** for Auto or percentage profiles. **Auto** is used when no resolution preference is saved.
4. Double-click **CoJ-VR.cmd**, choose **1 — Install/update** and select the game folder containing `CoJ.exe`. Stereo, independent hands, native torso/legs and physical Hands combat are enabled. Manual revolver reload defaults on for a fresh installation; **8 — Manual reload on/off** changes it while the game is closed.
5. Installation prepares the first session using the saved profile. Resolution selection is optional: choose **7 — Resolution profiles**, then **6 — Prepare session** only if you change it afterward. Start the **D3D9 game manually**.
6. Before **every subsequent play session**, close the game, start SteamVR manually for Auto/percentages, optionally change the profile with **7**, then choose **6 — Prepare session** for fresh provenance. It retains installed features. Start the game manually; the launcher never starts, closes or changes its focus.

No compiler, Git, system Python or PowerShell 7 is needed. The package bundles Python and the VC++ x86 runtime for offline installation and uses Windows PowerShell 5.1.

To update, close the game, extract a newer player ZIP, start SteamVR manually for Auto/percentages and choose **1 — Install/update**. Explicit resolution preferences and the installed manual-reload on/off choice persist. **8** changes reload transactionally; **9 — Controls and startup help** explains launch, pause and save-menu controls for each packaged profile. Installation prepares the updated session. Use **2 — Uninstall/recover** to restore original files and keep backups/recovery data until it succeeds. For **Access denied** in a protected game folder, run **CoJ-VR.cmd** as administrator and retry. **5 — Verify package** checks package files; it does not prepare a session. [Full package instructions](release/README.txt).

### Image quality

**Image resolution:** The player package includes [saved startup profiles](docs/research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md): **Preserve, Auto, 75%, 100%, 125% and Manual** width/height per eye. With the game closed, use **CoJ-VR.cmd → 7**, then **6** to apply a change. Missing preferences default to **Auto**; all explicit saved choices persist. Auto and 100% use the **SteamVR helper's recommended per-eye extent, not panel-native pixels**. Percentages scale total pixel count while approximately preserving aspect ratio. Start SteamVR manually before Install/update or Prepare session queries it. If the helper is unavailable or caps are incompatible, preparation keeps the selected game size and explains why; it retains profile intent and retries on the next preparation. Background/Scene recommendation identity remains unqualified.

**Validated local reference:** Manual **2804×2860 per eye** has measured native raster/capture/output dimensions and repeated positive PS VR2 operator feedback. That exercised profile needs no repeat acceptance test; a later candidate may use a different startup size. Auto does not impose that size on other users. Other profiles remain experimental. Higher sizes can increase GPU and x86 memory pressure; recommendation equivalence, sustained timings and broad recovery remain separate [validation limits](docs/VALIDATION.md#image-quality-profile-acceptance).

Profiles change native rendering at startup, including menu/theater sources, and never resize a running game. There is no new upscaler. At **2804×2860**, the accepted main-menu GPU presentation preserves its angular reference, proportions and native pointer coordinates; this targeted correction is not enabled for other sizes. SteamVR's slider alone does not enlarge the game's native render targets.

</details>

## Features in the headset

Already exercised on PS VR2 Sense in specific scenes:

- **Native stereo and natural movement:** tracked head, physical crouch, smooth/snap turning and a visible native body with independent tracked hands.
- **Weapons you handle:** independent pistol aiming, physical bow drawing, melee, virtual holsters and the corrected two-hand Remington grip.
- **Manual revolver loading:** open with Square, pick up a round with the free trigger, carry it to the weapon and release. Native ammunition remains authoritative.
- **The original adventure in VR:** controller menus, equipment wheel, encountered HUD/subtitles and exercised cinematic recovery, including Hope. Native VR options select cinematic mode, turning and guides.

Acceptance is limited to exercised scenes. Cartridge contact/chamber mechanics, other long-gun states and full campaign coverage remain unfinished.

**Current development source (after the public .5 release):** scoped Winchester two-hand grip and R2 firing have been accepted on PS VR2 Sense in one scene. Mounted Gatling right-stick X/Y traversal is also accepted in one section, though precise aiming remains difficult. Neither result establishes broader campaign or headset support.

**Manual reload in development:** an earlier original-campaign session could not carry or insert cartridges on the weapons tried. A subsequent long session recovered ten native round transfers on one revolver, confirmed by the operator. Other revolver profiles, convincing finger contact and interruptions still require checks. Winchester/Volcanic, double-barrel and Schofield experiments remain offline; these weapons use native Square reload in the game.

**Awaiting a headset check:** the previous scoped R3 view could not be fused comfortably with both eyes open. Revised binocular optics and a scope sight pass host tests but have not passed a new visor test; ordinary weapon zoom sharpness is also unresolved. Horse steering/recovery corrections, ammunition pickup assistance, binding-aware reload coaching, empty-reserve handling, immersive cinematic skip and diagnostic cleanup also await their applicable checks. These changes **are not included in the published .5 package**. [Current status and remaining checks](docs/VALIDATION.md#pending-headset-checks).

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
| Right stick / R3 | Smooth turn (duel: draw/raise/aim) / Focus zoom |
| L2 / R2 | Left/right native hand action; manual reload uses the free-hand trigger |
| L1 / R1 | Interact along left-hand ray / kick or mounted gallop |
| Square / Cross / Circle | Reload / jump / alternate fire |
| Hold Triangle | Equipment wheel; select with right stick and release to confirm |
| Create tap / hold 800 ms | Objectives / recenter |
| Options | Pause on a fresh press; native movie skip when permitted |

PS VR2 Sense default bindings for current development source. The held-Options quick panel has been removed; the published prealpha.5 package retains its earlier tap/hold behavior. For manual reload with an eligible revolver and empty support hand: **press Square to open → press/hold the free-hand trigger at your hip to grab a round → bring it to the gun and release → press Square to close**. For the other experimental controller profiles, see the [controls cheat sheet](release/README.txt).

**Options > Game** exposes cinematic presentation, smooth/snap turning and weapon guides, preserving saved preferences. Guides default off. Current source routes fresh Options/Circle/Cross or trigger selection to the game's own skip consumer during immersive cinematics; native skip restrictions remain authoritative and physical verification is pending.

**Bow:** aim with the left hand, bring the right hand to the string, freshly hold R2, draw back and release R2 to shoot. **Bimanual long guns:** with an idle right-held Remington, Winchester or SawOff and an empty left hand, freshly hold L1 near the wooden fore-end; release L1 to detach. Enable guides to locate contact. Square cancels support for native reload. In current development source, the scoped Winchester also admits L1 support and R2 fire in an exercised scene; its Square reload and revised R3 scope view still need separate headset checks.

**Virtual holsters:** freshly press R1 at the right hip or L1 at the left hip for pistols; R1 above/behind the right shoulder selects/stows the long weapon, L1 above/behind the left shoulder the bow. Release between gestures. These zones use native inventory permissions and operate on foot outside duels; no visible holstered weapon models are added.

**Duels:** center the right stick on entry; place the right hand at the hip and pull down to draw when the game permits, then physically raise the hand and push up to raise the weapon. Aim with your tracked hand and use the triggers to fire. **Ray concentration:** holster both weapons and wait for native readiness, then make a fresh L2/R2 press. R3 is Focus/zoom; selecting fists is not holstering. With both pistols drawn, hold Triangle, highlight the right sector with the right stick and release to holster the right pistol; center and repeat left for the left pistol. Let both animations finish. **Blocking hints:** Cross dismisses; release before the next action. A PC Z prompt means Circle/alternate fire once gameplay resumes. Native tutorial windows still display PC keys.

PS VR2 with Sense through SteamVR is the only physically validated reference, limited to exercised scenes. Packaged Touch, Index, Vive wand and WMR bindings are experimental and do not establish support for another headset or PC. Custom SteamVR bindings are allowed. The public .5 package retains its existing prompts; binding-aware coaching is part of the untested development candidate. [Detailed Sense controls](docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

</details>

## Known limitations

- A chest/possible-scroll interaction has caused a campaign crash. A native render-context mitigation is host-tested; the campaign fix and recovery remain unconfirmed. Progress can be lost.
- Full campaign cinematic coverage remains open. Immersive subtitle transport is implemented, with broader mode/timing checks pending; theater remains the default. Earlier intermittent full-scene flashes have not recurred in later exercised sessions, but their cause remains unresolved.
- Tracking/dashboard/held-control recovery, broader death/retry and save/load, device loss and abnormal exit remain incompletely tested. Normal quit acceptance does not establish crash recovery.
- Other weapons, mounted controls, duels, climbing, special optics, sustained performance and recording compatibility need broader campaign coverage.
- The scoped Winchester's previous R3 view required closing one eye to aim. The revised binocular projection/sight is host-tested only; comfortable fusion, sight clarity, shot alignment and campaign completion are pending. Other weapon zoom sharpness is also unresolved.
- DLAA/DLSS options in development source save a request but currently leave native game rendering active. GPU helper fixtures pass; game image reconstruction and performance gains have not been demonstrated.
- Manual reload has scoped revolver acceptance, but convincing cartridge grip, precise chamber loading and broader interruption recovery remain unfinished. Long guns retain automatic native reload and its original animation. The latest hand-directed interaction also awaits physical acceptance.
- Native tutorial windows still show PC keys. The long-gun support-hand correction applies in normal weapon states; firing/alternate/reload/transitions can retain native fingers.
- Community feedback confirms the retained .3 projection correction on Beyond and reports a good Quest view without specifying its model. Menu gaze alignment and broader headset/controller support remain unqualified. The rejected independent eye RT prototype and investigation probes remain off.

The latest published release is **v0.1.0-prealpha.5**, cumulative from .4, .3, .2 and .1. See [release notes](release/RELEASE_NOTES.md) for all changes since .4 and [Validation](docs/VALIDATION.md) for acceptance limits. The Pico 4/Virtual Desktop rhythmic-stutter report remains unresolved; reduced diagnostic work does not establish a performance fix. Archived packages and canonical verifier outcomes remain unchanged.

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

The distribution build uses `powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -Version 0.1.0-prealpha.5` from clean committed source. It builds/tests the isolated OpenVR Release tree and emits the player ZIP plus SHA-256 checksum. The GitHub release workflow accepts a Version through `workflow_dispatch` and prepares a draft prerelease with separate developer symbols; review it and select Publish when ready. Update `release/RELEASE_NOTES.md` for each version. Package checks do not establish additional hardware validation.

</details>

## Disclaimer

An unofficial community project, not affiliated with or endorsed by Techland, Ubisoft, Valve or Sony Interactive Entertainment. Game assets are not redistributed; the imported cartridge is [CC0](assets/models/README.md).

Original project code is licensed under **[GPL-3.0-or-later](LICENSE)**. See [copyright, reuse and provenance](COPYRIGHT_AND_REUSE.md) for scope, attribution and third-party exclusions. Bundled Python, OpenVR and Microsoft VC++ runtime notices are listed in [THIRD_PARTY_NOTICES.txt](release/THIRD_PARTY_NOTICES.txt) and retain their own terms.
