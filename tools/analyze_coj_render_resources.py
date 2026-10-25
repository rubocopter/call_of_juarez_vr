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
import struct

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


NATIVE_PREFIX = 'native_ppfx_resource_probe: '
# Recognize the namespace before validating its delimiter, so malformed native
# lines cannot expose marker-like byte names to the independent parsers.
NATIVE_LINE = re.compile(r'^[^{]*?(native_ppfx_resource_probe(?:_error)?)\b')
NATIVE_COVERAGE = 'native_ppfx_structural_snapshot'
U32 = 2**32-1
NATIVE_VOLATILE = ('currentTarget', 'pixelSamplerHandles', 'vertexSamplerHandles')
NATIVE_SURFACES = ('defaultColorSurface', 'defaultDepthSurface', 'internalColorSurface',
                   'internalDepthSurface', 'additionalDepthSurface')


def native_component():
    return dict(status='incomplete', coverage=NATIVE_COVERAGE, readOnly=True,
                resizingAdmitted=False, correlationOnly=True, headsetValidated=False,
                lifetimeQualified=False, atomicSnapshot=False, rolesQualified=False,
                completeAllocationGraph=False, scopes=[], errors=[])


def native_pointer(obj, key, optional=False):
    # All bits are significant, including the LAA high bit. No dereference or COM call.
    return integer(obj, key, 0 if optional else 1, U32-1)


def native_array(obj, key, count):
    values = obj.get(key)
    if type(values) is not list or len(values) != count:
        raise ValueError('Invalid native array: '+key)
    return values


def native_name(obj, capacity):
    name = obj.get('name')
    if (type(name) is not str or not 0 < len(name) < capacity or
            any(not 0 < ord(char) <= 255 for char in name)):
        raise ValueError('Invalid bounded native byte name')


def native_fields(obj, fields):
    if type(obj) is not dict or set(obj) != set(fields.split()):
        raise ValueError('Missing/foreign native structural fields')


