"""2D plan (top-down) view: horizontal = right, vertical = front, in canonical units."""
import math

from PySide6.QtCore import QPointF, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QWidget

from . import model

EXTENT = 2.0  # visible half-extent; the stage itself is +/-1
HIT_RADIUS = 10.0
CURVE_STEPS = 240


class PlanView(QWidget):
    keyframeMoved = Signal(int, int, float, float)  # track index, key index, front, right

    def __init__(self, parent=None):
        super().__init__(parent)
        self.document = None
        self.valid = False
        self.selected = 0
        self.time = 0.0
        self._drag = None
        self.setMinimumSize(360, 360)
        self.setMouseTracking(False)

    def set_document(self, document, valid, selected, time):
        self.document, self.valid, self.selected, self.time = document, valid, selected, time
        self.update()

    # --- pure geometry (unit-tested) -------------------------------------------------
    def scale(self):
        return min(self.width(), self.height()) / (2 * EXTENT)

    def to_screen(self, front, right):
        s = self.scale()
        return QPointF(self.width() / 2 + right * s, self.height() / 2 - front * s)

    def to_world(self, point):
        s = self.scale()
        return (self.height() / 2 - point.y()) / s, (point.x() - self.width() / 2) / s

    def hit_keyframe(self, point):
        """Index of the nearest draggable (explicit-position) keyframe in the selected track."""
        track = self._track()
        if track is None:
            return None
        best, best_distance = None, HIT_RADIUS
        for index, key in enumerate(track.get('keyframes', [])):
            position = key.get('position') if isinstance(key, dict) else None
            if not isinstance(position, list) or len(position) != 3:
                continue
            screen = self.to_screen(position[0], position[1])
            distance = math.hypot(screen.x() - point.x(), screen.y() - point.y())
            if distance <= best_distance:
                best, best_distance = index, distance
        return best

    def _track(self):
        tracks = self.document.get('tracks') if isinstance(self.document, dict) else None
        if isinstance(tracks, list) and 0 <= self.selected < len(tracks) and isinstance(tracks[self.selected], dict):
            return tracks[self.selected]
        return None

    # --- painting ------------------------------------------------------------------
    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor(24, 28, 34))
        self._grid(painter)
        if isinstance(self.document, dict):
            self._waypoints(painter)
            tracks = self.document.get('tracks')
            for index, track in enumerate(tracks if isinstance(tracks, list) else []):
                if isinstance(track, dict):
                    self._track_paths(painter, track, index == self.selected)
        painter.end()

    def _grid(self, painter):
        for value in range(-int(EXTENT), int(EXTENT) + 1):
            major = value == 0
            painter.setPen(QPen(QColor(90, 100, 112) if major else QColor(48, 54, 62), 1))
            a, b = self.to_screen(-EXTENT, value), self.to_screen(EXTENT, value)
            painter.drawLine(a, b)
            a, b = self.to_screen(value, -EXTENT), self.to_screen(value, EXTENT)
            painter.drawLine(a, b)
        painter.setPen(QPen(QColor(120, 170, 220), 2))
        painter.drawRect(self.to_screen(1, -1).x(), self.to_screen(1, -1).y(), 2 * self.scale(), 2 * self.scale())
        painter.setPen(QColor(150, 160, 170))
        painter.drawText(self.to_screen(EXTENT, 0) + QPointF(6, 14), 'front')
        painter.drawText(self.to_screen(0, EXTENT) + QPointF(-38, -6), 'right')

    def _waypoints(self, painter):
        painter.setPen(QPen(QColor(230, 190, 90), 1))
        for waypoint in self.document.get('waypoints', []) if isinstance(self.document.get('waypoints'), list) else []:
            position = waypoint.get('position') if isinstance(waypoint, dict) else None
            if isinstance(position, list) and len(position) == 3 and all(isinstance(v, (int, float)) for v in position[:2]):
                p = self.to_screen(position[0], position[1])
                painter.drawRect(p.x() - 4, p.y() - 4, 8, 8)
                painter.drawText(p + QPointF(7, -5), str(waypoint.get('name') or waypoint.get('id') or ''))

    def _track_paths(self, painter, track, selected):
        color = QColor(90, 220, 150) if selected else QColor(110, 130, 150)
        keys = track.get('keyframes') if isinstance(track.get('keyframes'), list) else []
        if self.valid and keys:
            points = self._curve(track)
            painter.setPen(QPen(color, 2 if selected else 1))
            for a, b in zip(points, points[1:]):
                painter.drawLine(a, b)
            if selected and track.get('id') is not None:
                position = model.sample(self.document, track['id'], self.time)['position']
                p = self.to_screen(position[0], position[1])
                painter.setPen(QPen(QColor(255, 255, 255), 2))
                painter.setBrush(QColor(255, 90, 90))
                painter.drawEllipse(p, 6, 6)
                painter.setBrush(Qt.NoBrush)
        painter.setPen(QPen(color, 1))
        for key in keys:
            position = key.get('position') if isinstance(key, dict) else None
            if isinstance(position, list) and len(position) == 3 and all(isinstance(v, (int, float)) for v in position[:2]):
                p = self.to_screen(position[0], position[1])
                painter.setBrush(color if selected else Qt.NoBrush)
                painter.drawEllipse(p, 5, 5)
        painter.setBrush(Qt.NoBrush)

    def _curve(self, track):
        keys = track['keyframes']
        first, last, speed = float(keys[0]['t']), float(keys[-1]['t']), float(track.get('speed', 1))
        if last <= first:
            position = model.sample(self.document, track['id'], first / speed)['position']
            return [self.to_screen(position[0], position[1])]
        points = []
        for step in range(CURVE_STEPS + 1):
            tau = first + (last - first) * step / CURVE_STEPS
            tau = min(tau, last - 1e-9)  # never wrap to the first key at the final sample
            position = model.sample(self.document, track['id'], tau / speed)['position']
            points.append(self.to_screen(position[0], position[1]))
        return points

    # --- interaction ---------------------------------------------------------------
    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton and self.document is not None:
            self._drag = self.hit_keyframe(event.position())

    def mouseMoveEvent(self, event):
        if self._drag is not None:
            front, right = self.to_world(event.position())
            limit = model.POSITION_LIMIT
            self.keyframeMoved.emit(self.selected, self._drag, max(-limit, min(limit, front)), max(-limit, min(limit, right)))

    def mouseReleaseEvent(self, _event):
        self._drag = None
