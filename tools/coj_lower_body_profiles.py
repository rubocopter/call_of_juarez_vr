"""Read-only, original-archive-bound Ray/Billy authored leg pose sampler.

Example (original backup required when staged):
  python -B tools/coj_lower_body_profiles.py --data0-pak ORIGINAL --model Billy \
      --track Unarmed_WalkForward --frame 9.25

Prints one bounded derived pose as JSON; never writes/exports archive members.
No native SetFrame, animation update, audio, footsteps or MakeNoise calls.
This is offline math, NOT physical walking or a runtime render transaction.
Only active original Unarmed stand/walk/step definitions are admitted; the
commented RotateLeft/Right tracks are deliberately not activated by this tool.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

from coj_animation_channels import MAX_BONES, compose, parse_anm, validate_matrix, _number

DATA0_SHA256 = '5eb6ec4856e53226dfabad929522706a21f02d428923a8c9c38804cc2cf11a94'
MESH_HASHES = {
    'Ray': '9b3f4579cd32476f866dc1aad5a1bd0f1bc913449548f40f7c76bd37cd8d33b5',
    'Billy': 'ce1e70dfab7ddae4f4036518820e34c84a18d9dc18ab48163815d4ac1dd15be7',
}
_SCRIPTS = {
    'Data/Player/Ray/Ray.scr': 'aa227b022a5bb676f56916b3e4b65715d31642b840dfef9592ce29d2566401d9',
    'Data/Player/Billy/Billy.scr': '2f640bc1961671d18ab7d152336837de33ea1ac4508f31568f42b38e18b053b8',
    'Data/Player/Animations/Ray_Animations.scr': '42bf0ff96add88c875b32ed1321e8fe6b0dae902586f3c57be6a7cab30cef420',
    'Data/Player/Animations/Ray_Unarmed.scr': 'c9117b649a3e96a71b357ccd3ac887c2d2ef7f521dea87126f8c1d5013402a8e',
}
MAX_ARCHIVE_BYTES = 1024*1024*1024
MAX_MEMBER_BYTES = 4*1024*1024
MAX_SCRIPT_BYTES = 128*1024
_NAMES = ('Bip01', 'Bip01 Pelvis', 'Bip01 L Thigh', 'Bip01 L Calf', 'Bip01 L Foot',
          'Bip01 L Toe0', 'Bip01 R Thigh', 'Bip01 R Calf', 'Bip01 R Foot', 'Bip01 R Toe0')
_INDICES = {'Ray': (0, 2, 3, 4, 5, 6, 9, 10, 11, 12), 'Billy': (3, 5, 6, 7, 8, 9, 11, 12, 13, 14)}
_PARENTS = (None, 0, 1, 2, 3, 4, 1, 6, 7, 8)


@dataclass(frozen=True)
class Track:
    resource: str
    sha256: str
    frames: tuple[int, int]
    parameter: float
    parameter_semantics: str


_CLIPS = {
    'Stand': 'c127f3680369d816c3fb8866fb5b512b3683fe9463224fc524be415b4e44b702',
    'WalkForward': 'baa23d42082b3609c1718192f374378f4fbbbf1256d20c4869901064e0549401',
    'WalkBackward': 'f0840beb50b3376b7c35cf32689047f0126bb35f5e5dc34c86db717dbaa7c7ce',
    'WalkLeft': '773aed9de69d4b1c9c5c5d7ccf26773065b1404383fc1f91841c71dc3996864b',
    'WalkRight': '6791e3fab4958ecfdd22bebb4f03afabd1c595527ed837cc9bc11accb31162d1',
    'StepL': 'cfccce5afb0986962b7aecde523ac7b4cae7606ae63ee8832e913eeca48ce08b',
    'StepR': '989888adb88a78d5d816f1c993b39a321fdee18a5c48581ffee77f5ba511358c',
}


def _track(suffix, begin, end, parameter, semantics):
    return Track(f'Data/Player/Animations/Ray_Unarmed_{suffix}.anm', _CLIPS[suffix],
                 (begin, end), parameter, semantics)


TRACKS = {'Unarmed_Stand': _track('Stand', 0, 30, 30., 'authored_rate')}
for _suffix, _end, _distance in (('WalkForward', 37, 234.475), ('WalkBackward', 40, 234.475),
                                ('WalkLeft', 50, 451.025), ('WalkRight', 50, 451.025)):
    TRACKS['Unarmed_'+_suffix] = _track(_suffix, 0, _end, _distance, 'movement_distance_cm_per_cycle')
for _side in 'LR':
    for _phase, _begin, _end, _rate in (('Beg', 7, 25, 45.), ('Loop', 25, 26, 1.), ('End', 25, 7, 45.)):
        TRACKS[f'Unarmed_Step{_side}{_phase}'] = _track('Step'+_side, _begin, _end, _rate, 'authored_rate')


@dataclass(frozen=True)
class BindNode:
    name: str
    mesh_index: int
    parent_index: int | None
    local: tuple[float, ...]
    world: tuple[float, ...]


@dataclass(frozen=True)
class BindChain:
    model: str
    sha256: str
    nodes: tuple[BindNode, ...]  # root, pelvis, L chain, R chain


def _chunks(data, start, end):
    count = 0
    while start < end:
        if start+16 > end:
            raise ValueError('truncated mesh chunk')
        kind, version, size, header = struct.unpack_from('<4I', data, start)
        count += 1
        if version or size < 16 or size > end-start or header > size-16 or count > 1024:
            raise ValueError('invalid/oversize mesh chunk')
        yield kind, start, size, header
        start += size


def parse_bind_chain(data: bytes, model: str) -> BindChain:
    """Structural bind decoder, not resource admission. Loader pins mesh SHA.

    Exact model-specific indices AND names/parents are checked independently.
    Only the native skeletal class 8 local frames enter the derived chain.
    """
    if model not in _INDICES:
        raise ValueError('unknown player model')
    if not isinstance(data, bytes) or not 24 <= len(data) <= MAX_MEMBER_BYTES:
        raise ValueError('invalid mesh size/type')
    if struct.unpack_from('<4I', data) != (0x48534d, 0, len(data), 8):
        raise ValueError('unsupported mesh root')
    count = struct.unpack_from('<I', data, 16)[0]
    if not 1 <= count <= MAX_BONES:
        raise ValueError('mesh node limit')
    nodes, names = [], set()
    for kind, p, size, header in _chunks(data, 24, len(data)):
        if kind not in (1, 0x500, 0x600):
            raise ValueError('unknown mesh chunk class')
        if kind != 1:
            continue  # No material/geometry/skin payloads are evaluated.
        if header != 240 or size < 256 or struct.unpack_from('<I', data, p+16)[0] not in (1, 2, 4, 8):
            raise ValueError('unknown mesh node class/header')
        raw = data[p+20:p+52]
        if b'\0' not in raw:
            raise ValueError('unterminated mesh name')
        raw = raw.split(b'\0', 1)[0]
        if not raw or any(c < 32 or c > 126 for c in raw):
            raise ValueError('invalid mesh name')
        name = raw.decode('ascii')
        if name.casefold() in names:
            raise ValueError('ambiguous mesh names')
        names.add(name.casefold())
        parent = struct.unpack_from('<H', data, p+84)[0]
        if parent != 65535 and parent >= count:
            raise ValueError('invalid mesh parent')
        nodes.append((name, parent, p))
    if len(nodes) != count:
        raise ValueError('mesh node count mismatch')
    selected = []
    for role, (index, expected_name, parent_role) in enumerate(zip(_INDICES[model], _NAMES, _PARENTS)):
        if index >= len(nodes):
            raise ValueError('missing chain node')
        name, parent, p = nodes[index]
        expected_parent = 65535 if parent_role is None else _INDICES[model][parent_role]
        if name != expected_name or parent != expected_parent or struct.unpack_from('<I', data, p+16)[0] != 8:
            raise ValueError('model chain name/index/parent/class mismatch')
        local = validate_matrix(struct.unpack_from('<16f', data, p+92))
        world = local if parent_role is None else compose(local, selected[parent_role].world)
        selected.append(BindNode(name, index, None if parent == 65535 else parent, local, world))
    return BindChain(model, hashlib.sha256(data).hexdigest(), tuple(selected))


def recompose_legs(chain: BindChain, local_matrices, pelvis_world):
    """Eight derived world frames under an explicit pelvis, no root/body write.

    No missing-channel bind fallback, inferred parents, root-motion application,
    axis repair, metre conversion, retargeting or engine evaluation occurs here.
    """
    if set(local_matrices) != set(_NAMES[2:]):
        raise ValueError('exactly eight authored leg locals required')
    worlds = {chain.nodes[1].mesh_index: validate_matrix(pelvis_world)}
    rows = []
    for node in chain.nodes[2:]:
        local = validate_matrix(local_matrices[node.name])
        if node.parent_index not in worlds:
            raise ValueError('invalid leg parent order')
        world = compose(local, worlds[node.parent_index])
        worlds[node.mesh_index] = world
        rows.append({'name': node.name, 'mesh_index': node.mesh_index, 'parent_index': node.parent_index,
                     'local_matrix': local, 'world_matrix': world,
                     'local_matrix_sha256': hashlib.sha256(struct.pack('<16d', *local)).hexdigest(),
                     'bind_local_sha256': hashlib.sha256(struct.pack('<16f', *node.local)).hexdigest()})
    return rows


@contextmanager
def _original(path):
    # Verify the SAME open handle before ZIP parsing; no filename-based trust.
    with Path(path).open('rb') as stream:
        if not 0 < stream.seek(0, 2) <= MAX_ARCHIVE_BYTES:
            raise ValueError('original archive size limit')
        stream.seek(0)
        if hashlib.file_digest(stream, 'sha256').hexdigest() != DATA0_SHA256:
            raise ValueError('unknown original Data0 SHA-256; use the original backup when staged')
        stream.seek(0)
        with zipfile.ZipFile(stream) as archive:
            infos = archive.infolist()
            if len(infos) > 50000:
                raise ValueError('archive member limit')
            keys = [i.filename.replace('\\', '/').casefold() for i in infos]
            if len(keys) != len(set(keys)):
                raise ValueError('ambiguous archive members')
            yield archive


def _read(archive, member, expected, limit=MAX_MEMBER_BYTES):
    info = archive.getinfo(member)
    if info.is_dir() or info.flag_bits & 1 or not 0 < info.file_size <= limit:
        raise ValueError('invalid/oversize archive member')
    with archive.open(info) as stream:
        data = stream.read(limit+1)
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != info.file_size or len(data) > limit or digest != expected:
        raise ValueError('unknown resource SHA-256/size')
    return data, {'member': member, 'sha256': digest, 'size': len(data)}


def _script(data):
    text = data.decode('cp1252').replace('\r\r\n', '\n').replace('\r\n', '\n').replace('\r', '\n')
    return re.sub(r'"[^"\r\n]*"|//[^\n]*|/\*[\s\S]*?\*/',
                  lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text)


def sample_original(data0_path, model, track_name, authored_frame, *, pelvis_world=None):
    """Admit one exact original resource/track; return one identified math pose.

    Optional pelvis_world is a caller-supplied native rigid frame, NOT proof of
    a retained live actor, render order, grounding or reversible body ownership.
    """
    if model not in MESH_HASHES or track_name not in TRACKS:
        raise ValueError('unknown model/active lower-body track')
    spec = TRACKS[track_name]
    frame = _number(authored_frame, min(spec.frames), max(spec.frames), 'clip frame')
    if pelvis_world is not None:
        pelvis_world = validate_matrix(pelvis_world)
    with _original(data0_path) as archive:
        script_sources, texts = [], {}
        roots = (f'Data/Player/{model}/{model}.scr', 'Data/Player/Animations/Ray_Animations.scr',
                 'Data/Player/Animations/Ray_Unarmed.scr')
        for root in roots:
            data, source = _read(archive, root, _SCRIPTS[root], MAX_SCRIPT_BYTES)
            script_sources.append(source)
            texts[root] = _script(data)
        for root, target in zip(roots[:2], ('../Animations/Ray_Animations.scr', 'Ray_Unarmed.scr')):
            includes = re.findall(r'!include\s*\(\s*"([^"\r\n]+)"\s*\)', texts[root])
            if includes.count(target) != 1:
                raise ValueError('unproven player animation include chain')
        calls = list(re.finditer(r'(?m)^[ \t]*SeqTrack\s*\(\s*"'+re.escape(track_name)+r'"\s*,([^()]*)\)', texts[roots[2]]))
        if len(calls) != 1:
            raise ValueError('absent/ambiguous active authored track')
        args = [a.strip() for a in calls[0][1].split(',')]
        filename = spec.resource.rsplit('/', 1)[1]
        if len(args) != 6 or args[0] != '"'+filename+'"' or tuple(float(a) for a in args[1:]) != (*spec.frames, spec.parameter, 1., .5):
            raise ValueError('authored track parameters mismatch')
        mesh_name = f'Data/Player/{model}/{model}.msh'
        mesh, mesh_source = _read(archive, mesh_name, MESH_HASHES[model])
        chain = parse_bind_chain(mesh, model)
        data, clip_source = _read(archive, spec.resource, spec.sha256)
        animation = parse_anm(data)
    anchor = chain.nodes[1].world if pelvis_world is None else pelvis_world
    locals_, poses = {}, {}
    for node in chain.nodes:
        pose = animation.sample(node.name.casefold(), frame)
        poses[node.name] = {'translation_cm': pose.translation, 'quaternion_xyzw': pose.quaternion}
        if node.name in _NAMES[2:]:
            locals_[node.name] = pose.matrix()
    rows = recompose_legs(chain, locals_, anchor)
    for row in rows:
        bone = next(b for b in animation.bones if b.name == row['name'].casefold())
        row.update(poses[row['name']], anm_record=bone.record_index, anm_name_hash=bone.name_hash,
                   rotation_mode=bone.mode, record_flags=bone.flags)
    bind_identity = hashlib.sha256(b''.join(struct.pack('<16f', *n.local) for n in chain.nodes)).hexdigest()
    ticks = int(struct.unpack('<f', struct.pack('<f', frame))[0]*160)
    anchor_identity = hashlib.sha256(struct.pack('<16f', *anchor)).hexdigest()
    pose_identity = {
        'contract': 'coj-anm1-10015-type3-native-legs-v1', 'archive': DATA0_SHA256,
        'model': model, 'mesh': chain.sha256, 'bind': bind_identity,
        'clip': animation.sha256, 'track': track_name, 'ticks': ticks,
        'anchor': anchor, 'locals': [row['local_matrix_sha256'] for row in rows],
    }
    return {
        'schema_version': 1, 'mode': 'offline_event_free_sample', 'model': model,
        'pose_sha256': hashlib.sha256(json.dumps(pose_identity, sort_keys=True, separators=(',', ':'), allow_nan=False).encode('ascii')).hexdigest(),
        'identity_encoding': 'bind/anchor: float32 LE; local_matrix: float64 LE; pose: sorted compact ASCII JSON',
        'archive_sha256': DATA0_SHA256, 'runtime_writer_implemented': False,
        'basis': 'native row-major axes; centimetres; local * parent_world; unit scale',
        'binding': {'mesh': mesh_source, 'chain_local_sha256': bind_identity,
                    'nodes': [{'name': n.name, 'mesh_index': n.mesh_index, 'parent_index': n.parent_index} for n in chain.nodes]},
        'clip': {'track': track_name, 'source': clip_source, 'script_chain': script_sources,
                 'definition_line': texts[roots[2]].count('\n', 0, calls[0].start())+1,
                 'authored_frame_interval': spec.frames, 'authored_frame': frame,
                 'native_truncated_ticks': ticks,
                 'authored_parameter': spec.parameter, 'parameter_semantics': spec.parameter_semantics},
        'anchor': {'kind': 'original_bind_pelvis_world' if pelvis_world is None else 'caller_supplied_pelvis_world',
                   'mesh_index': chain.nodes[1].mesh_index, 'world_matrix': anchor,
                   'sha256': anchor_identity},
        'unapplied_authored_root_and_pelvis': {name: poses[name] for name in _NAMES[:2]},
        'legs': rows,
        'limitations': [
            'Pure mathematical sample; no original animation events are evaluated or replayed.',
            'Eight authored leg locals only; root/pelvis channels are diagnostic and are not applied.',
            'No runtime owner/generation, native lifetime, both-eye restore transaction, physical gait eligibility or headset validation.',
            'Caller chooses an absolute authored frame; no looping, clip blending, stick/HMD phase or event clock policy.',
            'Bind matrices identify geometry; runtime use still requires fresh retained actor/local-frame identity.',
        ],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data0-pak', type=Path, required=True, help='explicit original or original backup, read-only')
    parser.add_argument('--model', choices=tuple(MESH_HASHES), required=True)
    parser.add_argument('--track', choices=tuple(TRACKS), required=True)
    parser.add_argument('--frame', type=float, required=True, help='absolute authored frame inside selected track interval')
    args = parser.parse_args(argv)
    try:
        report = sample_original(args.data0_pak, args.model, args.track, args.frame)
        print(json.dumps(report, indent=2, allow_nan=False))
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as exc:
        print(f'animation sample rejected: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
