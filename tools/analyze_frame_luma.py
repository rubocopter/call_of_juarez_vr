"""Summarize bounded native pixel samples; never identify a rendering cause."""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re

PREFIX = 'native_stereo_luma_probe:'

def fields(line):
    return dict(re.findall(r'(?:^|[ ;])([a-z_]+)=([^;\s]+)', line))

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
    outcome='accepted' if submit=='accepted' else 'rejected' if 'rejected' in (
        submit,row['left_capture'],row['right_capture']) else 'other'
    return dict(frameSequence=numbers['sequence'], poseSequence=numbers['pose_sequence'],
                presentSequence=numbers['present_sequence'], generation=numbers['generation'],
                device=numbers['device'], threadId=numbers['thread_id'], primaryBackbuffer=numbers['primary_backbuffer'],
                presentHresult=row['present_hr'], leftWrapper=row['left_wrapper'], rightWrapper=row['right_wrapper'],
                leftCapture=row['left_capture'], rightCapture=row['right_capture'], submitCallback=submit,
                naturalComplete=row['natural_complete']=='1', outcome=outcome, samples={})

def analyze(text):
    report = dict(schema=1, status='rejected', cause=None, headsetValidated=False,
                  flashAbsenceProven=False, streams={}, brightJumps=[], outputRoutes=[], errors=[],
                  limitations=['16x16 point samples do not cover every pixel or every frame.',
                               'White/brightness jumps may be authored transitions, not defects.',
                               'Readback/telemetry perturbs timing; this is not a performance gate.',
                               'Native-source samples do not measure compositor output.',
                               'Output routes observe callback order and sparse sample identities.'])
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
        reset_epoch=0
        for i,line in enumerate(lines):
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
            report['outputRoutes'].append(route)
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
