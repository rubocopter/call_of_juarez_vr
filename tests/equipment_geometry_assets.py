"""Read-only pinned-original integration; numeric C++ profile audit, NOT compilation."""
import argparse
import json
import math
from pathlib import Path
import re
import sys
import unittest

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import coj_equipment_geometry as g


class OriginalEquipment(unittest.TestCase):
    def test_scoped_foreend_is_measured_from_its_own_mesh(self):
        self.assertTrue('scoped_rifle_contact' in report,'scoped fore-end measurement missing')
        contact=report['scoped_rifle_contact']
        self.assertEqual(contact['source']['sha256'],
            'a989d5c4bcaad39e250a0fcaa070c985ec832ec51e2f48e6f2d62eb580d532cd')
        self.assertEqual((contact['element'],contact['element_index'],contact['triangle_count']),('GunBody',1,60))
        self.assertLess(contact['node_bounds_error_cm'],.002)
        for got,want in zip(contact['point_cm'],[.009580320795,-41.159535290790,-4.031802784884]):
            self.assertAlmostEqual(got,want,places=8)
        header=(ROOT/'src/games/call_of_juarez/long_gun_contact.hpp').read_text()
        row=re.search(r'"WeaponRifleWinchesterScope"\)\)\s*return\s*\{"GunBody",\{([^}]+)\}',header)
        self.assertIsNotNone(row,'measured scoped fore-end not connected to runtime profile')
        for value,measured in zip(row[1].split(','),contact['point_cm']):
            self.assertLess(abs(float(value.strip().rstrip('F'))-measured),.000001)

    def test_all_original_holster_recompositions(self):
        self.assertEqual(len(report['holsters']),12)
        distances={'peacemaker':12.472265,'frontier':9.770338,'walker':12.976699,'cogswell':10.743553}
        for row in report['holsters']:
            self.assertAlmostEqual(row['grip_distance_cm'],distances[row['model']],places=5)
            self.assertLess(row['recomposition_error'],1e-7)
            self.assertFalse(row['contact_admitted'])
        self.assertFalse(any(r['body']=='Billy' and r['side']=='left' for r in report['holsters']))

    def test_cpp_measurements_equal_original_assets(self):
        header=(ROOT/'src/games/call_of_juarez/holster_asset_profiles.hpp').read_text()
        rows=re.findall(r'\{"([a-f0-9]{64})",\s*\{\{\{([^}]+)\}\}\},\s*\{\{\{([^}]+)\}\}\},\s*(\d+),\s*(\d+)\}',header)
        self.assertEqual(len(rows),4)
        for digest,holder,holster,hi,si in rows:
            source=next(r for r in report['holsters'] if r['mesh_sha256']==digest)
            for text,key in ((holder,'holder'),(holster,'holster')):
                values=[float(s.strip()) for s in text.split(',')]
                self.assertEqual(values,sum(source[key],[]))
            self.assertEqual((int(hi),int(si)),(source['holder_index'],source['holster_index']))

    def test_bow_original_hierarchy_and_cpp_bind_values(self):
        bow=report['meshes']['Data/Player/Equipment/Bow/Bow.msh']['nodes']
        arrow=report['meshes']['Data/Player/Equipment/Bow/BowArrow.msh']['nodes']
        self.assertEqual((bow[1]['name'],bow[9]['name'],bow[9]['parent_index']),('DBowHolder','DArrow',1))
        self.assertEqual([(n['name'],n['parent_index']) for n in arrow],[('DArrowHolder',65535),('ArrowA',0)])
        header=(ROOT/'src/games/call_of_juarez/bow_visual_geometry.hpp').read_text()
        for constant,node in [('arrow_holder_bind',arrow[0]),('arrow_mesh_local_bind',arrow[1])]:
            match=re.search(constant+r'\{\{\{([^}]+)\}\}\}',header)
            self.assertIsNotNone(match)
            self.assertEqual([float(v.strip()) for v in match[1].split(',')],sum(node['local'],[]))
        self.assertFalse(report['runtime_writer_admitted'])

    def test_native_arrow_lifetime_and_gameplay_consumers_remain_explicit(self):
        methods=report['classes']['WeaponBow.class']['methods']
        show='\n'.join(methods['ShowArrowMesh()V'])
        for term in ("'Right_hand'","'Bip01 R Hand'",'FromUpForwardPos','AttachChild'):
            self.assertIn(term,show)
        self.assertNotIn('DArrowHolder',show)
        self.assertIn('MeshObject.delete()', '\n'.join(methods['DestroyArrowMesh()V']))
        self.assertIn('LoadObjectRef', '\n'.join(methods['SGLoadChunk(LFileChunk;)V']))
        drop='\n'.join(methods['DropArrowAmmo()V'])
        for term in ('GetPositionVectorVolatile','GetForwardVectorVolatile','CreateArrowAmmo'):
            self.assertIn(term,drop)
        for name in ('SetAnimSpeed(IFZ)V','SetAnimMorphTime(IFZ)V','PlayAnim(IIZF)I'):
            self.assertIn('m_cArrowMesh','\n'.join(methods[name]))

    def test_bow_skin_ownership_is_observed_not_a_string_simulation(self):
        profile=report['bow_skin_profile']
        self.assertEqual(profile['node'],'Bow')
        self.assertEqual(profile['vertex_count'],395)
        self.assertEqual(profile['index_count'],1278)
        self.assertFalse(profile['string_endpoints_qualified'])
        self.assertTrue(profile['bone_vertex_counts'])
        self.assertTrue(all(name.startswith('BowBone') for name in profile['bone_vertex_counts']))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code-pak',type=Path,required=True)
    parser.add_argument('--data0-pak',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not args.output.resolve().is_relative_to((ROOT/'work').resolve()):raise ValueError('output must stay in work/')
    report=g.build_inventory(args.code_pak,args.data0_pak)
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(OriginalEquipment))
    with args.output.open('x',encoding='utf-8') as f:json.dump(report,f,indent=2,allow_nan=False)
    sys.exit(0 if result.wasSuccessful() else 1)
