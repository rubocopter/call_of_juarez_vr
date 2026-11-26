"""Summarize bounded native pixel samples; never identify a rendering cause."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re

PREFIX = 'native_stereo_luma_probe:'
PRESENTER_UPLOAD = 'native_stereo_presenter_frame: status=new '
PRESENTER_SUBMIT = 'native_stereo_presenter_timing: status=ok '
SUBMIT_REJECTIONS = {'source_authorization_rejected', 'capture_epoch_missing',
                     'presentation_owner_changed', 'producer_end_frame_failed',
                     'queue_registration_rejected'}

def fields(line):
    return dict(re.findall(r'(?:^|[ ;])([a-z_]+)=([^;\s]+)', line))


def presenter_fields(line):
    keys = re.findall(r'(?:^|[ ;])([a-z_]+)=', line)
    if len(keys) != len(set(keys)):
        raise ValueError('Duplicate presenter telemetry field')
    return fields(line)


def positive_identity(row, field):
    value = row[field]
    if not re.fullmatch(r'[1-9][0-9]*', value) or int(value) >= 2**64:
        raise ValueError(f'Invalid presenter {field}')
    return int(value)


def presenter_upload(line):
    row = presenter_fields(line)
    if row.get('presentation_mode') != 'native_stereo':
        return None
    frame = positive_identity(row, 'frame_sequence')
    pose = positive_identity(row, 'render_pose_sequence')
    generation = positive_identity(row, 'generation')
    age = float(row['frame_age_ms'])
    if not math.isfinite(age) or age < 0:
        raise ValueError('Invalid presenter frame age')
    return dict(frame=frame, pose=pose, generation=generation, frameAgeMs=age)


def presenter_submission(line):
    row = presenter_fields(line)
    if (row.get('presentation_mode'), row.get('pose_mode'),
            row.get('left_result'), row.get('right_result')) != (
            'native_stereo', 'explicit_render_pose', '0', '0'):
        return None
    if row.get('content') not in ('new', 'repeated'):
        raise ValueError('Invalid presenter submission content')
    return dict(frame=positive_identity(row, 'capture_sequence'),
                pose=positive_identity(row, 'render_pose_sequence'),
                submitSequence=positive_identity(row, 'submit_sequence'), content=row['content'])

def output_route(line):
    pairs = line[len(PREFIX):].strip().split(';')
    row = dict(pair.split('=',1) for pair in pairs)
    if len(row)!=len(pairs) or row.get('present_kind')!='device_present' or (
            row.get('coverage')!='observed_wrapper_and_present_callbacks'):
        raise ValueError('Invalid output route envelope')
    identities = ('sequence','pose_sequence','present_sequence','generation','device','thread_id')
    numbers = {key:int(row[key]) for key in (*identities,'primary_backbuffer')}
    if any(not 0<numbers[key]<2**64 for key in identities) or not 0<=numbers['primary_backbuffer']<2**64:
        raise ValueError('Invalid output route identity')
    if not re.fullmatch(r'0x[0-9a-fA-F]{1,8}',row['present_hr']) or row['natural_complete'] not in ('0','1'):
        raise ValueError('Invalid output route result')
    for eye in ('left','right'):
        wrapper,capture=row[eye+'_wrapper'],row[eye+'_capture']
        if wrapper not in ('complete','incomplete','unobserved') or (
                capture!='unobserved' and (wrapper!='complete' or capture not in ('accepted','rejected','bypassed'))):
            raise ValueError('Invalid eye route')
    submit=row['submit_callback']
    if submit not in ('accepted','rejected','bypassed','unobserved') or row['left_wrapper']=='unobserved':
        raise ValueError('Invalid submit route')
    rejection = row.get('submit_rejection')
    if rejection is not None and (submit != 'rejected' or rejection not in SUBMIT_REJECTIONS):
        raise ValueError('Invalid submit rejection reason')
    captures=(row['left_capture'], row['right_capture'])
    rejected=submit=='rejected' or 'rejected' in captures
    accepted=submit=='accepted' and all(capture=='accepted' for capture in captures)
    outcome='rejected' if rejected else 'accepted' if accepted else 'other'
    return dict(frameSequence=numbers['sequence'], poseSequence=numbers['pose_sequence'],
                presentSequence=numbers['present_sequence'], generation=numbers['generation'],
                device=numbers['device'], threadId=numbers['thread_id'], primaryBackbuffer=numbers['primary_backbuffer'],
                presentHresult=row['present_hr'], leftWrapper=row['left_wrapper'], rightWrapper=row['right_wrapper'],
                leftCapture=row['left_capture'], rightCapture=row['right_capture'], submitCallback=submit,
                submitRejection=rejection,
                naturalComplete=row['natural_complete']=='1', outcome=outcome, samples={})

def analyze(text):
    report = dict(schema=1, status='rejected', cause=None, headsetValidated=False,
                  flashAbsenceProven=False, presenterCorrelationEligible=True,
                  streams={}, brightJumps=[], outputRoutes=[], errors=[],
                  limitations=['16x16 point samples do not cover every pixel or every frame.',
                               'White/brightness jumps may be authored transitions, not defects.',
                               'Readback/telemetry perturbs timing; this is not a performance gate.',
                               'Native-source samples do not measure compositor output.',
                               'Output routes observe callback order and sparse sample identities.',
                               'Sparse presenter logs may omit uploads or submissions.',
                               'Malformed or legacy presenter events disable presenter correlation for that run.',
                               'Successful OpenVR Submit calls do not prove headset display.'])
    lines=text.splitlines()
    try:
        starts=[i for i,s in enumerate(lines) if s.startswith('run_start:')]
        ends=[i for i,s in enumerate(lines) if s.startswith('run_end:')]
        if len(starts)!=1 or len(ends)!=1 or starts[0]>=ends[0]:
            raise ValueError('Require one complete ordered runtime interval')
        start,end=starts[0],ends[0]
        identity=fields(lines[start])
        if not identity.get('run_id') or fields(lines[end]).get('run_id')!=identity['run_id']:
            raise ValueError('Mismatched run identity')
        report.update(runId=identity['run_id'],buildManifestId=identity.get('build_manifest_id'))
        previous={}
        routes, route_ids, samples = [], set(), defaultdict(list)
        uploads, submissions = defaultdict(list), defaultdict(list)
        reset_epoch=0
        for i,line in enumerate(lines):
            if start < i < end and line.startswith(PRESENTER_UPLOAD):
                try:
                    upload = presenter_upload(line)
                    if upload is not None:
                        uploads[reset_epoch,upload['frame'],upload['pose']].append((i + 1, upload))
                except (KeyError, ValueError, OverflowError):
                    report['presenterCorrelationEligible'] = False
                continue
            if start < i < end and line.startswith(PRESENTER_SUBMIT):
                try:
                    submission = presenter_submission(line)
                    if submission is not None:
                        submissions[reset_epoch,submission['frame'],submission['pose']].append((i + 1, submission))
                except (KeyError, ValueError, OverflowError):
                    report['presenterCorrelationEligible'] = False
                continue
            if not line.startswith(PREFIX): continue
            if not start<i<end: raise ValueError('Pixel telemetry outside run interval')
            keys=re.findall(r'(?:^|[ ;])([a-z_]+)=',line[len(PREFIX):])
            if len(keys)!=len(set(keys)):raise ValueError('Duplicate pixel telemetry field')
            row=fields(line)
            if row.get('status')=='reset':
                previous.clear()
                reset_epoch+=1
            if row.get('status')=='output_route':
                route=output_route(line)
                identity=(reset_epoch,route['device'],route['generation'],route['presentSequence'])
                if identity in route_ids:
                    raise ValueError('Repeated device Present route')
                route_ids.add(identity)
                route['logLine'] = i + 1
                routes.append((reset_epoch,route))
                continue
            if row.get('status')!='sample': continue
            stream=row.get('stream')
            if stream not in ('left','right','desktop'): raise ValueError('Unknown sample stream')
            mean,white=float(row['mean']),float(row['white_fraction'])
            if not math.isfinite(mean) or not 0<=mean<=255 or not math.isfinite(white) or not 0<=white<=1:
                raise ValueError('Invalid sampled brightness')
            sequence,time,generation=map(int,(row['sequence'],row['time_ns'],row['generation']))
            if min(sequence,time,generation)<=0: raise ValueError('Invalid sample identity')
            key=(stream,generation,row['device'],row['source'],row['source_width'],row['source_height'])
            old=previous.get(key)
            if old and time<=old['time_ns']: raise ValueError('Non-monotonic sample timestamp')
            sample=dict(stream=stream,sequence=sequence,time_ns=time,generation=generation,
                        poseSequence=int(row['pose_sequence']),mean=mean,whiteFraction=white)
            samples[reset_epoch,stream,generation,row['device'],sequence,sample['poseSequence']].append((row['source'],sample))
            entry=report['streams'].setdefault(stream,dict(samples=0,maxMean=0,maxWhiteFraction=0,maxReadbackMs=0))
            entry['samples']+=1
            entry['maxMean']=max(entry['maxMean'],mean)
            entry['maxWhiteFraction']=max(entry['maxWhiteFraction'],white)
            readback=float(row['readback_ms'])
            if not math.isfinite(readback) or readback<0: raise ValueError('Invalid readback timing')
            entry['maxReadbackMs']=max(entry['maxReadbackMs'],readback)
            # The logger retains the preceding sample for each detected change.
            if old and mean-old['mean']>=24 and white>=.75:
                report['brightJumps'].append(dict(sample,previousMean=old['mean'],
                    precedingSequence=old['sequence'],gapNs=time-old['time_ns']))
            previous[key]=sample
        route_counts = defaultdict(int)
        for epoch,route in routes:
            route_counts[epoch,route['frameSequence'],route['poseSequence']] += 1
        for epoch,route in routes:
            for stream,sequence in (('left',route['frameSequence']),('right',route['frameSequence']),
                                    ('desktop',route['presentSequence'])):
                if stream!='desktop' and route[stream+'Wrapper']!='complete':
                    continue
                matches=samples.get((epoch,stream,route['generation'],str(route['device']),sequence,route['poseSequence']),[])
                if stream=='desktop':
                    matches=[(source,sample) for source,sample in matches
                             if route['primaryBackbuffer'] and source==str(route['primaryBackbuffer'])]
                if len(matches)==1:
                    route['samples'][stream]=dict(matches[0][1],source=matches[0][0])
            key = (epoch,route['frameSequence'],route['poseSequence'])
            candidates = uploads[key]
            if report['presenterCorrelationEligible'] and route_counts[key] == 1 and len(candidates) == 1:
                upload_line,upload = candidates[0]
                if upload_line > route['logLine'] and upload['generation'] == route['generation']:
                    observed = [(line,submission) for line,submission in submissions[key]
                                if line > upload_line]
                    sequences = [entry['submitSequence'] for _,entry in observed]
                    if len(sequences) == len(set(sequences)):
                        route['presenter'] = dict(
                            upload=dict(logLine=upload_line, frameAgeMs=upload['frameAgeMs']),
                            successfulOpenVrSubmissions=[dict(logLine=line, submitSequence=entry['submitSequence'],
                                                             content=entry['content']) for line,entry in observed])
            report['outputRoutes'].append(route)
        coverage = {}
        for outcome in ('accepted','rejected','other'):
            selected = [route for route in report['outputRoutes'] if route['outcome'] == outcome]
            presenters = [route['presenter'] for route in selected if 'presenter' in route]
            coverage[outcome] = dict(
                routes=len(selected),
                presenterUploadsObserved=len(presenters),
                newOpenVrSubmissionsObserved=sum(any(
                    item['content'] == 'new' for item in presenter['successfulOpenVrSubmissions'])
                    for presenter in presenters),
                repeatedOpenVrSubmissionsObserved=sum(any(
                    item['content'] == 'repeated' for item in presenter['successfulOpenVrSubmissions'])
                    for presenter in presenters))
        report['outputRouteCoverage'] = coverage
        report['status']='observed' if report['streams'] or report['outputRoutes'] else 'inconclusive'
    except (KeyError,ValueError,TypeError) as error:
        report['errors'].append(str(error))
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=analyze(args.log.read_text(encoding='utf-8-sig',errors='replace'))
    rendered=json.dumps(result,indent=2)
    if args.output: args.output.write_text(rendered+'\n',encoding='utf-8')
    print(rendered)
    raise SystemExit(1 if result['status']=='rejected' else 0)
