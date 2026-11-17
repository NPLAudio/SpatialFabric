"""Path Studio main window: open/save, track list, validation panel, 2D plan view and scrubber.

Offline only: this window never opens a socket, enumerates devices or starts audio.
"""
import copy
import sys
from pathlib import Path

from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QApplication, QFileDialog, QHBoxLayout, QLabel, QListWidget, QMainWindow,
                               QMessageBox, QSlider, QSplitter, QVBoxLayout, QWidget)

from . import __version__, model, store
from .plan import PlanView

SLIDER_STEPS = 1000
FILTER = 'SpatialFabric paths (*.sfpath.json *.json)'


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

        file_menu = self.menuBar().addMenu('&File')
        for text, handler, shortcut in (('&New', self.new, QKeySequence.New), ('&Open...', self.open, QKeySequence.Open),
                                        ('&Save', self.save, QKeySequence.Save), ('Save &As...', self.save_as, QKeySequence.SaveAs),
                                        ('&Quit', self.close, QKeySequence.Quit)):
            action = QAction(text, self)
            action.setShortcut(shortcut)
            action.triggered.connect(handler)
            file_menu.addAction(action)
        self.statusBar().showMessage('Offline editor: nothing is sent and no audio is started.')
        self._reload()

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
        event.accept() if self.confirm_discard() else event.ignore()


def main(argv=None):
    app = QApplication(argv if argv is not None else sys.argv)
    window = MainWindow()
    arguments = (argv if argv is not None else sys.argv)[1:]
    if arguments:
        window.open(arguments[0])
    window.show()
    return app.exec()
