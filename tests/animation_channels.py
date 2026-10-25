"""Tiny synthetic ANM fixtures only; run with python -B tests/animation_channels.py."""
from pathlib import Path
import importlib.util
import math
import struct
import sys
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))


def fixture(channels=None, mode=0, name='bip01 l thigh'):
    """One record, cubic clamped knots; no original game data."""
    channels = channels or [0., 0., 0., 0., 0., 0.]
    names = name.encode('ascii') + b'\0'
    table = (32 + len(names) + 3) & ~3
    data = bytearray(table + 60)
    data[32:32+len(names)] = names
    offsets = []
    for channel in channels:
        if isinstance(channel, tuple):
            knots, controls = channel
            data.extend(struct.pack('<2f', 0., 1.))
            start = len(data)
            data.extend(struct.pack('<' + 'H'*len(knots), *knots))
            end = len(data)
            data.extend(struct.pack('<' + 'H'*len(controls), *controls))
        else:
            data.extend(struct.pack('<2f', channel, 0.))
            start = end = len(data)
        offsets.extend((start, end))
        while len(data) % 4:
            data.append(0)
    hashed = 0
    for char in name.encode('ascii'):
        hashed = (hashed * 41 + char) & 0xffffffff
    struct.pack_into('<15I', data, table, 32, hashed, 1 | mode << 16, *offsets)
    struct.pack_into('<8I', data, 0, 0x314d4e41, 0x10015, 3, 32, 160, 1, table, len(data))
    return bytes(data)


class SamplerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if importlib.util.find_spec('coj_animation_channels') is not None:
            import coj_animation_channels
            cls.api = coj_animation_channels

    def setUp(self):
        self.assertTrue(hasattr(self, 'api'), 'bounded mathematical ANM sampler is missing')

    def test_cubic_is_not_linear_key_interpolation(self):
        clip = self.api.parse_anm(fixture([((0, 150), (0, 0, 100, 100)), 2., 3., 0., 0., 0.]))
        self.assertAlmostEqual(clip.sample('bip01 l thigh', 7.5).translation[0], 15.625, places=4)
        self.assertEqual(clip.sample('bip01 l thigh', 0).translation, (0., 2., 3.))
        self.assertEqual(clip.sample('bip01 l thigh', 30).translation[0], 100.)
        # Native time is truncated to integer ticks before the cubic evaluator.
        self.assertEqual(clip.sample('bip01 l thigh', 7.5), clip.sample('bip01 l thigh', 7.50001))

    def test_mode_and_native_basis(self):
        pose = self.api.parse_anm(fixture([2., 3., 4., 0., 0., 0.], mode=1)).sample('bip01 l thigh', 0)
        self.assertAlmostEqual(abs(pose.quaternion[0]), math.sqrt(.5), places=6)
        matrix = pose.matrix()
        self.assertEqual(matrix[12:15], (2., 3., 4.))
        self.assertAlmostEqual(matrix[6], -1., places=6)
        self.assertAlmostEqual(matrix[9], 1., places=6)
        for mode in range(6):
            q = self.api.parse_anm(fixture(mode=mode)).sample('bip01 l thigh', 0).quaternion
            self.assertAlmostEqual(sum(x*x for x in q), 1., places=6)

    def test_native_repeated_interior_knots_are_supported(self):
        clip = self.api.parse_anm(fixture([((0, 50, 50, 50, 150), (0, 10, 20, 30, 40, 50, 60)), 0., 0., 0., 0., 0.]))
        self.assertAlmostEqual(clip.sample('bip01 l thigh', 10).translation[0], 30., places=4)
        self.assertAlmostEqual(clip.sample('bip01 l thigh', 20).translation[0], 45., places=4)

    def test_native_euler_has_negative_w_before_fixed_mode_product(self):
        pose = self.api.parse_anm(fixture([0., 0., 0., 30., 20., 10.])).sample('bip01 l thigh', 0.)
        native = (.2685358226299286, .14487811923027039, .12767943739891052, -.9437143802642822)
        for a, b in zip(pose.quaternion, native):
            self.assertAlmostEqual(a, b, places=6)

    def test_all_rotation_modes_against_isolated_native_synthetic_fixture(self):
        # Original +0x251E90, synthetic angles (30,20,10). These are oracle
        # results, not recomputed with the sampler's quaternion formula.
        expected = (
            (.268535823, .144878119, .127679437, -.943714380),
            (-.477423310, .012161304, .192727298, -.857190311),
            (.292327791, -.087439202, .757589817, -.577023804),
            (-.473868042, .614724457, -.201310501, -.597525775),
            (-.769751132, .099600501, -.564862490, -.280166507),
            (-.346188605, -.328989923, -.469846308, -.742403865),
        )
        for mode, quaternion in enumerate(expected):
            pose = self.api.parse_anm(fixture([0., 0., 0., 30., 20., 10.], mode)).sample('bip01 l thigh', 0.)
            for a, b in zip(pose.quaternion, quaternion):
                self.assertAlmostEqual(a, b, places=6)

    def test_noncommuting_local_times_parent(self):
        local = self.api.Pose((2., 0., 0.), (0., 0., 0., 1.)).matrix()
        parent = self.api.Pose((10., 20., 30.), (0., 0., math.sqrt(.5), math.sqrt(.5))).matrix()
        result = self.api.compose(local, parent)
        self.assertAlmostEqual(result[12], 10.)
        self.assertAlmostEqual(result[13], 18.)
        self.assertAlmostEqual(result[14], 30.)
        self.assertNotEqual(result, self.api.compose(parent, local))

    def test_rejects_header_record_and_channel_corruption(self):
        good = fixture([((0, 150), (0, 10, 20, 30)), 0., 0., 0., 0., 0.])
        table = struct.unpack_from('<I', good, 24)[0]
        start = struct.unpack_from('<I', good, table + 12)[0]
        mutations = [(0, '<I', 0), (4, '<I', 0x10016), (8, '<I', 2),
                     (12, '<I', 0), (16, '<I', 0), (20, '<I', 0xffffffff),
                     (24, '<I', 4), (28, '<I', 0), (table, '<I', len(good)),
                     (table+4, '<I', 0), (table+8, '<I', 6 << 16 | 1),
                     (table+8, '<I', 0xffff), (table+12, '<I', 0),
                     (table+16, '<I', start+3), (start-8, '<f', float('nan')),
                     (start-4, '<f', float('inf')), (start+2, '<H', 0)]
        for offset, fmt, value in mutations:
            bad = bytearray(good)
            struct.pack_into(fmt, bad, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                self.api.parse_anm(bytes(bad))
        for bad in (good[:20], good[:-1], good+b'x', b'x'*(self.api.MAX_ANM_BYTES+1)):
            with self.assertRaises(ValueError):
                self.api.parse_anm(bad)

    def test_rejects_unknown_bone_and_invalid_numeric_domains(self):
        clip = self.api.parse_anm(fixture())
        for frame in (-1., float('nan'), float('inf'), 1e20, True):
            with self.assertRaises(ValueError):
                clip.sample('bip01 l thigh', frame)
        with self.assertRaises(ValueError):
            clip.sample('unknown', 0.)
        with self.assertRaises(ValueError):
            self.api.Pose((0., 0., 0.), (0., 0., 0., 0.)).matrix()
        with self.assertRaises(ValueError):
            self.api.compose((0.,)*16, (0.,)*16)

    def test_native_constant_sentinel_one_and_cubic_oversize_rejections(self):
        good = bytearray(fixture([4., 0., 0., 0., 0., 0.]))
        table = struct.unpack_from('<I', good, 24)[0]
        start = struct.unpack_from('<I', good, table+12)[0]
        struct.pack_into('<I', good, table+16, start+1)
        self.assertEqual(self.api.parse_anm(bytes(good)).sample('bip01 l thigh', 10.).translation[0], 4.)
        # Limits must reject before allocating/evaluating thousands of keys.
        knots = tuple(range(self.api.MAX_KEYS+1))
        oversized = fixture([(knots, (1,)*(len(knots)+2)), 0., 0., 0., 0., 0.])
        with self.assertRaises(ValueError):
            self.api.parse_anm(oversized)
        for knots in ((0, 50, 49, 150), (0, 50, 50, 50, 50, 150)):
            with self.assertRaises(ValueError):
                self.api.parse_anm(fixture([(knots, (1,)*(len(knots)+2)), 0., 0., 0., 0., 0.]))

    def test_out_of_range_payload_and_nonrigid_frames_fail_closed(self):
        with self.assertRaises(ValueError):
            self.api.parse_anm(fixture([self.api.MAX_COMPONENT*2, 0., 0., 0., 0., 0.]))
        good = bytearray(fixture([((0, 150), (0, 1, 2, 65535)), 0., 0., 0., 0., 0.]))
        table = struct.unpack_from('<I', good, 24)[0]
        start = struct.unpack_from('<I', good, table+12)[0]
        struct.pack_into('<f', good, start-4, 100.)
        with self.assertRaises(ValueError):
            self.api.parse_anm(bytes(good))
        for offset, value in ((0, -1.), (0, 2.), (3, .5), (12, float('nan'))):
            bad = list(self.api.IDENTITY)
            bad[offset] = value
            with self.assertRaises(ValueError):
                self.api.validate_matrix(bad)


if __name__ == '__main__':
    unittest.main()
