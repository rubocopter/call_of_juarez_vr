"""Bake one CC0 ammunition node and its albedo; no glTF parser in the game."""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/models/revolver_ammo.gltf'
SOURCE_SHA256 = '2cb1acda3e7e16aafd2778eabe9a5456f48847e35ad717d08cdbbb3f3a45242d'
ALBEDO_SHA256 = '74b47d1f2ff814806a20e3e74c4598b433875a2eb7cf32c9a5a0d1b7322f3a64'

def rgb_png(png):
    """Decode only the pinned non-interlaced RGB8 PNG, using the standard library."""
    if hashlib.sha256(png).hexdigest() != ALBEDO_SHA256:
        raise ValueError('Unknown cartridge albedo identity')
    if png[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('PNG signature')
    offset, compressed = 8, bytearray()
    while offset < len(png):
        size = struct.unpack_from('>I', png, offset)[0]
        kind = png[offset + 4:offset + 8]
        data = png[offset + 8:offset + 8 + size]
        crc = struct.unpack_from('>I', png, offset + 8 + size)[0]
        if zlib.crc32(kind + data) != crc:
            raise ValueError('PNG chunk integrity')
        if kind == b'IHDR' and struct.unpack('>IIBBBBB', data) != (1024, 1024, 8, 2, 0, 0, 0):
            raise ValueError('Unexpected PNG layout')
        if kind == b'IDAT':
            compressed.extend(data)
        offset += 12 + size
    raw = zlib.decompress(compressed)
    stride = 1024 * 3
    if len(raw) != 1024 * (stride + 1):
        raise ValueError('Unexpected PNG payload')
    rows, previous = [], bytearray(stride)
    for y in range(1024):
        start = y * (stride + 1)
        filter_type = raw[start]
        if filter_type > 4:
            raise ValueError('Unknown PNG filter')
        row = bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            left, up = (row[x - 3] if x >= 3 else 0), previous[x]
            diagonal = previous[x - 3] if x >= 3 else 0
            if filter_type == 1:
                value = left
            elif filter_type == 2:
                value = up
            elif filter_type == 3:
                value = (left + up) // 2
            elif filter_type == 4:
                p = left + up - diagonal
                a, b, c = abs(p - left), abs(p - up), abs(p - diagonal)
                value = left if a <= b and a <= c else up if b <= c else diagonal
            else:
                value = 0
            row[x] = (row[x] + value) & 255
        rows.append(row)
        previous = row
    return rows

def generate():
    source = SOURCE.read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unknown cartridge glTF identity')
    gltf = json.loads(source)
    buffer = base64.b64decode(gltf['buffers'][0]['uri'].split(',')[1], validate=True)
    if len(buffer) != gltf['buffers'][0]['byteLength']:
        raise ValueError('Buffer length')
    node = next(n for n in gltf['nodes'] if n['name'] == '357Bullet')
    primitive = gltf['meshes'][node['mesh']]['primitives'][0]
    def view(index):
        v = gltf['bufferViews'][index]
        start = v.get('byteOffset', 0)
        result = buffer[start:start + v['byteLength']]
        if len(result) != v['byteLength'] or 'byteStride' in v:
            raise ValueError('Unexpected buffer view')
        return result
    def accessor(index, fmt, expected_type, component):
        a = gltf['accessors'][index]
        if a['type'] != expected_type or a['componentType'] != component or 'sparse' in a:
            raise ValueError('Unexpected accessor')
        data = view(a['bufferView'])
        start, size = a.get('byteOffset', 0), struct.calcsize(fmt)
        return [struct.unpack_from(fmt, data, start + i * size) for i in range(a['count'])]
    attr = primitive['attributes']
    positions = accessor(attr['POSITION'], '<3f', 'VEC3', 5126)
    normals = accessor(attr['NORMAL'], '<3f', 'VEC3', 5126)
    uv = accessor(attr['TEXCOORD_0'], '<2f', 'VEC2', 5126)
    indices = [x[0] for x in accessor(primitive['indices'], '<H', 'SCALAR', 5123)]
    if len(positions) != 83 or len(normals) != 83 or len(uv) != 83 or len(indices) != 276:
        raise ValueError('Unexpected cartridge topology')
    if any(i >= len(positions) for i in indices) or any(not math.isfinite(v) for array in (positions, normals, uv) for p in array for v in p):
        raise ValueError('Invalid cartridge data')
    # Only this one loose node. Scene translations/other weapons never enter runtime.
    tip_y = max(p[1] for p in positions)
    tip_z = -.0475
    vertices = [(x, z, tip_z + tip_y - y) for x, y, z in positions]
    normals = [(x, z, -y) for x, y, z in normals]
    # Crop the source atlas to the loose cartridge's UV islands. Preserve orientation.
    crop = (0, 640, 300, 384)
    uv = [(u * 1024 / crop[2], (v * 1024 - crop[1]) / crop[3]) for u, v in uv]
    if any(not 0 <= v <= 1 for p in uv for v in p):
        raise ValueError('UV outside cartridge atlas crop')
    albedo_index = gltf['materials'][primitive['material']]['pbrMetallicRoughness']['baseColorTexture']['index']
    image = gltf['images'][gltf['textures'][albedo_index]['source']]
    rows = rgb_png(view(image['bufferView']))
    width, height = 128, 160
    pixels = []
    for y in range(height):
        y0, y1 = crop[1] + y * crop[3] // height, crop[1] + (y + 1) * crop[3] // height
        for x in range(width):
            x0, x1 = x * crop[2] // width, (x + 1) * crop[2] // width
            n = (x1 - x0) * (y1 - y0)
            r, g, b = [sum(rows[sy][sx * 3 + c] for sy in range(y0, y1) for sx in range(x0, x1)) // n for c in range(3)]
            pixels.append(0xff000000 | r << 16 | g << 8 | b)
    f = lambda v: f'{v:.9f}F'
    lines = ['#pragma once', '#include "runtime/vr_types.hpp"', '#include <array>',
             '// Generated by tools/import_textured_cartridge.py; CC0 loafbrr_1.',
             'namespace cojvr::runtime::textured_reload_mesh {',
             f'inline constexpr float centre_z = {f((min(v[2] for v in vertices) + max(v[2] for v in vertices)) / 2)};']
    for name, array, kind in [('vertices', vertices, 'Vec3'), ('normals', normals, 'Vec3'), ('uv', uv, 'Vec2')]:
        lines += [f'inline constexpr std::array<{kind},83> {name}' + '{{']
        lines += ['    {' + ','.join(map(f, p)) + '},' for p in array]
        lines += ['}};']
    lines += ['inline constexpr std::array<std::array<unsigned,3>,92> triangles{{']
    lines += ['    {' + ','.join(map(str, indices[i:i + 3])) + '},' for i in range(0, len(indices), 3)]
    lines += ['}};', '}']
    atlas = ['#pragma once', '#include <array>', '#include <cstdint>',
             '// Generated albedo crop; CC0 loafbrr_1. See assets/models/README.md.',
             'namespace cojvr::backends::openvr::reload_atlas {',
             f'inline constexpr unsigned width={width},height={height};',
             f'inline constexpr std::array<std::uint32_t,{len(pixels)}> pixels' + '{{']
    atlas += ['    ' + ','.join(f'0x{p:08X}U' for p in pixels[i:i + 8]) + ',' for i in range(0, len(pixels), 8)]
    atlas += ['}};', '}']
    return {ROOT / 'src/runtime/textured_cartridge_mesh.hpp': '\n'.join(lines) + '\n',
            ROOT / 'src/backends/openvr/reload_cartridge_texture.hpp': '\n'.join(atlas) + '\n'}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for target, output in generate().items():
        if args.check:
            if target.read_text(encoding='utf-8') != output:
                raise SystemExit(f'Generated cartridge differs: {target.name}')
        else:
            target.write_text(output, encoding='utf-8')
    print('Textured CC0 cartridge: 83 vertices, 92 triangles, cropped 128x160 albedo; identities verified')
