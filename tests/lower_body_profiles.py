"""Synthetic profile tests plus opt-in read-only original archive checks."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import io
import math
import struct
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
sys.path.insert(0, str(ROOT/'tests'))

ORIGINAL = None
NAMES = ('Bip01', 'Bip01 Pelvis', 'Bip01 L Thigh', 'Bip01 L Calf', 'Bip01 L Foot',
         'Bip01 L Toe0', 'Bip01 R Thigh', 'Bip01 R Calf', 'Bip01 R Foot', 'Bip01 R Toe0')
INDICES = {'Ray': (0, 2, 3, 4, 5, 6, 9, 10, 11, 12), 'Billy': (3, 5, 6, 7, 8, 9, 11, 12, 13, 14)}
PARENTS = (None, 0, 1, 2, 3, 4, 1, 6, 7, 8)


def mesh_fixture(model='Ray'):
    indices = INDICES[model]
    data = bytearray(24)
    for index in range(max(indices)+1):
        record = bytearray(256)
        struct.pack_into('<4I', record, 0, 1, 0, 256, 240)
        struct.pack_into('<I', record, 16, 8)
        role = indices.index(index) if index in indices else None
        name = NAMES[role] if role is not None else f'Unused{index}'
        record[20:52] = name.encode().ljust(32, b'\0')
        parent = PARENTS[role] if role is not None else None
        struct.pack_into('<H', record, 84, indices[parent] if parent is not None else 65535)
        matrix = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., float(index), 0., 1.)
        struct.pack_into('<16f', record, 92, *matrix)
        data.extend(record)
    struct.pack_into('<6I', data, 0, 0x48534d, 0, len(data), 8, max(indices)+1, 0)
    return bytes(data)


class ProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if importlib.util.find_spec('coj_lower_body_profiles'):
            import coj_lower_body_profiles
            cls.api = coj_lower_body_profiles

    def setUp(self):
        self.assertTrue(hasattr(self, 'api'), 'original lower-body profiles are missing')

    def test_exact_model_chains_have_distinct_indices_and_bind_identity(self):
        chains = [self.api.parse_bind_chain(mesh_fixture(model), model) for model in ('Ray', 'Billy')]
        for chain in chains:
            self.assertEqual(tuple(n.name for n in chain.nodes), NAMES)
            self.assertEqual(tuple(n.mesh_index for n in chain.nodes), INDICES[chain.model])
            self.assertEqual(chain.sha256, hashlib.sha256(mesh_fixture(chain.model)).hexdigest())
        self.assertNotEqual(chains[0].sha256, chains[1].sha256)

    def test_bind_recomposition_and_explicit_pelvis_anchor(self):
        from coj_animation_channels import Pose
        chain = self.api.parse_bind_chain(mesh_fixture(), 'Ray')
        # Sampled local leg matrices supplied explicitly; no implicit bind pose
        # substitution is allowed for a missing authored leg channel.
        locals_ = {name: Pose((1., 0., 0.), (0., 0., 0., 1.)).matrix() for name in NAMES[2:]}
        anchor = Pose((10., 20., 30.), (0., 0., math.sqrt(.5), math.sqrt(.5))).matrix()
        rows = self.api.recompose_legs(chain, locals_, anchor)
        self.assertEqual(len(rows), 8)
        self.assertAlmostEqual(rows[0]['world_matrix'][13], 19.)
        self.assertAlmostEqual(rows[3]['world_matrix'][13], 16.)
        self.assertAlmostEqual(rows[4]['world_matrix'][13], 19.)
        self.assertEqual(rows[0]['mesh_index'], 3)
        self.assertEqual(rows[0]['parent_index'], 2)
        self.assertEqual(rows[0]['local_matrix'], locals_[NAMES[2]])
        with self.assertRaises(ValueError):
            self.api.recompose_legs(chain, {NAMES[2]: locals_[NAMES[2]]}, anchor)
        with self.assertRaises(ValueError):
            self.api.recompose_legs(chain, dict(locals_, Head=anchor), anchor)

    def test_mesh_malformed_parent_cycle_basis_and_class_reject(self):
        data = mesh_fixture()
        mutations = [(0, '<I', 0), (16, '<I', 1000), (24+16, '<I', 9),
                     (24+2*256+84, '<H', 2), (24+3*256+84, '<H', 9),
                     (24+3*256+92, '<f', float('nan')),
                     (24+3*256+92, '<f', -1.)]
        for offset, fmt, value in mutations:
            bad = bytearray(data)
            struct.pack_into(fmt, bad, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                self.api.parse_bind_chain(bytes(bad), 'Ray')
        for bad in (data[:10], data[:-1], data+b'x'):
            with self.assertRaises(ValueError):
                self.api.parse_bind_chain(bad, 'Ray')
        with self.assertRaises(ValueError):
            self.api.parse_bind_chain(data, 'Unknown')
        with self.assertRaises(ValueError):
            self.api.parse_bind_chain(b'x'*(self.api.MAX_MEMBER_BYTES+1), 'Ray')

    def test_original_archive_is_not_caller_hash_overrideable(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)/'Data0.pak'
            archive.write_bytes(b'not an original archive')
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                self.api.sample_original(archive, 'Ray', 'Unarmed_WalkForward', 9.25)
            for model, track, frame in (('Unknown', 'Unarmed_Stand', 0),
                                        ('Ray', '../foreign', 0),
                                        ('Ray', 'Unarmed_RotateLeft', 0),
                                        ('Ray', 'Unarmed_WalkForward', 38),
                                        ('Ray', 'Unarmed_Stand', float('nan'))):
                with self.assertRaises(ValueError):
                    self.api.sample_original(archive, model, track, frame)

    def test_bounded_member_reads_reject_unknown_payload_and_oversize(self):
        payload = b'tiny synthetic resource'
        digest = hashlib.sha256(payload).hexdigest()
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('fixture', payload)
        stream.seek(0)
        with zipfile.ZipFile(stream) as archive:
            data, source = self.api._read(archive, 'fixture', digest)
            self.assertEqual(data, payload)
            self.assertEqual(source['size'], len(payload))
            with self.assertRaises(ValueError):
                self.api._read(archive, 'fixture', '0'*64)
            with self.assertRaises(ValueError):
                self.api._read(archive, 'fixture', digest, limit=4)

    def test_track_domains_keep_backward_step_end_and_exclude_commented_turns(self):
        self.assertEqual(self.api.TRACKS['Unarmed_StepLEnd'].frames, (25, 7))
        self.assertEqual(self.api.TRACKS['Unarmed_StepRLoop'].frames, (25, 26))
        self.assertEqual(self.api.TRACKS['Unarmed_WalkForward'].parameter_semantics,
                         'movement_distance_cm_per_cycle')
        self.assertNotIn('Unarmed_RotateLeft', self.api.TRACKS)
        text = self.api._script(b'// SeqTrack("bad")\r\r\n/* SeqTrack("bad2") */\r\r\nSeqTrack("good")')
        self.assertNotIn('bad', text)
        self.assertEqual(text.count('\n'), 2)

    def test_original_profiles(self):
        if not ORIGINAL:
            self.skipTest('explicit --data0-pak original required')
        for model in ('Ray', 'Billy'):
            for track, spec in self.api.TRACKS.items():
                with self.subTest(model=model, track=track):
                    report = self.api.sample_original(ORIGINAL, model, track, sum(spec.frames)/2)
                    self.assertEqual(report['model'], model)
                    self.assertEqual(report['mode'], 'offline_event_free_sample')
                    self.assertFalse(report['runtime_writer_implemented'])
                    self.assertEqual(len(report['legs']), 8)
                    self.assertEqual(report['clip']['source']['sha256'], spec.sha256)
                    self.assertEqual(report['binding']['mesh']['sha256'], self.api.MESH_HASHES[model])
                    self.assertEqual(report['anchor']['kind'], 'original_bind_pelvis_world')
                    self.assertEqual(len(report['pose_sha256']), 64)
                    self.assertTrue(all(len(row['local_matrix_sha256']) == 64 for row in report['legs']))

    def test_original_pose_identity_tracks_ticks_and_anchor_without_root_application(self):
        if not ORIGINAL:
            self.skipTest('explicit --data0-pak original required')
        from coj_animation_channels import Pose
        a = self.api.sample_original(ORIGINAL, 'Billy', 'Unarmed_WalkForward', 9.25)
        b = self.api.sample_original(ORIGINAL, 'Billy', 'Unarmed_WalkForward', 9.25001)
        self.assertEqual(a['pose_sha256'], b['pose_sha256'])
        anchor = Pose((123., 42., 9.), (0., 0., 0., 1.)).matrix()
        c = self.api.sample_original(ORIGINAL, 'Billy', 'Unarmed_WalkForward', 9.25, pelvis_world=anchor)
        self.assertNotEqual(a['pose_sha256'], c['pose_sha256'])
        self.assertEqual(c['anchor']['world_matrix'], anchor)
        self.assertEqual(c['anchor']['kind'], 'caller_supplied_pelvis_world')
        self.assertEqual(a['unapplied_authored_root_and_pelvis'], c['unapplied_authored_root_and_pelvis'])
        for old, new in zip(a['legs'], c['legs']):
            self.assertEqual(old['local_matrix_sha256'], new['local_matrix_sha256'])
            self.assertNotEqual(old['world_matrix'], new['world_matrix'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--data0-pak', type=Path)
    args, rest = parser.parse_known_args()
    ORIGINAL = args.data0_pak
    unittest.main(argv=[sys.argv[0], *rest])
