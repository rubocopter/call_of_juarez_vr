#!/usr/bin/env python3
"""Read-only inventory of pinned CoJ originals; never extracts or executes assets.

Explicit backup paths must be used when the game is staged. JSON contains only
authored metadata/provenance, not animation bytes. State times and SeqTrack
parameters are separate observations: neither is a measured runtime duration.
"""
from __future__ import annotations

import argparse
from contextlib import ExitStack
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys
import zipfile

from inspect_java_bytecode import Reader, parse_constant_pool, parse_class, disassemble


# Same original identities as tools/stage_d3d9_proxy.ps1. No CLI override.
PINNED_SHA256 = {
    'code': 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12',
    'data0': '5eb6ec4856e53226dfabad929522706a21f02d428923a8c9c38804cc2cf11a94',
}
RESEARCH = 'docs/research/COJ_RELOAD_OWNERSHIP.md#unimplemented-families-original-archive-reload-contracts'
EQUIPMENT = 'Data/Player/Equipment/'
MAX_MEMBER = 1024 * 1024
MAX_SCRIPT_BYTES = 4 * MAX_MEMBER
MAX_SCRIPTS = 32
MAX_DEPTH = 12

# Deliberately exact profiles. Reserve/index semantics come from the research,
# not from clip names, field order, or a generic firearm taxonomy.
# family, weapon suffix, whole-clip, (ammo suffix, capacity, reserve) per index
PROFILES = (
    ('Winchester', 'RifleWinchester', False, (('RifleWinchester', 12, 'rifle'),)),
    ('WinchesterScope', 'RifleWinchesterScope', False, (('RifleWinchesterScope', 12, 'rifle'),)),
    ('Volcanic', 'PistolVolcanic', False, (('PistolVolcanic', 8, 'pistol'),)),
    ('Remington', 'RifleRemington', True, (('ShotgunRemington', 1, 'shotgun'), ('ShotgunRemington', 1, 'shotgun'))),
    ('RemingtonSawOff', 'RifleRemingtonSawOff', True, (('ShotgunRemington', 1, 'shotgun'), ('ShotgunRemington', 1, 'shotgun'))),
    ('Derringer', 'PistolDerringer', True, (('PistolDerringer', 1, 'pistol'), ('PistolDerringer', 1, 'pistol'))),
    ('Lemant', 'PistolLemant', False, (('RifleLemant', 1, 'rifle'), ('PistolLemant', 8, 'pistol'))),
    ('Schofield', 'PistolSchofield', True, (('PistolSchofield', 6, 'pistol'),)),
)
INHERITED_FIELDS = {
    'WpnLogicRifleWinchesterScope': 'WpnLogicRifleWinchester',
    'WpnAmmoRifleWinchesterScope': 'WpnAmmoRifleWinchester',
}


def unknown(reason, definitions=()):
    return {'status': 'unknown', 'reason': reason, 'definitions': list(definitions)}


