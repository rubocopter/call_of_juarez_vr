"""Offline-only native reload laboratory; never produces an installable PAK.

The original native 20/21/22 callbacks remain responsible for Begin/Body/End.
The pre-Clear veto awaits explicit unit transfers without animation clock writes.
Neither modeled scheduling nor a successful helper qualifies a native loading
port, a free support hand, mechanical playback, or runtime/manual admission.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import zipfile

from patch_coj_weapon_consumers import PAK_SHA256, _patch_class
from coj_indexed_reload import PROFILES as INDEXED_PROFILES


# The scoped rifle shares the one-index native transfer/capacity with Winchester,
# but gets its own exact-class derivative and no runtime/optics admission.
OFFLINE_WEAPONS = ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope',
                   'WeaponPistolVolcanic')
INDEXED_WEAPONS = tuple(profile[0] for profile in INDEXED_PROFILES)


def _patch_offline_player(player: bytes, weapon: str) -> bytes:
    if weapon not in OFFLINE_WEAPONS:
        raise ValueError('No offline single-ammunition unit-transfer profile for this exact class')
    return _patch_class(player, (weapon,))


def patch_offline_classes(player: bytes, machine: bytes, weapon: str, *, cycle_animation=False) -> tuple[bytes, bytes]:
    if weapon not in OFFLINE_WEAPONS:
        raise ValueError('Unsupported offline manual reload class')
    from coj_manual_reload import patch_manual_classes
    return patch_manual_classes(player, machine, _offline_weapon=weapon, _offline_cycle=cycle_animation)


def export_lab(source: Path, output: Path, weapon: str, *, cycle_animation=False,
               indexed_body_cycle=False) -> dict:
    if indexed_body_cycle and (cycle_animation or weapon not in INDEXED_WEAPONS):
        raise ValueError('Select one exact indexed offline body-cycle profile only')
    source, output = source.resolve(), output.resolve()
    # Originals commonly sit inside the staged installation. This tool never
    # writes anywhere below that source directory, including through symlinks.
    if output == source.parent or source.parent in output.parents or output.exists():
        raise ValueError('Lab output must be a new directory outside the game/source directory')
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != PAK_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no output')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if len(archive.namelist()) != len(set(archive.namelist())):
            raise ValueError('Duplicate archive members; no output')
        if indexed_body_cycle:
            from coj_indexed_reload_session import build_indexed_session_lab
            player,machine=build_indexed_session_lab(data,weapon,cycle_animation=True)
        else:
            player, machine = patch_offline_classes(archive.read('ArmedPlayerBeing.class'),
                                                    archive.read('StateMashine.class'), weapon,
                                                    cycle_animation=cycle_animation)
    payloads = {'ArmedPlayerBeing.class.bin': player, 'StateMashine.class.bin': machine}
    manifest = {
        'schema_version': 1, 'kind': 'offline_manual_reload_lab', 'weapon_class': weapon,
        'original_code_sha256': PAK_SHA256,
        'runtime_admitted': False, 'animation_playback_validated': False,
        'support_hand_validated': False, 'loading_port_validated': False,
        'headset_validated': False,
        'native_states': {'begin': 20, 'body': 21, 'end': 22},
        'native_callbacks_preserved': True, 'animation_clock_writes': False,
        'native_body_cycle_implemented': bool(cycle_animation or indexed_body_cycle),
        'indexed_body_cycle_implemented': bool(indexed_body_cycle),
        'reload_mode': 'indexed' if indexed_body_cycle else 'unit',
        'limitations': [
            'No installer, runtime admission, or PAK is generated.',
            'Empty support slot and native scheduling are test hypotheses, not live observations.',
            'Native authored mechanical animation continues; no independent frame writer is admitted.',
            ('Indexed units use original selected-count/typed-reserve setters; whole-clip parameters stay unchanged.'
             if indexed_body_cycle else 'Multi-index and whole-clip weapons are excluded.'),
        ],
        'outputs': {name: hashlib.sha256(value).hexdigest() for name, value in payloads.items()},
    }
    output.mkdir(parents=True, exist_ok=False)
    for name, value in payloads.items():
        (output/name).write_bytes(value)
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Hash-pinned original code.pak or retained backup')
    parser.add_argument('output', type=Path, help='New local lab directory outside the installation')
    parser.add_argument('--weapon', required=True, choices=OFFLINE_WEAPONS+INDEXED_WEAPONS)
    cycle=parser.add_mutually_exclusive_group()
    cycle.add_argument('--cycle-animation', action='store_true',
                        help='Experimental guarded body reentry after exact unit readback; lab only')
    cycle.add_argument('--indexed-body-cycle', action='store_true',
                       help='Explicit isolated selected-index unit/body21 cycle; no installable PAK')
    args = parser.parse_args()
    try:
        manifest = export_lab(args.source, args.output, args.weapon, cycle_animation=args.cycle_animation,
                              indexed_body_cycle=args.indexed_body_cycle)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, str(error)+'\n')
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
