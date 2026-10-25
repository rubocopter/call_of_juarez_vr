import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_coj_temporal_source import analyze
RUN='fixture';BUILD='a'*64
def sample(eye=0):return dict(frame=8,generation=2,scope=eye+1,owner=3,device=4,source=5,
    eye=eye,width=2804,height=2860,sampling='16x16_point',temporalAdmitted=False,
    scopeKind='hdr_source',pose=9,consumer=1,sampler=0,target=6,shader=7,
    alpha=[2.5]*128+[1]*128,rgb=[.25,.5,1]*256)
def scope(eye):return dict(scope=eye+1,frame=8,generation=2,owner=3,eye=eye,complete=True,attemptedConsumers=1,temporalAdmitted=False)

def writes(event,calls,alpha,shader=7,complete=True):
    return dict(event,sourceWriteCalls=calls,sourceAlphaWriteCalls=alpha,
                sourceLastWriterShader=shader,sourceWriteObservationComplete=complete)

def copies(event,count=1):
    return dict(event,sourceCopyCalls=count,sourceLastCopySource=11 if count else 0,
                sourceLastCopySourceFormat=113 if count else 0,sourceCopyRectanglesObserved=bool(count),
                sourceLastCopySourceRect=[0,0,2804,2860] if count else [0,0,0,0],
                sourceLastCopyDestinationRect=[0,0,2804,2860] if count else [0,0,0,0])
def report(events,extra=()):return analyze([f'run_start: run_id={RUN} build_manifest_id={BUILD}',
    *[e if isinstance(e,str) else ('native_hdr_source_scope: ' if 'complete' in e else 'native_hdr_alpha_probe: ')+json.dumps(e) for e in events],
    f'run_end: run_id={RUN}',*extra],RUN,BUILD)
