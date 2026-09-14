# Codex objective — audit-driven stabilization

Use this objective for the current implementation pass.

## Objective

Stabilize Call of Juarez VR by executing the audit remediation program before adding new VR features.

The authoritative engineering sources for this pass are, in order:

1. `AGENTS.md`
2. `docs/TECHNICAL_AUDIT.md`
3. `docs/AUDIT_REMEDIATION_PLAN.md`
4. `docs/internal/CODEX_HANDOFF.md`
5. `docs/VALIDATION.md`
6. `ARCHITECTURE.md`
7. `ROADMAP.md`

Inspect the actual repository state first and do not assume a documented item is still pending if the code has already changed. Conversely, do not mark an audit item resolved merely because code exists; require the acceptance evidence defined in the remediation plan.

### Primary goal

Replace the current fragile, hard-to-audit D3D9/OpenVR diagnostic path with a reproducible and observable pipeline in which one execution can identify:

- the exact source/build/deployment/run identity;
- which D3D9 factory/device/swapchain owns the active render path;
- whether every installed hook remains owned and valid;
- how many callbacks, unique captures, uploads and submissions occur;
- where any stage enters, exits, fails or stalls;
- whether OpenVR continues presenting when game capture stops;
- whether repeated submissions contain new game content or merely repeat the last frame.

Do not optimize for "make something appear in the headset" at the expense of evidence quality. The immediate objective is to make the next runtime observation decisive and reproducible.

### Required execution order

Follow `docs/AUDIT_REMEDIATION_PLAN.md` in order:

- Phase 0: auditable source/build/run provenance;
- Phase 1: clean build, CI and host-test validity;
- Phase 2: safe vtable patch/HookRegistry infrastructure;
- Phase 3: native COM/factory/device discovery without split identity;
- Phase 4: structured run/render telemetry.

Complete the acceptance criteria of Phases 0-4 before asking for another manual game observation run.

After that evidence gate, continue with:

- Phase 5: separate D3D9 capture from OpenVR presentation;
- Phase 6: formalize OpenVR state/lifetime/synchronization;
- Phase 7: transactional staging/verification;
- Phase 8: neutral VR math contracts before real stereo cameras.

Phase 8 may be host-tested in parallel where useful, but it must not distract from the critical-path observability and interception work.

### Non-negotiable constraints

- Do not start camera hooks, stereo camera rendering, 6DOF, UI adaptation or PS VR2 Sense gameplay integration during this pass.
- Do not reactivate D3D9Ex as the primary game path without new evidence.
- Do not blame PSVR2 hardware or replace OpenVR without evidence.
- Do not use a blind periodic re-hook loop as the default solution.
- Do not introduce a full `IDirect3DDevice9` wrapper simply to avoid hook replacement unless COM identity, `QueryInterface`, `GetDirect3D`, lifetime and device-discovery semantics are explicitly validated and the audit evidence justifies that design.
- Preserve native D3D9 object identity where possible.
- Keep OpenXR experimental and independently selectable; OpenVR must not require OpenXR merely to configure/build.
- Never launch Call of Juarez or SteamVR automatically. The user owns all game/runtime/headset launches.
- Do not request a manual runtime test until the corresponding gate has an exact built artifact, reversible/transactional staging path, verifier and explicit expected evidence.
- Do not equate 300 submissions with 300 unique game frames; track capture/content/submission sequences separately.
- Keep Call of Juarez (2006) as the reference implementation and do not generalize Chrome Engine behavior to Bound in Blood/Gunslinger without evidence from a second game.

### Existing evidence that must be preserved

Do not discard or reinterpret these established facts:

- classic D3D9 -> CPU readback -> D3D11 upload has worked in the exact game build;
- OpenVR runtime initialization, valid PSVR2 HMD pose and D3D11 submission have worked against SteamVR;
- the in-game flat path has completed genuine captured-frame submissions;
- one diagnostic run observed three project `Present`/`BeginScene`/`EndScene` callbacks followed by loss of integrity of the installed device-vtable entries while monitor rendering continued;
- that hook-integrity loss is a confirmed failure mode of the current design, but not yet a complete root-cause explanation for the blank headset;
- the historical candidate `369754A6D93A1A93C87B157E9480F8F82518A1F703B67ADCB8C56F889A14A6AF` can resolve replacement slot owners and should be preserved as baseline evidence rather than treated as the architecture to extend.

### Implementation discipline

For every meaningful change:

1. inspect the current implementation and relevant tests first;
2. make the smallest coherent change that advances the current remediation phase;
3. add or strengthen tests for both success and failure paths;
4. build the relevant Debug/Release targets from current sources;
5. run the smallest meaningful host suite;
6. update `docs/TECHNICAL_AUDIT.md` only when evidence changes a finding status;
7. update `docs/VALIDATION.md` with actual evidence, never intended behavior;
8. update `docs/internal/CODEX_HANDOFF.md` with the exact continuation point before stopping.

Do not preserve obsolete implementation structure merely because it already exists. Refactor when the audit demonstrates that ownership or testability is fundamentally wrong, but keep changes phase-scoped and evidence-driven rather than performing an unbounded rewrite.

### Completion criterion for this objective

This objective is complete only when:

- Phases 0-4 satisfy their host acceptance criteria;
- one manual game observation run can unambiguously identify device/factory/generation, hook integrity, callback progression and stage progression from a single run ID;
- the next implementation decision can be made from that evidence without guessing which frame boundary or subsystem failed.

Do not claim the VR mod itself complete at that point. Flat headset integration, stereo cameras, 6DOF and controllers are later objectives.