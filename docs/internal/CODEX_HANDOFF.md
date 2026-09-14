# Codex handoff — Call of Juarez VR

## Current checkpoint

The repository is entering an **audit-driven stabilization pass**. Do not continue the earlier Present/EndScene-by-elimination workflow and do not start new VR gameplay features.

The authoritative implementation order is:

1. `AGENTS.md`
2. `docs/TECHNICAL_AUDIT.md`
3. `docs/AUDIT_REMEDIATION_PLAN.md`
4. this handoff
5. `docs/VALIDATION.md`
6. `ARCHITECTURE.md`
7. `ROADMAP.md`

`docs/internal/CODEX_OBJECTIVE.md` contains the complete current objective for a fresh Codex session.

## Evidence that must be preserved

Call of Juarez (2006) DX9 remains the reference implementation. OpenVR -> SteamVR remains the primary PSVR2 path; OpenXR is experimental/future.

Established evidence:

- the forwarding D3D9 path has run the exact game build successfully on the monitor;
- classic D3D9 -> CPU readback -> D3D11 upload has succeeded in the exact game build;
- OpenVR has initialized against manually started SteamVR, returned a valid PSVR2 HMD pose and accepted D3D11 submissions;
- the in-game flat bridge has completed genuine captured-game-frame submissions through readback, upload, pose wait and both-eye submit;
- one later exact-build diagnostic observed exactly three project `Present`, `BeginScene` and `EndScene` callbacks and then detected that the installed device-vtable entries no longer pointed at the project hooks while monitor rendering continued;
- that hook-integrity loss is confirmed for the current interception design, but it is not yet a complete root-cause explanation for the blank headset;
- no real game stereo camera, 6DOF integration or motion-controller gameplay is implemented yet.

Historical candidate:

`369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF`

This candidate can identify which `Reset`, `Present`, `BeginScene` and `EndScene` slots are replaced and resolve replacement addresses to owning modules. Preserve it as baseline evidence. Do **not** make another headset run with it the first action of this stabilization pass.

## Why the direction changed

The technical audit found that several parts of the diagnostic chain were too weak to support confident root-cause attribution:

- incomplete source/build/deployment/run provenance;
- unsafe global vtable-patch ownership and rollback semantics;
- split/ambiguous D3D9 COM factory identity and possible device-discovery bypasses;
- synchronous coupling of D3D9 capture, D3D11 upload, pose wait and OpenVR submission inside a game callback;
- resource ownership not keyed strongly enough by device/generation;
- host tests that do not fully exercise the deployed pipeline;
- live verification not bound to a unique run;
- non-transactional staging/evidence handling;
- neutral runtime contaminated by OpenXR/build-identity concerns;
- incomplete runtime lifetime/error contracts;
- ambiguous neutral math semantics before camera work.

Some observability has improved since the original audit: callback counters, return counters and hook-integrity inspection are now present, and the five-cycle device-hook host test is stronger. Those improvements are reflected in `docs/TECHNICAL_AUDIT.md`; they do not resolve the broader stabilization work.

## Immediate objective

Execute `docs/AUDIT_REMEDIATION_PLAN.md` in order.

### Phase 0 — current starting point

Begin by making the source/build/deployment/run chain auditable.

Required work includes:

- inspect current `git status`, HEAD and tracked state;
- introduce a manifest tying source snapshot to built artifact hashes;
- preserve historical logs instead of deleting them during staging;
- record exact `CoJ.exe`, `ChromeEngine3.dll`, proxy and runtime dependency identities;
- introduce run-evidence collection without launching the game;
- keep "known build" distinct from "supported integration".

Do not proceed to a manual runtime test during Phase 0.

### Phases 1-4 — host-only critical path

After Phase 0, continue through:

1. build/CI/test validity;
2. safe `VtablePatch`/`HookRegistry` infrastructure;
3. native factory/device discovery and COM-identity coverage;
4. structured run/render telemetry.

Complete each phase's acceptance criteria before moving on.

The next manual game run should occur **only after Phases 0-4 satisfy their host gates**. That run is a game-observation gate and does not require the user to wear the headset.

It must be able to answer from one `run_id`:

- which factory/device/swapchain/generation rendered;
- whether installed hooks stayed valid and who replaced any lost target;
- exact callback entry/exit progression;
- unique capture/content progression;
- stage entry/exit/timing;
- OpenVR/runtime state progression where applicable.

## After the first audit-quality game observation

Use the evidence, not preference, to choose the next interception/render path.

Then continue with:

- Phase 5 — separate D3D9 capture from OpenVR presentation through owned frames and a bounded mailbox;
- Phase 6 — formalize OpenVR state/lifetime/synchronization;
- Phase 7 — transactional deployment and run-bound verification;
- Phase 8 — neutral VR math semantics before real stereo cameras.

Only after flat integration is sustained and physically validated should work proceed to:

- rotational HMD camera ownership;
- real per-eye stereo projection;
- 6DOF/room-scale reconciliation;
- UI/cinematics/post-processing;
- PS VR2 Sense actions, weapon decoupling and motion interaction.

## Architecture constraints

- Preserve native classic D3D9 semantics unless evidence requires otherwise.
- Do not reactivate D3D9Ex as the main game path without new evidence.
- Do not use blind periodic re-hooking as the default fix.
- Preserve native COM identity where possible.
- A complete `IDirect3DDevice9` wrapper is not the default next step; use it only if evidence requires it and its COM/lifetime semantics are explicitly validated.
- OpenVR must be buildable without requiring the experimental OpenXR backend.
- OpenVR submit count is not unique-game-frame count; maintain separate capture/content/submit sequences.
- Call of Juarez is the reference implementation; do not generalize Chrome Engine contracts without evidence from another game.

## Manual runtime policy

Never launch Call of Juarez or SteamVR automatically. The user performs all game, SteamVR, headset and controller launches/tests manually.

Before requesting a manual run:

1. build the exact candidate from current sources;
2. pass the active phase's host acceptance criteria;
3. identify source/build/package/deployment hashes;
4. prepare safe staging/recovery;
5. prepare the post-run verifier;
6. define the exact evidence expected from the run.

Do not ask for another physical headset test merely because implementation changed.

## Documentation discipline

- `docs/TECHNICAL_AUDIT.md` is the authoritative open-findings/status document.
- `docs/AUDIT_REMEDIATION_PLAN.md` is the authoritative execution order.
- `docs/VALIDATION.md` records evidence and historical runs, not future instructions.
- Update this handoff with the precise next incomplete phase before stopping.
- Update `ROADMAP.md` only at product/milestone granularity.

An audit item becomes resolved only when its acceptance evidence exists, not when an implementation attempt is present.