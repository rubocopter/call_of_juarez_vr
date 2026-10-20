"""Offline evidence analysis: synthetic logs do not accept physical VR gates."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/analyze_coj_manual_reload.py'
if TOOL.exists():
    spec = importlib.util.spec_from_file_location('reload_analysis', TOOL)
    analysis = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(analysis)
else:
    analysis = None

RUN = 'synthetic-run'
BUILD = 'a' * 64


def event(name, result='observed', **fields):
    return f'camera_probe_event: event={name} result={result} detail=' + ';'.join(
        f'{key}={value}' for key, value in fields.items())


def trace(frame, status=2, **extra):
    fields = dict(frame_sequence=frame, observation_valid=1, context_allowed=1,
                  player_generation=7, native_context=5, weapon_id=42, armed_hand=0,
                  wait_probe_observed=1, wait_probe_status_after=status,
                  manual_session_enabled=1, ammo_readback_before=2, ammo_readback_after=2,
                  pistol_reserve_before=10, pistol_reserve_after=10, same_owner_delta=1)
    fields.update(extra)
    return event('reload_trace', **fields)


def insertion(frame=3, result='accepted', **extra):
    fields = dict(frame_sequence=frame, weapon_id=42, ammo_before=2, ammo_after=3,
                  reserve_before=10, reserve_after=9, after_valid=1,
                  native_call_completed=1, retry='false', transfer_owner='PawnArmed.WeaponReload')
    fields.update(extra)
    return event('manual_reload_insertion', result, **fields)


def depth_sample(frame=2, eye='left', **extra):
    matrix = ','.join('1' if index in (0, 5, 10, 15) else '0' for index in range(16))
    fields = dict(frame_sequence=frame, eye=eye, generation=3, device='0x1234',
                  target='0x1000', depth='0x2000', target_valid=1, depth_valid=1,
                  state_valid=1, layout_matches=1, transforms_valid=1,
                  target_width=100, target_height=80, depth_width=100, depth_height=80,
                  target_msaa=0, depth_msaa=0, target_quality=0, depth_quality=0,
                  viewport='0,0,100,80', min_z=0, max_z=1,
                  z_enable=1, z_write=1, z_func=4,
                  view=matrix, projection=matrix,
                  phase='full_eye_complete', read_only='true',
                  depth_content='unverified', occlusion_admission='false')
    fields.update(extra)
    return 'manual_reload_depth: ' + ';'.join(f'{key}={value}' for key, value in fields.items())


class ReloadAnalysisTests(unittest.TestCase):
    def test_bow_requests_counters_and_cancellations_are_not_arrow_spawns(self):
        report = self.evidence([
            event('physical_bow_draw', 'drawing', frame_sequence=2, weapon_id=42,
                  reason='none', gesture_cancelled=0, dispatch_cancelled=0,
                  native_attack_entries=9, counter_valid=1, arrow_spawn_confirmed='false'),
            event('physical_bow_draw', 'shot_requested', frame_sequence=3, weapon_id=42,
                  reason='none', native_attack_entries=9, counter_valid=1),
            event('physical_bow_draw', 'idle', frame_sequence=4, weapon_id=42,
                  reason='pose_stale', gesture_cancelled=1, dispatch_cancelled=0,
                  native_attack_entries=10, counter_valid=1),
            event('physical_bow_reset', 'cancelled', frame_sequence=5,
                  owner_token=1, reason='gameplay_input_lost'),
            event('physical_bow_draw', 'unavailable', frame_sequence=6, weapon_id=0,
                  reason='owner_changed', dispatch_cancelled=1, counter_valid=0,
                  native_attack_entries=999),
            # A later counter baseline must not become a count of arrow spawns.
            event('physical_bow_draw', 'idle', frame_sequence=7, weapon_id=43,
                  reason='none', counter_valid=1, native_attack_entries=2),
        ])
        bow = report['bow_draw_observations']
        self.assertTrue(bow['observed'])
        self.assertEqual(bow['draw_samples'], 5)
        self.assertEqual(bow['shot_requests'], 1)
        self.assertEqual(bow['native_attack_counter_max'], 10)
        self.assertEqual(bow['reset_reasons'], {'gameplay_input_lost': 1})
        self.assertEqual(bow['gesture_cancel_reasons'], {'pose_stale': 1})
        self.assertEqual(bow['dispatch_cancel_reasons'], {'owner_changed': 1})
        self.assertIsNone(bow['arrow_spawns_confirmed'])
        self.assertFalse(bow['headset_validated'])
        self.assertFalse(report['headset_validated'])
        self.assertEqual(report['summary']['bow_shot_requests'], 1)
        self.assertEqual(report['summary']['bow_resets'], 1)

    def test_absent_bow_or_counter_is_unobserved_and_legacy_remains_readable(self):
        bow = self.evidence([])['bow_draw_observations']
        self.assertFalse(bow['observed'])
        self.assertEqual(bow['shot_requests'], 0)
        self.assertIsNone(bow['native_attack_counter_max'])
        self.assertIsNone(bow['arrow_spawns_confirmed'])
        legacy = self.evidence([event('physical_bow_draw', 'drawing',
                                     frame_sequence=2, reason='none')])
        self.assertTrue(legacy['bow_draw_observations']['observed'])
        self.assertIsNone(legacy['bow_draw_observations']['native_attack_counter_max'])

    def test_bow_rejects_contradictory_or_malformed_observations(self):
        for change in ({'arrow_spawn_confirmed': 'true'},
                       {'counter_valid': 1},
                       {'counter_valid': 1, 'native_attack_entries': -1},
                       {'gesture_cancelled': 'perhaps'},
                       {'dispatch_cancelled': '2'},
                       {'frame_sequence': 0}, {'frame_sequence': 'nan'}):
            fields = dict(frame_sequence=2, reason='none')
            fields.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.evidence([event('physical_bow_draw', 'idle', **fields)])
        with self.assertRaises(ValueError):
            self.evidence([event('physical_bow_reset', 'cancelled', frame_sequence=2)])

    def test_bow_ignores_unrelated_detail_and_outside_run_records(self):
        report = self.evidence([event('unrelated_error', detail=
            'event=physical_bow_draw result=shot_requested detail=frame_sequence=2')])
        self.assertFalse(report['bow_draw_observations']['observed'])
        # read_events must not allow event-like error text or prior runs to add counts.
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'cojvr.log'
            bow = event('physical_bow_draw', 'shot_requested', frame_sequence=2, reason='none')
            path.write_text('\n'.join([bow, f'run_start: run_id={RUN} build_manifest_id={BUILD}',
                                       f'run_end: run_id={RUN}', bow]), encoding='utf-8')
            events = analysis.read_events(path, dict(runId=RUN, buildManifestId=BUILD,
                runtimeStarted=True, runtimeEnded=True, incomplete=False))
        self.assertEqual(analysis.bow_draw_observations(events)['shot_requests'], 0)

    def test_rifle_reference_reports_sparse_motion_without_attachment_claim(self):
        def sample(t, p, phase='ordinary_idle_tuple', **extra):
            fields=dict(schema=1,player_generation=7,native_context=5,player_id=11,
                weapon_id=42,weapon_class='WeaponRifleWinchester',sample_us=t,
                sample_gap_us=0 if t==100000 else 50000,sample_index=1 if t==100000 else 2,
                frame_sequence=t//1000,source='natural_pre_body',units='cm',read_only='true',
                contact_admission='false',attachment_admission='false',cycle_qualified='false',
                states_right='1,1,1,1' if phase=='ordinary_idle_tuple' else '30,30,30,30',
                states_left='0,0,0,0',two_handed_right='true,true,true',
                two_handed_left='false,false,false',phase=phase)
            for name in ('root','holder','support','support_in_weapon'):
                fields.update({name:p if name=='support_in_weapon' else '(0,0,0)',
                               name+'_up':'(0,1,0)',name+'_forward':'(0,0,1)'})
            fields.update(extra)
            return event('rifle_support_reference',**fields)
        report=self.evidence([sample(100000,'(1,2,3)'),
                              sample(150000,'(4,6,3)','ordinary_fire_tuple')])
        rows=report['rifle_support_observations']
        self.assertEqual(len(rows),2)
        self.assertEqual(report['summary']['rifle_support_samples'],2)
        self.assertEqual(rows[1]['support_displacement_from_first_cm'],5)
        self.assertEqual(rows[1]['observed_gap_us'],50000)
        self.assertFalse(rows[1]['attachment_admitted'])
        self.assertFalse(rows[1]['cycle_qualified'])
        for change in ({'attachment_admission':'true'}, {'cycle_qualified':'true'},
                       {'support_in_weapon':'(nan,0,0)'}, {'states_right':'129,1,1,1'},
                       {'support_up':'(0,2,0)'}, {'two_handed_left':'true,false,false'},
                       {'weapon_class':'WeaponRifleWinchesterScope'},
                       {'sample_gap_us':1}, {'sample_us':100000}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.evidence([sample(100000,'(1,2,3)'),
                               sample(150000,'(4,6,3)','ordinary_fire_tuple',**change)])
        report=self.evidence([sample(100000,'(1,2,3)'),
                              sample(150000,'(4,6,3)',weapon_id=43,sample_index=1,sample_gap_us=0)])
        self.assertIsNone(report['rifle_support_observations'][1]['support_displacement_from_first_cm'])
        with self.assertRaises(ValueError):
            self.evidence([sample(150000,'(4,6,3)')])

    def test_native_drum_phase_is_optional_bracketed_evidence_only(self):
        self.assertIsNone(analysis.native_drum_phase({}))
        self.assertEqual(analysis.native_drum_phase({'native_drum_phase_deg':'300.5',
                         'native_phase_stable':'1'}),300.5)
        for fields in ({'native_drum_phase_deg':'60'}, {'native_phase_stable':'1'},
                       {'native_drum_phase_deg':'nan','native_phase_stable':'1'},
                       {'native_drum_phase_deg':'60','native_phase_stable':'0'}):
            with self.assertRaises(ValueError): analysis.native_drum_phase(fields)

    def test_depth_pair_requires_ordered_same_frame_device_and_generation(self):
        report = self.evidence([depth_sample(), depth_sample(eye='right')])
        pair = report['depth_eye_pairs'][0]
        self.assertEqual(pair['status'], 'metadata_correlated')
        self.assertTrue(pair['layout_both'])
        self.assertEqual(report['summary']['depth_correlated_pairs'], 1)
        self.assertEqual(report['summary']['depth_incomplete_pairs'], 0)
        self.assertEqual(report['summary']['depth_rejected_pairs'], 0)
        self.assertFalse(pair['occlusion_admitted'])
        self.assertFalse(report['headset_validated'])

    def test_depth_pair_incomplete_and_invalid_samples_fail_closed(self):
        variants = (
            ([depth_sample()], 'incomplete'),
            ([depth_sample(eye='right'), depth_sample()], 'rejected'),
            ([depth_sample(), depth_sample(eye='left'), depth_sample(eye='right')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', generation=4)], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', device='0x9999')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', phase='wrong')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', projection='nan')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', depth_content='verified')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', occlusion_admission='true')], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', depth_valid=0)], 'rejected'),
            ([depth_sample(), depth_sample(eye='right', layout_matches=0)], 'rejected'),
        )
        for lines, status in variants:
            with self.subTest(status=status, lines=lines):
                report = self.evidence(lines)
                self.assertEqual(report['depth_eye_pairs'][0]['status'], status)
                self.assertEqual(report['summary']['depth_correlated_pairs'], 0)
                self.assertEqual(report['summary']['depth_incomplete_pairs'], int(status == 'incomplete'))
                self.assertEqual(report['summary']['depth_rejected_pairs'], int(status == 'rejected'))
                self.assertFalse(report['depth_eye_pairs'][0]['occlusion_admitted'])

    def test_depth_pair_cannot_join_samples_from_different_frames(self):
        report = self.evidence([depth_sample(frame=2), depth_sample(frame=3, eye='right')])
        self.assertEqual(report['summary']['depth_correlated_pairs'], 0)
        self.assertEqual(report['summary']['depth_incomplete_pairs'], 2)
        self.assertEqual([p['status'] for p in report['depth_eye_pairs']],
                         ['incomplete', 'incomplete'])

    def test_batched_grip_zone_haptics_and_depth_are_observations_not_acceptance(self):
        report=self.evidence([
            event('manual_reload_zone','entered',frame_sequence=2,hand=1,release_qualified=1),
            event('manual_reload_cartridge','visible',frame_sequence=2,hand=1,axis='projected_distal_mean'),
            'openvr_reload_haptic: event=zone_entered;status=submitted;frame_sequence=2;hand=left',
            'openvr_reload_haptic: event=insert_accepted;status=submitted;frame_sequence=3;hand=left',
            'manual_reload_depth: frame_sequence=2;eye=left;layout_matches=1;depth_valid=1;'
            'phase=full_eye_complete;read_only=true;depth_content=unverified;occlusion_admission=false',
        ])
        self.assertEqual(report['summary']['zone_entries'],1)
        self.assertEqual(report['summary']['cartridge_visible_events'],1)
        self.assertEqual(report['summary']['zone_haptics_submitted'],1)
        self.assertEqual(report['summary']['insertion_haptics_submitted'],1)
        self.assertEqual(report['summary']['depth_layout_samples'],1)
        self.assertFalse(report['renderer_depth_observations'][0]['occlusion_admitted'])
        self.assertFalse(report['headset_validated'])
        self.assertEqual(report['summary']['accepted_insertions'],0)

    def test_diagnostic_text_in_unrelated_event_is_not_haptic_evidence(self):
        report=self.evidence([event('unrelated_error',detail=
            'openvr_reload_haptic: event=zone_entered;status=submitted;frame_sequence=2')])
        self.assertEqual(report['summary']['zone_haptics_submitted'],0)
        self.assertEqual(report['timeline'],[])

    def test_pre_native_gate_mechanics_require_root_frame_and_provenance(self):
        def observed(frame, source, root='0,0,0', gate='0,2,0',
                     gate_up='0,1,0', drum_up='0,1,0'):
            return event('manual_reload_geometry', frame_sequence=frame, hand=0,
                         manual_session=1, model='peacemaker', source=source,
                         root=root, root_up='0,1,0', root_forward='0,0,1',
                         barrel='0,0,1', barrel_up='0,1,0', barrel_forward='0,0,1',
                         drum='0,0,0', drum_up=drum_up, inward='0,0,1',
                         gate=gate, gate_up=gate_up, gate_forward='0,0,1',
                         native_drum_phase_deg=(frame-2)*60,native_phase_stable=1)
        report=self.evidence([trace(2),observed(2,'pre_native'),
                              trace(3),observed(3,'pre_native',gate_up='1,0,0'),
                              trace(4),observed(4,'post_overlay',gate_up='0,1,0')])
        baseline,hinge,overlay=report['mechanical_observations']
        self.assertEqual([r['native_drum_phase_deg'] for r in (baseline,hinge,overlay)],[0,60,120])
        self.assertFalse(any(r['chamber_occupancy_observed'] for r in (baseline,hinge,overlay)))
        self.assertTrue(baseline['native_motion_evidence_eligible'])
        self.assertAlmostEqual(hinge['gate_root_up_cosine_change_from_first'],-1)
        self.assertFalse(overlay['native_motion_evidence_eligible'])
        self.assertNotIn('gate_root_up_cosine_change_from_first',overlay)
        self.assertFalse(any(row['gate_open_confirmed'] for row in (baseline,hinge,overlay)))

    def test_post_overlay_and_legacy_geometry_cannot_establish_native_baseline(self):
        legacy=dict(manual_session=1,frame_sequence=2,hand=0,model='peacemaker',
                    drum='0,0,0',gate='0,2,0',drum_up='0,1,0',inward='0,0,1',
                    gate_up='0,1,0',gate_forward='0,0,1')
        report=self.evidence([trace(2),event('manual_reload_geometry',**legacy),
                              trace(3),event('manual_reload_geometry',source='post_overlay',
                                             **dict(legacy,frame_sequence=3))])
        self.assertFalse(any(r['native_motion_evidence_eligible'] for r in report['mechanical_observations']))

    def test_post_overlay_gate_pivot_rank_is_diagnostic_only(self):
        fields=dict(manual_session=1,frame_sequence=2,hand=0,model='peacemaker',
                    source='post_overlay',root='0,0,0',root_up='0,1,0',root_forward='0,0,1',
                    barrel='0,0,1',barrel_up='0,1,0',barrel_forward='0,0,1',
                    drum='0,0,0',drum_up='0,1,0',inward='0,0,1',
                    gate='1,0,0',gate_up='0,1,0',gate_forward='0,0,1',
                    port_reference='gate_element_origin_unverified',port_owner_qualified=1,
                    port_rank='pivot_only',port_nearest_mouth=3,port_nearest_cm=1,
                    port_runner_up_cm=2,port_separation_cm=1,port_axial_cm=0.5,
                    port_radial_cm='0.866025',gate_open_confirmed='false',socket_admission='false')
        report=self.evidence([trace(2),event('manual_reload_geometry',**fields)])
        row=report['mechanical_observations'][0]
        self.assertEqual(row['port_probe']['status'],'pivot_only')
        self.assertEqual(row['port_probe']['nearest_mouth'],3)
        self.assertEqual(row['port_probe']['reference'],'gate_element_origin_unverified')
        self.assertTrue(row['port_probe']['owner_qualified'])
        self.assertFalse(row['port_probe']['gate_open_confirmed'])
        self.assertFalse(row['port_probe']['socket_admission'])
        self.assertFalse(row['gate_open_confirmed'])
        self.assertFalse(row['native_motion_evidence_eligible'])
        self.assertEqual(report['summary']['port_pivot_rank_samples'],1)
        self.assertFalse(report['headset_validated'])
        self.assertEqual(report['summary']['issue_count'],0)

    def test_malformed_port_ranking_is_rejected_without_clearance_claim(self):
        fields=dict(manual_session=1,frame_sequence=2,hand=0,model='frontier',
                    source='post_overlay',root='0,0,0',root_up='0,1,0',root_forward='0,0,1',
                    barrel='0,0,1',barrel_up='0,1,0',barrel_forward='0,0,1',
                    drum='0,0,0',drum_up='0,1,0',inward='0,0,1',
                    gate='1,0,0',gate_up='0,1,0',gate_forward='0,0,1',
                    port_reference='gate_element_origin_unverified',port_owner_qualified=1,
                    port_rank='pivot_only',port_nearest_mouth=3,port_nearest_cm=1,
                    port_runner_up_cm=2,port_separation_cm=1,port_axial_cm=0.5,
                    port_radial_cm='0.866025',gate_open_confirmed='false',socket_admission='false')
        alterations=(dict(port_nearest_cm='nan'),dict(port_nearest_mouth=6),
                     dict(port_separation_cm=0),dict(port_owner_qualified=0),
                     dict(gate_open_confirmed='true'),dict(socket_admission='true'),
                     dict(port_reference='claimed_port_center'),dict(source='pre_native'),
                     dict(manual_session=0),dict(port_radial_cm=-1),
                     dict(port_runner_up_cm=.5),dict(port_rank='ready_to_insert'))
        for changes in alterations:
            with self.subTest(changes=changes):
                report=self.evidence([trace(2),event('manual_reload_geometry',**dict(fields,**changes))])
                self.assertEqual(report['summary']['port_pivot_rank_samples'],0)
                self.assertTrue(any(i['kind']=='invalid_mechanical_geometry' for i in report['issues']))
                self.assertNotIn('port_probe',report['mechanical_observations'][0])
                self.assertFalse(report['headset_validated'])

    def test_ambiguous_or_unowned_port_never_ranks_accessible_mouth(self):
        base=dict(manual_session=1,frame_sequence=2,hand=0,model='peacemaker',
                  source='post_overlay',root='0,0,0',root_up='0,1,0',root_forward='0,0,1',
                  barrel='0,0,1',barrel_up='0,1,0',barrel_forward='0,0,1',
                  drum='0,0,0',drum_up='0,1,0',inward='0,0,1',
                  gate='1,0,0',gate_up='0,1,0',gate_forward='0,0,1',
                  port_reference='gate_element_origin_unverified',port_owner_qualified=1,
                  port_rank='ambiguous',port_nearest_mouth=-1,port_nearest_cm=1,
                  port_runner_up_cm='1.004',port_separation_cm='0.004',
                  port_axial_cm=0,port_radial_cm=1,
                  gate_open_confirmed='false',socket_admission='false')
        report=self.evidence([trace(2),event('manual_reload_geometry',**base),
                              event('manual_reload_geometry',**dict(base,frame_sequence=3,
                                  port_rank='invalid',port_owner_qualified=0,
                                  port_nearest_cm=0,port_runner_up_cm=0,
                                  port_separation_cm=0,port_axial_cm=0,port_radial_cm=0))])
        self.assertEqual([r['port_probe']['status'] for r in report['mechanical_observations']],
                         ['ambiguous','invalid'])
        self.assertEqual(report['summary']['port_pivot_rank_samples'],0)
        self.assertFalse(any(r['gate_open_confirmed'] for r in report['mechanical_observations']))

    def test_source_qualified_geometry_rejects_scaled_or_parallel_element_axes(self):
        original=dict(manual_session=1,frame_sequence=2,hand=0,model='peacemaker',
                      root='0,0,0',root_up='0,1,0',root_forward='0,0,1',
                      barrel='0,0,1',barrel_up='0,1,0',barrel_forward='0,0,1',
                      drum='0,0,0',drum_up='0,1,0',inward='0,0,1',
                      gate='0,2,0',gate_up='0,1,0',gate_forward='0,0,1')
        for source in ('pre_native','post_overlay'):
            for key,value in (('gate_up','0,2,0'),('gate_forward','0,1,0'),
                              ('drum_up','0,2,0'),('inward','0,1,0')):
                with self.subTest(source=source,key=key):
                    fields=dict(original,source=source)
                    fields[key]=value
                    report=self.evidence([trace(2),event('manual_reload_geometry',**fields)])
                    self.assertTrue(any(i['kind']=='invalid_mechanical_geometry'
                                        for i in report['issues']))
                    self.assertFalse(report['mechanical_observations'][0]['native_motion_evidence_eligible'])

    def setUp(self):
        self.assertIsNotNone(analysis, 'offline manual reload analyzer is not implemented')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'evidence'
        self.root.mkdir()

    def evidence(self, lines, manual=True, ended=True):
        build_data = json.dumps(dict(manifestId=BUILD)).encode()
        (self.root / 'build-manifest.json').write_bytes(build_data)
        files = {'run-manifest.json': json.dumps(dict(runId=RUN, buildManifestId=BUILD,
                  buildManifestSha256=hashlib.sha256(build_data).hexdigest(),
                  deployment=[dict(destination='d3d9.dll', sha256='b'*64)],
                  validation=dict(manualReloadInstalled=manual))).encode(),
                 'runtime/cojvr.log': ('\n'.join([
                     f'run_start: run_id={RUN} build_manifest_id={BUILD} pid=1', *lines,
                     *([f'run_end: run_id={RUN}'] if ended else [])]) + '\n').encode()}
        inventory = []
        for name, data in files.items():
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            inventory.append(dict(path=name, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        manifest = dict(schemaVersion=1, manifestType='cojvr-run-evidence', runId=RUN,
                        buildManifestId=BUILD, runtimeStarted=True, runtimeEnded=ended,
                        incomplete=not ended, deploymentChecks=[dict(destination='d3d9.dll',
                        expectedSha256='b'*64, actualSha256='b'*64, matches=True)], files=inventory)
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
        return analysis.analyze_evidence(self.root)

    def rewrite(self, name, data):
        encoded = data.encode('utf-8')
        (self.root / name).write_bytes(encoded)
        manifest = json.loads((self.root / 'evidence-manifest.json').read_text())
        for item in manifest['files']:
            if item['path'] == name:
                item.update(size=len(encoded), sha256=hashlib.sha256(encoded).hexdigest())
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))

    def test_unit_readback_with_same_owner_does_not_accept_headset(self):
        report = self.evidence([trace(2), insertion(), trace(3, ammo_readback_after=3,
                                    pistol_reserve_after=9)])
        self.assertTrue(report['provenance']['verified'])
        self.assertEqual(report['insertions'][0]['assessment'], 'unit_readback_observed')
        self.assertFalse(report['headset_validated'])

    def test_double_transfer_is_reported(self):
        report = self.evidence([trace(2), insertion(ammo_after=4, reserve_after=8),
                               trace(3, ammo_readback_after=4, pistol_reserve_after=8)])
        self.assertEqual(report['insertions'][0]['assessment'], 'non_unit_readback')

    def test_invalid_after_snapshot_cannot_confirm_any_transfer(self):
        report = self.evidence([trace(2), insertion(after_valid=0), trace(3)])
        self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')

    def test_typed_rifle_reserve_is_correlated_and_unknown_or_mixed_pools_reject(self):
        before=dict(reserve_kind_before='rifle',reserve_kind_after='rifle',reserve_count_before=10,reserve_count_after=10,
                    pistol_reserve_before='n/a',pistol_reserve_after='n/a')
        after=dict(before,ammo_readback_after=3,reserve_count_after=9)
        report=self.evidence([trace(2,**before),insertion(reserve_kind='rifle'),trace(3,**after)])
        self.assertEqual(report['insertions'][0]['assessment'],'unit_readback_observed')
        for change in (dict(reserve_kind_after='pistol'),dict(reserve_kind_after='unknown'),dict(reserve_count_after='n/a')):
            report=self.evidence([trace(2,**before),insertion(reserve_kind='rifle'),trace(3,**dict(after,**change))])
            self.assertEqual(report['insertions'][0]['assessment'],'inconclusive')
        report=self.evidence([trace(2,**before),insertion(reserve_kind='unknown'),trace(3,**after)])
        self.assertEqual(report['insertions'][0]['assessment'],'inconclusive')
        report=self.evidence([trace(2,**before),trace(3,**after)])
        self.assertIn('reload_readback_change_without_insertion',[issue['kind'] for issue in report['issues']])

    def test_after_trace_must_still_observe_the_manual_wait_or_close(self):
        for changed in (dict(manual_session_enabled=0), dict(wait_probe_observed=0),
                        dict(status=0), dict(status=1), dict(status=4)):
            with self.subTest(changed=changed):
                report = self.evidence([trace(2), insertion(), trace(3,
                    ammo_readback_after=3, pistol_reserve_after=9, **changed)])
                self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')
                self.assertFalse(report['insertions'][0]['owner_correlated'])
                self.assertEqual(report['summary']['unit_readbacks'], 0)

    def test_last_round_can_be_confirmed_when_native_helper_closes(self):
        report = self.evidence([trace(2), insertion(), trace(3, status=3,
            ammo_readback_after=3, pistol_reserve_after=9)])
        self.assertEqual(report['insertions'][0]['assessment'], 'unit_readback_observed')
        self.assertEqual(report['summary']['issue_count'], 0)
        self.assertFalse(report['headset_validated'])

    def test_owner_change_is_not_correlated_as_a_unit(self):
        report = self.evidence([trace(2), insertion(), trace(3, player_generation=8)])
        self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')

    def test_trace_counters_must_agree_with_insertion_readbacks(self):
        report = self.evidence([trace(2), insertion(), trace(3, ammo_readback_after=5,
                                                               pistol_reserve_after=7)])
        self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')

    def test_regressing_trace_frame_cannot_correlate_an_insertion(self):
        report = self.evidence([trace(99), insertion(), trace(3, ammo_readback_after=3,
                                                                 pistol_reserve_after=9)])
        self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')

    def test_duplicate_insertion_frame_never_claims_two_proven_units(self):
        report = self.evidence([trace(2), insertion(), insertion(), trace(3, ammo_readback_after=3,
                                                                                pistol_reserve_after=9)])
        self.assertEqual(report['summary']['unit_readbacks'], 0)
        self.assertTrue(any(i['kind'] == 'missing_or_repeated_insertion_frame' for i in report['issues']))

    def test_waiting_transfer_without_insertion_is_reported(self):
        report = self.evidence([trace(2), trace(3, ammo_readback_after=3, pistol_reserve_after=9)])
        self.assertTrue(any(i['kind'] == 'reload_readback_change_without_insertion' for i in report['issues']))

    def test_uncorrelated_accepted_event_cannot_hide_waiting_transfer(self):
        report = self.evidence([trace(2), insertion(weapon_id=99), trace(3, ammo_readback_after=3,
                                                                                  pistol_reserve_after=9)])
        self.assertEqual(report['insertions'][0]['assessment'], 'inconclusive')
        self.assertTrue(any(i['kind'] == 'reload_readback_change_without_insertion' for i in report['issues']))

    def test_transfer_during_native_closing_is_reported(self):
        for preceding_status in (2, 3):
            with self.subTest(preceding_status=preceding_status):
                report = self.evidence([trace(2, status=preceding_status), trace(3, status=3,
                    wait_probe_status_before=preceding_status, ammo_readback_after=3,
                    pistol_reserve_after=9)])
                self.assertTrue(any(i['kind'] == 'reload_readback_change_without_insertion' for i in report['issues']))

    def test_cleared_readback_does_not_claim_a_reload_transfer(self):
        report = self.evidence([trace(2), trace(3, status=4, wait_probe_status_before=2,
                                             ammo_readback_after=3, pistol_reserve_after=9)])
        self.assertEqual(report['summary']['issue_count'], 0)
        self.assertEqual(report['summary']['unit_readbacks'], 0)

    def test_end_before_start_is_rejected(self):
        self.evidence([])
        log = (self.root / 'runtime/cojvr.log').read_text()
        start, end = log.splitlines()
        self.rewrite('runtime/cojvr.log', end + '\n' + start + '\n')
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_deployment_check_must_cover_the_run_artifacts(self):
        self.evidence([])
        manifest = json.loads((self.root / 'evidence-manifest.json').read_text())
        manifest['deploymentChecks'][0]['destination'] = 'unrelated.bin'
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_build_manifest_hash_and_id_must_match_run(self):
        self.evidence([])
        (self.root / 'build-manifest.json').write_text(json.dumps(dict(manifestId='c'*64)))
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_build_identity_cannot_pass_with_a_consistent_wrong_hash(self):
        self.evidence([])
        data = json.dumps(dict(manifestId='c'*64)).encode()
        (self.root / 'build-manifest.json').write_bytes(data)
        run = json.loads((self.root / 'run-manifest.json').read_text())
        run['buildManifestSha256'] = hashlib.sha256(data).hexdigest()
        self.rewrite('run-manifest.json', json.dumps(run))
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_duplicate_missing_or_hashless_deployment_checks_are_rejected(self):
        for change in ('duplicate', 'missing', 'hashless', 'wrong_hash'):
            with self.subTest(change=change):
                self.evidence([])
                manifest = json.loads((self.root / 'evidence-manifest.json').read_text())
                if change == 'duplicate':
                    manifest['deploymentChecks'] *= 2
                elif change == 'missing':
                    manifest['deploymentChecks'] = []
                elif change == 'hashless':
                    del manifest['deploymentChecks'][0]['actualSha256']
                else:
                    manifest['deploymentChecks'][0]['actualSha256'] = 'c'*64
                (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):
                    analysis.analyze_evidence(self.root)

    def test_incomplete_runtime_remains_incomplete(self):
        report = self.evidence([trace(2)], ended=False)
        self.assertFalse(report['provenance']['runtime_complete'])
        self.assertFalse(report['headset_validated'])

    def test_cli_is_read_only_and_refuses_overwrite_or_evidence_output(self):
        self.evidence([])
        original = {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        output = Path(self.temp.name) / 'analysis.json'
        command = [sys.executable, str(TOOL), str(self.root), '--output']
        first = subprocess.run([*command, str(output)], capture_output=True, text=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertFalse(json.loads(output.read_text())['headset_validated'])
        written = output.read_bytes()
        for destination in (output, self.root / 'forbidden.json'):
            result = subprocess.run([*command, str(destination)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(output.read_bytes(), written)
        self.assertEqual(original, {p.relative_to(self.root): p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_gate_metrics_do_not_confuse_world_motion_with_opening(self):
        base = dict(manual_session=1, frame_sequence=3, hand=0, model='peacemaker',
                    drum='0,0,0', gate='0,2,0', drum_up='0,1,0', inward='0,0,1',
                    gate_up='0,1,0', gate_forward='0,0,1')
        moved = dict(base, frame_sequence=4, drum='10,20,30', gate='8,20,30',
                     drum_up='-1,0,0', gate_up='-1,0,0')
        report = self.evidence([trace(2), event('manual_reload_geometry', **base),
                               event('manual_reload_geometry', **moved)])
        rows = report['mechanical_observations']
        for key in ('gate_drum_distance_cm', 'gate_drum_up_cosine', 'gate_drum_forward_cosine'):
            self.assertAlmostEqual(rows[0][key], rows[1][key])
        self.assertFalse(any(r['gate_open_confirmed'] for r in rows))

    def test_geometry_needs_the_same_trace_frame_and_armed_hand(self):
        geometry = dict(manual_session=1, model='peacemaker', drum='0,0,0',
                        gate='0,2,0', drum_up='0,1,0', gate_up='0,1,0',
                        inward='0,0,1', gate_forward='0,0,1')
        report = self.evidence([
            trace(2, armed_hand=0),
            event('manual_reload_geometry', frame_sequence=2, hand=1, **geometry),
            event('manual_reload_geometry', frame_sequence=3, hand=0, **geometry),
            trace(4, armed_hand=0),
            event('manual_reload_geometry', frame_sequence=4, hand=0, **geometry),
        ])
        wrong, stale, matched = report['mechanical_observations']
        self.assertEqual(wrong['trace_correlation'], 'other_hand')
        self.assertIsNone(wrong['latest_probe_status'])
        self.assertEqual(stale['trace_correlation'], 'missing_same_frame_trace')
        self.assertIsNone(stale['latest_probe_status'])
        self.assertEqual(matched['trace_correlation'], 'same_frame_armed_hand')
        self.assertEqual(matched['latest_probe_status'], 2)
        self.assertEqual(report['summary']['correlated_mechanical_samples'], 1)

    def test_native_parenthesized_vectors_produce_mechanical_metrics(self):
        report = self.evidence([trace(2), event('manual_reload_geometry',
            manual_session=1, frame_sequence=2, hand=0, model='peacemaker',
            drum='(10.000000,20.000000,30.000000)',
            gate='(10.000000,22.000000,30.000000)',
            drum_up='(0.000000,1.000000,0.000000)',
            gate_up='(0.000000,1.000000,0.000000)',
            inward='(0.000000,0.000000,1.000000)',
            gate_forward='(0.000000,0.000000,1.000000)')])
        row = report['mechanical_observations'][0]
        self.assertEqual(report['summary']['issue_count'], 0)
        self.assertAlmostEqual(row['gate_drum_distance_cm'], 2)
        self.assertAlmostEqual(row['gate_drum_up_cosine'], 1)
        self.assertAlmostEqual(row['gate_drum_forward_cosine'], 1)
        self.assertFalse(row['gate_open_confirmed'])

    def test_malformed_native_vector_delimiters_remain_invalid(self):
        for gate in ('(0,2,0', '0,2,0)', '((0,2,0))', '(0,2)', '(0,nan,0)'):
            with self.subTest(gate=gate):
                report = self.evidence([trace(2), event('manual_reload_geometry',
                    manual_session=1, frame_sequence=2, hand=0, model='peacemaker',
                    drum='(0,0,0)', gate=gate, drum_up='(0,1,0)',
                    gate_up='(0,1,0)', inward='(0,0,1)', gate_forward='(0,0,1)')])
                self.assertTrue(any(i['kind'] == 'invalid_mechanical_geometry'
                                    for i in report['issues']))

    def test_mechanical_baseline_tracks_same_owner_only(self):
        def geometry(frame, gate, up='0,1,0'):
            return event('manual_reload_geometry', frame_sequence=frame, hand=0,
                         manual_session=1, model='peacemaker', drum='0,0,0',
                         gate=f'0,{gate},0', drum_up='0,1,0', gate_up=up,
                         inward='0,0,1', gate_forward='0,0,1')
        report = self.evidence([
            trace(2), geometry(2, 2),
            trace(3), geometry(3, 3, '1,0,0'),
            trace(4, player_generation=8), geometry(4, 10),
        ])
        start, shifted, replaced = report['mechanical_observations']
        self.assertAlmostEqual(start['gate_drum_distance_change_from_first_cm'], 0)
        self.assertAlmostEqual(shifted['gate_drum_distance_change_from_first_cm'], 1)
        self.assertAlmostEqual(shifted['gate_drum_up_cosine_change_from_first'], -1)
        self.assertAlmostEqual(replaced['gate_drum_distance_change_from_first_cm'], 0)
        self.assertEqual(report['summary']['correlated_mechanical_samples'], 3)
        self.assertFalse(any(row['gate_open_confirmed'] for row in report['mechanical_observations']))

    def test_ready_reference_compares_native_wait_without_claiming_clearance(self):
        def geometry(frame, gate_up, **extra):
            fields = dict(frame_sequence=frame, hand=0, manual_session=1,
                          model='peacemaker', source='pre_native', root='0,0,0',
                          root_up='0,1,0', root_forward='0,0,1', barrel='0,0,1',
                          barrel_up='0,1,0', barrel_forward='0,0,1', drum='0,0,0',
                          drum_up='0,1,0', inward='0,0,1', gate='0,2,0',
                          gate_up=gate_up, gate_forward='0,0,1')
            fields.update(extra)
            return event('manual_reload_geometry', **fields)

        ready = geometry(1, '0,1,0', manual_session=0, reference_pose=1,
                         phase='ready', native_status=0, native_observation_valid=1,
                         native_probe_valid=1, native_reloading=0, context_allowed=1,
                         player_generation=7, native_context=5, weapon_id=42, armed_hand=0)
        waiting = geometry(2, '1,0,0')
        overlay = geometry(2, '1,0,0', source='post_overlay')
        report = self.evidence([ready, trace(2), waiting, overlay])
        ref, movement, after_overlay = report['mechanical_observations']
        self.assertTrue(ref['ready_reference_eligible'])
        self.assertFalse(ref['native_motion_evidence_eligible'])
        self.assertTrue(movement['ready_reference_available'])
        self.assertAlmostEqual(movement['gate_root_up_cosine_change_from_ready'], -1)
        self.assertFalse(after_overlay['ready_reference_available'])
        self.assertFalse(any(row['gate_open_confirmed'] for row in report['mechanical_observations']))

    def test_relative_orientation_ignores_global_weapon_motion(self):
        def geometry(frame, **extra):
            fields = dict(frame_sequence=frame, hand=0, model='peacemaker',
                          source='pre_native', manual_session=1, root='0,0,0',
                          root_up='0,1,0', root_forward='0,0,1',
                          barrel='0,0,1', barrel_up='0,1,0', barrel_forward='0,0,1',
                          drum='0,0,0', drum_up='0,1,0', inward='0,0,1',
                          gate='0,2,0', gate_up='0,1,0', gate_forward='0,0,1')
            fields.update(extra)
            return event('manual_reload_geometry', **fields)

        ready = geometry(1, manual_session=0, reference_pose=1, phase='ready',
                         native_status=0, native_observation_valid=1,
                         native_probe_valid=1, native_reloading=0, context_allowed=1,
                         player_generation=7, native_context=5, weapon_id=42, armed_hand=0)
        # Translate the entire gun and rotate it by +90 degrees around world Y.
        moved = geometry(2, root='10,20,30', root_forward='1,0,0',
                         barrel='11,20,30', barrel_forward='1,0,0',
                         gate='10,22,30', gate_forward='1,0,0',
                         drum='10,20,30', inward='1,0,0')
        report = self.evidence([ready, trace(2), moved])
        row = report['mechanical_observations'][1]
        self.assertTrue(row['ready_reference_available'])
        for part in ('gate', 'drum'):
            for anchor in ('root', 'barrel'):
                self.assertAlmostEqual(row[f'{part}_{anchor}_rotation_from_ready_deg'], 0, places=4)
                self.assertAlmostEqual(row[f'{part}_{anchor}_translation_from_ready_cm'], 0, places=4)
        self.assertFalse(row['gate_open_confirmed'])

    def test_ready_rotation_measure_is_component_specific(self):
        fields = dict(frame_sequence=1, hand=0, model='peacemaker', source='pre_native',
                      manual_session=0, reference_pose=1, phase='ready', native_status=0,
                      native_observation_valid=1, native_probe_valid=1, native_reloading=0,
                      context_allowed=1, player_generation=7, native_context=5,
                      weapon_id=42, armed_hand=0, root='0,0,0', root_up='0,1,0',
                      root_forward='0,0,1', barrel='0,0,1', barrel_up='0,1,0',
                      barrel_forward='0,0,1', gate='0,2,0', gate_up='0,1,0',
                      gate_forward='0,0,1', drum='0,0,0', drum_up='0,1,0',
                      inward='0,0,1')
        gate_turn = dict(fields, frame_sequence=2, manual_session=1, reference_pose=0,
                         gate_up='0,0,1', gate_forward='0,-1,0')
        drum_turn = dict(fields, frame_sequence=3, manual_session=1, reference_pose=0,
                         drum_up='1,0,0')
        report = self.evidence([event('manual_reload_geometry', **fields),
                                trace(2), event('manual_reload_geometry', **gate_turn),
                                trace(3), event('manual_reload_geometry', **drum_turn)])
        gate_row, drum_row = report['mechanical_observations'][1:]
        for anchor in ('root', 'barrel'):
            self.assertAlmostEqual(gate_row[f'gate_{anchor}_rotation_from_ready_deg'], 90, places=4)
            self.assertAlmostEqual(gate_row[f'drum_{anchor}_rotation_from_ready_deg'], 0, places=4)
            self.assertAlmostEqual(drum_row[f'gate_{anchor}_rotation_from_ready_deg'], 0, places=4)
            self.assertAlmostEqual(drum_row[f'drum_{anchor}_rotation_from_ready_deg'], 90, places=4)
        self.assertFalse(any(row['gate_open_confirmed'] for row in report['mechanical_observations']))

    def test_ready_reference_requires_exact_owner_phase_and_native_readback(self):
        source = dict(frame_sequence=1, hand=0, manual_session=0,
                      reference_pose=1, phase='ready', native_status=4,
                      native_observation_valid=1, native_probe_valid=1, native_reloading=0,
                      context_allowed=1, player_generation=7,
                      native_context=5, weapon_id=42, model='peacemaker',
                      source='pre_native', root='0,0,0', root_up='0,1,0',
                      root_forward='0,0,1', barrel='0,0,1', barrel_up='0,1,0',
                      barrel_forward='0,0,1', drum='0,0,0', drum_up='0,1,0',
                      inward='0,0,1', gate='0,2,0', gate_up='0,1,0',
                      gate_forward='0,0,1')
        for changes in (dict(native_status=2), dict(native_reloading=1), dict(source='post_overlay'),
                        dict(native_probe_valid=0), dict(phase='manual_load'),
                        dict(weapon_id=0), dict(reference_pose=0),
                        dict(armed_hand=1)):
            with self.subTest(changes=changes):
                fields=dict(source, armed_hand=0, **changes) if 'armed_hand' not in changes else dict(source, **changes)
                report=self.evidence([event('manual_reload_geometry', **fields),
                                      trace(2), event('manual_reload_geometry',
                                          **dict(source, frame_sequence=2, manual_session=1,
                                                 reference_pose=0, phase='manual_load'))])
                self.assertFalse(any(r['ready_reference_eligible'] for r in report['mechanical_observations']))
                self.assertFalse(any(r['ready_reference_available'] for r in report['mechanical_observations']))

    def test_ready_reference_does_not_cross_weapon_owner_or_model(self):
        common = dict(hand=0, model='peacemaker', source='pre_native', root='0,0,0',
                      root_up='0,1,0', root_forward='0,0,1', barrel='0,0,1',
                      barrel_up='0,1,0', barrel_forward='0,0,1', drum='0,0,0',
                      drum_up='0,1,0', inward='0,0,1', gate='0,2,0',
                      gate_up='0,1,0', gate_forward='0,0,1')
        ready = dict(common, frame_sequence=1, manual_session=0, reference_pose=1,
                     phase='ready', native_status=0, native_observation_valid=1,
                     native_probe_valid=1, native_reloading=0, context_allowed=1,
                     player_generation=7, native_context=5, weapon_id=42, armed_hand=0)
        waiting = dict(common, frame_sequence=2, manual_session=1)
        report = self.evidence([event('manual_reload_geometry', **ready),
            trace(2, weapon_id=43), event('manual_reload_geometry', **waiting),
            trace(3, weapon_id=42, armed_hand=1),
            event('manual_reload_geometry', **dict(waiting, frame_sequence=3, hand=1)),
            trace(4), event('manual_reload_geometry', **dict(waiting, frame_sequence=4, model='frontier'))])
        self.assertEqual(report['summary']['ready_reference_samples'], 1)
        self.assertEqual(report['summary']['ready_comparisons'], 0)

    def test_nonfinite_computed_geometry_is_rejected(self):
        report = self.evidence([event('manual_reload_geometry', manual_session=1,
                                     drum='1e308,0,0', gate='-1e308,0,0', drum_up='0,1,0',
                                     inward='0,0,1', gate_up='0,1,0', gate_forward='0,0,1')])
        self.assertTrue(any(i['kind'] == 'invalid_mechanical_geometry' for i in report['issues']))
        json.dumps(report, allow_nan=False)

    def test_unchanged_rejection_is_distinct_from_acceptance(self):
        report = self.evidence([trace(2), insertion(result='rejected', ammo_after=2, reserve_after=10), trace(3)])
        self.assertEqual(report['insertions'][0]['assessment'], 'no_transfer_readback_observed')

    def test_tampered_log_is_rejected(self):
        self.evidence([])
        (self.root / 'runtime/cojvr.log').write_text('tampered', encoding='utf-8')
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_manifest_path_escape_is_rejected(self):
        self.evidence([])
        manifest = json.loads((self.root / 'evidence-manifest.json').read_text())
        manifest['files'][0]['path'] = '../outside.json'
        (self.root / 'evidence-manifest.json').write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            analysis.analyze_evidence(self.root)

    def test_legacy_probe_is_not_a_persistent_session(self):
        report = self.evidence([trace(2, manual_session_enabled=0)], manual=False)
        self.assertFalse(report['manual_candidate'])
        self.assertEqual(report['summary']['accepted_insertions'], 0)


if __name__ == '__main__':
    unittest.main()
