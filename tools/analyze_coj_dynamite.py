"""Correlate dynamite gesture and native receipts in an inventoried CoJ run.

This read-only analysis supplies bounded observational candidates. A physical
throw, fuse behavior, trajectory and headset acceptance require a headset run.
"""

import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
import re

from analyze_coj_manual_reload import (digest, integer, mechanical_vector,
                                       read_events, verify_inventory)


# These are conservative analysis horizons measured in render frame sequences,
# not game timings or eligibility controls. Missed samples remain inconclusive.
GESTURE_BEGIN_WINDOW_FRAMES = 90
BEGIN_RELEASE_WINDOW_FRAMES = 900
EVENT_TYPES = ('dynamite_motion', 'dynamite_lifecycle')


def positive(fields, key, minimum=1):
    value = integer(fields, key)
    if value is None or value < minimum:
        raise ValueError(f'Invalid {key}')
    return value


def native_weapon_id(fields):
    weapon_id = positive(fields, 'weapon_id')
    if weapon_id >= 0xffffffff:
        raise ValueError('Invalid weapon_id')
    return weapon_id


def parse_receipt(entry):
    fields = entry['fields']
    if entry.get('parse_issues'):
        raise ValueError(','.join(entry['parse_issues']))
    row = dict(line=entry['line'], event=entry['event'], result=entry['result'])
    if entry['event'] == 'dynamite_motion':
        if entry['result'] not in ('requested', 'withheld'):
            raise ValueError('Unknown gesture status')
        row.update(frame=positive(fields, 'frame_sequence'),
                   pose_sequence=positive(fields, 'pose_sequence'),
                   owner_epoch=positive(fields, 'owner_epoch'),
                   release_delivered=positive(fields, 'release_delivered', 0))
        has_weapon = 'weapon_id' in fields
        has_player = 'player_generation' in fields
        if has_weapon != has_player:
            raise ValueError('Incomplete gesture owner identity')
        if has_weapon:
            row.update(weapon_id=native_weapon_id(fields),
                       player_generation=positive(fields, 'player_generation'))
        if (positive(fields, 'native_action') != 10 or row['release_delivered'] not in (0, 1)
                or (row['result'] == 'withheld' and row['release_delivered'] != 0)):
            raise ValueError('Inconsistent native action or release dispatch')
    elif entry['event'] == 'dynamite_lifecycle':
        if entry['result'] not in ('attack_begin', 'native_release'):
            raise ValueError('Unknown native receipt status')
        row.update(frame=positive(fields, 'observed_frame'),
                   player_generation=positive(fields, 'player_generation'),
                   weapon_id=native_weapon_id(fields), serial=positive(fields, 'serial'))
        if entry['result'] == 'attack_begin':
            row['fuse_seconds'] = float(fields['fuse_seconds'])
            if not math.isfinite(row['fuse_seconds']) or row['fuse_seconds'] < 0:
                raise ValueError('Invalid fuse interval')
            row['spark_handle'] = positive(fields, 'spark_handle', 0)
            row['smoke_handle'] = positive(fields, 'smoke_handle', 0)
        else:
            row['origin_cm'] = mechanical_vector(fields['origin_cm'])
            row['requested_velocity_cm_s'] = mechanical_vector(fields['requested_velocity_cm_s'])
    else:
        raise ValueError('Unknown event')
    return row


