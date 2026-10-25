"""Offline proposals are never evidence that a native resolution was applied."""
from datetime import datetime, timezone
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import coj_resolution_profiles as profiles

NOW=datetime(2026,10,17,20,0,tzinfo=timezone.utc)
def observation(**overrides):
    result=dict(schemaVersion=1,recordType='cojvr-resolution-observation',status='observed',
        preparationId='fixture',observedUtc='2026-10-17T20:00:00Z',
        recommendedWidth=2804,recommendedHeight=2860,runtimeVersion='fixture-runtime',
        hmdSerial='fixture-headset',hmdModel='fixture-model',adapterIndex=0,
        applicationKey='',applicationKeyStatus='unavailable',identityQualified=False)
    result.update(overrides)
    return result
def resolve(profile,obs=None,**kwargs):
    return profiles.resolve(profile,observation=obs,expected_preparation_id='fixture',now=NOW,**kwargs)

class ResolutionProfiles(unittest.TestCase):
    def test_pixel_ratios_not_axis_ratios_and_never_apply(self):
        for name,extent in [('Auto',[2804,2860]),('75%',[2428,2477]),('100%',[2804,2860]),('125%',[3135,3198]),('150%',[3434,3503]),('200%',[3965,4045])]:
            result=resolve(name,observation())
            self.assertEqual(result['candidateDimensions'],extent)
            self.assertFalse(result['applyAllowed'])
            self.assertEqual(result['effectiveEyeDimensions'],None)
            self.assertEqual(result['fallback'],'preserve_selected_native_video')
            self.assertIn('recommendation_application_identity',result['unresolvedGates'])
            self.assertEqual(result['payloadLowerBoundBytes'],32*extent[0]*extent[1])
            self.assertLessEqual(abs(extent[0]/extent[1]-2804/2860),1/min(extent))

    def test_all_pixel_profiles_follow_current_headset_and_need_fresh_observation(self):
        for name,ratio,extent in [('125%',1.25,[2236,1118]),('150%',1.5,[2449,1225]),('200%',2,[2828,1414])]:
            result=resolve(name,observation(recommendedWidth=2000,recommendedHeight=1000))
            self.assertEqual(result['candidateDimensions'],extent)
            self.assertEqual(result['pixelRatio'],ratio)
            self.assertEqual(resolve(name)['status'],'unavailable')
            self.assertEqual(resolve(name,observation(observedUtc='2026-10-17T19:57:59Z'))['status'],'rejected')
            self.assertEqual(resolve(name,observation(),width=2000,height=1000)['status'],'rejected')

    def test_manual_preserves_aspect_without_runtime(self):
        result=resolve('Manual',width=2560,height=1440)
        self.assertEqual(result['candidateDimensions'],[2560,1440])
        self.assertEqual(result['status'],'proposal_only')
        for width,height in [(True,1440),(0,1440),(-1,1440),(1.2,1440),('2560',1440),(2560,None),(2**31,1440)]:
            self.assertEqual(resolve('Manual',width=width,height=height)['status'],'rejected')

    def test_unavailable_auto_and_foreign_stale_or_invalid_observation(self):
        self.assertEqual(resolve('Auto')['status'],'unavailable')
        for obs in [observation(preparationId='foreign'),observation(observedUtc='2026-10-17T19:57:59Z'),
                    observation(observedUtc='2026-10-17T20:00:01Z'),observation(observedUtc='2026-10-17T20:00:00'),
                    observation(recommendedWidth=0),observation(recommendedHeight=True),
                    observation(recommendedWidth=2**32),observation(adapterIndex=-1),
                    observation(hmdSerial=''),observation(schemaVersion=2),
                    observation(identityQualified=True)]:
            self.assertEqual(resolve('100%',obs)['status'],'rejected',obs)

    def test_rounding_and_checked_area_payload(self):
        # 125% of a one-pixel axis rounds up while remaining an unapplied proposal.
        self.assertEqual(resolve('125%',observation(recommendedWidth=1,recommendedHeight=1))['candidateDimensions'],[1,1])
        self.assertEqual(resolve('Manual',width=2**31-1,height=2**31-1)['status'],'rejected')
        self.assertEqual(resolve('unknown')['status'],'rejected')
        self.assertEqual(resolve('100%',observation(),width=10,height=10)['status'],'rejected')

    def test_json_rejects_duplicates_nonfinite_and_large_records(self):
        for text in ['{"schemaVersion":1,"schemaVersion":1}','{"x":NaN}','{"x":Infinity}',' '*65537]:
            with self.assertRaises(ValueError): profiles.load_observation(text)

    def test_cli_accepts_one_utf8_bom_from_windows_stdin(self):
        record=observation(observedUtc=datetime.now(timezone.utc).isoformat(),hmdModel='fixture-é')
        run=subprocess.run([sys.executable,str(Path(profiles.__file__)),
            '--profile','100%','--observation','-','--preparation-id','fixture'],
            input=b'\xef\xbb\xbf'+json.dumps(record,ensure_ascii=False).encode('utf-8'),capture_output=True,
            env=dict(os.environ,PYTHONIOENCODING='utf-8'))
        self.assertEqual(run.returncode,0,run.stdout.decode('utf-8')+run.stderr.decode('utf-8'))
        result=json.loads(run.stdout)
        self.assertEqual(result['candidateDimensions'],[2804,2860])
        self.assertEqual(result['observation']['hmdModel'],'fixture-é')
        self.assertFalse(result['applyAllowed'])
        for text in ['\ufeff\ufeff{}','\ufeff{"x":1,"x":2}','\ufeff{"x":NaN}','\ufeff'+' '*65534]:
            with self.assertRaises(ValueError): profiles.load_observation(text)

    def test_file_accepts_one_bom_and_rejects_two(self):
        record=observation(observedUtc=datetime.now(timezone.utc).isoformat())
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'observation.json'
            for count in (0,1,2):
                content=b'\xef\xbb\xbf'*count+json.dumps(record).encode('utf-8')
                source.write_bytes(content)
                run=subprocess.run([sys.executable,str(Path(profiles.__file__)),
                    '--profile','Auto','--observation',str(source),'--preparation-id','fixture'],
                    capture_output=True,env=dict(os.environ,PYTHONIOENCODING='utf-8'))
                self.assertEqual(run.returncode,0 if count<2 else 1,run.stdout.decode('utf-8'))
                self.assertFalse(json.loads(run.stdout)['applyAllowed'])
                self.assertEqual(source.read_bytes(),content)

if __name__=='__main__': unittest.main()
