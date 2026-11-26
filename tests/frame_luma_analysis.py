import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('luma', Path(__file__).parents[1] / 'tools/analyze_frame_luma.py')
luma = importlib.util.module_from_spec(spec)
spec.loader.exec_module(luma)

START = 'run_start: run_id=fixture build_manifest_id=build pid=123\n'
END = 'run_end: run_id=fixture\n'

def sample(stream, sequence, mean, white):
    return (f'native_stereo_luma_probe: status=sample;stream={stream};sequence={sequence};'
            f'generation=2;device=123;source=456;source_width=2804;source_height=2860;'
            f'pose_sequence={sequence+50};time_ns={sequence*10000000};mean={mean};'
            f'white_fraction={white};queue_ms=0.01;readback_ms=0.03\n')


def rejected_route():
    return ('native_stereo_luma_probe: status=output_route;present_kind=device_present;sequence=3;'
            'pose_sequence=53;present_sequence=7;generation=2;device=123;thread_id=91;'
            'left_wrapper=complete;right_wrapper=unobserved;natural_complete=1;'
            'left_capture=rejected;right_capture=unobserved;submit_callback=unobserved;'
            'primary_backbuffer=456;present_hr=0x0;coverage=observed_wrapper_and_present_callbacks\n')


def accepted_route():
    return (rejected_route().replace('left_capture=rejected', 'left_capture=accepted')
            .replace('right_wrapper=unobserved', 'right_wrapper=complete')
            .replace('right_capture=unobserved', 'right_capture=accepted')
            .replace('submit_callback=unobserved', 'submit_callback=accepted'))


def presenter_upload():
    return ('native_stereo_presenter_frame: status=new frame_sequence=3;render_pose_sequence=53;'
            'generation=2;presentation_mode=native_stereo;frame_age_ms=12.5\n')


def openvr_success():
    return ('native_stereo_presenter_timing: status=ok submit_sequence=41;capture_sequence=3;'
            'render_pose_sequence=53;pose_mode=explicit_render_pose;presentation_mode=native_stereo;'
            'content=new;left_result=0;right_result=0\n')


