# Validation

Validation states are intentionally strict:

`planned` -> `implemented` -> `host-tested` -> `live-tested` -> `headset-validated` -> `supported`

A higher state requires direct evidence for that boundary. Static disassembly, source existence, CTest and a successful build cannot substitute for a physical acceptance gesture.

Per-run logs, process IDs, videos, raw telemetry and evidence packages are local working data and belong under ignored `work/`. This document records only durable acceptance state and the current physical gate.

## Current physical gate

The D3D9Ex shared-texture implementation is experimental and host-tested. Two
physical Ex attempts completed three Presents and stopped before videos or
shared transport. A host-tested `GetDirect3D` hook addresses the factory COM
identity mismatch observed in the second attempt, but exact-game startup is
still unverified.

A separate classic-D3D9 gameplay run loaded a save, ran about 42 seconds and
rendered 4,311 frames. The probe observed two successful `D3DPOOL_MANAGED`
texture creations, proving that a verbatim Ex device is incompatible. It
captured only three creations total, so pool prevalence and complete resource
coverage are unknown. The current MANAGED-to-DEFAULT texture translation does
not establish preservation of system-memory backing, locking or reset
semantics; managed vertex/index-buffer requests were not measured. The next
gate is a complete classic-D3D9 resource census and a decision on the required
emulation before further physical D3D9Ex transport work.

Locomotion diagnosis is closed. Physical measurements established that native
normal movement, the walk modifier and jump are not materially reduced in VR.
The decisive comparison was `139.949 Hz` vanilla versus `138.117 Hz` with both
VR eye renders active and only readback/copy disabled. The old full transport
ran at about `83.473 Hz`. This causally localizes the perceived slowdown to the
old capture/presentation boundary; movement, input and physics are not an active
remediation target.

Independent stereo, tracking, recenter, snap-turn and locomotion/jump parity
remain valid. The separate menu-pointer, arm-continuity, weapon-alignment and
shutdown product gates also remain open, but are outside this transport gate.

## Deferred D3D9Ex startup gate

After the resource census and architecture decision, one fresh `prepare -StartupOnly`, one Call of Juarez process, one `finish`.
Observe startup videos and the main menu, then close the game normally. Do not
enter gameplay or validate VR in this run. The startup verifier requires the
game-visible Ex factory/device identity, at least 90 successful Presents, flat content
capture and proxy finalization. It does not promote shared transport or headset
state. If startup succeeds, the separate transport acceptance gesture below
becomes the next gate.

### Deferred transport acceptance

| Area | Required observation |
| --- | --- |
| Startup | process reaches flat theater without a D3D9Ex/device crash |
| Native stereo | both eyes show distinct, correctly paired current images |
| Tracking/pose | normal head rotation/translation remains stable with no pull/snap-back |
| Minimal gameplay | a few seconds of ordinary movement remain responsive; do not repeat the locomotion battery |
| Cadence | update/stereo-pair rate clearly exceeds the old approximately 83 Hz path and approaches the 138 Hz readback-off reference |
| Transport | shared frames and D3D11 copies advance; producer/consumer wait stay zero; no classic fallback, readback, eye mismatch or ring exhaustion |
| Shutdown | game closes normally, presenter reports `shutdown_complete=true`, pending GPU copies drain without abandoned leases, and transport/presenter summaries are emitted |

Failure in one area does not promote that area, but independent observations may still be recorded.

## Closed locomotion diagnosis

The retained diagnostic tools may be used to report update/stereo cadence for a
transport run, but the comparative locomotion protocol is complete and must not
be repeated as an input-tuning exercise. Durable physical results are:

| Mode | Update Hz | Stereo-pair Hz | Normal / walk cm/s | Jump |
| --- | ---: | ---: | ---: | --- |
| Vanilla | 139.949 | n/a | 503.056 / 256.109 | 58.7-60.8 cm, 0.77-0.78 s |
| VR full, old CPU transport | 83.473 | 83.502 | 498.451 / 248.325 | 3 observed |
| VR input-off | 70.692 | 70.726 | 489.064 / 247.893 | 62.623 cm average, 0.781 s average |
| VR readback-off | 138.117 | 137.705 | 499.113 / 248.360 | 3 observed |

