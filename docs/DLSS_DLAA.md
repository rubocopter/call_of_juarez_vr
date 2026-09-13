# DLSS / DLAA research track

DLSS/DLAA is potentially valuable in VR for both image quality and performance,
but it is intentionally separate from the first VR bring-up.

## Why it is not a drop-in feature

The games currently expose D3D9, plus a D3D10 path in the first title. Modern
DLSS integration is designed around newer graphics APIs and temporal inputs. A
useful DLSS Super Resolution or DLAA implementation must have reliable per-frame
data such as rendered color, depth, motion vectors and camera/jitter state.

VR adds another constraint: those inputs and history must remain correct for each
eye. A stereo path that merely upscales a final combined image would not provide
the same temporal information or geometric correctness.

## Investigation order

1. Establish stable native stereo and HMD tracking without temporal upscaling.
2. Characterize the Chrome Engine render graph for depth and temporal data.
3. Determine whether usable motion vectors already exist in any renderer pass.
4. Evaluate a justified modern graphics bridge or explicit reconstruction path.
5. Test DLAA first as a quality mode at native eye resolution.
6. Test DLSS Super Resolution at lower internal eye resolutions and measure both
   GPU time and temporal artifacts during head/controller motion.

## Acceptance criteria

DLAA/DLSS should not be considered integrated until it is tested in-headset for
both eyes under translation, rapid rotation, weapon motion, particles and UI.
Image stability matters more than a desktop screenshot comparison.