def validate_native_snapshot(event):
    """Validate copied structure, never an allocation graph or an atomic snapshot."""
    if type(event) is not dict or integer(event, 'schema', 1, 1) != 1:
        raise ValueError('Unsupported native schema')
    native_fields(event, '''schema phase coverage readOnly resizingAdmitted scope thread frame owner
        device deviceGeneration generation eye route bindingScopeComplete complete truncated status
        engineBase renderer manager nativeDevice rendererVtable managerVtable listSentinel loadedEffectCount
        globalBufferCount managerDirty effectCount bufferCount rendererState effects buffers''')
    if (event.get('coverage') != NATIVE_COVERAGE or event.get('readOnly') is not True or
            event.get('resizingAdmitted') is not False):
        raise ValueError('Unexpected native authority or coverage')
    phase = event.get('phase')
    if phase not in ('begin', 'end') or event.get('bindingScopeComplete') is not (phase == 'end'):
        raise ValueError('Invalid native phase/binding completion')
    if (event.get('complete') is not True or event.get('truncated') is not False or
            integer(event, 'status', 0, 14) != 0 or integer(event, 'managerDirty', 0, 255) != 0):
        raise ValueError('Incomplete/truncated/dirty native snapshot')
    for key in ('scope', 'frame', 'generation', 'deviceGeneration'):
        integer(event, key, 1)
    integer(event, 'thread', 1, U32)
    integer(event, 'eye', 0, 1)
    integer(event, 'route', 0, 2)
    for key in ('owner', 'device', 'engineBase', 'renderer', 'manager', 'nativeDevice', 'listSentinel'):
        native_pointer(event, key)
    if event['device'] != event['nativeDevice']:
        raise ValueError('Native device differs from binding context')
    base = event['engineBase']
    if base + 0x5902af >= U32:
        raise ValueError('Native image address overflow')
    for key, offset in (('rendererVtable', 0x312334), ('managerVtable', 0x314c48)):
        if native_pointer(event, key) != base + offset:
            raise ValueError('Foreign native vtable: '+key)
    effects = native_array(event, 'effects', integer(event, 'effectCount', 0, 16))
    buffers = native_array(event, 'buffers', integer(event, 'bufferCount', 0, 272))
    if integer(event, 'loadedEffectCount', 0, 16) != len(effects):
        raise ValueError('Native loaded-effect count mismatch')
    containers = [(event['manager'], integer(event, 'globalBufferCount', 0, 16), True)]
    nodes, owners = set(), {event['manager']}
    sentinel = event['listSentinel']
    previous = sentinel
    for index, effect in enumerate(effects):
        native_fields(effect, 'node next previous effect vtable bufferCount name')
        for key in ('node', 'next', 'previous', 'effect', 'vtable'):
            native_pointer(effect, key)
        node, owner = effect['node'], effect['effect']
        following = effects[index+1].get('node') if index+1 < len(effects) and type(effects[index+1]) is dict else sentinel
        if (node == sentinel or node in nodes or owner in owners or
                effect['previous'] != previous or effect['next'] != following):
            raise ValueError('Invalid/duplicate/cyclic native topology')
        if effect['vtable'] != base + 0x3180ac:
            raise ValueError('Foreign native effect vtable')
        native_name(effect, 260)
        containers.append((owner, integer(effect, 'bufferCount', 0, 16), False))
        nodes.add(node)
        owners.add(owner)
        previous = node
    if sum(count for _, count, _ in containers) != len(buffers):
        raise ValueError('Native global/effect buffer count mismatch')
    position = 0
    textures = {}
    for container, count, global_ in containers:
        if container + 0x647 >= U32:
            raise ValueError('Native container address overflow')
        for index in range(count):
            buffer = buffers[position]
            position += 1
            native_fields(buffer, '''container entry index global name logicalWidth logicalHeight rgbBits
                alphaBits widthDivisor heightDivisor internalColor depthAttachment nativeBpp tempShareable
                linkedBorrowed texture''')
            if (native_pointer(buffer, 'container') != container or
                    integer(buffer, 'index', 0, 15) != index or buffer.get('global') is not global_ or
                    native_pointer(buffer, 'entry') != container + 4 + index*0x64):
                raise ValueError('Native container/index/entry mismatch')
            native_name(buffer, 64)
            for key in ('logicalWidth', 'logicalHeight'):
                integer(buffer, key, 1, 2**31-1)
            for key in ('rgbBits', 'alphaBits', 'widthDivisor', 'heightDivisor'):
                integer(buffer, key, -2**31, 2**31-1)
            # These are raw native byte options, not normalized boolean fields.
            for key in ('internalColor', 'depthAttachment', 'nativeBpp', 'tempShareable', 'linkedBorrowed'):
                integer(buffer, key, 0, 255)
            texture = buffer.get('texture')
            native_fields(texture, '''handle selectedInterface texture2d textureCube textureVolume usage
                format flags kind requestedWidth requestedHeight requestedDepth width height depth inverseSizeBits''')
            handle = native_pointer(texture, 'handle')
            if handle + 0x15f >= U32:
                raise ValueError('Native texture address overflow')
            for key in ('texture2d', 'textureCube', 'textureVolume'):
                native_pointer(texture, key, optional=True)
            kind = integer(texture, 'kind', 0, 2)
            selected = native_pointer(texture, 'selectedInterface')
            if selected != texture[('texture2d', 'textureVolume', 'textureCube')[kind]]:
                raise ValueError('Unallocated/inconsistent native texture interface')
            for key in ('usage', 'format', 'flags'):
                integer(texture, key, 0, U32)
            for key in ('width', 'height', 'requestedWidth', 'requestedHeight'):
                integer(texture, key, 1, U32)
            for key in ('depth', 'requestedDepth'):
                integer(texture, key, 1 if kind == 1 else 0, U32)
            bits = native_array(texture, 'inverseSizeBits', 3)
            for axis, word in enumerate(bits):
                integer({'bits': word}, 'bits', 0, U32)
                value = struct.unpack('<f', struct.pack('<I', word))[0]
                # Native 2D creation can divide by depth zero. This unused
                # reciprocal is retained as bits; only volume depth qualifies.
                if (axis < 2 or kind == 1) and (not math.isfinite(value) or value <= 0):
                    raise ValueError('Invalid native reciprocal dimension')
            if handle in textures and textures[handle] != texture:
                raise ValueError('Conflicting native aliases for one handle')
            textures[handle] = texture
    state = event.get('rendererState')
    native_fields(state, '''logicalWidth logicalHeight viewportClampWidth viewportClampHeight currentTarget
        defaultColorSurface defaultDepthSurface internalColorSurface internalDepthSurface additionalDepthSurface
        placeholderTexture pixelSamplerHandles vertexSamplerHandles''')
    for key in ('logicalWidth', 'logicalHeight', 'viewportClampWidth', 'viewportClampHeight'):
        integer(state, key, 1, U32)
    for key in NATIVE_SURFACES + ('currentTarget', 'placeholderTexture'):
        native_pointer(state, key, optional=True)
    for key, size in (('pixelSamplerHandles', 16), ('vertexSamplerHandles', 4)):
        for value in native_array(state, key, size):
            native_pointer({'handle': value}, 'handle', optional=True)


