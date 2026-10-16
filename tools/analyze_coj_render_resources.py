"""Read-only binding census; a complete trace never admits native resizing.

Use a finished, inventoried run log. Markers provide correlation, not authentication.
Pointer identities are local to one scope; they do not prove allocation lifetime.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re

START = re.compile(r'run_start: run_id=([A-Za-z0-9._-]+) build_manifest_id=([a-fA-F0-9]{64})(?:\s|$)')
END = re.compile(r'run_end: run_id=([A-Za-z0-9._-]+)(?:\s|$)')
PREFIX = 'render_resource_probe: '
IDENTITY = ('thread', 'frame', 'owner', 'device', 'generation', 'route')
UNRESOLVED = [
    'Native allocation/destruction, pointer reuse and cache/selection fields',
    'Complete effect graph, shader constants, texel offsets and programmable projections',
    'Desktop/menu/theater and natural-sprite ordering outside these eye scopes',
    'D3D9Ex/D3D11/OpenVR lease and compositor retirement on failure/reset',
    'x86 address-space pressure, effective higher resolution, performance and headset continuity',
]


def integer(obj, key, minimum=0, maximum=2**64-1):
    value = obj.get(key)
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError('Invalid integer: '+key)
    return value


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key')
        result[key] = value
    return result


def reject_constant(_):
    raise ValueError('Nonfinite JSON number')


def resource(value):
    if not isinstance(value, dict) or value.get('valid') is not True or type(value.get('bound')) is not bool:
        raise ValueError('Failed resource read')
    if value['bound']:
        integer(value, 'object', 1)
        integer(value, 'width', 1, 16384)
        integer(value, 'height', 1, 16384)
        for key in ('parent', 'surface', 'format', 'usage', 'pool', 'samples', 'quality', 'depth', 'levels', 'type'):
            integer(value, key)
    return value


def validate_state(state, draw):
    if not isinstance(state, dict) or state.get('valid') is not True:
        raise ValueError('Failed binding snapshot')
    targets = state.get('targets', [])
    textures = state.get('textures', [])
    if len(targets) != integer(state, 'targetSlots', 1, 4) or len(textures) != integer(state, 'textureSlots', 16, 20):
        raise ValueError('Incomplete binding slots')
    if state['textureSlots'] not in (16, 20):
        raise ValueError('Unexpected sampler coverage')
    for target in targets + textures + [state.get('depth')]:
        resource(target)
    if not targets[0]['bound']:
        raise ValueError('No RT0')
    viewport = state.get('viewport')
    if not isinstance(viewport, list) or len(viewport) != 4 or any(type(v) is not int or v < 0 for v in viewport):
        raise ValueError('Invalid viewport')
    if draw:
        x, y, width, height = viewport
        if not width or not height or x+width > targets[0]['width'] or y+height > targets[0]['height']:
            raise ValueError('Draw outside RT0 extent')
    scissor = state.get('scissor')
    if not isinstance(scissor, list) or len(scissor) != 4 or any(type(v) is not int for v in scissor):
        raise ValueError('Invalid scissor metadata')
    integer(state, 'scissorEnabled', 0, 1)
    depth_range = state.get('viewportDepth')
    if not isinstance(depth_range, list) or len(depth_range) != 2 or any(
            type(v) not in (int, float) or not math.isfinite(v) or not 0 <= v <= 1 for v in depth_range):
        raise ValueError('Invalid depth range')
    if depth_range[0] > depth_range[1]:
        raise ValueError('Reversed depth range')
    z = state.get('z')
    if not isinstance(z, list) or len(z) != 3 or any(type(v) is not int for v in z) or z[0] not in (0, 1, 2) or z[1] not in (0, 1) or not 1 <= z[2] <= 8:
        raise ValueError('Invalid Z metadata')
    integer(state, 'vertexShader')
    integer(state, 'pixelShader')
    matrices = state.get('transforms')
    if type(state.get('transformsValid')) is not bool or not isinstance(matrices, list) or len(matrices) != 32 or any(
            type(v) not in (int, float) or not math.isfinite(v) for v in matrices):
        raise ValueError('Invalid transform metadata')


def analyze(text):
    report = dict(schema=1, status='incomplete', readOnly=True, resizingAdmitted=False,
                  coverage='binding_state_only', headsetValidated=False,
                  correlationOnly=True, scopes=[], pairs=[], unresolved=UNRESOLVED[:], errors=[])
    try:
        lines = text.splitlines()
        starts = [(i, START.search(line)) for i, line in enumerate(lines) if 'run_start:' in line]
        ends = [(i, END.search(line)) for i, line in enumerate(lines) if 'run_end:' in line]
        if len(starts) != 1 or len(ends) != 1 or not starts[0][1] or not ends[0][1]:
            raise ValueError('Require one exact run/build interval')
        start, marker = starts[0]
        end, final = ends[0]
        if end <= start or marker[1] != final[1] or marker[1] == 'unbound':
            raise ValueError('Mismatched or reversed run interval')
        report.update(runId=marker[1], buildManifestId=marker[2].lower())
        active = None
        seen = set()
        pending = None
        last_frames = {}
        for index, line in enumerate(lines):
            if 'render_resource_probe_error:' in line:
                raise ValueError('Probe serialization failure')
            if 'render_resource_probe:' in line and PREFIX not in line:
                raise ValueError('Malformed probe line')
            if PREFIX not in line:
                continue
            if not start < index < end:
                raise ValueError('Probe outside run interval')
            event = json.loads(line.split(PREFIX, 1)[1], object_pairs_hook=unique_object,
                               parse_constant=reject_constant)
            if not isinstance(event, dict) or event.get('schema') != 1 or type(event.get('schema')) is not int:
                raise ValueError('Unsupported probe schema')
            if event.get('readOnly') is not True or event.get('resizingAdmitted') is not False or event.get('coverage') != 'binding_state_only':
                raise ValueError('Unexpected authority or coverage')
            for key in IDENTITY:
                integer(event, key, 0 if key == 'route' else 1, 2 if key == 'route' else 2**64-1)
            scope = integer(event, 'scope', 1)
            eye = integer(event, 'eye', 0, 1)
            identity = tuple(event[key] for key in IDENTITY)
            key = (event['thread'], scope)
            kind = event.get('event')
            if kind == 'begin':
                if active or key in seen or len(seen) >= 12:
                    raise ValueError('Nested, duplicate or over-budget scope')
                seen.add(key)
                active = dict(key=key, identity=identity, eye=eye, calls=0, records=0,
                              drawCalls=0, drawExtents=set(), operations=Counter(), bindings={})
            elif not active or key != active['key'] or identity != active['identity'] or eye != active['eye']:
                raise ValueError('Orphan scope or changed identity')
            elif kind == 'record':
                if active['records'] >= 4096 or integer(event, 'ordinal', 1) != active['calls']+1:
                    raise ValueError('Record count/order exceeds scope')
                repeat = integer(event, 'repeat', 1)
                operation = integer(event, 'op', 0, 10)
                if integer(event, 'hr', 0, 2**32-1) >= 2**31:
                    raise ValueError('Native graphics call failed')
                integer(event, 'caller', 1)
                for field in ('selector', 'source', 'destination'):
                    integer(event, field)
                draw = operation in (6, 7, 8, 9)
                state = event.get('state')
                validate_state(state, draw)
                active['calls'] += repeat
                active['records'] += 1
                active['operations'][operation] += repeat
                if draw:
                    active['drawCalls'] += repeat
                    active['drawExtents'].add(tuple(state['viewport'][2:]))
                for binding in state['targets'] + state['textures'] + [state['depth']]:
                    if binding['bound']:
                        # Include descriptor in the key: a reused address is never merged into a lifetime.
                        active['bindings'][json.dumps(binding, sort_keys=True)] = binding
            elif kind == 'end':
                if event.get('complete') is not True or event.get('truncated') is not False or integer(event, 'readFailures'):
                    raise ValueError('Incomplete/truncated observation')
                if integer(event, 'calls') != active['calls'] or integer(event, 'records') != active['records'] or not active['drawCalls']:
                    raise ValueError('Count mismatch or no draw evidence')
                result = dict(zip(IDENTITY, identity), scope=scope, eye=eye,
                              calls=active['calls'], records=active['records'], drawCalls=active['drawCalls'],
                              drawExtents=[list(v) for v in sorted(active['drawExtents'])],
                              operations=dict(active['operations']), bindings=list(active['bindings'].values()))
                report['scopes'].append(result)
                if eye == 0:
                    if pending:
                        raise ValueError('Unmatched left eye')
                    pending = result
                else:
                    if not pending or any(pending[field] != result[field] for field in IDENTITY):
                        raise ValueError('Unmatched/mismatched/reversed right eye')
                    if result['frame'] <= last_frames.get(result['thread'], 0):
                        raise ValueError('Duplicate or reversed pair frame')
                    last_frames[result['thread']] = result['frame']
                    report['pairs'].append(dict(frame=result['frame'], leftScope=pending['scope'], rightScope=scope))
                    pending = None
                active = None
            else:
                raise ValueError('Unknown probe event')
        if active or pending or not report['pairs']:
            raise ValueError('Missing complete eye pair')
        report['status'] = 'observed'
    except (ValueError, TypeError, KeyError, AttributeError, OverflowError) as error:
        report['errors'].append(str(error))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve() == args.log.resolve():
        parser.error('Output must not overwrite source evidence')
    report = analyze(args.log.read_text(encoding='utf-8-sig'))
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(report['status']+'; resizingAdmitted=false; headsetValidated=false')
    return 0 if report['status'] == 'observed' else 2


if __name__ == '__main__':
    raise SystemExit(main())
