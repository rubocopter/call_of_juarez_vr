"""Read-only mounted input/native-cache correlation for an inventoried CoJ run.

This is a sparse post-dispatch, pre-simulation observation. Optional spatial
endpoints measure world displacement, not instantaneous speed, a same-frame
input response, VR comfort or headset acceptance.
"""
import argparse
import json
import math
from pathlib import Path
import re

from analyze_coj_manual_reload import digest, read_events, verify_inventory

EVENT = 'mounted_native_motion'
SOURCE = 'post_input_pre_simulation_cached_fields'
MAX_RECORDS = 2048
# A 10 Hz trace may skip samples in normal operation. Beyond this interval,
# nearby observations are no longer paired as a contiguous input comparison.
MAX_PAIR_GAP_US = 500_000

UINT64 = (1 << 64) - 1
UINT32 = (1 << 32) - 1

COMMON_UINT = {'frame_sequence', 'pose_sequence', 'native_context',
               'input_context_generation', 'monotonic_us', 'input_time_us',
               'record_index', 'record_limit', 'mapped_digital_available'}
COMMON_FLOAT = {'head_relative_move_yaw_degrees'}
COMMON_VECTOR = {'raw_move': 2, 'raw_turn': 2, 'mapped_move': 2, 'mapped_turn': 2}
COMMON_BOOL = {'raw_move_available', 'raw_turn_available', 'mapped_active',
               'mapped_move_available', 'mapped_turn_available',
               'raw_contextual_shoulder_held', 'mapped_run', 'vr_gameplay_input_enabled',
               'input_dispatch_ok', 'sample_valid', 'budget_exhausted', 'read_only',
               'same_frame_response_proven'}
COMMON_LITERAL = {'source'}
NATIVE_UINT = {'player_generation', 'player_id', 'horse_id'}
NATIVE_SIGNED = {'native_speed_key_state', 'native_speed_state'}
NATIVE_FLOAT = {'native_forward_request', 'native_side_request', 'native_turn_degrees_s'}
NATIVE_VECTOR = {'native_cached_velocity_cm_s': 3, 'rider_cached_velocity_cm_s': 3}
NATIVE_BOOL = {'native_run', 'rider_run', 'native_can_move', 'native_tied',
               'native_new_player_control'}
SPATIAL_FIELDS = {'spatial_observation_version', 'spatial_valid'}
SPATIAL_VECTOR = {'rider_world_position_cm': 3, 'natural_camera_position_cm': 3,
                  'natural_camera_forward': 3}
COMMON_FIELDS = (COMMON_UINT | COMMON_FLOAT | COMMON_VECTOR.keys() |
                 COMMON_BOOL | COMMON_LITERAL)
NATIVE_FIELDS = (NATIVE_UINT | NATIVE_SIGNED | NATIVE_FLOAT |
                 NATIVE_VECTOR.keys() | NATIVE_BOOL)


def parse_unsigned(value, *, limit=UINT64):
    if not re.fullmatch(r'[0-9]+', value):
        raise ValueError('invalid_unsigned_integer')
    number = int(value)
    if number > limit:
        raise ValueError('integer_out_of_range')
    return number


def parse_signed(value):
    if not re.fullmatch(r'-?[0-9]+', value):
        raise ValueError('invalid_signed_integer')
    number = int(value)
    if not -(1 << 31) <= number < (1 << 31):
        raise ValueError('integer_out_of_range')
    return number


def parse_finite(value):
    number = float(value)
    if not math.isfinite(number):
        raise ValueError('nonfinite_value')
    return number


def parse_vector(value, count):
    parts = value.split(',')
    if len(parts) != count:
        raise ValueError('wrong_vector_size')
    return [parse_finite(part) for part in parts]


def parse_bool(value):
    if value not in ('true', 'false'):
        raise ValueError('invalid_boolean')
    return value == 'true'


