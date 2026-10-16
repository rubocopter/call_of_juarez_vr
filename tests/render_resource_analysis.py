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
            thread=3, frame=9, owner=20, device=30, generation=1, route=0)
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

if __name__=='__main__':unittest.main()
