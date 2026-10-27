# Documentation map

Use this index to locate the single owner of each fact. The repository's
[README](../README.md) presents the public mod and latest *published* package;
the source checkout may have more features than that release.

| Source of truth | Owns | Does not own |
| --- | --- | --- |
| [Architecture](ARCHITECTURE.md) | System layers, exact build boundaries, invariants and integration contracts | Physical acceptance and task chronology |
| [Validation](VALIDATION.md) | Accepted behavior in specified contexts, rejected gates, physical checklist and evidence policy | Planned implementation order |
| [Roadmap](ROADMAP.md) | Next implementation work, priorities, prerequisite gates and deferred tracks | Independent proof of headset acceptance |
| [Agent instructions](../AGENTS.md) | Editing, provenance and physical workflow rules | Alternative technical implementation contracts |
| [Player package instructions](../release/README.txt) | Installation, update, recovery and controls for the **package built from this source**; check the README inside previously published ZIPs for their actual launcher | Current acceptance/roadmap and older package behavior |
| [Release notes](../release/RELEASE_NOTES.md) | Immutable chronological behavior of published versions | Current local-development status |
| [Copyright and reuse](../COPYRIGHT_AND_REUSE.md), [third-party notices](../release/THIRD_PARTY_NOTICES.txt) | Attribution and distribution terms | Game-specific acceptance |

## Exact-game research

These are detailed implementation/evidence boundaries, not additional project
roadmaps. Where an experiment has been rejected, its design remains available
for debugging and can only be reactivated after the stated gates are met.

| Topic | Technical source |
| --- | --- |
| Camera, stereo, native wrapper and eye basis | [Camera path](research/COJ_CAMERA_PATH.md) |
| Arm anatomy, weapon geometry, skinning, grip and bow | [Arm skinning and aim](research/COJ_ARM_SKINNING_AND_AIM.md) |
| Manual reload, exact weapon classes, native counters, animation and offline derivatives | [Reload ownership](research/COJ_RELOAD_OWNERSHIP.md) |
| Button ownership, controller input and HUD semantics | [PC controls and HUD](research/COJ_PC_CONTROLS_AND_HUD.md) |
| UI mouse and VR settings consumer | [UI/mouse path](research/COJ_UI_MOUSE_PATH.md) |
| Startup resolution and SteamVR recommendation | [Release resolution profiles](research/COJ_VR_RELEASE_RESOLUTION_PROFILES.md) |
| Measured render dimensions and normal native stereo | [Render resolution](research/COJ_VR_RENDER_RESOLUTION.md) |
| Rejected independent per-eye render-target design | [Independent resolution design](research/COJ_VR_INDEPENDENT_RESOLUTION_DESIGN.md) |
| D3D9 allocations and high-address pressure | [Resource census](research/COJ_D3D9_RESOURCE_CENSUS.md) |
| Postprocess resources, HDR alpha and depth ownership | [PPFX target ownership](research/COJ_PPFX_TARGET_OWNERSHIP.md) |
| Pause/level screenshot crash path | [Save/thumbnail path](research/COJ_SAVE_THUMBNAIL_PATH.md) |
| Native shutdown and callback ownership | [Shutdown boundary](research/COJ_SHUTDOWN_BOUNDARY.md) |
| Fire-emitter definitions and LAA pointer handling | [World-fire definitions](research/COJ_WORLD_FIRE_DEFINITIONS.md) |
| External VR implementations, reuse limits and DLSS comparisons | [Reference projects](research/COJ_VR_REFERENCE_PROJECTS.md) |

## Evidence versus history

`planned → implemented → host-tested → live-tested → headset-validated → supported`
is the project evidence ladder. An isolated JVM/NGX test and a prepared local
candidate never skip the physical gate. Session identifiers, temporary telemetry,
run notes, screenshots and handoffs live under ignored `work/`. Git history
retains superseded versioned narrative. Release notes and license notices are
preserved for traceability, even when older releases differ from current source.