class Analysis(unittest.TestCase):
    def blend_producer(self):
        s=self.producer(copies(writes(sample(),2,2,shader=0)))
        s['sourceCopyProducer']['firstAlphaBlend']=dict(observed=True,shader=31,
            stateComplete=True,separateAlpha=True,sourceBlend=1,destinationBlend=2,
            blendOperation=1,blendFactor=0xffffffff,shaderComplete=True,
            shaderBytes=20,bytecode=[0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff])
        return s
    def test_effective_alpha_operation_and_program_are_diagnostic_not_depth(self):
        s=self.blend_producer()
        r=report([s,scope(0)])
        b=r['rows'][0]['sourceCopyProducerObservation'].get('firstAlphaBlend',{})
        self.assertEqual(b.get('alphaOperation'),'destination_preserved')
        self.assertEqual(b.get('additionalBlendDrawCalls'),1)
        self.assertEqual(b.get('shaderSha256'),'38061858be33a1e15f407253976ead7c542d45a143809ad24cb5354290114867')
        self.assertFalse(r['sourceQualified'])
        self.assertFalse(r['depthAssessment']['qualified'])
        for src,dst,op,expected in ((2,1,1,'source_replaced'),(1,2,3,'destination_preserved'),
                                     (2,1,2,'source_replaced'),(2,1,3,'other'),(1,2,4,'other'),
                                     (5,6,1,'other')):
            b=s['sourceCopyProducer']['firstAlphaBlend']
            b.update(sourceBlend=src,destinationBlend=dst,blendOperation=op)
            actual=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']['firstAlphaBlend']
            self.assertEqual(actual['alphaOperation'],expected)
    def test_missing_or_failed_alpha_blend_program_stays_unknown(self):
        for s in (sample(),self.producer(copies(writes(sample(),2,2,shader=0)))):
            p=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']
            self.assertEqual(p.get('firstAlphaBlend',{}).get('alphaOperation'),'unknown')
        s=self.blend_producer();b=s['sourceCopyProducer']['firstAlphaBlend']
        b.update(stateComplete=False,shaderComplete=False,shaderBytes=8192,bytecode=[])
        actual=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']['firstAlphaBlend']
        self.assertEqual(actual['alphaOperation'],'unknown')
        self.assertIsNone(actual['shaderSha256'])
    def test_invalid_blend_program_or_fabricated_witness_rejects(self):
        for field,value in (('observed',1),('shader',-1),('stateComplete',1),('sourceBlend',0),
                            ('blendOperation',6),('separateAlpha',1),('shaderComplete',1),
                            ('shaderBytes',16),('bytecode',[True]),('bytecode',[2**32]),
                            ('bytecode',[0]*1025)):
            with self.subTest(field=field):
                s=self.blend_producer();s['sourceCopyProducer']['firstAlphaBlend'][field]=value
                self.assertEqual(report([s])['status'],'rejected')
        s=self.blend_producer();s['sourceCopyProducer']['alphaBlendDrawCalls']=0
        self.assertEqual(report([s])['status'],'rejected')
    def test_blend_program_change_without_copy_is_rejected(self):
        s=self.blend_producer();later=copy.deepcopy(s);later.update(consumer=2,target=16)
        later['sourceCopyProducer']['firstAlphaBlend']['destinationBlend']=1
        end=copies(writes(scope(0),2,2,shader=0));end['attemptedConsumers']=2
        self.assertEqual(report([s,later,end])['status'],'rejected')
    def test_fixed_function_and_unobserved_blend_are_distinct(self):
        s=self.blend_producer();b=s['sourceCopyProducer']['firstAlphaBlend']
        b.update(shader=0,shaderBytes=0,bytecode=[])
        actual=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']['firstAlphaBlend']
        self.assertTrue(actual['observed'])
        self.assertTrue(actual['shaderComplete'])
        self.assertIsNone(actual['shaderSha256'])
        s['sourceCopyProducer']['alphaBlendDrawCalls']=0
        for key in b:b[key]=[] if key=='bytecode' else False if type(b[key]) is bool else 0
        actual=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']['firstAlphaBlend']
        self.assertFalse(actual['observed'])
        self.assertEqual(actual['alphaOperation'],'unknown')
    def producer(self,event,complete=True):
        return dict(event,sourceCopyProducer=dict(surface=11,drawCalls=9,alphaDrawCalls=6,
            alphaBlendDrawCalls=2,clearCalls=1,firstAlphaShader=21,lastAlphaShader=22,
            observationComplete=complete,coverage='same_extent_fp16_rt0_draw_clear_only'))
    def test_copy_producer_counts_remain_call_intents_without_source_qualification(self):
        s=self.producer(copies(writes(sample(),2,2,shader=0)))
        r=report([s,copies(writes(scope(0),2,2,shader=0))])
        p=r['rows'][0].get('sourceCopyProducerObservation',{})
        self.assertEqual(p.get('rgbOnlyDrawCalls'),3)
        self.assertEqual(p.get('alphaBlendDrawCalls'),2)
        self.assertTrue(p.get('complete'))
        self.assertFalse(r.get('sourceQualified',True))
        self.assertFalse(r['depthAssessment']['qualified'])
    def test_missing_producer_is_unknown_and_incomplete_producer_does_not_grant_depth(self):
        for s in (sample(),self.producer(copies(writes(sample(),2,2,shader=0)),False)):
            p=report([s,scope(0)])['rows'][0].get('sourceCopyProducerObservation',{})
            self.assertIs(p.get('complete'),False)
        r=report([sample(),scope(0)])
        self.assertIsNone(r['rows'][0]['sourceCopyProducerObservation']['drawCalls'])
    def test_no_matching_producer_surface_means_unknown_writes_not_zero(self):
        s=copies(writes(sample(),2,2,shader=0))
        s['sourceCopyProducer']=dict(surface=0,drawCalls=0,alphaDrawCalls=0,alphaBlendDrawCalls=0,
            clearCalls=0,firstAlphaShader=0,lastAlphaShader=0,observationComplete=False,
            coverage='same_extent_fp16_rt0_draw_clear_only')
        p=report([s,scope(0)])['rows'][0]['sourceCopyProducerObservation']
        self.assertIsNone(p['drawCalls'])
        self.assertFalse(p['complete'])
        self.assertFalse(p['observed'])
    def test_invalid_or_foreign_copy_producer_witness_is_rejected(self):
        for field,value in (('drawCalls',True),('alphaDrawCalls',10),('alphaBlendDrawCalls',7),
                            ('surface',12),('observationComplete',1),('clearCalls',-1),
                            ('coverage','all_materials'),('firstAlphaShader',-1)):
            with self.subTest(field=field):
                s=self.producer(copies(writes(sample(),2,2,shader=0)))
                s['sourceCopyProducer'][field]=value
                self.assertEqual(report([s])['status'],'rejected')
        s=self.producer(copies(writes(sample(),2,2,shader=0)))
        del s['sourceCopyProducer']['drawCalls']
        self.assertEqual(report([s])['status'],'rejected')
    def test_frozen_producer_cannot_change_without_another_native_copy(self):
        s=self.producer(copies(writes(sample(),2,2,shader=0)))
        later=copy.deepcopy(s);later.update(consumer=2,target=16)
        later['sourceCopyProducer']['clearCalls']=2
        end=copies(writes(scope(0),2,2,shader=0));end['attemptedConsumers']=2
        self.assertEqual(report([s,later,end])['status'],'rejected')
        end=self.producer(copies(writes(scope(0),2,2,shader=0)))
        end['sourceCopyProducer']['lastAlphaShader']=99
        self.assertEqual(report([s,end])['status'],'rejected')
        # A new copy can refresh its snapshot, including a different source.
        end=self.producer(copies(writes(scope(0),3,3,shader=0),2))
        end['sourceCopyProducer']['clearCalls']=2
        self.assertEqual(report([s,end])['status'],'observed')
    def test_typed_copy_transition_selects_later_stable_samples_without_depth(self):
        first=copies(writes(sample(),1,1),0)
        later=copies(writes(sample(),2,2,shader=0));later.update(consumer=2,target=16)
        third=copy.deepcopy(later);third.update(consumer=3,target=17)
        end=copies(writes(scope(0),2,2,shader=0));end['attemptedConsumers']=3
        r=report([first,later,third,end])
        self.assertFalse(r['rows'][0]['postCopyStableObservation'])
        self.assertTrue(r['rows'][1]['postCopyStableObservation'])
        self.assertTrue(r['rows'][2]['postCopyStableObservation'])
        self.assertEqual(r['summary']['postCopyStableSamples'],2)
        self.assertEqual(r['copyBoundarySelection']['selectedSampleLines'],[3])
        self.assertEqual(r['copyBoundarySelection']['missingEyes'][0]['missingEyes'],[1])
        self.assertFalse(r['depthAssessment']['qualified'])
    def test_partial_copy_or_later_write_cannot_select_source(self):
        s=copies(writes(sample(),1,1,shader=0));end=copies(writes(scope(0),1,1,shader=0))
        s['sourceLastCopyDestinationRect'][2]=1402
        self.assertFalse(report([s,end])['rows'][0]['postCopyStableObservation'])
        s=copies(writes(sample(),1,1,shader=0));end=copies(writes(scope(0),2,2,shader=0))
        self.assertFalse(report([s,end])['rows'][0]['postCopyStableObservation'])
    def test_copy_counter_and_rectangle_corruption_rejects(self):
        for field,value in (('sourceCopyCalls',True),('sourceCopyCalls',3),
                            ('sourceCopyRectanglesObserved',1),('sourceLastCopySourceFormat',2**32),
                            ('sourceLastCopySourceRect',[0,0,True,3]),
                            ('sourceLastCopyDestinationRect',[0,0,0,3])):
            s=copies(writes(sample(),2,2));s[field]=value
            self.assertEqual(report([s])['status'],'rejected')
        s=copies(writes(sample(),2,2));end=copies(writes(scope(0),2,2),0)
        self.assertEqual(report([s,end])['status'],'rejected')
    def test_native_current_copy_marker_requires_matching_write_ordinal_and_extent(self):
        s=copies(writes(sample(),2,2,shader=0))
        s.update(sourceCopyWriteCall=2,sourceLastCopySourceWidth=2804,
                 sourceLastCopySourceHeight=2860,sourceCopyBoundaryCurrent=True)
        end=copies(writes(scope(0),2,2,shader=0))
        r=report([s,end]);self.assertTrue(r['rows'][0]['sourceCopyObservation']['boundaryCurrent'])
        for field,value in (('sourceCopyWriteCall',1),('sourceLastCopySourceWidth',5608)):
            bad=dict(s);bad[field]=value
            self.assertEqual(report([bad,end])['status'],'rejected')
    def test_copy_metadata_absence_is_unknown_and_cannot_select_historical_samples(self):
        r=report([writes(sample(),1,1),writes(scope(0),1,1)])
        self.assertFalse(r['rows'][0]['sourceCopyObservation']['available'])
        self.assertFalse(r['rows'][0]['postCopyStableObservation'])
    def test_write_intents_report_later_calls_without_depth_admission(self):
        r=report([writes(sample(),2,1),writes(scope(0),5,3,shader=0),
                  writes(sample(1),2,1),writes(scope(1),2,1)])
        self.assertEqual(r['status'],'observed')
        left=r['rows'][0]
        self.assertTrue(left['sourceWriteObservation']['complete'])
        self.assertEqual(left['sourceWritesAfterSample']['writeCalls'],3)
        self.assertEqual(left['sourceWritesAfterSample']['alphaWriteCalls'],2)
        self.assertTrue(r['pairs'][0]['sourceWriteObservationComplete'])
        self.assertEqual(r['summary']['samplesWithLaterSourceWriteCalls'],1)
        self.assertFalse(r['depthAssessment']['qualified'])
        self.assertFalse(r['temporalAdmitted'])
    def test_zero_shader_and_incomplete_write_observation_remain_diagnostic(self):
        r=report([writes(sample(),1,1,shader=0,complete=False),
                  writes(scope(0),2,2,shader=0,complete=False)])
        self.assertEqual(r['status'],'observed')
        self.assertEqual(r['rows'][0]['sourceWriteObservation']['lastWriterShader'],0)
        self.assertFalse(r['rows'][0]['sourceWritesAfterSample']['complete'])
        self.assertEqual(r['rows'][0]['sourceWritesAfterSample']['status'],'incomplete')
        self.assertFalse(r['depthAssessment']['qualified'])
    def test_missing_optional_write_fields_are_unknown_not_zero(self):
        for s in (sample(),dict(sample(),sourceWriteCalls=0)):
            r=report([s,scope(0)])
            self.assertEqual(r['status'],'observed')
            self.assertFalse(r['rows'][0]['sourceWriteObservation']['available'])
            self.assertIsNone(r['rows'][0]['sourceWritesAfterSample']['writeCalls'])
    def test_invalid_write_witness_values_reject(self):
        for field,value in (('sourceWriteCalls',True),('sourceWriteCalls',-1),
                            ('sourceWriteCalls',2**64),('sourceAlphaWriteCalls',3),
                            ('sourceLastWriterShader',-1),('sourceWriteObservationComplete',1)):
            s=writes(sample(),2,1);s[field]=value
            self.assertEqual(report([s])['status'],'rejected')
            end=writes(scope(0),2,1);end[field]=value
            self.assertEqual(report([sample(),end])['status'],'rejected')
    def test_write_counter_regressions_and_inconsistent_deltas_reject(self):
        for start,end in ((writes(sample(),3,2),writes(scope(0),2,2)),
                          (writes(sample(),3,1),writes(scope(0),4,3))):
            self.assertEqual(report([start,end])['status'],'rejected')
    def test_write_deltas_compare_consumers_but_do_not_cross_eye_reset(self):
        left=writes(sample(),1,1)
        later=writes(sample(),2,2);later.update(consumer=2,target=16)
        later['alpha'][0]=4
        end=writes(scope(0),2,2);end['attemptedConsumers']=2
        r=report([left,later,end,writes(sample(1),0,0,shader=0),
                  writes(scope(1),0,0,shader=0)])
        within=next(c for c in r['sampleComparisons'] if c['kind']=='within_eye_consumers')
        self.assertEqual(within['sourceWriteCallDelta'],1)
        self.assertFalse(within['contentChangedWithoutObservedWriteCalls'])
        cross=next(c for c in r['sampleComparisons'] if c['kind']=='left_terminal_to_right_first')
        self.assertIsNone(cross['sourceWriteCallDelta'])
        self.assertTrue(cross['sourceWriteCountersResetPerEye'])
    def test_changed_content_without_observed_writes_flags_a_diagnostic_gap(self):
        left=writes(sample(),1,1)
        later=writes(sample(),1,1);later.update(consumer=2,target=16)
        later['rgb'][0]=.7
        end=writes(scope(0),1,1);end['attemptedConsumers']=2
        r=report([left,later,end])
        self.assertTrue(r['sampleComparisons'][0]['contentChangedWithoutObservedWriteCalls'])
        later['sourceWriteObservationComplete']=False
        r=report([left,later,end])
        self.assertIsNone(r['sampleComparisons'][0]['contentChangedWithoutObservedWriteCalls'])
    def test_consumer_ordinal_cannot_pair_different_bound_resources(self):
        for field in ('target','shader','sampler','width','height'):
            with self.subTest(field=field):
                right=sample(1);right[field]+=1
                r=report([sample(),scope(0),right,scope(1)])
                self.assertFalse(r['pairs'])
                self.assertEqual(len(r['unpairedRows']),2)
    def test_phase_comparison_exposes_partial_cross_eye_source_reuse(self):
        left=sample();left.update(consumer=2,target=16)
        left['alpha']=[3]*256;left['rgb']=[.2,.3,.4]*256
        later=copy.deepcopy(left);later.update(consumer=3,target=17,shader=18)
        right=sample(1);right['alpha']=[4]*128+[3]*128
        right['rgb']=[.5,.6,.7]*128+[.2,.3,.4]*128
        s=scope(0);s['attemptedConsumers']=3
        r=report([left,later,s,right,scope(1)])
        transitions=r['sampleComparisons']
        same=next(c for c in transitions if c['kind']=='within_eye_consumers')
        self.assertEqual(same['identicalRgbaPixels'],256)
        reuse=next(c for c in transitions if c['kind']=='left_terminal_to_right_first')
        self.assertEqual(reuse['identicalRgbaPixels'],128)
        self.assertEqual(reuse['changedRgbaPixels'],128)
        self.assertEqual(reuse['identicalRows'],list(range(8,16)))
        self.assertEqual(reuse['changedGridBounds'],[0,0,15,7])
        self.assertFalse(r['depthAssessment']['qualified'])
    def test_null_pixels_are_neither_equal_nor_usable_depth(self):
        left=sample();left['alpha']=[None]*256
        later=copy.deepcopy(left);later.update(consumer=2,target=16)
        s=scope(0);s['attemptedConsumers']=2
        r=report([left,later,s])
        comparison=r['sampleComparisons'][0]
        self.assertEqual(comparison['comparablePixels'],0)
        self.assertEqual(comparison['identicalRgbaPixels'],0)
        self.assertEqual(comparison['identicalRows'],[])
        self.assertEqual(r['rows'][0]['statistics']['nonfinite'],256)
        self.assertFalse(r['depthAssessment']['qualified'])
    def test_queue_skip_and_missing_readback_are_reported(self):
        start=f'run_start: run_id={RUN} build_manifest_id={BUILD}'
        end=f'run_end: run_id={RUN}'
        queue='native_hdr_alpha_queue: status={};phase=matching_native_sampler_draw;sampler=0;consumer={};eye=0;frame=8;hr={};temporal_admitted=false'
        s=scope(0);s['attemptedConsumers']=2
        r=analyze([start,queue.format('skipped',1,1),queue.format('queued',2,0),
                   'native_hdr_source_scope: '+json.dumps(s),end],RUN,BUILD)
        self.assertEqual(r['status'],'not_observed')
        self.assertEqual(r['summary']['queueSkipped'],1)
        self.assertEqual(r['summary']['queuedWithoutReadback'],1)
        self.assertEqual(r['scopes'][0]['missingConsumers'],[1,2])
        self.assertFalse(r['scopes'][0]['deliveryComplete'])
        self.assertEqual({f['reason'] for f in r['deliveryFailures']},
                         {'queue_skipped','queued_without_readback','missing_consumer_readback'})
    def test_closed_scope_does_not_imply_all_consumers_delivered(self):
        s=scope(0);s['attemptedConsumers']=3
        r=report([sample(),s])
        self.assertTrue(r['rows'][0]['scopeComplete'])
        self.assertEqual(r['scopes'][0]['missingConsumers'],[2,3])
        self.assertFalse(r['scopes'][0]['deliveryComplete'])
    def test_sample_cannot_claim_an_unattempted_or_duplicate_consumer(self):
        s=scope(0);s['attemptedConsumers']=0
        self.assertEqual(report([sample(),s])['status'],'rejected')
        second=sample();second.update(source=77,pose=88)
        self.assertEqual(report([sample(),second,scope(0)])['status'],'rejected')
    def test_all_positive_alpha_still_does_not_qualify_depth(self):
        r=report([sample(),scope(0),sample(1),scope(1)])
        self.assertEqual(r['summary']['matchedConsumerPairs'],1)
        self.assertEqual(r['depthAssessment']['status'],'unqualified')
        self.assertFalse(r['depthAssessment']['qualified'])
        self.assertFalse(r['headsetValidated'])
    def test_queue_contradictions_and_ambiguous_delivery_reject(self):
        q='native_hdr_alpha_queue: status=queued;phase=matching_native_sampler_draw;sampler=0;consumer=1;eye=0;frame=8;hr=0;temporal_admitted=false'
        for events in ([q,q,sample(),scope(0)],
                       [q.replace('queued','skipped'),sample(),scope(0)],
                       [q.replace('sampler=0','sampler=2'),sample(),scope(0)]):
            self.assertEqual(report(events)['status'],'rejected')
        other=sample();other.update(scope=3,generation=3)
        end=scope(0);end.update(scope=3,generation=3)
        self.assertEqual(report([q,sample(),scope(0),other,end])['status'],'rejected')
    def test_legacy_queue_does_not_invent_consumer_or_pose_witness(self):
        s=sample()
        for key in ('scopeKind','pose','consumer','sampler','target','shader','rgb'):s.pop(key)
        q='native_hdr_alpha_queue: status=queued;phase=matching_native_sampler_draw;sampler=0;eye=0;frame=8;hr=0;temporal_admitted=false'
        r=report([q,s])
        self.assertEqual(r['status'],'observed')
        self.assertEqual(r['queues'][0]['sampleLines'],[3])
        self.assertEqual(r['summary']['matchedConsumerPairs'],0)
        self.assertFalse(r['sampleComparisons'])
    def test_incomplete_and_missing_scope_ends_remain_visible(self):
        s=scope(0);s['complete']=False
        r=report([sample(),s,sample(1)])
        self.assertFalse(r['pairs'][0]['scopeComplete'])
        self.assertEqual({f['reason'] for f in r['deliveryFailures']},
                         {'incomplete_scope','missing_scope_end'})
    def test_incomplete_scope_cannot_claim_complete_writer_observation(self):
        first=sample();second=sample();second['consumer']=2;second['alpha'][0]=7
        end=scope(0);end.update(complete=False,attemptedConsumers=2)
        for event in (first,second,end):
            event.update(sourceWriteCalls=4,sourceAlphaWriteCalls=3,
                         sourceLastWriterShader=9,sourceWriteObservationComplete=True)
        result=report([first,second,end])
        comparison=result['sampleComparisons'][0]
        self.assertFalse(comparison['sourceWriteObservationComplete'])
        self.assertIsNone(comparison['contentChangedWithoutObservedWriteCalls'])
        self.assertEqual(result['summary']['completeSourceWriteWitnessSamples'],0)
    def test_invalid_receipts_and_malformed_objects_reject(self):
        q='native_hdr_alpha_queue: status=queued;phase=matching_native_sampler_draw;sampler=0;consumer=1;eye=0;frame=8;hr=0;temporal_admitted=false'
        for bad in (q+';eye=0',q.replace('hr=0','hr=2147500037'),
                    q.replace('consumer=1','consumer=4'),q.replace('eye=0','eye=2'),
                    q.replace('false','true'),'native_hdr_alpha_probe: []'):
            self.assertEqual(report([bad])['status'],'rejected')
        self.assertEqual(report([],extra=[q])['status'],'rejected')
    def test_pair_plateau_color_and_scope_are_diagnostic_only(self):
        r=report([sample(),scope(0),sample(1),scope(1)])
        self.assertEqual(r['status'],'observed');self.assertEqual(len(r['pairs']),1)
        self.assertTrue(r['pairs'][0]['scopeComplete']);self.assertFalse(r['temporalAdmitted'])
        self.assertEqual(r['rows'][0]['statistics']['oneGridBounds'],[0,8,15,15])
    def test_old_grids_remain_content_without_new_provenance(self):
        s=sample()
        for key in ('scopeKind','pose','consumer','sampler','target','shader','rgb'):s.pop(key)
        r=report([s]);self.assertEqual(r['status'],'observed');self.assertFalse(r['rows'][0]['scopeComplete'])
    def test_mismatch_never_completes_a_pair(self):
        for field in ('pose','generation','source','frame'):
            right=sample(1);right[field]+=1
            self.assertFalse(report([sample(),scope(0),right,scope(1)])['pairs'])
        r=report([sample(),sample(1)]);self.assertFalse(r['pairs'][0]['scopeComplete'])
    def test_malformed_or_overclaimed_content_rejects(self):
        for field,value in (('alpha',[1]),('rgb',[1]),('pose',0),('consumer',4),('temporalAdmitted',True),('eye',2),('sampler',16)):
            s=sample();s[field]=value;self.assertEqual(report([s])['status'],'rejected')
        self.assertEqual(report([sample(),sample()])['status'],'rejected')
        self.assertEqual(report([],['native_hdr_alpha_probe: '+json.dumps(sample())])['status'],'rejected')
        self.assertEqual(analyze([],RUN,BUILD)['status'],'rejected')
if __name__=='__main__':unittest.main()
