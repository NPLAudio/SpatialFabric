import copy
import math
import os
from pathlib import Path
import sys
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
try:
    from PySide6.QtCore import QPointF, Qt
    from PySide6.QtTest import QTest
    from PySide6.QtWidgets import QApplication
    from pathstudio.app import MainWindow
    from pathstudio.viewport3d import Viewport3D, PRESETS
except ImportError:  # PySide6 is optional for the headless contract tests
    Viewport3D = None


@unittest.skipIf(Viewport3D is None, 'PySide6 not installed')
class ViewportMathTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.view = Viewport3D()
        self.view.resize(800, 600)

    def test_target_is_centered_and_axes_have_the_right_screen_direction(self):
        view = self.view
        view.set_view(0, 0)  # behind the stage, level: right is screen right, up is screen up
        center, _depth = view.project(0, 0, 0)
        self.assertAlmostEqual(400, center.x(), places=6)
        self.assertAlmostEqual(300, center.y(), places=6)
        right, _ = view.project(0, 1, 0)
        up, _ = view.project(0, 0, 1)
        self.assertGreater(right.x(), center.x())
        self.assertLess(up.y(), center.y())
        view.set_view(0, 89)  # top: front is screen up
        front, _ = view.project(1, 0, 0)
        self.assertLess(front.y(), view.project(0, 0, 0)[0].y())
        view.set_view(90, 0)  # side from the right: front is screen right
        self.assertGreater(view.project(1, 0, 0)[0].x(), view.project(0, 0, 0)[0].x())

    def test_points_behind_the_camera_are_not_projected(self):
        self.view.set_view(0, 0)
        self.assertIsNone(self.view.project(-50, 0, 0))

    def test_horizontal_plane_roundtrip(self):
        for name in ('Perspective', 'Top'):
            self.view.set_view(*PRESETS[name])
            for position in ((0.5, -0.75, 0.3), (-1.0, 1.25, 0.0), (1.5, 0.2, -0.4)):
                screen, _ = self.view.project(*position)
                front, right = self.view.plane_hit(screen, position[2])
                self.assertAlmostEqual(position[0], front, places=6, msg=name)
                self.assertAlmostEqual(position[1], right, places=6, msg=name)

    def test_vertical_roundtrip_and_degenerate_cases(self):
        for name in ('Perspective', 'Behind'):
            self.view.set_view(*PRESETS[name])
            for position in ((0.5, -0.75, 0.3), (-1.0, 1.25, 1.2)):
                screen, _ = self.view.project(*position)
                self.assertAlmostEqual(position[2], self.view.vertical_hit(screen, position[0], position[1]), places=6, msg=name)
        self.view.set_view(*PRESETS['Top'])
        self.assertIsNotNone(self.view.vertical_hit(QPointF(400, 300), 0, 0))  # 89 degrees is not exactly straight down
        self.view.pitch = math.pi / 2
        self.assertIsNone(self.view.vertical_hit(QPointF(400, 300), 0, 0))
        self.view.set_view(90, 0)
        self.assertIsNone(self.view.plane_hit(QPointF(400, 300), 0.0))  # level view: the floor is edge-on

    def test_orbit_zoom_and_pan_are_bounded(self):
        view = self.view
        view.orbit(0, 99)
        self.assertLessEqual(view.pitch, math.radians(89.0) + 1e-9)
        view.orbit(0, -999)
        self.assertGreaterEqual(view.pitch, -math.radians(89.0) - 1e-9)
        for _ in range(200):
            view.zoom(0.5)
        self.assertEqual(0.5, view.distance)
        for _ in range(200):
            view.zoom(2.0)
        self.assertEqual(60.0, view.distance)

    def test_hit_prefers_the_nearer_overlapping_key(self):
        view = self.view
        view.set_view(0, 0)
        document = {'tracks': [{'id': 't', 'keyframes': [{'t': 0, 'position': [2, 0, 0]}, {'t': 1, 'position': [-2, 0, 0]}, {'t': 2, 'position': [0, 1, 1]}]}]}
        view.set_document(document, True, 0, 0.0)
        # With a level view from behind, both front keys project onto the same pixel; the nearer one wins.
        screen, _ = view.project(-2, 0, 0)
        self.assertEqual(1, view.hit_keyframe(screen))
        self.assertEqual(2, view.hit_keyframe(view.project(0, 1, 1)[0]))
        self.assertIsNone(view.hit_keyframe(QPointF(5, 5)))

    def test_painting_survives_camera_inside_scene_and_malformed_documents(self):
        view = self.view
        view.set_view(10, 5)
        view.distance = 0.5
        view.target = [0.0, 1.0, 0.0]
        for document in (None, {}, {'tracks': 4}, {'tracks': [None, {'keyframes': 'x'}, {'id': 'a', 'keyframes': [{'t': 0, 'position': [0, 0, 0]}, {'t': 1, 'position': ['x']}]}],
                                    'waypoints': [None, {'position': [1, 'a', 3]}]}):
            view.set_document(document, False, 0, 0.0, 0)
            view.grab()
        view.set_document({'schema': 'org.nplaudio.spatialfabric.path', 'schema_version': 1, 'space': 'canonical',
                           'tracks': [{'id': 't', 'target': {'kind': 'object', 'key': 'k'}, 'keyframes': [
                               {'t': 0, 'position': [-3, 0, 0]}, {'t': 2, 'position': [3, 0, 2]}]}]}, True, 0, 1.0, 1)
        view.grab()


