# VR Physical Regressions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct the 2026-09-20 PS VR2 physical regressions: blocking UI visibility/input, player falling while UI freezes simulation, flat-menu hover/select, body yaw anchoring, persistent world tilt after recenter/save loads, locomotion feel, and ballistic shot origin.

**Architecture:** Keep all exact Call of Juarez JVM/action IDs, player/camera ownership and weapon seams inside the exact-game adapter. Preserve the already validated native stereo, physical roll, tracked-Z hand mapping, snap turn, local-head suppression and arm writer. Use timer/UI state as a safety boundary that can force flat presentation and suspend gameplay/body mutation, then restore native stereo/gameplay only after the blocking UI releases.

**Tech Stack:** C++20, Win32/x86, JNI 1.4 bridge, OpenVR, classic D3D9, CMake/CTest, PowerShell verifier tooling.

**Spec:** `AGENTS.md`, `docs/AUDIT_REMEDIATION_PLAN.md`, and the diagnostic run `20260920T120157Z-ec17bbca3a2b` described by the latest physical report.

## Global Constraints

- Never launch Call of Juarez or SteamVR automatically.
- Preserve validation states; new code can become at most `host-tested` in this task.
- Preserve exact-build/game-specific ownership boundaries.
- Preserve the physically validated native-camera roll and exact +/-45 degree snap turn.
- Preserve native spread/accuracy and the network-forced firing branch.
- Do not revisit validated tracked-hand positional axes while addressing body/aim ownership.

## Review Focus

- Frozen timer while a non-`GameUILoading` modal is active must neutralize all gameplay and suspend actor/body mutation.
- UI select must work for the current active UI even before/without `GameWithMenu.sm_cMenuModule`, and hover must follow the Sense pointer.
- Recenter on a naturally pitched/rolled game camera must produce a level locomotion/body reference while retaining instantaneous physical head tilt in the rendered view.
- HMD yaw ownership must rotate the actor without double-applying the same yaw to the camera.
- Ballistic origin changes must be scoped to the actual attack boundary and restore native state outside that boundary.

---

### Task 1: Blocking UI safety and presentation ownership

**Files:**
- Modify: `src/games/call_of_juarez/java_player_bridge.hpp`
- Modify: `src/games/call_of_juarez/java_player_bridge.cpp`
- Modify: `src/games/call_of_juarez/camera_probe.cpp`
- Modify: `src/games/call_of_juarez/native_stereo_proxy.cpp`
- Test: `tests/body_adapter.cpp`

**Interfaces:**
- Produces: an exact-game blocking-UI policy result with `blocking`, `dispatch_select`, `suppress_gameplay`, and `suppress_body_mutation` semantics.
- Consumes: `TryGetActiveGameTimerFrozen()` and the existing UI-select action edge.

- [ ] Add failing tests showing a frozen timer suppresses movement/run/fire and actor/body mutation even when the active UI is not `GameUILoading`.
- [ ] Run `ctest --test-dir build-win32 -C Debug -R body_adapter --output-on-failure` and confirm the new assertions fail.
- [ ] Implement the minimal generalized blocking-UI gate and wire it before gameplay/body mutation.
- [ ] Make the presenter prefer `flat_theater` while a blocking timer/UI modal is active, then return to native stereo after resume.
- [ ] Re-run the focused Debug test and confirm PASS.

### Task 2: Flat UI hover and exact active-UI selection

**Files:**
- Modify: `src/games/call_of_juarez/java_player_bridge.hpp`
- Modify: `src/games/call_of_juarez/java_player_bridge.cpp`
- Modify: `src/games/call_of_juarez/native_stereo_proxy.cpp`
- Test: `tests/body_adapter.cpp`

**Interfaces:**
- Produces: pointer-position dispatch into the active `GameUserInterface` mouse-processing seam and select dispatch that does not require `sm_cMenuModule` when another active UI owns input.
- Consumes: normalized flat-plane pointer coordinates and the current Win32 cursor position.

- [ ] Add a failing host policy test for pointer-active/select routing independent of loading-only state.
- [ ] Confirm RED with the focused `body_adapter` CTest.
- [ ] Resolve/call the active UI mouse-processing/focus seam from the exact shipped classes and keep Win32 cursor movement as presentation-only state.
- [ ] Dispatch Enter/select to the active UI and retain the trigger-release fence.
- [ ] Confirm the focused test is GREEN.

### Task 3: Body yaw ownership and level recenter reference

