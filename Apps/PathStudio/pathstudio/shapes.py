"""Deterministic shape generators that return Path v1 keyframes (canonical front/right/up).

Pure functions: no GUI, no randomness except an explicit-seed LCG, so results are reproducible
across Python versions. Curved shapes use catmull_rom on every key except the last.
"""
import math

LIMIT = 16.0


def _clamp(value):
    return max(-LIMIT, min(LIMIT, value))


def _keys(points, duration, interp='catmull_rom'):
    count = len(points)
    keys = []
    for index, (f, r, u) in enumerate(points):
        key = {'t': round(duration * index / (count - 1), 6), 'position': [round(_clamp(f), 6), round(_clamp(r), 6), round(_clamp(u), 6)]}
        if interp != 'linear' and index < count - 1:
            key['interp'] = interp
        keys.append(key)
    return keys


def _check(duration, points):
    if not (isinstance(duration, (int, float)) and math.isfinite(duration) and duration > 0):
        raise ValueError('duration must be a positive number')
    if not isinstance(points, int) or isinstance(points, bool) or not 2 <= points <= 2000:
        raise ValueError('points must be an integer from 2 to 2000')


def _finite(*values):
    if not all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in values):
        raise ValueError('shape parameters must be finite numbers')


def line(start, end, duration):
    _check(duration, 2)
    _finite(*start, *end)
    return _keys([start, end], duration, 'linear')


def circle(center=(0, 0, 0), radius=1.0, duration=8.0, points=16, clockwise=True, revolutions=1.0):
    """Circle in the front/right plane; starts at front=+radius. Closed when revolutions is whole."""
    _check(duration, points)
    _finite(*center, radius, revolutions)
    sign = -1.0 if clockwise else 1.0  # clockwise seen from above with front up on screen
    path = []
    for i in range(points):
        angle = sign * 2 * math.pi * revolutions * i / (points - 1)
        path.append((center[0] + radius * math.cos(angle), center[1] + radius * math.sin(-angle), center[2]))
    return _keys(path, duration)


def spiral(center=(0, 0, 0), radius_start=0.2, radius_end=1.0, turns=2.0, duration=8.0, points=48,
           height_start=None, height_end=None):
    _check(duration, points)
    _finite(*center, radius_start, radius_end, turns)
    h0 = center[2] if height_start is None else height_start
    h1 = center[2] if height_end is None else height_end
    _finite(h0, h1)
    path = []
    for i in range(points):
        u = i / (points - 1)
        angle = -2 * math.pi * turns * u
        radius = radius_start + (radius_end - radius_start) * u
        path.append((center[0] + radius * math.cos(angle), center[1] + radius * math.sin(-angle), h0 + (h1 - h0) * u))
    return _keys(path, duration)


def figure_eight(center=(0, 0, 0), radius=1.0, duration=8.0, points=32):
    """Lemniscate of Gerono: front = r*sin(t), right = r*sin(t)*cos(t)."""
    _check(duration, points)
    _finite(*center, radius)
    path = []
    for i in range(points):
        a = 2 * math.pi * i / (points - 1)
        path.append((center[0] + radius * math.sin(a), center[1] + radius * math.sin(a) * math.cos(a), center[2]))
    return _keys(path, duration)


def random_walk(seed, center=(0, 0, 0), step=0.25, bound=1.0, duration=8.0, points=24):
    """Seeded walk confined to +/-bound around center (reflecting). Same seed -> same path everywhere."""
    _check(duration, points)
    _finite(*center, step, bound)
    if not isinstance(seed, int) or isinstance(seed, bool) or step < 0 or bound <= 0:
        raise ValueError('seed must be an integer, step >= 0 and bound > 0')
    state = (seed * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF

    def uniform():
        nonlocal state
        state = (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        return (state >> 11) / float(1 << 53) * 2 - 1  # [-1, 1)

    f, r = 0.0, 0.0
    path = [(center[0], center[1], center[2])]
    for _ in range(points - 1):
        for axis in range(2):
            value = (f, r)[axis] + uniform() * step
            if value > bound:
                value = 2 * bound - value
            elif value < -bound:
                value = -2 * bound - value
            value = max(-bound, min(bound, value))
            if axis == 0:
                f = value
            else:
                r = value
        path.append((center[0] + f, center[1] + r, center[2]))
    return _keys(path, duration)


SHAPES = ('line', 'circle', 'spiral', 'figure_eight', 'random_walk')


def generate(name, center=(0, 0, 0), size=1.0, duration=8.0, points=24, seed=1):
    """Uniform entry point used by the GUI."""
    if name == 'line':
        return line((center[0] - size, center[1], center[2]), (center[0] + size, center[1], center[2]), duration)
    if name == 'circle':
        return circle(center, size, duration, max(points, 8))
    if name == 'spiral':
        return spiral(center, size * 0.2, size, 2.0, duration, max(points, 16))
    if name == 'figure_eight':
        return figure_eight(center, size, duration, max(points, 16))
    if name == 'random_walk':
        return random_walk(seed, center, size * 0.25, size, duration, points)
    raise ValueError('Unknown shape')