class LumaAnalysis(unittest.TestCase):
    def test_rejected_eye_route_joins_eye_and_desktop_on_distinct_sequences(self):
        desktop = sample('desktop',7,50,.1).replace('pose_sequence=57','pose_sequence=53')
        result = luma.analyze(START + sample('left',3,42,0) + rejected_route() + desktop + END)
        route, = result['outputRoutes']
        self.assertEqual(route['outcome'], 'rejected')
        self.assertEqual(route['leftWrapper'], 'complete')
        self.assertEqual(route['rightWrapper'], 'unobserved')
        self.assertTrue(route['naturalComplete'])
        self.assertEqual({k: v['mean'] for k, v in route['samples'].items()}, {'left': 42, 'desktop': 50})
        self.assertIsNone(result['cause'])

    def test_output_route_does_not_pair_foreign_pose_backbuffer_or_reset(self):
        desktop = sample('desktop',7,50,.1).replace('pose_sequence=57','pose_sequence=53')
        for incompatible in (desktop.replace('pose_sequence=53','pose_sequence=54'),
                             desktop.replace('source=456','source=457'),
                             'native_stereo_luma_probe: status=reset\n'+desktop):
            with self.subTest(incompatible=incompatible):
                result = luma.analyze(START + rejected_route() + incompatible + END)
                route, = result['outputRoutes']
                self.assertEqual(route['samples'], {})
                self.assertFalse(result['flashAbsenceProven'])

    def test_invalid_and_repeated_present_route_reject(self):
        for route in (rejected_route() + rejected_route(),
                      rejected_route().replace('left_wrapper=complete','left_wrapper=unobserved'),
                      rejected_route().rstrip()+';status=disabled\n'):
            with self.subTest(route=route):
                self.assertEqual(luma.analyze(START+route+END)['status'],'rejected')

    def test_submit_reason_is_bound_to_a_rejected_submit_callback(self):
        legacy = rejected_route()
        rejected = legacy.replace('submit_callback=unobserved', 'submit_callback=rejected')
        stage = 'capture_epoch_missing'
        for route, reason in ((legacy, None), (rejected, None),
                              (rejected.rstrip()+f';submit_rejection={stage}\n', stage)):
            with self.subTest(route=route):
                result = luma.analyze(START+route+END)
                self.assertEqual(result['outputRoutes'][0]['submitRejection'], reason)
                self.assertIsNone(result['cause'])
        for route in (legacy.rstrip()+f';submit_rejection={stage}\n',
                      rejected.rstrip()+';submit_rejection=none\n',
                      rejected.rstrip()+';submit_rejection=arbitrary\n'):
            with self.subTest(route=route):
                self.assertEqual(luma.analyze(START+route+END)['status'], 'rejected')

    def test_output_route_requires_both_accepted_eye_captures(self):
        submitted = rejected_route().replace('submit_callback=unobserved',
                                              'submit_callback=accepted')
        incomplete = submitted.replace('left_capture=rejected', 'left_capture=accepted')
        complete = incomplete.replace('right_wrapper=unobserved', 'right_wrapper=complete').replace(
            'right_capture=unobserved', 'right_capture=accepted')
        for route, expected in ((submitted, 'rejected'), (incomplete, 'other'),
                                (complete, 'accepted')):
            with self.subTest(expected=expected):
                report = luma.analyze(START + route + END)
                self.assertEqual(report['status'], 'observed')
                self.assertEqual(report['outputRoutes'][0]['outcome'], expected)
                self.assertIsNone(report['cause'])

    def test_unique_native_pair_links_present_upload_and_openvr_api_success(self):
        repeated = openvr_success().replace('submit_sequence=41', 'submit_sequence=42').replace(
            'content=new', 'content=repeated')
        log = START + accepted_route() + presenter_upload() + openvr_success() + repeated + END
        report = luma.analyze(log)
        route, = report['outputRoutes']
        self.assertEqual(report['status'], 'observed')
        self.assertEqual(route['presenter']['upload']['frameAgeMs'], 12.5)
        self.assertEqual([s['content'] for s in route['presenter']['successfulOpenVrSubmissions']],
                         ['new', 'repeated'])
        self.assertLess(route['logLine'], route['presenter']['upload']['logLine'])
        self.assertEqual(report['outputRouteCoverage']['accepted'], dict(
            routes=1, presenterUploadsObserved=1, newOpenVrSubmissionsObserved=1,
            repeatedOpenVrSubmissionsObserved=1))
        self.assertFalse(report['headsetValidated'])
        self.assertIsNone(report['cause'])

    def test_presenter_correlation_requires_unique_ordered_generation_and_pose(self):
        route = accepted_route()
        upload = presenter_upload()
        submit = openvr_success()
        variants = (
            upload + route + submit,
            route + upload.replace('generation=2', 'generation=3') + submit,
            route + upload.replace('render_pose_sequence=53', 'render_pose_sequence=54') + submit,
            route + upload + upload + submit,
            route + route.replace('present_sequence=7', 'present_sequence=8') + upload + submit,
            route + 'native_stereo_luma_probe: status=reset\n' + upload + submit,
        )
        for candidate in variants:
            with self.subTest(candidate=candidate):
                report = luma.analyze(START + candidate + END)
                self.assertEqual(report['status'], 'observed')
                self.assertTrue(all('presenter' not in entry for entry in report['outputRoutes']))

    def test_presenter_submits_are_sparse_and_require_both_openvr_results(self):
        route = accepted_route()
        upload = presenter_upload()
        success = openvr_success()
        invalid = (success.replace('status=ok', 'status=failed'),
                   success.replace('right_result=0', 'right_result=101'),
                   success.replace('pose_mode=explicit_render_pose', 'pose_mode=flat_theater_anchor'),
                   success.replace('presentation_mode=native_stereo', 'presentation_mode=flat_theater'),
                   success.replace('render_pose_sequence=53', 'render_pose_sequence=54'))
        for candidate in invalid:
            with self.subTest(candidate=candidate):
                report = luma.analyze(START + route + upload + candidate + END)
                self.assertEqual(report['status'], 'observed')
                self.assertEqual(report['outputRoutes'][0]['presenter']['successfulOpenVrSubmissions'], [])
        report = luma.analyze(START + route + success + upload + END)
        self.assertEqual(report['outputRoutes'][0]['presenter']['successfulOpenVrSubmissions'], [])
        report = luma.analyze(START + route + upload + success + success + END)
        self.assertNotIn('presenter', report['outputRoutes'][0])

    def test_invalid_presenter_metadata_preserves_pixel_analysis_without_join(self):
        for candidate in (presenter_upload().replace('frame_sequence=3', 'frame_sequence=0'),
                          presenter_upload().replace('frame_age_ms=12.5', 'frame_age_ms=nan'),
                          presenter_upload().replace('generation=2', 'generation=2;generation=3'),
                          presenter_upload().replace('render_pose_sequence=53;', ''),
                          openvr_success().replace('submit_sequence=41', 'submit_sequence=18446744073709551616')):
            with self.subTest(candidate=candidate):
                report = luma.analyze(START + accepted_route() + candidate + END)
                self.assertEqual(report['status'], 'observed')
                self.assertFalse(report['presenterCorrelationEligible'])
                self.assertNotIn('presenter', report['outputRoutes'][0])
                self.assertFalse(report['headsetValidated'])

        report = luma.analyze(START + accepted_route() + presenter_upload() + openvr_success() +
                              presenter_upload().replace('frame_age_ms=12.5', 'frame_age_ms=nan') + END)
        self.assertFalse(report['presenterCorrelationEligible'])
        self.assertNotIn('presenter', report['outputRoutes'][0])

    def test_white_jump_bound_to_stream_and_generation(self):
        text = START + sample('left',1,40,0) + sample('right',1,40,0)
        text += sample('left',2,250,.98) + sample('right',2,250,.98) + END
        result = luma.analyze(text)
        self.assertEqual(result['status'],'observed')
        self.assertEqual({e['stream'] for e in result['brightJumps']},{'left','right'})
        self.assertFalse(result['headsetValidated'])
        self.assertIsNone(result['cause'])

    def test_missing_samples_and_no_jump_never_prove_no_flash(self):
        self.assertEqual(luma.analyze(START+END)['status'],'inconclusive')
        result = luma.analyze(START+sample('desktop',1,250,1)+END)
        self.assertEqual(result['brightJumps'],[])
        self.assertFalse(result['flashAbsenceProven'])

    def test_identity_change_cannot_create_temporal_jump(self):
        text=START+sample('left',1,40,0)+sample('left',2,250,1).replace('generation=2','generation=3')+END
        self.assertEqual(luma.analyze(text)['brightJumps'],[])
        text=START+sample('left',1,40,0)+'native_stereo_luma_probe: status=reset\n'+sample('left',2,250,1)+END
        self.assertEqual(luma.analyze(text)['brightJumps'],[])

    def test_reject_incomplete_foreign_and_invalid_values(self):
        for text in (START, START+END+END, START+END.replace('fixture','foreign'),
                     START+sample('left',1,'nan',0)+END,
                     START+sample('left',1,255,1.5)+END,
                     sample('left',1,40,0)+START+END):
            self.assertEqual(luma.analyze(text)['status'],'rejected')

if __name__ == '__main__':
    unittest.main()
