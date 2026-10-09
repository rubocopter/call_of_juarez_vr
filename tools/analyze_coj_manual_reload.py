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
IDENTITY = re.compile(r'run_start: run_id=(\S+) build_manifest_id=(\S+)(?:\s|$)')
END = re.compile(r'run_end: run_id=(\S+)(?:\s|$)')
RELEVANT = {'reload_trace', 'manual_reload_session', 'manual_reload_insertion',
            'manual_reload_presentation', 'manual_reload_geometry', 'body_hand_render_probe',
            'controller_weapon_render_probe', 'body_hand_restore', 'controller_weapon_restore'}


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
        match = EVENT.search(line)
        if inside and match and match[1] in RELEVANT:
            # Error descriptions can contain semicolons/key-like text. Treat the
            # trailing opaque detail as data, never extra telemetry or instructions.
            fields = {}
            for pair in match[3].split(';'):
                if '=' not in pair:
                    continue
                key, value = pair.split('=', 1)
                if key == 'detail':
                    break
                if key in fields:
                    raise ValueError(f'Duplicate telemetry field on line {number}')
                fields[key] = value
            events.append(dict(line=number, event=match[1], result=match[2], fields=fields))
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


def analyze_evidence(directory):
    root = Path(directory).resolve()
    evidence, run, paths = verify_inventory(root)
    events = read_events(paths['runtime/cojvr.log'], evidence)
    manual = run.get('validation', {}).get('manualReloadInstalled') is True
    traces = [e for e in events if e['event'] == 'reload_trace']
    insertions, issues, mechanics = [], [], []
    mechanical_baselines = {}
    ready_references = {}
    reported = Counter((e['event'], e['result']) for e in events)
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
                              integer(after['fields'], 'wait_probe_status_after') in (2, 3) and
                              readable and
                              [integer(before['fields'], k) for k in ('ammo_readback_after', 'pistol_reserve_after')] == [numbers[0], numbers[2]] and
                              [integer(after['fields'], k) for k in ('ammo_readback_after', 'pistol_reserve_after')] == [numbers[1], numbers[3]])
            delta = [numbers[1]-numbers[0], numbers[3]-numbers[2]] if readable else None
            assessment = 'inconclusive'
            if readable and correlated:
                if e['result'] == 'accepted' and yes(f, 'native_call_completed'):
                    assessment = 'unit_readback_observed' if delta == [1, -1] else 'non_unit_readback'
                elif e['result'] == 'rejected':
                    assessment = 'no_transfer_readback_observed' if delta == [0, 0] else 'readback_changed_after_rejection'
            insertions.append(dict(line=e['line'], frame=frame, weapon_id=integer(f, 'weapon_id'),
                                   result=e['result'], assessment=assessment, delta=delta,
                                   owner_correlated=correlated,
                                   native_call_completed=yes(f, 'native_call_completed')))
            if assessment in ('non_unit_readback', 'readback_changed_after_rejection'):
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
            values = [integer(f, k) for k in ('ammo_readback_before', 'ammo_readback_after', 'pistol_reserve_before', 'pistol_reserve_after')]
            if all(v is not None and v >= 0 for v in values) and (values[1] > values[0] or values[3] < values[2]):
                issues.append(dict(line=e['line'], kind='reload_readback_change_without_insertion'))
    return dict(schemaVersion=1, analysis_type='cojvr-offline-manual-reload', manual_candidate=manual,
                headset_validated=False, provenance=dict(verified=True, runId=evidence['runId'],
                    buildManifestId=evidence['buildManifestId'], inventoried_files=len(paths),
                    runtime_complete=evidence['runtimeEnded'], log_sha256=digest(paths['runtime/cojvr.log'])),
                summary=dict(accepted_insertions=reported['manual_reload_insertion', 'accepted'],
                    rejected_insertions=reported['manual_reload_insertion', 'rejected'],
                    unit_readbacks=sum(r['assessment'] == 'unit_readback_observed' for r in insertions),
                    native_wait_entries=waiting_entries, issue_count=len(issues),
                    correlated_mechanical_samples=sum(r['trace_correlation'] == 'same_frame_armed_hand'
                                                       for r in mechanics),
                    port_pivot_rank_samples=sum(r.get('port_probe', {}).get('status') == 'pivot_only'
                                                for r in mechanics),
                    port_ambiguous_samples=sum(r.get('port_probe', {}).get('status') == 'ambiguous'
                                               for r in mechanics),
                    ready_reference_samples=sum(r['ready_reference_eligible'] for r in mechanics),
                    ready_comparisons=sum(r['ready_reference_available'] for r in mechanics)),
                insertions=insertions, issues=issues, mechanical_observations=mechanics,
                timeline=events,
                physical_gates_pending=['gate_clearance', 'tracked_hand_comfort', 'sounds', 'interruption_recovery'],
                limits=['Readback observations are not physical acceptance or callback-complete history.',
                        'Sparse trace may miss state transitions; absent samples are not success.',
                        'A cleared-state sample cannot delimit transfers from resumed native firing/reloading.',
                        'Mechanical metrics do not prove an open gate or a safe independent writer.',
                        'Only same-frame coherent traces bind geometry to native status; missing transition traces remain inconclusive.',
                        'Relative mechanical changes compare sparse samples of one owner/session, not continuous animation.',
                        'Full-frame relative rotation angles measure pose differences, not a hinge axis or usable loading-port clearance.',
                        'Port ranking measures mouth proximity to an unverified gate element pivot; it never qualifies a loading socket.',
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
