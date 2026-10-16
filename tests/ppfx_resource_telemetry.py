"""Real host serializer output must round-trip as one-line JSON, including byte names."""
import json
from pathlib import Path
import runpy
import subprocess
import sys

def read(mode):
    text=subprocess.check_output([sys.argv[1],mode],text=True,encoding='utf-8')
    assert len(text.splitlines())==1
    prefix='native_ppfx_resource_probe: '
    assert text.startswith(prefix)
    return json.loads(text[len(prefix):])

r=read('normal')
assert r['readOnly'] and not r['resizingAdmitted'] and r['complete']
assert r['coverage']=='native_ppfx_structural_snapshot'
assert r['deviceGeneration']==2 and r['generation']==7
assert r['device']==0xA0002000 and r['owner']==0xA0040000
assert r['effects'][0]['name']=='x\x80'
assert r['buffers'][0]['name']=='HDR"\\\n\t\x01\x80'
assert r['buffers'][0]['texture']['texture2d']==0xA0030000
assert r['buffers'][1]['texture']['handle']==r['buffers'][0]['texture']['handle']
assert r['buffers'][1]['linkedBorrowed']==1
assert r['buffers'][0]['texture']['selectedInterface']==0xA0030000
assert r['buffers'][0]['texture']['requestedWidth']==2560
assert r['buffers'][0]['texture']['depth']==1
assert r['loadedEffectCount']==1 and r['managerVtable']==0x10314C48
assert r['effects'][0]['vtable']==0x103180AC
assert r['rendererState']['pixelSamplerHandles'][15]==0xA0020000
assert r['rendererState']['vertexSamplerHandles'][0]==0xA0020000
for mode in ('mismatch','overflow','invalid','no_context'):
    r=read(mode)
    assert not r['complete'] and not r['resizingAdmitted'],mode
    assert len(r['buffers'])<=272 and len(r['effects'])<=16
assert read('overflow')['truncated']
r=read('zero_depth')
assert r['complete'] and r['buffers'][0]['texture']['depth']==0
assert r['buffers'][0]['texture']['inverseSizeBits'][2]==0x7f800000
# Exercise the real decoder and formatter against the independent strict parser.
fixture=runpy.run_path(str(Path(__file__).with_name('render_resource_analysis.py')))
native=subprocess.check_output([sys.argv[2],'--telemetry'],text=True,encoding='utf-8').splitlines()
assert len(native)==4
events=fixture['pair']()
lines=[f"run_start: run_id=synthetic build_manifest_id={fixture['BUILD']} pid=1"]
for eye in (0,1):
    begin,end=native[eye*2:eye*2+2]
    snapshot=json.loads(begin.split(': ',1)[1])
    common={k:snapshot[k] for k in ('thread','frame','owner','device','generation',
                                   'deviceGeneration','route','scope','eye')}
    scope=events[eye*3:eye*3+3]
    for event in scope:event.update(common)
    state=scope[1]['state']
    state['targets'][0].update(object=0xef000010,surface=0xef000010,
                              parent=snapshot['buffers'][0]['texture']['texture2d'])
    depth=snapshot['rendererState']['internalDepthSurface']
    state['depth'].update(object=depth,surface=depth)
    lines.extend((begin,'render_resource_probe: '+json.dumps(scope[0]),
                  'render_resource_probe: '+json.dumps(scope[1]),end,
                  'render_resource_probe: '+json.dumps(scope[2])))
lines.append('run_end: run_id=synthetic')
report=fixture['a'].analyze('\n'.join(lines))
assert report['status']=='observed',report['errors']
component=report['nativePostprocess']
assert component['status']=='observed',component['errors']
assert len(component['scopes'])==2 and all(s['matches'] for s in component['scopes'])
assert not component['resizingAdmitted'] and not component['lifetimeQualified']
print('PPFX telemetry: escaped byte names, LAA identities, aliases and rejected contexts verified')
