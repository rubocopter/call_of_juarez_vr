# Codex handoff — Call of Juarez VR

## Current checkpoint

The repository is in Milestone 0. The architecture follows the useful separation
learned from Penumbra VR: shared game-neutral runtime, renderer backends and
per-game adapters, with exact-build evidence kept out of shared policy.

Implemented:

- Win32 CMake project/presets;
- game/renderer IDs and exact executable SHA-256 catalog;
- CNG SHA-256 helper and current-host identity boundary;
- renderer-neutral pose/eye types;
- D3D9 availability probe;
- runtime host tests;
- initial architecture, roadmap, validation, research and DLSS/DLAA documents.

Validation on 2026-09-13:

- CMake Win32 configuration generated successfully;
- Debug Win32 MSBuild completed with 0 warnings and 0 errors;
- Release Win32 MSBuild completed with 0 warnings and 0 errors;
- `cojvr_runtime_tests.exe` passed;
- `cojvr_runtime_tests.exe` also passed in Release;
- the Release D3D9 availability probe executed successfully and detected two
  adapters. This is host validation only; no game process has loaded framework
  code yet.

## Next gate

Implement a forwarding-only D3D9 bootstrap for the exact observed Call of Juarez
build. It must preserve normal rendering and log identity/device creation with no
VR modifications. Only after that is live-tested should OpenXR be introduced.

Do not skip directly to camera hooks, stereo rendering, motion controls or DLSS.
Do not treat the D3D10 path as disposable; it remains a first-class follow-up for
the first game because it provides visible rendering improvements.
