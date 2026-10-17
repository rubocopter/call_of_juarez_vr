"""Offline proposals are never evidence that a native resolution was applied."""
from datetime import datetime, timezone
from pathlib import Path
import sys
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
        for name,extent in [('Auto',[2804,2860]),('75%',[2428,2477]),('100%',[2804,2860]),('125%',[3135,3198])]:
            result=resolve(name,observation())
            self.assertEqual(result['candidateDimensions'],extent)
            self.assertFalse(result['applyAllowed'])
            self.assertEqual(result['effectiveEyeDimensions'],None)
            self.assertEqual(result['fallback'],'preserve_selected_native_video')
            self.assertIn('recommendation_application_identity',result['unresolvedGates'])
            self.assertEqual(result['payloadLowerBoundBytes'],32*extent[0]*extent[1])
            self.assertLessEqual(abs(extent[0]/extent[1]-2804/2860),1/min(extent))

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

if __name__=='__main__': unittest.main()
