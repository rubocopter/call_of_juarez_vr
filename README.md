# Call of Juarez VR

Experimental VR framework for Techland's Chrome Engine Call of Juarez games.

The project currently targets:

- **Call of Juarez (2006)** — Chrome Engine 3, Direct3D 9 and Direct3D 10;
- **Call of Juarez: Bound in Blood** — Chrome Engine 4, Direct3D 9;
- **Call of Juarez: Gunslinger** — later Chrome Engine branch, Direct3D 9.

The intended product is one VR framework with shared runtime policy and narrow
per-renderer/per-game backends. Call of Juarez is the reference implementation;
reuse is promoted only when another game demonstrates the same contract.

## Current status

Pre-alpha research/bootstrap stage. The repository currently provides:

- Win32 CMake project and presets;
- exact-build SHA-256 catalog for the locally inspected Steam executables;
- renderer-neutral VR pose/eye types;
- host/build identity helpers;
- Direct3D 9 availability probe;
- host-side tests for build classification and SHA-256 calculation.

No game files are modified by the current code. No VR rendering path is enabled
yet.

## Build

Requirements: Windows, Visual Studio 2022 Build Tools with the C++ workload, and
CMake 3.25 or newer.

```powershell
cmake --preset win32-debug
cmake --build --preset debug
ctest --preset debug
```

The target architecture is currently **Win32/x86**, matching all three inspected
games.

See [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md) and
[docs/RESEARCH_NOTES.md](docs/RESEARCH_NOTES.md).