def validated_sample(event):
    fields = event['fields']
    if event.get('parse_issues'):
        raise ValueError('malformed_fields:' + ','.join(event['parse_issues']))
    result = event['result']
    if result not in ('observed', 'unavailable'):
        raise ValueError('invalid_result')
    spatial_schema = bool(set(fields) & (SPATIAL_FIELDS | SPATIAL_VECTOR.keys()))
    expected = COMMON_FIELDS | (NATIVE_FIELDS if result == 'observed' else set())
    if spatial_schema:
        expected |= SPATIAL_FIELDS
        if fields.get('spatial_valid') == 'true':
            expected |= SPATIAL_VECTOR.keys()
    if set(fields) != expected:
        raise ValueError('field_set_mismatch:missing=' + ','.join(sorted(expected-set(fields))) +
                         ':extra=' + ','.join(sorted(set(fields)-expected)))
    sample = {'line': event['line'], 'result': result}
    for key in COMMON_UINT:
        sample[key] = parse_unsigned(fields[key],
                                     limit=UINT32 if key == 'mapped_digital_available' else UINT64)
    for key in COMMON_FLOAT:
        sample[key] = parse_finite(fields[key])
    for key, count in COMMON_VECTOR.items():
        sample[key] = parse_vector(fields[key], count)
    for key in COMMON_BOOL:
        sample[key] = parse_bool(fields[key])
    sample['spatial_observation_version'] = 0
    sample['spatial_valid'] = False
    if spatial_schema:
        sample['spatial_observation_version'] = parse_unsigned(
            fields['spatial_observation_version'], limit=UINT32)
        sample['spatial_valid'] = parse_bool(fields['spatial_valid'])
        if sample['spatial_observation_version'] != 1 or (sample['spatial_valid'] and result != 'observed'):
            raise ValueError('invalid_spatial_contract')
        if sample['spatial_valid']:
            for key, count in SPATIAL_VECTOR.items():
                sample[key] = parse_vector(fields[key], count)
            forward = sample['natural_camera_forward']
            if not 0.98 <= math.hypot(*forward) <= 1.02:
                raise ValueError('invalid_natural_camera_forward')
    if result == 'observed':
        for key in NATIVE_UINT:
            sample[key] = parse_unsigned(fields[key], limit=UINT32 if key.endswith('_id') else UINT64)
        for key in NATIVE_SIGNED:
            sample[key] = parse_signed(fields[key])
        for key in NATIVE_FLOAT:
            sample[key] = parse_finite(fields[key])
        for key, count in NATIVE_VECTOR.items():
            sample[key] = parse_vector(fields[key], count)
        for key in NATIVE_BOOL:
            sample[key] = parse_bool(fields[key])
        if (not sample['player_generation'] or sample['player_id'] in (0, UINT32) or
                sample['horse_id'] in (0, UINT32) or sample['player_id'] == sample['horse_id']):
            raise ValueError('invalid_native_identity')
        sample['cached_horse_speed_cm_s'] = math.hypot(*sample['native_cached_velocity_cm_s'])
        sample['cached_rider_speed_cm_s'] = math.hypot(*sample['rider_cached_velocity_cm_s'])
        if not all(math.isfinite(sample[key]) for key in
                   ('cached_horse_speed_cm_s', 'cached_rider_speed_cm_s')):
            raise ValueError('nonfinite_derived_speed')
    if fields['source'] != SOURCE:
        raise ValueError('wrong_source')
    if (not sample['read_only'] or sample['same_frame_response_proven'] or
            sample['sample_valid'] != (result == 'observed')):
        raise ValueError('invalid_observation_contract')
    if (sample['record_limit'] != MAX_RECORDS or
            not 1 <= sample['record_index'] <= MAX_RECORDS or
            sample['budget_exhausted'] != (sample['record_index'] == MAX_RECORDS)):
        raise ValueError('invalid_budget')
    if (not sample['frame_sequence'] or not sample['native_context'] & 1 or
            not sample['monotonic_us']):
        raise ValueError('invalid_frame_or_context')
    return sample


def direction(value):
    return (value > 0) - (value < 0)