**Files:**
- Modify: `src/games/call_of_juarez/body_adapter.hpp`
- Modify: `src/games/call_of_juarez/body_adapter.cpp`
- Modify: `src/games/call_of_juarez/camera_probe.cpp`
- Test: `tests/body_adapter.cpp`
- Test: `tests/camera_probe.cpp`

**Interfaces:**
- Produces: an actor-yaw ownership calculation and a leveled recenter/body reference that excludes natural camera pitch/roll from locomotion/body axes.
- Consumes: relative HMD orientation and the exact `PlayerBeing.RotateHorizontally(F)` route.

- [ ] Add failing tests for actor yaw follow without camera double-yaw and for recenter on a pitched/rolled natural camera basis.
- [ ] Confirm RED in `body_adapter` and `camera_probe` tests.
- [ ] Implement the minimal actor-yaw decomposition and world-up/level reference while preserving instantaneous physical roll in `ApplyCameraPoseOrientation`.
- [ ] Confirm both focused tests are GREEN.

### Task 4: Native analog locomotion semantics

**Files:**
- Modify: `src/games/call_of_juarez/java_player_bridge.cpp`
- Test: `tests/body_adapter.cpp`

**Interfaces:**
- Produces: smooth signed action values through the game’s configured analog input path, preserving run as a separate boolean.
- Consumes: `GameplayInputState.move` and shipped action/target configuration.

- [ ] Add failing tests for expected low-speed, diagonal, full-speed and release behavior based on the native analog transform contract.
- [ ] Confirm RED.
- [ ] Correct only the adapter-side transform/deadzone behavior needed to match the native analog route.
- [ ] Confirm GREEN.

### Task 5: Controller-owned ballistic origin at attack boundary

**Files:**
- Modify: `src/games/call_of_juarez/java_player_bridge.hpp`
- Modify: `src/games/call_of_juarez/java_player_bridge.cpp`
- Modify: `src/games/call_of_juarez/camera_probe.cpp`
- Test: `tests/body_adapter.cpp`

**Interfaces:**
- Produces: a scoped per-attack origin override/restore using the exact native firing boundary, with per-hand controller-tip origin telemetry.
- Consumes: existing `/pose/tip` position/orientation, native `GetFireDirForWeapon` spread/accuracy and network-forced state.

- [ ] Add failing policy/math tests for mapping controller-tip position into the native world basis and for native-state restoration after a scoped override.
- [ ] Confirm RED.
- [ ] Instrument and apply the origin only at the exact attack/fire boundary; never persistently replace the player look-from point.
- [ ] Confirm GREEN and add verifier telemetry for origin ownership.

### Task 6: Verification, evidence, documentation, and candidate preparation

**Files:**
- Modify: `docs/VALIDATION.md`
- Modify: `docs/internal/CODEX_HANDOFF.md`
- Modify: `ROADMAP.md`
- Modify: `tools/verify_native_stereo_live_test.ps1`

**Interfaces:**
- Consumes all host-tested corrections above.
- Produces a fresh, uniquely identified full/body candidate ready for one manual PS VR2 run.

- [ ] Run fresh Debug and Release builds and the complete CTest suite; expect 24 PASS plus the known classic-D3D9 shared-texture SKIP unless the suite count changes intentionally.
- [ ] Run the relevant provenance/verifier tests.
- [ ] Record run `20260920T120157Z-ec17bbca3a2b` as diagnostic multiprocess evidence only and document each observed failure/root cause.
- [ ] Update validation/handoff/roadmap with only host-tested claims for the new fixes.
- [ ] Prepare/stage a new `-BodyIkAtStart` candidate through `tools/vr_test.ps1 prepare -BodyIkAtStart`; do not launch SteamVR or the game.

### Task 7: Follow-up from physical run `20260920T152316Z-48dab54366d5`

**Evidence:** `docs/research/evidence/20260920T152316Z-48dab54366d5.json`

- [x] Finalize and unstage the interrupted run; preserve it as diagnostic single-process evidence.
- [x] Add exact `IntroModule.OnInputKey` startup-skip and active `LawmanModule.cMenu` selection routes.
- [x] Add a bounded cyan flat-theater laser segment and reticle.
- [x] Remove per-frame Java actor-position reconciliation; keep room-scale translation camera-owned.
- [x] Add a body-yaw comfort cone and remove actor-owned yaw from controller/body mapping.
- [x] Require intro skip, Escape-menu selection, collision-safe translation and stable tracking-basis telemetry.
- [ ] Run fresh Debug and Release suites and prepare a unique `-BodyIkAtStart` candidate without launching it.