def correlate(events, runtime_complete):
    """Use ordered observations and unique candidates, never inferred native causality."""
    rows, issues = [], []
    last_frame = 0
    for entry in events:
        if entry['event'] not in EVENT_TYPES:
            continue
        try:
            row = parse_receipt(entry)
            if row['frame'] < last_frame:
                raise ValueError('non_monotonic_frame')
            last_frame = row['frame']
            row['valid'] = True
        except (KeyError, ValueError, OverflowError) as error:
            row = dict(line=entry['line'], event=entry['event'], result=entry['result'],
                       valid=False)
            issues.append(dict(line=entry['line'], kind='invalid_observation', detail=str(error)))
        rows.append(row)

    native_by_serial = defaultdict(list)
    for row in rows:
        if row['valid'] and row['event'] == 'dynamite_lifecycle':
            native_by_serial[row['player_generation'], row['result'], row['serial']].append(row)
    for duplicates in native_by_serial.values():
        if len(duplicates) > 1:
            for row in duplicates:
                row['valid'] = False
                issues.append(dict(line=row['line'], kind='repeated_serial'))

    # Each Java owner starts both native counters at zero. A skipped serial
    # proves that at least one callback was lost between observations, so the
    # surviving snapshot cannot establish a unique begin/release handoff.
    last_serial = {}
    for row in rows:
        if row['event'] != 'dynamite_lifecycle' or not row['valid']:
            continue
        key = (row['player_generation'], row['result'])
        expected = last_serial.get(key, 0) + 1
        last_serial[key] = row['serial']
        if row['serial'] != expected:
            row['valid'] = False
            issues.append(dict(line=row['line'], kind='serial_gap',
                               expected=expected, observed=row['serial']))

    motions = [dict(row, assessment=('withheld' if row['result'] == 'withheld' else
                   'dispatch_incomplete' if row['release_delivered'] == 0 else 'unpaired')
                   if row['valid'] else 'invalid') for row in rows
               if row['event'] == 'dynamite_motion']
    native_pairs, native_unpaired = [], []
    active_begins = []
    paired_lines = set()
    for row in rows:
        if row['event'] != 'dynamite_lifecycle':
            continue
        if not row['valid']:
            native_unpaired.append(dict(line=row['line'], result=row['result'], reason='invalid'))
            active_begins.clear()
            continue
        if row['result'] == 'attack_begin':
            active_begins = [begin for begin in active_begins
                             if row['frame'] - begin['frame'] <= BEGIN_RELEASE_WINDOW_FRAMES]
            active_begins.append(row)
            continue
        eligible = [begin for begin in active_begins
                    if 0 <= row['frame'] - begin['frame'] <= BEGIN_RELEASE_WINDOW_FRAMES]
        if (len(eligible) == 1 and eligible[0]['frame'] < row['frame'] and
                (eligible[0]['player_generation'], eligible[0]['weapon_id']) ==
                (row['player_generation'], row['weapon_id'])):
            begin = eligible[0]
            paired_lines.update((begin['line'], row['line']))
            native_pairs.append(dict(begin_line=begin['line'], release_line=row['line'],
                                     begin_frame=begin['frame'], release_frame=row['frame'],
                                     player_generation=row['player_generation'],
                                     weapon_id=row['weapon_id'],
                                     begin_serial=begin['serial'], release_serial=row['serial'],
                                     fuse_seconds=begin['fuse_seconds'],
                                     origin_cm=row['origin_cm'],
                                     requested_velocity_cm_s=row['requested_velocity_cm_s'],
                                     gesture_line=None, identity_relation='unattributed',
                                     assessment='same_owner_bounded'))
        else:
            native_unpaired.append(dict(line=row['line'], result=row['result'],
                                        reason='ambiguous' if len(eligible) > 1 else
                                               'same_observed_frame' if eligible and
                                               eligible[0]['frame'] == row['frame'] else
                                               'owner_mismatch' if eligible else 'missing_begin'))
        active_begins.clear()

    for row in rows:
        if row['event'] == 'dynamite_lifecycle' and row['valid'] and row['result'] == 'attack_begin':
            if row['line'] not in paired_lines:
                native_unpaired.append(dict(line=row['line'], result=row['result'],
                                            reason='missing_unique_release'))
    native_unpaired.sort(key=lambda entry: entry['line'])

    all_begins = [row for row in rows if row['event'] == 'dynamite_lifecycle' and
                  row['result'] == 'attack_begin' and row['valid']]
    begin_pairs = {pair['begin_line']: pair for pair in native_pairs}
    requested = [row for row in motions if row['valid'] and row['assessment'] == 'unpaired']
    completed_releases = {}
    for motion in requested:
        first_release_line = next((row['line'] for row in rows
                                   if row['line'] > motion['line'] and
                                   row['event'] == 'dynamite_lifecycle' and
                                   row['result'] == 'native_release'), None)
        begins = [row for row in all_begins if row['line'] > motion['line'] and
                  (first_release_line is None or row['line'] < first_release_line) and
                  0 <= row['frame'] - motion['frame'] <= GESTURE_BEGIN_WINDOW_FRAMES]
        if len(begins) > 1:
            motion['assessment'] = 'ambiguous'
            continue
        if len(begins) != 1:
            continue
        begin = begins[0]
        pair = begin_pairs.get(begin['line'])
        if pair is None:
            # An observed begin without a unique native release remains unresolved.
            motion['assessment'] = 'ambiguous' if native_unpaired else 'unpaired'
            continue
        if 'weapon_id' in motion and (motion['weapon_id'], motion['player_generation']) != (
                pair['weapon_id'], pair['player_generation']):
            motion['assessment'] = 'owner_mismatch'
            pair['identity_relation'] = 'mismatch'
            continue
        competing = [other for other in motions if other['line'] != motion['line'] and
                     other.get('valid') and other['line'] < begin['line'] and
                     other.get('frame') is not None and
                     0 <= begin['frame'] - other['frame'] <= GESTURE_BEGIN_WINDOW_FRAMES and
                     completed_releases.get(other['line'], motion['line']) >= motion['line']]
        # Any other dynamite observation in this proposed triple leaves
        # attribution uncertain, including an interruption during the fuse.
        intervening = [row for row in rows if motion['line'] < row['line'] < pair['release_line']
                       and row['line'] != begin['line']]
        if competing or intervening:
            motion['assessment'] = 'ambiguous'
            continue
        pair['gesture_line'] = motion['line']
        pair['identity_relation'] = 'matched' if 'weapon_id' in motion else 'legacy_unverified'
        completed_releases[motion['line']] = pair['release_line']
        motion['assessment'] = 'candidate_correlated' if runtime_complete else 'run_incomplete'

    if not runtime_complete:
        for pair in native_pairs:
            pair['gesture_line'] = None

    counts = Counter(motion['assessment'] for motion in motions)
    return dict(motions=motions, native_pairs=native_pairs, native_unpaired=native_unpaired,
                issues=issues, summary=dict(total_gestures=len(motions),
                    withheld=counts['withheld'], dispatch_incomplete=counts['dispatch_incomplete'],
                    candidate_correlations=counts['candidate_correlated'],
                    same_weapon_candidates=sum(pair['identity_relation'] == 'matched' and
                                               pair['gesture_line'] is not None
                                               for pair in native_pairs),
                    owner_mismatch=counts['owner_mismatch'],
                    ambiguous=counts['ambiguous'], invalid=counts['invalid'],
                    native_pairs=len(native_pairs), native_unpaired=len(native_unpaired)))


