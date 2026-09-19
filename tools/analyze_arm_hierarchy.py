"""Compare observed arm frames with rigid parent/child propagation in a run log."""
import argparse
import math
import re
from pathlib import Path


def vec(s):
    return tuple(float(x) for x in s.strip('()').split(','))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def rotate(v, axis, degrees):
    n = math.sqrt(dot(axis, axis))
    if n < 1e-10 or abs(degrees) < 1e-10:
        return v
    a = tuple(x/n for x in axis)
    c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    av = cross(a, v)
    return tuple(v[i]*c + av[i]*s + a[i]*dot(a, v)*(1-c) for i in range(3))


def distance(a, b):
    return math.sqrt(sum((x-y)**2 for x, y in zip(a, b)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    records = {}
    for line in args.log.read_text(encoding='utf-8').splitlines():
        m = re.search(r'event=(body_arm_tracking|body_arm_write_probe) result=(applied|changed) detail=(.*)', line)
        if not m:
            continue
        fields = dict(part.split('=', 1) for part in m[3].split(';') if '=' in part)
        records.setdefault((fields['frame_sequence'], fields['side']), {})[m[1]] = fields
    errors = {k: [] for k in ('forearm_full', 'foretwist_full', 'foretwist_parent_only', 'hand_full', 'hand_parent_only', 'hand_without_twist')}
    for key, pair in records.items():
        if len(pair) != 2:
            continue
        natural, observed = pair['body_arm_tracking'], pair['body_arm_write_probe']
        upper_axis, fore_axis = vec(natural['upper_rotation_axis']), vec(natural['forearm_rotation_axis'])
        upper_angle, fore_angle = float(natural['upper_rotation_degrees']), float(natural['forearm_rotation_degrees'])
        # Convert the recorded element-local twist axis using the POST-write basis:
        # rotation leaves its own axis invariant, so this equals the pre-write world axis.
        up, forward = vec(observed['foretwist_element_up']), vec(observed['foretwist_element_forward'])
        xaxis = cross(up, forward)
        local = vec(natural['forearm_twist_native_axis'])
        twist_axis = tuple(xaxis[i]*local[0]+up[i]*local[1]+forward[i]*local[2] for i in range(3))
        twist_angle = float(natural['forearm_twist_degrees'])
        for element in ('forearm', 'foretwist', 'hand'):
            for axis in ('up', 'forward'):
                field = f'{element}_element_{axis}'
                actual = vec(observed[field])
                if element != 'forearm':
                    actual = rotate(actual, twist_axis, -twist_angle)
                parent = rotate(vec(natural[field]), upper_axis, upper_angle)
                full = rotate(parent, fore_axis, fore_angle)
                if element == 'hand':
                    errors['hand_without_twist'].append(distance(vec(observed[field]), full))
                errors[f'{element}_full'].append(distance(actual, full))
                if element != 'forearm':
                    errors[f'{element}_parent_only'].append(distance(actual, parent))
    for name, values in errors.items():
        if values:
            print(f'{name}: n={len(values)} mean_axis_error={sum(values)/len(values):.6f} max={max(values):.6f}')


if __name__ == '__main__':
    main()
