"""Interactive viewports: perspective plus orthographic Top / Front / Side (software drawing via QPainter).

World axes follow the canonical contract: front, right, up. Internally x=right, y=front, z=up.
The perspective camera orbits a target and defaults to sitting behind the stage looking toward +front.
Orthographic views: Top shows right (horizontal) / front (vertical); Front shows the stage from behind with
right / up; Side shows front (horizontal) / up (vertical).

Mouse: left on a keyframe selects/moves it (perspective: horizontal plane at its height, Shift = up/down;
orthographic: in the view plane, the third coordinate is kept). Empty-space left drag or right drag orbits
(perspective) or pans (orthographic); middle drag pans; the wheel zooms.
"""
import math

from PySide6.QtCore import QPointF, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QWidget

from . import model

NEAR = 0.05
HIT_RADIUS = 11.0
PITCH_LIMIT = math.radians(89.0)
MIN_DISTANCE, MAX_DISTANCE = 0.5, 60.0
MIN_ZOOM, MAX_ZOOM = 0.05, 200.0
CURVE_STEPS = 200
PRESETS = {'Perspective': (35.0, 25.0), 'Top': (0.0, 89.0), 'Behind': (0.0, 0.0), 'Side': (90.0, 0.0)}
MODES = ('perspective', 'top', 'front', 'side')
TITLES = {'perspective': 'Perspective', 'top': 'Top', 'front': 'Front (from behind)', 'side': 'Side'}
# (horizontal, vertical) canonical indices: 0=front, 1=right, 2=up
ORTHO_AXES = {'top': (1, 0), 'front': (1, 2), 'side': (0, 2)}


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _unit(a):
    length = math.sqrt(_dot(a, a))
    return (a[0] / length, a[1] / length, a[2] / length)


def _numeric(position):
    return (isinstance(position, list) and len(position) == 3
            and all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in position))


