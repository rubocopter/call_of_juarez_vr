# Roadmap

Status vocabulary: `planned`, `implemented`, `host-tested`, `live-tested`, `headset-validated`, `supported`. [Validation](VALIDATION.md) owns acceptance and unresolved physical gates; this document owns development direction.

## Current gate

The active target is the original Steam Call of Juarez (2006), native D3D9 stereo through OpenVR. Core tracking, native movement, independent hands, exercised pistol/whip gameplay, menus, revised available Sense controls and encountered HUD elements have scoped operator acceptance.

Current priorities:

1. Validate the host-tested left-controller L1 interaction replacement. Drawers opened with the previous HMD reference, but its persistent cyan ring was uncomfortable; the new reference exists only while L1 is held.
2. Validate the host-tested carrying/animation distinction for cartridge-paced reload. Previous continuation stopped after one round and remains physically rejected. Develop fuller physical reload only after native ownership and continuation are reliable; complete cylinder/ejection/chamber UX remains planned.
3. Cover death retry/back, held-control recovery, dashboard/loading transitions and newly available campaign mechanics. Death screen visibility itself is accepted.
4. Check the implemented protected-target red aiming cross when convenient. Original warning visibility in the visor remains unresolved and low priority.
5. Investigate Steam recording-associated microskips and sustained transport/resource recovery from evidence rather than arbitrary tuning.

Horse and broader weapon checks await available campaign content. Kick is inconclusive, not a confirmed defect. The default layout is maintained in [controls research](research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls).

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
