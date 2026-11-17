"""Path Studio main window: open/save, track list, validation panel, 2D plan view and scrubber.

Offline only: this window never opens a socket, enumerates devices or starts audio.
"""
import copy
import sys
from pathlib import Path

from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtWidgets import (QApplication, QFileDialog, QHBoxLayout, QLabel, QLineEdit, QListWidget, QMainWindow,
                               QMessageBox, QPushButton, QSlider, QSpinBox, QSplitter, QVBoxLayout, QWidget)

from . import __version__, discovery, model, store
from .plan import PlanView

SLIDER_STEPS = 1000
FILTER = 'SpatialFabric paths (*.sfpath.json *.json)'


class QueryWorker(QThread):
    """One read-only scene query off the GUI thread; emits exactly one of the two outcomes."""
    succeeded = Signal(list)
    failed = Signal(str)

    def __init__(self, host, port, parent=None):
        super().__init__(parent)
        self.host, self.port = host, port

    def run(self):
        try:
            _scene, targets = discovery.fetch_targets(self.host, self.port)
        except (ValueError, OSError, RuntimeError) as error:
            self.failed.emit(str(error))
        else:
            self.succeeded.emit(targets)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.document = store.new_document()
        self.path = None
        self.dirty = False
        self.issues = []
        self.resize(1100, 720)

        self.tracks = QListWidget()
        self.tracks.currentRowChanged.connect(self._select_track)
        self.plan = PlanView()
        self.plan.keyframeMoved.connect(self.move_keyframe)
        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(0, SLIDER_STEPS)
        self.slider.valueChanged.connect(self._refresh_plan)
        self.time_label = QLabel('t = 0.00 s')
        self.problems = QListWidget()
        self.problems.setMaximumHeight(140)

        scrub = QHBoxLayout()
        scrub.addWidget(self.slider, 1)
        scrub.addWidget(self.time_label)
        center = QWidget()
        column = QVBoxLayout(center)
        column.addWidget(self.plan, 1)
        column.addLayout(scrub)
        column.addWidget(QLabel('Validation'))
        column.addWidget(self.problems)
        splitter = QSplitter()
        splitter.addWidget(self.tracks)
        splitter.addWidget(center)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)
        self.targets = []
        self.worker = None

        file_menu = self.menuBar().addMenu('&File')
        for text, handler, shortcut in (('&New', self.new, QKeySequence.New), ('&Open...', self.open, QKeySequence.Open),
                                        ('&Save', self.save, QKeySequence.Save), ('Save &As...', self.save_as, QKeySequence.SaveAs),
                                        ('&Quit', self.close, QKeySequence.Quit)):
            action = QAction(text, self)
            action.setShortcut(shortcut)
            action.triggered.connect(handler)
            file_menu.addAction(action)
        self._build_scene_panel(splitter)
        self.statusBar().showMessage('Offline editor: nothing is sent and no audio is started.')
        self._reload()

    # --- scene discovery and binding ------------------------------------------------
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
        splitter.addWidget(panel)

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

    def apply_targets(self, targets):
        self.targets = list(targets)
        self.target_list.clear()
        for target in self.targets:
            ident = f' #{target.object_id}' if target.object_id else ''
            self.target_list.addItem(f'{target.kind}: {target.label}{ident}')
        self.scene_status.setText(f'{len(self.targets)} bindable target(s). Keys are per-snapshot; the object id is kept for export.')

    def _selected_target(self):
        row = self.target_list.currentRow()
        return self.targets[row] if 0 <= row < len(self.targets) else None

    def bind_selected(self):
        target, tracks = self._selected_target(), self.document.get('tracks')
        if target is None or not isinstance(tracks, list) or not tracks or not isinstance(tracks[self.selected_row()], dict):
            return False
        tracks[self.selected_row()]['target'] = target.path_target()
        self.dirty = True
        self._reload()
        return True

    def new_track_for_target(self):
        target = self._selected_target()
        if target is None or not isinstance(self.document.get('tracks'), list):
            return False
        position = [round(v, 4) for v in target.position] if target.position else [0.0, 0.0, 0.0]
        existing = {t.get('id') for t in self.document['tracks'] if isinstance(t, dict)}
        number = 1
        while f'track{number}' in existing:
            number += 1
        self.document['tracks'].append({'id': f'track{number}', 'target': target.path_target(), 'loop': 'none', 'speed': 1,
                                        'keyframes': [{'t': 0, 'position': list(position)}, {'t': 4, 'position': list(position)}]})
        self.dirty = True
        self._reload()
        self.tracks.setCurrentRow(len(self.document['tracks']) - 1)
        return True

    def remove_track(self):
        tracks, row = self.document.get('tracks'), self.tracks.currentRow()
        if not isinstance(tracks, list) or not 0 <= row < len(tracks):
            return False
        del tracks[row]
        self.dirty = True
        self._reload()
        return True

    # --- document state ------------------------------------------------------------
    def duration(self):
        longest = 0.0
        for track in self.document.get('tracks', []) if isinstance(self.document.get('tracks'), list) else []:
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

    def _reload(self):
        row = self.selected_row()
        self.tracks.blockSignals(True)
        self.tracks.clear()
        tracks = self.document.get('tracks')
        for track in tracks if isinstance(tracks, list) else []:
            track = track if isinstance(track, dict) else {}
            target = track.get('target') if isinstance(track.get('target'), dict) else {}
            self.tracks.addItem(f"{track.get('id', '?')}  [{target.get('kind', '?')}: {target.get('key', '?')}]")
        self.tracks.setCurrentRow(min(row, self.tracks.count() - 1) if self.tracks.count() else -1)
        self.tracks.blockSignals(False)
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

    def _refresh_plan(self):
        time = self.current_time()
        self.time_label.setText(f't = {time:.2f} s')
        self.plan.set_document(self.document, not self.issues, self.selected_row(), time)

    def _select_track(self, _row):
        self._refresh_plan()

    def _title(self):
        name = Path(self.path).name if self.path else 'Untitled'
        self.setWindowTitle(f"{name}{' *' if self.dirty else ''} - Path Studio {__version__}")

    # --- editing -------------------------------------------------------------------
    def move_keyframe(self, track_index, key_index, front, right):
        keys = self.document['tracks'][track_index]['keyframes']
        up = keys[key_index]['position'][2]
        keys[key_index]['position'] = [round(front, 4), round(right, 4), up]
        self.dirty = True
        self._revalidate()

    # --- file operations -----------------------------------------------------------
    def confirm_discard(self):
        if not self.dirty:
            return True
        answer = QMessageBox.question(self, 'Unsaved changes', 'Discard unsaved changes?')
        return answer == QMessageBox.Yes

    def new(self):
        if self.confirm_discard():
            self.document, self.path, self.dirty = store.new_document(), None, False
            self.slider.setValue(0)
            self._reload()

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
        self.document, self.path, self.dirty = document, str(path), False
        self.slider.setValue(0)
        self._reload()
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
