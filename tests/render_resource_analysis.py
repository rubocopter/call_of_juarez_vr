"""Synthetic diagnostics never qualify native ownership or headset behavior."""
import copy
import importlib.util
import json
from pathlib import Path
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

if __name__=='__main__':unittest.main()
