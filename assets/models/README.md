# Reload cartridge

The current manual visual is the **loose `357Bullet` node only** from loafbrr_1's
[Revolver Game Asset](https://opengameart.org/content/revolver-game-asset)
(December 4, 2021), licensed [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
`revolver_ammo.gltf` is the unchanged `GLTF/RevolverAmmo.gltf` archive member;
SHA-256 `2cb1acda3e7e16aafd2778eabe9a5456f48847e35ad717d08cdbbb3f3a45242d`.
The embedded albedo SHA-256 is
`74b47d1f2ff814806a20e3e74c4598b433875a2eb7cf32c9a5a0d1b7322f3a64`.
Only 83 positions, source normals/UVs and 92 triangles enter runtime. Scene
placements, boxes, cylinders and other models are excluded. The tool crops the
loose-round albedo islands to 128x160 pixels; the shader adapts the source orange
case palette to brass and adds authored metal preview shading. This is an adapted
third-party visual, not a game asset, native environment lighting or a calibre match.

Run `python tools/import_textured_cartridge.py --check` to verify the source and
generated `src/runtime/textured_cartridge_mesh.hpp` and
`src/backends/openvr/reload_cartridge_texture.hpp`. Generation uses Python's
standard library only. Git preserves source bytes for the SHA-256 checks.

The earlier `44_magnum.obj` is the unmodified loose .44 Magnum model from Pichuliru's
[CC0 Flat Ammunition](https://opengameart.org/content/cc0-flat-ammunition),
published April 4, 2022. License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
This is third-party public-domain geometry, not a Call of Juarez resource or a
claim of historical/calibre fidelity to the in-game weapon.

Archive member: `Flat_Ammunition/OBJ/Loose Ammo/44 Magnum.obj`.
SHA-256: `b78f01faf375f32a27e3f9356b7926dee8bfae8c03882d1b94fd94807fbd3b1d`.
The OBJ's unused MTL reference is retained for source identity. Its previous
runtime visual used authored brass/copper/primer swatches and was rejected for
appearance; this source/bake is retained for comparison, not the active manual model.

`tools/import_reload_cartridge.py` validates source identity/topology and generates
`src/runtime/reload_cartridge_mesh.hpp`. Run it with `--check` to verify the bake.
The 122 source positions and all 224 triangles are retained, with a right-handed
axis conversion and the existing local tip offset. Runtime geometry is captured
in head space and batched per eye; it does not create an engine ammunition object
and has no scene-depth occlusion.
