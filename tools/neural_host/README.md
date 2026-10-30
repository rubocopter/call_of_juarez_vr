# Standalone Neural laboratory

This developer-only x64 companion processes stereo color pairs for the x86 D3D11
laboratory and an explicitly opted-in CoJ candidate. It is not included by the
player installer. It does not implement Super Resolution, DLAA or frame generation.

The operator first exercised this kind of Neural effect in Deus Ex: Human
Revolution Director's Cut. Penumbra supplies reusable source/lifecycle behavior,
but its working, imperfectly applied effect is not an accepted quality reference.
CoJ must independently qualify both application and appearance.

## Build and run

Build the client/probes with the normal Win32 configuration:

```powershell
cmake --build --preset release --target cojvr_neural_session_test cojvr_neural_gpu_roundtrip_test
```

Configure the companion separately. Supply a local NVIDIA definitions header;
configuration verifies its SHA-256 and copies it into the build's SDK directory.
The pinned upstream header is `include/nvsdk_ngx_defs.h` at NVIDIA/DLSS revision
`9981c6350dc58c0ce21f024fd65e31e851c31689`, SHA-256
`ea23f33497cd274860d1c25a97644fce807dcb0037c594547203343103fad03e`.
No SDK or model download is performed by this build.

For the physical developer workflow, pass both `-NeuralHostPath <absolute-helper>`
and `-NeuralRuntimeDirectory <absolute-local-runtime>` to `tools/vr_test.ps1 prepare`
with the normal `-BodyIkAtStart` recipe. These options journal `cojvr-neural.ini`,
record x64 helper/model/forwarder SHA-256 identities and admit the developer capability.
`finish` verifies/restores/removes that config using the ordinary transaction.
The operator starts/stops SteamVR and the game manually. No model is copied into
the game or public package. Default preparation does not enable Neural.
Use the separate **Neural Off/On** selector in **Options > Game**, then confirm
with OK. The saved request defaults Off. A dedicated status/count distinguishes
the request from actual initialization, warmup, processing, commits and fallback.
Off retains the native pair; a fresh On retries optional setup. This does not
change the Native/DLAA/DLSS Quality preference. Compare a still scene and moving
hands/weapons in both eyes. Native text baked into the image may also be altered.
The corrected candidate uses stage height384 with source aspect and 25% model
scale. Capture/inference/resolve are queued and polled without waits; one immutable
native pair and its pose are retained across GPU readiness. While pending, the
compositor repeats that matching native image, bounded by the existing 100ms
capture-age fence. Completion never modifies a newer frame. Cancelled resources
drain before reuse, and expired work returns to native delivery. The legacy INI
TimeoutMs entry remains readable for older laboratory probes; it does not add a
synchronous runtime wait. These are experimental policies, not a latency/quality
guarantee. Judge the added image age/cadence and binocular consistency explicitly.

```powershell
cmake -S tools/neural_host -B build/neural-host -A x64 -DCOJ_NEURAL_NGX_DEFS="C:/local-sdk/include/nvsdk_ngx_defs.h"
cmake --build build/neural-host --config Release
ctest --test-dir build/neural-host -C Release --output-on-failure
pwsh -File tools/test_neural_lab.ps1 -HelperPath build/neural-host/Release/CoJNeuralHost.exe
```

The explicit GPU probe requires a hardware D3D11 adapter. Identity and known-edit
diagnostics require no NVIDIA model. For real inference, create a local development
runtime directory containing your supplied `nvngx_dlssnr.dll` and the built
`nvngx.dll_nr_bridge.dll`. Then pass `-RuntimeDirectory C:/local-runtime` to the
same test script. NVIDIA model bytes remain outside version control and releases.
Missing dependencies or a rejected model fail the real test; there is no silent
diagnostic substitution. Neural runtime availability is machine-specific.

## Contract and test boundary

The helper command line is `<base-name> <parent-pid> <runtime-directory> <mode>`.
The parent creates session-unique `-map`, `-ready`, `-request`, `-complete`, `-stop`
objects. `neural_protocol.hpp` owns the fixed-width ABI, RGBA8 encoded UNORM pair,
frame/pose/device/generation/revision/history identity, and per-view quaternion/FOV
in radians. Request and response IDs must match; publication requires both eyes.
One caller owns each session. Only CPU memory is shared; GPU resources remain
private to the helper or probe. Stop/parent exit request companion shutdown.
Names are never recycled for another pending request/session.

Modes `identity`, `edit`, `delay`, `fault`, `stale` are synthetic fault fixtures.
`model` invokes the adapted DirectNR implementation. The synthetic tests check
byte identity/known RGB edits, source alpha, separate eye patterns, stale/invalid
identities, repeated frames, settings/generation changes, asynchronous warmup,
timeout/late discard, restart and child faults. The GPU fixture downsamples a
256px original to a 64px diagnostic proxy, then resolves
`original + filtered(processed_proxy - input_proxy)` in encoded UNORM space.
sRGB originals are encoded back after sampling decode; source alpha is retained.
Identity must retain one-pixel details exactly. Inputs are never overwritten.