@unittest.skipIf(Viewport3D is None, 'PySide6 not installed')
class ViewportIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.window = MainWindow()
        self.window.confirm_discard = lambda: True
        self.window.resize(1400, 850)
        self.window.show()
        QApplication.processEvents()
        self.addCleanup(self.window.close)

    def test_mouse_drag_moves_a_key_in_three_dimensions_as_one_undo_step(self):
        window, view = self.window, self.window.view3d
        view.set_view(*PRESETS['Perspective'])
        original = copy.deepcopy(window.document)
        start, _ = view.project(*window.document['tracks'][0]['keyframes'][1]['position'])
        end, _ = view.project(0.5, 0.75, 0.0)
        QTest.mousePress(view, Qt.LeftButton, Qt.NoModifier, start.toPoint())
        self.assertEqual(1, window.selected_key)
        QTest.mouseMove(view, ((start + end) / 2).toPoint())
        QTest.mouseMove(view, end.toPoint())
        QTest.mouseRelease(view, Qt.LeftButton, Qt.NoModifier, end.toPoint())
        position = window.document['tracks'][0]['keyframes'][1]['position']
        self.assertAlmostEqual(0.5, position[0], delta=0.03)
        self.assertAlmostEqual(0.75, position[1], delta=0.03)
        self.assertEqual(0, position[2])  # a plain drag keeps the key's height
        self.assertEqual(1, len(window.history.undo_stack))
        window.undo()
        self.assertEqual(original, window.document)

    def test_shift_drag_changes_only_height(self):
        window, view = self.window, self.window.view3d
        view.set_view(*PRESETS['Perspective'])
        before = list(window.document['tracks'][0]['keyframes'][1]['position'])
        start, _ = view.project(*before)
        end, _ = view.project(before[0], before[1], 0.8)
        QTest.mousePress(view, Qt.LeftButton, Qt.ShiftModifier, start.toPoint())
        QTest.mouseMove(view, end.toPoint())
        QTest.mouseRelease(view, Qt.LeftButton, Qt.ShiftModifier, end.toPoint())
        after = window.document['tracks'][0]['keyframes'][1]['position']
        self.assertEqual(before[:2], after[:2])
        self.assertAlmostEqual(0.8, after[2], delta=0.03)
        self.assertEqual([], window.issues)

    def test_orbit_with_empty_space_does_not_edit(self):
        window, view = self.window, self.window.view3d
        yaw = view.yaw
        QTest.mousePress(view, Qt.LeftButton, Qt.NoModifier, QPointF(8, 8).toPoint())
        QTest.mouseMove(view, QPointF(60, 20).toPoint())
        QTest.mouseRelease(view, Qt.LeftButton, Qt.NoModifier, QPointF(60, 20).toPoint())
        self.assertNotEqual(yaw, view.yaw)
        self.assertFalse(window.dirty)
        self.assertFalse(window.history.can_undo())

    def test_selection_is_shared_between_views_and_height_edits_validate(self):
        window = self.window
        window.selected_key = 1
        window.move_keyframe3(0, 1, 0.5, 0.5, 2.5)
        self.assertEqual([0.5, 0.5, 2.5], window.document['tracks'][0]['keyframes'][1]['position'])
        self.assertEqual(1, window.view3d.selected_key)
        window.move_keyframe3(0, 1, 0.5, 0.5, 99.0)  # refused, not applied
        self.assertEqual(2.5, window.document['tracks'][0]['keyframes'][1]['position'][2])


if __name__ == '__main__':
    unittest.main()
