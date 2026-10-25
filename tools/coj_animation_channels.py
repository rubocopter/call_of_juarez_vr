"""Event-free CoJ ANM1 mathematics, not an engine animation API.

Only version 0x10015/type 3 with the proven 32/160 time encoding is admitted.
Evidence: COJ_ARM_SKINNING_AND_AIM.md and the isolated original readers
+0x253190/+0x253DC0, mode converter +0x251E90 and TRS +0x219530.
The compressed scalar is a clamped cubic B-spline, NOT linear key interpolation.
Units/basis stay native (centimetres, row axes); composition is local * parent.
No imports of native libraries, game calls, resource lookup or asset export.
Use coj_lower_body_profiles.py for hash-bound original resource admission.
"""
from __future__ import annotations

from bisect import bisect_right
from dataclasses import dataclass
import hashlib
import math
import struct

MAX_ANM_BYTES = 4 * 1024 * 1024
MAX_BONES = 512
MAX_KEYS = 4096
MAX_TOTAL_KEYS = 262144
MAX_COMPONENT = 1_000_000.
IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
# Literal float32 table from exact engine SHA db69bc35...131ec8, +0x38FD58.
_H = 0.7071067690849304
_MODES = ((0., 0., 0., 1.), (_H, 0., 0., _H), (0., 0., -_H, _H),
          (.5, -.5, .5, .5), (_H, 0., _H, 0.), (.5, .5, .5, .5))
_DEG_HALF = struct.unpack('<f', struct.pack('<f', math.pi / 360.))[0]


def _number(value, low, high, label):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not low <= value <= high:
        raise ValueError(f'invalid {label}')
    return float(value)


def _f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def _vector(values, count, limit, label):
    if len(values) != count:
        raise ValueError(f'invalid {label} length')
    return tuple(_number(v, -limit, limit, label) for v in values)


def validate_matrix(matrix):
    """Reject non-rigid/reflected/non-affine frames; never repair a basis."""
    m = _vector(matrix, 16, MAX_COMPONENT, 'matrix')
    if any(abs(m[i]) > 1e-6 for i in (3, 7, 11)) or abs(m[15]-1.) > 1e-6:
        raise ValueError('non-affine matrix')
    rows = [m[i:i+3] for i in (0, 4, 8)]
    for i in range(3):
        for j in range(i, 3):
            if abs(sum(a*b for a, b in zip(rows[i], rows[j])) - (i == j)) > 5e-4:
                raise ValueError('non-rigid matrix')
    a, b, c = rows
    determinant = a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
    if abs(determinant-1.) > 5e-4:
        raise ValueError('reflected matrix')
    return m


def compose(local, parent_world):
    """Native row-major local * parent_world; all sixteen components retained."""
    a, b = validate_matrix(local), validate_matrix(parent_world)
    return validate_matrix(tuple(sum(a[i*4+k]*b[k*4+j] for k in range(4))
                                 for i in range(4) for j in range(4)))


@dataclass(frozen=True)
class Pose:
    translation: tuple[float, float, float]
    quaternion: tuple[float, float, float, float]  # XYZW, native mode applied

    def matrix(self):
        t = _vector(self.translation, 3, MAX_COMPONENT, 'translation')
        x, y, z, w = _vector(self.quaternion, 4, 1.001, 'quaternion')
        if abs(x*x+y*y+z*z+w*w-1.) > 1e-4:
            raise ValueError('invalid unit quaternion')
        # Proven skeletal converter; do not transpose or reconstruct an axis.
        return validate_matrix((1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), 0.,
                                2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), 0.,
                                2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), 0.,
                                *t, 1.))


def _quaternion(angles, mode):
    # +0x251E90 builds XYZ Euler components, NEGATES W, then left-multiplies
    # table[mode]. It is not the conventional Euler-to-quaternion function.
    sx, sy, sz = (math.sin(a*_DEG_HALF) for a in angles)
    cx, cy, cz = (math.cos(a*_DEG_HALF) for a in angles)
    x, y, z, w = (sx*cy*cz+cx*sy*sz, cx*sy*cz-sx*cy*sz,
                  cx*cy*sz+sx*sy*cz, -(cx*cy*cz-sx*sy*sz))
    a, b, c, d = _MODES[mode]
    return tuple(_f32(v) for v in (d*x+a*w+b*z-c*y, d*y-a*z+b*w+c*x,
                                   d*z+a*y-b*x+c*w, d*w-a*x-b*y-c*z))


@dataclass(frozen=True)
class ScalarChannel:
    minimum: float
    step: float
    knots: tuple[int, ...]
    controls: tuple[int, ...]

    def sample(self, ticks):
        if not self.knots:
            return self.minimum
        # Native comparisons use the integer quotient, interpolation the whole
        # truncated tick count. Clamped endpoints are explicit in +0x253DC0.
        quotient = ticks // 32
        if quotient <= self.knots[0]:
            return _f32(self.minimum+self.controls[0]*self.step)
        if quotient >= self.knots[-1]:
            return _f32(self.minimum+self.controls[-1]*self.step)
        span = bisect_right(self.knots, quotient)-1
        vector = (self.knots[0],)*3 + self.knots + (self.knots[-1],)*3
        k = span+3
        values = [self.minimum+self.step*self.controls[span+j] for j in range(4)]
        # de Boor on the four local controls, degree three. The common native
        # 1/4800 time scale cancels; no animation clock is advanced here.
        time = ticks/32.
        for degree in range(1, 4):
            for j in range(3, degree-1, -1):
                i = k-3+j
                left, right = vector[i], vector[i+4-degree]
                if right <= left:
                    raise ValueError('degenerate cubic interval')
                alpha = (time-left)/(right-left)
                values[j] = (1.-alpha)*values[j-1]+alpha*values[j]
        return _f32(values[3])