def correlate_native_scope(scope, begin, end):
    """Match values only. Native handles and sampler handles are never COM identities."""
    aliases = {}
    sources = {}
    for index, buffer in enumerate(begin['buffers']):
        handle = buffer['texture']['handle']
        group = aliases.setdefault(handle, dict(nativeHandle=handle, names=[], bufferIndices=[]))
        group['names'].append(buffer['name'])
        group['bufferIndices'].append(index)
        for key in ('texture2d', 'textureCube', 'textureVolume'):
            value = buffer['texture'][key]
            if value:
                sources.setdefault(value, []).append((f'buffers[{index}].texture.{key}', True, index))
    for key in NATIVE_SURFACES + ('placeholderTexture',):
        value = begin['rendererState'][key]
        if value:
            sources.setdefault(value, []).append(('rendererState.'+key, key == 'placeholderTexture', None))
    matches, unknowns, used = [], [], set()
    for index, binding in enumerate(scope['bindings']):
        obj = native_pointer(binding, 'object')
        parent = native_pointer(binding, 'parent', optional=True)
        found = []
        for value in {obj, parent} - {0}:
            for path, texture, buffer_index in sources.get(value, []):
                if value == obj or texture:
                    found.append(path)
                    if buffer_index is not None:
                        used.add(buffer_index)
        row = dict(bindingIndex=index, object=obj, parent=parent)
        if found:
            matches.append(dict(row, sources=sorted(found)))
        else:
            unknowns.append(row)
    return dict(scope=scope['scope'], thread=scope['thread'], frame=scope['frame'], eye=scope['eye'],
                beginSnapshot=begin, endSnapshot=end, aliases=list(aliases.values()), matches=matches,
                unknowns=unknowns, unusedBuffers=sorted(set(range(len(begin['buffers']))) - used))