def compare(previous, current):
    if (current['frame_sequence'] <= previous['frame_sequence'] or
            current['monotonic_us'] <= previous['monotonic_us'] or
            current['input_time_us'] < previous['input_time_us']):
        return 'clock_regression'
    if (previous['player_generation'], previous['player_id'], previous['horse_id']) != (
            current['player_generation'], current['player_id'], current['horse_id']):
        return 'owner_changed'
    if (previous['native_context'], previous['input_context_generation']) != (
            current['native_context'], current['input_context_generation']):
        return 'context_changed'
    if current['record_index'] != previous['record_index'] + 1:
        return 'record_index_gap'
    if current['monotonic_us'] - previous['monotonic_us'] > MAX_PAIR_GAP_US:
        return 'time_gap'
    if not all(row[key] for row in (previous, current) for key in (
            'vr_gameplay_input_enabled', 'input_dispatch_ok', 'mapped_active',
            'mapped_move_available', 'mapped_turn_available',
            'raw_move_available', 'raw_turn_available')):
        return 'unavailable_input'
    # The contextual right-shoulder binding is the kick channel (bit 9).
    # This is a mask, not a bool; availability of another button is irrelevant.
    if not all(row['mapped_digital_available'] & (1 << 9) for row in (previous, current)):
        return 'unavailable_shoulder_binding'
    return None


def transition(kind, before, after):
    return dict(kind=kind, before_line=before['line'], after_line=after['line'],
                before_frame=before['frame_sequence'], after_frame=after['frame_sequence'],
                elapsed_us=after['monotonic_us']-before['monotonic_us'],
                r1_before=before['raw_contextual_shoulder_held'],
                r1_after=after['raw_contextual_shoulder_held'],
                mapped_run_before=before['mapped_run'], mapped_run_after=after['mapped_run'],
                native_run_before=before['native_run'], native_run_after=after['native_run'],
                native_side_request_before=before['native_side_request'],
                native_side_request_after=after['native_side_request'],
                native_turn_degrees_s_before=before['native_turn_degrees_s'],
                native_turn_degrees_s_after=after['native_turn_degrees_s'],
                native_speed_state_before=before['native_speed_state'],
                native_speed_state_after=after['native_speed_state'],
                cached_horse_speed_before_cm_s=before['cached_horse_speed_cm_s'],
                cached_horse_speed_after_cm_s=after['cached_horse_speed_cm_s'],
                same_frame_response_proven=False)


def spatial_pair(before, after):
    """World position chords and natural-camera yaw across admitted endpoints."""
    elapsed = (after['monotonic_us'] - before['monotonic_us']) / 1_000_000
    rider_delta = [b-a for a, b in zip(before['rider_world_position_cm'],
                                      after['rider_world_position_cm'])]
    camera_delta = [b-a for a, b in zip(before['natural_camera_position_cm'],
                                       after['natural_camera_position_cm'])]
    forward_a = before['natural_camera_forward']
    forward_b = after['natural_camera_forward']
    yaw = None
    if math.hypot(forward_a[0], forward_a[2]) > 0.05 and math.hypot(forward_b[0], forward_b[2]) > 0.05:
        yaw = math.degrees(math.atan2(forward_a[0]*forward_b[2]-forward_a[2]*forward_b[0],
                                      forward_a[0]*forward_b[0]+forward_a[2]*forward_b[2]))
    return dict(before_line=before['line'], after_line=after['line'],
                elapsed_us=after['monotonic_us']-before['monotonic_us'],
                rider_displacement_cm=rider_delta,
                rider_horizontal_chord_speed_cm_s=math.hypot(rider_delta[0], rider_delta[2])/elapsed,
                rider_3d_chord_speed_cm_s=math.hypot(*rider_delta)/elapsed,
                natural_camera_displacement_cm=camera_delta,
                natural_camera_horizontal_chord_speed_cm_s=math.hypot(camera_delta[0], camera_delta[2])/elapsed,
                natural_camera_signed_yaw_delta_degrees=yaw,
                mapped_turn_before=before['mapped_turn'], mapped_turn_after=after['mapped_turn'],
                native_side_request_before=before['native_side_request'],
                native_side_request_after=after['native_side_request'],
                native_turn_degrees_s_before=before['native_turn_degrees_s'],
                native_turn_degrees_s_after=after['native_turn_degrees_s'],
                same_frame_response_proven=False)