@dataclass(frozen=True)
class BoneChannels:
    name: str
    record_index: int
    name_hash: int
    flags: int
    mode: int
    channels: tuple[ScalarChannel, ...]


@dataclass(frozen=True)
class Animation:
    sha256: str
    bones: tuple[BoneChannels, ...]

    def sample(self, bone_name, authored_frame):
        frame = _number(authored_frame, 0., (65536*32-1)/160, 'authored frame')
        # Original +0x253190 truncates float32 caller frame * header[4].
        ticks = int(_f32(frame)*160)
        if ticks // 32 > 65535:
            raise ValueError('frame exceeds native uint16 knot domain')
        bone = next((b for b in self.bones if b.name == bone_name), None)
        if bone is None:
            raise ValueError('unknown animation bone')
        values = tuple(c.sample(ticks) for c in bone.channels)
        pose = Pose(values[:3], _quaternion(values[3:], bone.mode))
        pose.matrix()  # Fail before publishing a malformed pose.
        return pose


def parse_anm(data: bytes) -> Animation:
    """Bounded structural decoder. Raw bytes alone do NOT admit a game resource.

    For constant descriptors end-start is 0 or 1 and the scalar is start-8.
    Other descriptors have uint16 knots, followed by count+2 uint16 controls.
    Shared/packed scalar storage is native, so overlap is not itself corruption.
    """
    if not isinstance(data, bytes) or not 32 <= len(data) <= MAX_ANM_BYTES:
        raise ValueError('ANM size/type limit')
    magic, version, kind, divisor, ticks, count, table, size = struct.unpack_from('<8I', data)
    if (magic, version, kind, divisor, ticks) != (0x314d4e41, 0x10015, 3, 32, 160):
        raise ValueError('unsupported ANM format/time encoding')
    if size != len(data) or not 1 <= count <= MAX_BONES or table < 32 or table % 4 or table+60*count > size:
        raise ValueError('invalid ANM table/size')
    floor = table+60*count
    bones, names, hashes, total = [], set(), set(), 0
    for index in range(count):
        name_offset, hashed, flags, *offsets = struct.unpack_from('<15I', data, table+60*index)
        if not 32 <= name_offset < table:
            raise ValueError('invalid name offset')
        end_name = data.find(b'\0', name_offset, min(table, name_offset+128))
        if end_name < 0:
            raise ValueError('unterminated/oversize bone name')
        raw = data[name_offset:end_name]
        if not raw or any(c < 32 or c > 126 for c in raw):
            raise ValueError('invalid ASCII bone name')
        name = raw.decode('ascii')
        check = 0
        for c in raw:
            check = (check*41+c) & 0xffffffff
        if hashed != check or name.casefold() in names or hashed in hashes:
            raise ValueError('invalid/duplicate bone identity')
        if flags & 0xffff not in (1, 4, 16) or flags >> 16 >= 6:
            raise ValueError('unsupported bone flags/rotation mode')
        names.add(name.casefold())
        hashes.add(hashed)
        channels = []
        for start, end in zip(offsets[::2], offsets[1::2]):
            if not floor+8 <= start <= end <= size:
                raise ValueError('scalar descriptor outside payload')
            minimum = struct.unpack_from('<f', data, start-8)[0]
            _number(minimum, -MAX_COMPONENT, MAX_COMPONENT, 'scalar minimum')
            distance = end-start
            if distance <= 1:
                channel = ScalarChannel(minimum, 0., (), ())
            else:
                if start % 2 or distance % 2:
                    raise ValueError('unaligned scalar keys')
                n = distance//2
                total += n
                if not 2 <= n <= MAX_KEYS or total > MAX_TOTAL_KEYS or end+2*(n+2) > size:
                    raise ValueError('scalar key size limit')
                step = struct.unpack_from('<f', data, start-4)[0]
                _number(step, 0., MAX_COMPONENT, 'scalar quantization')
                knots = struct.unpack_from('<'+'H'*n, data, start)
                # Original walking clips use triple interior knots (C0 joins).
                # Native upper_bound selects the span after an equal run.
                if (knots[0] == knots[-1] or any(a > b for a, b in zip(knots, knots[1:]))
                        or knots[0] == knots[1] or knots[-1] == knots[-2]
                        or any(knots[i] == knots[i+3] for i in range(n-3))):
                    raise ValueError('unordered/degenerate scalar knots')
                controls = struct.unpack_from('<'+'H'*(n+2), data, end)
                for c in controls:
                    _number(minimum+step*c, -MAX_COMPONENT, MAX_COMPONENT, 'decoded scalar')
                channel = ScalarChannel(minimum, step, knots, controls)
            channels.append(channel)
        bones.append(BoneChannels(name, index, hashed, flags & 0xffff, flags >> 16, tuple(channels)))
    return Animation(hashlib.sha256(data).hexdigest(), tuple(bones))
