import math
from pathlib import Path
import sys
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from pathstudio import model, shapes  # noqa: E402
from pathstudio.history import History  # noqa: E402


def doc(keys):
    return {'schema': model.SCHEMA, 'schema_version': 1, 'space': 'canonical',
            'tracks': [{'id': 't', 'target': {'kind': 'object', 'key': 'k'}, 'keyframes': keys}]}


class HistoryTests(unittest.TestCase):
    def test_undo_redo_roundtrip_and_isolation(self):
        history, state = History(), {'a': [1]}
        history.checkpoint(state)
        state['a'].append(2)  # mutation after the checkpoint must not alter the snapshot
        previous = history.undo(state)
        self.assertEqual({'a': [1]}, previous)
        self.assertTrue(history.can_redo())
        self.assertEqual({'a': [1, 2]}, history.redo(previous))
        self.assertIsNone(History().undo({}))
        self.assertIsNone(History().redo({}))

    def test_new_edit_clears_redo_and_depth_is_bounded(self):
        history = History(max_depth=3)
        for i in range(10):
            history.checkpoint({'i': i})
        self.assertEqual(3, len(history.undo_stack))
        history.undo({'now': 1})
        history.checkpoint({'x': 1})
        self.assertFalse(history.can_redo())


class ShapeTests(unittest.TestCase):
    def test_every_shape_validates_and_stays_in_bounds(self):
        for name in shapes.SHAPES:
            keys = shapes.generate(name, (0.1, -0.2, 0.3), 1.5, 8, 24, 7)
            self.assertEqual([], model.validate(doc(keys)), name)
            self.assertAlmostEqual(8, keys[-1]['t'])
            self.assertNotIn('interp', keys[-1])

    def test_circle_is_closed_with_hand_checked_points(self):
        keys = shapes.circle((0, 0, 0), 2.0, 8.0, 5, clockwise=False)
        self.assertEqual([2.0, 0.0, 0.0], keys[0]['position'])
        self.assertEqual(keys[0]['position'], keys[-1]['position'])
        self.assertAlmostEqual(0.0, keys[1]['position'][0], places=5)  # quarter turn: front 0
        self.assertAlmostEqual(-2.0, keys[1]['position'][1], places=5)  # counter-clockwise from above
        clockwise = shapes.circle((0, 0, 0), 2.0, 8.0, 5, clockwise=True)
        self.assertAlmostEqual(2.0, clockwise[1]['position'][1], places=5)

    def test_random_walk_is_reproducible_bounded_and_seed_sensitive(self):
        a = shapes.random_walk(5, (0, 0, 0), 0.3, 1.0, 8, 40)
        self.assertEqual(a, shapes.random_walk(5, (0, 0, 0), 0.3, 1.0, 8, 40))
        self.assertNotEqual(a, shapes.random_walk(6, (0, 0, 0), 0.3, 1.0, 8, 40))
        for key in a:
            self.assertLessEqual(max(abs(key['position'][0]), abs(key['position'][1])), 1.0 + 1e-9)

    def test_line_and_spiral_endpoints(self):
        keys = shapes.line((0, -1, 0), (0, 1, 0.5), 4)
        self.assertEqual([[0, -1, 0], [0, 1, 0.5]], [k['position'] for k in keys])
        spiral = shapes.spiral((0, 0, 0), 0.5, 1.5, 1.0, 6, 25, 0.0, 1.0)
        self.assertAlmostEqual(0.5, math.hypot(*spiral[0]['position'][:2]))
        self.assertAlmostEqual(1.5, math.hypot(*spiral[-1]['position'][:2]), places=5)
        self.assertEqual(1.0, spiral[-1]['position'][2])

    def test_bad_parameters_are_rejected_not_clamped_silently(self):
        for call in (lambda: shapes.circle(duration=0), lambda: shapes.circle(points=1),
                     lambda: shapes.circle(radius=float('nan')), lambda: shapes.random_walk('x'),
                     lambda: shapes.random_walk(1, bound=0), lambda: shapes.generate('star'),
                     lambda: shapes.circle(points=True)):
            with self.assertRaises(ValueError):
                call()

    def test_huge_values_are_clamped_to_contract_range(self):
        keys = shapes.circle((0, 0, 0), 1000.0, 4.0, 8)
        self.assertEqual([], model.validate(doc(keys)))


if __name__ == '__main__':
    unittest.main()
