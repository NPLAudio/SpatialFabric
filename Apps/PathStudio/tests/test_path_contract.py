import copy
import json
import math
from pathlib import Path
import sys
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from pathstudio import model  # noqa: E402

HUB = APP.parents[1]
FIXTURE = json.loads((HUB / 'Fixtures/path-v1.json').read_text(encoding='utf-8'))
TOLERANCE = FIXTURE['tolerance']


class PathContract(unittest.TestCase):
    def test_golden_samples_match_hand_computed_values(self):
        for case in FIXTURE['cases']:
            self.assertEqual([], model.validate(case['document']), case['name'])
            for sample in case['samples']:
                got = model.sample(case['document'], sample['track'], sample['time'])
                for a, b in zip(got['position'], sample['position']):
                    self.assertAlmostEqual(a, b, delta=TOLERANCE, msg=f"{case['name']} t={sample['time']}")
                for name, value in sample.get('parameters', {}).items():
                    self.assertAlmostEqual(got['parameters'][name], value, delta=TOLERANCE, msg=case['name'])

    def test_invalid_documents_report_exact_codes(self):
        for case in FIXTURE['invalid']:
            codes = sorted({code for code, _ in model.validate(case['document'])})
            self.assertEqual(case['codes'], codes, case['name'])

    def test_validation_reports_json_paths(self):
        doc = copy.deepcopy(FIXTURE['invalid'][0]['document'])
        doc['tracks'][0]['keyframes'][1]['t'] = 0
        self.assertIn(('times_not_increasing', '$.tracks[0].keyframes[1].t'), model.validate(doc))

    def test_engine_conversions_use_shared_axes(self):
        for row in FIXTURE['engine_conversions']:
            for engine in ('unity_m', 'unreal_cm', 'adm_xyz'):
                self.assertEqual(row[engine], model.to_engine(row['canonical'], engine))
        coordinates = json.loads((HUB / 'Fixtures/coordinates-v1.json').read_text())
        for row in coordinates['vectors']:
            for engine in ('unity_m', 'unreal_cm', 'adm_xyz'):
                self.assertEqual(row[engine], model.to_engine(row['canonical'], engine))
        with self.assertRaises(ValueError):
            model.to_engine([0, 0, 0], 'blender')

    def test_catmull_rom_passes_through_keyframes(self):
        keys = [{'t': t, 'position': p, 'interp': 'catmull_rom'} for t, p in
                [(0, [0, 0, 0]), (1, [1, 2, -1]), (2.5, [-1, 3, 0.5]), (4, [2, -2, 1])]]
        doc = {'schema': model.SCHEMA, 'schema_version': 1, 'space': 'canonical',
               'tracks': [{'id': 't', 'target': {'kind': 'camera', 'key': 'cam'}, 'keyframes': keys}]}
        self.assertEqual([], model.validate(doc))
        for key in keys:
            for a, b in zip(model.sample(doc, 't', key['t'])['position'], key['position']):
                self.assertAlmostEqual(a, b, delta=TOLERANCE)

    def test_sampling_is_finite_and_continuous_inside_segments(self):
        for case in FIXTURE['cases']:
            doc = case['document']
            previous = None
            for step in range(0, 801):
                position = model.sample(doc, 't', step * 0.01)['position']
                self.assertTrue(all(math.isfinite(v) for v in position), case['name'])
                if previous and case['name'] not in ('hold_steps', 'loop_wraps_without_closing_segment'):
                    if 'ping_pong' not in case['name'] and 'speed' not in case['name']:
                        self.assertLess(max(abs(a - b) for a, b in zip(position, previous)), 0.5, case['name'])
                previous = position

    def test_unknown_fields_are_ignored_for_forward_compatibility(self):
        doc = copy.deepcopy(FIXTURE['cases'][0]['document'])
        doc['future'] = {'x': 1}
        doc['tracks'][0]['keyframes'][0]['look_at'] = [1, 0, 0]
        self.assertEqual([], model.validate(doc))

    def test_schema_file_matches_contract_limits(self):
        schema = json.loads((HUB / 'Schemas/path-v1.schema.json').read_text(encoding='utf-8'))
        self.assertEqual(model.SCHEMA, schema['properties']['schema']['const'])
        self.assertEqual(1, schema['properties']['schema_version']['const'])
        self.assertEqual(model.MAX_TRACKS, schema['properties']['tracks']['maxItems'])
        track = schema['properties']['tracks']['items']['properties']
        self.assertEqual(model.MAX_POINTS, track['keyframes']['maxItems'])
        self.assertEqual(list(model.KINDS), track['target']['properties']['kind']['enum'])
        self.assertEqual(list(model.LOOPS), track['loop']['enum'])
        frame = track['keyframes']['items']['properties']
        self.assertEqual(list(model.INTERPS), frame['interp']['enum'])
        self.assertEqual(list(model.EASES), frame['ease']['enum'])
        self.assertEqual(model.POSITION_LIMIT, schema['$defs']['vector']['items']['maximum'])

    def test_malformed_input_never_raises(self):
        for value in (None, [], 3, 'x', {'schema': model.SCHEMA, 'schema_version': True, 'tracks': [None, 5, {'keyframes': 'x'}]}):
            self.assertTrue(model.validate(value))


if __name__ == '__main__':
    unittest.main()
