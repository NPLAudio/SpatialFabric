import os
from pathlib import Path
import sys
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
try:
    from PySide6.QtCore import QPointF
    from PySide6.QtWidgets import QApplication, QMessageBox
    from pathstudio.app import MainWindow
except ImportError:  # PySide6 is optional for the headless contract tests
    MainWindow = None


@unittest.skipIf(MainWindow is None, 'PySide6 not installed')
class GuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.window = MainWindow()
        self.window.confirm_discard = lambda: True  # never block on a modal prompt in tests
        self.window.resize(800, 800)
        self.addCleanup(self.window.close)
        self.window.dirty = False

    def test_starts_valid_and_offline(self):
        self.assertEqual([], self.window.issues)
        self.assertEqual(1, self.window.tracks.count())
        self.assertIn('No problems', self.window.problems.item(0).text())
        self.assertNotIn('socket', sys.modules['pathstudio.app'].__dict__)

    def test_plan_geometry_roundtrips_canonical_axes(self):
        plan = self.window.plan
        point = plan.to_screen(1.0, 0.5)  # front up, right to the right
        self.assertLess(point.y(), plan.height() / 2)
        self.assertGreater(point.x(), plan.width() / 2)
        front, right = plan.to_world(point)
        self.assertAlmostEqual(1.0, front)
        self.assertAlmostEqual(0.5, right)

    def test_dragging_keyframe_updates_document_validity_and_dirty(self):
        plan = self.window.plan
        self.assertIsNone(plan.hit_keyframe(plan.to_screen(0.5, 0.5)))
        self.assertEqual(1, plan.hit_keyframe(plan.to_screen(1.0, 0.0) + QPointF(3, 3)))  # second key is front 1, right 0
        self.assertEqual(0, plan.hit_keyframe(plan.to_screen(0.0, 0.0)))
        plan.keyframeMoved.emit(0, 1, 99.0, -99.0)
        self.assertEqual([99.0, -99.0, 0], self.window.document['tracks'][0]['keyframes'][1]['position'])
        self.assertIn('out_of_range', [code for code, _ in self.window.issues])
        self.assertTrue(self.window.dirty)
        plan.keyframeMoved.emit(0, 1, 0.5, 0.25)
        self.assertEqual([], self.window.issues)

    def test_scrubber_samples_with_model(self):
        self.window.slider.setValue(500)
        self.assertEqual('t = 2.00 s', self.window.time_label.text())
        self.window.plan.grab()  # painting a valid document must not raise

    def test_save_open_roundtrip_and_discard_guard(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'p.sfpath.json'
            self.window.move_keyframe(0, 0, 0.5, 0.5)
            self.assertTrue(self.window.save_as(path))
            self.assertFalse(self.window.dirty)
            other = MainWindow()
            other.confirm_discard = lambda: True
            self.addCleanup(other.close)
            self.assertTrue(other.open(path))
            self.assertEqual(self.window.document, other.document)

    def test_malformed_documents_never_crash_painting(self):
        for document in ({}, {'tracks': 3}, {'tracks': [None, {'keyframes': 'x'}]}, {'waypoints': 'x', 'tracks': [{'target': 7, 'keyframes': [{'position': 'q'}]}]}):
            self.window.document = document
            self.window._reload()
            self.window.plan.grab()
            self.assertTrue(self.window.issues)

    def test_invalid_file_is_reported_not_loaded(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'bad.json'
            path.write_text('[1]')
            original = self.window.document
            from unittest.mock import patch
            with patch.object(QMessageBox, 'warning') as warning:
                self.assertFalse(self.window.open(path))
            warning.assert_called_once()
            self.assertIs(original, self.window.document)

    def _targets(self):
        from pathstudio.discovery import Target
        return [Target('object', '/sim/binding/7', 'Seven', 7, (0.5, -0.25, 0.1)), Target('listener', '/sim/listener', 'Listener')]

    def test_bind_and_new_track_keep_label_and_object_id(self):
        window = self.window
        window.apply_targets(self._targets())
        self.assertEqual(2, window.target_list.count())
        window.target_list.setCurrentRow(0)
        self.assertTrue(window.bind_selected())
        self.assertEqual({'kind': 'object', 'key': '/sim/binding/7', 'label': 'Seven', 'object_id': 7}, window.document['tracks'][0]['target'])
        self.assertEqual([], window.issues)
        self.assertTrue(window.new_track_for_target())
        self.assertEqual(2, window.tracks.count())
        self.assertEqual(1, window.tracks.currentRow())
        self.assertEqual([0.5, -0.25, 0.1], window.document['tracks'][1]['keyframes'][0]['position'])
        self.assertEqual([], window.issues)
        window.target_list.setCurrentRow(1)
        self.assertTrue(window.new_track_for_target())
        self.assertEqual('track3', window.document['tracks'][2]['id'])
        self.assertEqual('listener', window.document['tracks'][2]['target']['kind'])
        self.assertTrue(window.remove_track())
        self.assertEqual(2, len(window.document['tracks']))

    def test_actions_without_selection_do_nothing(self):
        before = str(self.window.document)
        self.assertFalse(self.window.bind_selected())
        self.assertFalse(self.window.new_track_for_target())
        self.assertEqual(before, str(self.window.document))

    def test_query_failure_is_reported_and_button_reenabled(self):
        import socket
        from PySide6.QtCore import QEventLoop, QTimer
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(('127.0.0.1', 0))
            port = s.getsockname()[1]  # nothing answers here
        window = self.window
        window.port_spin.setValue(port)
        window.query_scene()
        self.assertFalse(window.query_button.isEnabled())
        loop = QEventLoop()
        window.worker.finished.connect(loop.quit)
        QTimer.singleShot(15000, loop.quit)
        loop.exec()
        window.worker.wait(2000)
        QApplication.processEvents()
        self.assertIn('Query failed', window.scene_status.text())
        self.assertTrue(window.query_button.isEnabled())


if __name__ == '__main__':
    unittest.main()
