"""Synthetic, inventoried-run regression tests for dynamite observation correlation."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import analyze_coj_dynamite as analysis


RUN = 'dynamite-fixture'
BUILD = 'c' * 64
TOOL = Path(__file__).resolve().parents[1] / 'tools/analyze_coj_dynamite.py'


def event(name, status, **fields):
    return f'camera_probe_event: event={name} result={status} detail=' + ';'.join(
        f'{key}={value}' for key, value in fields.items())


def gesture(frame=100, status='requested', delivered=1, **extra):
    fields = dict(frame_sequence=frame, pose_sequence=frame + 10, owner_epoch=5,
                  native_action=10, release_delivered=delivered)
    fields.update(extra)
    return event('dynamite_motion', status, **fields)


def begin(frame=102, serial=1, generation=7, weapon=42, **extra):
    fields = dict(observed_frame=frame, player_generation=generation, weapon_id=weapon,
                  serial=serial, fuse_seconds=5.0, spark_handle=0, smoke_handle=0)
    fields.update(extra)
    return event('dynamite_lifecycle', 'attack_begin', **fields)


def release(frame=180, serial=1, generation=7, weapon=42, **extra):
    fields = dict(observed_frame=frame, player_generation=generation, weapon_id=weapon,
                  serial=serial, origin_cm='(1,2,3)', requested_velocity_cm_s='(10,30,40)')
    fields.update(extra)
    return event('dynamite_lifecycle', 'native_release', **fields)


class DynamiteAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'evidence'
        self.root.mkdir()

    def evidence(self, lines, ended=True):
        build_data = json.dumps(dict(manifestId=BUILD)).encode()
        (self.root / 'build-manifest.json').write_bytes(build_data)
        files = {
            'run-manifest.json': json.dumps(dict(runId=RUN, buildManifestId=BUILD,
                buildManifestSha256=hashlib.sha256(build_data).hexdigest(),
                deployment=[dict(destination='d3d9.dll', sha256='d'*64)])).encode(),
            'runtime/cojvr.log': ('\n'.join([
                f'run_start: run_id={RUN} build_manifest_id={BUILD} pid=1732', *lines,
                *([f'run_end: run_id={RUN}'] if ended else [])]) + '\n').encode(),
        }
        inventory = []
        for name, contents in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(contents)
            inventory.append(dict(path=name, size=len(contents),
                                  sha256=hashlib.sha256(contents).hexdigest()))
        manifest = dict(schemaVersion=1, manifestType='cojvr-run-evidence', runId=RUN,
                        buildManifestId=BUILD, runtimeStarted=True, runtimeEnded=ended,
                        incomplete=not ended, deploymentChecks=[dict(destination='d3d9.dll',
                            expectedSha256='d'*64, actualSha256='d'*64, matches=True)],
                        files=inventory)
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
        return analysis.analyze_evidence(self.root)

    def test_one_motion_and_matching_native_receipts_form_observational_candidate(self):
        report = self.evidence([gesture(), begin(), release()])
        self.assertEqual(report['provenance']['processId'], 1732)
        self.assertTrue(report['provenance']['runtime_complete'])
        self.assertEqual(report['motions'][0]['assessment'], 'candidate_correlated')
        self.assertEqual(report['native_pairs'][0]['assessment'], 'same_owner_bounded')
        self.assertEqual(report['native_pairs'][0]['weapon_id'], 42)
        self.assertEqual(report['native_pairs'][0]['player_generation'], 7)
        self.assertEqual(report['native_pairs'][0]['gesture_line'], report['motions'][0]['line'])
        self.assertEqual(report['summary']['candidate_correlations'], 1)
        self.assertFalse(report['headset_validated'])

    def test_explicit_matching_weapon_and_generation_are_correlated(self):
        report = self.evidence([gesture(weapon_id=42, player_generation=7), begin(), release()])
        self.assertEqual(report['motions'][0]['assessment'], 'candidate_correlated')
        self.assertEqual(report['native_pairs'][0]['identity_relation'], 'matched')
        self.assertEqual(report['summary']['same_weapon_candidates'], 1)

    def test_mismatched_native_weapon_or_player_generation_withholds_attribution(self):
        for identity in (dict(weapon_id=43, player_generation=7),
                         dict(weapon_id=42, player_generation=8)):
            with self.subTest(identity=identity):
                report = self.evidence([gesture(**identity), begin(), release()])
                self.assertEqual(report['motions'][0]['assessment'], 'owner_mismatch')
                self.assertIsNone(report['native_pairs'][0]['gesture_line'])
                self.assertEqual(report['native_pairs'][0]['identity_relation'], 'mismatch')
                self.assertEqual(report['summary']['same_weapon_candidates'], 0)

    def test_high_bit_native_weapon_identity_retains_all_32_bits(self):
        high_id = 0xf0000001
        report = self.evidence([gesture(weapon_id=high_id, player_generation=7),
                                begin(weapon=high_id), release(weapon=high_id)])
        self.assertEqual(report['motions'][0]['weapon_id'], high_id)
        self.assertEqual(report['native_pairs'][0]['weapon_id'], high_id)
        self.assertEqual(report['summary']['same_weapon_candidates'], 1)

    def test_partial_or_invalid_gesture_identity_is_rejected(self):
        for identity in (dict(weapon_id=42), dict(player_generation=7),
                         dict(weapon_id=0, player_generation=7),
                         dict(weapon_id=0xffffffff, player_generation=7),
                         dict(weapon_id=0x100000000, player_generation=7),
                         dict(weapon_id=42, player_generation=0)):
            with self.subTest(identity=identity):
                report = self.evidence([gesture(**identity), begin(), release()])
                self.assertEqual(report['motions'][0]['assessment'], 'invalid')
                self.assertIsNone(report['native_pairs'][0]['gesture_line'])
                self.assertEqual(report['summary']['same_weapon_candidates'], 0)

    def test_legacy_gestures_remain_readable_without_identity_claim(self):
        report = self.evidence([gesture(), begin(), release()])
        self.assertEqual(report['motions'][0]['assessment'], 'candidate_correlated')
        self.assertEqual(report['native_pairs'][0]['identity_relation'], 'legacy_unverified')
        self.assertEqual(report['summary']['same_weapon_candidates'], 0)

    def test_native_trigger_without_gesture_is_unattributed(self):
        report = self.evidence([begin(), release()])
        self.assertEqual(len(report['native_pairs']), 1)
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])
        self.assertEqual(report['summary']['candidate_correlations'], 0)

    def test_withheld_and_incomplete_jni_release_cannot_claim_native_causality(self):
        report = self.evidence([gesture(90, status='withheld', delivered=0),
                                gesture(100, delivered=0), begin(), release()])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['withheld', 'dispatch_incomplete'])
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])

    def test_two_eligible_gestures_make_native_request_ambiguous(self):
        report = self.evidence([gesture(100), gesture(101), begin(102), release(190)])
        self.assertEqual([row['assessment'] for row in report['motions']], ['ambiguous', 'ambiguous'])
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])

    def test_withheld_gesture_in_same_candidate_window_blocks_attribution(self):
        report = self.evidence([gesture(100, status='withheld', delivered=0),
                                gesture(101), begin(102), release(180)])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['withheld', 'ambiguous'])
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])

    def test_intervening_invalid_motion_blocks_attribution(self):
        report = self.evidence([gesture(100), gesture(101, delivered=2),
                                begin(102), release(180)])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['ambiguous', 'invalid'])
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])

    def test_interruption_between_native_begin_and_release_blocks_gesture_attribution(self):
        report = self.evidence([gesture(100), begin(101), gesture(110, status='withheld', delivered=0),
                                release(180)])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['ambiguous', 'withheld'])
        self.assertEqual(len(report['native_pairs']), 1)
        self.assertIsNone(report['native_pairs'][0]['gesture_line'])

    def test_two_separate_unique_throws_remain_independent(self):
        report = self.evidence([gesture(100), begin(101, serial=1), release(180, serial=1),
                                gesture(190), begin(191, serial=2), release(260, serial=2)])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['candidate_correlated', 'candidate_correlated'])
        self.assertEqual([pair['gesture_line'] for pair in report['native_pairs']],
                         [row['line'] for row in report['motions']])

    def test_missing_native_receipts_cannot_claim_unique_throw(self):
        for receipts in ([begin(101, serial=3), release(180)],
                         [begin(101), release(180, serial=3)]):
            with self.subTest(receipts=receipts):
                report = self.evidence([gesture(100), *receipts])
                self.assertEqual(report['summary']['candidate_correlations'], 0)
                self.assertEqual(report['native_pairs'], [])
                self.assertIn('serial_gap', {issue['kind'] for issue in report['issues']})

    def test_serial_gap_after_verified_sequence_does_not_invalidate_prior_throw(self):
        report = self.evidence([gesture(100), begin(101), release(150),
                                gesture(160), begin(161, serial=3), release(200, serial=3)])
        self.assertEqual(report['summary']['candidate_correlations'], 1)
        self.assertEqual(len(report['native_pairs']), 1)
        self.assertEqual(report['native_pairs'][0]['begin_serial'], 1)
        self.assertIn('serial_gap', {issue['kind'] for issue in report['issues']})

    def test_completed_native_throw_separates_close_successive_gestures(self):
        report = self.evidence([gesture(100), begin(101, serial=1), release(103, serial=1),
                                gesture(104), begin(105, serial=2), release(106, serial=2)])
        self.assertEqual([row['assessment'] for row in report['motions']],
                         ['candidate_correlated', 'candidate_correlated'])
        self.assertEqual([pair['gesture_line'] for pair in report['native_pairs']],
                         [row['line'] for row in report['motions']])

    def test_same_frame_native_snapshots_do_not_establish_ordered_throw(self):
        report = self.evidence([gesture(100), begin(101), release(101)])
        self.assertEqual(report['native_pairs'], [])
        self.assertEqual(report['summary']['candidate_correlations'], 0)

    def test_multiple_native_begins_do_not_pair_with_one_release(self):
        report = self.evidence([gesture(), begin(102, serial=1), begin(110, serial=2), release(170)])
        self.assertEqual(report['native_pairs'], [])
        self.assertEqual(report['motions'][0]['assessment'], 'ambiguous')
        self.assertEqual(len(report['native_unpaired']), 3)

    def test_native_owner_changes_and_serial_collision_do_not_pair(self):
        report = self.evidence([gesture(), begin(), release(generation=8),
                                begin(210, serial=1), release(230, serial=1)])
        self.assertEqual(report['native_pairs'], [])
        self.assertIn('repeated_serial', {x['kind'] for x in report['issues']})

    def test_reordered_or_distant_events_cannot_pair(self):
        report = self.evidence([release(90), gesture(100), begin(105), release(1100)])
        self.assertEqual(report['native_pairs'], [])
        self.assertEqual(report['summary']['candidate_correlations'], 0)

    def test_invalid_fields_and_duplicate_detail_are_explicit_issues(self):
        report = self.evidence([gesture(), begin(fuse_seconds='nan'), release(),
                                begin(201, serial=2) + ';weapon_id=42'])
        self.assertEqual(report['native_pairs'], [])
        self.assertIn('invalid_observation', {x['kind'] for x in report['issues']})
        self.assertEqual(report['summary']['candidate_correlations'], 0)

    def test_early_run_end_keeps_native_observation_inconclusive(self):
        report = self.evidence([gesture(), begin(), release()], ended=False)
        self.assertEqual(report['motions'][0]['assessment'], 'run_incomplete')
        self.assertEqual(report['summary']['candidate_correlations'], 0)

    def test_tampered_inventory_and_foreign_process_marker_are_rejected(self):
        self.evidence([gesture(), begin(), release()])
        log = self.root / 'runtime/cojvr.log'
        log.write_text(log.read_text() + 'tampered')
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)
        self.evidence([gesture(), begin(), release()])
        log = self.root / 'runtime/cojvr.log'
        new_contents = log.read_text().replace('pid=1732', 'pid=0').encode()
        log.write_bytes(new_contents)
        manifest = json.loads((self.root / 'evidence-manifest.json').read_text())
        for item in manifest['files']:
            if item['path'] == 'runtime/cojvr.log':
                item.update(size=len(new_contents), sha256=hashlib.sha256(new_contents).hexdigest())
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_cli_outputs_json_outside_evidence_without_overwriting(self):
        self.evidence([gesture(), begin(), release()])
        output = self.root.parent / 'dynamite-report.json'
        command = [sys.executable, '-B', str(TOOL), str(self.root), '--output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(output.read_text())['summary']['candidate_correlations'], 1)
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)


if __name__ == '__main__':
    unittest.main()
