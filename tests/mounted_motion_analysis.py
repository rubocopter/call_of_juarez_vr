"""Synthetic, inventoried run fixtures for the mounted diagnostic analyzer."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from analyze_coj_mounted_motion import analyze_evidence  # noqa: E402

RUN = 'mounted-synthetic'
BUILD = 'a' * 64


def mounted(result='observed', **changes):
    fields = dict(frame_sequence=11, pose_sequence=12, native_context=5,
                  input_context_generation=3, monotonic_us=1000000, input_time_us=990000,
                  raw_move='0,0.6', raw_turn='0,0', raw_move_available='true',
                  raw_turn_available='true', mapped_move='0,0.6', mapped_turn='0,0',
                  mapped_active='true', mapped_move_available='true',
                  mapped_turn_available='true', mapped_digital_available=4160756335,
                  raw_contextual_shoulder_held='false', mapped_run='false',
                  vr_gameplay_input_enabled='true', input_dispatch_ok='true',
                  head_relative_move_yaw_degrees=0, sample_valid='true',
                  player_generation=7, player_id=0x80000011, horse_id=0x80000042,
                  native_forward_request=0.5, native_side_request=0,
                  native_turn_degrees_s=0, native_speed_key_state=1, native_speed_state=1,
                  native_run='false', rider_run='false', native_can_move='true',
                  native_tied='false', native_new_player_control='true',
                  native_cached_velocity_cm_s='0,0,250', rider_cached_velocity_cm_s='0,0,0',
                  record_index=1, record_limit=2048, budget_exhausted='false',
                  source='post_input_pre_simulation_cached_fields',
                  read_only='true', same_frame_response_proven='false')
    fields.update(changes)
    return 'camera_probe_event: event=mounted_native_motion result=' + result + ' detail=' + ';'.join(
        f'{key}={value}' for key, value in fields.items())


def spatial(**changes):
    fields = dict(spatial_observation_version=1, spatial_valid='true',
                  rider_world_position_cm='0,0,0', natural_camera_position_cm='0,0,0',
                  natural_camera_forward='0,0,1')
    fields.update(changes)
    return fields


class MountedAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'evidence'
        self.root.mkdir()

    def evidence(self, lines, ended=True):
        build = json.dumps(dict(manifestId=BUILD)).encode()
        (self.root / 'build-manifest.json').write_bytes(build)
        data = {'run-manifest.json': json.dumps(dict(runId=RUN, buildManifestId=BUILD,
                  buildManifestSha256=hashlib.sha256(build).hexdigest(),
                  deployment=[dict(destination='d3d9.dll', sha256='b'*64)])).encode(),
                'runtime/cojvr.log': ('\n'.join([f'run_start: run_id={RUN} build_manifest_id={BUILD}',
                     *lines, *([f'run_end: run_id={RUN}'] if ended else [])])+'\n').encode()}
        inventory = []
        for name, value in data.items():
            target = self.root / name
            target.parent.mkdir(exist_ok=True)
            target.write_bytes(value)
            inventory.append(dict(path=name, size=len(value), sha256=hashlib.sha256(value).hexdigest()))
        manifest = dict(schemaVersion=1, manifestType='cojvr-run-evidence', runId=RUN,
                        buildManifestId=BUILD, runtimeStarted=True, runtimeEnded=ended,
                        incomplete=not ended, deploymentChecks=[dict(destination='d3d9.dll',
                        expectedSha256='b'*64, actualSha256='b'*64, matches=True)], files=inventory)
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
        return analyze_evidence(self.root)

    def test_valid_run_relates_two_cached_samples_without_causal_claim(self):
        report = self.evidence([mounted(), mounted(frame_sequence=15, pose_sequence=16,
            monotonic_us=1120000, input_time_us=1110000, record_index=2,
            raw_contextual_shoulder_held='true', mapped_run='true',
            native_run='true', native_speed_state=2, native_cached_velocity_cm_s='0,0,310')])
        self.assertTrue(report['provenance']['verified'])
        self.assertTrue(report['provenance']['runtime_complete'])
        self.assertFalse(report['headset_validated'])
        self.assertEqual(report['summary']['valid_samples'], 2)
        self.assertEqual(report['summary']['comparable_pairs'], 1)
        self.assertEqual(len(report['input_transitions']), 1)
        change = report['input_transitions'][0]
        self.assertEqual(change['kind'], 'r1_or_run_changed')
        self.assertEqual(change['cached_horse_speed_before_cm_s'], 250)
        self.assertEqual(change['cached_horse_speed_after_cm_s'], 310)
        self.assertFalse(change['same_frame_response_proven'])
        self.assertEqual(report['summary']['spatial_pairs'], 0)  # Legacy log.
        json.dumps(report, allow_nan=False)

    def test_spatial_endpoints_measure_displacement_and_natural_camera_yaw(self):
        report = self.evidence([mounted(**spatial()),
            mounted(frame_sequence=15, monotonic_us=1200000, input_time_us=1190000,
                    record_index=2, native_side_request=0.5,
                    **spatial(rider_world_position_cm='30,0,40',
                              natural_camera_position_cm='3,0,4',
                              natural_camera_forward='1,0,0'))])
        self.assertEqual(report['schemaVersion'], 2)
        self.assertEqual(report['summary']['spatial_samples'], 2)
        self.assertEqual(report['summary']['spatial_pairs'], 1)
        sample = report['spatial_pairs'][0]
        self.assertEqual(sample['rider_horizontal_chord_speed_cm_s'], 250)
        self.assertEqual(sample['natural_camera_horizontal_chord_speed_cm_s'], 25)
        self.assertEqual(sample['natural_camera_signed_yaw_delta_degrees'], -90)
        self.assertEqual(sample['native_side_request_after'], 0.5)
        self.assertFalse(sample['same_frame_response_proven'])
        json.dumps(report, allow_nan=False)

    def test_spatial_missing_endpoint_or_mount_change_never_creates_pair(self):
        report = self.evidence([mounted(**spatial()),
            mounted(frame_sequence=15, monotonic_us=1200000, record_index=2,
                    **dict(spatial_observation_version=1, spatial_valid='false')),
            mounted(frame_sequence=18, monotonic_us=1400000, record_index=3,
                    horse_id=12, **spatial(rider_world_position_cm='25,0,0'))])
        self.assertEqual(report['summary']['valid_samples'], 3)
        self.assertEqual(report['summary']['spatial_samples'], 2)
        self.assertEqual(report['summary']['spatial_pairs'], 0)

    def test_spatial_schema_rejects_partial_or_nonfinite_samples(self):
        invalid = [mounted(**spatial(spatial_observation_version=2)),
                   mounted(**spatial(spatial_valid='false')),
                   mounted(**spatial(rider_world_position_cm='nan,0,0')),
                   mounted(**spatial(natural_camera_forward='0,0,0')),
                   mounted(spatial_observation_version=1),
                   mounted(spatial_valid='true'),
                   mounted('unavailable', sample_valid='false', **spatial())]
        report = self.evidence(invalid)
        self.assertEqual(report['summary']['valid_samples'], 0)
        self.assertEqual(report['summary']['rejected_samples'], len(invalid))

    def test_unavailable_read_is_counted_and_breaks_comparability(self):
        lost = mounted('unavailable', sample_valid='false')
        # Runtime omits native-only fields when observation fails.
        lost = lost.split(';player_generation=', 1)[0] + ';record_index=2;record_limit=2048;budget_exhausted=false;source=post_input_pre_simulation_cached_fields;read_only=true;same_frame_response_proven=false'
        report = self.evidence([mounted(), lost, mounted(frame_sequence=30, monotonic_us=1240000,
                                record_index=3, mapped_run='true')])
        self.assertEqual(report['summary']['unavailable_samples'], 1)
        self.assertEqual(report['summary']['comparable_pairs'], 0)
        self.assertEqual(report['input_transitions'], [])

    def test_bad_fields_and_duplicate_are_reported_without_interpretation(self):
        bad = [mounted()+';mapped_run=false', mounted(frame_sequence='nan'),
               mounted(native_cached_velocity_cm_s='nan,0,1'),
               mounted(read_only='false'), mounted(record_index=2049),
               mounted('observed', sample_valid='false'),
               mounted('unavailable', sample_valid='true'),
               mounted(mapped_move='0,inf'), mounted(head_relative_move_yaw_degrees='inf')]
        report = self.evidence(bad)
        self.assertEqual(report['summary']['valid_samples'], 0)
        self.assertEqual(report['summary']['rejected_samples'], len(bad))
        self.assertEqual(report['summary']['comparable_pairs'], 0)
        self.assertEqual(len(report['issues']), len(bad))

    def test_owner_and_clock_gaps_do_not_create_false_pairs(self):
        rows = [mounted(), mounted(frame_sequence=12, monotonic_us=1100000, record_index=2,
                horse_id=22, mapped_run='true'), mounted(frame_sequence=13,
                monotonic_us=1090000, record_index=3), mounted(frame_sequence=14,
                monotonic_us=1900000, record_index=4)]
        report = self.evidence(rows)
        self.assertEqual(report['summary']['comparable_pairs'], 0)
        self.assertTrue(any(i['kind'] == 'clock_regression' for i in report['issues']))
        self.assertEqual(report['input_transitions'], [])

    def test_no_samples_or_incomplete_run_is_not_headset_proof(self):
        report = self.evidence([], ended=False)
        self.assertEqual(report['summary']['valid_samples'], 0)
        self.assertFalse(report['provenance']['runtime_complete'])
        self.assertFalse(report['headset_validated'])

    def test_native_reins_sign_is_a_separate_transition_from_run(self):
        report = self.evidence([mounted(native_side_request=-0.25),
            mounted(frame_sequence=15, pose_sequence=16, monotonic_us=1140000,
                    input_time_us=1120000, record_index=2, native_side_request=0.5,
                    native_turn_degrees_s=30,
                    native_cached_velocity_cm_s='0,0,250')])
        self.assertEqual(report['summary']['comparable_pairs'], 1)
        self.assertEqual([t['kind'] for t in report['input_transitions']],
                         ['native_reins_sign_changed'])
        self.assertFalse(report['input_transitions'][0]['same_frame_response_proven'])

    def test_record_reset_and_unavailable_input_break_pairs(self):
        report = self.evidence([mounted(), mounted(frame_sequence=15,
            monotonic_us=1120000, record_index=1, mapped_run='true'),
            mounted(frame_sequence=16, monotonic_us=1230000, record_index=2,
                    raw_turn_available='false', mapped_run='false')])
        self.assertEqual(report['summary']['comparable_pairs'], 0)
        self.assertEqual(report['input_transitions'], [])
        self.assertEqual([i['kind'] for i in report['issues']],
                         ['record_index_gap', 'unavailable_input'])

    def test_mapped_digital_availability_is_uint32_mask_and_r1_must_be_present(self):
        report = self.evidence([mounted(), mounted(frame_sequence=15,
            monotonic_us=1120000, record_index=2,
            mapped_digital_available=(4160756335 & ~(1 << 9)), mapped_run='true')])
        self.assertEqual(report['summary']['valid_samples'], 2)
        self.assertEqual(report['summary']['comparable_pairs'], 0)
        self.assertEqual([issue['kind'] for issue in report['issues']],
                         ['unavailable_shoulder_binding'])
        invalid = self.evidence([mounted(mapped_digital_available='true'),
                                 mounted(mapped_digital_available=2**32),
                                 mounted(mapped_digital_available=-1)])
        self.assertEqual(invalid['summary']['rejected_samples'], 3)

    def test_invalid_native_identity_and_output_contract_are_rejected(self):
        report = self.evidence([
            mounted(player_id=0), mounted(horse_id=0xFFFFFFFF),
            mounted(player_id=12, horse_id=12), mounted(player_generation=0),
            mounted(native_speed_state=2**35), mounted(source='other'),
            mounted(same_frame_response_proven='true'), mounted(budget_exhausted='true'),
            mounted(raw_move_available='1'), mounted(native_context=4),
            mounted()+';unrecognized=5',
            'camera_probe_event: event=mounted_native_motion result=observed'
        ])
        self.assertEqual(report['summary']['valid_samples'], 0)
        self.assertEqual(report['summary']['rejected_samples'], 12)
        self.assertEqual(len(report['issues']), 12)

    def test_inventory_tampering_and_foreign_run_are_rejected(self):
        self.evidence([mounted()])
        with (self.root / 'runtime/cojvr.log').open('a') as stream:
            stream.write('tampered\n')
        with self.assertRaises(ValueError):
            analyze_evidence(self.root)
        with self.assertRaises(ValueError):
            self.evidence([f'run_start: run_id=evil build_manifest_id={BUILD}', mounted()])

    def test_cli_refuses_output_inside_evidence_or_existing_file(self):
        self.evidence([mounted()])
        tool = ROOT / 'tools/analyze_coj_mounted_motion.py'
        for output in (self.root / 'analysis.json', self.root / 'evidence-manifest.json'):
            result = subprocess.run([sys.executable, str(tool), str(self.root), '--output',
                                    str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
        output = self.root.parent / 'analysis.json'
        result = subprocess.run([sys.executable, str(tool), str(self.root), '--output',
                                str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(json.loads(output.read_text())['headset_validated'])


if __name__ == '__main__':
    unittest.main()