These measurements rule out native movement speed, native jump amplitude,
duplicated gameplay-input ownership and the second eye render as the primary
cause. The old per-frame GPU-to-CPU transport is the demonstrated cause.

## Current feature state

| Contract | State | Durable limit |
| --- | --- | --- |
| Exact camera -> view/projection -> renderer path | live-tested | camera path reaches the renderer used for physical stereo |
| Complete two-eye ChromeEngine render | live-tested | distinct eye rendering and SteamVR submission observed |
| Physical eye scale | headset-validated | validated at the game/XR unit boundary |
| HMD yaw/pitch/roll and positional offset | headset-validated | exercised repeatedly in stereo gameplay |
| Explicit render-pose submission | headset-validated | removed the previous head-turn pull/snap-back artifact |
| Recenter | headset-validated | controller recenter exercised physically |
| Exact snap turn | headset-validated | controller turn exercised physically |
| Flat-theater startup/load -> native stereo | headset-validated for exercised path | startup and level transition reached gameplay safely |
| Local head/hair suppression | headset-validated for HMD view | shadow behavior remains separate |
| D3D9Ex GPU-resident native-stereo transport | host-tested / blocked on startup gate | shared DEFAULT-pool eye textures and nonblocking steady-state D3D9/D3D11 queries pass host interop; exact-game/headset stability and cadence pending |
| Classic-D3D9 resource pool census | live-exercised / incomplete coverage | DX9 gameplay shows successful MANAGED 2D and cube textures; only three creations were captured, so prevalence and full emulation scope remain unknown |
| Classic-D3D9 CPU transport fallback | implemented / unpromoted | retained only when D3D9Ex/device sharing is unavailable; any use during the physical gate is a failure |
| Inner presenter finalization | open | recent physical testing exposed incomplete shutdown behavior |
| Native analog locomotion | headset-validated diagnostically | normal/walk speed matches vanilla within the measured transport runs; do not retune input or movement |
| Native jump action | headset-validated diagnostically | measured apex/duration matches vanilla; do not retune jump or physics |
| Locomotion divergence instrumentation | host-tested and physically completed | retained for cadence reporting; causal investigation is closed |
| Room-scale visual body compensation | live-exercised technically | vertical pelvis drift was removed; visual walking parity remains open |
| Physical-walk visual animation | planned/open | should reuse native locomotion animation semantics without surrendering collision ownership |
| Sense tracking in game space | live-tested | left/right controller transforms reach the backend |
| Visible arm writer/restoration | live-tested | geometry changes and restoration are proven |
| Body IK continuity/anatomy | live-exercised / rejected | safety must not produce repeated visible fallback to default animation |
| Native reload ownership | host-tested | VR writes yield during native reload state |
| Flat-menu pointer | live-exercised / rejected | current ownership model remains unusable in-headset |
| Cross/Circle/L2/R2 UI actions | host-tested follow-up | physical acceptance still pending |
| Loading continuation | host-tested | native loading input route is mapped |
| Controller-origin UI beam | host-tested | physical usability still pending |
| Controller-owned weapon direction/visual origin | host-tested | final physical firing alignment still pending |
| Sense tip direction convention | live-tested diagnostically | local `-Z` is the demonstrated pointing direction |
| Temporary controller alignment ray | planned diagnostic | diagnostic only; must not become production ballistics |
| Physical gun origin/direction | pending/rejected | production shots must originate from the visible weapon/barrel |
| Supported end-to-end VR release | planned | project remains pre-alpha |

## Evidence policy

Only conclusions that remain useful across sessions belong here. Raw headset-run manifests, hashes, local video paths, telemetry counts, process IDs and agent handoffs stay under ignored `work/` and may be discarded once their conclusions are represented by code, tests or the durable documents above.
