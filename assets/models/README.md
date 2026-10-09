# Reload cartridge

`44_magnum.obj` is the unmodified loose .44 Magnum model from Pichuliru's
[CC0 Flat Ammunition](https://opengameart.org/content/cc0-flat-ammunition),
published April 4, 2022. License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
This is third-party public-domain geometry, not a Call of Juarez resource or a
claim of historical/calibre fidelity to the in-game weapon.

Archive member: `Flat_Ammunition/OBJ/Loose Ammo/44 Magnum.obj`.
SHA-256: `b78f01faf375f32a27e3f9356b7926dee8bfae8c03882d1b94fd94807fbd3b1d`.
The OBJ's unused MTL reference is retained for source identity. Runtime uses
authored brass/copper/primer colors, without loading the source palette texture.

`tools/import_reload_cartridge.py` validates source identity/topology and generates
`src/runtime/reload_cartridge_mesh.hpp`. Run it with `--check` to verify the bake.
The 122 source positions and all 224 triangles are retained, with a right-handed
axis conversion and the existing local tip offset. Runtime geometry is captured
in head space and batched per eye; it does not create an engine ammunition object
and has no scene-depth occlusion.