def analyze_native_postprocess(lines, start, end, scopes):
    result = native_component()
    snapshots = {}
    count = 0
    line_number = 0
    try:
        if not 0 < len(scopes) <= 12:
            raise ValueError('Native correlation requires bounded complete binding scopes')
        selected = {(scope['thread'], scope['scope']): scope for scope in scopes}
        for index, line in enumerate(lines):
            line_number = index+1
            marker = NATIVE_LINE.match(line)
            if marker is None:
                continue
            if marker[1] == 'native_ppfx_resource_probe_error':
                raise ValueError('Native probe serialization failure')
            if not start < index < end:
                raise ValueError('Native snapshot outside run interval')
            if line[marker.end():marker.end()+2] != ': ':
                raise ValueError('Malformed native probe prefix')
            count += 1
            if count > 24:
                raise ValueError('Native snapshot budget exceeded')
            event = json.loads(line[marker.end()+2:], object_pairs_hook=unique_object,
                               parse_constant=reject_constant)
            validate_native_snapshot(event)
            scope = selected.get((event['thread'], event['scope']))
            if scope is None or any(event[key] != scope.get(key) for key in
                                    IDENTITY + ('eye', 'deviceGeneration')):
                raise ValueError('Foreign native scope identity/device generation')
            key = (event['thread'], event['scope'], event['phase'])
            if key in snapshots:
                raise ValueError('Duplicate native phase')
            if event['phase'] == 'begin':
                prior_end = max([start+1] + [s['endLine'] for s in scopes if
                    s['thread'] == scope['thread'] and s['endLine'] < scope['beginLine']])
                if not prior_end < line_number < scope['beginLine']:
                    raise ValueError('Native begin outside preceding binding boundary')
            else:
                if not scope['beginLine'] < line_number < scope['endLine']:
                    raise ValueError('Native end outside binding scope')
                if (event['thread'], event['scope'], 'begin') not in snapshots:
                    raise ValueError('Native end precedes begin')
            snapshots[key] = dict(event, line=line_number)
        for scope in scopes:
            key = (scope['thread'], scope['scope'])
            if key+('begin',) not in snapshots or key+('end',) not in snapshots:
                raise ValueError('Missing native begin/end snapshots')
            begin, finish = snapshots[key+('begin',)], snapshots[key+('end',)]
            def family(snapshot):
                stable = {k: v for k, v in snapshot.items() if k not in
                          ('phase', 'bindingScopeComplete', 'line', 'rendererState')}
                stable['rendererState'] = {k: v for k, v in snapshot['rendererState'].items()
                                           if k not in NATIVE_VOLATILE}
                return stable
            if family(begin) != family(finish):
                raise ValueError('Native family/topology/cache changed within scope')
            result['scopes'].append(correlate_native_scope(scope, begin, finish))
        result['status'] = 'observed'
    except (ValueError, TypeError, KeyError, AttributeError, OverflowError, RecursionError) as error:
        result['errors'].append(f'Line {line_number}: {error}')
    return result


HUD_LINE = re.compile(r'^[^{]*?(hud_batch_probe(?:_error)?)\b')
HUD_KEYS = (27,19,20,171,193,206,207,208,209,168,190,191,192,
            15,24,25,7,14,23,195,175,174,194)
HUD_IDENTITY = ('device', 'nativeGeneration', 'continuityEpoch', 'drawHookEpoch',
                'nativeHookEpoch', 'cachedPermissionEpoch', 'frame', 'present', 'thread', 'pass')
HUD_FAULTS = ('nested', 'identity_changed', 'bad_order', 'read_failed', 'draw_failed',
              'truncated', 'unterminated', 'native_fault', 'disabled', 'contended', 'missing_flush_draw')


def hud_marker(line):
    marker = HUD_LINE.match(line)
    native = NATIVE_LINE.match(line)
    # Even a malformed non-JSON message belongs to its first namespace. Later
    # marker-like text is payload, not a second diagnostic row.
    if marker and (native is None or marker.start(1) < native.start(1)):
        return marker
    return None


def hud_component():
    return dict(status='incomplete', coverage='hud_natural_pass', readOnly=True,
                correlationOnly=True, captureAdmitted=False, occlusionAdmitted=False,
                headsetValidated=False, route='unknown', nativeCaller='unknown',
                rows=[], passes=[], errors=[])


def hud_fields(obj, fields):
    if type(obj) is not dict or set(obj) != set(fields.split()):
        raise ValueError('Missing/foreign HUD fields')


def hud_flag(obj, key):
    # DrainHudBatch has no boolalpha: native bools are JSON integers, while its
    # explicitly authored safety claims are literal JSON booleans.
    return integer(obj, key, 0, 1)


def hud_float(text):
    value = float(text)
    if not math.isfinite(value):
        raise ValueError('Nonfinite HUD JSON number')
    return value


def hud_array(obj, key, count):
    values = obj.get(key)
    if type(values) is not list or len(values) != count:
        raise ValueError('Invalid HUD array: '+key)
    return values