class Archive:
    """Archive-local, bounded member reads from the already verified handle."""
    def __init__(self, archive, label):
        self.zip = archive
        self.label = label
        self.members = {}
        self.sources = {}
        for info in archive.infolist():
            if info.is_dir():
                continue
            key = info.filename.replace('\\', '/').casefold()
            if key in self.members:
                raise ValueError(f'ambiguous duplicate archive member: {info.filename}')
            self.members[key] = info

    def read(self, member):
        info = self.members.get(member.casefold())
        if info is None:
            raise ValueError(f'missing member: {member}')
        if info.file_size > MAX_MEMBER:
            raise ValueError(f'member size limit: {member}')
        with self.zip.open(info) as stream:
            data = stream.read(MAX_MEMBER + 1)
        if len(data) > MAX_MEMBER or len(data) != info.file_size:
            raise ValueError(f'member size limit/mismatch: {member}')
        source = {'archive': self.label, 'member': info.filename,
                  'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)}
        self.sources[info.filename] = source
        return data, source


class NativeFacts:
    """Only literal this.field stores in the selected straight-line constructors.

    This is not a JVM evaluator. Unsupported/conditional/repeated stores remain
    unknown. Inheritance is limited to the two explicitly observed Scope links.
    """
    def __init__(self, archive):
        self.archive = archive
        self.cache = {}

    def inspect(self, name):
        if name not in self.cache:
            try:
                data, source = self.archive.read(name + '.class')
                pool, methods = parse_class(data)
                reader = Reader(data)
                reader.read(8)
                parse_constant_pool(reader)
                reader.u2()
                this, parent = pool.describe(reader.u2()), pool.describe(reader.u2())
                if this != name:
                    raise ValueError('class identity mismatch')
                self.cache[name] = (pool, methods, source, parent)
            except (ValueError, IndexError) as exc:
                self.cache[name] = str(exc)
        return self.cache[name]

    def source(self, name):
        facts = self.inspect(name)
        if isinstance(facts, str):
            return unknown(facts)
        return {'status': 'known', 'class': name, 'superclass': facts[3], 'source': facts[2]}

    def field(self, name, field):
        facts = self.inspect(name)
        if isinstance(facts, str):
            return unknown(facts)
        pool, methods, source, parent = facts
        ctors = [m for m in methods if (m.name, m.descriptor) == ('<init>', '()V') and m.code]
        if len(ctors) != 1:
            return unknown('missing/ambiguous no-argument constructor')
        code = ctors[0].code
        offsets = [int(line.split(':', 1)[0]) for line in disassemble(code, pool)]
        conditional = any(0x99 <= code[p] <= 0xab or code[p] in (0xc6, 0xc7, 0xc8, 0xc9) for p in offsets)
        stores = []
        for i, p in enumerate(offsets):
            if code[p] != 0xb5:
                continue
            ref = pool.get(int.from_bytes(code[p+1:p+3], 'big'))
            if ref[0] != 'Fieldref':
                continue
            nt = pool.get(ref[2])
            if pool.utf8(nt[1]) != field:
                continue
            desc = pool.utf8(nt[2])
            item = {'source': dict(source, method='<init>()V', field=field, descriptor=desc, bytecode_offset=p)}
            value = None
            if i >= 2 and code[offsets[i-2]] == 0x2a:
                v = offsets[i-1]
                op = code[v]
                if 0x02 <= op <= 0x08:
                    value = op - 3
                elif op in (0x10, 0x11):
                    value = int.from_bytes(code[v+1:v+(2 if op == 0x10 else 3)], 'big', signed=True)
                elif op in (0x12, 0x13):
                    entry = pool.get(int.from_bytes(code[v+1:v+(2 if op == 0x12 else 3)], 'big'))
                    if entry[0] == 'String':
                        value = pool.utf8(entry[1])
                    elif entry[0] == 'Integer':
                        value = entry[1]
            if desc == 'Z' and value in (0, 1):
                value = bool(value)
            item['value'] = value
            stores.append(item)
        if stores:
            if conditional or len(stores) != 1 or stores[0]['value'] is None:
                return unknown('conditional, repeated or unsupported constructor store', stores)
            return dict(stores[0], status='known')
        if INHERITED_FIELDS.get(name) == parent:
            return self.field(parent, field)
        return unknown('no explicit literal store in selected constructor chain')


# Preserve line numbers while removing comments; quoted // and include paths are
# not comments. No macro evaluation, filesystem search or asset payload reads.
LEX = re.compile(r'"[^"\r\n]*"|//[^\n]*|/\*[\s\S]*?\*/')
CALL = re.compile(r'(?m)^[ \t]*(!include|WpnStateTime|SeqTrackAlias|SeqTrack)[ \t]*\(([^()]*?)\)')
START = re.compile(r'(?<![\w!])(!include|WpnStateTime|SeqTrackAlias|SeqTrack)[ \t]*\(')
NUMBER = re.compile(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?\Z')
STRING = re.compile(r'"([^"\r\n]*)"\Z')


def member_path(parent, relative):
    relative = relative.replace('\\', '/')
    if not relative or relative.startswith('/') or ':' in relative or '\x00' in relative:
        raise ValueError('unsafe archive include path')
    parts = list(PurePosixPath(parent).parent.parts)
    for part in relative.split('/'):
        if part == '..':
            if not parts:
                raise ValueError('archive include escapes root')
            parts.pop()
        elif part not in ('', '.'):
            parts.append(part)
    result = '/'.join(parts)
    if not result.casefold().startswith(EQUIPMENT.casefold()) or not result.lower().endswith(('.scr', '.def')):
        raise ValueError('archive include outside equipment scripts')
    return result


class ScriptGraph:
    def __init__(self, archive):
        self.archive = archive
        self.definitions = []
        self.issues = []
        self.total = 0
        self.count = 0

    def collect(self, root):
        self.visit(root, ())
        return self

    def visit(self, member, chain):
        try:
            key = member.casefold()
            if key in chain:
                raise ValueError('script include cycle')
            if len(chain) >= MAX_DEPTH or self.count >= MAX_SCRIPTS:
                raise ValueError('script traversal limit')
            self.count += 1
            data, source = self.archive.read(member)
            self.total += len(data)
            if self.total > MAX_SCRIPT_BYTES:
                raise ValueError('script byte limit')
            # Real originals use CRCRLF. Normalize as a single logical newline.
            text = data.decode('cp1252').replace('\r\r\n', '\n').replace('\r\n', '\n').replace('\r', '\n')
            text = LEX.sub(lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text)
            calls = list(CALL.finditer(text))
            valid_starts = {m.start(1) for m in calls}
            for m in START.finditer(text):
                if m.start() not in valid_starts:
                    raise ValueError('malformed script call')
            for m in calls:
                kind, body = m.groups()
                args = [s.strip() for s in body.split(',')]
                provenance = dict(source, line=text.count('\n', 0, m.start(1)) + 1)
                def string(s):
                    match = STRING.fullmatch(s)
                    if not match:
                        raise ValueError('expected quoted script string')
                    return match[1]
                def number(s):
                    if not NUMBER.fullmatch(s) or not math.isfinite(float(s)):
                        raise ValueError('invalid authored numeric parameter')
                    return float(s)
                if kind == '!include':
                    if len(args) != 1:
                        raise ValueError('invalid include')
                    self.visit(member_path(member, string(args[0])), chain + (key,))
                elif kind == 'WpnStateTime':
                    if len(args) != 2:
                        raise ValueError('invalid state-time definition')
                    if 'RELOAD' in args[0]:
                        value = number(args[1])
                        if value < 0 or not re.fullmatch(r'(ALT_)?RELOAD_(BEG|LOOP|END)', args[0]):
                            raise ValueError('invalid reload state time')
                        self.definitions.append({'kind': kind, 'name': args[0], 'literal': args[1], 'value': value, 'source': provenance})
                elif kind == 'SeqTrack':
                    if len(args) != 7:
                        raise ValueError('invalid SeqTrack arity')
                    name, clip = string(args[0]), string(args[1])
                    values = [number(s) for s in args[2:]]
                    if values[2] <= 0:
                        raise ValueError('invalid SeqTrack rate')
                    self.definitions.append({'kind': kind, 'name': name, 'clip': clip, 'parameters': args[2:], 'source': provenance})
                else:
                    if len(args) != 2:
                        raise ValueError('invalid SeqTrackAlias arity')
                    self.definitions.append({'kind': kind, 'name': string(args[0]), 'target': string(args[1]), 'source': provenance})
        except (ValueError, UnicodeError) as exc:
            self.issues.append({'member': member, 'reason': str(exc)})

    def lookup(self, name, kinds, chain=()):
        definitions = [d for d in self.definitions if d['name'] == name and d['kind'] in kinds]
        if self.issues or len(definitions) != 1 or name in chain or len(chain) >= MAX_DEPTH:
            return unknown('incomplete graph, absent/ambiguous definition or alias cycle', definitions)
        item = definitions[0]
        result = {'status': 'known', 'definitions': definitions}
        if item['kind'] == 'SeqTrackAlias':
            target = self.lookup(item['target'], kinds, chain + (name,))
            if target['status'] != 'known':
                return unknown('alias target unknown', definitions)
            result['resolved'] = target['resolved']
            result['alias_chain'] = [item] + target.get('alias_chain', [])
        elif item['kind'] == 'SeqTrack':
            result['resolved'] = item
        return result


def inspect_profiles(code, data):
    native = NativeFacts(code)
    families = []
    for family, suffix, whole, ammunition in PROFILES:
        weapon, logic = 'Weapon' + suffix, 'WpnLogic' + suffix
        fields = {field: native.field(logic, field) for field in (
            'm_bReloadWholeClipAtOnce', 'm_sStateTimesScript', 'm_eWeaponAnimType')}
        fields['class'] = logic
        script = fields['m_sStateTimesScript']
        state = ScriptGraph(data)
        if script['status'] == 'known' and isinstance(script['value'], str):
            # Native literals also stay within the exact equipment namespace.
            try:
                path = member_path(EQUIPMENT + 'root.scr', script['value'][len(EQUIPMENT):]) if script['value'].startswith(EQUIPMENT) else None
                if path is None:
                    raise ValueError('state script outside equipment')
                state.collect(path)
            except ValueError as exc:
                state.issues.append({'reason': str(exc)})
        else:
            state.issues.append({'reason': 'native state-script literal unknown'})
        root = f'{EQUIPMENT}{family}/{family}.scr'
        animation = ScriptGraph(data).collect(root)
        prefix = 'Winchester' if family == 'WinchesterScope' else family
        phases = {}
        modes = ['base']
        if any(d['name'].startswith('ALT_RELOAD_') for d in state.definitions) or any(d['name'].startswith(prefix + '_A_Reload') for d in animation.definitions):
            modes.append('alternate')
        for mode in modes:
            phases[mode] = {}
            for phase, symbol, ending in (('begin', 'BEG', 'Beg'), ('body', 'LOOP', 'Loop'), ('end', 'END', 'End')):
                state_symbol = ('ALT_' if mode == 'alternate' else '') + 'RELOAD_' + symbol
                track = prefix + ('_A' if mode == 'alternate' else '') + '_Reload' + ending
                phases[mode][phase] = {
                    'state_symbol': state_symbol,
                    'state_time': state.lookup(state_symbol, ('WpnStateTime',)),
                    'track_name': track,
                    'animation': animation.lookup(track, ('SeqTrack', 'SeqTrackAlias')),
                }
        classes = [weapon] + (['WeaponPistolSchofield_A', 'WeaponPistolSchofield_B'] if family == 'Schofield' else [])
        families.append({
            'family': family, 'classes': [native.source(c) for c in classes], 'logic': fields,
            'accounting': {
                'provenance': RESEARCH, 'whole_clip': whole,
                'transfer_boundary': 'up to remaining capacity per index' if whole else 'at most one unit total; first eligible index ascending',
                'caller_selectable_index': 'unknown',
                'ammo': [{'index': i, 'class': 'WpnAmmo' + ammo, 'capacity': capacity, 'reserve': reserve,
                          'capacity_observation': native.field('WpnAmmo' + ammo, 'm_iCapacity')}
                         for i, (ammo, capacity, reserve) in enumerate(ammunition)],
            },
            'animation_script': root, 'state_issues': state.issues, 'animation_issues': animation.issues,
            'phases': phases,
            'reload_clips': [d for d in animation.definitions if 'reload' in d['name'].lower()],
        })
    return families


def build_inventory(code_path, data0_path):
    """Hash both original archives before reading selected members; no writes."""
    paths = {'code': Path(code_path).resolve(), 'data0': Path(data0_path).resolve()}
    with ExitStack() as stack:
        handles, identities = {}, {}
        for label, path in paths.items():
            handle = stack.enter_context(path.open('rb'))
            digest = hashlib.file_digest(handle, 'sha256').hexdigest()
            if digest != PINNED_SHA256[label]:
                raise ValueError(f'unknown original {label} SHA-256; use explicit backup original when staged')
            handle.seek(0)
            handles[label] = handle
            identities[label] = {'path': str(path), 'sha256': digest}
        archives = {label: Archive(stack.enter_context(zipfile.ZipFile(handle)), label) for label, handle in handles.items()}
        families = inspect_profiles(archives['code'], archives['data0'])
        return {
            'schema_version': 1, 'mode': 'offline_authored_inventory', 'archives': identities,
            'limitations': [
                'Exact selected profiles only; missing, malformed, ambiguous and unsupported definitions are unknown.',
                'SeqTrack parameters are authored literals, not measured animation duration or a native playback command.',
                'State times are authored script values; runtime quality/mode multipliers and callbacks are not evaluated.',
                'Equipment animation roots are explicit profiles; mesh payloads and animation bytes are never read/exported.',
                'Research accounting is static; no manual admission, port, selective index transaction, native write or physical validation.',
            ],
            'sources': sorted((s for a in archives.values() for s in a.sources.values()), key=lambda s: (s['archive'], s['member'])),
            'families': families,
        }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code-pak', type=Path, required=True, help='explicit original code.pak or backup')
    parser.add_argument('--data0-pak', type=Path, required=True, help='explicit original Data0.pak or backup')
    parser.add_argument('--output', type=Path, required=True, help='new local JSON outside both archive parent trees')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if any(output.is_relative_to(path.resolve().parent) for path in (args.code_pak, args.data0_pak)):
            raise ValueError('output must be outside both game archive parent trees')
        if output.suffix.lower() != '.json':
            raise ValueError('output must be a local .json file')
        if output.exists():
            raise ValueError('output already exists; choose a new local path')
        report = build_inventory(args.code_pak, args.data0_pak)
        # No implicit directories and no replacing existing evidence/staging files.
        with output.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2, ensure_ascii=True, allow_nan=False) + '\n')
        print(f'Wrote offline authored inventory: {output}')
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print(f'inventory rejected: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