def process_id(log, run_id, build_id):
    markers = [line for line in log.read_text(encoding='utf-8-sig').splitlines()
               if line.startswith('run_start:')]
    if len(markers) != 1:
        raise ValueError('Expected one process start marker')
    marker = re.fullmatch(r'run_start: run_id=(\S+) build_manifest_id=(\S+) pid=(\d+)', markers[0])
    if not marker or marker.groups()[:2] != (run_id, build_id) or int(marker[3]) < 1:
        raise ValueError('Missing or inconsistent native process identity')
    return int(marker[3])


def analyze_evidence(directory):
    root = Path(directory).resolve()
    evidence, _run, paths = verify_inventory(root)
    log = paths['runtime/cojvr.log']
    events = read_events(log, evidence, extra_events=EVENT_TYPES)
    pid = process_id(log, evidence['runId'], evidence['buildManifestId'])
    observations = correlate(events, evidence['runtimeEnded'])
    return dict(schemaVersion=1, analysis_type='cojvr-offline-dynamite',
                headset_validated=False,
                provenance=dict(verified=True, runId=evidence['runId'],
                    buildManifestId=evidence['buildManifestId'], processId=pid,
                    runtime_complete=evidence['runtimeEnded'],
                    log_sha256=digest(log)),
                **observations,
                limits=[
                    'A unique frame-bounded gesture, attack-begin and release sequence is an observational candidate, not a causal or physical throw proof.',
                    'Current gestures carry native weapon_id and player_generation. Legacy gestures have owner_epoch only; native begin and release have independent serials.',
                    'Matching weapon identities strengthen observational correlation but do not prove gesture causality.',
                    'Sparse native counters can skip events. Native-frame observations do not prove uninterrupted fuse, inventory removal or projectile trajectory.',
                    'JNI release_delivered confirms dispatch of input press/release only.',
                    'Physical trigger interaction, interruptions and headset acceptance remain pending.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory', type=Path)
    parser.add_argument('--output', type=Path, required=True,
                        help='Path to a new JSON file outside the evidence directory')
    args = parser.parse_args()
    try:
        if args.output.resolve().is_relative_to(args.evidence_directory.resolve()) or args.output.exists():
            raise ValueError('Output must be a new file outside the evidence directory')
        report = analyze_evidence(args.evidence_directory)
        with args.output.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2, allow_nan=False)
            stream.write('\n')
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f'Dynamite evidence analysis rejected: {error}\n')
    print(json.dumps(report['summary'], indent=2))
    print('Physical VR gates remain pending. Analysis written to:', args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
