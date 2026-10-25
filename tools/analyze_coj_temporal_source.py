"""Inventoried HDR color/alpha content evidence, never temporal admission."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
from analyze_coj_manual_reload import verify_inventory
from analyze_coj_render_resources import unique_object,reject_constant
START=re.compile(r'^run_start: run_id=(\S+) build_manifest_id=([A-Fa-f0-9]{64})(?: |$)')
END=re.compile(r'^run_end: run_id=(\S+)$')
PREFIX='native_hdr_alpha_probe: '
SCOPE='native_hdr_source_scope: '
QUEUE='native_hdr_alpha_queue: '
WRITE_FIELDS=('sourceWriteCalls','sourceAlphaWriteCalls','sourceLastWriterShader',
              'sourceWriteObservationComplete')
COPY_FIELDS=('sourceCopyCalls','sourceLastCopySource','sourceLastCopySourceFormat',
             'sourceCopyRectanglesObserved','sourceLastCopySourceRect','sourceLastCopyDestinationRect')

def alpha_blend_observation(producer):
    """First observed blend draw only; effective alpha algebra, never depth/coverage.

    Shader bytes come from a COM-owned program at that draw, not a later pointer
    lookup. A full bounded copy permits a stable SHA-256, not material admission.
    """
    unknown=dict(available=False,observed=False,alphaOperation='unknown',shaderSha256=None,
                 additionalBlendDrawCalls=None)
    if 'firstAlphaBlend' not in producer:return unknown
    b=producer['firstAlphaBlend']
    if not isinstance(b,dict):raise ValueError('Invalid firstAlphaBlend')
    for key in ('observed','stateComplete','separateAlpha','shaderComplete'):
        if type(b.get(key)) is not bool:raise ValueError('Invalid blend '+key)
    integer(b,'shader',0)
    for key in ('sourceBlend','destinationBlend'):integer(b,key,0,17)
    integer(b,'blendOperation',0,5)
    for key in ('blendFactor','shaderBytes'):integer(b,key,0,2**32-1)
    code=b.get('bytecode')
    if not isinstance(code,list) or len(code)>1024 or any(type(v) is not int or not 0<=v<2**32 for v in code):
        raise ValueError('Invalid bounded blend shader bytecode')
    if b['observed']!=bool(producer['alphaBlendDrawCalls']):
        raise ValueError('Blend witness without matching alpha blend draw')
    if not b['observed'] and (any(b[k] for k in b if k!='bytecode') or code):
        raise ValueError('Unobserved blend witness has state/program')
    if b['stateComplete'] and (not b['sourceBlend'] or not b['destinationBlend'] or not b['blendOperation']):
        raise ValueError('Complete blend state has missing factors/operation')
    if b['shaderComplete']:
        if b['shaderBytes']!=len(code)*4 or bool(b['shader'])!=bool(code):
            raise ValueError('Complete blend shader byte count/identity mismatch')
    elif code:raise ValueError('Incomplete blend shader includes purported full bytecode')
    operation='unknown'
    if b['stateComplete']:
        src,dst,op=b['sourceBlend'],b['destinationBlend'],b['blendOperation']
        if (src,dst)==(1,2) and op in (1,3):operation='destination_preserved'
        elif (src,dst)==(2,1) and op in (1,2):operation='source_replaced'
        else:operation='other'
    return dict(b,available=True,alphaOperation=operation,
        shaderSha256=hashlib.sha256(struct.pack('<'+'I'*len(code),*code)).hexdigest() if code and b['shaderComplete'] else None,
        additionalBlendDrawCalls=producer['alphaBlendDrawCalls']-int(b['observed']),
        coverage='first_blend_enabled_alpha_draw_only')

def producer_observation(event):
    """Optional direct-RT0 producer intents frozen at the native copy boundary.

    Observation completeness covers this bounded reader only. It cannot prove
    fragment coverage, alpha semantics, source lifetime, MRT or other API writes.
    """
    p=event.get('sourceCopyProducer')
    if 'sourceCopyProducer' not in event:
        return dict(available=False,observed=False,complete=False,surface=None,drawCalls=None,
            alphaDrawCalls=None,rgbOnlyDrawCalls=None,alphaBlendDrawCalls=None,clearCalls=None,
            firstAlphaBlend=alpha_blend_observation({}))
    counts=('surface','drawCalls','alphaDrawCalls','alphaBlendDrawCalls','clearCalls',
            'firstAlphaShader','lastAlphaShader')
    if not isinstance(p,dict):raise ValueError('Invalid sourceCopyProducer')
    for key in counts:integer(p,key,0)
    if type(p.get('observationComplete')) is not bool:raise ValueError('Invalid producer observationComplete')
    if p.get('coverage')!='same_extent_fp16_rt0_draw_clear_only':raise ValueError('Invalid producer coverage')
    if p['alphaDrawCalls']>p['drawCalls'] or p['alphaBlendDrawCalls']>p['alphaDrawCalls']:
        raise ValueError('Inconsistent producer alpha draw counters')
    observed=bool(p['surface'])
    if observed and (p['surface']!=event.get('sourceLastCopySource') or
                     not event.get('sourceCopyCalls') or event.get('sourceLastCopySourceFormat')!=113):
        raise ValueError('Producer surface does not match native FP16 copy source')
    if (not observed and any(p[k] for k in counts[1:])) or (
            p['observationComplete'] and (not observed or not (p['drawCalls']+p['clearCalls']))):
        raise ValueError('Producer completeness/counters without observed surface writes')
    if not p['alphaDrawCalls'] and (p['firstAlphaShader'] or p['lastAlphaShader']):
        raise ValueError('Producer shader without alpha draw')
    return dict(available=True,observed=observed,complete=p['observationComplete'],surface=p['surface'],
        drawCalls=p['drawCalls'] if observed else None,alphaDrawCalls=p['alphaDrawCalls'] if observed else None,
        rgbOnlyDrawCalls=p['drawCalls']-p['alphaDrawCalls'] if observed else None,
        alphaBlendDrawCalls=p['alphaBlendDrawCalls'] if observed else None,clearCalls=p['clearCalls'] if observed else None,
        firstAlphaShader=p['firstAlphaShader'] if observed else None,lastAlphaShader=p['lastAlphaShader'] if observed else None,
        firstAlphaBlend=alpha_blend_observation(p),coverage=p['coverage'],semantics='successful_rt0_call_intent_only')

def producer_snapshot_changed(before,after,copy_delta):
    # Both values are frozen at the native copy, not running producer totals.
    return (copy_delta==0 and 'sourceCopyProducer' in before and 'sourceCopyProducer' in after
            and before['sourceCopyProducer']!=after['sourceCopyProducer'])

def copy_observation(event):
    """Typed native-copy evidence, independently optional on historical records."""
    for key in COPY_FIELDS[:3]+('sourceCopyWriteCall','sourceLastCopySourceWidth','sourceLastCopySourceHeight'):
        if key in event:integer(event,key,0,2**32-1 if key.endswith(('Format','Width','Height')) else 2**64-1)
    for key in ('sourceCopyRectanglesObserved','sourceCopyBoundaryCurrent'):
        if key in event and type(event[key]) is not bool:raise ValueError('Invalid '+key)
    for key in COPY_FIELDS[4:]:
        if key not in event:continue
        rect=event[key]
        if not isinstance(rect,list) or len(rect)!=4 or any(type(v) is not int or not -2**31<=v<2**31 for v in rect):
            raise ValueError('Invalid '+key)
        if event.get('sourceCopyRectanglesObserved') and not (0<=rect[0]<rect[2] and 0<=rect[1]<rect[3]):
            raise ValueError('Empty/reversed source copy rectangle')
    count=event.get('sourceCopyCalls')
    if count is not None:
        for key in WRITE_FIELDS[:2]:
            if key in event and count>event[key]:raise ValueError('Copy calls exceed source/alpha write calls')
        if count==0 and event.get('sourceCopyRectanglesObserved'):raise ValueError('Copy rectangles without copy')
    stamp=event.get('sourceCopyWriteCall')
    if stamp is not None and 'sourceWriteCalls' in event and stamp>event['sourceWriteCalls']:
        raise ValueError('Copy write ordinal exceeds source writes')
    available=all(k in event for k in COPY_FIELDS)
    width,height=event.get('width'),event.get('height')
    if width is None:width,height=event.get('sourceLastCopySourceWidth'),event.get('sourceLastCopySourceHeight')
    full=bool(available and count and event['sourceCopyRectanglesObserved'] and width and height and
              event['sourceLastCopySourceRect']==event['sourceLastCopyDestinationRect']==[0,0,width,height])
    extent_verified=bool(full and event.get('sourceLastCopySourceWidth')==width and
                         event.get('sourceLastCopySourceHeight')==height)
    current=event.get('sourceCopyBoundaryCurrent')
    if current and not (full and extent_verified and 0<width<=8192 and 0<height<=8192 and
                       event.get('sourceLastCopySource') and event.get('sourceLastCopySourceFormat')==113 and
                       stamp and stamp==event.get('sourceWriteCalls') and
                       event.get('sourceLastWriterShader')==0 and write_observation(event)['complete']):
        raise ValueError('Current-copy boundary contradicts retained witness')
    return dict(available=available,copyCalls=count,source=event.get('sourceLastCopySource'),
        sourceFormat=event.get('sourceLastCopySourceFormat'),fullRectangles=full,
        sourceExtentVerified=extent_verified,copyWriteCall=stamp,boundaryCurrent=current,
        semantics='native_copy_observation_only')

def write_observation(event):
    """Optional successful-call intents; never pixel coverage or depth evidence."""
    for key in WRITE_FIELDS[:3]:
        if key in event:integer(event,key,0)
    if WRITE_FIELDS[3] in event and type(event[WRITE_FIELDS[3]]) is not bool:
        raise ValueError('Invalid sourceWriteObservationComplete')
    calls,alpha=event.get(WRITE_FIELDS[0]),event.get(WRITE_FIELDS[1])
    if calls is not None and alpha is not None and alpha>calls:
        raise ValueError('Alpha write calls exceed source write calls')
    available=all(k in event for k in WRITE_FIELDS)
    return dict(available=available,complete=available and event[WRITE_FIELDS[3]],
        calls=calls,alphaCalls=alpha,lastWriterShader=event.get(WRITE_FIELDS[2]),
        semantics='successful_call_write_intent_only')

def write_delta(before,after,correlated=True):
    if not before or not after or not before['available'] or not after['available']:
        return dict(status='unavailable',complete=False,writeCalls=None,alphaWriteCalls=None)
    calls=after['calls']-before['calls'];alpha=after['alphaCalls']-before['alphaCalls']
    if calls<0 or alpha<0 or alpha>calls:raise ValueError('Inconsistent source write-call counters')
    complete=correlated and before['complete'] and after['complete']
    return dict(status='complete' if complete else 'incomplete',complete=complete,
                writeCalls=calls,alphaWriteCalls=alpha)

def grid_bounds(indices):
    return ([min(i%16 for i in indices),min(i//16 for i in indices),
             max(i%16 for i in indices),max(i//16 for i in indices)] if indices else None)

def compare_samples(left,right,kind):
    """Compare finite, co-located *logged* texels, without inferring freshness/depth."""
    a,b=left['event'],right['event']
    comparable=[];same=[];changed=[];alpha_changed=[];rgb_changed=[]
    for i in range(256):
        ar,br=a['rgb'][i*3:i*3+3],b['rgb'][i*3:i*3+3]
        if any(v is None for v in [a['alpha'][i],b['alpha'][i],*ar,*br]):continue
        comparable.append(i)
        if a['alpha'][i]!=b['alpha'][i]:alpha_changed.append(i)
        if ar!=br:rgb_changed.append(i)
        (same if a['alpha'][i]==b['alpha'][i] and ar==br else changed).append(i)
    reset=a['eye']!=b['eye']
    delta=write_delta(write_observation(a),write_observation(b),
                      left['scopeComplete'] and right['scopeComplete']) if not reset else write_delta(None,None)
    return dict(kind=kind,frame=a['frame'],generation=a['generation'],pose=a['pose'],
        owner=a['owner'],device=a['device'],source=a['source'],
        fromLine=left['line'],toLine=right['line'],fromEye=a['eye'],toEye=b['eye'],
        fromConsumer=a['consumer'],toConsumer=b['consumer'],
        comparablePixels=len(comparable),identicalRgbaPixels=len(same),
        changedRgbaPixels=len(changed),changedAlphaPixels=len(alpha_changed),
        changedRgbPixels=len(rgb_changed),identicalGridBounds=grid_bounds(same),
        changedGridBounds=grid_bounds(changed),
        identicalRows=[y for y in range(16) if all(y*16+x in same for x in range(16))],
        changedRows=sorted({i//16 for i in changed}),
        sourceWriteCountersResetPerEye=reset,sourceWriteCallDelta=delta['writeCalls'],
        sourceAlphaWriteCallDelta=delta['alphaWriteCalls'],sourceWriteObservationComplete=delta['complete'],
        contentChangedWithoutObservedWriteCalls=(bool(changed) and delta['writeCalls']==0
                                                if delta['complete'] else None))

def parse_queue(line):
    fields={}
    for part in line[len(QUEUE):].split(';'):
        key,sep,value=part.partition('=')
        if not sep or key in fields:raise ValueError('Malformed queue event')
        fields[key]=value
    required={'status','phase','sampler','eye','frame','hr','temporal_admitted'}
    if set(fields) not in (required,required|{'consumer'}):raise ValueError('Invalid queue fields')
    if fields['temporal_admitted']!='false':raise ValueError('Temporal admission overclaim')
    if fields['status'] not in ('queued','skipped') or fields['phase']!='matching_native_sampler_draw':
        raise ValueError('Unknown queue result/phase')
    for key in ('frame','eye','sampler','consumer','hr'):
        if key not in fields:continue # Legacy alpha-only receipts have no phase ordinal.
        if not re.fullmatch(r'\d+',fields[key]):raise ValueError('Invalid queue '+key)
        fields[key]=int(fields[key])
    for key,low,high in (('frame',1,2**64-1),('eye',0,1),('sampler',0,15),('hr',0,2**32-1)):
        integer(fields,key,low,high)
    if 'consumer' in fields:integer(fields,'consumer',1,3)
    if fields['status']=='queued' and fields['hr']!=0:raise ValueError('Queued with failed HRESULT')
    return fields

def integer(obj,key,low=1,high=2**64-1):
    value=obj.get(key)
    if type(value) is not int or not low<=value<=high:raise ValueError('Invalid '+key)
    return value
def vector(obj,key,count):
    values=obj.get(key)
    if not isinstance(values,list) or len(values)!=count:raise ValueError('Invalid '+key+' sample grid')
    if any(v is not None and (type(v) not in (int,float) or not math.isfinite(v)) for v in values):
        raise ValueError('Invalid '+key+' value')
    return values
def analyze(lines,run_id,build_id):
    rows=[];scopes={};queues=[];errors=[];starts=ends=0;running=False
    for number,line in enumerate(lines,1):
        line=line.rstrip('\r\n')
        if line.startswith('run_start:'):
            starts+=1;match=START.fullmatch(line) or START.match(line)
            if not match or match.groups()!=(run_id,build_id):errors.append('Foreign/malformed start')
            running=True
        elif line.startswith('run_end:'):
            ends+=1;match=END.fullmatch(line)
            if not match or match[1]!=run_id:errors.append('Foreign/malformed end')
            if not running:errors.append('End before start')
            running=False
        elif line.startswith((PREFIX,SCOPE,QUEUE)):
            try:
                if not running:raise ValueError('Observation outside run interval')
                if len(line)>64*1024:raise ValueError('Oversized sample')
                if line.startswith(QUEUE):
                    if len(queues)>=60:raise ValueError('Queue budget exceeded')
                    queues.append(dict(line=number,event=parse_queue(line)));continue
                event=json.loads(line.split(': ',1)[1],object_pairs_hook=unique_object,parse_constant=reject_constant)
                if not isinstance(event,dict):raise ValueError('Invalid observation object')
                if event.get('temporalAdmitted') is not False:raise ValueError('Temporal admission overclaim')
                if event.get('sourceQualified',False) is not False:raise ValueError('Source qualification overclaim')
                for key in ('frame','generation','scope','owner'):integer(event,key)
                integer(event,'eye',0,1)
                observation=write_observation(event)
                copy_witness=copy_observation(event)
                producer=producer_observation(event)
                if line.startswith(SCOPE):
                    scope=event['scope']
                    if scope in scopes or len(scopes)>=20:raise ValueError('Duplicate/excess source scope')
                    if type(event.get('complete')) is not bool:raise ValueError('Invalid scope completion')
                    integer(event,'attemptedConsumers',0,3);scopes[scope]=event;continue
                if len(rows)>=60:raise ValueError('Source sample budget exceeded')
                for key in ('device','source'):integer(event,key)
                for key in ('width','height'):integer(event,key,1,8192)
                if event.get('sampling')!='16x16_point':raise ValueError('Unknown source grid')
                alpha=vector(event,'alpha',256);finite=[v for v in alpha if v is not None]
                new=event.get('scopeKind')=='hdr_source'
                if new:
                    integer(event,'pose');integer(event,'consumer',1,3);integer(event,'sampler',0,15)
                    integer(event,'target');integer(event,'shader');vector(event,'rgb',768)
                elif any(k in event for k in ('pose','consumer','rgb')):raise ValueError('Incomplete consumer witness')
                key=tuple(event.get(k,0) for k in ('frame','generation','owner','device','source','eye','consumer','pose'))
                if any(row['key']==key for row in rows):raise ValueError('Duplicate source identity')
                if new and any(row['consumerWitness'] and row['event']['scope']==event['scope'] and
                               row['event']['consumer']==event['consumer'] for row in rows):
                    raise ValueError('Duplicate scope consumer')
                ones=[i for i,v in enumerate(alpha) if v==1]
                rows.append(dict(line=number,key=key,event=event,consumerWitness=new,sourceWriteObservation=observation,
                    sourceCopyObservation=copy_witness,sourceCopyProducerObservation=producer,postCopyStableObservation=False,
                    statistics=dict(finite=len(finite),nonfinite=256-len(finite),nonpositive=sum(v<=0 for v in finite),
                        minimum=min(finite) if finite else None,maximum=max(finite) if finite else None,
                        exactlyOne=len(ones),far65504=sum(v==65504 for v in finite),
                        oneGridBounds=grid_bounds(ones))))
            except (ValueError,KeyError,TypeError,OverflowError,RecursionError) as error:
                errors.append(f'Line {number}: {error}')
    if starts!=1 or ends!=1 or running:errors.append('Require one complete correlated run')
    groups={};by_scope={};failures=[]
    for row in rows:
        event=row['event'];scope=scopes.get(event['scope']) if row['consumerWitness'] else None
        row['scopeComplete']=bool(scope and scope['complete'] and all(scope[k]==event[k] for k in ('frame','owner','generation','eye')))
        try:
            row['sourceWritesAfterSample']=write_delta(row['sourceWriteObservation'],
                write_observation(scope) if scope else None,row['scopeComplete'])
        except ValueError as error:
            errors.append(f"Line {row['line']}: {error}")
            row['sourceWritesAfterSample']=write_delta(None,None)
        row['sourceCopiesAfterSample']=None
        if scope and row['sourceCopyObservation']['available'] and all(k in scope for k in COPY_FIELDS):
            delta=scope['sourceCopyCalls']-event['sourceCopyCalls']
            writes_after=row['sourceWritesAfterSample']['writeCalls']
            if delta<0 or (writes_after is not None and delta>writes_after):
                errors.append(f"Line {row['line']}: Inconsistent source copy-call counters")
            else:row['sourceCopiesAfterSample']=delta
            if producer_snapshot_changed(event,scope,delta):
                errors.append(f"Line {row['line']}: Producer snapshot changed without native copy before scope end")
        if row['consumerWitness']:
            by_scope.setdefault(event['scope'],[]).append(row)
            if not scope:
                failures.append(dict(reason='missing_scope_end',line=row['line'],scope=event['scope']))
            elif any(scope[k]!=event[k] for k in ('frame','owner','generation','eye')) or event['consumer']>scope['attemptedConsumers']:
                errors.append(f"Line {row['line']}: Sample/scope identity or consumer mismatch")
        key=tuple(event.get(k,0) for k in ('frame','generation','owner','device','source','consumer','pose',
                                         'target','shader','sampler','width','height'))+(row['consumerWitness'],)
        groups.setdefault(key,[]).append(row)
        row.pop('key')
    pairs=[];unpaired=[]
    for key,eyes in groups.items():
        if len(eyes)!=2 or {r['event']['eye'] for r in eyes}!={0,1}:
            unpaired.extend(dict(line=r['line'],reason='No matching eye/pose/consumer signature') for r in eyes)
            continue
        eyes.sort(key=lambda r:r['event']['eye']);left,right=eyes
        extent=all(left['event'][k]==right['event'][k] for k in ('width','height'))
        pairs.append(dict(frame=key[0],generation=key[1],consumer=key[5],pose=key[6],
            owner=key[2],device=key[3],source=key[4],target=key[7],shader=key[8],sampler=key[9],
            sampleLines=[r['line'] for r in eyes],consumerWitness=all(r['consumerWitness'] for r in eyes),
            sourceWriteObservationComplete=all(r['sourceWritesAfterSample']['complete'] for r in eyes),
            eyeWritesAfterSample=[r['sourceWritesAfterSample'] for r in eyes],
            extentMatched=extent,scopeComplete=all(r['scopeComplete'] for r in eyes),
            eyeStatistics=[r['statistics'] for r in eyes],temporalAdmitted=False))

    scope_reports=[];comparisons=[];source_groups={};selected=[]
    for identity,scope in scopes.items():
        samples=sorted(by_scope.get(identity,[]),key=lambda r:r['event']['consumer'])
        delivered=[r['event']['consumer'] for r in samples]
        missing=sorted(set(range(1,scope['attemptedConsumers']+1))-set(delivered))
        previous=None;latest_copy=False
        for row in samples:
            co=row['sourceCopyObservation'];wo=row['sourceWriteObservation']
            proved=bool(co['available'] and co['copyCalls'] and wo['complete'] and wo['calls']==co['copyCalls'])
            if previous and all(previous['event'][k]==row['event'][k] for k in ('device','source','pose','width','height')):
                pc,pw=previous['sourceCopyObservation'],previous['sourceWriteObservation']
                if pc['available'] and co['available'] and pw['complete'] and wo['complete']:
                    dw,dc=wo['calls']-pw['calls'],co['copyCalls']-pc['copyCalls']
                    if dc<0 or dc>dw:errors.append(f"Line {row['line']}: Inconsistent consumer copy-call delta")
                    if producer_snapshot_changed(previous['event'],row['event'],dc):
                        errors.append(f"Line {row['line']}: Producer snapshot changed without native copy between consumers")
                    proved=proved or (dw>0 and dw==dc) or (dw==dc==0 and latest_copy)
            latest_copy=co['boundaryCurrent'] if co['boundaryCurrent'] is not None else proved
            row['postCopyStableObservation']=bool(latest_copy and wo['lastWriterShader']==0 and
                row['scopeComplete'] and co['fullRectangles'] and co['sourceFormat']==113 and
                row['sourceWritesAfterSample']['complete'] and row['sourceWritesAfterSample']['writeCalls']==0 and
                row['sourceCopiesAfterSample']==0)
            previous=row
        eligible=[r for r in samples if r['postCopyStableObservation']]
        if eligible:selected.append(eligible[0])
        scope_reports.append(dict(event=scope,sourceWriteObservation=write_observation(scope),
            sourceCopyObservation=copy_observation(scope),
            sourceCopyProducerObservation=producer_observation(scope),
            deliveredConsumers=delivered,missingConsumers=missing,
            deliveryComplete=scope['complete'] and not missing and all(r['scopeComplete'] for r in samples)))
        if missing:failures.append(dict(reason='missing_consumer_readback',scope=identity,consumers=missing))
        if not scope['complete']:failures.append(dict(reason='incomplete_scope',scope=identity))
        for a,b in zip(samples,samples[1:]):
            # Same scope is not enough if the bound source or render pose changed.
            if all(a['event'][k]==b['event'][k] for k in ('frame','generation','owner','device','source','pose','width','height')):
                try:comparisons.append(compare_samples(a,b,'within_eye_consumers'))
                except ValueError as error:errors.append(f"Line {b['line']}: {error}")
    for row in rows:
        if not row['consumerWitness']:continue
        event=row['event']
        key=tuple(event[k] for k in ('frame','generation','owner','device','source','pose','width','height'))
        source_groups.setdefault(key,{0:[],1:[]})[event['eye']].append(row)
    for eyes in source_groups.values():
        if not eyes[0] or not eyes[1]:continue
        left=max(eyes[0],key=lambda r:r['event']['consumer'])
        right=min(eyes[1],key=lambda r:r['event']['consumer'])
        # Compare only actual terminal/first phases; missing readbacks must not
        # silently substitute an earlier/later consumer for those boundaries.
        ls,rs=(scopes.get(r['event']['scope']) for r in (left,right))
        if (ls and rs and left['event']['consumer']==ls['attemptedConsumers'] and
                right['event']['consumer']==1 and left['scopeComplete'] and right['scopeComplete']):
            comparisons.append(compare_samples(left,right,'left_terminal_to_right_first'))

    selected_lines={r['line'] for r in selected}
    selected_pairs=[p for p in pairs if all(n in selected_lines for n in p['sampleLines'])]
    missing_eyes=[]
    scope_eyes={}
    for s in scopes.values():
        key=tuple(s[k] for k in ('frame','generation','owner'))
        scope_eyes.setdefault(key,set()).add(s['eye'])
    for key,eyes in scope_eyes.items():
        if eyes!={0,1}:missing_eyes.append(dict(frame=key[0],generation=key[1],owner=key[2],missingEyes=sorted({0,1}-eyes)))

    # Receipts lack owner/generation. Correlate only unique frame/eye/ordinal
    # candidates, never guess across two scopes with the same reduced identity.
    seen_queues=set()
    for queue in queues:
        q=queue['event'];key=(q['frame'],q['eye'],q.get('consumer',0))
        if key in seen_queues:errors.append(f"Line {queue['line']}: Ambiguous/duplicate queue identity")
        seen_queues.add(key)
        candidates=[r for r in rows if (r['event']['frame'],r['event']['eye'],r['event'].get('consumer',0))==key]
        if len(candidates)>1:errors.append(f"Line {queue['line']}: Ambiguous queue readback")
        if candidates and any(r['consumerWitness'] and r['event']['sampler']!=q['sampler'] for r in candidates):
            errors.append(f"Line {queue['line']}: Queue/readback sampler mismatch")
        queue['sampleLines']=[r['line'] for r in candidates]
        if q['status']=='skipped':
            failures.append(dict(reason='queue_skipped',line=queue['line'],event=q))
            if candidates:errors.append(f"Line {queue['line']}: Skipped queue has readback")
        elif not candidates:failures.append(dict(reason='queued_without_readback',line=queue['line'],event=q))
    summary=dict(grids=len(rows),frames=sorted({r['event']['frame'] for r in rows}),
        scopes=len(scopes),completeScopes=sum(s['complete'] for s in scopes.values()),
        deliveredScopes=sum(s['deliveryComplete'] for s in scope_reports),
        matchedConsumerPairs=sum(p['consumerWitness'] and p['scopeComplete'] for p in pairs),
        unpairedRows=len(unpaired),queueObserved=len(queues),
        postCopyStableSamples=sum(r['postCopyStableObservation'] for r in rows),
        selectedCopyBoundaryPairs=len(selected_pairs),missingEyeScopeFrames=len(missing_eyes),
        completeSourceWriteWitnessSamples=sum(r['sourceWritesAfterSample']['complete'] for r in rows),
        samplesWithLaterSourceWriteCalls=sum((r['sourceWritesAfterSample']['writeCalls'] or 0)>0 for r in rows),
        queueSkipped=sum(q['event']['status']=='skipped' for q in queues),
        copyProducerObservedSamples=sum(bool(r['sourceCopyProducerObservation']['surface']) for r in rows),
        completeCopyProducerSamples=sum(r['sourceCopyProducerObservation']['complete'] for r in rows),
        samplesWithProducerRgbOnlyDraws=sum((r['sourceCopyProducerObservation']['rgbOnlyDrawCalls'] or 0)>0 for r in rows),
        samplesWithProducerAlphaBlendDraws=sum((r['sourceCopyProducerObservation']['alphaBlendDrawCalls'] or 0)>0 for r in rows),
        queuedWithoutReadback=sum(q['event']['status']=='queued' and not q['sampleLines'] for q in queues))
    return dict(schema=1,status='rejected' if errors else 'observed' if rows else 'not_observed',
        runId=run_id,buildManifestId=build_id,coverage='sampled_hdr_content_only',
        temporalAdmitted=False,sourceQualified=False,headsetValidated=False,rows=rows,pairs=pairs,errors=errors,
        summary=summary,scopes=scope_reports,queues=queues,deliveryFailures=failures,
        unpairedRows=unpaired,sampleComparisons=comparisons,
        copyBoundarySelection=dict(selectedSampleLines=sorted(selected_lines),pairs=selected_pairs,missingEyes=missing_eyes,
            coverage='diagnostic_post_copy_stability_only',temporalAdmitted=False,depthQualified=False),
        depthAssessment=dict(status='unqualified',qualified=False,
            reason='Finite alpha and sampled RGBA agreement do not establish axial depth, coverage or same-eye freshness'),
        unresolved=['Axial-depth material and native hand/transparent/sky coverage',
                    'Full-size same-eye/frame/pose leases and controlled jitter',
                    'Dynamic motion/current-color masks and atomic reconstructed presentation'])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory',type=Path);parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();root=args.evidence_directory.resolve();output=args.output.resolve()
    if output.is_relative_to(root):parser.error('Output must be outside evidence')
    evidence,run,paths=verify_inventory(root)
    with paths['runtime/cojvr.log'].open(encoding='utf-8-sig') as stream:
        report=analyze(stream,run['runId'],run['buildManifestId'])
    with output.open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2,allow_nan=False)
    print(f"{report['status']}: {len(report['rows'])} grids, {len(report['pairs'])} pairs; temporalAdmitted=false")
    return 2 if report['errors'] else 0
if __name__=='__main__':raise SystemExit(main())
