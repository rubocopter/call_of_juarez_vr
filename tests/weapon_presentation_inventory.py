"""Interpreted offline fixtures; optional pinned-original integration, no JVM."""
import argparse
import copy
import importlib.util
import io
import json
import math
from pathlib import Path
import struct
import sys
import unittest
import wave
import zipfile

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
SPEC=importlib.util.find_spec('coj_weapon_presentation_inventory')
OPTIONS=None


class PresentationFixtures(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(SPEC,'weapon presentation inventory implementation missing')
        global p
        import coj_weapon_presentation_inventory as p

    def test_scope_normalization_preserves_native_camera_contract(self):
        self.assertAlmostEqual(p.scope_zoom(8,80),8)
        self.assertAlmostEqual(p.scope_zoom(8,60),8*math.tan(math.pi/6)/math.tan(math.radians(40)))
        self.assertNotEqual(p.scope_zoom(8,90),8)
        for zoom,fov in [(0,80),(-8,80),(8,0),(8,180),(8,math.nan),(math.inf,80)]:
            with self.assertRaises(ValueError):p.scope_zoom(zoom,fov)

    def test_tangents_are_copied_and_asymmetry_is_preserved(self):
        eyes=[{'tangents':[-1.1,.9,-.8,1.2],'pose':{'x':-.032},'width':1440},
              {'tangents':[-.9,1.1,-1.2,.8],'pose':{'x':.032},'width':1440}]
        original=copy.deepcopy(eyes)
        result=p.scale_tangents(eyes,8)
        self.assertEqual(eyes,original)
        self.assertEqual(result[0]['tangents'],[-1.1/8,.9/8,-.8/8,1.2/8])
        self.assertEqual(result[1]['pose'],eyes[1]['pose'])
        result[0]['pose']['x']=7
        self.assertEqual(eyes,original)
        for malformed in [[],[{'tangents':[1,1,1,1]}], [{'tangents':[math.nan,1,-1,1]}]]:
            with self.assertRaises(ValueError):p.scale_tangents(malformed,8)
        for tangents in ([-1,True,-1,1],[-1e308,1e308,-1e308,1e308]):
            with self.assertRaises(ValueError):
                p.scale_tangents([{'tangents':tangents}]*2,1e-6)

    def test_scope_translation_uses_current_full_direction(self):
        self.assertEqual(p.scope_position([10,20,30],[0,1,0],10),[10,10,30])
        with self.assertRaises(ValueError):p.scope_position([0,0,0],[0,2,0],10)

    def test_pcm_inventory_measures_energy_without_content_verdict(self):
        buf=io.BytesIO()
        with wave.open(buf,'wb') as w:
            w.setparams((1,2,8000,0,'NONE','not compressed'))
            w.writeframes(struct.pack('<4h',0,16384,-16384,0))
        result=p.wav_profile(buf.getvalue())
        self.assertEqual(result['frames'],4)
        self.assertAlmostEqual(result['seconds'],4/8000)
        self.assertAlmostEqual(result['peak_normalized'],.5)
        self.assertAlmostEqual(result['rms_normalized'],math.sqrt(.125))
        self.assertEqual(result['acoustic_content'],'unqualified')
        with self.assertRaises(ValueError):p.wav_profile(b'not audio')

    def test_overlap_is_predicted_window_only(self):
        result=p.predicted_overlap(0,.696598639,.2)
        self.assertAlmostEqual(result['body_clip_remaining_seconds'],.496598639)
        self.assertFalse(result['duplication_proven'])
        self.assertEqual(p.predicted_overlap(0,.696598639,1)['body_clip_remaining_seconds'],0)
        for values in [(0,-1,1),(math.nan,1,2),(2,1,1)]:
            with self.assertRaises(ValueError):p.predicted_overlap(*values)

    def test_ring_fit_and_rejection_do_not_admit_socket(self):
        points=[[2+.5*math.cos(i*math.pi/4),3+.5*math.sin(i*math.pi/4),-2] for i in range(8)]
        result=p.fit_ring(points)
        for a,b in zip(result['centre_cm'],[2,3,-2]):self.assertAlmostEqual(a,b)
        self.assertAlmostEqual(result['radius_xy_cm'],.5)
        self.assertFalse(result['socket_admitted'])
        for bad in [[],[[0,0,0],[1,0,0],[2,0,0]],[[math.nan,0,0]]*8]:
            with self.assertRaises(ValueError):p.fit_ring(bad)

    def test_ring_mapping_uses_complete_frame(self):
        frame=[[0,1,0,0],[-1,0,0,0],[0,0,1,0],[100,200,300,1]]
        self.assertEqual(p.transform_point([2,3,-2],frame),[97,202,298])
        with self.assertRaises(ValueError):p.transform_point([0,0,0],[[0]*4]*4)

    def test_unknown_archives_and_duplicate_members_reject(self):
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as z:z.writestr('x',b'unknown')
        with self.assertRaisesRegex(ValueError,'unrecognized'):p.PinnedArchive(stream,'data2')
        with self.assertRaisesRegex(ValueError,'unsafe'):p.normalize_member('../foreign')

    def test_duplicate_and_unsafe_members_reject_even_in_hashed_archive(self):
        # Patch only the fixture's expected digest; production has no override.
        import hashlib
        from unittest.mock import patch
        for names in [('A/B','a\\b'),('A/../B',)]:
            stream=io.BytesIO()
            with zipfile.ZipFile(stream,'w') as z:
                for name in names:z.writestr(name,b'fixture')
            digest=hashlib.sha256(stream.getvalue()).hexdigest()
            with patch.dict(p.PINNED_ARCHIVES,{'fixture':digest}),self.assertRaises(ValueError):
                p.PinnedArchive(stream,'fixture')

    def test_pcm_rejects_truncated_payload_and_unsupported_width(self):
        stream=io.BytesIO()
        with wave.open(stream,'wb') as w:
            w.setparams((1,2,8000,0,'NONE','not compressed'));w.writeframes(b'\0'*16)
        with self.assertRaisesRegex(ValueError,'truncated'):p.wav_profile(stream.getvalue()[:-4])
        stream=io.BytesIO()
        with wave.open(stream,'wb') as w:
            w.setparams((1,1,8000,0,'NONE','not compressed'));w.writeframes(b'\0'*8)
        with self.assertRaises(ValueError):p.wav_profile(stream.getvalue())

    def test_native_buffer_filter_rejects_long_suffixes_not_all_prefix_matches_play(self):
        self.assertTrue(hasattr(p,'soundbuffer_accept'),'native prefix filter is missing')
        for name in ('schofield_reload.wav','schofield_reload1.wav','schofield_reload_01.ogg'):
            self.assertTrue(p.soundbuffer_accept(name,'schofield_reload'))
        for name in ('schofield_reload_beg.wav','schofield_reload_end.wav',
                     'schofield_reload1234.wav','schofield_reload.txt','foreign.wav','SCHOFIELD_RELOAD.wav'):
            self.assertFalse(p.soundbuffer_accept(name,'schofield_reload'))

    def test_quantized_vertex_decoder_measures_authored_bias_and_scale(self):
        # Exact known type=4, bias XYZ + signed-short XYZ * scalar, stride 8.
        data=struct.pack('<I4f',4,1,2,3,.25)+b'\0'*40+struct.pack('<4h',4,-4,8,1)
        part={'arrays':{0x160:(0,60),0x101:(60,8)},'vertex_count':1}
        self.assertEqual(p.packed_vertices(data,part),([(2,1,5)],.25))
        bad=bytearray(data);struct.pack_into('<I',bad,0,99)
        with self.assertRaises(ValueError):p.packed_vertices(bad,part)

    def test_output_cannot_target_source_assets_or_existing_evidence(self):
        with self.assertRaisesRegex(ValueError,'work'):
            p.main(['--code-pak','missing','--data0-pak','missing','--data2-pak','missing',
                    '--output',str(ROOT/'tests/unwanted.json')])


class OriginalPresentation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if OPTIONS is None or OPTIONS.code_pak is None:raise unittest.SkipTest('explicit original paths not supplied')
        import coj_weapon_presentation_inventory as p
        cls.report=p.build_inventory(OPTIONS.code_pak,OPTIONS.data0_pak,OPTIONS.data2_pak)

    def test_original_scope_profiles_and_mesh_owners(self):
        optics=self.report['optics']
        self.assertEqual(optics['authored_scope_zoom'],8)
        self.assertEqual(optics['camera_defaults']['m_fScopeForward'],10)
        self.assertAlmostEqual(optics['camera_defaults']['m_fScopeScale'],1.05,places=6)
        self.assertEqual(optics['weapon_scope_element']['name'],'Scope')
        self.assertEqual(optics['weapon_scope_element']['index'],9)
        self.assertNotEqual(optics['overlay_mesh']['source']['sha256'],optics['weapon_mesh']['source']['sha256'])
        self.assertFalse(optics['runtime_admitted'])

    def test_original_audio_body_entry_and_duplicate_gate(self):
        audio=self.report['audio']
        self.assertEqual(audio['asset']['sha256'],'ef0ee1f5785336a444242c271504cb74e2a44c54e4e4a4b081ea29a603b51aed')
        self.assertEqual(audio['wave']['frames'],30720)
        self.assertAlmostEqual(audio['wave']['seconds'],30720/44100)
        self.assertTrue(audio['body_entry_plays_shared_clip'])
        self.assertFalse(audio['unit_transfer_has_direct_sound_call'])
        self.assertFalse(audio['additional_sound_admitted'])
        self.assertEqual(len(audio['profiles']),9)
        remington=next(row for row in audio['profiles'] if row['class']=='WpnAmmoShotgunRemington')
        self.assertIsNone(remington['buffers']['Body'])
        self.assertEqual(len(remington['prefix_matches_in_explicit_data2']['Begin']),3)
        self.assertEqual(sum(m['accept_after_lowercase_normalization'] for m in
            remington['prefix_matches_in_explicit_data2']['Begin']),1)
        self.assertEqual(len(audio['sound_assets']),5)
        self.assertEqual(len(audio['weapon_profiles']),10)

    def test_original_loading_rings_reproduce_measured_profiles(self):
        self.assertEqual(len(self.report['loading_mouths']),2)
        for model in self.report['loading_mouths']:
            self.assertEqual(len(model['rings']),6)
            for ring in model['rings']:
                self.assertGreaterEqual(ring['witness_count'],8)
                self.assertLess(ring['centre_seed_error_cm'],.002)
                self.assertLess(ring['max_radial_residual_cm'],.001)
                self.assertFalse(ring['socket_admitted'])
            self.assertFalse(model['gate_clearance_qualified'])
            self.assertFalse(model['native_phase_qualified'])
            self.assertTrue(model['player_animation_sources'])

    def test_original_sources_are_hashed_and_runtime_gates_retained(self):
        self.assertTrue(all(len(s['sha256'])==64 for s in self.report['sources']))
        self.assertTrue(self.report['native_boundaries']['scope_parameter_factory_on_cache_miss'])
        self.assertTrue(self.report['native_boundaries']['scope_overlay_independent_update'])
        self.assertFalse(self.report['runtime_admitted'])
        self.assertIn('SetSkin',self.report['audio']['unqualified_consumers'])


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code-pak',type=Path)
    parser.add_argument('--data0-pak',type=Path)
    parser.add_argument('--data2-pak',type=Path)
    parser.add_argument('--output',type=Path)
    OPTIONS=parser.parse_args()
    if any((OPTIONS.code_pak,OPTIONS.data0_pak,OPTIONS.data2_pak)) and not all((OPTIONS.code_pak,OPTIONS.data0_pak,OPTIONS.data2_pak)):
        parser.error('all three explicit originals are required together')
    if OPTIONS.output and OPTIONS.code_pak is None:parser.error('--output requires all three originals')
    if OPTIONS.output and (not OPTIONS.output.resolve().is_relative_to((ROOT/'work').resolve()) or OPTIONS.output.exists()):
        parser.error('output must be a new file under ignored work/')
    suite=unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    if OPTIONS.output:
        if not OPTIONS.output.resolve().is_relative_to((ROOT/'work').resolve()):raise ValueError('output must stay in work')
        with OPTIONS.output.open('x',encoding='utf-8') as f:
            json.dump(OriginalPresentation.report,f,indent=2,allow_nan=False)
    sys.exit(0 if result.wasSuccessful() else 1)
