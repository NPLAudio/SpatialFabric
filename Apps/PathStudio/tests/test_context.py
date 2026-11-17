import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
HUB = APP.parents[1]
sys.path.insert(0, str(APP))
from pathstudio import context  # noqa: E402

EXAMPLE = APP / 'examples/room.sfctx.json'


def doc():
    return {'schema': context.SCHEMA, 'schema_version': 1, 'space': 'canonical', 'objects': [
        {'id': 'wall', 'kind': 'box', 'label': 'Wall', 'position': [1.2, 0, 0.5], 'half_extents': [0.05, 1.2, 0.5]},
        {'id': 'ball', 'kind': 'sphere', 'position': [0, 0, 0], 'radius': 0.3},
        {'id': 'mic', 'kind': 'point', 'position': [0.5, 0.5, 0.2]},
        {'id': 'ears', 'kind': 'listener', 'position': [0, 0, 0.1], 'yaw_degrees': 90}]}


class ContextTests(unittest.TestCase):
    def test_valid_document_and_example_file(self):
        self.assertEqual([], context.validate(doc()))
        objects = context.load_context(EXAMPLE)
        self.assertGreaterEqual(len(objects), 5)
        self.assertTrue(all(o['source'] == 'file' for o in objects))

    def test_objects_carry_only_what_each_kind_needs(self):
        by_id = {o['id']: o for o in context.objects_from_document(doc())}
        self.assertEqual([0.05, 1.2, 0.5], by_id['wall']['half_extents'])
        self.assertEqual(0.3, by_id['ball']['radius'])
        self.assertEqual(90.0, by_id['ears']['yaw_degrees'])
        self.assertEqual('mic', by_id['mic']['label'])  # falls back to the id
        self.assertNotIn('radius', by_id['wall'])

    def test_invalid_documents_report_codes(self):
        cases = {
            'bad_schema': lambda d: d.update(schema='x'),
            'bad_version': lambda d: d.update(schema_version=2),
            'bad_space': lambda d: d.update(space='unity'),
            'bad_objects': lambda d: d.update(objects='no'),
            'bad_object': lambda d: d['objects'].append(5),
            'bad_id': lambda d: d['objects'][0].update(id='bad id!'),
            'duplicate_id': lambda d: d['objects'][1].update(id='wall'),
            'bad_kind': lambda d: d['objects'][0].update(kind='mesh'),
            'bad_position': lambda d: d['objects'][0].update(position=[0, 0]),
            'bad_label': lambda d: d['objects'][0].update(label=3),
            'bad_extent': lambda d: d['objects'][0].update(half_extents=[1, -1, 1]),
            'bad_yaw': lambda d: d['objects'][3].update(yaw_degrees=999),
        }
        for code, mutate in cases.items():
            document = doc()
            mutate(document)
            self.assertIn(code, [c for c, _ in context.validate(document)], code)
        document = doc()
        document['objects'][1]['radius'] = float('nan')
        self.assertIn('bad_extent', [c for c, _ in context.validate(document)])
        document = doc()
        document['objects'][2]['position'] = [99, 0, 0]
        self.assertIn('bad_position', [c for c, _ in context.validate(document)])

    def test_malformed_values_never_raise(self):
        for value in (None, [], 'x', {'objects': [None, {'kind': {}}]}):
            self.assertTrue(context.validate(value))

    def test_load_rejects_bad_files(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'c.json'
            for data, message in ((b'{', 'valid UTF-8 JSON'), (b'\xff', 'valid UTF-8 JSON'), (json.dumps({'schema': 'x'}).encode(), 'Invalid context file')):
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    context.load_context(path)
            path.write_text(json.dumps(doc()))
            old, context.MAX_BYTES = context.MAX_BYTES, 3
            try:
                with self.assertRaisesRegex(ValueError, 'larger'):
                    context.load_context(path)
            finally:
                context.MAX_BYTES = old

    def test_scene_places_only_exactly_located_bindings(self):
        scene = json.loads((HUB / 'Examples/OSCSceneQuery/sample_scene.json').read_text(encoding='utf-8'))
        placed, unplaced = context.objects_from_scene(scene)
        self.assertEqual(1, len(placed))
        self.assertEqual([0.5, 0.25, 0.1], placed[0]['position'])
        self.assertEqual('scene', placed[0]['source'])
        self.assertEqual(0, unplaced)
        scene = copy.deepcopy(scene)
        scene['objects'].append({'kind': 'listener', 'key': 'L', 'world_transform': {'position_cm': [0, 0, 0]}})
        scene['objects'].append({'kind': 'spatial_binding', 'key': 'B', 'stage_normalized': [1, 'x', 0]})
        placed, unplaced = context.objects_from_scene(scene)
        self.assertEqual(1, len(placed))  # the malformed binding is skipped, not guessed
        self.assertEqual(1, unplaced)  # the listener has only an engine-world transform
        self.assertEqual(([], 0), context.objects_from_scene(None))


if __name__ == '__main__':
    unittest.main()
