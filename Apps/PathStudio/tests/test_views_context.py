import copy
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
EXAMPLE = APP / 'examples/room.sfctx.json'
try:
    from PySide6.QtCore import Qt
    from PySide6.QtTest import QTest
    from PySide6.QtWidgets import QApplication, QMessageBox
    from pathstudio.app import MainWindow
    from pathstudio.discovery import Target
    from pathstudio.viewport3d import Viewport3D
except ImportError:  # PySide6 is optional for the headless contract tests
    Viewport3D = None


@unittest.skipIf(Viewport3D is None, 'PySide6 not installed')
class OrthographicViewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def make(self, mode):
        view = Viewport3D(mode)
        view.resize(600, 600)
        return view

    def test_each_orthographic_view_maps_axes_as_documented(self):
        top, front, side = self.make('top'), self.make('front'), self.make('side')
        origin = top.project(0, 0, 0)[0]
        self.assertAlmostEqual(300, origin.x())
        self.assertAlmostEqual(300, origin.y())
        self.assertLess(top.project(1, 0, 0)[0].y(), origin.y())      # top: front is up on screen
        self.assertGreater(top.project(0, 1, 0)[0].x(), origin.x())   # top: right is right
        self.assertLess(front.project(0, 0, 1)[0].y(), origin.y())    # front: up is up
        self.assertGreater(front.project(0, 1, 0)[0].x(), origin.x())
        self.assertGreater(side.project(1, 0, 0)[0].x(), origin.x())  # side: front is to the right
        self.assertLess(side.project(0, 0, 1)[0].y(), origin.y())
        self.assertEqual(top.project(1, 0, 0)[0], top.project(1, 0, 5)[0])  # height is dropped in the top view

    def test_ortho_hit_inverts_project_and_zoom_is_bounded(self):
        axes = {'top': (1, 0), 'front': (1, 2), 'side': (0, 2)}
        for mode, (h, v) in axes.items():
            view = self.make(mode)
            view.zoom(0.5)
            view.focus_on(0.3, -0.2, 0.4)
            position = (0.7, 0.6, -0.1)
            horizontal, vertical = view.ortho_hit(view.project(*position)[0])
            self.assertAlmostEqual(position[h], horizontal, places=6)
            self.assertAlmostEqual(position[v], vertical, places=6)
            for _ in range(500):
                view.zoom(0.5)
            self.assertEqual(200.0, view.zoom_level)
            for _ in range(500):
                view.zoom(2.0)
            self.assertEqual(0.05, view.zoom_level)
            yaw = view.yaw
            view.orbit(1.0, 1.0)  # orthographic views do not orbit
            self.assertEqual(yaw, view.yaw)

    def test_unknown_mode_is_rejected_and_every_object_kind_paints_in_every_view(self):
        with self.assertRaises(ValueError):
            Viewport3D('isometric')
        objects = [{'id': 'b', 'kind': 'box', 'label': 'B', 'position': [0, 0, 0.5], 'half_extents': [0.5, 0.5, 0.5], 'source': 'file'},
                   {'id': 's', 'kind': 'sphere', 'label': 'S', 'position': [0.2, 0, 0], 'radius': 0.3, 'source': 'file'},
                   {'id': 'p', 'kind': 'point', 'label': 'P', 'position': [0, 1, 0], 'source': 'scene'},
                   {'id': 'l', 'kind': 'listener', 'label': 'L', 'position': [0, 0, 0.1], 'yaw_degrees': 45.0, 'source': 'file'}]
        for mode in ('perspective', 'top', 'front', 'side'):
            view = self.make(mode)
            view.set_context(objects, 0)
            view.grab()


