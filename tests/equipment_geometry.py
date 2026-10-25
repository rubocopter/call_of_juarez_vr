"""Interpreted equipment geometry fixtures; no game or compiler invocation."""
import copy
import importlib.util
import math
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
spec = importlib.util.find_spec('coj_equipment_geometry')


class EquipmentGeometry(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(spec, 'equipment geometry implementation is missing')
        global g
        import coj_equipment_geometry as g

    def near(self, a, b):
        self.assertLess(max(abs(x-y) for ra, rb in zip(a,b) for x,y in zip(ra,rb)), 1e-7)

    def test_inverse_preserves_full_affine_scale_and_roll(self):
        a = [[0,2,0,0],[-3,0,0,0],[0,0,4,0],[17,-3,12,1]]
        self.near(g.multiply(a, g.inverse(a)), g.identity())
        self.near(g.multiply(g.inverse(a), a), g.identity())

    def test_holster_recomposition_and_covariance(self):
        holder, holster, mount = g.identity(), g.identity(), g.identity()
        holder[3][:3] = [1,2,3]
        holster[3][:3] = [-4,5,6]
        mount[0][:3], mount[1][:3] = [0,1,0],[-1,0,0]
        mount[3][:3] = [100,200,300]
        weapon, grasp = g.compose_holster(holder,holster,mount)
        self.near(g.multiply(holster,weapon),mount)
        self.near(g.multiply(holder,weapon),grasp)
        self.assertEqual(grasp[3][:3], [103,205,297])
        shift = g.identity(); shift[3][:3] = [11,22,33]
        _, moved = g.compose_holster(holder,holster,g.multiply(mount,shift))
        self.near(moved,g.multiply(grasp,shift))

    def test_reject_degenerate_nonfinite_projective_and_reflection(self):
        bad = []
        for value in (0, 1e-12, -1, math.inf, math.nan):
            m=g.identity(); m[0][0]=value; bad.append(m)
        m=g.identity(); m[0][3]=0.1; bad.append(m)
        m=g.identity(); m[1][:3]=[1,1e-9,0]; bad.append(m)
        bad += [[], [[1]*4]*3]
        for m in bad:
            with self.subTest(matrix=m), self.assertRaises(ValueError):
                g.inverse(m)

    def test_unknown_models_and_billy_left_do_not_fallback(self):
        self.assertEqual(g.body_mount(g.BODY_HASHES['Ray'],'left'),('HolsterL',8))
        self.assertEqual(g.body_mount(g.BODY_HASHES['Billy'],'right'),('HolsterR',16))
        for digest,side in [('unknown','right'),(g.BODY_HASHES['Billy'],'left'),(g.BODY_HASHES['Ray'],'shoulder')]:
            with self.assertRaises(ValueError): g.body_mount(digest,side)
        with self.assertRaises(ValueError): g.weapon_profile('ColtPeacemaker.msh')
        self.assertEqual(g.weapon_profile(g.WEAPONS['peacemaker'][1]),'peacemaker')

    def test_bow_observer_is_value_only_and_preserves_animation(self):
        root=g.identity(); root[3][:3]=[4,5,6]
        holder=copy.deepcopy(root); holder[3][2]-=51 # current animation, not bind offset
        mesh=copy.deepcopy(root); mesh[3][1]+=2
        hand=g.identity(); hand[3][0]=10
        sample=g.BowSample(actor=0x81234567,bow=12,arrow=13,parent=0x81234567,
            generation=5,frame=7,parent_element=42,resolved_right_element=42,
            bow_sha256=g.BOW_HASH,arrow_sha256=g.ARROW_HASH,
            hand=hand,root=root,holder=holder,mesh=mesh,
            player_sha256=g.BODY_HASHES['Ray'],exact_weapon_bow=True,
            holder_element=0,mesh_element=1)
        original=copy.deepcopy(sample)
        result=g.observe_bow(sample,sample)
        self.assertEqual(sample,original)
        self.near(g.multiply(result['root_in_hand'],hand),root)
        self.near(g.multiply(result['holder_in_root'],root),holder)
        target=g.identity(); target[3][:3]=[100,200,300]
        mapped=g.remap_element(root,target,holder)
        self.near(g.remap_element(target,root,mapped),holder)
        self.assertFalse(result['writer_admitted'])
        for field,value in [('arrow',14),('actor',14),('bow',14),('parent',14),('generation',6),('frame',8),('parent_element',43),('arrow_sha256','unknown')]:
            changed=copy.deepcopy(sample); setattr(changed,field,value)
            with self.subTest(field=field),self.assertRaises(ValueError):g.observe_bow(sample,changed)
        for field,value in [('visible',False),('bow_sha256','unknown'),('player_sha256','unknown'),
            ('exact_weapon_bow',False),('actor',0),('arrow',0xffffffff),('parent',99),
            ('parent_element',-1),('resolved_right_element',43),('holder_element',1),('mesh_element',0),
            ('generation',0),('frame',0),('hand',[[0]*4]*4)]:
            changed=copy.deepcopy(sample); setattr(changed,field,value)
            with self.subTest(field=field),self.assertRaises(ValueError):g.observe_bow(changed,changed)

    def test_mesh_parser_rejects_truncation_and_cycles(self):
        import struct
        with self.assertRaises(ValueError):g.mesh_frames(b'not a mesh')
        node=bytearray(256)
        struct.pack_into('<4I',node,0,1,0,256,240)
        struct.pack_into('<I',node,16,4);node[20:22]=b'A\0'
        struct.pack_into('<H',node,84,0)
        struct.pack_into('<16f',node,92,*sum(g.identity(),[]))
        data=struct.pack('<6I',0x48534d,0,280,8,1,0)+node
        with self.assertRaisesRegex(ValueError,'cycle'):g.mesh_frames(data)

    def test_unknown_archive_is_rejected_before_member_use(self):
        import io
        import zipfile
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as z:z.writestr('WeaponBow.class',b'unknown')
        stream.seek(0)
        with self.assertRaisesRegex(ValueError,'unrecognized original'):
            g.OriginalArchive(stream,'code')

    def test_scoped_foreend_rejects_unpinned_mesh_before_geometry(self):
        self.assertTrue(hasattr(g,'scoped_rifle_contact'),'scoped fore-end measurement missing')
        with self.assertRaisesRegex(ValueError,'unrecognized scoped rifle mesh'):
            g.scoped_rifle_contact(b'unknown mesh')

    def test_bow_skin_profile_retains_authored_influences_without_string_claim(self):
        self.assertTrue(hasattr(g,'bow_skin_profile'),'bow skin observer is missing')
        # Actual original-asset coverage is in equipment_geometry_assets.py.
        with self.assertRaises(ValueError):g.bow_skin_profile(b'bad mesh')

    def test_cli_rejects_output_outside_work_before_opening_inputs(self):
        with self.assertRaisesRegex(ValueError,'ignored work'):
            g.main(['--code-pak','missing','--data0-pak','missing',
                    '--output',str(Path(__file__).resolve().parent/'not-evidence.json')])

    def test_mesh_world_composition_retains_scale_and_rejects_bad_parent(self):
        import struct
        def node(name,parent,matrix):
            result=bytearray(256)
            struct.pack_into('<4I',result,0,1,0,256,240)
            struct.pack_into('<I',result,16,4);result[20:20+len(name)]=name.encode()
            struct.pack_into('<H',result,84,parent)
            struct.pack_into('<16f',result,92,*sum(matrix,[]))
            return result
        parent=g.identity();parent[0][0]=2;parent[3][0]=100
        child=g.identity();child[3][0]=7
        data=struct.pack('<6I',0x48534d,0,536,8,2,0)+node('Parent',65535,parent)+node('Child',0,child)
        nodes=g.mesh_frames(data)
        self.assertEqual(nodes[1]['world'][3][0],114)
        bad=bytearray(data);struct.pack_into('<H',bad,24+256+84,3)
        with self.assertRaisesRegex(ValueError,'invalid mesh parent'):g.mesh_frames(bad)


if __name__=='__main__':unittest.main()
