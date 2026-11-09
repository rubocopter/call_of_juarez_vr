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
