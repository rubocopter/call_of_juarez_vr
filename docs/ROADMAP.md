# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`. [Validation](VALIDATION.md) owns acceptance and unresolved physical gates; this document owns development direction.

## Current gate

The active target is the original Steam Call of Juarez (2006), native D3D9 stereo through OpenVR. Core tracking, native movement, independent hands, exercised pistol/whip gameplay, menus, revised available Sense controls and encountered HUD elements have scoped operator acceptance.

Current priorities:

1. Validate the host-tested left-controller L1 interaction replacement. Drawers opened with the previous HMD reference, but its persistent cyan ring was uncomfortable; the new reference exists only while L1 is held.
2. Preserve the scoped headset-validated `-ManualReload` flow: Square opens/closes without automatic loading, hands remain tracked, successive trigger-carried cartridges load, insertion-only haptics work, and weapon/menu interruptions recover in the exercised context. Native right-hand Peacemaker unit transfers and scoped restoration have correlated live evidence. The next reload iteration should investigate a native loading-port socket using the existing root/barrel/gate/drum observer; enable precise insertion only after its geometry and clearance are demonstrated. Approximately 50-degree native gate motion and independent drum motion are measured, but a safe independent writer and individual chamber access remain unproven. Tracking/save/load/death/disable recovery, held-control edges, prolonged comfort and mirrored/Frontier contexts stay open; batch their physical checks when relevant rather than repeating the accepted basic flow. Full cylinder/ejection UX remains planned.
3. Cover death retry/back, held-control recovery, dashboard/loading transitions and newly available campaign mechanics. Death screen visibility itself is accepted.
4. Check the implemented protected-target red aiming cross when convenient. Original warning visibility in the visor remains unresolved and low priority.
5. Investigate Steam recording-associated microskips and sustained transport/resource recovery from evidence rather than arbitrary tuning.

Horse and broader weapon checks await available campaign content. Kick is inconclusive, not a confirmed defect. The default layout is maintained in [controls research](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

Manual insertion acceptance haptics have scoped headset acceptance and correlated
live submission records through the existing optional OpenVR output. They require
coherent native unit readbacks and fresh capture/ownership; no-replay guards are
host-tested, while broader physical recovery and prolonged comfort remain open.
Zone-entry feedback, insertion sound and direct loading of a native-game cartridge
mesh remain pending. The first CC0-inspired 16-surface visual and pinch integration
were physically rejected for appearance/contact and native loading-pose takeover.
The later full .44 OBJ also failed its physical appearance gate: capture events
identify the imported geometry, but it still used the placeholder color swatches.
The current correction has **scoped headset acceptance for visibility**: the
operator confirms the textured cartridge appears, but explicitly reports that it
does not look held between the fingers. It uses one CC0 .357 glTF cartridge with
source UVs/normals, cropped albedo, authored metal shading, bounded per-eye
batching and a new **host-tested and bounded live-tested**
skin-derived midpoint on verified displayed distal thumb/index bones. The
.7/.7 thumb/index pose and its guard use an offline exact-game Ray-skin reference;
live delivery is observed, while headset contact remains unaccepted. The latest
close-up still appears superimposed on the hand rather than convincingly held.
Native scene depth remains absent.
A scoped same-owner manual-recovery exception admits the support finger overlay; the
pinch requires an actually held trigger claim. While waiting without a round,
normal/rest fingers replace the native loading pose. Armed-hand and ammunition
ownership remain unchanged. Source/bake identity, ownership guards, release,
bone-anchor restoration and WARP stereo rendering have host checks. The new contact
anchor is a candidate correction; orientation/scale, appearance quality, comfort
and broader recovery remain unaccepted. Qualify that anchor, axis and finger
occlusion together after a material correction in one physical trial. Do not
repeat the current candidate merely to reconfirm its delivery. The compositor
currently draws the
round over the captured hand without native scene depth; anchor tuning alone
cannot establish convincing contact. The read-only pickup finger-reference
trace now has bounded live observations; its zero weapon-ID field cannot
independently establish weapon ownership. No new
physical trial is required merely to reconfirm visibility.
Original-game pickups remain unverified as loose revolver rounds; no proprietary
asset is distributed. The existing free-hand trigger pickup/release insertion zone
remains the active route.

The new `post_overlay` gate-pivot-to-six-mouth ranking and offline report are
**host-tested diagnostics**. They use observed native drum phase and owner
checks without affecting insertion. The gate element origin is not a verified
loading-port centre. The next specific reload gate is a single comparable
`READY`/sustained `MANUAL_LOAD` headset observation of the **visible** chamber
opening and clearance; only then consider a precisely located insertion zone.

## Stabilization baseline

Preserve exact executable SHA-256 admission, native permissions and transactional camera/body/weapon restoration. Primary transport uses D3D9Ex GPU sharing; normal shutdown has bounded acceptance. Broad device reset, abnormal shutdown, resource pressure and campaign coverage remain open. Canonical prepare/finish builds, tests and correlates each candidate without automatically starting the game.

## Milestone 1 — native stereo and HMD

Native eye rendering, tracked head, recenter, room-scale movement and exercised motion stability are achieved. Finish broad loading/reset/transition coverage and recording compatibility before public support claims.

## Milestone 2 — player body and comfort

Native analog locomotion/jump parity, head-relative movement, physical crouch and corrected snap direction are achieved in exercised contexts. Preserve them while validating mounted transitions, camera continuity and interruption recovery.

## Milestone 3 — tracked hands and body IK

Independent native hands and weapon mapping are active with the original torso/legs. Exercised pistol effects and whip use have acceptance. Expand left/dual/special weapons and finger-pose coverage. Connected arms/full-body IK remain future work; the earlier connected-arm diagnostic presentation is not the production baseline.

## Milestone 4 — controller UI and interactions

Automatic menu selection, pause, equipment wheel/haptics, Focus, Create timing, wrist compass and encountered HUD/subtitles are achieved. Finish the current hand interaction and reload gates, broaden native HUD/mechanics coverage and recovery. Full graphical HUD capture and complete physical cylinder reload remain planned.

## Milestone 5 — additional renderers and games

OpenXR is experimental; D3D10 is a separate future renderer. Bound in Blood and Gunslinger remain planned. Generalize only game-neutral contracts already supported by evidence; a Chrome Engine behavior needs independent confirmation in a second game before reuse.

## Release direction

Remain pre-alpha with no public release until campaign coverage, recovery, comfort and reproducible deployment satisfy their gates. Scope support to verified builds/devices/contexts. Keep the landing page concise and refer detailed limits to Validation.
