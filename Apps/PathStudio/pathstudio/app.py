"""Path Studio main window: files, tracks, keyframe inspector, shapes, four viewports (perspective, top, front,
side), timeline, undo/redo, read-only scene discovery and reference context.

Offline editor: it opens a socket only when the operator presses "Query scene" (read-only), and it never
starts audio or enumerates devices.
"""
import copy
import sys
from pathlib import Path

from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtWidgets import (QApplication, QComboBox, QDoubleSpinBox, QFileDialog, QFormLayout, QGroupBox,
                               QHBoxLayout, QLabel, QLineEdit, QListWidget, QMainWindow, QMessageBox, QPushButton,
                               QGridLayout, QSlider, QSpinBox, QSplitter, QVBoxLayout, QWidget)

from . import __version__, context, discovery, edit, model, shapes, store
from .history import History
from .timeline import TimelineView
from .viewport3d import MODES, PRESETS, Viewport3D

SLIDER_STEPS = 1000
FILTER = 'SpatialFabric paths (*.sfpath.json *.json)'


class QueryWorker(QThread):
    """One read-only scene query off the GUI thread; emits exactly one of the two outcomes."""
    succeeded = Signal(list, dict)
    failed = Signal(str)

    def __init__(self, host, port, parent=None):
        super().__init__(parent)
        self.host, self.port = host, port

    def run(self):
        try:
            scene, targets = discovery.fetch_targets(self.host, self.port)
        except (ValueError, OSError, RuntimeError) as error:
            self.failed.emit(str(error))
        else:
            self.succeeded.emit(targets, scene)


