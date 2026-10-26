"""Read-only analysis of a collected CoJ run; never accepts a physical VR gate.

Use a finished evidence directory, not the installation's current mutable log.
Inventory hashes establish internal consistency, not external authentication.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path, PureWindowsPath
import re

EVENT = re.compile(r'camera_probe_event: event=(\w+) result=(\w+) detail=(.*)$')
# Retain malformed indexed results for rejection instead of silently filtering
# them out with the legacy event grammar. Other telemetry keeps that grammar.
INDEXED_EVENT = re.compile(r'camera_probe_event: event=(indexed_native_reload) result=(\S*) detail=(.*)$')
IDENTITY = re.compile(r'run_start: run_id=(\S+) build_manifest_id=(\S+)(?:\s|$)')
END = re.compile(r'run_end: run_id=(\S+)(?:\s|$)')
RENDER_FEEDBACK = re.compile(r'(manual_reload_depth|openvr_reload_haptic): (.*)$')
RELEVANT = {'reload_trace', 'manual_reload_session', 'manual_reload_insertion',
            'manual_reload_presentation', 'manual_reload_geometry', 'body_hand_render_probe',
            'controller_weapon_render_probe', 'body_hand_restore', 'controller_weapon_restore',
            'manual_reload_zone', 'manual_reload_cartridge', 'manual_reload_finger_reference',
            'rifle_support_reference', 'winchester_native_reload', 'indexed_native_reload',
            'physical_bow_draw', 'physical_bow_reset'}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def integer(fields, key):
    value = fields.get(key, '')
    return int(value) if re.fullmatch(r'-?\d+', value) else None


def yes(fields, key):
    return fields.get(key) in ('1', 'true')


def trace_reserve_kind(fields, phase):
    # Old inventoried traces observed pistol ammunition only. New traces bind
    # each copied reserve to a named exact-game pool; incomplete tags reject.
    if any('reserve_'+part+'_'+phase in fields for part in ('kind', 'count')):
        kind = fields.get('reserve_kind_'+phase)
        return kind if kind in ('pistol', 'rifle', 'shotgun') else None
    return 'pistol'


def trace_reserve(fields, phase):
    if any('reserve_'+part+'_'+phase in fields for part in ('kind', 'count')):
        return integer(fields, 'reserve_count_'+phase) if trace_reserve_kind(fields, phase) else None
    return integer(fields, 'pistol_reserve_'+phase)


def owner(fields):
    return tuple(integer(fields, k) for k in
                 ('player_generation', 'native_context', 'weapon_id', 'armed_hand'))


def coherent_trace(fields):
    identity = owner(fields)
    return (yes(fields, 'observation_valid') and yes(fields, 'context_allowed') and
            all(v is not None for v in identity) and all(v > 0 for v in identity[:3]) and
            identity[3] in (0, 1))


def deployment_key(value):
    path = PureWindowsPath(value)
    if not value or path.drive or path.root or '..' in path.parts or not path.parts:
        raise ValueError('Invalid relative deployment destination')
    return path.as_posix().casefold()


def checked_hash(value):
    if not isinstance(value, str) or not re.fullmatch(r'[a-fA-F0-9]{64}', value):
        raise ValueError('Missing or invalid SHA-256')
    return value.lower()


def verify_inventory(root):
    evidence = read_json(root / 'evidence-manifest.json')
    if evidence.get('schemaVersion') != 1 or evidence.get('manifestType') != 'cojvr-run-evidence':
        raise ValueError('Unsupported evidence manifest')
    if not re.fullmatch(r'[A-Za-z0-9._-]+', evidence.get('runId', '')) or not re.fullmatch(
            r'[a-fA-F0-9]{64}', evidence.get('buildManifestId', '')):
        raise ValueError('Invalid run/build identity')
    paths = {}
    for item in evidence.get('files', []):
        relative = Path(item['path'])
        path = (root / relative).resolve()
        if relative.is_absolute() or '..' in relative.parts or not path.is_relative_to(root):
            raise ValueError('Inventory path escapes evidence directory')
        if path in paths.values() or not path.is_file():
            raise ValueError('Duplicate or missing inventory file')
        if not re.fullmatch(r'[a-fA-F0-9]{64}', item.get('sha256', '')) or (
                path.stat().st_size != item.get('size') or digest(path) != item['sha256'].lower()):
            raise ValueError(f'Inventory hash/size mismatch: {relative}')
        paths[relative.as_posix()] = path
    if not {'run-manifest.json', 'runtime/cojvr.log'}.issubset(paths):
        raise ValueError('Run manifest and runtime log must be inventoried')
    run = read_json(paths['run-manifest.json'])
    if any(run.get(k) != evidence.get(k) for k in ('runId', 'buildManifestId')):
        raise ValueError('Run/evidence identity mismatch')
    build = (root / 'build-manifest.json').resolve()
    if not build.is_relative_to(root) or not build.is_file() or (
            digest(build) != checked_hash(run.get('buildManifestSha256'))):
        raise ValueError('Build manifest hash mismatch or missing file')
    if read_json(build).get('manifestId') != run['buildManifestId']:
        raise ValueError('Build manifest identity mismatch')
    expected_deployment = {}
    for artifact in run.get('deployment', []):
        key = deployment_key(artifact['destination'])
        if key in expected_deployment:
            raise ValueError('Duplicate run deployment destination')
        expected_deployment[key] = checked_hash(artifact.get('sha256'))
    if not expected_deployment:
        raise ValueError('Run deployment inventory is missing')
    checks, checked_destinations = evidence.get('deploymentChecks', []), set()
    for check in checks:
        key = deployment_key(check['destination'])
        if key in checked_destinations or key not in expected_deployment:
            raise ValueError('Duplicate or unrelated deployment check')
        if check.get('matches') is not True or (
                checked_hash(check.get('expectedSha256')) != expected_deployment[key] or
                checked_hash(check.get('actualSha256')) != expected_deployment[key]):
            raise ValueError('Collected deployment hash mismatch')
        checked_destinations.add(key)
    if checked_destinations != expected_deployment.keys():
        raise ValueError('Collected deployment checks do not cover the run')
    return evidence, run, paths


def read_events(log, evidence):
    events, starts, ends = [], [], []
    inside = False
    for number, line in enumerate(log.read_text(encoding='utf-8-sig').splitlines(), 1):
        start, end = IDENTITY.search(line), END.search(line)
        if start:
            if starts or ends or end or start.groups() != (evidence['runId'], evidence['buildManifestId']):
                raise ValueError('Foreign, reused or out-of-order runtime start')
            starts.append(start.groups())
            inside = True
        if end:
            if not inside or ends or end[1] != evidence['runId']:
                raise ValueError('Foreign, repeated or out-of-order runtime end')
            ends.append(end[1])
            inside = False
        match = INDEXED_EVENT.search(line) or EVENT.search(line)
        feedback = RENDER_FEEDBACK.search(line) if not match else None
        if inside and ((match and match[1] in RELEVANT) or feedback):
            # Error descriptions can contain semicolons/key-like text. Treat the
            # trailing opaque detail as data, never extra telemetry or instructions.
            fields, parse_issues = {}, []
            indexed = bool(match and match[1] == 'indexed_native_reload')
            for pair in (match[3] if match else feedback[2]).split(';'):
                if '=' not in pair:
                    if indexed:
                        parse_issues.append('malformed_field')
                    continue
                key, value = pair.split('=', 1)
                if key == 'detail':
                    if indexed:
                        parse_issues.append('unexpected_detail_field')
                    break
                if key in fields:
                    if indexed:
                        parse_issues.append('duplicate_field:'+key)
                        continue
                    raise ValueError(f'Duplicate telemetry field on line {number}')
                fields[key] = value
            entry = dict(line=number, event=match[1] if match else feedback[1],
                         result=match[2] if match else fields.get('status','observed'), fields=fields)
            if indexed:
                entry.update(raw_detail=match[3], parse_issues=parse_issues)
            events.append(entry)
    if starts != [(evidence['runId'], evidence['buildManifestId'])]:
        raise ValueError('Missing, foreign or reused runtime run/build identity')
    if ends not in ([], [evidence['runId']]):
        raise ValueError('Foreign or repeated runtime end')
    if evidence.get('runtimeStarted') is not True or evidence.get('runtimeEnded') is not bool(ends) or (
            evidence.get('incomplete') is not (not bool(ends))):
        raise ValueError('Runtime markers contradict collected completion state')
    return events


def geometry_metrics(fields):
    def vector(key):
        text = fields[key]
        # RuntimeVectorText emits (x,y,z); retain bare-vector fixture compatibility.
        if text.startswith('(') and text.endswith(')'):
            text = text[1:-1]
        result = tuple(float(x) for x in text.split(','))
        if len(result) != 3 or not all(math.isfinite(x) for x in result):
            raise ValueError('Invalid mechanical vector')
        return result
    def cosine(a, b):
        na, nb = math.hypot(*a), math.hypot(*b)
        if not math.isfinite(na) or not math.isfinite(nb) or min(na, nb) < 1e-12:
            raise ValueError('Degenerate mechanical axis')
        return sum((x/na)*(y/nb) for x, y in zip(a, b))
    drum, gate = vector('drum'), vector('gate')
    metrics = dict(gate_drum_distance_cm=math.dist(gate, drum),
                gate_drum_up_cosine=cosine(vector('gate_up'), vector('drum_up')),
                gate_drum_forward_cosine=cosine(vector('gate_forward'), vector('inward')))
    if not all(math.isfinite(x) for x in metrics.values()):
        raise ValueError('Non-finite mechanical measurement')
    if fields.get('source') in ('pre_native', 'post_overlay'):
        def rigid(label, forward_key=None):
            pos, up, forward = vector(label), vector(label+'_up'), vector(forward_key or label+'_forward')
            if abs(sum(x*x for x in up)-1)>0.002 or abs(sum(x*x for x in forward)-1)>0.002 or abs(sum(x*y for x,y in zip(up,forward)))>0.002:
                raise ValueError('Non-rigid reference frame')
            right = (up[1]*forward[2]-up[2]*forward[1],
                     up[2]*forward[0]-up[0]*forward[2],
                     up[0]*forward[1]-up[1]*forward[0])
            return pos, up, forward, right
        def project(point, origin, basis):
            delta = tuple(x-y for x,y in zip(point,origin))
            if not all(math.isfinite(x) for x in delta):
                raise ValueError('Non-finite relative frame')
            return tuple(sum(x*y for x,y in zip(delta,axis)) for axis in basis)
        # Cosine normalization can conceal invalid/scaled element axes.
        # Both native and mapped source-qualified measurements need rigid parts.
        part_frames = {'gate': rigid('gate'), 'drum': rigid('drum', 'inward')}
        for anchor in ('root','barrel'):
            origin,up,forward,right=rigid(anchor)
            for part in ('gate','drum'):
                relative=project(vector(part),origin,(right,up,forward))
                if not all(math.isfinite(x) for x in relative):
                    raise ValueError('Non-finite local position')
                for axis,value in zip('xyz',relative):
                    metrics[f'{part}_{anchor}_{axis}_cm']=value
                # Preserve the complete oriented part frame in local weapon
                # coordinates; a lone world-space cosine loses yaw/roll and sign.
                for direction, direction_vector in (('up', part_frames[part][1]),
                                                    ('forward', part_frames[part][2])):
                    local = tuple(sum(a*b for a,b in zip(direction_vector,basis))
                                  for basis in (right,up,forward))
                    for axis,value in zip('xyz',local):
                        metrics[f'{part}_{anchor}_{direction}_local_{axis}']=value
                metrics[f'{part}_{anchor}_up_cosine']=cosine(vector(part+'_up'),up)
                metrics[f'{part}_{anchor}_forward_cosine']=cosine(vector('inward' if part=='drum' else 'gate_forward'),forward)
        if not all(math.isfinite(x) for x in metrics.values()):
            raise ValueError('Non-finite local metrics')
    return metrics


def finger_joint_metrics(fields):
    """Compare distal-joint frames in native hand-local cm, not skin contacts.

    The logged frames come from the same verified overlay and are already in
    each hand's local basis. Cartridge position/axis are XR-space, so mixing
    them with these centimetre-space frames would invent a contact measurement.
    """
    if (integer(fields, 'frame_sequence') is None or integer(fields, 'frame_sequence') <= 0 or
            integer(fields, 'hand') not in (0, 1) or
            integer(fields, 'armed_hand') != 1-integer(fields, 'hand') or
            any(integer(fields, key) in (None, 0, -1)
                for key in ('player_generation', 'native_context', 'weapon_id')) or
            not yes(fields, 'owner_snapshot_valid') or
            fields.get('units') != 'cm' or fields.get('basis') != 'each_hand_local' or
            fields.get('sampling') != 'pre_restore_verified' or
            fields.get('mutation') != 'false' or fields.get('finger_pad_contact') != 'false'):
        raise ValueError('Unqualified finger-frame evidence')

    def vec(key):
        raw = fields[key]
        if raw.startswith('(') and raw.endswith(')'):
            raw = raw[1:-1]
        values = tuple(float(v) for v in raw.split(','))
        if len(values) != 3 or not all(math.isfinite(v) for v in values):
            raise ValueError('Non-finite finger-frame component')
        return values

    def dot(a, b):
        return sum(x*y for x, y in zip(a, b))

    def frame(prefix):
        pos, up, forward = (vec(prefix+'_'+key) for key in ('position', 'up', 'forward'))
        if (abs(dot(up, up)-1) > .002 or abs(dot(forward, forward)-1) > .002 or
                abs(dot(up, forward)) > .002):
            raise ValueError('Non-rigid finger frame')
        right = (up[1]*forward[2]-up[2]*forward[1],
                 up[2]*forward[0]-up[0]*forward[2],
                 up[0]*forward[1]-up[1]*forward[0])
        return pos, (right, up, forward)

    frames = {(digit, phase): frame(f'{digit}_{phase}')
              for digit in ('thumb', 'index') for phase in ('native', 'displayed')}
    gaps = {phase: math.dist(frames['thumb', phase][0], frames['index', phase][0])
            for phase in ('native', 'displayed')}
    metrics = dict(native_joint_gap_cm=gaps['native'],
                   displayed_joint_gap_cm=gaps['displayed'],
                   joint_gap_change_cm=gaps['displayed']-gaps['native'])
    for digit in ('thumb', 'index'):
        native, displayed = frames[digit, 'native'], frames[digit, 'displayed']
        metrics[digit+'_joint_translation_cm'] = math.dist(native[0], displayed[0])
        cosine = (sum(dot(a, b) for a, b in zip(native[1], displayed[1]))-1)/2
        metrics[digit+'_rotation_from_native_deg'] = math.degrees(
            math.acos(max(-1., min(1., cosine))))
    # Optional in new traces; earlier events contained only distal-joint origins.
    # This measured gap is between derived skin-pad points, not contact proof.
    if 'skin_pad_gap_cm' in fields:
        skin_gap = float(fields['skin_pad_gap_cm'])
        if not math.isfinite(skin_gap) or not .6 <= skin_gap <= 1.8:
            raise ValueError('Unqualified displayed skin-pad gap')
        metrics['displayed_skin_pad_gap_cm'] = skin_gap
    if 'case_span_cm' in fields:
        if 'skin_pad_gap_cm' not in fields:
            raise ValueError('Case span without same-snapshot skin gap')
        span = float(fields['case_span_cm'])
        if not math.isfinite(span) or not .6 <= span <= 1.8:
            raise ValueError('Unqualified displayed case envelope')
        metrics['displayed_case_transverse_span_cm'] = span
        # Signed nominal margin only: a mesh envelope over all axial stations
        # does not prove actual finger-surface contact or scene occlusion.
        metrics['pad_minus_case_span_cm'] = metrics['displayed_skin_pad_gap_cm']-span
    proximity_keys=('thumb_mesh_surface_distance_cm','index_mesh_surface_distance_cm')
    if any(key in fields for key in proximity_keys):
        if ('skin_pad_gap_cm' not in fields or
                not all(key in fields for key in proximity_keys)):
            raise ValueError('Incomplete same-snapshot triangle surface proximity')
        for key in proximity_keys:
            distance = float(fields[key])
            if not math.isfinite(distance) or distance < 0 or distance > 5:
                raise ValueError('Invalid triangle surface proximity')
            metrics[key] = distance
        metrics['mesh_surface_distance_asymmetry_cm'] = abs(
            metrics[proximity_keys[0]]-metrics[proximity_keys[1]])
    station_keys=('thumb_station_from_base_cm','index_station_from_base_cm')
    if any(key in fields for key in station_keys):
        if not all(key in fields for key in (*station_keys,*proximity_keys)):
            raise ValueError('Incomplete same-snapshot mesh axial station')
        for key in station_keys:
            station = float(fields[key])
            # Signed distance to the imported rear base; retain out-of-envelope
            # readings instead of clipping them to the cartridge visual.
            if not math.isfinite(station) or abs(station) > 15:
                raise ValueError('Invalid mesh axial station')
            metrics[key] = station
        metrics['skin_pad_axial_separation_cm'] = abs(
            metrics[station_keys[0]]-metrics[station_keys[1]])
    if not all(math.isfinite(value) for value in metrics.values()):
        raise ValueError('Non-finite finger-frame measurement')
    return metrics


def finger_reference_observations(events):
    """Report sparse on-pickup joint geometry without claiming visual contact."""
    rows, issues, seen = [], [], set()
    for index, sample in enumerate(events):
        if sample['event'] != 'manual_reload_finger_reference':
            continue
        fields = sample['fields']
        frame, hand = integer(fields, 'frame_sequence'), integer(fields, 'hand')
        row = dict(line=sample['line'], frame=frame, hand=hand,
                   status=sample['result'], skin_pad_contact_measured=False,
                   trace_correlation='unverified', cartridge_correlated=False)
        if sample['result'] == 'unavailable':
            rows.append(row)
            continue
        if sample['result'] != 'observed' or (frame, hand) in seen:
            issues.append(dict(line=sample['line'], kind='invalid_finger_reference'))
            row['status'] = 'invalid'
            rows.append(row)
            continue
        seen.add((frame, hand))
        try:
            row.update(finger_joint_metrics(fields))
        except (KeyError, ValueError, OverflowError):
            issues.append(dict(line=sample['line'], kind='invalid_finger_reference'))
            row['status'] = 'invalid'
            rows.append(row)
            continue
        trace = next((e for e in reversed(events[:index])
                      if e['event'] == 'reload_trace'), None)
        if trace is None or integer(trace['fields'], 'frame_sequence') != frame:
            row['trace_correlation'] = 'missing_same_frame_trace'
        elif owner(trace['fields']) != owner(fields):
            row['trace_correlation'] = 'owner_mismatch'
        elif not (coherent_trace(trace['fields']) and
                  yes(trace['fields'], 'manual_session_enabled') and
                  integer(trace['fields'], 'wait_probe_status_after') in (1, 2, 3)):
            row['trace_correlation'] = 'native_state_unverified'
        else:
            row['trace_correlation'] = 'same_frame_owner'
        cartridge = next((e for e in reversed(events[:index])
                          if e['event'] == 'manual_reload_cartridge'), None)
        row['cartridge_correlated'] = bool(
            row['trace_correlation'] == 'same_frame_owner' and cartridge and
            cartridge['result'] == 'visible' and
            integer(cartridge['fields'], 'frame_sequence') == frame and
            integer(cartridge['fields'], 'hand') == hand and
            yes(cartridge['fields'], 'anchor_valid') and
            yes(cartridge['fields'], 'physical_claim'))
        rows.append(row)
    return rows, issues


def native_drum_phase(fields):
    """Optional stable native scalar, distinct from displayed geometry/chambers."""
    keys=('native_drum_phase_deg','native_phase_stable')
    if not any(key in fields for key in keys):
        return None
    if not all(key in fields for key in keys) or fields['native_phase_stable']!='1':
        raise ValueError('Unqualified native drum phase')
    value=float(fields['native_drum_phase_deg'])
    if not math.isfinite(value):
        raise ValueError('Non-finite native drum phase')
    return value


def port_pivot_probe(fields):
    """Interpret optional native pivot-distance telemetry without granting socket clearance.

    A ranked chamber is simply nearest to the *gate element origin*, which is
    not verified to be the visible loading-port centre.
    """
    keys = ('port_reference', 'port_owner_qualified', 'port_rank', 'port_nearest_mouth',
            'port_nearest_cm', 'port_runner_up_cm', 'port_separation_cm',
            'port_axial_cm', 'port_radial_cm', 'gate_open_confirmed', 'socket_admission')
    if not any(k.startswith('port_') or k == 'socket_admission' for k in fields):
        return None  # Earlier runs did not emit a port diagnostic.
    if (not all(k in fields for k in keys) or fields.get('source') != 'post_overlay' or
            not yes(fields, 'manual_session') or
            fields.get('model') not in ('peacemaker', 'frontier') or
            fields['port_reference'] != 'gate_element_origin_unverified' or
            fields['gate_open_confirmed'] != 'false' or
            fields['socket_admission'] != 'false' or
            fields['port_owner_qualified'] not in ('0', '1') or
            fields['port_rank'] not in ('pivot_only', 'ambiguous', 'invalid')):
        raise ValueError('Invalid or overclaimed pivot telemetry')
    index = integer(fields, 'port_nearest_mouth')
    if index is None:
        raise ValueError('Invalid pivot nearest index')
    try:
        nearest, runner, separation, axial, radial = (
            float(fields[k]) for k in ('port_nearest_cm', 'port_runner_up_cm',
                                       'port_separation_cm', 'port_axial_cm',
                                       'port_radial_cm'))
    except ValueError as error:
        raise ValueError('Invalid pivot distance') from error
    if (not all(math.isfinite(v) for v in (nearest, runner, separation, axial, radial)) or
            min(nearest, runner, separation, radial) < 0 or
            runner + .00001 < nearest or
            not math.isclose(runner-nearest, separation, abs_tol=.001) or
            not math.isclose(math.hypot(axial, radial), nearest, abs_tol=.001)):
        raise ValueError('Incoherent pivot distance')
    status = fields['port_rank']
    qualified = fields['port_owner_qualified'] == '1'
    if status == 'pivot_only':
        if not qualified or index not in range(6) or separation <= .005:
            raise ValueError('Invalid unique pivot ranking')
    elif status == 'ambiguous':
        if not qualified or index != -1 or separation > .005:
            raise ValueError('Invalid ambiguous pivot ranking')
    elif index != -1 or any(v != 0 for v in (nearest, runner, separation, axial, radial)):
        raise ValueError('Invalid failed pivot ranking')
    return dict(reference=fields['port_reference'], owner_qualified=qualified,
                status=status, nearest_mouth=index, nearest_cm=nearest,
                runner_up_cm=runner, separation_cm=separation, axial_cm=axial,
                radial_cm=radial, gate_open_confirmed=False, socket_admission=False)


def relative_orientation_angle_degrees(reference, current, part, anchor):
    """Angle between two part frames expressed in their own weapon anchor.

    Normalize small sampler rounding errors before taking the SO(3) trace.
    This is a pose difference, not a hinge axis or physical-port clearance test.
    """
    def frame(metrics):
        def read(direction):
            return tuple(metrics[f'{part}_{anchor}_{direction}_local_{axis}']
                         for axis in 'xyz')
        def dot(a, b):
            return sum(x*y for x,y in zip(a,b))
        def normalize(v):
            size=math.hypot(*v)
            if not math.isfinite(size) or size < 1e-12:
                raise ValueError('Invalid local orientation')
            return tuple(x/size for x in v)
        up=normalize(read('up'))
        forward=read('forward')
        forward=normalize(tuple(x-dot(forward,up)*y for x,y in zip(forward,up)))
        right=(up[1]*forward[2]-up[2]*forward[1],
               up[2]*forward[0]-up[0]*forward[2],
               up[0]*forward[1]-up[1]*forward[0])
        return right,up,forward
    before,after=frame(reference),frame(current)
    cosine=(sum(sum(a*b for a,b in zip(axis0,axis1))
                for axis0,axis1 in zip(before,after))-1.0)/2.0
    return math.degrees(math.acos(max(-1.0,min(1.0,cosine))))


def depth_sample_metadata_valid(fields):
    """Check trace integrity only; even valid metadata cannot establish depth content."""
    if (fields.get('eye') not in ('left', 'right') or
            integer(fields, 'frame_sequence') is None or integer(fields, 'frame_sequence') <= 0 or
            integer(fields, 'generation') is None or integer(fields, 'generation') <= 0 or
            fields.get('phase') != 'full_eye_complete' or
            fields.get('read_only') != 'true' or
            fields.get('depth_content') != 'unverified' or
            fields.get('occlusion_admission') != 'false' or
            any(fields.get(key) != '1' for key in
                ('target_valid', 'depth_valid', 'state_valid', 'layout_matches', 'transforms_valid'))):
        return False
    if not re.fullmatch(r'0x[0-9a-fA-F]+', fields.get('device', '')) or int(fields['device'], 16) == 0:
        return False
    for key in ('target', 'depth'):
        if not re.fullmatch(r'0x[0-9a-fA-F]+', fields.get(key, '')) or int(fields[key], 16) == 0:
            return False
    dimensions = [integer(fields, key) for key in
                  ('target_width', 'target_height', 'depth_width', 'depth_height')]
    msaa = [integer(fields, key) for key in
            ('target_msaa', 'depth_msaa', 'target_quality', 'depth_quality')]
    states = [integer(fields, key) for key in ('z_enable', 'z_write', 'z_func')]
    if (any(value is None or value <= 0 for value in dimensions) or
            any(value is None or value < 0 for value in msaa) or
            any(value is None or value < 0 for value in states) or
            dimensions[2] < dimensions[0] or dimensions[3] < dimensions[1] or
            msaa[0] != msaa[1] or msaa[2] != msaa[3]):
        return False
    try:
        viewport = [int(v) for v in fields.get('viewport', '').split(',')]
        minimum, maximum = float(fields['min_z']), float(fields['max_z'])
        matrices = [[float(v) for v in fields.get(key, '').split(',')]
                    for key in ('view', 'projection')]
    except (ValueError, KeyError):
        return False
    return (len(viewport) == 4 and viewport[0] >= 0 and viewport[1] >= 0 and
            viewport[2] > 0 and viewport[3] > 0 and
            viewport[0] + viewport[2] <= dimensions[0] and
            viewport[1] + viewport[3] <= dimensions[1] and
            0 <= minimum <= maximum <= 1 and
            all(len(matrix) == 16 and all(math.isfinite(v) for v in matrix)
                for matrix in matrices))


def correlate_depth_eye_pairs(samples):
    """Pair ordered eye diagnostics within one frame, never across a reset/device."""
    groups = {}
    for sample in samples:
        frame = integer(sample['fields'], 'frame_sequence')
        # Unframed records are isolated rather than being accidentally paired.
        key = frame if frame is not None and frame > 0 else ('unframed', sample['line'])
        groups.setdefault(key, []).append(sample)
    pairs = []
    for group in groups.values():
        eyes = [sample['fields'].get('eye') for sample in group]
        valid = all(depth_sample_metadata_valid(sample['fields']) for sample in group)
        ordered = len(group) == 2 and eyes == ['left', 'right']
        same_owner = ordered and (group[0]['fields']['generation'], group[0]['fields']['device']) == (
            group[1]['fields']['generation'], group[1]['fields']['device'])
        if len(group) == 1 and valid:
            status, reason = 'incomplete', 'missing_eye'
        elif not valid:
            status, reason = 'rejected', 'invalid_sample_metadata'
        elif not ordered:
            status, reason = 'rejected', 'missing_order_or_duplicate_eye'
        elif not same_owner:
            status, reason = 'rejected', 'device_or_generation_mismatch'
        else:
            status, reason = 'metadata_correlated', 'ordered_same_frame_device_generation'
        pairs.append(dict(frame=integer(group[0]['fields'], 'frame_sequence'),
                          lines=[s['line'] for s in group], eyes=eyes,
                          status=status, reason=reason,
                          layout_both=ordered and valid,
                          content_correspondence='unverified', occlusion_admitted=False))
    return pairs


def rifle_support_observations(events):
    """Compare sparse same-owner natural support frames, never grant attachment."""
    rows, owners = [], {}
    for event in events:
        if event['event'] != 'rifle_support_reference':
            continue
        f = event['fields']
        fixed = dict(schema='1', source='natural_pre_body', units='cm', read_only='true',
                     contact_admission='false', attachment_admission='false', cycle_qualified='false',
                     states_left='0,0,0,0', two_handed_right='true,true,true',
                     two_handed_left='false,false,false')
        identity = tuple(integer(f,k) for k in
                         ('player_generation','native_context','player_id','weapon_id'))
        stamp, gap, index, frame = (integer(f,k) for k in
                                   ('sample_us','sample_gap_us','sample_index','frame_sequence'))
        if (event['result'] != 'observed' or any(f.get(k) != v for k,v in fixed.items()) or
                f.get('weapon_class') not in ('WeaponRifleWinchester','WeaponRifleRemington',
                                             'WeaponRifleRemingtonSawOff','WeaponRifleWinchesterScope') or
                any(v is None or v <= 0 for v in identity) or
                any(v is None or v <= 0 for v in (stamp,index,frame)) or
                gap is None or gap < 0 or index > 240 or
                any(v >= 0xffffffff for v in identity[2:])):
            raise ValueError('Invalid or overclaimed rifle support sample')
        right = tuple(int(v) for v in f['states_right'].split(','))
        scoped = f['weapon_class'] == 'WeaponRifleWinchesterScope'
        idle = (1,129) if scoped else (1,)
        firing = any(v not in idle for v in right)
        if (len(right) != 4 or any(v not in idle and not 30 <= v <= 38 and
                not (scoped and 158 <= v <= 166) for v in right) or
                f.get('phase') != ('ordinary_fire_tuple' if firing else 'ordinary_idle_tuple')):
            raise ValueError('Unqualified rifle support phase')
        def vector(key):
            value=f[key]
            if value.startswith('(') and value.endswith(')'):
                value=value[1:-1]
            result=tuple(float(v) for v in value.split(','))
            if len(result)!=3 or not all(math.isfinite(v) for v in result):
                raise ValueError('Invalid rifle support vector')
            return result
        for name in ('root','holder','support','support_in_weapon'):
            position,up,forward=vector(name),vector(name+'_up'),vector(name+'_forward')
            if (abs(sum(v*v for v in up)-1)>.002 or
                    abs(sum(v*v for v in forward)-1)>.002 or
                    abs(sum(a*b for a,b in zip(up,forward)))>.002):
                raise ValueError('Non-rigid rifle support frame')
        previous=owners.get(identity)
        observed_gap=None
        displacement=None
        if previous and (stamp<=previous['stamp'] or frame<=previous['frame'] or
                         f['weapon_class']!=previous['weapon_class']):
            raise ValueError('Non-monotonic rifle support sample')
        if index==1:
            if gap!=0:
                raise ValueError('Initial rifle sample has a stale gap')
            baseline=position
        else:
            if not previous or index!=previous['index']+1 or gap!=stamp-previous['stamp']:
                raise ValueError('Missing or inconsistent rifle sample history')
            observed_gap=gap
            baseline=previous['baseline']
            displacement=math.dist(position,baseline)
            if not math.isfinite(displacement):
                raise ValueError('Non-finite rifle support displacement')
        owners[identity]=dict(stamp=stamp,frame=frame,index=index,baseline=baseline,
                              weapon_class=f['weapon_class'])
        rows.append(dict(line=event['line'],owner=identity,weapon_class=f['weapon_class'],
                         sample_us=stamp,observed_gap_us=observed_gap,phase=f['phase'],
                         support_in_weapon_cm=position,
                         support_displacement_from_first_cm=displacement,
                         attachment_admitted=False,cycle_qualified=False))
    return rows


def winchester_reload_observations(events):
    """Validate sparse native Winchester witnesses without inferring a loading port.

    Exact WeaponFireAmmo.GetAmmoCount(0) reads m_iAmmo0Count. Sparse reserve
    and count deltas alone do not qualify a transfer or physical chamber state.
    """
    rows, previous, baseline, last_observed = [], None, None, None

    def triple_or_quad(f, key, size, boolean=False):
        parts = f[key].split(',')
        if len(parts) != size:
            raise ValueError('Invalid Winchester hand tuple')
        if boolean and any(part not in ('0', '1') for part in parts):
            raise ValueError('Invalid Winchester hand flag')
        values = tuple(int(part) for part in parts)
        if not boolean and any(not 0 <= value <= 1024 for value in values):
            raise ValueError('Invalid Winchester native state')
        return values

    def vector(f, name):
        value = f[name]
        if value.startswith('(') and value.endswith(')'):
            value = value[1:-1]
        values = tuple(float(v) for v in value.split(','))
        if len(values) != 3 or not all(math.isfinite(v) for v in values):
            raise ValueError('Invalid Winchester pose vector')
        return values

    for e in events:
        if e['event'] != 'winchester_native_reload':
            continue
        f = e['fields']
        frame = integer(f, 'frame_sequence')
        if frame is None or frame <= 0 or f.get('manual_admission') != 'false':
            raise ValueError('Invalid Winchester event authority')
        if e['result'] == 'unavailable':
            if f.get('owner_or_phase_unavailable') != 'true' or any(
                    integer(f, key) is None or integer(f, key) <= 0
                    for key in ('player_generation', 'native_context')):
                raise ValueError('Invalid Winchester unavailable event')
            rows.append(dict(line=e['line'], frame=frame, status='unavailable',
                             manual_admitted=False, native_phase_qualified=False))
            previous, baseline = None, None
            continue
        if e['result'] == 'window_end':
            reason = f.get('reason')
            # Older schema-1 window ends did not label the rifle class: they
            # could only have been produced by the ordinary Winchester probe.
            weapon_class = f.get('weapon_class', 'WeaponRifleWinchester')
            identity = tuple(integer(f, key) for key in (
                'player_generation', 'native_context', 'weapon_id'))
            stamp, last_stamp, index = (integer(f, key) for key in (
                'sample_us', 'last_sample_us', 'sample_index'))
            if (f.get('schema') != '1' or reason not in (
                    'expired', 'exhausted', 'idle_limit', 'retriggered',
                    'owner_changed', 'native_reload_started', 'context_changed',
                    'clock_reset', 'disabled') or not previous or
                    weapon_class != previous['weapon_class'] or
                    identity != (previous['identity'][0], previous['identity'][1],
                                 previous['identity'][3]) or
                    index != previous['index'] or last_stamp != previous['stamp'] or
                    stamp is None or stamp < last_stamp or frame < previous['frame'] or
                    (reason == 'exhausted' and index != 220) or
                    (reason == 'idle_limit' and index != 4)):
                raise ValueError('Invalid Winchester window end or sample history')
            rows.append(dict(line=e['line'], frame=frame, status='window_end',
                             weapon_class=weapon_class, reason=reason,
                             sample_index=index, sample_us=stamp,
                             last_sample_us=last_stamp, owner=identity,
                             manual_admitted=False, native_phase_qualified=False,
                             ammo_clip_qualified=False))
            previous, baseline = None, None
            continue
        if e['result'] != 'observed' or f.get('weapon_class') not in (
                'WeaponRifleWinchester', 'WeaponRifleWinchesterScope') or any(
                f.get(key) != value for key, value in dict(
                schema='1',
                source='native_read_only', units='cm', phase='unclassified').items()):
            raise ValueError('Invalid Winchester observation schema')
        identity = tuple(integer(f, key) for key in (
            'player_generation', 'native_context', 'player_id', 'weapon_id'))
        stamp, gap, index = (integer(f, key) for key in
                             ('sample_us', 'sample_gap_us', 'sample_index'))
        if (any(v is None or v <= 0 for v in identity) or
                any(v is None or v <= 0 for v in (stamp, index)) or
                gap is None or gap < 0 or index > 220 or
                any(v >= 0xffffffff for v in identity[2:])):
            raise ValueError('Invalid Winchester owner or sample clock')
        right, left = (triple_or_quad(f, 'states_'+hand, 4)
                       for hand in ('right', 'left'))
        weapons = tuple(triple_or_quad(f, 'weapons_'+hand, 3, True)
                        for hand in ('right', 'left'))
        flags = tuple(triple_or_quad(f, 'two_handed_'+hand, 3, True)
                      for hand in ('right', 'left'))
        if not any(weapons[0]):
            raise ValueError('Winchester right-hand owner absent')
        native_flags = {}
        for key in ('native_reloading', 'carrying', 'dead', 'pose_valid'):
            if f.get(key) not in ('0', '1'):
                raise ValueError('Invalid Winchester native flag')
            native_flags[key] = yes(f, key)
        animation = []
        for hand in ('right', 'left'):
            anim_id = integer(f, 'anim_'+hand)
            time = float(f['anim_time_'+hand])
            duration = float(f['anim_duration_'+hand])
            if anim_id is None or not all(map(math.isfinite, (time, duration))):
                raise ValueError('Invalid Winchester native animation clock')
            animation.append(dict(id=anim_id, time=time, duration=duration))
        ammo, reserve, ammo_delta, reserve_delta = (integer(f, key) for key in (
            'ammo_readback', 'rifle_reserve', 'ammo_readback_delta', 'reserve_delta'))
        if ammo is None or not 0 <= ammo <= 12 or reserve is None or reserve < 0 or (
                ammo_delta is None or reserve_delta is None):
            raise ValueError('Invalid Winchester ammunition observation')
        if last_observed and identity == last_observed['identity'] and (
                stamp <= last_observed['stamp'] or frame <= last_observed['frame']):
            raise ValueError('Non-monotonic Winchester observation')
        if previous and f['weapon_class'] != previous['weapon_class']:
            raise ValueError('Winchester class changed without closing its history')
        if index == 1:
            if gap or ammo_delta or reserve_delta:
                raise ValueError('Winchester initial sample retains stale deltas')
            baseline = None
        elif (not previous or identity != previous['identity'] or
              index != previous['index']+1 or gap != stamp-previous['stamp'] or
              ammo_delta != ammo-previous['ammo'] or
              reserve_delta != reserve-previous['reserve']):
            raise ValueError('Missing or inconsistent Winchester sample history')
        position, displacement = None, None
        if native_flags['pose_valid']:
            if any(integer(f, key) is None or integer(f, key) < 0 for key in
                   ('holder_element', 'support_element')):
                raise ValueError('Invalid Winchester authored element')
            for name in ('root', 'holder', 'support', 'support_in_weapon'):
                vector(f, name)
                up, forward = vector(f, name+'_up'), vector(f, name+'_forward')
                if (abs(sum(v*v for v in up)-1) > .002 or
                        abs(sum(v*v for v in forward)-1) > .002 or
                        abs(sum(a*b for a, b in zip(up, forward))) > .002):
                    raise ValueError('Non-rigid Winchester frame')
            position = vector(f, 'support_in_weapon')
            if baseline is None:
                baseline = position
            displacement = math.dist(position, baseline)
            if not math.isfinite(displacement):
                raise ValueError('Non-finite Winchester support displacement')
        elif any(k in f for name in ('root', 'holder', 'support', 'support_in_weapon')
                 for k in (name, name+'_up', name+'_forward')) or any(
                     k in f for k in ('holder_element', 'support_element')):
            raise ValueError('Unqualified Winchester frame published')
        previous = dict(identity=identity, stamp=stamp, frame=frame, index=index,
                        ammo=ammo, reserve=reserve, weapon_class=f['weapon_class'])
        last_observed = previous
        rows.append(dict(line=e['line'], frame=frame, status='observed',
                         owner=identity, weapon_class=f['weapon_class'],
                         sample_index=index, sample_us=stamp,
                         observed_gap_us=gap if index > 1 else None,
                         states_right=right, states_left=left,
                         weapon_slots=weapons, two_handed=flags, animation=animation,
                         native_reloading=native_flags['native_reloading'],
                         carrying=native_flags['carrying'], dead=native_flags['dead'],
                         ammo_readback=ammo, rifle_reserve=reserve,
                         ammo_readback_delta=ammo_delta, reserve_delta=reserve_delta,
                         pose_valid=native_flags['pose_valid'],
                         support_in_weapon_cm=position,
                         support_displacement_from_first_cm=displacement,
                         ammo_clip_qualified=False, native_phase_qualified=False,
                         manual_admitted=False))
    return rows


def indexed_reload_observations(events, runtime_complete):
    """Copy schema-1 passive samples, never classify their ammunition changes.

    Profiles mirror indexed_reload_delta.hpp and COJ_RELOAD_OWNERSHIP.md.
    Only numeric owner continuity is used to reject repeated/reversed clocks.
    No cross-poll object lease/epoch is emitted, so equality cannot prove the
    same Java object, a transfer, callback completion or native index choice.
    """
    profiles = {
        'WeaponRifleRemington': ([1, 1], ['shotgun', 'shotgun']),
        'WeaponRifleRemingtonSawOff': ([1, 1], ['shotgun', 'shotgun']),
        'WeaponPistolDerringer': ([1, 1], ['pistol', 'pistol']),
        'WeaponPistolLemant': ([1, 8], ['rifle', 'pistol']),
    }
    claims = dict(schema='1', synchronous_scope='0', transfer_proven='false',
                  manual_admission='false')
    owner_keys = ('player_generation', 'native_context', 'player_id', 'weapon_id', 'armed_hand')
    numeric_ranges = dict.fromkeys(('player_generation', 'native_context', 'frame_sequence',
                                   'sample_ms'), (1, 2**64-1))
    numeric_ranges.update(player_id=(1, 0xfffffffe), weapon_id=(1, 0xfffffffe), armed_hand=(0, 1))
    numeric_ranges.update({'reserve_'+pool: (0, 2**31-1) for pool in ('pistol', 'rifle', 'shotgun')})
    required = set(claims) | set(numeric_ranges) | {'weapon_class', 'loaded'}
    rows, issues, previous_owner, previous_clock = [], [], None, None
    for e in events:
        if e['event'] != 'indexed_native_reload':
            continue
        f = e['fields']
        problems = list(e['parse_issues'])
        if not runtime_complete:
            problems.append('runtime_not_finished')
        if e['result'] != 'observed':
            problems.append('invalid_result')
        problems.extend('missing_field:'+key for key in sorted(required-f.keys()))
        problems.extend('unexpected_field:'+key for key in sorted(f.keys()-required))
        problems.extend('invalid_claim:'+key for key, value in claims.items() if f.get(key) != value)
        numbers = {}
        for key, (low, high) in numeric_ranges.items():
            # ASCII decimal only; length also bounds conversion of forged payloads.
            raw = f.get(key, '')
            value = int(raw) if re.fullmatch(r'[0-9]{1,20}', raw) else None
            numbers[key] = value if value is not None and low <= value <= high else None
            if numbers[key] is None:
                problems.append('invalid_number:'+key)
        cls = f.get('weapon_class')
        profile = profiles.get(cls)
        loaded = None
        if not profile:
            problems.append('unsupported_weapon_class')
        else:
            raw = f.get('loaded', '')
            if re.fullmatch(r'[0-9]{1,10},[0-9]{1,10}', raw):
                loaded = [int(part) for part in raw.split(',')]
            if loaded is None or any(v > cap for v, cap in zip(loaded, profile[0])):
                problems.append('invalid_loaded_counts')
            for pool in ('pistol', 'rifle', 'shotgun'):
                if pool not in profile[1] and numbers['reserve_'+pool] != 0:
                    problems.append('irrelevant_reserve_nonzero:'+pool)

        numeric_owner = tuple(numbers[key] for key in owner_keys) + (cls,)
        if not profile or any(numbers[key] is None for key in owner_keys):
            numeric_owner = None
        if numeric_owner is None or numeric_owner != previous_owner:
            previous_clock = None
        previous_owner = numeric_owner
        frame, stamp = numbers['frame_sequence'], numbers['sample_ms']
        if previous_clock and frame is not None and stamp is not None and (
                frame <= previous_clock[0] or stamp <= previous_clock[1]):
            problems.append('non_monotonic_numeric_owner_sample')
        # Keep numeric clock high-water marks even for a rejected payload. A
        # forged claim or bad count must not hide a later duplicate/regression.
        # This is only a rejection guard, never cross-poll object confidence.
        if numeric_owner is not None and frame is not None and stamp is not None:
            previous_clock = (max(frame, previous_clock[0]), max(stamp, previous_clock[1])) if (
                previous_clock) else (frame, stamp)
        row = dict(line=e['line'], status='rejected' if problems else 'observed',
                   reported_result=e['result'], raw_detail=e['raw_detail'], raw_fields=dict(f), issues=problems,
                   object_epoch=0, synchronous_scope=0, same_object_proven=False,
                   transfer_proven=False, callback_completion_proven=False,
                   native_index_choice_proven=False, manual_admitted=False)
        if problems:
            issues.append(dict(line=e['line'], kind='invalid_indexed_reload_sample', issues=problems))
        else:
            row.update({key: numbers[key] for key in owner_keys})
            row.update(frame=frame, sample_ms=stamp, weapon_class=cls, loaded=loaded,
                       capacity_by_index=profile[0], reserve_pool_by_index=profile[1],
                       reserves={pool: numbers['reserve_'+pool] for pool in ('pistol', 'rifle', 'shotgun')})
        rows.append(row)
    return rows, issues


def bow_draw_observations(events):
    """Summarize reported bow input/cancellation; never infer an arrow spawn."""
    samples, requests, counter_max = 0, 0, None
    resets, gesture_cancels, dispatch_cancels = Counter(), Counter(), Counter()
    for event in events:
        if event['event'] not in ('physical_bow_draw', 'physical_bow_reset'):
            continue
        f = event['fields']
        frame = integer(f, 'frame_sequence')
        reason = f.get('reason')
        if frame is None or frame <= 0 or not reason:
            raise ValueError('Missing bow frame or diagnostic reason')
        if event['event'] == 'physical_bow_reset':
            if event['result'] != 'cancelled':
                raise ValueError('Invalid bow reset result')
            resets[reason] += 1
            continue
        if event['result'] not in ('idle', 'drawing', 'shot_requested', 'unavailable'):
            raise ValueError('Invalid bow draw result')
        for key in ('gesture_cancelled', 'dispatch_cancelled', 'counter_valid'):
            if key in f and f[key] not in ('0', '1', 'false', 'true'):
                raise ValueError('Invalid bow diagnostic flag')
        if 'arrow_spawn_confirmed' in f and f['arrow_spawn_confirmed'] != 'false':
            raise ValueError('Bow input telemetry cannot confirm an arrow spawn')
        if yes(f, 'counter_valid'):
            counter = integer(f, 'native_attack_entries')
            if counter is None or counter < 0:
                raise ValueError('Invalid bow native attack counter')
            counter_max = counter if counter_max is None else max(counter_max, counter)
        samples += 1
        requests += event['result'] == 'shot_requested'
        if yes(f, 'gesture_cancelled'):
            gesture_cancels[reason] += 1
        if yes(f, 'dispatch_cancelled'):
            dispatch_cancels[reason] += 1
    return dict(observed=bool(samples or resets), draw_samples=samples,
                shot_requests=requests, native_attack_counter_max=counter_max,
                reset_reasons=dict(resets), gesture_cancel_reasons=dict(gesture_cancels),
                dispatch_cancel_reasons=dict(dispatch_cancels), arrow_spawns_confirmed=None,
                headset_validated=False,
                limits=['Sparse input events do not prove continuous draw or successful release.',
                        'Counter maximum is an observed baseline, not a count of shots or arrows.',
                        'Absent cancellation events do not establish recovery or physical acceptance.'])


def family_readback_reported(f):
    """Check reported synchronous vectors, never infer a Java lease or accept VR."""
    mode=f.get('reload_mode')
    profiles={'WeaponRifleRemington':((1,1),(2,2)),
              'WeaponRifleRemingtonSawOff':((1,1),(2,2)),
              'WeaponPistolDerringer':((1,1),(0,0)),
              'WeaponPistolLemant':((1,8),(1,0)),
              **{name:((6,0),(0,0)) for name in
                 ('WeaponPistolSchofield','WeaponPistolSchofield_A','WeaponPistolSchofield_B')}}
    profile=profiles.get(f.get('weapon_class'))
    if profile is None or mode not in ('indexed','clip'):return False
    indexed=mode=='indexed'
    if indexed!=(f.get('weapon_class') in ('WeaponRifleRemington','WeaponRifleRemingtonSawOff',
                                          'WeaponPistolDerringer','WeaponPistolLemant')):return False
    def vector(key,size):
        parts=f.get(key,'').split(',')
        if len(parts)!=size or any(not re.fullmatch(r'-?\d+',p) for p in parts):raise ValueError(key)
        return tuple(int(p) for p in parts)
    try:
        before,after=vector('loaded_before',2),vector('loaded_after',2)
        reserve,next_reserve=vector('reserves_before',3),vector('reserves_after',3)
        capacities,pools=vector('capacities',2),vector('reserve_indices',2)
        if (capacities,pools)!=profile:return False
        raw_index=integer(f,'selected_index')
        if raw_index not in ((0,1) if indexed else (-1,0)):return False
        index=raw_index if indexed else 0;pool=pools[index]
        if any(v<0 for v in before+after+reserve+next_reserve):return False
        if any(a>cap or b>cap for a,b,cap in zip(before,after,capacities)):return False
        amount=1 if indexed else min(capacities[index]-before[index],reserve[pool])
        if amount<=0 or before[index]>=capacities[index] or reserve[pool]<amount:return False
        if tuple(b-a for a,b in zip(before,after))!=tuple(amount if i==index else 0 for i in range(2)):return False
        if tuple(b-a for a,b in zip(reserve,next_reserve))!=tuple(-amount if i==pool else 0 for i in range(3)):return False
        if [integer(f,k) for k in ('ammo_before','ammo_after','reserve_before','reserve_after')]!=[
                before[index],after[index],reserve[pool],next_reserve[pool]]:return False
        if f.get('reserve_kind')!=('pistol','rifle','shotgun')[pool]:return False
        return (yes(f,'after_valid') and yes(f,'native_call_completed') and yes(f,'feedback_qualified') and
                integer(f,'probe_status_after') in ((2,3) if indexed else (3,)))
    except (ValueError,TypeError,IndexError):return False


def analyze_evidence(directory):
    root = Path(directory).resolve()
    evidence, run, paths = verify_inventory(root)
    events = read_events(paths['runtime/cojvr.log'], evidence)
    finger_observations, finger_issues = finger_reference_observations(events)
    rifle_samples = rifle_support_observations(events)
    winchester_samples = winchester_reload_observations(events)
    indexed_samples, indexed_issues = indexed_reload_observations(events, evidence['runtimeEnded'])
    bow = bow_draw_observations(events)
    manual = run.get('validation', {}).get('manualReloadInstalled') is True
    traces = [e for e in events if e['event'] == 'reload_trace']
    insertions, issues, mechanics = [], list(finger_issues) + indexed_issues, []
    mechanical_baselines = {}
    ready_references = {}
    reported = Counter((e['event'], e['result']) for e in events)
    depth_samples=[dict(line=e['line'],fields=e['fields'],occlusion_admitted=False,
                        content_correspondence='unverified') for e in events if e['event']=='manual_reload_depth']
    depth_eye_pairs=correlate_depth_eye_pairs(depth_samples)
    haptics=Counter(e['fields'].get('event') for e in events
                    if e['event']=='openvr_reload_haptic' and e['result']=='submitted')
    insert_frames = Counter()
    previous = None
    waiting_entries = 0
    for e in traces:
        f = e['fields']
        waiting = coherent_trace(f) and yes(f, 'wait_probe_observed') and integer(f, 'wait_probe_status_after') == 2
        if waiting and (previous is None or not coherent_trace(previous) or
                        owner(f) != owner(previous) or integer(previous, 'wait_probe_status_after') != 2):
            waiting_entries += 1
        previous = f
    for index, e in enumerate(events):
        f, frame = e['fields'], integer(e['fields'], 'frame_sequence')
        if (e['event'] == 'reload_trace' and
                (not coherent_trace(f) or not yes(f, 'manual_session_enabled') or
                 integer(f, 'wait_probe_status_after') not in (1, 2, 3))):
            mechanical_baselines.clear()
        elif e['event'] == 'manual_reload_session' and (
                e['result'] == 'cancelled' or
                (integer(f, 'phase_after') == 0 and integer(f, 'phase_before') != 0)):
            mechanical_baselines.clear()
        if e['event'] == 'manual_reload_insertion':
            insert_frames[frame] += 1
            before = next((r for r in reversed(events[:index]) if r['event'] == 'reload_trace'), None)
            after = next((r for r in events[index+1:] if r['event'] == 'reload_trace'), None)
            numbers = [integer(f, k) for k in ('ammo_before', 'ammo_after', 'reserve_before', 'reserve_after')]
            readable = yes(f, 'after_valid') and all(v is not None and v >= 0 for v in numbers)
            correlated = bool(manual and frame is not None and before and after and
                              coherent_trace(before['fields']) and coherent_trace(after['fields']) and
                              owner(before['fields']) == owner(after['fields']) and
                              owner(after['fields'])[2] == integer(f, 'weapon_id') and
                              integer(after['fields'], 'frame_sequence') == frame and
                              integer(before['fields'], 'frame_sequence') is not None and
                              integer(before['fields'], 'frame_sequence') < frame and
                              yes(before['fields'], 'manual_session_enabled') and
                              yes(before['fields'], 'wait_probe_observed') and
                              integer(before['fields'], 'wait_probe_status_after') == 2 and
                              yes(after['fields'], 'manual_session_enabled') and
                              yes(after['fields'], 'wait_probe_observed') and
                              # The final round may synchronously start native closing.
                              (integer(after['fields'], 'wait_probe_status_after') in (2, 3) or
                               (integer(after['fields'], 'wait_probe_status_after')==1 and
                                yes(f,'native_body_cycle') and yes(after['fields'],'native_body_cycle_after'))) and
                              readable and
                              f.get('reserve_kind', 'pistol') in ('pistol', 'rifle') and
                              trace_reserve_kind(before['fields'], 'after') == f.get('reserve_kind', 'pistol') ==
                              trace_reserve_kind(after['fields'], 'after') and
                              [integer(before['fields'], 'ammo_readback_after'), trace_reserve(before['fields'], 'after')] == [numbers[0], numbers[2]] and
                              [integer(after['fields'], 'ammo_readback_after'), trace_reserve(after['fields'], 'after')] == [numbers[1], numbers[3]])
            delta = [numbers[1]-numbers[0], numbers[3]-numbers[2]] if readable else None
            assessment = 'inconclusive'
            if readable and correlated:
                if e['result'] == 'accepted' and yes(f, 'native_call_completed'):
                    assessment = 'unit_readback_observed' if delta == [1, -1] else 'non_unit_readback'
                elif e['result'] == 'rejected':
                    assessment = 'no_transfer_readback_observed' if delta == [0, 0] else 'readback_changed_after_rejection'
            mode=f.get('reload_mode','unit')
            if mode in ('indexed','clip'):
                # Scalar adjacent traces may describe another eligible index.
                # Keep this as a reported synchronous vector, not a reconstructed
                # transaction or a promotion of sparse samples to native leases.
                correlated=bool(manual and before and after and frame is not None and
                    coherent_trace(before['fields']) and coherent_trace(after['fields']) and
                    owner(before['fields'])==owner(after['fields']) and
                    owner(after['fields'])[2]==integer(f,'weapon_id') and
                    integer(after['fields'],'frame_sequence')==frame and
                    integer(before['fields'],'frame_sequence') is not None and
                    integer(before['fields'],'frame_sequence')<frame and
                    yes(before['fields'],'manual_session_enabled') and yes(after['fields'],'manual_session_enabled'))
                assessment='inconclusive'
                if correlated and e['result']=='accepted':
                    assessment=('indexed_unit_readback_reported' if mode=='indexed' else 'clip_readback_reported') if family_readback_reported(f) else 'invalid_family_readback'
            insertions.append(dict(line=e['line'], frame=frame, weapon_id=integer(f, 'weapon_id'),
                                   result=e['result'], assessment=assessment, delta=delta,
                                   reload_mode=mode,selected_index=integer(f,'selected_index'),
                                   owner_correlated=correlated,
                                   native_call_completed=yes(f, 'native_call_completed')))
            if assessment in ('non_unit_readback', 'readback_changed_after_rejection','invalid_family_readback'):
                issues.append(dict(line=e['line'], kind=assessment))
        elif e['event'] == 'manual_reload_geometry' and (
                yes(f, 'manual_session') or yes(f, 'reference_pose') or
                any(k.startswith('port_') for k in f)):
            is_ready_candidate = yes(f, 'reference_pose')
            latest = next((r for r in reversed(events[:index]) if r['event'] == 'reload_trace'), None)
            native = latest['fields'] if latest else {}
            native_frame = integer(native, 'frame_sequence')
            if frame is None or frame != native_frame:
                correlation = 'missing_same_frame_trace'
            elif integer(native, 'armed_hand') != integer(f, 'hand'):
                correlation = 'other_hand'
            elif not (coherent_trace(native) and yes(native, 'manual_session_enabled') and
                      integer(native, 'wait_probe_status_after') in (1, 2, 3)):
                correlation = 'native_state_unverified'
            else:
                correlation = 'same_frame_armed_hand'
            correlated = correlation == 'same_frame_armed_hand'
            row = dict(line=e['line'], frame=frame, model=f.get('model'), hand=integer(f, 'hand'),
                       gate_open_confirmed=False, trace_correlation=correlation,
                       source=f.get('source', 'legacy_unknown'), native_motion_evidence_eligible=False,
                       ready_reference_eligible=False, ready_reference_available=False,
                       latest_trace_frame=native_frame,
                       latest_probe_status=integer(native, 'wait_probe_status_after') if correlated else None)
            try:
                metrics = geometry_metrics(f)
                row.update(metrics)
                row['native_drum_phase_deg']=native_drum_phase(f)
                row['chamber_occupancy_observed']=False
                port_probe = port_pivot_probe(f)
                if port_probe is not None:
                    row['port_probe'] = port_probe
                native_eligible=correlated and f.get('source')=='pre_native'
                row['native_motion_evidence_eligible']=native_eligible and yes(f, 'manual_session')
                reference_owner=owner(f)
                ready_eligible=(manual and is_ready_candidate and not yes(f, 'manual_session') and
                                f.get('phase')=='ready' and f.get('source')=='pre_native' and
                                yes(f, 'native_observation_valid') and yes(f, 'native_probe_valid') and
                                yes(f, 'context_allowed') and f.get('native_reloading') in ('0','false') and
                                integer(f, 'native_status') in (0,4) and
                                f.get('model') in ('peacemaker','frontier') and
                                all(v is not None for v in reference_owner) and
                                all(v>0 for v in reference_owner[:3]) and
                                reference_owner[3] in (0,1) and
                                integer(f, 'hand')==reference_owner[3] and frame is not None)
                row['ready_reference_eligible']=ready_eligible
                if ready_eligible:
                    # READY has no required same-frame transition trace: native
                    # status/owner were captured independently before overlays.
                    ready_references[(reference_owner,f['model'])]=(frame,metrics)
                elif native_eligible and yes(f, 'manual_session'):
                    reference=ready_references.get((owner(native),f.get('model')))
                    if reference and frame is not None and frame>reference[0]:
                        row['ready_reference_available']=True
                        for name,value in metrics.items():
                            label=name.removesuffix('_cm')+'_change_from_ready'
                            row[label+('_cm' if name.endswith('_cm') else '')]=value-reference[1][name]
                        for part in ('gate','drum'):
                            for anchor in ('root','barrel'):
                                prefix=f'{part}_{anchor}'
                                row[f'{prefix}_rotation_from_ready_deg']=(
                                    relative_orientation_angle_degrees(reference[1],metrics,part,anchor))
                                row[f'{prefix}_translation_from_ready_cm']=math.dist(
                                    tuple(reference[1][f'{prefix}_{axis}_cm'] for axis in 'xyz'),
                                    tuple(metrics[f'{prefix}_{axis}_cm'] for axis in 'xyz'))
                if correlated and f.get('source') in (None,'pre_native'):
                    key = (owner(native), f.get('model'),f.get('source','legacy_unknown'))
                    first = mechanical_baselines.setdefault(key, metrics)
                    for name, value in metrics.items():
                        label = name.removesuffix('_cm') + '_change_from_first'
                        row[label + ('_cm' if name.endswith('_cm') else '')] = value - first[name]
            except (KeyError, ValueError, OverflowError):
                issues.append(dict(line=e['line'], kind='invalid_mechanical_geometry'))
            mechanics.append(row)
        elif e['result'] in ('failed', 'unavailable') and e['event'] in (
                'body_hand_render_probe', 'controller_weapon_render_probe', 'body_hand_restore',
                'controller_weapon_restore', 'manual_reload_presentation'):
            issues.append(dict(line=e['line'], kind=e['event'] + '_' + e['result'], frame=frame))
    for frame, count in insert_frames.items():
        if frame is None or count > 1:
            issues.append(dict(frame=frame, kind='missing_or_repeated_insertion_frame', count=count))
            for row in insertions:
                if row['frame'] == frame:
                    row['assessment'] = 'inconclusive'
    accepted_frames = {r['frame'] for r in insertions if r['result'] == 'accepted' and
                       r['owner_correlated'] and r['native_call_completed'] and
                       r['assessment'] != 'inconclusive'}
    for e in traces:
        f = e['fields']
        if manual and coherent_trace(f) and yes(f, 'manual_session_enabled') and yes(f, 'same_owner_delta') and (
                yes(f, 'wait_probe_observed') and integer(f, 'wait_probe_status_after') in (2, 3) and
                integer(f, 'frame_sequence') not in accepted_frames):
            values = [integer(f, 'ammo_readback_before'), integer(f, 'ammo_readback_after'), trace_reserve(f, 'before'), trace_reserve(f, 'after')]
            if trace_reserve_kind(f, 'before') == trace_reserve_kind(f, 'after') and all(v is not None and v >= 0 for v in values) and (values[1] > values[0] or values[3] < values[2]):
                issues.append(dict(line=e['line'], kind='reload_readback_change_without_insertion'))
    return dict(schemaVersion=1, analysis_type='cojvr-offline-manual-reload', manual_candidate=manual,
                headset_validated=False, provenance=dict(verified=True, runId=evidence['runId'],
                    buildManifestId=evidence['buildManifestId'], inventoried_files=len(paths),
                    runtime_complete=evidence['runtimeEnded'], log_sha256=digest(paths['runtime/cojvr.log'])),
                summary=dict(accepted_insertions=reported['manual_reload_insertion', 'accepted'],
                    rejected_insertions=reported['manual_reload_insertion', 'rejected'],
                    unit_readbacks=sum(r['assessment'] == 'unit_readback_observed' for r in insertions),
                    indexed_unit_readbacks_reported=sum(r['assessment']=='indexed_unit_readback_reported' for r in insertions),
                    clip_readbacks_reported=sum(r['assessment']=='clip_readback_reported' for r in insertions),
                    native_wait_entries=waiting_entries, issue_count=len(issues),
                    zone_entries=reported['manual_reload_zone','entered'],
                    zone_exits=reported['manual_reload_zone','exited'],
                    cartridge_visible_events=reported['manual_reload_cartridge','visible'],
                    finger_reference_samples=sum(r['status']=='observed'
                                                 for r in finger_observations),
                    finger_reference_correlated_samples=sum(r['cartridge_correlated']
                                                            for r in finger_observations),
                    zone_haptics_submitted=haptics['zone_entered'],
                    insertion_haptics_submitted=haptics['insert_accepted'],
                    depth_observation_samples=len(depth_samples),
                    rifle_support_samples=len(rifle_samples),
                    winchester_reload_samples=sum(r['status'] == 'observed' for r in winchester_samples),
                    winchester_reload_unavailable=sum(r['status'] == 'unavailable' for r in winchester_samples),
                    winchester_reload_window_ends=sum(r['status'] == 'window_end' for r in winchester_samples),
                    indexed_reload_samples=sum(r['status'] == 'observed' for r in indexed_samples),
                    indexed_reload_rejected_samples=sum(r['status'] == 'rejected' for r in indexed_samples),
                    bow_draw_samples=bow['draw_samples'],
                    bow_shot_requests=bow['shot_requests'],
                    bow_resets=sum(bow['reset_reasons'].values()),
                    depth_unavailable_samples=sum(not yes(r['fields'],'depth_valid') for r in depth_samples),
                    depth_layout_samples=sum(yes(r['fields'],'layout_matches') for r in depth_samples),
                    depth_correlated_pairs=sum(r['status']=='metadata_correlated' for r in depth_eye_pairs),
                    depth_incomplete_pairs=sum(r['status']=='incomplete' for r in depth_eye_pairs),
                    depth_rejected_pairs=sum(r['status']=='rejected' for r in depth_eye_pairs),
                    correlated_mechanical_samples=sum(r['trace_correlation'] == 'same_frame_armed_hand'
                                                       for r in mechanics),
                    port_pivot_rank_samples=sum(r.get('port_probe', {}).get('status') == 'pivot_only'
                                                for r in mechanics),
                    port_ambiguous_samples=sum(r.get('port_probe', {}).get('status') == 'ambiguous'
                                               for r in mechanics),
                    ready_reference_samples=sum(r['ready_reference_eligible'] for r in mechanics),
                    ready_comparisons=sum(r['ready_reference_available'] for r in mechanics)),
                rifle_support_observations=rifle_samples,
                winchester_reload_observations=winchester_samples,
                indexed_reload_observations=indexed_samples,
                bow_draw_observations=bow,
                insertions=insertions, issues=issues, mechanical_observations=mechanics,
                finger_joint_observations=finger_observations,
                renderer_depth_observations=depth_samples,
                depth_eye_pairs=depth_eye_pairs,
                timeline=events,
                physical_gates_pending=['gate_clearance', 'tracked_hand_comfort', 'sounds', 'interruption_recovery'],
                limits=['Readback observations are not physical acceptance or callback-complete history.',
                        'Sparse trace may miss state transitions; absent samples are not success.',
                        'A cleared-state sample cannot delimit transfers from resumed native firing/reloading.',
                        'Mechanical metrics do not prove an open gate or a safe independent writer.',
                        'Only same-frame coherent traces bind geometry to native status; missing transition traces remain inconclusive.',
                        'Relative mechanical changes compare sparse samples of one owner/session, not continuous animation.',
                        'Winchester sample continuity and rifle reserve changes do not validate native rounds, reload phase or a manual loading port.',
                        'Indexed samples have no cross-poll object epoch or synchronous lease; numeric owner equality does not prove the same object, a transaction, unit insertion, callback completion or native index choice.',
                        'Full-frame relative rotation angles measure pose differences, not a hinge axis or usable loading-port clearance.',
                        'Port ranking measures mouth proximity to an unverified gate element pivot; it never qualifies a loading socket.',
                        'Ordered same-frame/device/generation depth metadata does not prove capture, eye/hand content or occlusion.',
                        'A zone cue grants no ammunition; submitted haptics and cartridge visibility do not validate physical grip.',
                        'Finger-frame deltas are distal joint origins; skin-pad gap minus whole-mesh transverse span is only a nominal envelope margin, not a pad/case intersection or visual-contact measurement.',
                        'Inventory hashes verify internal consistency, not external authentication.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory', type=Path)
    parser.add_argument('--output', type=Path, required=True, help='New JSON file outside the evidence directory')
    args = parser.parse_args()
    try:
        if args.output.resolve().is_relative_to(args.evidence_directory.resolve()) or args.output.exists():
            raise ValueError('Output must be a new file outside the evidence directory')
        report = analyze_evidence(args.evidence_directory)
        with args.output.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2, allow_nan=False)
            stream.write('\n')
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f'Evidence analysis rejected: {error}\n')
    print(json.dumps(report['summary'], indent=2))
    print('Physical VR gates remain pending. Analysis written to:', args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
