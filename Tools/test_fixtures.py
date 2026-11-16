import json,struct,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class ContractFixtures(unittest.TestCase):
    def test_related_repository_links(self):
        text=(ROOT/'README.md').read_text()
        for name in ('SpatialFabric','SpatialFabric-Unity','SpatialFabric-Interop','AudioLoom'):
            self.assertIn('https://github.com/NPLAudio/'+name,text)
    def test_schema_keeps_v1_identity(self):
        d=json.loads((ROOT/'Schemas/scene-v1.schema.json').read_text());self.assertEqual(d['properties']['schema']['const'],'org.nplaudio.spatialfabric.scene');self.assertEqual(d['properties']['schema_version']['const'],1)
    def test_vector_packets_are_aligned_and_named(self):
        d=json.loads((ROOT/'Fixtures/osc-v1.json').read_text());self.assertEqual(len(d['valid']),5)
        for p in d['valid']:
            b=bytes.fromhex(p['hex']);self.assertEqual(len(b)%4,0);self.assertEqual(b.split(b'\0')[0].decode(),p['address'])
    def test_request_has_expected_network_order_arguments(self):
        d=json.loads((ROOT/'Fixtures/osc-v1.json').read_text());p=next(p for p in d['valid'] if p['name']=='scene_query');b=bytes.fromhex(p['hex']);self.assertEqual(struct.unpack('>iii',b[-12:]),(9011,0,16))
    def test_custom_contract_packet_vectors(self):
        d=json.loads((ROOT/'Fixtures/custom-v1.json').read_text());self.assertEqual(len(d['vectors']),5)
        for row in d['vectors']:
            for message in row['expected']:
                b=bytes.fromhex(message['hex']);self.assertEqual(len(b)%4,0);self.assertEqual(b.split(b'\0')[0].decode(),message['address'])
    def test_axes_are_explicit(self):
        d=json.loads((ROOT/'Fixtures/coordinates-v1.json').read_text())
        for row in d['vectors']:
            f,r,u=row['canonical'];self.assertEqual(row['unity_m'],[r,u,f]);self.assertEqual(row['unreal_cm'],[100*f,100*r,100*u]);self.assertEqual(row['adm_xyz'],[r,f,u])
if __name__=='__main__':unittest.main()
