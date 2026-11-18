"""Correlate inventoried native CoJ HDR allocation and initialization receipts."""

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path

from analyze_coj_manual_reload import verify_inventory
from analyze_coj_temporal_source import (
    SCOPE, analyze as analyze_temporal, integer, reject_constant, unique_object,
)


ALLOCATION = 'native_hdr_allocation: '
MAX_ALLOCATION_ROWS = 2048
UINT32_MAX = 2**32 - 1
FP16 = 113  # D3DFMT_A16B16G16R16F
CALLERS = {'producer': 0x210ECB, 'texture': 0x23DEF9, 'fill': 0x23E96B, 'overflow': 0}
DESCRIPTORS = {
    'producer': ('width', 'height', 'format', 'samples', 'quality', 'lockable', 'sharedArgument'),
    'texture': ('width', 'height', 'format', 'usage', 'pool', 'levels', 'sharedArgument'),
    'fill': ('fullRect', 'color'),
    'overflow': (),
}
BOOLEAN_FIELDS = {'lockable', 'sharedArgument', 'fullRect'}
DEVICE_ID = ('device', 'generation', 'continuity')
ALLOCATION_ID = (*DEVICE_ID, 'thread')
SCOPE_FIELDS = ('device', 'continuity', 'thread', 'sourceTexture', 'sourceSurface', 'hookEpoch')
STEREO_ID = (*ALLOCATION_ID, 'hookEpoch', 'sourceTexture', 'sourceSurface', 'sourceLastCopySource')


def identity(event, fields):
    return tuple(event.get(key) for key in fields)


def succeeded(result):
    return result < 2**31  # HRESULT uses the high bit for failure.


def allocation_event(line):
    if not line.startswith(ALLOCATION):
        raise ValueError('Malformed HDR allocation prefix')
    if len(line) > 4096:
        raise ValueError('Oversized HDR allocation receipt')
    event = json.loads(line[len(ALLOCATION):], object_pairs_hook=unique_object,
                       parse_constant=reject_constant)
    if not isinstance(event, dict) or type(event.get('kind')) is not str or event['kind'] not in CALLERS:
        raise ValueError('Invalid HDR allocation kind')
    kind = event['kind']
    if event.keys() != {*ALLOCATION_ID, 'kind', 'callerRva', 'object', 'result', *DESCRIPTORS[kind]}:
        raise ValueError('Invalid HDR allocation fields')
    integer(event, 'device')
    generation = integer(event, 'generation', 0)
    integer(event, 'continuity', generation)
    integer(event, 'thread', 1, UINT32_MAX)
    integer(event, 'callerRva', CALLERS[kind], CALLERS[kind])
    obj = integer(event, 'object', 0)
    result = integer(event, 'result', 0, UINT32_MAX)
    for key in DESCRIPTORS[kind]:
        if key in BOOLEAN_FIELDS:
            if type(event.get(key)) is not bool:
                raise ValueError(f'Invalid {key}')
        else:
            integer(event, key, 0, UINT32_MAX)
    if kind == 'overflow' and (obj or result):
        raise ValueError('Invalid HDR allocation overflow marker')
    if kind in ('producer', 'texture') and bool(obj) != succeeded(result):
        raise ValueError('HDR create object contradicts HRESULT')
    if kind == 'fill' and succeeded(result) and not obj:
        raise ValueError('Successful HDR fill without a target surface')
    return event


def scope_report(line, witness):
    scope = witness['event']
    for key in SCOPE_FIELDS:
        if key in scope:
            low = scope['generation'] if key == 'continuity' else 0
            integer(scope, key, low, UINT32_MAX if key == 'thread' else 2**64 - 1)
    producer = witness['sourceCopyProducerObservation']
    draws = witness['sourceCopyDrawPairObservation']
    terminal_copy = dict(witness['sourceCopyObservation'], sourceExtent=list(identity(
        scope, ('sourceLastCopySourceWidth', 'sourceLastCopySourceHeight'))))
    return dict(line=line, scope=scope['scope'], frame=scope['frame'], eye=scope['eye'], owner=scope['owner'],
                complete=scope['complete'], attemptedConsumers=scope['attemptedConsumers'],
                ownershipFieldsAvailable=all(k in scope for k in SCOPE_FIELDS),
                identity=dict(zip(STEREO_ID, identity(scope, STEREO_ID))),
                terminalCopy=terminal_copy, terminalWrites=witness['sourceWriteObservation'],
                producerWrites={key: producer[key] for key in (
                    'available', 'complete', 'drawCalls', 'alphaDrawCalls', 'rgbOnlyDrawCalls', 'alphaBlendDrawCalls',
                    'clearCalls', 'firstColorClear', 'incomingStretchRect', 'directDeviceWrites',
                    'secondaryTargetBinding',
                )} | {'drawPairs': {key: value for key, value in draws.items() if key != 'samples'}})


