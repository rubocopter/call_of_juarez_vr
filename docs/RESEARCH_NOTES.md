# Research notes

This file keeps only cross-title research that still affects current decisions. Exact Call of Juarez findings live in the dedicated camera and arm/weapon research documents.

## Call of Juarez (2006)

Current authoritative research:

- `docs/research/COJ_CAMERA_PATH.md`: exact camera/render path, stereo wrapper, units and presentation findings.
- `docs/research/COJ_ARM_SKINNING_AND_AIM.md`: observed skeleton propagation, visible arm writer, reach and weapon-origin ownership.

The active renderer/runtime path is the D3D9 API + OpenVR/SteamVR. The preferred
native-stereo transport substitutes a D3D9Ex device to create shared
DEFAULT-pool eye textures; classic D3D9 is an explicit CPU fallback. D3D10
remains a later first-class target.

Classic-D3D9 CPU readback is a demonstrated performance blocker at the current
per-eye source resolution. D3D9Ex-to-D3D11 handle sharing is host-tested and
removes that boundary for native stereo. This requires D3D9Ex specifically:
classic D3D9 cannot create the DXGI-shareable DEFAULT resources used by the
presenter. Exact-game stability and physical cadence remain unvalidated.

`tig3rmast3r/OFXR-Bridge` was investigated as a performance option. It is an experimental **OpenXR** API layer that inserts color-only optical-flow-generated frames between rendered OpenXR frames. It does not replace the OpenXR runtime and does not receive game motion vectors/depth. Because the current Call of Juarez backend is OpenVR, OFXR-Bridge neither implements the required D3D9Ex resource handoff nor recovers detail absent from the source image. Keep it as future OpenXR/frame-generation research, not as a current D3D9/OpenVR transport fix.

## Bound in Blood

Planned future backend. No Call of Juarez addresses, RVAs, Java classes, camera offsets, skeleton hierarchy or renderer ownership assumptions may be copied as if they were shared Chrome Engine contracts.

The first integration must independently establish:

- exact executable/build identity;
- renderer/device path;
- camera/view/projection ownership;
- units and coordinate conventions;
- input and actor ownership;
- skeleton/weapon seams.

Only independently matching boundaries may then be promoted into shared runtime policy.

## Gunslinger

Planned future backend with the same evidence rule. Engine-family similarity is a research hint, not proof of binary or gameplay compatibility.

## Cross-game promotion rule

Call of Juarez (2006) remains the reference implementation. Shared runtime code should contain only renderer/game-neutral policy that can be described without exact game layouts. A Chrome Engine behavior becomes reusable only after a second supported title demonstrates the same semantic boundary.

## Local evidence policy

Run manifests, videos, hashes, process IDs, raw telemetry, temporary disassembly notes and agent handoffs belong under ignored `work/`. Once a durable conclusion is encoded in source, tests or the research documents above, the local evidence may be discarded.
