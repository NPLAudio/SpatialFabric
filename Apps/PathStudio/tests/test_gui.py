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
        plan.keyframeMoved.emit(0, 1, 99.0, -99.0)  # the view clamps; an out-of-range request is refused, not applied
        self.assertEqual([1, 0, 0], self.window.document['tracks'][0]['keyframes'][1]['position'])
        self.assertFalse(self.window.dirty)
        self.assertFalse(self.window.history.can_undo())
        plan.keyframeMoved.emit(0, 1, 0.5, 0.25)
        self.assertEqual([0.5, 0.25, 0], self.window.document['tracks'][0]['keyframes'][1]['position'])
        self.assertEqual([], self.window.issues)
        self.assertTrue(self.window.dirty)

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

    def test_undo_redo_and_dirty_follow_the_saved_state(self):
        window = self.window
        original = copy_of(window.document)
        window.move_keyframe(0, 1, 0.5, 0.5)
        self.assertTrue(window.dirty)
        self.assertTrue(window.undo_action.isEnabled())
        window.undo()
        self.assertEqual(original, window.document)
        self.assertFalse(window.dirty)  # back at the saved state
        self.assertTrue(window.redo_action.isEnabled())
        window.redo()
        self.assertEqual([0.5, 0.5, 0], window.document['tracks'][0]['keyframes'][1]['position'])
        self.assertTrue(window.dirty)

    def test_a_whole_drag_is_one_undo_step(self):
        window = self.window
        original = copy_of(window.document)
        window.plan.dragBegan.emit(1)
        for step in range(1, 6):
            window.plan.keyframeMoved.emit(0, 1, 0.1 * step, 0.2 * step)
        window.plan.dragEnded.emit()
        self.assertEqual(1, len(window.history.undo_stack))
        window.undo()
        self.assertEqual(original, window.document)
        self.assertFalse(window.history.can_undo())

    def test_add_select_inspect_edit_and_delete_keyframe(self):
        window = self.window
        window.slider.setValue(500)  # t = 2.0 s on a 4 s track
        self.assertTrue(window.add_keyframe_at_scrub())
        self.assertEqual(1, window.selected_key)
        self.assertEqual(3, len(window.document['tracks'][0]['keyframes']))
        self.assertEqual(2.0, window.document['tracks'][0]['keyframes'][1]['t'])
        self.assertEqual('Keyframe 2 of 3', window.key_title.text())
        window.t_spin.setValue(3.0)
        window.t_spin.editingFinished.emit()
        self.assertEqual(3.0, window.document['tracks'][0]['keyframes'][1]['t'])
        window.front_spin.setValue(0.25)
        window.front_spin.editingFinished.emit()
        self.assertEqual(0.25, window.document['tracks'][0]['keyframes'][1]['position'][0])
        window.interp_combo.setCurrentText('catmull_rom')
        window.interp_combo.activated.emit(window.interp_combo.currentIndex())
        self.assertEqual('catmull_rom', window.document['tracks'][0]['keyframes'][1]['interp'])
        self.assertEqual([], window.issues)
        self.assertTrue(window.delete_selected_keyframe())
        self.assertEqual(2, len(window.document['tracks'][0]['keyframes']))
        for _ in range(5):  # add, time, front, interp, delete
            window.undo()
        self.assertEqual(2, len(window.document['tracks'][0]['keyframes']))
        self.assertFalse(window.dirty)

    def test_refused_edits_do_not_touch_document_or_history(self):
        window = self.window
        window.selected_key = 0
        window.slider.setValue(0)  # time 0 already has a keyframe
        before = copy_of(window.document)
        self.assertFalse(window.add_keyframe_at_scrub())
        self.assertEqual(before, window.document)
        self.assertFalse(window.history.can_undo())
        window.selected_key = None
        self.assertFalse(window.delete_selected_keyframe())
        window.t_spin.editingFinished.emit()  # no selection: nothing happens
        self.assertFalse(window.dirty)

    def test_generate_shape_replaces_keyframes_and_is_undoable(self):
        window = self.window
        window.shape_combo.setCurrentText('circle')
        window.size_spin.setValue(0.8)
        window.points_spin.setValue(12)
        self.assertTrue(window.generate_shape())
        self.assertEqual(12, len(window.document['tracks'][0]['keyframes']))
        self.assertEqual([], window.issues)
        window.undo()
        self.assertEqual(2, len(window.document['tracks'][0]['keyframes']))

    def test_timeline_geometry_scrub_and_retime(self):
        window = self.window
        window.resize(1200, 800)
        window.show()
        QApplication.processEvents()
        timeline = window.timeline
        self.assertAlmostEqual(2.0, timeline.x_to_time(timeline.time_to_x(2.0)), places=6)
        self.assertEqual((0, 1), timeline.hit(QPointF(timeline.time_to_x(4.0), timeline.lane_y(0))))
        self.assertIsNone(timeline.hit(QPointF(timeline.time_to_x(2.0), timeline.lane_y(0))))
        timeline.scrubbed.emit(2.0)
        self.assertEqual('t = 2.00 s', window.time_label.text())
        timeline.keySelected.emit(0, 1)
        self.assertEqual(1, window.selected_key)
        timeline.dragBegan.emit()
        timeline.keyRetimed.emit(0, 1, 2.5)
        timeline.keyRetimed.emit(0, 1, 3.0)
        timeline.dragEnded.emit()
        self.assertEqual(3.0, window.document['tracks'][0]['keyframes'][1]['t'])
        self.assertEqual(1, len(window.history.undo_stack))
        timeline.keyRetimed.emit(0, 1, -50)  # clamped just after the first key, never reordering keys
        self.assertEqual(0.001, window.document['tracks'][0]['keyframes'][1]['t'])
        self.assertEqual([], window.issues)
        window.grab()

    def test_malformed_documents_never_crash_timeline_or_inspector(self):
        for document in ({}, {'tracks': [None, {'keyframes': [None, {'t': 'x'}, {'t': float('nan')}], 'speed': 'fast'}]}):
            self.window.document = document
            self.window.selected_key = 0
            self.window._reload()
            self.window.timeline.grab()
            self.window.add_keyframe_at_scrub()
            self.window.generate_shape()

    def test_new_and_open_reset_history(self):
        window = self.window
        window.move_keyframe(0, 1, 0.5, 0.5)
        window.new()
        self.assertFalse(window.history.can_undo())
        self.assertFalse(window.dirty)
        self.assertIsNone(window.selected_key)


def copy_of(value):
    import copy
    return copy.deepcopy(value)


if __name__ == '__main__':
    unittest.main()