def hud_resource(value):
    hud_fields(value, '''object parent surface width height depth levels format usage pool
        samples quality type bound valid''')
    bound, valid = hud_flag(value, 'bound'), hud_flag(value, 'valid')
    for key in ('object', 'parent', 'surface'):
        native_pointer(value, key, optional=True)
    if bool(value['object']) != bool(bound):
        raise ValueError('HUD resource object/bound mismatch')
    for key in ('width', 'height'):
        integer(value, key, 1 if bound and valid else 0, 16384)
    for key in ('depth', 'levels', 'format', 'usage', 'pool', 'samples', 'quality', 'type'):
        integer(value, key, 0, U32)
    return bool(valid)


def hud_snapshot(record):
    valid = hud_flag(record, 'valid')
    reads = [hud_resource(record[key]) for key in ('rt0', 'depth', 'texture0')]
    reads.append(bool(record['rt0']['bound']))
    for key in ('vertexShader', 'pixelShader'):
        shader = hud_array(record, key, 3)
        native_pointer({'object': shader[0]}, 'object', optional=True)
        integer({'bytes': shader[1]}, 'bytes', 0, U32)
        reads.append(bool(hud_flag({'valid': shader[2]}, 'valid')))
        if shader[2] and bool(shader[0]) != bool(shader[1]):
            raise ValueError('HUD shader object/byte-size mismatch')
    viewport = hud_array(record, 'viewport', 4)
    for value in viewport:
        integer({'coordinate': value}, 'coordinate', 0, U32)
    vrange = hud_array(record, 'viewportDepth', 2)
    viewport_valid = hud_flag(record, 'viewportValid')
    # The producer writes null for a nonfinite returned scalar, even when a
    # failed getter left partial output. Preserve that truthful failure without
    # allowing null or an invalid range to qualify a successful viewport read.
    if any(v is not None and (type(v) not in (int, float) or not math.isfinite(v)) for v in vrange):
        raise ValueError('Invalid HUD viewport depth')
    if viewport_valid and any(v is None or not 0 <= v <= 1 for v in vrange):
        raise ValueError('Invalid HUD viewport depth')
    if viewport_valid and vrange[0] > vrange[1]:
        raise ValueError('Reversed HUD viewport depth')
    reads.append(bool(viewport_valid))
    if viewport_valid:
        x, y, width, height = viewport
        raster = record['rt0']
        if raster['format'] == NULL_COLOR_FORMAT and record['depth']['bound']:
            raster = record['depth']
        if (not width or not height or x+width > U32 or y+height > U32 or
                (raster['valid'] and raster['bound'] and
                 (x+width > raster['width'] or y+height > raster['height']))):
            raise ValueError('HUD viewport outside observed extent')
    for value in hud_array(record, 'scissor', 4):
        integer({'coordinate': value}, 'coordinate', -2**31, 2**31-1)
    reads.append(bool(hud_flag(record, 'scissorValid')))
    keys = hud_array(record, 'stateKeys', len(HUD_KEYS))
    for value in keys:
        integer({'key': value}, 'key', 0, U32)
    if tuple(keys) != HUD_KEYS:
        raise ValueError('HUD state keys differ from frozen producer order')
    for value in hud_array(record, 'states', len(HUD_KEYS)):
        integer({'state': value}, 'state', 0, U32)
    for value in hud_array(record, 'stateValid', len(HUD_KEYS)):
        reads.append(bool(hud_flag({'valid': value}, 'valid')))
    if bool(valid) != all(reads):
        raise ValueError('Untruthful HUD aggregate snapshot validity')
    return bool(valid)


