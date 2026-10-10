# Roadmap

The active target is original Steam Call of Juarez (2006), Windows x86, native
D3D9 stereo through OpenVR/SteamVR. PS VR2 Sense is the development reference.
[Validation](VALIDATION.md) owns acceptance and unresolved physical gates;
[Architecture](ARCHITECTURE.md) owns integration contracts.

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`,
`headset-validated`, `supported`. A working build does not imply visor support.

## Current gate

Native stereo/head tracking, exercised locomotion, independent hands/weapons,
menus, available Sense controls and encountered HUD/subtitles have scoped
operator acceptance. The opt-in persistent manual reload now also has scoped
headset acceptance for opening/closing, tracked hands, successive native unit
loading, insertion-only haptics and exercised weapon/menu recovery.
Recovered mission countdown, encountered notices and corrected green/red attack
directions now have scoped headset acceptance. Ray's Bible use and on-foot kick
are accepted in exercised gameplay; concentration and special mechanics remain
separate gates.

Immediate work, in order:

1. **Make the held cartridge convincing.** Preserve the accepted reload and
   textured CC0 .357. Its revised skin-derived anchor is host-tested and has
   bounded live delivery evidence, but the latest close-up does not establish
   finger contact. Qualify a safe hand/scene-occlusion boundary and a measured
   cartridge axis relative to displayed fingers; avoid further arbitrary offset
   tuning. A measured tangent axis is now implemented and host-tested for both
   hands; its visual fit is still unobserved. A bounded read-only D3D9 depth
   observer is ready to collect the next batch, without enabling occlusion.
   The current compositor has no native scene depth. Batch one physical
   grip/roll/release/insertion/recovery check after a material correction.
2. **Qualify the native loading port.** Existing read-only root/barrel/gate/drum
   observations show changed gate pose and independent drum motion. Displayed
   six-mouth pivot rankings have bounded live evidence, but a pivot is not a
   socket. Establish visible opening/clearance before enabling precise insertion
   or any independent mechanical writer. The current grip zone stays active.
3. **Close relevant recovery and interaction gates.** Square rearming across
   ownership/availability loss and conventional recovery after disable are
   strengthened and host-tested. Qualified zone-entry feedback is host-tested;
   native insertion retains its stronger independently gated pulse.
   Direct native completion/fresh opening clear old continuation tokens, and
   synchronous failed insertions yield presentation/supervision with accurate
   cancellation reasons; these corrections are implemented/host-tested.
   Batch manual tracking,
   held-control, save/load, death/retry and disable checks; broaden mirrored and
   Frontier coverage when available. The replacement left-Sense L1 ray and
   held-only cyan reference still need acceptance, independent of the earlier
   successful HMD-directed drawer interaction.
4. **Broaden campaign and performance coverage.** Mounted controls, other weapons,
   special optics/duels/climbing and optional HUD rows need eligible scenes.
   Investigate Steam recording-associated microskips and sustained transport
   recovery from evidence. Protected-target red-cross visibility is low priority.

No new test is required merely to reconfirm cartridge visibility, native unit
loading or already accepted Square/menu/weapon behavior. Unavailable campaign
actions and horse contexts remain pending/inconclusive; Ray's exercised on-foot
kick does not override native permission restrictions elsewhere.

A pending visual/physical gate blocks promotion of that specific behavior only.
Continue independent offline work on controls, UI, recovery and other qualified
boundaries while waiting; accumulate relevant checks into one substantial
candidate rather than requesting repeated small physical sessions.

## Next implementation gates

The reload sequence remains G1 then G2. G3-G5 are independent engineering work;
they can advance while native/physical evidence is unavailable. The exit criteria
below authorize implementation or host-tested state only. Runtime and visor
acceptance still require [Validation](VALIDATION.md).

| Gate | Actual starting state | Next implementation and prerequisite | Exit criteria |
| --- | --- | --- | --- |
| **G1 — cartridge occlusion and contact** | Skin-derived centre has bounded live evidence; displayed-finger tangent and read-only depth observer are host-tested. Hand/scene depth capture and occlusion are planned. | Qualify exact per-eye depth contents, hand coverage, projection, format and lifetime before adding renderer-owned capture/draw/transport. If scene depth is unsuitable, independently qualify a native-hand depth source. Keep hand-only and wall occlusion separate. | Both-eye ordering, exact capture/pose/generation correlation, scoped state restoration and reset/invalid-data rejection must have host coverage before a candidate. Then qualify measured pad contact, axis/scale and wrist roll in the consolidated batch; surface-layout metadata or offline skin replay cannot close it. |
| **G2 — native port and precise tip insertion** | Gate/drum and pivot ranking are live-observed diagnostics. Neutral socket policy is synthetic-tested but unwired in CoJ; active insertion remains the 14 cm grip zone. | Demonstrate visible READY/open clearance and a loading-mouth frame through actual drum phase. Only then supply a qualified game-adapter socket and cartridge tip/trajectory to the existing policy. Preserve false socket admission until qualified. | Stale/ambiguous owners, closed gate, wrong approach and misses reject. A deliberate aligned release calls native loading once, with loaded +1/reserve -1 and restored hand/weapon state. Do not infer chamber occupancy or enable mechanical writers/ejection from this gate. |
| **G3 — complete native graphical HUD** | Native sprite/flush observer is host-tested; text, compass and wrist cards are implemented. Independent countdown, encountered objective/journal notices and corrected green direction/red damage chevrons have scoped headset acceptance. Native graphical capture/composition is planned. | Qualify the naturally scheduled sprite pass, batch-flush lifetime, target ownership and usable alpha/coverage before renderer capture. Observe once; never replay traversal for the second eye. | Capture follows native visibility/settings, preserves callbacks/state, clears on owner/generation loss and composes coherently for both eyes. Synthetic raster tests precede live scheduling and visor readability acceptance. |
| **G4 — transport recovery and sustained pacing** | Normal transport/quit has scoped acceptance. Independent ordered consumer-query recovery is implemented/host-tested, including multiple pending leases, ResetEx and source/consumer replacement; failed recovery proof retains quarantine. Original errors remain visible to the canonical verifier. | Qualify exact-game reset/device-loss and session teardown/recreation when the independent proof is unavailable. Reacquire transport only after old GPU work is proven retired or its owning session safely destroyed. Measure recording and sustained frame-age tails before choosing a performance fix. | Host faults retain uncertain leases, reject stale generations and mismatched contexts, and defer replacement until retirement. Healthy shutdown retains matching copy/completion totals and zero abandoned leases. Native hook restoration, transition/recovery and sustained recorded/unrecorded pacing remain separate physical gates. |
| **G5 — persisted validation verdicts** | Implemented/host-tested; canonical rejection persistence is live-tested through collection and original restoration. Inventoried results bind source/build/run/profile, requirements and exact input hashes; operator observations remain separate. | Preserve rejection and stale-input fences. Exercise successful full-profile workflow coverage during the next relevant candidate; no dedicated headset session is required. Collection without verification remains inconclusive. | Host fixtures cover pass/fail/inconclusive, partial runs, stale identity, ZIP/hash integrity and real `finish` restoration. Live collection retains a failed verifier despite a complete run and scoped operator acceptance. Missing required gestures cannot become a pass through collection or notes. The report always leaves headset acceptance unevaluated. |

G1 depth metadata collection and G2 visible-clearance comparison join the existing
candidate; no separate probe session is required. Native loading-pose reuse is a
measured research option, not an authorized animation-time writer. Loading sound,
spent-case ejection, independent cylinder/gate control and chamber occupancy need
their own exact-game contracts after port qualification. Bow/scoped optics and
physical-walk animation remain planned integration beyond the accepted ordinary
Focus and native locomotion seams. Backend/game expansion remains Milestone 5.

## Next acceptance gates for implemented changes

The following work already exists and is host-tested. Include it in one fresh
canonical candidate instead of reimplementing it or requesting separate short
headset sessions:

- **Reload:** tangent fit, zone-entry cue, per-owner Square rearming, disable
  recovery, new-session waist pickup and synchronous insertion rejection. Check
  held triggers/Square across tracking/recenter/menu/weapon loss and available
  save/load/death; require no stale token, extra transfer, closing-press shot or
  permanent lock. Broaden left-hand/Frontier use when available.
- **Interaction and HUD:** left-Sense L1 ray independent of gaze, held-only cyan
  reference, pickup/carry/put-down, and wrist-meter changes after available damage
  or healing. Prior HMD interaction/text-card acceptance does not qualify them.
- **Global ownership:** menu -> gameplay -> wheel -> pause -> dashboard ->
  gameplay with held mapped controls, tracking loss and eligible source resize.
  Require fresh release/press, no delayed action/haptic replay and coherent eyes.

Mounted routing, campaign permission transitions, special weapons/duels/climbing and optional
HUD rows need eligible campaign scenes; missing observations are pending or
inconclusive. General LAA high-address compatibility, broad allocation pressure,
abnormal exit and device loss remain open even though an exercised loading
transition and normal quit are accepted.

## Stabilization baseline

Preserve exact-build SHA-256 admission, native inventory/permissions and scoped
camera/body/weapon restoration. The presentation remains head/connected arms
hidden, independent hands, native torso/legs. No new IK, arm reconstruction,
ammo replica, full physics simulation or backend migration is part of the
current reload iteration.

Canonical `prepare`/`finish` remains the reversible physical workflow. Keep
configured SDKs and recovery data; consume local evidence into durable findings
and remove obsolete large copies. A fresh run identity is required for each new
candidate, and the operator starts/closes the game and SteamVR.

## Milestone 1 — native stereo and HMD

Achieved in exercised contexts: per-eye rendering, tracked head, recenter and
room-scale motion. Finish broad loading/reset/transition coverage, sustained
pacing and recording compatibility before public support claims.

## Milestone 2 — player body and comfort

Exercised native movement/jump parity, head-relative locomotion, physical crouch
and corrected snap turning are achieved. Mounted camera continuity and broader
ownership recovery remain open; physical-walk animation is planned.

## Milestone 3 — tracked hands and body IK

Independent native hands/weapons with original torso/legs are active and accepted
where exercised. Expand left/dual/special weapon and finger-pose coverage.
Connected-arm IK remains a rejected diagnostic presentation, not a production
requirement or completed milestone.

## Milestone 4 — controller UI and interactions

Exercised menus, wheel/haptics, Focus, Create timing, compass and HUD/subtitles
are achieved. Recovered countdown/notices, corrected attack directions and Ray's
Bible/on-foot kick have scoped headset acceptance. Persistent manual reload has
scoped acceptance; convincing grip,
precise chamber loading, mechanical clearance and broader recovery remain open.
Spent-case ejection and complete graphical HUD capture are planned. Zone-entry
feedback is implemented/host-tested, awaiting physical acceptance.
Direct native unit loading is already implemented and scoped headset-validated
through the manual grip-zone gesture. Insertion-specific sound and precise
port/chamber loading remain pending; neither blocks that functional prototype.

Offline work outside the Peacemaker reload now includes a proportional wrist
health meter and low-health text colors, derived from the owned native HUD cache.
Both are implemented/host-tested with bounded invalid-data fallback; readability
and live percentage updates join the next consolidated gameplay acceptance batch.
The existing text card's headset acceptance does not qualify this new styling.
Global-button release barriers and failed-input rollback, partial stereo-submit
state recovery and pointer source-size/in-flight cancellation are also
implemented/host-tested. Physical focus/dashboard/tracking and eligible resize
recovery remain in the combined menu/gameplay gate.

## Milestone 5 — additional renderers and games

OpenXR is experimental; D3D10 is a separate future renderer. Bound in Blood and
Gunslinger are planned. Reuse game-neutral policy only; Chrome Engine behavior
requires a second independent game demonstration before generalization.

## Release direction

Remain pre-alpha without a public release until campaign coverage, recovery,
comfort and reproducible deployment meet their gates. Keep the landing concise
and describe only exercised achievements at a broad level; detailed limits belong
in Validation. Compatibility applies only to verified builds/devices/contexts.
