# Validation model

The project uses explicit evidence states:

`planned` -> `implemented` -> `host-tested` -> `live-tested` ->
`headset-validated` -> `supported`.

Current bootstrap checks:

1. Configure a Win32 CMake build.
2. Build with MSVC at `/W4`.
3. Run `cojvr_runtime_tests` to validate filename routing, catalog lookup and
   SHA-256 calculation.
4. Run `cojvr_d3d9_probe` to verify that a D3D9 interface can be created on the
   target machine.
5. When the forwarding bootstrap exists, test it with VR disabled before any
   OpenXR or camera work.

Host tests do not promote a game integration to `live-tested`. A live desktop
test does not promote it to `headset-validated`.

The initial development host passed the Release D3D9 availability probe and
reported two adapters.
