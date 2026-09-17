# Research notes

Initial binary research began on 2026-09-13. Evidence below comes from the locally inspected Steam builds and from subsequent live validation in the reference Call of Juarez build. Do not generalize exact binary behavior to other releases without verification.

## Call of Juarez (2006)

Observed files include `CoJ.exe`, `CoJ_DX10.exe`, `ChromeEngine3.dll`, `ChromeEngine3_DX10.dll`, `ChromEd.exe`, `code.pak` and `Data*.pak`.

The game provides first-party D3D9 and D3D10 launch paths. The D3D9 engine binary contains `d3d9.dll` / `Direct3DCreate9`; the D3D10 engine binary contains `d3d10.dll`, `D3D10CreateDevice`, `dxgi.dll` and `CreateDXGIFactory` references.

`code.pak` and the inspected `Data*.pak` archives use ZIP signatures. `code.pak` contains Java bytecode with relevant classes including `Camera`, `BaseCamera`, `CamerasManager`, `GameInputController`, `PlayerBeing`, `ArmedPlayerBeing`, `WeaponHands` and `Weapon`. `game.ini` declares `LawmanGame` and Java source/class paths, making this game the best initial reference for game-layer research once the renderer/runtime gate is stable.

Static inspection of the shipped `code.pak` also identifies `EBones.class` as an explicit human-skeleton ordinal contract: pelvis `0`, spine/spine1/spine2 `1/2/3`, neck/head `4/5`, left upper-arm/forearm/hand `7/8/10`, right upper-arm/forearm/hand `12/13/15`, thighs `16/17`, calves `20/21` and feet `22/23`. `PlayerBeing.class` references `GetMeshElemFromBoneID`, while `ArmedPlayerBeing.class` exposes head, spine and hand rotation state plus body/hand animation nodes. These are static game-data facts; the native actor pointer and writable native bone-transform boundary still require live/exact-binary evidence before writes are enabled.

Observed executable identities:

- `CoJ.exe`: `5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`
- `CoJ_DX10.exe`: `23EDE8E8B3BA0E9E662E83DA2B70D3F80BCADAC5BEE9554578C3AAA7AC109390`

The D3D9 executable identity was rechecked before proxy validation. An ASCII symbol scan of the observed `ChromeEngine3.dll` found `Direct3DCreate9` and no additional common D3D9 bootstrap export names in the inspected set. This supports the current minimal forwarding surface for this exact build; it does not establish requirements for other game builds.

### D3D9 live evidence

The current reference build has established several useful renderer facts:

- a forwarding `d3d9.dll` proxy can preserve normal menus/gameplay and observe device creation;
- classic D3D9 `GetRenderTargetData` -> system-memory readback -> D3D11 upload works in the live game without replacing the native device;
- synthetic D3D9Ex -> D3D11 shared-resource transport works on the host, but substituting the live game device with D3D9Ex is not stable and is not the current path;
- OpenVR/SteamVR can receive genuine captured game frames after D3D11 upload;
- the current failure is interception continuity: after exactly three observed `Present`, `BeginScene` and `EndScene` callbacks, the installed D3D9 device-vtable hooks are overwritten while the game continues rendering on the monitor.

The active diagnostic records each affected vtable slot and resolves the replacement target to its owning loaded module. Until that ownership is identified, do not infer whether the replacement is caused by an overlay, D3D9 itself or a Chrome Engine lifecycle transition.

This evidence is specific to the inspected `CoJ.exe` build and its D3D9 path.

## Bound in Blood

The installation contains `CoJBiBGame_x86.exe`, `engine_x86.dll` and `CoJ2_x86.dll`. Strings in `engine_x86.dll` explicitly reference Chrome Engine 4 source paths and its D3D renderer. The engine also references `d3d9.dll` and `Direct3DCreate9`.

`Game.ini` selects `GameClassName("CoJ2Game")` and `GameDLL("CoJ2")`. This is a notable change from the Java game layer in the first title: game behavior is now primarily behind native DLL boundaries plus data/scripts.

The data still exposes useful first-person and weapon concepts, including `PlayerFpp_*.scr`, `WeaponStateMachine.sms` and ChromeEd render settings.

Observed executable identity:

- `CoJBiBGame_x86.exe`: `5EDD55804D69AE8B48ABB2DAB7C412892EB64A86BA15BE3C1EC2C724FDA2A838`

## Gunslinger

The installation has a monolithic `CoJGunslinger.exe` plus a `coj4` data tree. The executable contains `ChromeEngineCoJ4` / `ChromeEngine` strings and references `d3d9.dll` / `Direct3DCreate9`.

`game.ini` selects `GameClassName("GameDI")` and `GameDLL("gamedll")`. Data files retain many familiar concepts: `player_fpp.ascr`, `player_tpp.ascr`, `weaponstatemachine.sms`, `inputs*.scr`, `duelcameras.scr`, `humanheadcamera.phx`, `render_script_game.scr` and render-loop resources.

Observed executable identity:

- `CoJGunslinger.exe`: `CA1C4766900FEB867372E0E2E87EB5ADB92CA26EE537598A034ED7BE1313D93C`

## Cross-game evidence

The inspected `Data0.pak` files all have ZIP signatures. Comparing exact paths in those archives found 632 shared paths between Call of Juarez and Bound in Blood, 217 between Bound in Blood and Gunslinger, and 43 between Call of Juarez and Gunslinger. Path equality does not imply byte or semantic equality, but it is evidence of an evolving shared content architecture.

The strongest renderer-level commonality currently identified is D3D9 across all three games. That justifies designing a reusable D3D9 backend, but not assuming that one exact interception mechanism or lifecycle will behave identically in each game.

Camera, player, weapon, UI and physics internals remain per-game until live evidence proves reusable contracts.