The real-model fixture uses a 256px transport stage with the inherited 25% scale
(a 128x64 binocular model atlas). A 64px transport stage caused device removal
in one local model run; it is not a qualified model input. These observations do
not define a universal model-size limit. Observe other sizes before admitting them.

Startup is polled without blocking. Explicit `Warmup` submits one disposable pair
without waiting; polling allows up to 60 seconds for model creation and discards
its output. A fresh history epoch is required before processing current images.
Ordinary frame waits retain their own caller deadline (at most 5 seconds in this
lab), then drain/discard late output with a two-second watchdog. These budgets are
laboratory bounds, not a usable VR render-thread budget. CPU/GPU readbacks here
can block. No runtime pacing or performance claim follows from `lab_wall_ms`.

Real-model runs verify a nonzero RGB edit, different eye results, original-image
and alpha preservation. They do not prove visual quality, binocular consistency
of generated content, material/HUD behavior or headset comfort. Game integration,
representative-size cost measurements, reset/device loss and sustained inference
remain separate gates. Runtime logs are `neural-<pid>.log` beside the helper.

## Offline human-crop image probe

`CoJNeuralImageLab` is a separate x64 executable; it does not change the game,
presenter or production helper configuration. Supply already selected left/right
crop pixels at their source texel density, encoded RGBA8 UNORM, entire left eye
followed by right eye. The output path must be new and the payload length exact.
No human detector, game visibility mask or live ROI transport is implied.

```powershell
cmake --build build/neural-host --config Release --target CoJNeuralImageLab
ctest --test-dir build/neural-host -C Release -R neural_image_lab_cli --output-on-failure
build/neural-host/Release/CoJNeuralImageLab.exe --runtime C:/local-runtime --input C:/local-input/pair.rgba --output C:/local-output/new-pair.rgba --width 512 --height 512 --scale100 --warmup 4 --iterations 32
```

Choose exactly one of `--scale25`, `--scale50`, `--scale100`; it is the only
model reduction and there is no preceding transport resize. Zero effective eye
extents after scale/even rounding are rejected. Accepted CLI dimensions are not
model-shape qualification: real-model controlled crop inputs at 256, 384 and
512 square are exercised; the small-input device-removal finding above remains.
Style/preset/intensity default to the inherited 0/0/60, with exactly one pass.
The CLI accepts `--style 0|1|2`, `--preset 0|1` and `--intensity 0..100`.
Third-party NR panels label style 2 *Cinematic*; it is a separate control from
the model preset hint. For a controlled comparison of the operator's preferred
settings on the existing real-game face crop, use `--scale100 --style 2 --preset 1
--intensity 80`. The requested values and fixed pass count appear in JSON. This
profile is experimental, not a new default or a live performance/quality preset.
Actual still-image variants and repeatability evidence are in ignored
`work/neural-human-cinematic-20261030/`.

The JSON timing covers CPU wall time for pair upload, model and blocking readback,
with initialization/warmup excluded. It excludes game rendering, selection,
segmentation, IPC, compositor and live load. Alpha is restored from source after
readback. The tool labels duplicated mono inputs and never reports binocular or
headset validation. Nonzero RGB differences do not measure improvement. Keep
original assets/captures and generated comparison outputs local under `work/`.
`--diagnostic identity|edit` is explicit synthetic testing; failed real inference
never silently substitutes either diagnostic.

The [human-targeting research](../../docs/research/COJ_NEURAL_HUMAN_TARGETING.md)
owns NPC/head identity, visible-pixel ownership and fresh-frame prerequisites.

## Reuse and licenses

`src/NeuralPass.*`, `NeuralSettings.h`, `NeuralTracking.h` and lifecycle/resolve
patterns are adapted from Penumbra VR Framework revision
`7b014c96ed928440abf4452664f35220d06c1fb6`, under GPL-3.0-or-later; see the root
project license. CoJ changes add its standalone pair protocol, diagnostics and
validation without HPL/OpenGL hooks.

`vendor/nr-bridge/nr_direct.h` and `nr_forwarder.cpp` are the MIT LowPopLabs
DirectNR sources carried by that revision. Penumbra records upstream nr-bridge
revision `0bb6bf86e13ac19a85d7693062c1f74cfcd5f77f`; the source/license files are
retained together. `nr_direct.h` SHA-256 is
`07bd8dd23592cf71c7f417328de80ed6b1b8b7462d6ef8452264e14c660f856f`.
The NVIDIA definitions retain `NVIDIA-SDK-LICENSE.txt`; NVIDIA binary/model terms
are separate. No NVIDIA binary is embedded in this repository.