def _spin(low, high, decimals, step, value=0.0):
    box = QDoubleSpinBox()
    box.setRange(low, high)
    box.setDecimals(decimals)
    box.setSingleStep(step)
    box.setValue(value)
    return box


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.document = store.new_document()
        self.saved = copy.deepcopy(self.document)
        self.history = History()
        self.path = None
        self.dirty = False
        self.issues = []
        self.selected_key = None
        self._drag_snapshot = None
        self._drag_recorded = False
        self.targets = []
        self.worker = None
        self.resize(1360, 800)

        self.tracks = QListWidget()
        self.tracks.currentRowChanged.connect(self._select_track)
        self.views = {mode: Viewport3D(mode) for mode in MODES}
        for view in self.views.values():
            view.dragBegan.connect(self._plan_pressed)
            view.keyframeMoved3.connect(self.move_keyframe3)
            view.dragEnded.connect(self._end_drag)
        self.view3d = self.views['perspective']
        self.view_top, self.view_front, self.view_side = self.views['top'], self.views['front'], self.views['side']
        self.context_objects = []
        self.timeline = TimelineView()
        self.timeline.keySelected.connect(self._timeline_selected)
        self.timeline.dragBegan.connect(self._begin_drag)
        self.timeline.keyRetimed.connect(self.retime_keyframe)
        self.timeline.dragEnded.connect(self._end_drag)
        self.timeline.scrubbed.connect(self._scrub_to)
        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(0, SLIDER_STEPS)
        self.slider.valueChanged.connect(self._refresh_plan)
        self.time_label = QLabel('t = 0.00 s')
        self.problems = QListWidget()
        self.problems.setMaximumHeight(110)

        scrub = QHBoxLayout()
        scrub.addWidget(self.slider, 1)
        scrub.addWidget(self.time_label)
        center = QWidget()
        column = QVBoxLayout(center)
        bar = QHBoxLayout()
        bar.addWidget(QLabel('Layout'))
        self.layout_combo = QComboBox()
        self.layout_combo.addItems(['Quad', 'Perspective', 'Top', 'Front', 'Side'])
        self.layout_combo.currentTextChanged.connect(self.set_layout)
        bar.addWidget(self.layout_combo)
        bar.addWidget(QLabel('Perspective'))
        for name, (yaw, pitch) in PRESETS.items():
            button = QPushButton(name)
            button.clicked.connect(lambda _checked=False, y=yaw, pt=pitch: self.view3d.set_view(y, pt))
            bar.addWidget(button)
        bar.addStretch(1)
        bar.addWidget(QLabel('Drag keys (Shift = up/down in perspective); right/empty drag orbits or pans; wheel zooms'))
        column.addLayout(bar)
        self.view_grid = QGridLayout()
        for view, (row, col) in ((self.view3d, (0, 0)), (self.view_top, (0, 1)), (self.view_front, (1, 0)), (self.view_side, (1, 1))):
            self.view_grid.addWidget(view, row, col)
        grid_holder = QWidget()
        grid_holder.setLayout(self.view_grid)
        column.addWidget(grid_holder, 1)
        column.addWidget(self.timeline)
        column.addLayout(scrub)
        column.addWidget(QLabel('Validation'))
        column.addWidget(self.problems)

        left = QWidget()
        left_column = QVBoxLayout(left)
        left_column.addWidget(QLabel('Tracks'))
        left_column.addWidget(self.tracks, 1)
        left_column.addWidget(self._build_inspector())

        splitter = QSplitter()
        splitter.addWidget(left)
        splitter.addWidget(center)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)
        self._build_menus()
        self._build_scene_panel(splitter)
        self.statusBar().showMessage('Offline editor: nothing is sent and no audio is started.')
        self._reload()

    # --- widgets -------------------------------------------------------------------
    def _build_menus(self):
        def action(menu, text, handler, shortcut=None):
            item = QAction(text, self)
            if shortcut is not None:
                item.setShortcut(shortcut)
            item.triggered.connect(handler)
            menu.addAction(item)
            return item

        file_menu = self.menuBar().addMenu('&File')
        action(file_menu, '&New', self.new, QKeySequence.New)
        action(file_menu, '&Open...', self.open, QKeySequence.Open)
        action(file_menu, '&Save', self.save, QKeySequence.Save)
        action(file_menu, 'Save &As...', self.save_as, QKeySequence.SaveAs)
        action(file_menu, '&Quit', self.close, QKeySequence.Quit)
        edit_menu = self.menuBar().addMenu('&Edit')
        self.undo_action = action(edit_menu, '&Undo', self.undo, QKeySequence.Undo)
        self.redo_action = action(edit_menu, '&Redo', self.redo, QKeySequence.Redo)

    def _build_inspector(self):
        box = QGroupBox('Keyframe')
        form = QFormLayout(box)
        self.key_title = QLabel('No keyframe selected')
        form.addRow(self.key_title)
        self.t_spin = _spin(0, 1_000_000, 3, 0.1)
        self.front_spin, self.right_spin, self.up_spin = (_spin(-16, 16, 4, 0.05) for _ in range(3))
        self.interp_combo, self.ease_combo = QComboBox(), QComboBox()
        self.interp_combo.addItems(model.INTERPS)
        self.ease_combo.addItems(model.EASES)
        for label, widget in (('time (track s)', self.t_spin), ('front', self.front_spin), ('right', self.right_spin),
                              ('up', self.up_spin), ('interp', self.interp_combo), ('ease', self.ease_combo)):
            form.addRow(label, widget)
        self.t_spin.editingFinished.connect(self._commit_time)
        for spin in (self.front_spin, self.right_spin, self.up_spin):
            spin.editingFinished.connect(self._commit_position)
        self.interp_combo.activated.connect(lambda _i: self._commit_choice('interp', self.interp_combo))
        self.ease_combo.activated.connect(lambda _i: self._commit_choice('ease', self.ease_combo))
        buttons = QHBoxLayout()
        self.add_button = QPushButton('Add at scrub time')
        self.add_button.clicked.connect(self.add_keyframe_at_scrub)
        self.delete_button = QPushButton('Delete')
        self.delete_button.clicked.connect(self.delete_selected_keyframe)
        buttons.addWidget(self.add_button)
        buttons.addWidget(self.delete_button)
        form.addRow(buttons)
        self.shape_combo = QComboBox()
        self.shape_combo.addItems(shapes.SHAPES)
        self.size_spin = _spin(0.05, 16, 2, 0.1, 1.0)
        self.duration_spin = _spin(0.1, 3600, 2, 1.0, 8.0)
        self.points_spin = QSpinBox()
        self.points_spin.setRange(2, 200)
        self.points_spin.setValue(24)
        self.seed_spin = QSpinBox()
        self.seed_spin.setRange(0, 2_000_000_000)
        self.seed_spin.setValue(1)
        for label, widget in (('shape', self.shape_combo), ('size', self.size_spin), ('duration s', self.duration_spin),
                              ('points', self.points_spin), ('seed', self.seed_spin)):
            form.addRow(label, widget)
        self.generate_button = QPushButton('Replace track keyframes with shape')
        self.generate_button.clicked.connect(self.generate_shape)
        form.addRow(self.generate_button)
        return box

    def _build_scene_panel(self, splitter):
        panel = QWidget()
        column = QVBoxLayout(panel)
        column.addWidget(QLabel('Scene (read-only discovery)'))
        row = QHBoxLayout()
        self.host_edit = QLineEdit('127.0.0.1')
        self.port_spin = QSpinBox()
        self.port_spin.setRange(1, 65535)
        self.port_spin.setValue(8100)
        row.addWidget(self.host_edit, 1)
        row.addWidget(self.port_spin)
        column.addLayout(row)
        self.query_button = QPushButton('Query scene')
        self.query_button.clicked.connect(self.query_scene)
        column.addWidget(self.query_button)
        self.scene_status = QLabel('Not queried. Start the engine OSC input first.')
        self.scene_status.setWordWrap(True)
        column.addWidget(self.scene_status)
        self.target_list = QListWidget()
        column.addWidget(self.target_list, 1)
        for text, handler in (('Bind to selected track', self.bind_selected), ('New track for target', self.new_track_for_target),
                              ('Remove selected track', self.remove_track)):
            button = QPushButton(text)
            button.clicked.connect(handler)
            column.addWidget(button)
        column.addWidget(QLabel('Context (read-only reference)'))
        self.context_list = QListWidget()
        self.context_list.currentRowChanged.connect(lambda _row: self._refresh_context(keep_selection=True))
        column.addWidget(self.context_list, 1)
        for text, handler in (('Import context file...', self.import_context), ('Clear context', self.clear_context),
                              ('Focus selected', self.focus_context), ('Snap selected key to it', self.snap_key_to_context)):
            button = QPushButton(text)
            button.clicked.connect(handler)
            column.addWidget(button)
        splitter.addWidget(panel)

    # --- context --------------------------------------------------------------------
    def _refresh_context(self, keep_selection=False):
        row = self.context_list.currentRow() if keep_selection else -1
        self.context_list.blockSignals(True)
        self.context_list.clear()
        for obj in self.context_objects:
            self.context_list.addItem(f"{obj['kind']}: {obj['label']}" + (' (scene)' if obj['source'] == 'scene' else ''))
        if 0 <= row < self.context_list.count():
            self.context_list.setCurrentRow(row)
        self.context_list.blockSignals(False)
        selected = self.context_list.currentRow() if self.context_list.currentRow() >= 0 else None
        for view in self.views.values():
            view.set_context(self.context_objects, selected)

    def _selected_context(self):
        row = self.context_list.currentRow()
        return self.context_objects[row] if 0 <= row < len(self.context_objects) else None

    def import_context(self, path=None):
        if not isinstance(path, (str, Path)):
            path, _ = QFileDialog.getOpenFileName(self, 'Import context file', '', 'SpatialFabric context (*.sfctx.json *.json)')
        if not path:
            return False
        try:
            objects = context.load_context(path)
        except (OSError, ValueError) as error:
            QMessageBox.warning(self, 'Cannot import context', str(error))
            return False
        self.context_objects = [o for o in self.context_objects if o['source'] != 'file'] + objects
        self._refresh_context()
        self._say(f'Imported {len(objects)} context object(s) (read-only; not saved into the path).')
        return True

    def clear_context(self):
        self.context_objects = []
        self._refresh_context()

    def focus_context(self):
        obj = self._selected_context()
        if obj is None:
            return False
        for view in self.views.values():
            view.focus_on(*obj['position'])
        return True

    def snap_key_to_context(self):
        obj, row, key = self._selected_context(), self.selected_row(), self.selected_key
        if obj is None or key is None:
            self._say('Select a keyframe and a context object first.')
            return False
        return self._edit(lambda document: edit.set_position(document, row, key, obj['position']))

    def set_layout(self, name):
        visible = {'Quad': MODES, 'Perspective': ('perspective',), 'Top': ('top',), 'Front': ('front',), 'Side': ('side',)}.get(name, MODES)
        for mode, view in self.views.items():
            view.setVisible(mode in visible)

    # --- scene discovery and binding ------------------------------------------------
    def query_scene(self):
        if self.worker is not None and self.worker.isRunning():
            return
        self.query_button.setEnabled(False)
        self.scene_status.setText('Querying (read-only)...')
        self.worker = QueryWorker(self.host_edit.text().strip(), self.port_spin.value(), self)
        self.worker.succeeded.connect(self.apply_targets)
        self.worker.failed.connect(self._query_failed)
        self.worker.finished.connect(lambda: self.query_button.setEnabled(True))
        self.worker.start()

    def _query_failed(self, message):
        self.scene_status.setText('Query failed: ' + message)

    def apply_targets(self, targets, scene=None):
        self.targets = list(targets)
        self.target_list.clear()
        for target in self.targets:
            ident = f' #{target.object_id}' if target.object_id else ''
            self.target_list.addItem(f'{target.kind}: {target.label}{ident}')
        note = ''
        if isinstance(scene, dict):
            placed, unplaced = context.objects_from_scene(scene)
            self.context_objects = [o for o in self.context_objects if o['source'] != 'scene'] + placed
            self._refresh_context()
            note = f' {len(placed)} placed in the views' + (f'; {unplaced} engine-world record(s) not placed (no exact stage position).' if unplaced else '.')
        self.scene_status.setText(f'{len(self.targets)} bindable target(s). Keys are per-snapshot; the object id is kept for export.' + note)

    def _selected_target(self):
        row = self.target_list.currentRow()
        return self.targets[row] if 0 <= row < len(self.targets) else None

    def bind_selected(self):
        target, tracks, row = self._selected_target(), self.document.get('tracks'), self.selected_row()
        if target is None or not isinstance(tracks, list) or not tracks or not isinstance(tracks[row], dict):
            return False
        return self._edit(lambda doc: doc['tracks'][row].__setitem__('target', target.path_target()))

    def new_track_for_target(self):
        target = self._selected_target()
        if target is None or not isinstance(self.document.get('tracks'), list):
            return False
        position = [round(v, 4) for v in target.position] if target.position else [0.0, 0.0, 0.0]
        existing = {t.get('id') for t in self.document['tracks'] if isinstance(t, dict)}
        number = 1
        while f'track{number}' in existing:
            number += 1

        def add(doc):
            doc['tracks'].append({'id': f'track{number}', 'target': target.path_target(), 'loop': 'none', 'speed': 1,
                                  'keyframes': [{'t': 0, 'position': list(position)}, {'t': 4, 'position': list(position)}]})

        if not self._edit(add):
            return False
        self.tracks.setCurrentRow(len(self.document['tracks']) - 1)
        return True

    def remove_track(self):
        tracks, row = self.document.get('tracks'), self.tracks.currentRow()
        if not isinstance(tracks, list) or not 0 <= row < len(tracks):
            return False
        return self._edit(lambda doc: doc['tracks'].pop(row))

    # --- document state ------------------------------------------------------------
    def _tracks_list(self):
        tracks = self.document.get('tracks') if isinstance(self.document, dict) else None
        return tracks if isinstance(tracks, list) else []

    def duration(self):
        longest = 0.0
        for track in self._tracks_list():
            keys = track.get('keyframes') if isinstance(track, dict) else None
            if isinstance(keys, list) and keys and isinstance(keys[-1], dict) and isinstance(keys[-1].get('t'), (int, float)):
                speed = track.get('speed', 1)
                speed = speed if isinstance(speed, (int, float)) and speed > 0 else 1
                longest = max(longest, keys[-1]['t'] / speed)
        return longest

    def current_time(self):
        return self.duration() * self.slider.value() / SLIDER_STEPS

    def selected_row(self):
        return max(0, self.tracks.currentRow())

    def _selected_track(self):
        tracks, row = self._tracks_list(), self.selected_row()
        return tracks[row] if row < len(tracks) and isinstance(tracks[row], dict) else None

    def _keys(self):
        track = self._selected_track()
        keys = track.get('keyframes') if track else None
        return keys if isinstance(keys, list) else []

    def _reload(self):
        row = self.selected_row()
        self.tracks.blockSignals(True)
        self.tracks.clear()
        for track in self._tracks_list():
            track = track if isinstance(track, dict) else {}
            target = track.get('target') if isinstance(track.get('target'), dict) else {}
            self.tracks.addItem(f"{track.get('id', '?')}  [{target.get('kind', '?')}: {target.get('key', '?')}]")
        self.tracks.setCurrentRow(min(row, self.tracks.count() - 1) if self.tracks.count() else -1)
        self.tracks.blockSignals(False)
        if self.selected_key is not None and not 0 <= self.selected_key < len(self._keys()):
            self.selected_key = None
        self._revalidate()

    def _revalidate(self):
        self.issues = model.validate(self.document)
        self.problems.clear()
        if not self.issues:
            self.problems.addItem('No problems: document is valid.')
        for code, where in self.issues:
            self.problems.addItem(f'{code}  {where}')
        self._refresh_plan()
        self._title()

    def _refresh_plan(self, *_ignored):
        time = self.current_time()
        self.time_label.setText(f't = {time:.2f} s')
        for view in self.views.values():
            view.set_document(self.document, not self.issues, self.selected_row(), time, self.selected_key)
        self.timeline.set_document(self.document, self.duration(), time, self.selected_row(), self.selected_key)
        self._sync_inspector()
        self.undo_action.setEnabled(self.history.can_undo())
        self.redo_action.setEnabled(self.history.can_redo())

    def _select_track(self, _row):
        self.selected_key = None
        self._refresh_plan()

    def _title(self):
        name = Path(self.path).name if self.path else 'Untitled'
        self.setWindowTitle(f"{name}{' *' if self.dirty else ''} - Path Studio {__version__}")

    def _changed(self):
        self.dirty = self.document != self.saved
        self._reload()

    def _say(self, message):
        self.statusBar().showMessage(message, 6000)

    # --- editing core --------------------------------------------------------------
    def _edit(self, function, *arguments):
        """Run one undoable mutation. On failure the document and history are left untouched."""
        snapshot = copy.deepcopy(self.document)
        try:
            function(self.document, *arguments)
        except (ValueError, KeyError, IndexError, TypeError) as error:
            self.document = snapshot
            self._say(f'Edit refused: {error}')
            return False
        self.history.record(snapshot)
        self._changed()
        return True

    def _begin_drag(self):
        self._drag_snapshot = copy.deepcopy(self.document)
        self._drag_recorded = False

    def _end_drag(self):
        self._drag_snapshot, self._drag_recorded = None, False

    def _drag_edit(self, function, *arguments):
        """Mutations repeated during a drag collapse into one undo step."""
        snapshot = self._drag_snapshot if self._drag_snapshot is not None else copy.deepcopy(self.document)
        trial = copy.deepcopy(self.document)
        try:
            function(trial, *arguments)
        except (ValueError, KeyError, IndexError, TypeError) as error:
            self._say(f'Edit refused: {error}')
            return False
        if self._drag_snapshot is None or not self._drag_recorded:
            self.history.record(snapshot)
            self._drag_recorded = self._drag_snapshot is not None
        self.document = trial
        self._changed()
        return True

    def undo(self):
        previous = self.history.undo(self.document)
        if previous is not None:
            self.document = previous
            self._changed()
            self._say('Undone')

    def redo(self):
        following = self.history.redo(self.document)
        if following is not None:
            self.document = following
            self._changed()
            self._say('Redone')

    # --- keyframe editing ----------------------------------------------------------
    def _plan_pressed(self, key_index):
        self.selected_key = key_index
        self._begin_drag()
        self._refresh_plan()

    def _timeline_selected(self, track_index, key_index):
        self.tracks.blockSignals(True)
        self.tracks.setCurrentRow(track_index)
        self.tracks.blockSignals(False)
        self.selected_key = key_index
        self._refresh_plan()

    def _scrub_to(self, time):
        duration = self.duration()
        self.slider.setValue(round(SLIDER_STEPS * time / duration) if duration > 0 else 0)

    def move_keyframe(self, track_index, key_index, front, right):
        def move(document):
            up = document['tracks'][track_index]['keyframes'][key_index]['position'][2]
            edit.set_position(document, track_index, key_index, [round(front, 4), round(right, 4), up])

        self._drag_edit(move)

    def move_keyframe3(self, track_index, key_index, front, right, up):
        self._drag_edit(lambda document: edit.set_position(
            document, track_index, key_index, [round(front, 4), round(right, 4), round(up, 4)]))

    def retime_keyframe(self, track_index, key_index, time):
        self._drag_edit(lambda document: edit.set_time(document, track_index, key_index, time))

    def add_keyframe_at_scrub(self):
        track = self._selected_track()
        if track is None:
            return False
        speed = track.get('speed', 1) if isinstance(track.get('speed', 1), (int, float)) else 1
        row, created = self.selected_row(), []
        ok = self._edit(lambda document: created.append(edit.add_keyframe(document, row, self.current_time() * speed)))
        if ok:
            self.selected_key = created[0]
            self._refresh_plan()
        return ok

    def delete_selected_keyframe(self):
        if self.selected_key is None:
            return False
        row, key = self.selected_row(), self.selected_key
        ok = self._edit(lambda document: edit.delete_keyframe(document, row, key))
        if ok:
            self.selected_key = min(key, len(self._keys()) - 1)
            self._refresh_plan()
        return ok

    def generate_shape(self):
        track = self._selected_track()
        if track is None:
            return False
        keys = self._keys()
        first = keys[0].get('position') if keys and isinstance(keys[0], dict) else None
        center = tuple(first) if isinstance(first, list) and len(first) == 3 else (0.0, 0.0, 0.0)
        row = self.selected_row()

        def apply(document):
            generated = shapes.generate(self.shape_combo.currentText(), center, self.size_spin.value(),
                                        self.duration_spin.value(), self.points_spin.value(), self.seed_spin.value())
            edit.replace_keyframes(document, row, generated)

        ok = self._edit(apply)
        if ok:
            self.selected_key = None
            self._refresh_plan()
        return ok

    def _sync_inspector(self):
        keys = self._keys()
        key = keys[self.selected_key] if self.selected_key is not None and self.selected_key < len(keys) else None
        key = key if isinstance(key, dict) else None
        widgets = (self.t_spin, self.front_spin, self.right_spin, self.up_spin, self.interp_combo, self.ease_combo)
        for widget in widgets:
            widget.blockSignals(True)
        try:
            if key is None:
                self.key_title.setText('No keyframe selected')
            else:
                position = key.get('position')
                linked = key.get('waypoint')
                self.key_title.setText(f'Keyframe {self.selected_key + 1} of {len(keys)}' + (f' (waypoint {linked})' if linked else ''))
                if isinstance(key.get('t'), (int, float)):
                    self.t_spin.setValue(min(key['t'], self.t_spin.maximum()))
                if isinstance(position, list) and len(position) == 3 and all(isinstance(v, (int, float)) for v in position):
                    for spin, value in zip((self.front_spin, self.right_spin, self.up_spin), position):
                        spin.setValue(max(-16, min(16, value)))
                self.interp_combo.setCurrentText(str(key.get('interp', 'linear')))
                self.ease_combo.setCurrentText(str(key.get('ease', 'linear')))
        finally:
            for widget in widgets:
                widget.blockSignals(False)
        editable = key is not None and not self.issues
        for widget in widgets:
            widget.setEnabled(key is not None)
        self.delete_button.setEnabled(key is not None)
        self.add_button.setEnabled(self._selected_track() is not None and not self.issues)
        self.generate_button.setEnabled(self._selected_track() is not None)
        return editable

    def _commit_time(self):
        if self.selected_key is not None:
            row, key = self.selected_row(), self.selected_key
            if abs(self.t_spin.value() - self._keys()[key].get('t', 0)) > 1e-9:
                self._edit(lambda document: edit.set_time(document, row, key, self.t_spin.value()))
                self._refresh_plan()

    def _commit_position(self):
        if self.selected_key is not None:
            row, key = self.selected_row(), self.selected_key
            position = [self.front_spin.value(), self.right_spin.value(), self.up_spin.value()]
            current = self._keys()[key].get('position')
            if current != position:
                self._edit(lambda document: edit.set_position(document, row, key, position))
                self._refresh_plan()

    def _commit_choice(self, field, combo):
        if self.selected_key is not None:
            row, key = self.selected_row(), self.selected_key
            if self._keys()[key].get(field, 'linear') != combo.currentText():
                self._edit(lambda document: edit.set_choice(document, row, key, field, combo.currentText()))

    # --- file operations -----------------------------------------------------------
    def confirm_discard(self):
        if not self.dirty:
            return True
        answer = QMessageBox.question(self, 'Unsaved changes', 'Discard unsaved changes?')
        return answer == QMessageBox.Yes

    def _adopt(self, document, path):
        self.document, self.path, self.dirty = document, path, False
        self.saved = copy.deepcopy(document)
        self.history.clear()
        self.selected_key = None
        self.slider.setValue(0)
        self._reload()

    def new(self):
        if self.confirm_discard():
            self._adopt(store.new_document(), None)

    def open(self, path=None):
        if not self.confirm_discard():
            return False
        if not isinstance(path, (str, Path)):
            path, _ = QFileDialog.getOpenFileName(self, 'Open path document', '', FILTER)
        if not path:
            return False
        try:
            document = store.load_document(path)
        except (OSError, ValueError) as error:
            QMessageBox.warning(self, 'Cannot open', str(error))
            return False
        self._adopt(document, str(path))
        return True

    def save(self):
        if not self.path:
            return self.save_as()
        return self._write(self.path)

    def save_as(self, path=None):
        if not isinstance(path, (str, Path)):
            path, _ = QFileDialog.getSaveFileName(self, 'Save path document', self.path or 'untitled.sfpath.json', FILTER)
        return bool(path) and self._write(path)

    def _write(self, path):
        if self.issues and QMessageBox.question(self, 'Invalid document', 'The document has validation problems. Save anyway?') != QMessageBox.Yes:
            return False
        try:
            store.save_document(path, copy.deepcopy(self.document))
        except (OSError, ValueError) as error:
            QMessageBox.warning(self, 'Cannot save', str(error))
            return False
        self.path, self.dirty = str(path), False
        self.saved = copy.deepcopy(self.document)
        self._title()
        return True

    def closeEvent(self, event):
        if not self.confirm_discard():
            event.ignore()
            return
        if self.worker is not None:
            self.worker.wait(8000)  # a query is bounded by its own timeouts
        event.accept()


def main(argv=None):
    app = QApplication(argv if argv is not None else sys.argv)
    window = MainWindow()
    arguments = (argv if argv is not None else sys.argv)[1:]
    if arguments:
        window.open(arguments[0])
    window.show()
    return app.exec()
