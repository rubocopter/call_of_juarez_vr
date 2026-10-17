"""Summarize bounded native pixel samples; never identify a rendering cause."""
import argparse
import json
import math
from pathlib import Path
import re

PREFIX = 'native_stereo_luma_probe:'

def fields(line):
    return dict(re.findall(r'(?:^|[ ;])([a-z_]+)=([^;\s]+)', line))

def analyze(text):
    report = dict(schema=1, status='rejected', cause=None, headsetValidated=False,
                  flashAbsenceProven=False, streams={}, brightJumps=[], errors=[],
                  limitations=['16x16 point samples do not cover every pixel or every frame.',
                               'White/brightness jumps may be authored transitions, not defects.',
                               'Readback/telemetry perturbs timing; this is not a performance gate.',
                               'Native-source samples do not measure compositor output.'])
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
        for i,line in enumerate(lines):
            if not line.startswith(PREFIX): continue
            if not start<i<end: raise ValueError('Pixel telemetry outside run interval')
            row=fields(line)
            if row.get('status')=='reset': previous.clear()
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
        report['status']='observed' if report['streams'] else 'inconclusive'
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
