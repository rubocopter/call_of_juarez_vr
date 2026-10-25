"""Synthetic diagnostics never qualify native ownership or headset behavior."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'tools/analyze_coj_render_resources.py'
spec = importlib.util.spec_from_file_location('resources', TOOL)
a = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a)
BUILD = 'a'*64
BASE = dict(schema=1, coverage='binding_state_only', readOnly=True, resizingAdmitted=False,
            thread=3, frame=9, owner=20, device=30, generation=1, deviceGeneration=1, route=0)
RESOURCE = dict(object=40, parent=50, surface=40, width=2560, height=1440,
                bound=True, valid=True, format=21, usage=1, pool=0, samples=0, quality=0,
                depth=0, levels=1, type=1)

def pair():
    events=[]
    for eye in (0,1):
        common=dict(BASE,eye=eye,scope=eye+1)
        events.append(dict(common,event='begin'))
        state=dict(valid=True,targetSlots=1,textureSlots=16,targets=[RESOURCE],
                   depth=dict(RESOURCE,object=60,parent=0,format=75),textures=[dict(bound=False,valid=True)]*16,
                   viewport=[0,0,2560,1440],viewportDepth=[0,1],scissor=[0,0,2560,1440],
                   z=[1,1,4],scissorEnabled=0,vertexShader=0,pixelShader=0,
                   transformsValid=True,transforms=[0]*32)
        events.append(dict(common,event='record',ordinal=1,repeat=2,op=6,selector=4,
                           hr=0,caller=77,source=0,destination=0,state=state))
        events.append(dict(common,event='end',calls=2,records=1,readFailures=0,
                           complete=True,truncated=False))
    return copy.deepcopy(events)

def log(events):
    return '\n'.join([f'run_start: run_id=synthetic build_manifest_id={BUILD} pid=1']+
                     ['render_resource_probe: '+json.dumps(e) for e in events]+['run_end: run_id=synthetic'])


def null_depth_pair():
    events=pair()
    for e in events:
        if e['event']!='record':continue
        state=e['state']
        state['targets'][0].update(width=1,height=1,format=int.from_bytes(b'NULL','little'))
        state['depth'].update(width=2048,height=2048,format=80,usage=2,parent=70)
        state['viewport']=[1,1,2046,2046]
        state['scissor']=[0,0,1,1]
    return events


def sprite_log(**changes):
    fields=dict(pass_sequence=7,frame_sequence=9,present_sequence=12,thread_id=3,
                owner='0x70',option=0,device='0x1e',rt='0x28',rt_hr='0x0',
                desc_hr='0x0',viewport_hr='0x0',generation=1,size='2560,1440',
                format=21,viewport='0,0,2560,1440',alpha_blend=1,src_blend=5,
                dst_blend=6,color_write=15,separate_alpha=0,depth=0)
    fields.update(changes)
    lines=[]
    for stage in ('sprites_begin','sprites_end','flush_begin','flush_end'):
        row=dict(fields)
        if stage.startswith('flush'):row['owner']='0x80'
        lines.append('native_hud_boundary: stage='+stage+';'+
                     ';'.join(f'{k}={v}' for k,v in row.items()))
    return lines


def with_sprites(lines=None,events=None):
    source=log(pair() if events is None else events)
    return source.replace('run_end:', '\n'.join(sprite_log() if lines is None else lines)+'\nrun_end:')

class ResourceAnalysisTests(unittest.TestCase):
    def reject(self,events):
        r=a.analyze(log(events));self.assertNotEqual(r['status'],'observed');self.assertFalse(r['resizingAdmitted'])
    def test_complete_pair_is_observation_only(self):
        r=a.analyze(log(pair()));self.assertEqual(r['status'],'observed')
        self.assertEqual(len(r['pairs']),1);self.assertFalse(r['resizingAdmitted'])
        self.assertEqual(r['scopes'][0]['drawCalls'],2)
        self.assertEqual(r['scopes'][0]['drawExtents'],[[2560,1440]])
        self.assertTrue(r['unresolved'])
    def test_missing_and_orphan(self):
        self.reject([]);self.reject(pair()[1:]);self.reject(pair()[:-1])
    def test_duplicate_and_reversed(self):
        events=pair();self.reject(events+events)
        self.reject(events[3:]+events[:3]);self.reject([events[0],events[2],events[1]]+events[3:])
    def test_identity_mismatch(self):
        for key in ('owner','device','generation','frame','route','thread'):
            events=pair()
            for e in events[3:]:e[key]+=1
            self.reject(events)
    def test_truncated_reads_and_forged_counts(self):
        for key,value in [('truncated',True),('complete',False),('readFailures',1),('calls',4),('records',0)]:
            events=pair();events[2][key]=value;self.reject(events)
        for key,value in [('repeat',0),('ordinal',2),('op',20),('hr',0x8876086C)]:
            events=pair();events[1][key]=value;self.reject(events)
        events=pair();events[1]['state']['valid']=False;self.reject(events)
    def test_claims_and_types_fail_closed(self):
        for key,value in [('readOnly',False),('resizingAdmitted',True),('eye',True),('scope',0),('schema',2)]:
            events=pair();events[0][key]=value;self.reject(events)
    def test_marker_and_outside_rejection(self):
        valid=log(pair())
        for text in (valid+'\nrun_end: run_id=synthetic',valid+'\n'+valid,
                     valid.replace('run_end: run_id=synthetic','run_end: run_id=other'),
                     'render_resource_probe: {}\n'+valid,valid+'\nrender_resource_probe: {}',
                     valid.replace(BUILD,'unbound'),valid+'\nrender_resource_probe_error: serialization_failed'):
            self.assertNotEqual(a.analyze(text)['status'],'observed')
    def test_bad_json_and_nonfinite(self):
        text=log(pair()).replace('"width": 2560','"width": NaN',1)
        self.assertNotEqual(a.analyze(text)['status'],'observed')
        self.assertNotEqual(a.analyze(log(pair())+'\nrender_resource_probe: {')['status'],'observed')
    def test_schema_requires_all_producer_fields(self):
        for field in ('scissor','scissorEnabled','z','viewportDepth','vertexShader','pixelShader','transforms','transformsValid'):
            events=pair();del events[1]['state'][field];self.reject(events)
        events=pair();events[1]['state']['textureSlots']=0;events[1]['state']['textures']=[];self.reject(events)
    def test_pair_frame_order_and_uniqueness(self):
        for next_frame in (9,8):
            events=pair();extra=pair()
            for e in extra:e['scope']+=2;e['frame']=next_frame
            self.reject(events+extra)
    def test_draw_state_requires_real_extent(self):
        for viewport in ([0,0,0,1440],[0,0,3000,1440]):
            events=pair();events[1]['state']['viewport']=viewport;self.reject(events)
        events=pair();events[1]['op']=0;self.reject(events)

    def test_null_color_pass_uses_verified_depth_extent(self):
        r=a.analyze(log(null_depth_pair()))
        self.assertEqual(r['status'],'observed',r['errors'])
        self.assertFalse(r['resizingAdmitted']);self.assertFalse(r['headsetValidated'])
        draw=r['scopes'][0]['drawPasses'][0]
        self.assertEqual(draw['kind'],'null_color_depth')
        self.assertEqual(draw['rasterExtent'],[2048,2048])
        self.assertEqual(draw['colorExtent'],[1,1])
        self.assertEqual(draw['viewport'],[1,1,2046,2046])
        self.assertEqual(draw['drawCalls'],2)
        self.assertTrue(r['unresolved'])

    def test_null_color_profile_rejects_unqualified_layouts(self):
        for key,value in [('bound',False),('format',75),('usage',1),('width',1024),
                          ('samples',2),('quality',1),('pool',2),('type',3)]:
            events=null_depth_pair();events[1]['state']['depth'][key]=value;self.reject(events)
        for key,value in [('width',2),('usage',0),('samples',2),('quality',1),('pool',2),('type',3)]:
            events=null_depth_pair();events[1]['state']['targets'][0][key]=value;self.reject(events)
        for z in ([0,1,4],[1,0,4],[2,1,4]):
            events=null_depth_pair();events[1]['state']['z']=z;self.reject(events)
        events=null_depth_pair();state=events[1]['state']
        state['targetSlots']=2;state['targets'].append(copy.deepcopy(RESOURCE));self.reject(events)
        events=null_depth_pair();events[1]['state']['viewport']=[1,1,2048,2048];self.reject(events)

    def test_ordinary_color_does_not_borrow_larger_depth_bounds(self):
        events=null_depth_pair();events[1]['state']['targets'][0]['format']=21
        self.reject(events)
        r=a.analyze(log(pair()));self.assertEqual(r['status'],'observed')
        self.assertEqual(r['scopes'][0]['drawPasses'][0]['kind'],'color')

    def test_sprite_order_is_separate_read_only_correlation(self):
        r=a.analyze(with_sprites());self.assertEqual(r['status'],'observed')
        hud=r['desktopSpriteOrdering'];self.assertEqual(hud['status'],'observed',hud['errors'])
        self.assertFalse(hud['mirrorAdmitted']);self.assertFalse(r['resizingAdmitted'])
        self.assertEqual(hud['passes'][0]['target'],40)
        self.assertGreater(hud['passes'][0]['beginLine'],r['scopes'][1]['endLine'])
        self.assertEqual(r['scopes'][1]['lastObservedTarget']['object'],40)
        self.assertEqual(hud['passes'][0]['spriteOwner'],112)
        self.assertEqual(hud['passes'][0]['flushOwner'],128)

    def test_sprite_missing_or_unavailable_does_not_invalidate_binding_evidence(self):
        for source in (log(pair()),with_sprites(sprite_log()[:2]),
                       with_sprites([s+';target_observation=unavailable' for s in sprite_log()])):
            r=a.analyze(source);self.assertEqual(r['status'],'observed')
            self.assertEqual(r['desktopSpriteOrdering']['status'],'incomplete')
            self.assertFalse(r['desktopSpriteOrdering']['mirrorAdmitted'])

    def test_sprite_wrong_target_context_and_queries_reject(self):
        for field,value in [('rt','0x99'),('device','0x99'),('generation',2),
                            ('thread_id',99),('rt_hr','0x8876086c'),('desc_hr','0x1'),
                            ('viewport_hr','0x8876086c'),('option',1),('format',113),
                            ('size','2804,2860'),('viewport','0,0,1,1')]:
            with self.subTest(field=field):
                r=a.analyze(with_sprites(sprite_log(**{field:value})))
                self.assertEqual(r['status'],'observed')
                self.assertEqual(r['desktopSpriteOrdering']['status'],'incomplete')

    def test_sprite_order_duplicate_nested_and_owner_changes_reject(self):
        good=sprite_log()
        cases=[list(reversed(good)),good+good,[good[0],good[0]]+good[1:],
               [good[0],good[1].replace('owner=0x70','owner=0x71')]+good[2:],
               good[:2]+[good[2].replace('present_sequence=12','present_sequence=13')]+good[3:],
               good[:1]+sprite_log(frame_sequence=8)+good[1:]]
        for lines in cases:
            self.assertEqual(a.analyze(with_sprites(lines))['desktopSpriteOrdering']['status'],'incomplete')

    def test_sprite_before_pair_or_outside_interval_reject(self):
        good='\n'.join(sprite_log())
        sources=[good+'\n'+log(pair()),log(pair())+'\n'+good,
                 log(pair()).replace('render_resource_probe:',good+'\nrender_resource_probe:',1)]
        for source in sources:
            r=a.analyze(source);self.assertEqual(r['status'],'observed')
            self.assertEqual(r['desktopSpriteOrdering']['status'],'incomplete')

    def test_sprite_correlation_reports_partial_sampling(self):
        extra=pair()
        for e in extra:e['scope']+=2;e['frame']+=1
        source=with_sprites().replace('run_end:', '\n'.join(
            'render_resource_probe: '+json.dumps(e) for e in extra)+'\nrun_end:')
        hud=a.analyze(source)['desktopSpriteOrdering']
        self.assertEqual(hud['status'],'observed',hud['errors'])
        self.assertEqual(hud['matchedFrames'],[9]);self.assertEqual(hud['unobservedFrames'],[10])

    def test_stale_sprite_frame_after_later_eye_scope_rejects(self):
        extra=pair()
        for e in extra:e['scope']+=2;e['frame']+=1
        r=a.analyze(with_sprites(events=pair()+extra))
        self.assertEqual(r['status'],'observed')
        self.assertEqual(r['desktopSpriteOrdering']['status'],'incomplete')

    def test_sprite_unrelated_frame_does_not_establish_order(self):
        hud=a.analyze(with_sprites(sprite_log(frame_sequence=8)))['desktopSpriteOrdering']
        self.assertEqual(hud['status'],'incomplete');self.assertEqual(hud['matchedFrames'],[])

    def test_legacy_probe_epoch_is_not_native_device_generation(self):
        events=pair()
        for e in events:
            del e['deviceGeneration'];e['generation']=4
        r=a.analyze(with_sprites(sprite_log(generation=2),events))
        self.assertEqual(r['status'],'observed')
        hud=r['desktopSpriteOrdering'];self.assertEqual(hud['status'],'incomplete')
        self.assertIn('Native device generation absent from eye scope',hud['errors'])
        self.assertEqual(len(hud['passes']),1)
        self.assertEqual(hud['passes'][0]['probeEpoch'],4)
        self.assertEqual(hud['passes'][0]['deviceGeneration'],2)
        self.assertFalse(hud['generationQualified']);self.assertFalse(hud['mirrorAdmitted'])

    def test_explicit_device_generation_must_be_complete_and_stable(self):
        for mutate in ('missing','changed','foreign_eye','boolean'):
            events=pair()
            if mutate=='missing':del events[1]['deviceGeneration']
            if mutate=='changed':events[1]['deviceGeneration']=2
            if mutate=='foreign_eye':
                for e in events[3:]:e['deviceGeneration']=2
            if mutate=='boolean':
                for e in events:e['deviceGeneration']=True
            self.reject(events)

    def test_sprite_bad_fields_and_over_budget_reject(self):
        for lines in ([s+';rt=0x28' for s in sprite_log()],
                      [s.replace('pass_sequence=7','pass_sequence=-1') for s in sprite_log()],
                      [s.replace('thread_id=3','thread_id=3junk') for s in sprite_log()],
                      [s.replace('stage=sprites_begin','stage=unknown') for s in sprite_log()],
                      [s for i in range(33) for s in sprite_log(pass_sequence=i+1)]):
            self.assertEqual(a.analyze(with_sprites(lines))['desktopSpriteOrdering']['status'],'incomplete')

def native_snapshot(scope=1, eye=0, phase='begin'):
    texture=dict(handle=0xe1000000,texture2d=50,textureCube=0,textureVolume=0,
                 usage=1,format=21,flags=0x219,width=2560,height=1440,
                 depth=1,selectedInterface=50,kind=0,requestedWidth=2560,
                 requestedHeight=1440,requestedDepth=1,
                 inverseSizeBits=[0x39cccccd,0x3a360b61,0x3f800000])
    def buffer(container,index,name,global_,handle=None):
        return dict(container=container,entry=container+4+index*0x64,index=index,
                    name=name,logicalWidth=2560,logicalHeight=1440,
                    rgbBits=24,alphaBits=8,widthDivisor=0,heightDivisor=0,
                    internalColor=9,depthAttachment=1,nativeBpp=1,tempShareable=1,
                    linkedBorrowed=7 if not global_ else 0,
                    texture=dict(texture,**(handle or {})),**{'global':global_})
    return dict(BASE,coverage='native_ppfx_structural_snapshot',scope=scope,eye=eye,
                phase=phase,bindingScopeComplete=phase=='end',complete=True,truncated=False,
                status=0,engineBase=0x90000000,renderer=0xe2000000,manager=0xe3000000,
                nativeDevice=30,rendererVtable=0x90312334,managerVtable=0x90314c48,
                listSentinel=0xe4000000,loadedEffectCount=1,
                globalBufferCount=1,managerDirty=0,effectCount=1,bufferCount=3,
                rendererState=dict(logicalWidth=2560,logicalHeight=1440,
                    viewportClampWidth=2560,viewportClampHeight=1440,currentTarget=0,
                    defaultColorSurface=40,defaultDepthSurface=60,internalColorSurface=40,
                    internalDepthSurface=60,additionalDepthSurface=0,placeholderTexture=50,
                    pixelSamplerHandles=[0]*16,vertexSamplerHandles=[0]*4),
                effects=[dict(node=0xe5000000,next=0xe4000000,previous=0xe4000000,
                              effect=0xe6000000,vtable=0x903180ac,bufferCount=2,name='data/scene30.ppfx')],
                buffers=[buffer(0xe3000000,0,'Global\xff',True),
                         buffer(0xe6000000,0,'NamedAlias',False),
                         buffer(0xe6000000,1,'Unused',False,
                                dict(handle=0xe1001000,texture2d=90,selectedInterface=90))])


def native_lines(events=None):
    """Native begin precedes binding begin; native end precedes binding end."""
    lines=[f'run_start: run_id=synthetic build_manifest_id={BUILD} pid=1']
    for event in pair() if events is None else events:
        if event['event']=='begin':
            lines.append('native_ppfx_resource_probe: '+json.dumps(
                native_snapshot(event['scope'],event['eye'])))
        if event['event']=='end':
            lines.append('native_ppfx_resource_probe: '+json.dumps(
                native_snapshot(event['scope'],event['eye'],'end')))
        lines.append('render_resource_probe: '+json.dumps(event))
    return lines+['run_end: run_id=synthetic']


class NativePostprocessTests(unittest.TestCase):
    def analyze(self,lines):
        report=a.analyze('\n'.join(lines))
        self.assertEqual(report['status'],'observed',report['errors'])
        self.assertFalse(report['resizingAdmitted'])
        self.assertIn('nativePostprocess',report,'Missing separate native component')
        component=report.get('nativePostprocess',{})
        self.assertIs(component.get('resizingAdmitted'),False)
        self.assertIs(component.get('correlationOnly'),True)
        self.assertIs(component.get('headsetValidated'),False)
        return component

    def reject(self,lines):
        component=self.analyze(lines)
        self.assertEqual(component['status'],'incomplete')
        self.assertTrue(component['errors'])

    def mutate(self,callback,phase=None):
        lines=native_lines()
        for i,line in enumerate(lines):
            if not line.startswith('native_ppfx_resource_probe: '):continue
            event=json.loads(line.split(': ',1)[1])
            if phase is None or event['phase']==phase:
                callback(event)
                lines[i]='native_ppfx_resource_probe: '+json.dumps(event)
        return lines

    def test_named_aliases_internal_surfaces_and_unknowns_are_exact_values(self):
        events=pair()
        for e in events:
            if e['event']=='record':e['state']['textures'][0]=dict(RESOURCE,object=999,parent=0)
        component=self.analyze(native_lines(events))
        self.assertEqual(component['status'],'observed',component['errors'])
        self.assertEqual(len(component['scopes']),2)
        scope=component['scopes'][0]
        self.assertEqual(scope['aliases'][0]['names'],['Global\xff','NamedAlias'])
        self.assertEqual(scope['aliases'][0]['nativeHandle'],0xe1000000)
        self.assertIn('rendererState.internalColorSurface',scope['matches'][0]['sources'])
        self.assertIn('buffers[0].texture.texture2d',scope['matches'][0]['sources'])
        self.assertEqual(scope['unknowns'][0]['object'],999)
        self.assertEqual(scope['unusedBuffers'],[2])
        self.assertFalse(component['completeAllocationGraph'])
        self.assertFalse(component['lifetimeQualified']);self.assertFalse(component['atomicSnapshot'])
        self.assertFalse(component['rolesQualified'])

    def test_old_missing_native_does_not_invalidate_bindings(self):
        self.reject(log(pair()).splitlines())

    def test_component_errors_never_change_binding_or_sprite_verdicts(self):
        lines=native_lines()
        for bad in ('native_ppfx_resource_probe_error: failed',
                    'native_ppfx_resource_probe: {',
                    'native_ppfx_resource_probe:{}',
                    'native_ppfx_resource_probe: []'):
            self.reject(lines[:-1]+[bad]+lines[-1:])
        good=with_sprites().splitlines()
        baseline=a.analyze('\n'.join(good))
        report=a.analyze('\n'.join(good[:-1]+['native_ppfx_resource_probe_error: failed']+good[-1:]))
        for key in ('status','errors','scopes','pairs','desktopSpriteOrdering'):
            self.assertEqual(report[key],baseline[key])

    def test_native_interval_phase_order_and_duplicates(self):
        lines=native_lines()
        for changed in (lines[1:2]+lines,lines+lines[1:2],
                        lines[:2]+lines[1:2]+lines[2:],
                        lines[:1]+lines[2:],
                        lines[:4]+lines[5:],
                        lines[:1]+[lines[2],lines[1]]+lines[3:],
                        lines[:4]+[lines[5],lines[4]]+lines[6:],
                        lines[:6]+[lines[7],lines[6]]+lines[8:]):
            with self.subTest(lines=changed):self.reject(changed)

    def test_foreign_identity_and_generation_reject_only_native(self):
        for key in ('scope','thread','frame','owner','device','generation','deviceGeneration','eye','route'):
            with self.subTest(key=key):
                self.reject(self.mutate(lambda s:s.update({key:s[key]+1})))
        events=pair()
        for e in events:del e['deviceGeneration']
        self.reject(native_lines(events))

    def test_native_headers_authority_and_types(self):
        for key,value in [('schema',True),('coverage','binding_state_only'),('readOnly',False),
                          ('resizingAdmitted',True),('complete',False),('status',1),
                          ('status',True),('truncated',True),('managerDirty',1),
                          ('bindingScopeComplete',True),('nativeDevice',31),
                          ('rendererVtable',0x90312338),('deviceGeneration',0),
                          ('managerVtable',0x90314c4c),('loadedEffectCount',2),
                          ('generation',True),('engineBase',0xfff00000),
                          ('renderer',0),('manager',0xffffffff),('listSentinel',2**32),
                          ('effects',{}),('buffers',None),('rendererState',[])]:
            with self.subTest(key=key,value=value):
                self.reject(self.mutate(lambda s:s.update({key:value}),phase='begin'))

    def test_counts_topology_containers_and_entry_arithmetic(self):
        mutations=[lambda s:s.update(effectCount=17),lambda s:s.update(bufferCount=273),
                   lambda s:s.update(globalBufferCount=-1),lambda s:s.update(globalBufferCount=17),
                   lambda s:s.update(bufferCount=2),lambda s:s.update(effectCount=0),
                   lambda s:s['effects'][0].update(next=0xe5000000),
                   lambda s:s['effects'][0].update(previous=0),
                   lambda s:s['effects'][0].update(effect=s['manager']),
                   lambda s:s['effects'][0].update(vtable=0x903180a8),
                   lambda s:s['effects'][0].update(bufferCount=17),
                   lambda s:s['buffers'][1].update(container=s['manager']),
                   lambda s:s['buffers'][1].update(index=1),
                   lambda s:s['buffers'][1].update({'global':True}),
                   lambda s:s['buffers'][1].update(entry=0xe6000008)]
        for mutation in mutations:
            with self.subTest(mutation=mutation):self.reject(self.mutate(mutation))

    def test_changed_family_or_cache_rejected_but_target_selection_allowed(self):
        for mutation in (lambda s:s['buffers'][0]['texture'].update(texture2d=51),
                         lambda s:s['buffers'][0].update(name='changed'),
                         lambda s:s['effects'][0].update(name='changed'),
                         lambda s:s['rendererState'].update(logicalWidth=1280),
                         lambda s:s['rendererState'].update(internalColorSurface=41),
                         lambda s:s.update(manager=0xe7000000)):
            self.reject(self.mutate(mutation,phase='end'))
        c=self.analyze(self.mutate(lambda s:s['rendererState'].update(currentTarget=0xe1000000),phase='end'))
        self.assertEqual(c['status'],'observed',c['errors'])
        def samplers(s):
            s['rendererState'].update(pixelSamplerHandles=[0xe1000000]*16,
                                      vertexSamplerHandles=[0xe1001000]*4)
        c=self.analyze(self.mutate(samplers,phase='end'))
        self.assertEqual(c['status'],'observed',c['errors'])
        for key,value in [('pixelSamplerHandles',[0]*20),('vertexSamplerHandles',[0]*3),
                          ('pixelSamplerHandles',[True]*16)]:
            self.reject(self.mutate(lambda s:s['rendererState'].update({key:value})))

    def test_names_raw_bytes_and_bounded_lengths(self):
        c=self.analyze(self.mutate(lambda s:s['effects'][0].update(name='x'*259)))
        self.assertEqual(c['status'],'observed',c['errors'])
        for target,key,value in [('effects','name','x'*260),('buffers','name','x'*64),
                                 ('effects','name',''),('buffers','name','a\x00b'),
                                 ('effects','name','\u0100'),('buffers','name',3)]:
            self.reject(self.mutate(lambda s:s[target][0].update({key:value})))

    def test_texture_fields_interfaces_dimensions_and_raw_flags(self):
        for key,value in [('handle',0),('handle',0xffffffff),('texture2d',0),
                          ('textureCube',-1),('textureVolume',2**32),('format',True),
                          ('usage',-1),('flags',2**32),('width',0),('height',-1),
                          ('requestedWidth',0),('requestedDepth',-1),('depth',-1),
                          ('kind',3),('kind',True),('selectedInterface',0),
                          ('selectedInterface',92),
                          ('inverseSizeBits',[0,0,0]),('inverseSizeBits',[0x7f800000,1,1])]:
            with self.subTest(key=key,value=value):
                self.reject(self.mutate(lambda s:s['buffers'][0]['texture'].update({key:value})))
        for key,value in [('logicalWidth',0),('widthDivisor',2**31),('internalColor',256),
                          ('linkedBorrowed',True),('rgbBits',2**31)]:
            self.reject(self.mutate(lambda s:s['buffers'][0].update({key:value})))
        for field in ('textureCube','textureVolume'):
            def change(s):
                for b in s['buffers']:
                    b['texture'].update(texture2d=0,selectedInterface=50,
                                        kind=2 if field=='textureCube' else 1,**{field:50})
            c=self.analyze(self.mutate(change))
            self.assertEqual(c['status'],'observed',c['errors'])

    def test_native_handle_never_matches_com_and_surfaces_require_object(self):
        def change(s):
            for b in s['buffers']:b['texture'].update(handle=40,texture2d=91,selectedInterface=91)
            s['rendererState'].update(defaultColorSurface=50,internalColorSurface=50,placeholderTexture=0)
        c=self.analyze(self.mutate(change))
        self.assertEqual(c['status'],'observed',c['errors'])
        self.assertEqual(c['scopes'][0]['unknowns'][0]['object'],40)
        self.assertEqual([m['object'] for m in c['scopes'][0]['matches']],[60])

    def test_duplicate_json_keys_and_nonfinite_are_component_only(self):
        lines=native_lines()
        for bad in (lines[1].replace('"schema": 1','"schema": 1, "schema": 1'),
                    lines[1].replace('"width": 2560','"width": NaN'),
                    lines[1].replace('"effects": [','"effects": [null,')):
            self.reject(lines[:1]+[bad]+lines[2:])

    def test_native_names_cannot_become_other_telemetry(self):
        for name in ('render_resource_probe_error: failed', 'render_resource_probe: {}',
                     'run_start: run_id=foreign', 'run_end: run_id=foreign',
                     'native_ppfx_resource_probe_error: failed',
                     'native_hud_boundary: bad'):
            with self.subTest(name=name):
                lines=self.mutate(lambda s:s['effects'][0].update(name=name))
                lines=lines[:-1]+sprite_log()+lines[-1:]
                c=self.analyze(lines)
                self.assertEqual(c['status'],'observed',c['errors'])
                self.assertEqual(a.analyze('\n'.join(lines))['desktopSpriteOrdering']['status'],'observed')

    def test_malformed_native_prefix_stays_in_its_component(self):
        for prefix in ('native_ppfx_resource_probe : ', 'native_ppfx_resource_probe\t: ',
                       'native_ppfx_resource_probe ', 'native_ppfx_resource_probe_error : '):
            with self.subTest(prefix=prefix):
                lines=native_lines()
                lines.insert(-1,prefix+'{')
                self.reject(lines)

    def test_malformed_native_prefix_cannot_promote_names_to_markers(self):
        for prefix in ('native_ppfx_resource_probe : ', 'native_ppfx_resource_probe\t: ',
                       'native_ppfx_resource_probe ', 'native_ppfx_resource_probe_error : '):
            for name in ('run_end: run_id=foreign', 'run_start: run_id=foreign',
                         'render_resource_probe_error: failed', 'native_hud_boundary: bad'):
                with self.subTest(prefix=prefix,name=name):
                    lines=self.mutate(lambda s:s['effects'][0].update(name=name))
                    lines[1]=lines[1].replace('native_ppfx_resource_probe: ',prefix,1)
                    lines=lines[:-1]+sprite_log()+lines[-1:]
                    self.reject(lines)
                    self.assertEqual(a.analyze('\n'.join(lines))['desktopSpriteOrdering']['status'],
                                     'observed')

    def test_full_scope_snapshot_budget_and_over_budget_are_bounded(self):
        events=[]
        for frame in range(9,15):
            for e in pair():
                e.update(scope=e['scope']+(frame-9)*2,frame=frame)
                events.append(e)
        lines=native_lines(events)
        # The native helper defaults to frame 9; align its actual context.
        for i,line in enumerate(lines):
            if line.startswith('native_ppfx_resource_probe: '):
                s=json.loads(line.split(': ',1)[1]);s['frame']=9+(s['scope']-1)//2
                lines[i]='native_ppfx_resource_probe: '+json.dumps(s)
        c=self.analyze(lines)
        self.assertEqual(c['status'],'observed',c['errors'])
        self.assertEqual(len(c['scopes']),12)
        self.reject(lines[:-1]+[lines[1]]+lines[-1:])

    def test_duplicate_effect_nodes_and_containers_reject(self):
        def duplicate(s,which):
            first=s['effects'][0]
            second=dict(first,node=0xe5001000,effect=0xe6001000,previous=first['node'],
                        next=s['listSentinel'],bufferCount=0)
            first['next']=second['node']
            second[which]=first[which]
            s['effects'].append(second);s.update(effectCount=2,loadedEffectCount=2)
        for which in ('node','effect'):
            self.reject(self.mutate(lambda s:duplicate(s,which)))

    def test_sampler_native_handles_cannot_become_com_matches(self):
        def samplers(s):
            for b in s['buffers']:b['texture'].update(texture2d=91,selectedInterface=91)
            s['rendererState'].update(defaultColorSurface=0,internalColorSurface=0,
                                      placeholderTexture=0,pixelSamplerHandles=[40]*16,
                                      vertexSamplerHandles=[50]*4)
        c=self.analyze(self.mutate(samplers))
        self.assertEqual(c['status'],'observed',c['errors'])
        self.assertEqual(c['scopes'][0]['unknowns'][0]['object'],40)

    def test_unused_2d_depth_zero_and_raw_infinity_are_retained(self):
        def depth_zero(s):
            for b in s['buffers']:
                b['texture'].update(requestedDepth=0,depth=0,
                                     inverseSizeBits=[0x39cccccd,0x3a360b61,0x7f800000])
        c=self.analyze(self.mutate(depth_zero))
        self.assertEqual(c['status'],'observed',c['errors'])
        self.assertEqual(c['scopes'][0]['beginSnapshot']['buffers'][0]['texture']['inverseSizeBits'][2],
                         0x7f800000)
        def volume(s):
            depth_zero(s)
            for b in s['buffers']:
                t=b['texture'];t.update(kind=1,textureVolume=t['texture2d'],texture2d=0)
        self.reject(self.mutate(volume))

    def test_foreign_fields_cannot_claim_authority_or_expand_snapshot(self):
        for mutation in (lambda s:s.update(atomicSnapshot=True),
                         lambda s:s['rendererState'].update(samplers=[1]*1000),
                         lambda s:s['buffers'][0].update(role='scene_color'),
                         lambda s:s['effects'][0].update(extra=[]),
                         lambda s:s['buffers'][0]['texture'].update(comLease=True)):
            self.reject(self.mutate(mutation))

    def test_deeply_malformed_json_remains_component_only(self):
        lines=native_lines()
        bad='native_ppfx_resource_probe: '+ '['*1500+'0'+']'*1500
        self.reject(lines[:-1]+[bad]+lines[-1:])

    def test_cli_summarizes_native_without_changing_binding_exit_or_evidence(self):
        for lines,status in ((native_lines(),'observed'),(log(pair()).splitlines(),'incomplete')):
            with tempfile.TemporaryDirectory() as directory:
                source=Path(directory)/'native-cli.log'
                output=Path(directory)/'report.json'
                content='\n'.join(lines).encode('utf-8')
                source.write_bytes(content)
                run=subprocess.run([sys.executable,str(TOOL),str(source),'--output',str(output)],
                                   capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                self.assertIn('spriteOrdering=incomplete',run.stdout)
                self.assertIn('nativePostprocess='+status,run.stdout)
                self.assertEqual(json.loads(output.read_text())['nativePostprocess']['status'],status)
                self.assertEqual(source.read_bytes(),content)


HUD_KEYS = [27,19,20,171,193,206,207,208,209,168,190,191,192,
            15,24,25,7,14,23,195,175,174,194]


def hud_pass():
    def boundary(sprite, flush):
        return dict(spriteDepth=sprite,flushDepth=flush,epoch=3,intact=1,faulted=0)
    def resource_(**changes):
        return dict(RESOURCE,bound=1,valid=1,**changes)
    states={key:0 for key in HUD_KEYS}
    states.update({19:5,20:6,171:1,207:5,208:6,209:1,168:15,25:8,23:4})
    def record(ordinal,phase,owner):
        return dict(ordinal=ordinal,repeat=1,phase=phase,operation=7,result=0,
                    drawIngressAddress=0x81234567,nativeOwner=owner,option=0,
                    spriteDepth=1 if phase==0 else 0,flushDepth=1 if phase==2 else 0,
                    nativeHookEpoch=3,rt0=resource_(),depth=resource_(object=60,format=75),
                    texture0=resource_(object=70,type=3),vertexShader=[0,0,1],pixelShader=[0,0,1],
                    viewport=[0,0,2560,1440],viewportDepth=[0,1],viewportValid=1,
                    scissorValid=1,scissor=[0,0,2560,1440],stateKeys=HUD_KEYS[:],
                    states=[states[key] for key in HUD_KEYS],stateValid=[1]*len(HUD_KEYS),valid=1)
    return dict(schema=1,event='hud_natural_pass',**{'pass':7},device=30,
                nativeGeneration=1,continuityEpoch=4,drawHookEpoch=2,nativeHookEpoch=3,
                cachedPermissionEpoch=5,frame=9,present=12,thread=3,
                spriteOwner=112,spriteEndOwner=112,flushOwner=128,flushEndOwner=128,
                option=0,flushEndOption=0,route='unknown',nativeCaller='unknown',
                complete=1,faults=0,draws=2,samples=2,flushDraws=1,
                correlationOnly=True,captureAdmitted=False,occlusionAdmitted=False,
                boundaries=[boundary(1,0),boundary(1,0),boundary(0,1),boundary(0,1)],
                records=[record(1,0,112),record(2,2,128)])


def with_hud(events=None,source=None):
    rows=[hud_pass()] if events is None else events
    content=log(pair()) if source is None else source
    return content.replace('run_end:', '\n'.join('hud_batch_probe: '+json.dumps(e)
                                               for e in rows)+'\nrun_end:')


class HudBatchAnalysisTests(unittest.TestCase):
    def component(self,source):
        report=a.analyze(source)
        self.assertIn('hudBatch',report,'Missing connected HUD batch analysis')
        hud=report['hudBatch']
        for key,value in [('correlationOnly',True),('captureAdmitted',False),
                          ('occlusionAdmitted',False),('headsetValidated',False)]:
            self.assertIs(hud[key],value)
        self.assertEqual(hud['route'],'unknown');self.assertEqual(hud['nativeCaller'],'unknown')
        return report,hud

    def reject(self,event):
        report,hud=self.component(with_hud([event]))
        self.assertEqual(report['status'],'observed',report['errors'])
        self.assertEqual(hud['status'],'incomplete')
        self.assertEqual(hud['rows'][0]['status'],'rejected')
        self.assertEqual(hud['rows'][0]['event'],event)
        self.assertTrue(hud['rows'][0]['errors'])

    def test_connected_complete_integer_boolean_pass_is_correlation_only(self):
        report,hud=self.component(with_hud())
        self.assertEqual(report['status'],'observed')
        self.assertEqual(hud['status'],'observed',hud['errors'])
        self.assertEqual(len(hud['passes']),1)
        self.assertEqual(hud['rows'][0]['event'],hud_pass())
        self.assertEqual(hud['passes'][0]['nativeGeneration'],1)
        self.assertEqual(hud['passes'][0]['continuityEpoch'],4)

    def test_absent_rows_preserve_existing_components(self):
        baseline=a.analyze(with_hud([],source=with_sprites()))
        report,hud=self.component(with_hud(source=with_sprites()))
        for key in ('status','errors','scopes','pairs','nativePostprocess','desktopSpriteOrdering'):
            self.assertEqual(report[key],baseline[key])
        _,missing=self.component(log(pair()))
        self.assertEqual(missing['status'],'incomplete');self.assertEqual(missing['rows'],[])

    def test_authority_route_schema_types_and_foreign_fields_reject(self):
        for key,value in [('schema',True),('schema',2),('event','capture'),('complete',True),
                          ('correlationOnly',1),('captureAdmitted',0),('occlusionAdmitted',True),
                          ('route',0),('nativeCaller',77),('eye',0),('capture',True)]:
            with self.subTest(key=key,value=value):
                event=hud_pass();event[key]=value;self.reject(event)

    def test_context_identity_ranges_preserve_high_bits(self):
        event=hud_pass();event['device']=0xe1000000
        _,hud=self.component(with_hud([event]));self.assertEqual(hud['status'],'observed')
        for key in ('pass','device','nativeGeneration','continuityEpoch','drawHookEpoch',
                    'nativeHookEpoch','cachedPermissionEpoch','frame','present','thread','spriteOwner'):
            for value in (0,True,-1):
                event=hud_pass();event[key]=value;self.reject(event)
        for value in (2**32-1,2**32):
            event=hud_pass();event['device']=value;self.reject(event)

    def test_complete_claim_requires_matching_closure_and_faults(self):
        for key,value in [('spriteEndOwner',113),('flushEndOwner',129),('flushEndOption',1),
                          ('faults',1),('complete',0),('flushDraws',0)]:
            event=hud_pass();event[key]=value;self.reject(event)
        for key,value in [('epoch',4),('spriteDepth',2),('flushDepth',2),('intact',0),('faulted',1)]:
            event=hud_pass();event['boundaries'][2][key]=value;self.reject(event)

    def test_faulted_read_and_partial_closure_are_retained_incomplete(self):
        event=hud_pass();event.update(complete=0,faults=8)
        event['records'][0]['rt0']['valid']=0;event['records'][0]['valid']=0
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete')
        self.assertEqual(hud['rows'][0]['event'],event)
        event=hud_pass();event.update(complete=0,faults=64,draws=0,samples=0,flushDraws=0,
                                   records=[],spriteEndOwner=0,flushOwner=0,flushEndOwner=0)
        event['boundaries'][1:]=[dict(spriteDepth=0,flushDepth=0,epoch=0,intact=0,faulted=1)]*3
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete')
        self.assertEqual(hud['passes'],[])

    def test_failed_viewport_read_preserves_null_numeric_values_as_incomplete(self):
        event=hud_pass();event.update(complete=0,faults=8)
        event['records'][0].update(viewportValid=0,viewportDepth=[None,None],valid=0)
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete',hud['errors'])
        self.assertEqual(hud['rows'][0]['event'],event)
        event['records'][0]['viewportValid']=1
        self.reject(event)

    def test_signed_hresult_and_read_fault_truth(self):
        event=hud_pass();event.update(complete=0,faults=16)
        event['records'][0]['result']=-2005530516
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete')
        event['faults']=0;self.reject(event)
        for value in (2**31,-2**31-1,True):
            event=hud_pass();event['records'][0]['result']=value;self.reject(event)

    def test_record_witness_phase_ordinals_and_counts_reject(self):
        for key,value in [('ordinal',2),('repeat',0),('phase',3),('operation',5),
                          ('nativeOwner',999),('nativeHookEpoch',4),('spriteDepth',0),
                          ('flushDepth',1),('drawIngressAddress',0)]:
            event=hud_pass();event['records'][0][key]=value;self.reject(event)
        for key,value in [('samples',1),('samples',65),('draws',3),('flushDraws',2),('faults',2048)]:
            event=hud_pass();event[key]=value;self.reject(event)
        event=hud_pass();event['records'].reverse();self.reject(event)

    def test_state_arrays_exact_alignment_and_integer_booleans(self):
        for key,value in [('states',[0]),('stateValid',[1]),('stateKeys',list(reversed(HUD_KEYS))),
                          ('valid',True),('viewportValid',True),('scissorValid',2),
                          ('vertexShader',[0,0,True]),('pixelShader',[0,0]),('unknown',1)]:
            event=hud_pass();event['records'][0][key]=value;self.reject(event)
        for key,value in [('states',True),('stateValid',True),('stateKeys',True)]:
            event=hud_pass();event['records'][0][key][0]=value;self.reject(event)

    def test_invalid_resource_viewport_shader_and_nonfinite_reject(self):
        for target,key,value in [('rt0','width',0),('rt0','width',True),('rt0','bound',True),
                                 ('texture0','object',2**32),('depth','valid',True)]:
            event=hud_pass();event['records'][0][target][key]=value;self.reject(event)
        for key,value in [('viewport',[0,0,0,1440]),('viewport',[0,0,3000,1440]),
                          ('viewportDepth',[1,0]),('viewportDepth',[0,None]),
                          ('viewportDepth',[0,1e300]),('scissor',[1,2,3]),
                          ('vertexShader',[5,0,1])]:
            event=hud_pass();event['records'][0][key]=value;self.reject(event)

    def test_pass_order_duplicate_context_and_six_pass_budget(self):
        rows=[]
        for i in range(6):
            event=hud_pass();event.update({'pass':7+i});rows.append(event)
        _,hud=self.component(with_hud(rows));self.assertEqual(hud['status'],'observed',hud['errors'])
        extra=hud_pass();extra['pass']=13
        for events in (rows+[extra],rows[:1]*2,list(reversed(rows))):
            _,hud=self.component(with_hud(events));self.assertEqual(hud['status'],'incomplete')
            self.assertTrue(any(row['status']=='rejected' for row in hud['rows']))
        event=hud_pass();event.update({'pass':8,'frame':8})
        _,hud=self.component(with_hud([hud_pass(),event]));self.assertEqual(hud['status'],'incomplete')

    def test_exact_sample_cap_and_truthful_truncation(self):
        event=hud_pass();event['records'][0]['repeat']=63
        event['records'][1]['ordinal']=64;event.update(draws=64,samples=64)
        _,hud=self.component(with_hud([event]));self.assertEqual(hud['status'],'observed',hud['errors'])
        event.update(draws=65,complete=0,faults=32)
        _,hud=self.component(with_hud([event]));self.assertEqual(hud['rows'][0]['status'],'incomplete')
        event['faults']=0;self.reject(event)

    def test_canonical_interval_gate_cannot_be_replaced_by_hud(self):
        good=with_hud()
        for source in (good+'\nrun_end: run_id=synthetic',good.replace(BUILD,'unbound'),
                       with_hud(source=log([])),good.replace('run_id=synthetic','run_id=unbound'),
                       good.replace('run_end: run_id=synthetic','run_end: run_id=foreign')):
            report,hud=self.component(source)
            self.assertEqual(report['status'],'incomplete')
            self.assertEqual(hud['status'],'incomplete');self.assertEqual(hud['passes'],[])
            self.assertEqual(len(hud['rows']),1);self.assertNotEqual(hud['rows'][0]['status'],'observed')

    def test_outside_interval_is_retained_but_not_observed(self):
        row='hud_batch_probe: '+json.dumps(hud_pass())
        for source in (row+'\n'+log(pair()),log(pair())+'\n'+row):
            report,hud=self.component(source)
            self.assertEqual(report['status'],'observed')
            self.assertEqual(hud['rows'][0]['status'],'rejected')
            self.assertEqual(hud['rows'][0]['raw'],row)

    def test_json_errors_and_namespace_spoofs_remain_hud_only(self):
        baseline=a.analyze(with_hud([],source=with_sprites()))
        for row in ('hud_batch_probe: {','hud_batch_probe:{}','hud_batch_probe_error: failed',
                    'hud_batch_probe: {"schema":1,"schema":1}',
                    'hud_batch_probe: {"hint":"run_start: run_id=foreign build_manifest_id='+BUILD+'"}',
                    'hud_batch_probe: {"x":NaN}',
                    'hud_batch_probe: {"x":1e999}',
                    'hud_batch_probe: '+'['*1500+'0'+']'*1500):
            source=with_sprites().replace('run_end:',row+'\nrun_end:')
            report,hud=self.component(source)
            for key in ('status','errors','scopes','pairs','desktopSpriteOrdering','nativePostprocess'):
                self.assertEqual(report[key],baseline[key],row[:100])
            self.assertEqual(hud['rows'][0]['status'],'rejected')
            self.assertEqual(hud['rows'][0]['raw'],row)
            json.dumps(report,allow_nan=False)

    def test_cli_exposes_hud_without_changing_binding_exit_or_input(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'hud.log';output=Path(directory)/'report.json'
            content=with_hud().encode();source.write_bytes(content)
            run=subprocess.run([sys.executable,'-B',str(TOOL),str(source),'--output',str(output)],
                               capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('hudBatch=observed',run.stdout)
            self.assertEqual(json.loads(output.read_text())['hudBatch']['status'],'observed')
            self.assertEqual(source.read_bytes(),content)

    def test_nonjson_hud_text_cannot_inject_native_namespace(self):
        source='\n'.join(native_lines())
        baseline=a.analyze(source)
        for row in ('hud_batch_probe_error: native_ppfx_resource_probe_error: failed',
                    'hud_batch_probe: native_ppfx_resource_probe: {}'):
            report,hud=self.component(source.replace('run_end:',row+'\nrun_end:'))
            self.assertEqual(hud['rows'][0]['status'],'rejected')
            self.assertEqual(report['nativePostprocess'],baseline['nativePostprocess'])

    def test_aborted_pending_sample_and_changed_end_witness_are_incomplete(self):
        event=hud_pass();event.update(complete=0,faults=64)
        event['records']=event['records'][:1]
        event['flushEndOwner']=0
        event['boundaries'][3]=dict(spriteDepth=0,flushDepth=0,epoch=0,intact=0,faulted=1)
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete',hud['errors'])
        self.assertEqual(hud['rows'][0]['event']['samples'],2)
        event=hud_pass();event.update(complete=0,faults=2)
        event['boundaries'][3].update(epoch=4,spriteDepth=2,flushDepth=3)
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['rows'][0]['status'],'incomplete',hud['errors'])
        event['complete']=1;self.reject(event)

    def test_unknown_deep_json_can_still_be_written_as_rejected_raw(self):
        row='hud_batch_probe: {"foreign":'+'['*990+'0'+']'*990+'}'
        report,hud=self.component(log(pair()).replace('run_end:',row+'\nrun_end:'))
        self.assertEqual(hud['rows'][0]['status'],'rejected')
        self.assertEqual(hud['rows'][0]['raw'],row)
        json.dumps(report,allow_nan=False)

    def test_missing_frozen_fields_and_record_cap_reject(self):
        for key in hud_pass():
            event=hud_pass();del event[key];self.reject(event)
        for key in hud_pass()['records'][0]:
            event=hud_pass();del event['records'][0][key];self.reject(event)
        event=hud_pass();event['records']*=33;self.reject(event)

    def test_good_records_after_bad_rows_are_retained_with_component_incomplete(self):
        bad=hud_pass();bad.update(complete=1,faults=64)
        good=hud_pass();good['pass']=8
        _,hud=self.component(with_hud([bad,good]))
        self.assertEqual([r['status'] for r in hud['rows']],['rejected','observed'])
        self.assertEqual(hud['status'],'incomplete')
        self.assertEqual(len(hud['passes']),1)

    def test_sprites_end_phase_has_no_native_owner_and_no_route_inference(self):
        event=hud_pass();middle=copy.deepcopy(event['records'][0])
        middle.update(phase=1,nativeOwner=0,spriteDepth=0,ordinal=2)
        event['records'][1]['ordinal']=3
        event['records'].insert(1,middle);event.update(draws=3,samples=3)
        _,hud=self.component(with_hud([event]))
        self.assertEqual(hud['status'],'observed',hud['errors'])
        self.assertEqual(hud['passes'][0]['route'],'unknown')


if __name__=='__main__':unittest.main()
