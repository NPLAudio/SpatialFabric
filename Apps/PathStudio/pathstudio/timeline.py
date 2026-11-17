"""Timeline: one lane per track, keyframes at effective time (key t / speed), playhead and scrubbing."""
import math

from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QWidget

LEFT, RIGHT, RULER, LANE = 90, 12, 22, 30
HIT = 8.0


class TimelineView(QWidget):
    keySelected = Signal(int, int)        # track index, key index
    dragBegan = Signal()
    keyRetimed = Signal(int, int, float)  # track index, key index, requested track time
    dragEnded = Signal()
    scrubbed = Signal(float)              # effective time in seconds

    def __init__(self, parent=None):
        super().__init__(parent)
        self.document = None
        self.duration = 1.0
        self.time = 0.0
        self.selected = (-1, -1)
        self._mode = None
        self._drag = None
        self.setMinimumHeight(RULER + LANE * 2 + 8)

    def set_document(self, document, duration, time, selected_track, selected_key):
        self.document = document
        self.duration = max(float(duration), 1.0)
        self.time = time
        self.selected = (selected_track, selected_key)
        tracks = self._tracks()
        self.setMinimumHeight(RULER + LANE * max(len(tracks), 1) + 8)
        self.update()

    # --- geometry (unit-tested) ----------------------------------------------------
    def _tracks(self):
        tracks = self.document.get('tracks') if isinstance(self.document, dict) else None
        return tracks if isinstance(tracks, list) else []

    def time_to_x(self, time):
        return LEFT + (self.width() - LEFT - RIGHT) * time / self.duration

    def x_to_time(self, x):
        width = max(1.0, self.width() - LEFT - RIGHT)
        return max(0.0, min(self.duration, (x - LEFT) / width * self.duration))

    def lane_y(self, index):
        return RULER + index * LANE + LANE / 2

    def key_points(self, index):
        """[(key index, effective time)] for numeric keyframes of one track; malformed entries are skipped."""
        track = self._tracks()[index]
        keys = track.get('keyframes') if isinstance(track, dict) else None
        speed = track.get('speed', 1) if isinstance(track, dict) else 1
        speed = speed if isinstance(speed, (int, float)) and not isinstance(speed, bool) and speed > 0 else 1
        points = []
        for ki, key in enumerate(keys if isinstance(keys, list) else []):
            t = key.get('t') if isinstance(key, dict) else None
            if isinstance(t, (int, float)) and not isinstance(t, bool) and math.isfinite(t):
                points.append((ki, t / speed))
        return points, speed

    def hit(self, point):
        best, best_distance = None, HIT
        for ti in range(len(self._tracks())):
            points, _speed = self.key_points(ti)
            for ki, effective in points:
                distance = math.hypot(self.time_to_x(effective) - point.x(), self.lane_y(ti) - point.y())
                if distance <= best_distance:
                    best, best_distance = (ti, ki), distance
        return best

    # --- painting ------------------------------------------------------------------
    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor(30, 34, 40))
        painter.setPen(QColor(150, 160, 170))
        step = max(1, int(math.ceil(self.duration / 10)))
        for second in range(0, int(self.duration) + 1, step):
            x = self.time_to_x(second)
            painter.drawLine(QPointF(x, RULER - 6), QPointF(x, RULER))
            painter.drawText(QPointF(x + 3, RULER - 8), f'{second}s')
        for ti, track in enumerate(self._tracks()):
            y = self.lane_y(ti)
            painter.setPen(QPen(QColor(55, 62, 72), 1))
            painter.drawLine(QPointF(LEFT, y), QPointF(self.width() - RIGHT, y))
            painter.setPen(QColor(190, 200, 210))
            name = str(track.get('id', '?')) if isinstance(track, dict) else '?'
            painter.drawText(QRectF(6, y - 10, LEFT - 10, 20), Qt.AlignVCenter | Qt.AlignLeft, name)
            points, _speed = self.key_points(ti)
            for ki, effective in points:
                chosen = (ti, ki) == self.selected
                painter.setPen(QPen(QColor(255, 255, 255) if chosen else QColor(90, 220, 150), 2 if chosen else 1))
                painter.setBrush(QColor(255, 190, 70) if chosen else QColor(60, 160, 110))
                x = self.time_to_x(effective)
                painter.drawRect(QRectF(x - 4, y - 4, 8, 8))
        painter.setPen(QPen(QColor(255, 90, 90), 2))
        x = self.time_to_x(min(self.time, self.duration))
        painter.drawLine(QPointF(x, 0), QPointF(x, self.height()))
        painter.end()

    # --- interaction ---------------------------------------------------------------
    def mousePressEvent(self, event):
        if event.button() != Qt.LeftButton:
            return
        found = self.hit(event.position())
        if found is not None:
            self._mode, self._drag = 'key', found
            self.keySelected.emit(*found)
            self.dragBegan.emit()
        elif event.position().x() >= LEFT:
            self._mode = 'scrub'
            self.scrubbed.emit(self.x_to_time(event.position().x()))

    def mouseMoveEvent(self, event):
        if self._mode == 'key' and self._drag is not None:
            ti, ki = self._drag
            speed = self.key_points(ti)[1]
            self.keyRetimed.emit(ti, ki, self.x_to_time(event.position().x()) * speed)
        elif self._mode == 'scrub':
            self.scrubbed.emit(self.x_to_time(event.position().x()))

    def mouseReleaseEvent(self, _event):
        if self._mode == 'key':
            self.dragEnded.emit()
        self._mode, self._drag = None, None