class Viewport3D(QWidget):
    dragBegan = Signal(int)
    keyframeMoved3 = Signal(int, int, float, float, float)  # track, key, front, right, up
    dragEnded = Signal()

    def __init__(self, mode='perspective', parent=None):
        super().__init__(parent)
        if mode not in MODES:
            raise ValueError('Unknown viewport mode')
        self.mode = mode
        self.document = None
        self.valid = False
        self.selected = 0
        self.selected_key = None
        self.time = 0.0
        self.context = []
        self.selected_context = None
        self.yaw, self.pitch, self.distance = math.radians(35), math.radians(25), 5.5
        self.target = [0.0, 0.0, 0.0]  # x=right, y=front, z=up
        self.zoom_level = 1.0          # orthographic only
        self.center = [0.0, 0.0]       # orthographic view centre in canonical (horizontal, vertical) units
        self._mode = None
        self._last = QPointF()
        self._drag = None
        self._vertical = False
        self.setMinimumSize(260, 220)
        self.setFocusPolicy(Qt.StrongFocus)

    # --- camera maths (unit-tested) ------------------------------------------------
    def set_view(self, yaw_degrees, pitch_degrees):
        self.yaw = math.radians(yaw_degrees)
        self.pitch = max(-PITCH_LIMIT, min(PITCH_LIMIT, math.radians(pitch_degrees)))
        self.update()

    def orbit(self, d_yaw, d_pitch):
        if self.mode != 'perspective':
            return
        self.yaw += d_yaw
        self.pitch = max(-PITCH_LIMIT, min(PITCH_LIMIT, self.pitch + d_pitch))
        self.update()

    def zoom(self, factor):
        if self.mode == 'perspective':
            self.distance = max(MIN_DISTANCE, min(MAX_DISTANCE, self.distance * factor))
        else:
            self.zoom_level = max(MIN_ZOOM, min(MAX_ZOOM, self.zoom_level / factor))
        self.update()

    def focus_on(self, front, right, up):
        """Centre the view on a canonical position."""
        if self.mode == 'perspective':
            self.target = [right, front, up]
        else:
            h, v = ORTHO_AXES[self.mode]
            position = (front, right, up)
            self.center = [position[h], position[v]]
        self.update()

    def unit_pixels(self):
        return min(self.width(), self.height()) / 4.0 * self.zoom_level

    def eye(self):
        c = math.cos(self.pitch)
        return (self.target[0] + self.distance * math.sin(self.yaw) * c,
                self.target[1] - self.distance * math.cos(self.yaw) * c,
                self.target[2] + self.distance * math.sin(self.pitch))

    def basis(self):
        forward = _unit(_sub(self.target, self.eye()))
        right = _unit(_cross(forward, (0.0, 0.0, 1.0)))
        up = _cross(right, forward)
        return forward, right, up

    def focal(self):
        return 0.9 * min(self.width(), self.height())

    def _camera_space(self, xyz):
        forward, right, up = self.basis()
        v = _sub(xyz, self.eye())
        return _dot(v, right), _dot(v, up), _dot(v, forward)

    def _screen(self, cam):
        scale = self.focal() / cam[2]
        return QPointF(self.width() / 2 + cam[0] * scale, self.height() / 2 - cam[1] * scale)

    def _ortho_screen(self, h, v):
        scale = self.unit_pixels()
        return QPointF(self.width() / 2 + (h - self.center[0]) * scale, self.height() / 2 - (v - self.center[1]) * scale)

    def project(self, front, right, up):
        """Screen point and depth for a canonical position, or None when behind the near plane."""
        if self.mode != 'perspective':
            h, v = ORTHO_AXES[self.mode]
            position = (front, right, up)
            return self._ortho_screen(position[h], position[v]), 0.0
        cam = self._camera_space((right, front, up))
        if cam[2] < NEAR:
            return None
        return self._screen(cam), cam[2]

    def ortho_hit(self, point):
        """(horizontal, vertical) canonical coordinates under a pixel in an orthographic view."""
        scale = self.unit_pixels()
        return ((point.x() - self.width() / 2) / scale + self.center[0],
                (self.height() / 2 - point.y()) / scale + self.center[1])

    def ray(self, point):
        forward, right, up = self.basis()
        dx = (point.x() - self.width() / 2) / self.focal()
        dy = (self.height() / 2 - point.y()) / self.focal()
        direction = _unit(tuple(forward[i] + dx * right[i] + dy * up[i] for i in range(3)))
        return self.eye(), direction

    def plane_hit(self, point, up_value):
        """(front, right) where the pixel ray meets the horizontal plane at height up_value."""
        origin, direction = self.ray(point)
        if abs(direction[2]) < 1e-6:
            return None
        t = (up_value - origin[2]) / direction[2]
        if t <= 0:
            return None
        return origin[1] + t * direction[1], origin[0] + t * direction[0]

    def vertical_hit(self, point, front, right):
        """Height on the camera-facing vertical plane through (front, right); None when looking straight down."""
        forward = self.basis()[0]
        normal = (forward[0], forward[1], 0.0)
        if _dot(normal, normal) < 1e-6:
            return None
        normal = _unit(normal)
        origin, direction = self.ray(point)
        denominator = _dot(direction, normal)
        if abs(denominator) < 1e-6:
            return None
        t = _dot(_sub((right, front, 0.0), (origin[0], origin[1], 0.0)), normal) / denominator
        return origin[2] + t * direction[2] if t > 0 else None

    # --- data ------------------------------------------------------------------
    def set_document(self, document, valid, selected, time, selected_key=None):
        self.document, self.valid, self.selected, self.time = document, valid, selected, time
        self.selected_key = selected_key
        self.update()

    def set_context(self, objects, selected=None):
        self.context, self.selected_context = list(objects), selected
        self.update()

    def _track(self, index):
        tracks = self.document.get('tracks') if isinstance(self.document, dict) else None
        return tracks[index] if isinstance(tracks, list) and 0 <= index < len(tracks) and isinstance(tracks[index], dict) else None

    def key_positions(self, track_index):
        track = self._track(track_index)
        keys = track.get('keyframes') if track else None
        return [(i, k['position']) for i, k in enumerate(keys if isinstance(keys, list) else [])
                if isinstance(k, dict) and _numeric(k.get('position'))]

    def hit_keyframe(self, point):
        best, best_distance, best_depth = None, HIT_RADIUS, math.inf
        for index, position in self.key_positions(self.selected):
            projected = self.project(*position)
            if projected is None:
                continue
            screen, depth = projected
            distance = math.hypot(screen.x() - point.x(), screen.y() - point.y())
            if distance <= best_distance and (distance < best_distance or depth < best_depth):
                best, best_distance, best_depth = index, distance, depth
        return best

    # --- painting ------------------------------------------------------------------
    def _line(self, painter, a, b):
        """Draw a world-space segment (x=right, y=front, z=up), clipped to the near plane when in perspective."""
        if self.mode != 'perspective':
            h, v = ORTHO_AXES[self.mode]
            pa, pb = (a[1], a[0], a[2]), (b[1], b[0], b[2])  # to (front, right, up) indexing
            painter.drawLine(self._ortho_screen(pa[h], pa[v]), self._ortho_screen(pb[h], pb[v]))
            return
        ca, cb = self._camera_space(a), self._camera_space(b)
        if ca[2] < NEAR and cb[2] < NEAR:
            return
        if ca[2] < NEAR or cb[2] < NEAR:
            near, far = (ca, cb) if ca[2] < NEAR else (cb, ca)
            t = (NEAR - near[2]) / (far[2] - near[2])
            clipped = tuple(near[i] + t * (far[i] - near[i]) for i in range(3))
            ca, cb = (clipped, far) if ca[2] < NEAR else (far, clipped)
        painter.drawLine(self._screen(ca), self._screen(cb))

    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor(20, 24, 30))
        self._grid(painter)
        self._context(painter)
        if isinstance(self.document, dict):
            self._waypoints(painter)
            tracks = self.document.get('tracks')
            for index, track in enumerate(tracks if isinstance(tracks, list) else []):
                if isinstance(track, dict):
                    self._track_paint(painter, index, track, index == self.selected)
        self._axes(painter)
        painter.setPen(QColor(170, 180, 190))
        painter.drawText(8, 16, TITLES[self.mode])
        painter.end()

    def _grid(self, painter):
        painter.setPen(QPen(QColor(44, 50, 58), 1))
        for v in range(-3, 4):
            if self.mode in ('perspective', 'top'):
                self._line(painter, (v, -3, 0), (v, 3, 0))
                self._line(painter, (-3, v, 0), (3, v, 0))
            elif self.mode == 'front':
                self._line(painter, (v, 0, -3), (v, 0, 3))
                self._line(painter, (-3, 0, v), (3, 0, v))
            else:
                self._line(painter, (0, v, -3), (0, v, 3))
                self._line(painter, (0, -3, v), (0, 3, v))
        painter.setPen(QPen(QColor(110, 160, 215), 1))
        for a, b in (((-1, -1, -1), (1, -1, -1)), ((1, -1, -1), (1, 1, -1)), ((1, 1, -1), (-1, 1, -1)), ((-1, 1, -1), (-1, -1, -1)),
                     ((-1, -1, 1), (1, -1, 1)), ((1, -1, 1), (1, 1, 1)), ((1, 1, 1), (-1, 1, 1)), ((-1, 1, 1), (-1, -1, 1)),
                     ((-1, -1, -1), (-1, -1, 1)), ((1, -1, -1), (1, -1, 1)), ((1, 1, -1), (1, 1, 1)), ((-1, 1, -1), (-1, 1, 1))):
            self._line(painter, a, b)

    def _axes(self, painter):
        for end, color, name in (((0, 1.4, 0), QColor(120, 220, 120), 'front'), ((1.4, 0, 0), QColor(230, 110, 110), 'right'),
                                 ((0, 0, 1.4), QColor(110, 150, 240), 'up')):
            painter.setPen(QPen(color, 2))
            self._line(painter, (0, 0, 0), end)
            projected = self.project(end[1], end[0], end[2])
            if projected is not None:
                painter.drawText(projected[0] + QPointF(5, -5), name)

    def _context(self, painter):
        """Read-only reference objects from the scene (never saved into the path document)."""
        for index, obj in enumerate(self.context):
            chosen = index == self.selected_context
            painter.setPen(QPen(QColor(255, 220, 120) if chosen else QColor(150, 130, 190), 2 if chosen else 1))
            f, r, u = obj['position']
            if obj['kind'] == 'box':
                hf, hr, hu = obj['half_extents']
                corners = [(r + sr * hr, f + sf * hf, u + su * hu) for sf in (-1, 1) for sr in (-1, 1) for su in (-1, 1)]
                for i, a in enumerate(corners):
                    for j in range(i + 1, 8):
                        if bin(i ^ j).count('1') == 1:
                            self._line(painter, a, corners[j])
            elif obj['kind'] == 'sphere':
                radius = obj['radius']
                steps = 24
                for plane in range(3):
                    previous = None
                    for k in range(steps + 1):
                        angle = 2 * math.pi * k / steps
                        c, s = radius * math.cos(angle), radius * math.sin(angle)
                        offset = ((c, s, 0), (c, 0, s), (0, c, s))[plane]  # (right, front, up) offsets
                        point = (r + offset[0], f + offset[1], u + offset[2])
                        if previous is not None:
                            self._line(painter, previous, point)
                        previous = point
            else:
                projected = self.project(f, r, u)
                if projected is not None:
                    p = projected[0]
                    painter.drawLine(p + QPointF(-5, 0), p + QPointF(0, -5))
                    painter.drawLine(p + QPointF(0, -5), p + QPointF(5, 0))
                    painter.drawLine(p + QPointF(5, 0), p + QPointF(0, 5))
                    painter.drawLine(p + QPointF(0, 5), p + QPointF(-5, 0))
                if obj['kind'] == 'listener':
                    yaw = math.radians(obj.get('yaw_degrees', 0.0))
                    # yaw 0 faces +front; positive turns toward the left (-right), like ADM azimuth
                    self._line(painter, (r, f, u), (r - 0.4 * math.sin(yaw), f + 0.4 * math.cos(yaw), u))
            projected = self.project(f, r, u)
            if projected is not None and obj.get('label'):
                painter.drawText(projected[0] + QPointF(8, -7), str(obj['label']))

    def _waypoints(self, painter):
        waypoints = self.document.get('waypoints')
        painter.setPen(QPen(QColor(230, 190, 90), 1))
        for waypoint in waypoints if isinstance(waypoints, list) else []:
            position = waypoint.get('position') if isinstance(waypoint, dict) else None
            projected = self.project(*position) if _numeric(position) else None
            if projected is not None:
                p = projected[0]
                painter.drawRect(p.x() - 4, p.y() - 4, 8, 8)
                painter.drawText(p + QPointF(7, -5), str(waypoint.get('name') or waypoint.get('id') or ''))

    def _track_paint(self, painter, index, track, selected):
        color = QColor(90, 220, 150) if selected else QColor(100, 120, 140)
        if self.valid and isinstance(track.get('keyframes'), list) and track['keyframes']:
            points = model.curve_points(self.document, track['id'], CURVE_STEPS)
            painter.setPen(QPen(color, 2 if selected else 1))
            for a, b in zip(points, points[1:]):
                self._line(painter, (a[1], a[0], a[2]), (b[1], b[0], b[2]))
            if selected:
                position = model.sample(self.document, track['id'], self.time)['position']
                projected = self.project(*position)
                if projected is not None:
                    painter.setPen(QPen(QColor(255, 255, 255), 2))
                    painter.setBrush(QColor(255, 90, 90))
                    painter.drawEllipse(projected[0], 6, 6)
                    painter.setBrush(Qt.NoBrush)
        for key_index, position in self.key_positions(index):
            projected = self.project(*position)
            if projected is None:
                continue
            chosen = selected and key_index == self.selected_key
            if selected and self.mode in ('perspective', 'front', 'side'):  # drop line to the floor makes height readable
                painter.setPen(QPen(QColor(70, 90, 80), 1, Qt.DotLine))
                self._line(painter, (position[1], position[0], position[2]), (position[1], position[0], 0))
            painter.setPen(QPen(QColor(255, 255, 255) if chosen else color, 2 if chosen else 1))
            painter.setBrush(QColor(255, 190, 70) if chosen else (color if selected else Qt.NoBrush))
            painter.drawEllipse(projected[0], 7 if chosen else 5, 7 if chosen else 5)
        painter.setBrush(Qt.NoBrush)

    # --- interaction ---------------------------------------------------------------
    def mousePressEvent(self, event):
        self._last = event.position()
        if event.button() == Qt.LeftButton and self.document is not None:
            hit = self.hit_keyframe(event.position())
            if hit is not None:
                self._drag, self._mode = hit, 'move'
                self._vertical = bool(event.modifiers() & Qt.ShiftModifier)
                self.dragBegan.emit(hit)
                return
            self._mode = 'orbit' if self.mode == 'perspective' else 'pan'
        elif event.button() == Qt.RightButton:
            self._mode = 'orbit' if self.mode == 'perspective' else 'pan'
        elif event.button() == Qt.MiddleButton:
            self._mode = 'pan'

    def mouseMoveEvent(self, event):
        delta = event.position() - self._last
        self._last = event.position()
        if self._mode == 'orbit':
            self.orbit(-delta.x() * 0.01, delta.y() * 0.01)
        elif self._mode == 'pan':
            if self.mode == 'perspective':
                _forward, right, up = self.basis()
                scale = self.distance / self.focal()
                for i in range(3):
                    self.target[i] += (-delta.x() * right[i] + delta.y() * up[i]) * scale
            else:
                scale = 1.0 / self.unit_pixels()
                self.center[0] -= delta.x() * scale
                self.center[1] += delta.y() * scale
            self.update()
        elif self._mode == 'move' and self._drag is not None:
            self._move(event.position())

    def _move(self, point):
        positions = dict(self.key_positions(self.selected))
        current = positions.get(self._drag)
        if current is None:
            return
        limit = model.POSITION_LIMIT

        def clamp(value):
            return max(-limit, min(limit, value))

        if self.mode != 'perspective':
            h, v = ORTHO_AXES[self.mode]
            new = list(current)
            new[h], new[v] = (clamp(c) for c in self.ortho_hit(point))
            self.keyframeMoved3.emit(self.selected, self._drag, new[0], new[1], new[2])
        elif self._vertical:
            up = self.vertical_hit(point, current[0], current[1])
            if up is not None:
                self.keyframeMoved3.emit(self.selected, self._drag, current[0], current[1], clamp(up))
        else:
            hit = self.plane_hit(point, current[2])
            if hit is not None:
                self.keyframeMoved3.emit(self.selected, self._drag, clamp(hit[0]), clamp(hit[1]), current[2])

    def mouseReleaseEvent(self, _event):
        if self._mode == 'move':
            self.dragEnded.emit()
        self._mode, self._drag = None, None

    def wheelEvent(self, event):
        self.zoom(0.9 if event.angleDelta().y() > 0 else 1.1)