def known_descriptor(event, width, height):
    if identity(event, ('width', 'height', 'format')) != (width, height, FP16) or event['sharedArgument']:
        return False
    if event['kind'] == 'producer':
        return not event['lockable'] and event['quality'] == 0 and 0 <= event['samples'] <= 16
    return identity(event, ('usage', 'pool', 'levels')) == (1, 0, 1)  # RENDERTARGET, DEFAULT, level zero only.


def match_consumer(pair, scopes, rows, queues, first_uses):
    if not (pair['consumerWitness'] and pair['scopeComplete'] and pair['extentMatched']):
        return None
    samples = [rows[line] for line in pair['sampleLines']]
    for sample, scope in zip(samples, scopes):
        event, copy = sample['event'], sample['sourceCopyObservation']
        terminal = scope['terminalCopy']
        if (event['scope'] != scope['scope'] or event['device'] != scope['identity']['device'] or
                event['source'] != scope['identity']['sourceTexture'] or
                not sample['postCopyStableObservation'] or not copy['sourceExtentVerified'] or
                not terminal['boundaryCurrent'] or terminal['source'] != copy['source'] or
                terminal['copyWriteCall'] != copy['copyWriteCall'] or
                terminal['sourceExtent'] != [event['width'], event['height']]):
            return None
    receipts = [queues.get(row['line']) for row in samples]
    if not all(receipts) or any(receipt >= row['line'] for receipt, row in zip(receipts, samples)):
        return None
    if any(not first_use <= receipt < scope['line']
           for first_use, receipt, scope in zip(first_uses, receipts, scopes)):
        return None
    return dict(consumer=pair['consumer'], pose=pair['pose'], sampleLines=pair['sampleLines'],
                queueLines=receipts, extent=[samples[0]['event'][key] for key in ('width', 'height')])


