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
NULL_COLOR_FORMAT = int.from_bytes(b'NULL', 'little')
# Exact CoJ census profile. This classifies observations, not a generic driver
# capability, verified shadow contents or permission to replace these resources.
NULL_DEPTH_FORMAT = 80  # D3DFMT_D16
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
    if not draw:
        return None
    color, depth = targets[0], state['depth']
    kind, raster = 'color', color
    if color['format'] == NULL_COLOR_FORMAT:
        # NULL is a vendor color placeholder. Its advertised color extent is
        # not a pixel-bearing scene target. Admit only the observed depth-only
        # layout, retaining strict viewport/depth and metadata checks.
        if (color['width'] != 1 or color['height'] != 1 or color['usage'] != 1 or
                color['pool'] != 0 or color['type'] != 1 or color['samples'] or color['quality'] or
                any(target['bound'] for target in targets[1:]) or not depth['bound'] or
                depth['format'] != NULL_DEPTH_FORMAT or depth['usage'] != 2 or
                depth['pool'] != 0 or depth['type'] != 1 or depth['samples'] or depth['quality'] or
                z[0] != 1 or z[1] != 1):
            raise ValueError('Unqualified NULL color/depth layout')
        kind, raster = 'null_color_depth', depth
    x, y, width, height = viewport
    if not width or not height or x+width > raster['width'] or y+height > raster['height']:
        raise ValueError('Draw outside '+('depth' if kind == 'null_color_depth' else 'RT0')+' extent')
    return dict(kind=kind, rasterExtent=[raster['width'], raster['height']],
                colorExtent=[color['width'], color['height']], colorFormat=color['format'],
                depthExtent=[depth['width'], depth['height']] if depth['bound'] else None,
                depthFormat=depth['format'] if depth['bound'] else None, viewport=viewport)


