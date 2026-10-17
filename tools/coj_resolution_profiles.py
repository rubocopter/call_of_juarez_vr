"""Calculate startup resolution proposals. This tool never writes game settings."""
import argparse
from datetime import datetime, timedelta, timezone
from decimal import Decimal, localcontext, ROUND_FLOOR
import json
from pathlib import Path
import re
import sys

RATIOS={'Auto':Decimal(1),'75%':Decimal('.75'),'100%':Decimal(1),'125%':Decimal('1.25')}
INT32_MAX=2**31-1
UINT64_MAX=2**64-1
GATES=['exact_game_build','recommendation_application_identity','native_adapter_format_viewport',
       'shared_transport_and_padded_theater','x86_address_space_and_loading_peaks',
       'video_transaction_and_conflict_preservation','effective_native_raster_and_headset_validation']

def load_observation(text):
    if not isinstance(text,str) or len(text.encode('utf-8'))>65536:
        raise ValueError('Observation exceeds 64 KiB')
    # Windows PowerShell 5.1 can prefix redirected stdin with a UTF-8 BOM.
    # Match the existing utf-8-sig file input without relaxing JSON validation.
    if text.startswith('\ufeff'): text=text[1:]
    def unique(pairs):
        value={}
        for key,item in pairs:
            if key in value: raise ValueError('Duplicate observation key: '+key)
            value[key]=item
        return value
    def reject_constant(value): raise ValueError('Nonfinite JSON constant: '+value)
    result=json.loads(text,object_pairs_hook=unique,parse_constant=reject_constant)
    if not isinstance(result,dict): raise ValueError('Observation must be a JSON object')
    return result

def dimension(value):
    if type(value) is not int or not 1<=value<=INT32_MAX:
        raise ValueError('Dimensions must be positive native 32-bit integers')
    return value

def validate_observation(record,expected_preparation_id,now):
    if not isinstance(record,dict) or type(record.get('schemaVersion')) is not int or record['schemaVersion']!=1:
        raise ValueError('Unsupported recommendation observation schema')
    if record.get('recordType')!='cojvr-resolution-observation': raise ValueError('Foreign observation type')
    if record.get('identityQualified') is not False:
        raise ValueError('Background observation cannot qualify the game application identity')
    if not isinstance(expected_preparation_id,str) or not re.fullmatch(r'[A-Za-z0-9._-]{1,80}',expected_preparation_id):
        raise ValueError('An expected preparation identity is required')
    if record.get('preparationId')!=expected_preparation_id: raise ValueError('Foreign preparation identity')
    timestamp=record.get('observedUtc')
    if not isinstance(timestamp,str) or len(timestamp)>40: raise ValueError('Missing observation timestamp')
    observed=datetime.fromisoformat(timestamp.replace('Z','+00:00'))
    if observed.tzinfo is None or observed.utcoffset()!=timedelta(0): raise ValueError('Timestamp must be UTC')
    age=(now-observed).total_seconds()
    if not 0<=age<=120: raise ValueError('Recommendation is stale or from the future')
    if record.get('status')!='observed': return None
    for field in ('runtimeVersion','hmdSerial','hmdModel'):
        value=record.get(field)
        if not isinstance(value,str) or not value or len(value)>1024: raise ValueError('Missing/bounded identity: '+field)
    if type(record.get('adapterIndex')) is not int or not 0<=record['adapterIndex']<=INT32_MAX:
        raise ValueError('Invalid runtime adapter identity')
    for field in ('applicationKey','applicationKeyStatus'):
        if not isinstance(record.get(field),str) or len(record[field])>1024:
            raise ValueError('Missing/bounded application identity status')
    return dimension(record.get('recommendedWidth')),dimension(record.get('recommendedHeight'))

def resolve(profile,*,observation=None,expected_preparation_id=None,width=None,height=None,now=None):
    report=dict(schemaVersion=1,recordType='cojvr-resolution-proposal',requestedProfile=profile,
        status='rejected',candidateDimensions=None,applyAllowed=False,
        effectiveEngineDimensions=None,effectiveEyeDimensions=None,compositorSubmitDimensions=None,
        fallback='preserve_selected_native_video',unresolvedGates=GATES.copy(),errors=[],
        payloadLowerBoundBytes=None,totalMemoryEstimateBytes=None,
        limitations=['Proposal only: no settings, render targets or compositor output were changed.',
          '32*W*H counts six 4-byte capture textures and two native presenter textures only.',
          'Native color/depth/effects, padded theater, mono readback, driver copies and overlapping lifetimes are additional.',
          'Payload is not x86 virtual-address capacity, a contiguous-allocation test or a frame-time prediction.',
          'A helper recommendation is not qualified as the game application recommendation.'])
    try:
        if profile not in (*RATIOS,'Manual'): raise ValueError('Unknown resolution profile')
        if profile=='Manual':
            if observation is not None: raise ValueError('Manual dimensions do not consume a recommendation')
            extent=(dimension(width),dimension(height))
            report['unresolvedGates'].remove('recommendation_application_identity')
        else:
            if width is not None or height is not None: raise ValueError('Only Manual accepts explicit dimensions')
            if observation is None:
                report.update(status='unavailable',reason='fresh_recommendation_required')
                return report
            extent=validate_observation(observation,expected_preparation_id,now or datetime.now(timezone.utc))
            report['observation']=observation
            if extent is None:
                report.update(status='unavailable',reason='helper_recommendation_unavailable')
                return report
            report['recommendedDimensions']=list(extent)
            ratio=RATIOS[profile]
            with localcontext() as context:
                context.prec=60
                scale=ratio.sqrt()
                extent=tuple(dimension(int((Decimal(axis)*scale+Decimal('.5')).to_integral_value(rounding=ROUND_FLOOR)))
                             for axis in extent)
            report.update(pixelRatio=float(ratio),linearScale=str(scale))
        pixels=extent[0]*extent[1]
        if pixels>UINT64_MAX//32: raise ValueError('Nominal capture/output payload exceeds uint64')
        report.update(status='proposal_only',candidateDimensions=list(extent),pixelCount=pixels,
                      payloadLowerBoundBytes=32*pixels)
    except (ValueError,TypeError,KeyError,OverflowError) as error:
        report['errors'].append(str(error))
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=[*RATIOS,'Manual'],required=True)
    parser.add_argument('--observation',type=Path)
    parser.add_argument('--preparation-id')
    parser.add_argument('--width',type=int)
    parser.add_argument('--height',type=int)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:
        stdin_observation=args.observation is not None and str(args.observation)=='-'
        if args.observation and not stdin_observation and args.observation.stat().st_size>65536: raise ValueError('Observation exceeds 64 KiB')
        observation=load_observation(sys.stdin.read(65537) if stdin_observation else
            args.observation.read_text(encoding='utf-8')) if args.observation else None
        report=resolve(args.profile,observation=observation,expected_preparation_id=args.preparation_id,
                       width=args.width,height=args.height)
    except (OSError,ValueError) as error:
        report=dict(schemaVersion=1,status='rejected',applyAllowed=False,
                    fallback='preserve_selected_native_video',errors=[str(error)])
    output=json.dumps(report,indent=2,allow_nan=False)+'\n'
    # Protect read-only observation input against an accidentally reused output path.
    if args.output and args.observation and str(args.observation)!='-' and args.output.resolve()==args.observation.resolve():
        parser.error('--output must differ from the observation input')
    if args.output: args.output.write_text(output,encoding='utf-8')
    print(output,end='')
    return 1 if report['status']=='rejected' else 0

if __name__=='__main__': raise SystemExit(main())
