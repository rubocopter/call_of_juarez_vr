import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_coj_hdr_ownership import analyze


RUN = 'fixture'
BUILD = 'a' * 64
IDENTITY = dict(device=4, generation=2, continuity=3, thread=12)
WIDTH, HEIGHT = 2804, 2860
PRODUCER, TEXTURE, SURFACE = 11, 5, 15


def allocation(kind, **changes):
    descriptor = {
        'producer': dict(callerRva=0x210ECB, object=PRODUCER, width=WIDTH, height=HEIGHT,
                         format=113, samples=0, quality=0, lockable=False, sharedArgument=False),
        'texture': dict(callerRva=0x23DEF9, object=TEXTURE, width=WIDTH, height=HEIGHT,
                        format=113, usage=1, pool=0, levels=1, sharedArgument=False),
        'fill': dict(callerRva=0x23E96B, object=SURFACE, fullRect=True, color=0),
        'overflow': dict(callerRva=0, object=0),
    }[kind]
    return dict(dict(IDENTITY, kind=kind, result=0, **descriptor), **changes)


def copy_witness():
    rect = [0, 0, WIDTH, HEIGHT]
    return dict(sourceWriteCalls=2, sourceAlphaWriteCalls=2, sourceLastWriterShader=0,
                sourceWriteObservationComplete=True, sourceCopyCalls=1,
                sourceLastCopySource=PRODUCER, sourceLastCopySourceFormat=113,
                sourceLastCopySourceWidth=WIDTH, sourceLastCopySourceHeight=HEIGHT,
                sourceCopyRectanglesObserved=True, sourceLastCopySourceRect=rect,
                sourceLastCopyDestinationRect=rect, sourceCopyWriteCall=2,
                sourceCopyBoundaryCurrent=True)


def scope(eye, **changes):
    return dict(dict(IDENTITY, scope=eye + 1, frame=8, eye=eye, owner=3, complete=True,
                     attemptedConsumers=1, sourceTexture=TEXTURE, sourceSurface=SURFACE,
                     hookEpoch=7, temporalAdmitted=False, **copy_witness()), **changes)


def sample(eye, **changes):
    return dict(dict(frame=8, generation=2, scope=eye + 1, owner=3, device=4, source=TEXTURE,
                     eye=eye, width=WIDTH, height=HEIGHT, sampling='16x16_point',
                     temporalAdmitted=False, scopeKind='hdr_source', pose=9, consumer=1,
                     sampler=0, target=6, shader=7, alpha=[1] * 256, rgb=[0] * 768,
                     **copy_witness()), **changes)


def queue(eye, consumer=1):
    return ('native_hdr_alpha_queue: status=queued;phase=matching_native_sampler_draw;'
            f'sampler=0;consumer={consumer};eye={eye};frame=8;hr=0;temporal_admitted=false')


def report(events):
    lines = [f'run_start: run_id={RUN} build_manifest_id={BUILD}']
    for event in events:
        if isinstance(event, str):
            lines.append(event)
            continue
        prefix = ('native_hdr_allocation' if 'kind' in event else
                  'native_hdr_source_scope' if 'complete' in event else 'native_hdr_alpha_probe')
        lines.append(f'{prefix}: {json.dumps(event)}')
    return analyze([*lines, f'run_end: run_id={RUN}'], RUN, BUILD)


def stereo(left=None, right=None):
    # GPU readbacks arrive after both synchronous queue and scope receipts.
    return [queue(0), left or scope(0), queue(1), right or scope(1), sample(0), sample(1)]


