import copy
from pathlib import Path
import sys
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from pathstudio import edit, model, shapes, store  # noqa: E402


def make():
    document = store.new_document()
    document['tracks'][0]['keyframes'] = [{'t': 0, 'position': [0, 0, 0]}, {'t': 4, 'position': [4, 0, 0]}]
    return document


class EditTests(unittest.TestCase):
    def test_add_keyframe_lies_on_the_current_path(self):
        document = make()
        index = edit.add_keyframe(document, 0, 1.0)
        self.assertEqual(1, index)
        self.assertEqual([1.0, 0.0, 0.0], document['tracks'][0]['keyframes'][1]['position'])
        self.assertEqual([], model.validate(document))
        self.assertEqual(3, edit.add_keyframe(document, 0, 9.0))  # after the end: holds the last position
        self.assertEqual([4.0, 0.0, 0.0], document['tracks'][0]['keyframes'][3]['position'])

    def test_add_respects_speed_and_inherits_segment_style(self):
        document = make()
        document['tracks'][0]['speed'] = 2
        document['tracks'][0]['keyframes'][0]['ease'] = 'in_out'
        edit.add_keyframe(document, 0, 2.0)
        new = document['tracks'][0]['keyframes'][1]
        self.assertEqual('in_out', new['ease'])
        reference = model.sample(make() | {'tracks': [dict(make()['tracks'][0], speed=2, keyframes=[
            {'t': 0, 'position': [0, 0, 0], 'ease': 'in_out'}, {'t': 4, 'position': [4, 0, 0]}])]}, 'track1', 1.0)['position']
        self.assertEqual([round(v, 6) for v in reference], new['position'])

    def test_add_rejects_duplicates_negative_and_invalid_documents(self):
        document = make()
        for t in (0, 4, 0.0004, -1, float('nan'), True):
            before = copy.deepcopy(document)
            with self.assertRaises(ValueError):
                edit.add_keyframe(document, 0, t)
            self.assertEqual(before, document)
        document['tracks'][0]['speed'] = 0
        with self.assertRaisesRegex(ValueError, 'validation'):
            edit.add_keyframe(document, 0, 1)

    def test_retime_is_clamped_between_neighbors(self):
        document = make()
        edit.add_keyframe(document, 0, 1.0)
        self.assertEqual(3.999, edit.clamp_time(document, 0, 1, 50))
        self.assertEqual(0.001, edit.clamp_time(document, 0, 1, -5))
        edit.set_time(document, 0, 1, 2.5)
        self.assertEqual([], model.validate(document))
        self.assertEqual(0.0, edit.clamp_time(document, 0, 0, -3))
        self.assertEqual(2.499, edit.clamp_time(document, 0, 0, 99))

    def test_set_position_detaches_waypoint_and_enforces_range(self):
        document = make()
        document['waypoints'] = [{'id': 'w', 'position': [1, 1, 0]}]
        document['tracks'][0]['keyframes'][1] = {'t': 4, 'waypoint': 'w'}
        edit.set_position(document, 0, 1, [0.5, 0.25, 0.125])
        self.assertEqual({'t': 4, 'position': [0.5, 0.25, 0.125]}, document['tracks'][0]['keyframes'][1])
        for bad in ([17, 0, 0], [0, 0], [float('inf'), 0, 0], ['a', 0, 0]):
            with self.assertRaises(ValueError):
                edit.set_position(document, 0, 1, bad)

    def test_choices_delete_and_generated_replacement(self):
        document = make()
        edit.set_choice(document, 0, 0, 'interp', 'bezier')
        edit.set_choice(document, 0, 0, 'ease', 'out')
        for field, value in (('interp', 'spline'), ('speed', 'x'), ('ease', None)):
            with self.assertRaises(ValueError):
                edit.set_choice(document, 0, 0, field, value)
        edit.delete_keyframe(document, 0, 1)
        with self.assertRaisesRegex(ValueError, 'at least one'):
            edit.delete_keyframe(document, 0, 0)
        edit.replace_keyframes(document, 0, shapes.circle((0, 0, 0), 1, 8, 12))
        self.assertEqual(12, len(document['tracks'][0]['keyframes']))
        before = copy.deepcopy(document)
        with self.assertRaises(ValueError):
            edit.replace_keyframes(document, 0, [{'t': 1, 'position': [0, 0, 0]}, {'t': 1, 'position': [0, 0, 0]}])
        self.assertEqual(before, document)

    def test_bad_indices_are_value_errors(self):
        document = make()
        for call in (lambda: edit.set_time(document, 5, 0, 1), lambda: edit.set_time(document, 0, 9, 1), lambda: edit.set_time(document, 0, True, 1),
                     lambda: edit.delete_keyframe({}, 0, 0), lambda: edit.set_position(document, 0, -1, [0, 0, 0])):
            with self.assertRaises(ValueError):
                call()


if __name__ == '__main__':
    unittest.main()
