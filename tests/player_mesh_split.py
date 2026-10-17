"""Geometry/ownership regression; no proprietary game assets in the fixture."""
import sys, struct, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from split_coj_player_mesh import split_player_mesh, chunks

def chunk(kind, header=b'', children=b''):
    return struct.pack('<4I',kind,0,16+len(header)+len(children),len(header))+header+children

def fixture(body_name='BillyBody', coat=False):
    names=['Bip01 Spine','Bip01 L Forearm','Bip01 L Hand',
           'Bip01 R Forearm','Bip01 R Hand']
    if coat: names += ['Lip_Cloak_L_A00','Lip_Cloak_L_A01','Lip_Cloak_R_D03']
    names += [body_name]
    nodes=[]
    for name in names[:-1]:
        h=bytearray(240);struct.pack_into('<I',h,0,8);h[4:36]=name.encode().ljust(32,b'\0')
        nodes.append(chunk(1,h))
    # body, arm, hand and wrist boundary, each with independent vertices.
    bone_ids=[0]*3+[1]*3+[2]*3+[1,2,2]+[3]*3+[4]*3
    if coat: bone_ids += [5]*3+[6]*3+[7]*3
    nv=ni=len(bone_ids);nb=len(names)-1
    weights=b''.join(struct.pack('<4B4H',b,0,0,0,32767,0,0,0) for b in bone_ids)
    indices=struct.pack('<'+'H'*ni,*range(ni))
    geom=chunk(256,struct.pack('<4I',nv,ni,1,0),
        chunk(0x101,b'\0'*(nv*12))+chunk(0x130,weights)+chunk(0x140,indices)+
        chunk(0x151,struct.pack('<HIIH'+'H'*nb,0,0,ni,nb,*range(nb))))
    h=bytearray(240);struct.pack_into('<I',h,0,2);h[4:36]=body_name.encode().ljust(32,b'\0')
    nodes.append(chunk(1,h,geom))
    payload=struct.pack('<2I',len(names),1)+chunk(0x500,b'fixture.mat'.ljust(64,b'\0'))+b''.join(nodes)+chunk(0x600,b'unchanged auxiliary data')
    return chunk(0x48534d,payload[:8],payload[8:])

class Split(unittest.TestCase):
    def test_only_ray_free_coat_moves_to_scoped_hidden_node(self):
        for body_name,hidden_cloth in [('RayBody',2),('BillyBody',0)]:
            original=fixture(body_name,coat=True)
            result,stats=split_player_mesh(original,body_name)
            self.assertEqual(stats['arm_triangles'],3)
            self.assertEqual(stats['free_coat_triangles'],hidden_cloth)
            self.assertEqual(stats['retained_triangles'],6-hidden_cloth)
            nodes=[c for c in chunks(result,24,len(result)) if c[0]==1]
            body=next(c for c in nodes if result[c[1]+20:c[1]+52].split(b'\0')[0].decode()==body_name)
            def triangles(node):
                geom=next(chunks(result,node[1]+16+node[3],node[1]+node[2]))
                index=next(c for c in chunks(result,geom[1]+16+geom[3],geom[1]+geom[2]) if c[0]==0x140)
                return [struct.unpack_from('<3H',result,index[1]+16+i*6) for i in range(9)]
            kept,hidden=triangles(body),triangles(nodes[-1])
            for i in (0,2,5,6): # torso, hands and anchored row A00 survive.
                self.assertEqual(kept[i],tuple(range(i*3,i*3+3)))
                self.assertEqual(len(set(hidden[i])),1)
            for i in (7,8):
                self.assertEqual(len(set(kept[i]))==1,body_name=='RayBody')
                self.assertEqual(len(set(hidden[i]))==1,body_name!='RayBody')

    def test_split_preserves_body_hands_and_reassembles_original_arm_faces(self):
        original=fixture(); result, stats=split_player_mesh(original,'BillyBody')
        self.assertEqual(stats['arm_triangles'],3) # L, boundary, R
        self.assertEqual(stats['retained_triangles'],3) # torso, both hands
        root_nodes=[c for c in chunks(result,24,len(result)) if c[0]==1]
        self.assertEqual(len(root_nodes),7)
        self.assertEqual(result[root_nodes[-1][1]+20:root_nodes[-1][1]+52].split(b'\0')[0],b'CoJVRHiddenArms')
        self.assertEqual(struct.unpack_from('<I',result,16)[0],7)
        # Existing chunks retain offsets; only body indices and root fields change.
        original_chunks=list(chunks(original,24,len(original)))
        body=next(c for c in original_chunks if c[0]==1 and original[c[1]+20:c[1]+52].startswith(b'BillyBody\0'))
        arm=root_nodes[-1]
        def children(data,c):
            geom=next(chunks(data,c[1]+16+c[3],c[1]+c[2]))
            return {k:(p,s,h) for k,p,s,h in chunks(data,geom[1]+16+geom[3],geom[1]+geom[2])}
        before=children(original,body); kept=children(result,body); removed=children(result,arm)
        for k,(p,s,h) in before.items():
            if k!=0x140:
                self.assertEqual(original[p:p+s],result[kept[k][0]:kept[k][0]+s])
                self.assertEqual(original[p:p+s],result[removed[k][0]:removed[k][0]+s])
        p=before[0x140][0]+16; a=kept[0x140][0]+16; b=removed[0x140][0]+16
        for i in range(6):
            native=original[p+6*i:p+6*i+6]; ka=result[a+6*i:a+6*i+6]; ra=result[b+6*i:b+6*i+6]
            deg=lambda t:len(set(struct.unpack('<3H',t)))==1
            self.assertNotEqual(deg(ka),deg(ra))
            self.assertEqual(ra if deg(ka) else ka,native)
        for k,p,s,h in original_chunks:
            if p!=body[1]:self.assertEqual(original[p:p+s],result[p:p+s])

    def test_invalid_chunk_and_unmapped_palette_fail_without_output(self):
        data=bytearray(fixture());struct.pack_into('<I',data,8,len(data)+1)
        with self.assertRaises(ValueError):split_player_mesh(bytes(data),'BillyBody')
        with self.assertRaises(ValueError):split_player_mesh(fixture(),'UnknownBody')
        data=bytearray(fixture())
        body=next(c for c in chunks(data,24,len(data)) if c[0]==1 and data[c[1]+20:c[1]+52].startswith(b'BillyBody\0'))
        geom=next(chunks(data,body[1]+16+body[3],body[1]+body[2]))
        palette=next(c for c in chunks(data,geom[1]+16+geom[3],geom[1]+geom[2]) if c[0]==0x151)
        struct.pack_into('<H',data,palette[1]+16+12,1000)
        with self.assertRaisesRegex(ValueError,'palette'):split_player_mesh(bytes(data),'BillyBody')

    def test_cloth_hand_boundary_is_rejected(self):
        data=bytearray(fixture('RayBody',coat=True))
        body=next(c for c in chunks(data,24,len(data)) if c[0]==1 and data[c[1]+20:c[1]+52].startswith(b'RayBody\0'))
        geom=next(chunks(data,body[1]+16+body[3],body[1]+body[2]))
        weights=next(c for c in chunks(data,geom[1]+16+geom[3],geom[1]+geom[2]) if c[0]==0x130)
        data[weights[1]+16+21*12]=2 # Hand vertex in a triangle still touching free cloth.
        with self.assertRaisesRegex(ValueError,'overlaps retained hand'):
            split_player_mesh(bytes(data),'RayBody')

if __name__=='__main__':unittest.main()