def validate_hud_pass(event):
    """Return incomplete reasons; structural contradictions raise ValueError.

    Emission implies closed storage, not a matched flush end. Aborts retain zero
    end owners/default witnesses and queried-but-unreturned samples truthfully.
    """
    hud_fields(event, '''schema event pass device nativeGeneration continuityEpoch drawHookEpoch
        nativeHookEpoch cachedPermissionEpoch frame present thread spriteOwner spriteEndOwner
        flushOwner flushEndOwner option flushEndOption route nativeCaller complete faults draws
        samples flushDraws correlationOnly captureAdmitted occlusionAdmitted boundaries records''')
    integer(event, 'schema', 1, 1)
    if (event['event'] != 'hud_natural_pass' or event['route'] != 'unknown' or
            event['nativeCaller'] != 'unknown' or event['correlationOnly'] is not True or
            event['captureAdmitted'] is not False or event['occlusionAdmitted'] is not False):
        raise ValueError('Unexpected HUD schema/authority/route/caller')
    for key in HUD_IDENTITY:
        if key == 'device':
            native_pointer(event, key)
        else:
            integer(event, key, 1, U32 if key == 'thread' else 2**64-1)
    if event['continuityEpoch'] < event['nativeGeneration']:
        raise ValueError('HUD continuity epoch precedes native generation')
    native_pointer(event, 'spriteOwner')
    for key in ('spriteEndOwner', 'flushOwner', 'flushEndOwner'):
        native_pointer(event, key, optional=True)
    for key in ('option', 'flushEndOption'):
        integer(event, key, 0, U32)
    complete = hud_flag(event, 'complete')
    faults = integer(event, 'faults', 0, (1 << len(HUD_FAULTS))-1)
    draws = integer(event, 'draws')
    samples = integer(event, 'samples', 0, 64)
    flush_draws = integer(event, 'flushDraws', 0, draws)
    records = event['records']
    if type(records) is not list or len(records) > 64 or not len(records) <= samples <= min(draws, 64):
        raise ValueError('HUD sample/record budget or count mismatch')
    boundaries = hud_array(event, 'boundaries', 4)
    closure = True
    for boundary, depths in zip(boundaries, ((1,0), (1,0), (0,1), (0,1))):
        hud_fields(boundary, 'spriteDepth flushDepth epoch intact faulted')
        for key in ('spriteDepth', 'flushDepth'):
            integer(boundary, key, 0, U32)
        integer(boundary, 'epoch')
        intact, faulted = hud_flag(boundary, 'intact'), hud_flag(boundary, 'faulted')
        closure &= ((boundary['spriteDepth'], boundary['flushDepth']) == depths and
                    boundary['epoch'] == event['nativeHookEpoch'] and intact == 1 and faulted == 0)
    closure &= (event['spriteEndOwner'] == event['spriteOwner'] and event['flushOwner'] != 0 and
                event['flushEndOwner'] == event['flushOwner'] and event['flushEndOption'] == event['option'])
    represented = flush_samples = 0
    last_phase = 0
    read_failed = draw_failed = False
    for record in records:
        hud_fields(record, '''ordinal repeat phase operation result drawIngressAddress nativeOwner option
            spriteDepth flushDepth nativeHookEpoch rt0 depth texture0 vertexShader pixelShader viewport
            viewportDepth viewportValid scissorValid scissor stateKeys states stateValid valid''')
        if integer(record, 'ordinal', 1, 64) != represented+1:
            raise ValueError('HUD record ordinal gap/duplicate/reversal')
        repeat = integer(record, 'repeat', 1, 64)
        represented += repeat
        phase = integer(record, 'phase', 0, 2)
        if phase < last_phase:
            raise ValueError('Reversed HUD record phase')
        last_phase = phase
        integer(record, 'operation', 6, 9)
        draw_failed |= integer(record, 'result', -2**31, 2**31-1) < 0
        native_pointer(record, 'drawIngressAddress')
        native_pointer(record, 'nativeOwner', optional=phase == 1)
        integer(record, 'option', 0, U32)
        integer(record, 'nativeHookEpoch', 1)
        for key in ('spriteDepth', 'flushDepth'):
            integer(record, key, 0, U32)
        owner = (event['spriteOwner'], 0, event['flushOwner'])[phase]
        if (record['nativeOwner'] != owner or record['nativeHookEpoch'] != event['nativeHookEpoch'] or
                (record['spriteDepth'], record['flushDepth']) != ((1,0),(0,0),(0,1))[phase] or
                (phase == 2 and record['option'] != event['option'])):
            raise ValueError('HUD record owner/depth/epoch/option mismatch')
        flush_samples += repeat if phase == 2 else 0
        read_failed |= not hud_snapshot(record)
    if represented > samples or flush_samples > flush_draws:
        raise ValueError('HUD represented samples exceed queries/draws')
    if ((read_failed and not faults & 8) or (draw_failed and not faults & 16) or
            (draws > 64 and not faults & 32)):
        raise ValueError('HUD failed read/draw/truncation lacks fault')
    healthy = (not faults and closure and draws > 0 and flush_draws > 0 and
               represented == samples == draws and flush_samples == flush_draws and
               not read_failed and not draw_failed)
    if bool(complete) != bool(healthy):
        raise ValueError('Untruthful HUD complete claim')
    if not complete and not faults:
        raise ValueError('HUD incomplete closure/counts lack a fault')
    return [name for bit, name in enumerate(HUD_FAULTS) if faults & (1 << bit)]