def analyze_evidence(directory):
    root = Path(directory).resolve()
    evidence, run, paths = verify_inventory(root)
    events = read_events(paths['runtime/cojvr.log'], evidence, extra_events={EVENT})
    issues, observations, transitions, spatial_pairs = [], [], [], []
    counts = dict(valid_samples=0, unavailable_samples=0, rejected_samples=0,
                  comparable_pairs=0, spatial_samples=0, spatial_pairs=0)
    previous = None
    for event in events:
        if event['event'] != EVENT:
            continue
        try:
            sample = validated_sample(event)
        except (KeyError, TypeError, ValueError, OverflowError) as error:
            counts['rejected_samples'] += 1
            issues.append(dict(line=event['line'], kind='rejected_sample', reason=str(error)))
            previous = None
            continue
        if sample['result'] == 'unavailable':
            counts['unavailable_samples'] += 1
            previous = None
            continue
        counts['valid_samples'] += 1
        if sample['spatial_valid']:
            counts['spatial_samples'] += 1
        observations.append(sample)
        if previous is not None:
            issue = compare(previous, sample)
            if issue:
                issues.append(dict(line=sample['line'], kind=issue,
                                   previous_line=previous['line']))
            else:
                counts['comparable_pairs'] += 1
                if previous['spatial_valid'] and sample['spatial_valid']:
                    spatial_pairs.append(spatial_pair(previous, sample))
                    counts['spatial_pairs'] += 1
                if (previous['raw_contextual_shoulder_held'] != sample['raw_contextual_shoulder_held'] or
                        previous['mapped_run'] != sample['mapped_run']):
                    transitions.append(transition('r1_or_run_changed', previous, sample))
                if direction(previous['native_side_request']) != direction(sample['native_side_request']):
                    transitions.append(transition('native_reins_sign_changed', previous, sample))
        previous = sample
    counts['issue_count'] = len(issues)
    return dict(schemaVersion=2, analysis_type='cojvr-offline-mounted-motion',
                headset_validated=False,
                provenance=dict(verified=True, runId=evidence['runId'],
                                buildManifestId=evidence['buildManifestId'],
                                inventoried_files=len(paths),
                                runtime_complete=evidence['runtimeEnded'],
                                log_sha256=digest(paths['runtime/cojvr.log'])),
                summary=counts, observations=observations, input_transitions=transitions,
                spatial_pairs=spatial_pairs,
                issues=issues, limits=[
                    'Inventory hashes establish internal consistency, not external authentication.',
                    'Observations are sparse and sampled after input dispatch, before simulation.',
                    'Input requests, cached horse/rider velocities and native turn rate have separate timing.',
                    'Cached rider velocity omits parent/mount motion. Spatial endpoints use PlayerBeing.GetPosition and the natural pre-VR camera basis.',
                    'World endpoint chord speeds are interval displacement / elapsed monotonic time, not instantaneous speed or path length.',
                    'Signed natural-camera yaw uses atan2 in world XZ; it does not establish a controller-to-right/left steering convention.',
                    'Legacy records without optional spatial fields remain valid and cannot provide displacement.',
                    'Nearby samples do not prove same-frame response or causal input-to-motion behavior.',
                    'Missing, failed and gap samples never prove an unchanged or successful state.',
                    'No headset, steering, gallop, comfort or physical gate is accepted by this report.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory', type=Path)
    parser.add_argument('--output', required=True, type=Path,
                        help='New JSON file outside the inventoried evidence directory')
    args = parser.parse_args()
    try:
        if (args.output.resolve().is_relative_to(args.evidence_directory.resolve()) or
                args.output.exists()):
            raise ValueError('Output must be a new file outside the evidence directory')
        report = analyze_evidence(args.evidence_directory)
        with args.output.open('x', encoding='utf-8') as output:
            json.dump(report, output, indent=2, allow_nan=False)
            output.write('\n')
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, f'Mounted evidence analysis rejected: {error}\n')
    print(json.dumps(report['summary'], indent=2))
    print('Mounted physical VR gates remain pending. Analysis written to:', args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
