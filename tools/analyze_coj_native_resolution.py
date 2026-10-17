"""Check sampled native raster/capture/output extents, never headset acceptance."""
import argparse
import json
from pathlib import Path
import re

import analyze_coj_render_resources as resources


def fields(line):
    return dict(re.findall(r'(?:^|[ ;])([a-z_]+)=([^; ]+)', line))


def analyze(text, width, height):
    expected = [width, height]
    size = f'{width}x{height}'
    report = dict(schema=1, status='rejected', expectedExtent=expected,
                  coverage='sampled_native_depth_draws_capture_and_submitted_outputs',
                  headsetValidated=False, geometryFrames=[], submittedCaptureSequences=[],
                  eyes={eye: dict(captureExtents=[], outputExtents=[], geometry=[]) for eye in ('left', 'right')},
                  errors=[], limitations=[
                      'Binding evidence is not pixel/shader-content or visual-quality proof.',
                      'Reduced-resolution shadow/postprocess passes may remain native.',
                      'Timing during the bounded resource probe is diagnostic overhead.'])
    try:
        binding = resources.analyze(text)
        if binding['status'] != 'observed':
            raise ValueError('Incomplete binding evidence: ' + '; '.join(binding['errors']))
        report.update(runId=binding['runId'], buildManifestId=binding['buildManifestId'])
        lines = text.splitlines()
        starts = [i for i, line in enumerate(lines) if line.startswith('run_start:')]
        ends = [i for i, line in enumerate(lines) if line.startswith('run_end:')]
        if len(starts) != 1 or len(ends) != 1 or ends[0] <= starts[0]:
            raise ValueError('Require one anchored, complete runtime interval')
        lines = lines[starts[0]+1:ends[0]]
        scopes = {(s['thread'], s['scope']): s for s in binding['scopes'] if s['route'] == 0}
        geometry = {}
        captures = {}
        uploads = {}
        submitted = set()
        for line in lines:
            if line.startswith(resources.PREFIX):
                event = json.loads(line[len(resources.PREFIX):])
                scope = scopes.get((event['thread'], event['scope']))
                if not scope or event['event'] != 'record' or event['op'] not in (6, 7, 8, 9):
                    continue
                state = event['state']
                color, depth = state['targets'][0], state['depth']
                if (color['format'] == resources.NULL_COLOR_FORMAT or
                        [color['width'], color['height']] != expected or not depth['bound'] or
                        depth['width'] < width or depth['height'] < height or
                        state['viewport'] != [0, 0, width, height] or
                        state['z'][0] != 1 or state['z'][1] != 1 or
                        (state['scissorEnabled'] and (state['scissor'][0] > 0 or state['scissor'][1] > 0 or
                            state['scissor'][2] < width or state['scissor'][3] < height))):
                    continue
                eye = ('left', 'right')[scope['eye']]
                geometry.setdefault(scope['frame'], set()).add(eye)
                descriptor = dict(frame=scope['frame'], deviceGeneration=scope['deviceGeneration'],
                                  color=expected, depth=[depth['width'], depth['height']], viewport=state['viewport'],
                                  colorFormat=color['format'], depthFormat=depth['format'])
                if descriptor not in report['eyes'][eye]['geometry']:
                    report['eyes'][eye]['geometry'].append(descriptor)
            elif line.startswith('native_stereo_capture_timing: status=ok '):
                f = fields(line)
                eye = f.get('eye')
                if eye not in report['eyes']:
                    raise ValueError('Unknown capture eye')
                if (f.get('eye_surface') != size or f.get('capture_surface') != size or
                        f.get('viewport') != f'0,0,{width},{height},0,1' or
                        f.get('source_msaa') != '0' or f.get('transport') != 'd3d9ex_shared_texture_ring'):
                    raise ValueError('Native capture extent/viewport/transport mismatch: ' + eye)
                captures.setdefault(int(f['frame_sequence']), set()).add(eye)
                report['eyes'][eye]['captureExtents'] = [expected]
            elif line.startswith('native_stereo_presenter_frame: status=new '):
                f = fields(line)
                if f.get('presentation_mode') != 'native_stereo':
                    continue
                if (f.get('source') != size or f.get('left_texture') != size or
                        f.get('right_texture') != size or f.get('eye_resources_distinct') != 'true' or
                        f.get('transport') != 'd3d9ex_shared_texture_ring'):
                    raise ValueError('Native D3D11 output extent/transport mismatch')
                uploads[int(f['frame_sequence'])] = int(f['render_pose_sequence'])
                for eye in report['eyes'].values():
                    eye['outputExtents'] = [expected]
            elif line.startswith('native_stereo_presenter_timing: status=ok '):
                f = fields(line)
                if (f.get('presentation_mode') == 'native_stereo' and f.get('content') == 'new' and
                        f.get('left_result') == '0' and f.get('right_result') == '0' and
                        f.get('pose_mode') == 'explicit_render_pose'):
                    submitted.add((int(f['capture_sequence']), int(f['render_pose_sequence'])))
        both = {'left', 'right'}
        report['geometryFrames'] = sorted(frame for frame, eyes in geometry.items() if eyes == both)
        report['submittedCaptureSequences'] = sorted(frame for frame, pose in submitted
            if pose > 0 and uploads.get(frame) == pose and captures.get(frame) == both)
        if not report['geometryFrames'] or not report['submittedCaptureSequences']:
            raise ValueError('Require full-size native depth draws for both eyes and a captured/uploaded/submitted pair')
        report['status'] = 'observed'
    except (ValueError, TypeError, KeyError, IndexError) as error:
        report['errors'].append(str(error))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    def extent(value):
        match=re.fullmatch(r'([1-9][0-9]*)x([1-9][0-9]*)',value)
        if not match or any(int(axis)>2**31-1 for axis in match.groups()):
            raise argparse.ArgumentTypeError('Expected positive native int32 widthxheight')
        return tuple(map(int,match.groups()))
    parser.add_argument('--expected',type=extent,required=True)
    args = parser.parse_args()
    # A valid JSON report can describe rejection; callers must check status.
    print(json.dumps(analyze(args.log.read_text(encoding='utf-8-sig'), *args.expected), allow_nan=False))


if __name__ == '__main__':
    main()
