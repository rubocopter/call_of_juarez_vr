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
loading or already accepted Square/menu/weapon behavior. Unavailable horse or
kick actions are pending/inconclusive, not demonstrated binding failures.

A pending visual/physical gate blocks promotion of that specific behavior only.
Continue independent offline work on controls, UI, recovery and other qualified
boundaries while waiting; accumulate relevant checks into one substantial
candidate rather than requesting repeated small physical sessions.

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
are achieved. Persistent manual reload has scoped acceptance; convincing grip,
precise chamber loading, mechanical clearance and broader recovery remain open.
Spent-case ejection and complete graphical HUD capture are planned. Zone-entry
feedback is implemented/host-tested, awaiting physical acceptance.
Insertion-specific sound and direct native loose-round loading remain pending;
none blocks the existing functional prototype.

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
