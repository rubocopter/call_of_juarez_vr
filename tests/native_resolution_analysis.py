"""Synthetic evidence must never promote a headset gate."""
from pathlib import Path
import json
import sys
import unittest
import subprocess
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import analyze_coj_native_resolution as a
from render_resource_analysis import pair, log


def sample():
    events = pair()
    for event in events:
        if event['event'] == 'record':
            state = event['state']
            for resource in (state['targets'][0], state['depth']):
                resource.update(width=2804, height=2860)
            state['viewport'] = [0, 0, 2804, 2860]
            state['scissor'] = [0, 0, 2804, 2860]
    lines = []
    for eye in ('left', 'right'):
        lines.append(f'native_stereo_capture_timing: status=ok frame_sequence=9 eye={eye} transport=d3d9ex_shared_texture_ring;eye_surface=2804x2860;capture_surface=2804x2860;viewport=0,0,2804,2860,0,1;source_msaa=0')
    lines += [
        'native_stereo_presenter_frame: status=new frame_sequence=9;generation=1;render_pose_sequence=7;presentation_mode=native_stereo;transport=d3d9ex_shared_texture_ring;source=2804x2860;left_texture=2804x2860;right_texture=2804x2860;eye_resources_distinct=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=1;capture_sequence=9;render_pose_sequence=7;pose_mode=explicit_render_pose;presentation_mode=native_stereo;content=new;left_result=0;right_result=0',
    ]
    return log(events).replace('run_end:', '\n'.join(lines) + '\nrun_end:')


class NativeResolutionTests(unittest.TestCase):
    def test_full_native_chain_is_observed_without_headset_acceptance(self):
        result = a.analyze(sample(), 2804, 2860)
        self.assertEqual(result['status'], 'observed', result)
        self.assertEqual(result['eyes']['left']['captureExtents'], [[2804, 2860]])
        self.assertEqual(result['eyes']['right']['outputExtents'], [[2804, 2860]])
        self.assertEqual(result['geometryFrames'], [9])
        self.assertEqual(result['submittedCaptureSequences'], [9])
        self.assertFalse(result['headsetValidated'])

    def test_window_or_capture_only_does_not_qualify_geometry(self):
        text = sample().replace('"width": 2804', '"width": 2560').replace('"height": 2860', '"height": 1440').replace('[0, 0, 2804, 2860]', '[0, 0, 2560, 1440]')
        self.assertEqual(a.analyze(text, 2804, 2860)['status'], 'rejected')

    def test_missing_smaller_or_stretched_eye_rejects(self):
        text = sample()
        for bad in (
            text.replace('eye=right ', 'eye=missing '),
            text.replace('capture_surface=2804x2860', 'capture_surface=2560x1440'),
            text.replace('right_texture=2804x2860', 'right_texture=2560x1440'),
            text.replace('viewport=0,0,2804,2860', 'viewport=0,0,2560,1440'),
            text.replace('left_result=0', 'left_result=1'),
            text.replace('render_pose_sequence=7;pose_mode', 'render_pose_sequence=8;pose_mode'),
            text.replace('capture_sequence=9', 'capture_sequence=10'),
            text.replace('eye_resources_distinct=true', 'eye_resources_distinct=false'),
            text.replace('run_end: run_id=synthetic', 'run_end: run_id=foreign'),
            text.replace('run_end: run_id=synthetic', ''),
            text.replace(';source_msaa=0', ';source_msaa=2'),
        ):
            with self.subTest(bad=bad[-220:]):
                self.assertEqual(a.analyze(bad, 2804, 2860)['status'], 'rejected')

    def test_geometry_requires_depth_and_full_scissor(self):
        text = sample()
        for bad in (text.replace('"z": [1, 1, 4]', '"z": [0, 0, 4]'),
                    text.replace('"scissorEnabled": 0', '"scissorEnabled": 1').replace('"scissor": [0, 0, 2804, 2860]', '"scissor": [0, 0, 1280, 720]')):
            self.assertEqual(a.analyze(bad, 2804, 2860)['status'], 'rejected')

    def test_padded_depth_reports_actual_extent(self):
        lines = sample().splitlines()
        for index, line in enumerate(lines):
            if line.startswith('render_resource_probe: '):
                event = json.loads(line.split(': ', 1)[1])
                if event['event'] == 'record':
                    event['state']['depth'].update(width=2816, height=2864)
                lines[index] = 'render_resource_probe: ' + json.dumps(event)
        result = a.analyze('\n'.join(lines), 2804, 2860)
        self.assertEqual(result['status'], 'observed', result)
        self.assertEqual(result['eyes']['left']['geometry'][0]['depth'], [2816, 2864])


class ExperimentalExtentCli(unittest.TestCase):
    def test_independent_requested_extent_is_measured_without_upscale_acceptance(self):
        with tempfile.TemporaryDirectory() as folder:
            trace=Path(folder)/'runtime.log'
            trace.write_text(sample().replace('2804','2428').replace('2860','2477'),encoding='utf-8')
            command=[sys.executable,str(Path(__file__).parents[1]/'tools/analyze_coj_native_resolution.py'),str(trace),'--expected','2428x2477']
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            report=json.loads(result.stdout)
            self.assertEqual(report['status'],'observed',report)
            self.assertEqual(report['expectedExtent'],[2428,2477])
            self.assertFalse(report['headsetValidated'])

if __name__ == '__main__':
    unittest.main()
