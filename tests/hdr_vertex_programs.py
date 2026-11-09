"""Real GPU witness -> production JSON writer -> production evidence reader."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_coj_temporal_source import vertex_program_observation
from temporal_source_analysis import sample,scope,copies,writes,report,Analysis

with tempfile.TemporaryDirectory(prefix='cojvr-vertex-witness-') as directory:
    output=Path(directory)/'witness.json'
    result=subprocess.run([sys.argv[1],str(output)],check=False)
    if result.returncode:
        raise SystemExit(result.returncode)
    raw=json.loads(output.read_text(encoding='utf-8'))
    observation=vertex_program_observation(dict(drawCalls=raw['draws'],vertexPrograms=raw))
    assert observation['available'] and not observation['materialQualified'] and not observation['complete']
    assert len(observation['programs'])==4 and observation['unobservedDraws']==1
    assert observation['programs'][0]['constantBits'][:4]==[0x3f800000,0x40000000,0x40400000,0x40800000]
    assert len({p['shaderSha256'] for p in observation['programs']})==4
    for path in (output,Path(str(output)+'.max.json')):
        raw=json.loads(path.read_text(encoding='utf-8'))
        event=Analysis().producer(copies(writes(sample(),2,2,shader=0)))
        event['sourceCopyProducer'].update(drawCalls=raw['draws'],alphaDrawCalls=0,alphaBlendDrawCalls=0,
            firstAlphaShader=0,lastAlphaShader=0,vertexPrograms=raw)
        result=report([event,scope(0)])
        assert result['status']=='observed',result['errors']
        assert result['sourceQualified'] is False and result['temporalAdmitted'] is False
        if path!=output:
            assert len(json.dumps(event))>65536
print('Native vertex-program evidence roundtrip passed; no material/temporal admission.')