def match_allocations(scopes, consumers, first_use, receipts, all_scopes):
    left, right = scopes
    source = left['identity']
    through = right['line']
    device_id = identity(source, DEVICE_ID)
    allocation_id = identity(source, ALLOCATION_ID)
    producer, texture, surface = identity(source, ('sourceLastCopySource', 'sourceTexture', 'sourceSurface'))
    if len({producer, texture, surface}) != 3:
        return None, 'aliased_source_identities'
    # A logged surface/texture association has no lifetime token. A conflicting
    # association or another create at a retained address makes reuse ambiguous.
    earlier_epochs = []
    for scope in all_scopes:
        other = scope['identity']
        if scope['line'] > through or identity(other, DEVICE_ID) != device_id:
            continue
        ot, os = identity(other, ('sourceTexture', 'sourceSurface'))
        if ((ot == texture and os and os != surface) or (os == surface and ot and ot != texture) or
                producer in (ot, os) or ot == surface or os == texture):
            return None, 'ambiguous_surface_texture_identity'
        if other['hookEpoch'] is not None and other['hookEpoch'] != source['hookEpoch']:
            earlier_epochs.append(scope['line'])
    candidates = defaultdict(list)
    for receipt in receipts:
        event = receipt['event']
        if receipt['line'] <= through and identity(event, DEVICE_ID) == device_id:
            candidates[event['object']].append(receipt)
    creates = {ptr: [r for r in candidates[ptr] if r['event']['kind'] in ('producer', 'texture')]
               for ptr in (producer, texture, surface)}
    if creates[surface] or len(creates[producer]) > 1 or len(creates[texture]) > 1:
        return None, 'ambiguous_allocation_address'
    matched = {}
    for kind, ptr in (('producer', producer), ('texture', texture)):
        if not creates[ptr]:
            return None, f'{kind}_allocation_unavailable'
        receipt, = creates[ptr]
        event = receipt['event']
        if receipt['line'] >= first_use or event['kind'] != kind or identity(event, ALLOCATION_ID) != allocation_id:
            return None, f'{kind}_allocation_identity_or_order_mismatch'
        if not all(known_descriptor(event, *pair['extent']) for pair in consumers):
            return None, f'{kind}_descriptor_mismatch'
        matched[kind] = receipt

    if any(line > min(r['line'] for r in matched.values()) for line in earlier_epochs):
        return None, 'observed_hook_epoch_change_since_allocation'

    texture_line = matched['texture']['line']
    # A fill preceding this texture's creation can only refer to an earlier
    # surface at this address. Keep ambiguity checks for all subsequent fills.
    fills = [r for r in candidates[surface] if r['event']['kind'] == 'fill' and r['line'] > texture_line]
    fill = dict(status='unknown', reason='fill_receipt_unavailable')
    # The live hook never queries a destination after a failed native call.
    # A failed, unidentified fill in this window makes a success ambiguous.
    unidentified = [r for r in candidates[0] if r['event']['kind'] == 'fill' and
                    texture_line < r['line'] < first_use and identity(r['event'], ALLOCATION_ID) == allocation_id]
    if unidentified:
        fill.update(reason='unidentified_fill_target', lines=[r['line'] for r in unidentified])
    elif len(fills) > 1:
        fill['reason'] = 'ambiguous_fill_receipts'
    elif fills:
        receipt, = fills
        event = receipt['event']
        intervening_texture = any(
            texture_line < other['line'] < receipt['line'] and other['event']['kind'] == 'texture'
            for ptr, events in candidates.items() if ptr for other in events
        )
        if intervening_texture:
            fill.update(reason='intervening_texture_creation', line=receipt['line'])
        elif (texture_line < receipt['line'] < first_use and
                identity(event, ALLOCATION_ID) == allocation_id and event['fullRect'] and event['color'] == 0):
            fill = dict(status='successful' if succeeded(event['result']) else 'failed',
                        line=receipt['line'], result=event['result'])
        else:
            fill.update(reason='fill_identity_order_or_arguments_mismatch', line=receipt['line'])
    return dict(**matched, fillBeforeFirstConsumer=fill), None


def match_stereo(eyes, consumer_pairs, rows, queues, first_receipts, receipts, scopes):
    if [s['eye'] for s in eyes] != [0, 1] or not all(s['complete'] for s in eyes):
        return None, 'incomparable_eye_scopes'
    left, right = eyes
    if not all(s['ownershipFieldsAvailable'] for s in eyes):
        return None, 'scope_ownership_fields_unavailable'
    ident = left['identity']
    if ident != right['identity'] or not all(ident[k] for k in STEREO_ID if k != 'hookEpoch'):
        return None, 'stereo_ownership_identity_mismatch'

    # A later stable sample cannot establish when earlier use began. The
    # first consumer's synchronous receipt bounds both creation and fill.
    first_uses = []
    for scope in eyes:
        first = first_receipts.get((scope['frame'], scope['eye']))
        if not first or not scope['attemptedConsumers']:
            return None, 'first_consumer_receipt_unavailable'
        for line in first['sampleLines']:
            event = rows[line]['event']
            if (event['scope'] != scope['scope'] or event['device'] != ident['device'] or
                    event['source'] != ident['sourceTexture'] or first['line'] >= line):
                return None, 'first_consumer_receipt_identity_or_order_mismatch'
        first_uses.append(first['line'])
    if not first_uses[0] < left['line'] < first_uses[1] < right['line']:
        return None, 'incomparable_consumer_order'

    consumers = [consumer for pair in consumer_pairs
                 if (consumer := match_consumer(pair, eyes, rows, queues, first_uses)) is not None]
    if not consumers:
        return None, 'stable_stereo_consumer_receipts_unavailable'
    matched, reason = match_allocations(eyes, consumers, first_uses[0], receipts, scopes)
    if matched is None:
        return None, reason
    return dict(frame=left['frame'], owner=left['owner'], identity=ident,
                scopeLines=[left['line'], right['line']], firstConsumerQueueLines=first_uses,
                consumerPairs=consumers, **matched), None