@unittest.skipIf(Viewport3D is None, 'PySide6 not installed')
class ContextIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.window = MainWindow()
        self.window.confirm_discard = lambda: True
        self.window.resize(1500, 900)
        self.window.show()
        QApplication.processEvents()
        self.addCleanup(self.window.close)

    def test_ortho_mouse_drag_moves_in_the_view_plane_and_keeps_the_third_axis(self):
        window = self.window
        window.move_keyframe3(0, 1, 1.0, 0.0, 0.5)
        window.history.clear()
        view = window.view_front  # horizontal = right, vertical = up; front must stay 1.0
        start = view.project(1.0, 0.0, 0.5)[0]
        end = view.project(1.0, 0.6, 0.9)[0]
        QTest.mousePress(view, Qt.LeftButton, Qt.NoModifier, start.toPoint())
        QTest.mouseMove(view, end.toPoint())
        QTest.mouseRelease(view, Qt.LeftButton, Qt.NoModifier, end.toPoint())
        position = window.document['tracks'][0]['keyframes'][1]['position']
        self.assertEqual(1.0, position[0])
        self.assertAlmostEqual(0.6, position[1], delta=0.03)
        self.assertAlmostEqual(0.9, position[2], delta=0.03)
        self.assertEqual(1, len(window.history.undo_stack))

    def test_import_context_marks_objects_in_every_view_without_touching_the_document(self):
        window = self.window
        before = copy.deepcopy(window.document)
        self.assertTrue(window.import_context(EXAMPLE))
        self.assertEqual(8, window.context_list.count())
        for view in window.views.values():
            self.assertEqual(8, len(view.context))
            view.grab()
        self.assertEqual(before, window.document)
        self.assertFalse(window.dirty)
        window.clear_context()
        self.assertEqual(0, window.context_list.count())
        self.assertEqual([], window.view_top.context)

    def test_bad_context_file_is_reported_and_keeps_existing_context(self):
        window = self.window
        window.import_context(EXAMPLE)
        with tempfile.TemporaryDirectory() as temp:
            bad = Path(temp) / 'bad.json'
            bad.write_text('{"schema": "x"}')
            with patch.object(QMessageBox, 'warning') as warning:
                self.assertFalse(window.import_context(bad))
            warning.assert_called_once()
        self.assertEqual(8, len(window.context_objects))

    def test_scene_objects_replace_only_previous_scene_context(self):
        window = self.window
        window.import_context(EXAMPLE)
        scene = {'objects': [{'kind': 'spatial_binding', 'key': '/sim/binding/1', 'label': 'One', 'stage_normalized': [0.5, 0.0, 0.2]},
                             {'kind': 'listener', 'key': 'L', 'world_transform': {'position_cm': [0, 0, 0]}}]}
        window.apply_targets([Target('object', '/sim/binding/1', 'One', 1, (0.5, 0.0, 0.2))], scene)
        self.assertEqual(9, len(window.context_objects))
        self.assertIn('1 placed', window.scene_status.text())
        self.assertIn('not placed', window.scene_status.text())
        window.apply_targets([], {'objects': []})  # a fresh query without bindings drops stale scene markers
        self.assertEqual(8, len(window.context_objects))

    def test_focus_and_snap_use_the_selected_context_object(self):
        window = self.window
        window.import_context(EXAMPLE)
        window.context_list.setCurrentRow(5)  # Speaker FL at front 1.0, right -1.0, up 0.8
        self.assertEqual('Speaker FL', window._selected_context()['label'])
        self.assertTrue(window.focus_context())
        self.assertEqual([-1.0, 1.0], window.view_top.center)  # [right, front]
        window.selected_key = None
        self.assertFalse(window.snap_key_to_context())  # no keyframe selected
        window.selected_key = 1
        self.assertTrue(window.snap_key_to_context())
        self.assertEqual([1.0, -1.0, 0.8], window.document['tracks'][0]['keyframes'][1]['position'])
        window.undo()
        self.assertEqual([1, 0, 0], window.document['tracks'][0]['keyframes'][1]['position'])

    def test_layout_switch_shows_one_or_all_views(self):
        window = self.window
        window.set_layout('Top')
        self.assertTrue(window.view_top.isVisible())
        self.assertFalse(window.view3d.isVisible())
        window.set_layout('Quad')
        self.assertTrue(all(view.isVisible() for view in window.views.values()))
        window.set_layout('nonsense')  # unknown names fall back to showing everything
        self.assertTrue(all(view.isVisible() for view in window.views.values()))


if __name__ == '__main__':
    unittest.main()