def analyze_hud_batches(lines, start, end, canonical):
    result = hud_component()
    prior = None
    count = 0
    for index, line in enumerate(lines):
        marker = hud_marker(line)
        if marker is None:
            continue
        count += 1
        row = dict(line=index+1, raw=line, event=None, status='rejected', errors=[])
        result['rows'].append(row)
        try:
            if marker[1] != 'hud_batch_probe' or line[marker.end():marker.end()+2] != ': ':
                raise ValueError('Malformed HUD prefix or serialization failure')
            if len(line) > 256*1024:
                raise ValueError('HUD serialized row exceeds bounded size')
            event = json.loads(line[marker.end()+2:], object_pairs_hook=unique_object,
                               parse_constant=reject_constant, parse_float=hud_float)
            row['event'] = event
            if count > 6:
                raise ValueError('HUD six-pass session budget exceeded')
            reasons = validate_hud_pass(event)
            if not canonical:
                raise ValueError('Complete canonical binding interval unavailable')
            if not start < index < end:
                raise ValueError('HUD observation outside canonical run interval')
            if prior is not None and (event['pass'] <= prior['pass'] or
                    event['frame'] < prior['frame'] or event['present'] < prior['present'] or
                    event['nativeGeneration'] < prior['nativeGeneration'] or
                    event['continuityEpoch'] < prior['continuityEpoch']):
                raise ValueError('Duplicate/reversed HUD pass context')
            prior = event
            row['status'] = 'incomplete' if reasons else 'observed'
            row['errors'] = reasons
            if not reasons:
                result['passes'].append(dict(event, line=index+1))
        except (ValueError, TypeError, KeyError, AttributeError, OverflowError, RecursionError) as error:
            row['errors'].append(str(error))
        result['errors'].extend(f"Line {index+1}: {reason}" for reason in row['errors'])
    if not result['rows']:
        result['errors'].append('No HUD batch observations')
    elif all(row['status'] == 'observed' for row in result['rows']):
        result['status'] = 'observed'
    return result


def analyze(text):
    report = dict(schema=1, status='incomplete', readOnly=True, resizingAdmitted=False,
                  coverage='binding_state_only', headsetValidated=False,
                  correlationOnly=True, scopes=[], pairs=[], unresolved=UNRESOLVED[:], errors=[],
                  nativePostprocess=native_component())
    report['nativePostprocess']['errors'] = ['Complete canonical binding interval unavailable']
    start = end = None
    try:
        native_lines = text.splitlines()
        # Preserve line indices while isolating the optional namespace from the
        # established marker/binding/sprite parsers. Byte names can contain their
        # marker text; those strings are data, never additional telemetry.
        lines = ['' if NATIVE_LINE.match(line) or hud_marker(line) else line for line in native_lines]
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
        native_only = ['' if hud_marker(line) else line for line in native_lines]
        report['nativePostprocess'] = analyze_native_postprocess(native_only, start, end, report['scopes'])
    except (ValueError, TypeError, KeyError, AttributeError, OverflowError) as error:
        report['errors'].append(str(error))
    report['hudBatch'] = analyze_hud_batches(native_lines, start, end, report['status'] == 'observed')
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
    native_status = report['nativePostprocess']['status']
    print(report['status']+'; resizingAdmitted=false; headsetValidated=false; spriteOrdering='+sprite_status+
          '; nativePostprocess='+native_status+'; hudBatch='+report['hudBatch']['status'])
    return 0 if report['status'] == 'observed' else 2


if __name__ == '__main__':
    raise SystemExit(main())