def analyze_sprite_ordering(lines, start, end, scopes):
    """Correlate sampled native metadata; never authorize a desktop write."""
    result = dict(status='incomplete', coverage='sampled_pair_sprite_order',
                  correlationOnly=True, mirrorAdmitted=False, generationQualified=False, passes=[],
                  matchedFrames=[], unobservedFrames=sorted({s['frame'] for s in scopes}), errors=[])
    right = [s for s in scopes if s['eye'] == 1]
    active = None
    seen = set()
    counts = Counter()
    stages = ('sprites_begin', 'sprites_end', 'flush_begin', 'flush_end')

    def number(fields, key, pointer=False, minimum=0, maximum=2**64-1):
        value = fields.get(key, '')
        if not re.fullmatch(r'0x[0-9a-fA-F]+' if pointer else r'[0-9]+', value):
            raise ValueError('Invalid sprite field: '+key)
        parsed = int(value, 16 if pointer else 10)
        if not minimum <= parsed <= maximum:
            raise ValueError('Out-of-range sprite field: '+key)
        return parsed

    try:
        for index, line in enumerate(lines):
            if 'native_hud_boundary:' not in line:
                continue
            if not start < index < end:
                raise ValueError('Sprite observation outside run interval')
            fields = {}
            for item in line.split('native_hud_boundary:', 1)[1].strip().split(';'):
                key, separator, value = item.partition('=')
                if not separator or key in fields or not key or not value:
                    raise ValueError('Malformed or duplicate sprite field')
                fields[key] = value
            frame = number(fields, 'frame_sequence')
            candidates = [s for s in right if s['frame'] == frame]
            if not candidates:
                if active:
                    raise ValueError('Unrelated sprite frame interleaved within pass')
                continue  # Independent sampling; unrelated frames prove no pair order.
            thread = number(fields, 'thread_id', minimum=1, maximum=2**32-1)
            candidates = [s for s in candidates if s['thread'] == thread]
            if len(candidates) != 1:
                raise ValueError('Sprite thread does not match sampled pair')
            pair = candidates[0]
            if index+1 <= pair['endLine']:
                raise ValueError('Sprite observation precedes completed right eye')
            if any(s['thread'] == thread and pair['endLine'] < s['beginLine'] <= index+1
                   for s in scopes):
                raise ValueError('Stale sprite frame after a later sampled eye scope')
            if fields.get('target_observation') == 'unavailable':
                raise ValueError('Sprite target observation unavailable')
            device = number(fields, 'device', True, 1, 2**32-1)
            generation = number(fields, 'generation', minimum=1)
            target = number(fields, 'rt', True, 1, 2**32-1)
            final = pair['lastObservedTarget']
            # The original eye collector's generation is a continuity epoch
            # (native generation + reset attempts), not the HUD device counter.
            # Never infer an offset/ratio from matching pointer values.
            native_generation = pair.get('deviceGeneration')
            if (device != pair['device'] or
                    (native_generation is not None and generation != native_generation) or
                    target != final['object'] or final['format'] != 21):
                raise ValueError('Sprite device/generation/target differs from eye exit')
            for key in ('rt_hr', 'desc_hr', 'viewport_hr'):
                if number(fields, key, True, maximum=2**32-1) != 0:
                    raise ValueError('Sprite target query did not return S_OK')
            if (number(fields, 'option') != 0 or number(fields, 'format') != 21 or
                    fields.get('size') != f"{final['width']},{final['height']}" or
                    fields.get('viewport') != f"0,0,{final['width']},{final['height']}"):
                raise ValueError('Unqualified sprite option/format/extent/viewport')
            states = tuple(number(fields, key, maximum=maximum) for key, maximum in
                           (('alpha_blend', 1), ('src_blend', 17), ('dst_blend', 17),
                            ('color_write', 15), ('separate_alpha', 1), ('depth', 2)))
            if not states[1] or not states[2]:
                raise ValueError('Invalid sprite blend metadata')
            sequence = number(fields, 'pass_sequence', minimum=1)
            present = number(fields, 'present_sequence')
            owner = number(fields, 'owner', True, 1, 2**32-1)
            key = (thread, sequence)
            context = (frame, thread, device, generation, target, present)
            stage = fields.get('stage')
            if stage == stages[0]:
                if active or key in seen or counts[(thread, frame)] >= 32:
                    raise ValueError('Nested/duplicate/over-budget sprite pass')
                seen.add(key)
                counts[(thread, frame)] += 1
                active = dict(key=key, context=context, next=1, beginLine=index+1,
                              spriteOwner=owner, flushOwner=None, states=[list(states)])
            elif not active or active['key'] != key or active['context'] != context:
                raise ValueError('Orphan sprite stage or changed context')
            elif stage != stages[active['next']]:
                raise ValueError('Reversed/unknown sprite stage')
            else:
                if stage == 'sprites_end' and owner != active['spriteOwner']:
                    raise ValueError('Sprite owner changed within traversal')
                if stage == 'flush_begin':
                    active['flushOwner'] = owner
                if stage == 'flush_end' and owner != active['flushOwner']:
                    raise ValueError('Renderer owner changed within flush')
                active['states'].append(list(states))
                active['next'] += 1
                if stage == 'flush_end':
                    result['passes'].append(dict(frame=frame, thread=thread, device=device,
                        route=pair['route'],
                        deviceGeneration=generation, probeEpoch=pair['generation'],
                        generationQualified=native_generation is not None,
                        presentSequence=present, passSequence=sequence,
                        target=target, extent=[final['width'], final['height']],
                        beginLine=active['beginLine'], endLine=index+1, eyeEndLine=pair['endLine'],
                        spriteOwner=active['spriteOwner'], flushOwner=active['flushOwner'],
                        stageStates=active['states']))
                    active = None
        if active:
            raise ValueError('Unfinished sprite/flush pass')
        if not result['passes']:
            raise ValueError('No complete sprite pass correlated to sampled eye pair')
        if not all(p['generationQualified'] for p in result['passes']):
            raise ValueError('Native device generation absent from eye scope')
        result['generationQualified'] = True
        result['status'] = 'observed'
    except (ValueError, TypeError, KeyError, AttributeError, OverflowError) as error:
        result['errors'].append(str(error))
    result['matchedFrames'] = sorted({p['frame'] for p in result['passes']})
    result['unobservedFrames'] = sorted({s['frame'] for s in right} - set(result['matchedFrames']))
    return result


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
            device_generation = (integer(event, 'deviceGeneration', 1)
                                 if 'deviceGeneration' in event else None)
            key = (event['thread'], scope)
            kind = event.get('event')
            if kind == 'begin':
                if active or key in seen or len(seen) >= 12:
                    raise ValueError('Nested, duplicate or over-budget scope')
                seen.add(key)
                active = dict(key=key, identity=identity, eye=eye, deviceGeneration=device_generation,
                              calls=0, records=0, beginLine=index+1,
                              drawCalls=0, drawExtents=set(), drawPasses={}, operations=Counter(), bindings={})
            elif (not active or key != active['key'] or identity != active['identity'] or
                  eye != active['eye'] or device_generation != active['deviceGeneration']):
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
                raster_pass = validate_state(state, draw)
                active['lastObservedTarget'] = state['targets'][0]
                active['calls'] += repeat
                active['records'] += 1
                active['operations'][operation] += repeat
                if draw:
                    active['drawCalls'] += repeat
                    active['drawExtents'].add(tuple(state['viewport'][2:]))
                    pass_key = json.dumps(raster_pass, sort_keys=True)
                    if pass_key not in active['drawPasses']:
                        active['drawPasses'][pass_key] = dict(raster_pass, drawCalls=0)
                    active['drawPasses'][pass_key]['drawCalls'] += repeat
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
                              beginLine=active['beginLine'], endLine=index+1,
                              deviceGeneration=device_generation,
                              lastObservedTarget=active['lastObservedTarget'],
                              calls=active['calls'], records=active['records'], drawCalls=active['drawCalls'],
                              drawExtents=[list(v) for v in sorted(active['drawExtents'])],
                              drawPasses=list(active['drawPasses'].values()),
                              operations=dict(active['operations']), bindings=list(active['bindings'].values()))
                report['scopes'].append(result)
                if eye == 0:
                    if pending:
                        raise ValueError('Unmatched left eye')
                    pending = result
                else:
                    if (not pending or any(pending[field] != result[field] for field in IDENTITY) or
                            pending['deviceGeneration'] != result['deviceGeneration']):
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
        report['desktopSpriteOrdering'] = analyze_sprite_ordering(lines, start, end, report['scopes'])
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
    sprite_status = report.get('desktopSpriteOrdering', {}).get('status', 'not_evaluated')
    print(report['status']+'; resizingAdmitted=false; headsetValidated=false; spriteOrdering='+sprite_status)
    return 0 if report['status'] == 'observed' else 2


if __name__ == '__main__':
    raise SystemExit(main())