class HdrOwnershipAnalysis(unittest.TestCase):
    def setUp(self):
        self.creates = [allocation('producer'), allocation('texture')]

    def test_correlates_prior_creates_and_full_black_fill_with_both_eyes(self):
        result = report([*self.creates, allocation('fill'), *stereo()])
        self.assertEqual(result['status'], 'correlated', result)
        pair, = result['pairs']
        self.assertEqual(pair['producer']['line'], 2)
        self.assertEqual(pair['texture']['line'], 3)
        self.assertEqual(pair['fillBeforeFirstConsumer'], dict(status='successful', line=4, result=0))
        self.assertEqual(pair['consumerPairs'][0]['sampleLines'], [9, 10])
        self.assertEqual(pair['consumerPairs'][0]['queueLines'], [5, 7])
        self.assertFalse(result['sourceQualified'])
        self.assertFalse(result['temporalAdmitted'])
        self.assertFalse(result['headsetValidated'])
        self.assertFalse(result['resourceLifetimeProven'])

    def test_unknown_and_failed_initialization_are_distinct(self):
        failure = 0x8876086C
        for fills, expected in [([], 'unknown'), ([allocation('fill', result=failure)], 'failed'),
                                ([allocation('fill', fullRect=False)], 'unknown'),
                                ([allocation('fill', color=0xFF000000)], 'unknown'),
                                ([allocation('fill', thread=13)], 'unknown'),
                                ([allocation('fill'), allocation('fill')], 'unknown')]:
            with self.subTest(fills=fills):
                result = report([*self.creates, *fills, *stereo()])
                fill = result['pairs'][0]['fillBeforeFirstConsumer']
                self.assertEqual(fill['status'], expected)
                if expected == 'failed':
                    self.assertEqual(fill['result'], failure)

    def test_unidentified_failed_fill_withholds_success(self):
        failed = allocation('fill', object=0, result=0x8876086C)
        result = report([*self.creates, allocation('fill'), failed, *stereo()])
        self.assertEqual(result['pairs'][0]['fillBeforeFirstConsumer']['reason'],
                         'unidentified_fill_target')

    def test_incomplete_scope_still_records_a_hook_epoch_change(self):
        earlier = scope(0, scope=3, frame=7, hookEpoch=6, complete=False, attemptedConsumers=0)
        self.assertFalse(report([*self.creates, earlier, *stereo()])['pairs'])

    def test_crossed_identities_and_creation_after_use_do_not_correlate(self):
        cases = [([allocation('producer', **{key: value}), self.creates[1]], stereo())
                 for key, value in dict(device=9, generation=1, continuity=2, thread=13).items()]
        cases += [(self.creates, stereo(right=scope(1, hookEpoch=8))),
                  (self.creates, stereo(right=scope(1, sourceSurface=17))),
                  (self.creates, stereo(right=scope(1, complete=False))),
                  ([], [*stereo()[:1], *self.creates, *stereo()[1:]]),
                  (self.creates, [scope(0), scope(1), sample(0), sample(1)])]
        for creates, events in cases:
            with self.subTest(creates=creates, events=len(events)):
                self.assertFalse(report([*creates, *events])['pairs'])

    def test_reuse_and_overflow_withdraw_the_join(self):
        for conflict in (allocation('producer'), allocation('producer', thread=13),
                         allocation('texture', object=SURFACE), allocation('overflow')):
            with self.subTest(conflict=conflict):
                events = stereo()
                result = report([*self.creates, *events[:2], conflict, *events[2:]])
                self.assertFalse(result['pairs'])
        result = report([*self.creates, *stereo(), allocation('overflow')])
        self.assertEqual(result['status'], 'overflow')
        earlier_scope = scope(0, scope=3, frame=7, hookEpoch=6, attemptedConsumers=0)
        result = report([*self.creates, earlier_scope, *stereo()])
        self.assertFalse(result['pairs'])
        self.assertIn('observed_hook_epoch_change_since_allocation',
                      [frame['reason'] for frame in result['unresolved']])

    def test_known_descriptors_and_hresults_are_required(self):
        for producer in (allocation('producer', lockable=True), allocation('producer', quality=1),
                         allocation('producer', width=WIDTH + 1),
                         allocation('producer', result=0x80004005, object=0)):
            self.assertFalse(report([producer, self.creates[1], *stereo()])['pairs'])
        for changes in (dict(pool=1), dict(usage=513), dict(levels=0), dict(sharedArgument=True)):
            self.assertFalse(report([self.creates[0], allocation('texture', **changes), *stereo()])['pairs'])
        self.assertEqual(report([allocation('producer', result=1, samples=4),
                                 self.creates[1], *stereo()])['status'], 'correlated')

    def test_fill_must_follow_texture_creation_and_precede_the_first_receipt(self):
        events = stereo()
        for rows in ([allocation('fill'), *self.creates, *events],
                     [*self.creates, *events[:1], allocation('fill'), *events[1:]],
                     [*self.creates, allocation('texture', object=32), allocation('fill'), *events],
                     [*self.creates, allocation('fill'), *events[:2], allocation('fill'), *events[2:]]):
            self.assertEqual(report(rows)['pairs'][0]['fillBeforeFirstConsumer']['status'], 'unknown')

    def test_fill_at_reused_address_before_texture_creation_does_not_hide_its_initial_fill(self):
        result = report([allocation('fill'), *self.creates, allocation('fill'), *stereo()])
        self.assertEqual(result['pairs'][0]['fillBeforeFirstConsumer'],
                         dict(status='successful', line=5, result=0))

    def test_terminal_writes_withdraw_stability_and_draw_budget_is_validated(self):
        late_write = scope(1, sourceWriteCalls=3, sourceCopyBoundaryCurrent=False)
        self.assertFalse(report([*self.creates, *stereo(right=late_write)])['pairs'])
        uncertain_terminal = scope(1, sourceCopyBoundaryCurrent=False)
        self.assertFalse(report([*self.creates, *stereo(right=uncertain_terminal)])['pairs'])
        different_rect = [0, 0, WIDTH + 1, HEIGHT]
        different_extent = scope(1, sourceLastCopySourceWidth=WIDTH + 1,
                                 sourceLastCopySourceRect=different_rect,
                                 sourceLastCopyDestinationRect=different_rect)
        self.assertFalse(report([*self.creates, *stereo(right=different_extent)])['pairs'])
        producer = dict(surface=PRODUCER, drawCalls=1025, alphaDrawCalls=0, alphaBlendDrawCalls=0,
                        clearCalls=1, firstAlphaShader=0, lastAlphaShader=0, observationComplete=True,
                        coverage='same_extent_fp16_rt0_draw_clear_only',
                        firstColorClear=dict(rectangles=1, drawsBefore=0, stateComplete=True,
                                             fullViewport=True, scissorEnabled=False, leadingFullViewport=True))
        draws = dict(schema=1, coverage='first_8_then_every_16_through_1024',
                     observed=0, readFailures=72, samples=[])
        result = report([scope(0, sourceCopyProducer=producer, sourceCopyDrawPairs=draws)])
        witness = result['scopes'][0]['producerWrites']
        self.assertTrue(witness['firstColorClear']['leadingFullViewport'])
        self.assertEqual(witness['drawPairs']['drawsNotSampled'], 953)
        draws['readFailures'] = 73
        result = report([scope(0, sourceCopyProducer=producer, sourceCopyDrawPairs=draws)])
        self.assertEqual(result['status'], 'rejected')

    def test_first_consumer_receipt_bounds_creation_even_when_only_second_is_stable(self):
        # The first consumer has another write before scope end. Both eyes only
        # become stable at their second consumer.
        first = dict(sourceWriteCalls=1, sourceAlphaWriteCalls=1, sourceCopyWriteCall=1)
        events = [queue(0), queue(0, 2), scope(0, attemptedConsumers=2),
                  queue(1), queue(1, 2), scope(1, attemptedConsumers=2),
                  sample(0, **first), sample(0, consumer=2),
                  sample(1, **first), sample(1, consumer=2)]
        self.assertEqual(report([*self.creates, *events])['status'], 'correlated')
        self.assertFalse(report([events[0], *self.creates, *events[1:]])['pairs'])
        for receipt in (queue(0), queue(0).replace('queued', 'skipped').replace('hr=0', 'hr=1')):
            without_first_grid = [receipt, *events[1:6], *events[7:]]
            result = report([*self.creates, *without_first_grid])
            self.assertEqual(result['status'], 'correlated', result)
            self.assertFalse(report([receipt, *self.creates, *without_first_grid[1:]])['pairs'])

    def test_legacy_scope_fields_are_unavailable(self):
        legacy = scope(0)
        for key in ('device', 'continuity', 'thread', 'sourceTexture', 'sourceSurface', 'hookEpoch'):
            del legacy[key]
        result = report([queue(0), legacy, sample(0)])
        self.assertEqual(result['status'], 'unavailable')
        self.assertFalse(result['scopes'][0]['ownershipFieldsAvailable'])
        self.assertEqual(result['summary']['allocationRows'], 0)
        self.assertEqual(result['errors'], [])

    def test_malformed_rows_fail_closed(self):
        for event in (allocation('fill', callerRva=0x23E965), allocation('fill', fullRect=1),
                      allocation('fill', result=-1), allocation('producer', object=0),
                      allocation('overflow', result=1), allocation('fill', liveQualified=True),
                      scope(0, thread=True),
                      'native_hdr_allocation:{}',
                      'native_hdr_allocation: {"kind":"fill","kind":"producer"}'):
            with self.subTest(event=event):
                result = report([*self.creates, *stereo(), event])
                self.assertEqual(result['status'], 'rejected')
                self.assertFalse(result['pairs'])


if __name__ == '__main__':
    unittest.main()
