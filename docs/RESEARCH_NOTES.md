# Initial research notes

Research date: 2026-09-13. Evidence below comes from the locally installed Steam
builds and should not be generalized to other releases without verification.

## Call of Juarez (2006)

Observed files include `CoJ.exe`, `CoJ_DX10.exe`, `ChromeEngine3.dll`,
`ChromeEngine3_DX10.dll`, `ChromEd.exe`, `code.pak` and `Data*.pak`.

The game provides first-party D3D9 and D3D10 launch paths. The D3D9 engine binary
contains `d3d9.dll` / `Direct3DCreate9`; the D3D10 engine binary contains
`d3d10.dll`, `D3D10CreateDevice`, `dxgi.dll` and `CreateDXGIFactory` references.

`code.pak` and the inspected `Data*.pak` archives use ZIP signatures. `code.pak`
contains Java bytecode with relevant classes including `Camera`, `BaseCamera`,
`CamerasManager`, `GameInputController`, `PlayerBeing`, `ArmedPlayerBeing`,
`WeaponHands` and `Weapon`. `game.ini` declares `LawmanGame` and Java source/class
paths, making this game the best initial reference for game-layer research.

Observed executable identities:

- `CoJ.exe`: `5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE`
- `CoJ_DX10.exe`: `23EDE8E8B3BA0E9E662E83DA2B70D3F80BCADAC5BEE9554578C3AAA7AC109390`

The D3D9 executable identity was rechecked on 2026-09-13 before proxy validation.
An ASCII symbol scan of the observed `ChromeEngine3.dll` found `Direct3DCreate9`
and no other common D3D9 bootstrap export names in the inspected set. This supports
the current minimal forwarding surface for this exact build; it does not establish
requirements for other game builds.

## Bound in Blood

The installation contains `CoJBiBGame_x86.exe`, `engine_x86.dll` and
`CoJ2_x86.dll`. Strings in `engine_x86.dll` explicitly reference Chrome Engine 4
source paths and its D3D renderer. The engine also references `d3d9.dll` and
`Direct3DCreate9`.

`Game.ini` selects `GameClassName("CoJ2Game")` and `GameDLL("CoJ2")`. This is a
notable change from the Java game layer in the first title: game behavior is now
primarily behind native DLL boundaries plus data/scripts.

The data still exposes useful first-person and weapon concepts, including
`PlayerFpp_*.scr`, `WeaponStateMachine.sms` and ChromeEd render settings.

Observed executable identity:

- `CoJBiBGame_x86.exe`: `5EDD55804D69AE8B48ABB2DAB7C412892EB64A86BA15BE3C1EC2C724FDA2A838`

## Gunslinger

The installation has a monolithic `CoJGunslinger.exe` plus a `coj4` data tree.
The executable contains `ChromeEngineCoJ4` / `ChromeEngine` strings and references
`d3d9.dll` / `Direct3DCreate9`.

`game.ini` selects `GameClassName("GameDI")` and `GameDLL("gamedll")`. Data files
retain many familiar concepts: `player_fpp.ascr`, `player_tpp.ascr`,
`weaponstatemachine.sms`, `inputs*.scr`, `duelcameras.scr`,
`humanheadcamera.phx`, `render_script_game.scr` and render-loop resources.

Observed executable identity:

- `CoJGunslinger.exe`: `CA1C4766900FEB867372E0E2E87EB5ADB92CA26EE537598A034ED7BE1313D93C`

## Cross-game evidence

The inspected `Data0.pak` files all have ZIP signatures. Comparing exact paths in
those archives found 632 shared paths between Call of Juarez and Bound in Blood,
217 between Bound in Blood and Gunslinger, and 43 between Call of Juarez and
Gunslinger. Path equality does not imply byte or semantic equality, but it is
strong evidence of an evolving shared content architecture.

The most valuable renderer-level commonality is D3D9 across all three games.
That justifies a shared D3D9 backend. Camera/player/weapon internals remain
per-game until live evidence proves reusable contracts.