def analyze(lines, run_id, build_id):
    receipts, scope_lines, errors = [], [], []
    overflow_lines = []

    def observe_lines():
        running = False
        for number, line in enumerate(lines, 1):
            if line.startswith('run_start:'):
                running = True
            elif line.startswith('run_end:'):
                running = False
            elif line.startswith(SCOPE):
                scope_lines.append(number)
            elif line.startswith(ALLOCATION.rstrip()):
                try:
                    if not running:
                        raise ValueError('HDR allocation outside run interval')
                    event = allocation_event(line)
                    if event['kind'] == 'overflow':
                        if overflow_lines:
                            raise ValueError('Duplicate HDR allocation overflow marker')
                        overflow_lines.append(number)
                    elif len(receipts) >= MAX_ALLOCATION_ROWS:
                        raise ValueError('HDR allocation receipt budget exceeded')
                    else:
                        receipts.append(dict(line=number, event=event))
                except (ValueError, OverflowError, RecursionError) as error:
                    errors.append(f'Line {number}: {error}')
            yield line

    temporal = analyze_temporal(observe_lines(), run_id, build_id)
    errors.extend(temporal['errors'])
    scopes, pairs, unresolved = [], [], []
    if not temporal['errors']:
        for line, witness in zip(scope_lines, temporal['scopes'], strict=True):
            try:
                scopes.append(scope_report(line, witness))
            except ValueError as error:
                errors.append(f'Line {line}: {error}')
    if not errors and not overflow_lines:
        by_frame = defaultdict(list)
        for scope in scopes:
            by_frame[scope['frame'], scope['owner']].append(scope)
        rows = {row['line']: row for row in temporal['rows']}
        eye_scope_counts = Counter((s['frame'], s['eye']) for s in scopes)
        first_receipts = {}
        for queue in temporal['queues']:
            event = queue['event']
            key = event['frame'], event['eye']
            if event.get('consumer') == 1 and eye_scope_counts[key] == 1:
                first_receipts[key] = queue
        queues = {line: queue['line'] for queue in temporal['queues'] if queue['event']['status'] == 'queued'
                  for line in queue['sampleLines']}
        consumer_pairs = defaultdict(list)
        for pair in temporal['pairs']:
            consumer_pairs[pair['frame'], pair['owner']].append(pair)
        for (frame, owner), eyes in by_frame.items():
            eyes.sort(key=lambda scope: scope['eye'])
            matched, reason = match_stereo(eyes, consumer_pairs[frame, owner], rows, queues,
                                           first_receipts, receipts, scopes)
            if matched is None:
                unresolved.append(dict(frame=frame, owner=owner, reason=reason))
            else:
                pairs.append(matched)

    available_scopes = sum(scope['ownershipFieldsAvailable'] for scope in scopes)
    counts = Counter(r['event']['kind'] for r in receipts)
    status = 'uncorrelated'
    if errors:
        status = 'rejected'
    elif overflow_lines:
        status = 'overflow'
    elif pairs:
        status = 'correlated'
    elif not available_scopes or not receipts:
        status = 'unavailable'
    return dict(schema=1, status=status, runId=run_id, buildManifestId=build_id,
                coverage='bounded_native_hdr_address_and_call_receipts',
                fillTiming='known_caller_after_texture_create_before_first_consumer_receipt',
                allocationHookContinuity='unavailable_before_scope', resourceLifetimeProven=False,
                sourceQualified=False, temporalAdmitted=False, headsetValidated=False,
                summary=dict(allocationRows=len(receipts), producerRows=counts['producer'], textureRows=counts['texture'],
                             fillRows=counts['fill'], overflowRows=len(overflow_lines), scopeRows=len(scope_lines),
                             scopeOwnershipFieldsAvailable=available_scopes, correlatedStereoPairs=len(pairs)),
                overflowLines=overflow_lines, allocations=receipts, scopes=scopes, pairs=pairs,
                unresolved=unresolved, errors=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence_directory', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root, output = args.evidence_directory.resolve(), args.output.resolve()
    if output.is_relative_to(root):
        parser.error('Output must be outside evidence')
    evidence, run, paths = verify_inventory(root)
    for key, expected in (('runtimeStarted', True), ('runtimeEnded', True), ('incomplete', False)):
        if key in evidence and evidence[key] is not expected:
            parser.error(f'Evidence reports an unfinished run ({key})')
    with paths['runtime/cojvr.log'].open(encoding='utf-8-sig') as stream:
        report = analyze(stream, run['runId'], run['buildManifestId'])
    with output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(f"{report['status']}: {len(report['pairs'])} correlated stereo pairs")
    return 2 if report['errors'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
