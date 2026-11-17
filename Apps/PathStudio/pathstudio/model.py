"""Reference implementation of Specs/Path-v1.md (standard library only, no GUI/network)."""
import bisect
import math
import re

SCHEMA = 'org.nplaudio.spatialfabric.path'
ID = re.compile(r'^[A-Za-z0-9_-]{1,64}\Z')
KINDS = ('object', 'listener', 'camera')
LOOPS = ('none', 'loop', 'ping_pong')
INTERPS = ('linear', 'hold', 'catmull_rom', 'bezier')
EASES = ('linear', 'in', 'out', 'in_out')
PARAMETER_RANGES = {'gain': (0.0, 4.0), 'spread': (0.0, 1.0)}
MAX_TRACKS, MAX_POINTS, MAX_WAYPOINTS = 256, 10000, 4096
POSITION_LIMIT, TANGENT_LIMIT, MAX_SPEED = 16.0, 32.0, 64.0


def _number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _vector(value, limit):
    """Return (well-formed, within-limit)."""
    shaped = isinstance(value, list) and len(value) == 3 and all(_number(v) for v in value)
    return shaped, shaped and all(abs(v) <= limit for v in value)


def validate(document):
    """Return a sorted list of (code, path) tuples; empty means valid."""
    errors = []

    def add(code, path):
        errors.append((code, path))

    if not isinstance(document, dict):
        return [('bad_schema', '$')]
    if document.get('schema') != SCHEMA:
        add('bad_schema', '$.schema')
    version = document.get('schema_version')
    if version != 1 or isinstance(version, bool):
        add('bad_version', '$.schema_version')
    if document.get('space') != 'canonical':
        add('bad_space', '$.space')
    waypoints = {}
    raw_waypoints = document.get('waypoints', [])
    if not isinstance(raw_waypoints, list) or len(raw_waypoints) > MAX_WAYPOINTS:
        add('bad_id', '$.waypoints')
        raw_waypoints = []
    for index, waypoint in enumerate(raw_waypoints):
        where = f'$.waypoints[{index}]'
        if not isinstance(waypoint, dict) or not isinstance(waypoint.get('id'), str) or not ID.match(waypoint['id']):
            add('bad_id', where + '.id')
            continue
        if waypoint['id'] in waypoints:
            add('duplicate_id', where + '.id')
        shaped, ranged = _vector(waypoint.get('position'), POSITION_LIMIT)
        if not shaped:
            add('bad_position', where + '.position')
        elif not ranged:
            add('out_of_range', where + '.position')
        else:
            waypoints[waypoint['id']] = [float(v) for v in waypoint['position']]
    tracks = document.get('tracks')
    if not isinstance(tracks, list) or not tracks:
        add('no_tracks', '$.tracks')
        tracks = []
    elif len(tracks) > MAX_TRACKS:
        add('too_many_tracks', '$.tracks')
        tracks = []
    seen = set()
    for ti, track in enumerate(tracks):
        _validate_track(track, f'$.tracks[{ti}]', waypoints, seen, add)
    return sorted(set(errors))


def _optional_target_fields_ok(target):
    label, object_id = target.get('label'), target.get('object_id')
    if 'label' in target and (not isinstance(label, str) or len(label) > 256):
        return False
    return 'object_id' not in target or (type(object_id) is int and object_id >= 1)


def _validate_track(track, where, waypoints, seen, add):
    if not isinstance(track, dict):
        add('bad_target', where)
        return
    tid = track.get('id')
    if not isinstance(tid, str) or not ID.match(tid):
        add('bad_id', where + '.id')
    elif tid in seen:
        add('duplicate_id', where + '.id')
    else:
        seen.add(tid)
    target = track.get('target')
    key = target.get('key') if isinstance(target, dict) else None
    if not isinstance(target, dict) or target.get('kind') not in KINDS or not isinstance(key, str) or not 1 <= len(key) <= 256:
        add('bad_target', where + '.target')
    elif not _optional_target_fields_ok(target):
        add('bad_target', where + '.target')
    if track.get('loop', 'none') not in LOOPS:
        add('bad_loop', where + '.loop')
    speed = track.get('speed', 1)
    if not _number(speed) or not 0 < speed <= MAX_SPEED:
        add('bad_speed', where + '.speed')
    keys = track.get('keyframes')
    if not isinstance(keys, list) or not keys:
        add('no_keyframes', where + '.keyframes')
        keys = []
    elif len(keys) > MAX_POINTS:
        add('too_many_keyframes', where + '.keyframes')
        keys = []
    previous = None
    for ki, frame in enumerate(keys):
        kw = f'{where}.keyframes[{ki}]'
        if not isinstance(frame, dict):
            add('bad_time', kw)
            continue
        t = frame.get('t')
        if not _number(t) or t < 0:
            add('bad_time', kw + '.t')
        else:
            if previous is not None and t <= previous:
                add('times_not_increasing', kw + '.t')
            previous = t
        has_position, has_waypoint = 'position' in frame, 'waypoint' in frame
        if has_position == has_waypoint:
            add('position_conflict', kw)
        elif has_position:
            shaped, ranged = _vector(frame['position'], POSITION_LIMIT)
            if not shaped:
                add('bad_position', kw + '.position')
            elif not ranged:
                add('out_of_range', kw + '.position')
        elif frame['waypoint'] not in waypoints:
            add('unknown_waypoint', kw + '.waypoint')
        if frame.get('interp', 'linear') not in INTERPS:
            add('bad_interp', kw + '.interp')
        if frame.get('ease', 'linear') not in EASES:
            add('bad_ease', kw + '.ease')
        for name in ('tangent_in', 'tangent_out'):
            if name in frame and not _vector(frame[name], TANGENT_LIMIT)[1]:
                add('bad_tangent', f'{kw}.{name}')
    parameters = track.get('parameters', {})
    if not isinstance(parameters, dict):
        add('bad_parameter', where + '.parameters')
        return
    for name, curve in parameters.items():
        if name not in PARAMETER_RANGES:
            continue
        low, high = PARAMETER_RANGES[name]
        pw = f'{where}.parameters.{name}'
        if not isinstance(curve, list) or not 1 <= len(curve) <= MAX_POINTS:
            add('bad_parameter', pw)
            continue
        last = None
        for point in curve:
            if (not isinstance(point, dict) or not _number(point.get('t')) or point['t'] < 0
                    or not _number(point.get('value')) or not low <= point['value'] <= high
                    or (last is not None and point['t'] <= last)):
                add('bad_parameter', pw)
                break
            last = point['t']


def _resolve(document, track):
    waypoints = {w['id']: w['position'] for w in document.get('waypoints', [])}
    keys = []
    for key in track['keyframes']:
        position = key['position'] if 'position' in key else waypoints[key['waypoint']]
        keys.append(dict(t=float(key['t']), p=[float(v) for v in position], interp=key.get('interp', 'linear'),
                         ease=key.get('ease', 'linear'), tin=[float(v) for v in key.get('tangent_in', (0, 0, 0))],
                         tout=[float(v) for v in key.get('tangent_out', (0, 0, 0))]))
    return keys


def _map_time(time, first, last, loop, speed):
    tau = float(time) * float(speed)
    span = last - first
    if tau <= first or span == 0:
        return first
    if loop == 'loop':
        return first + math.fmod(tau - first, span)
    if loop == 'ping_pong':
        m = math.fmod(tau - first, 2 * span)
        return first + (2 * span - m if m > span else m)
    return min(tau, last)


def _ease(name, u):
    if name == 'in':
        return u * u
    if name == 'out':
        return 1 - (1 - u) * (1 - u)
    if name == 'in_out':
        return u * u * (3 - 2 * u)
    return u


def _position(keys, tau):
    if tau >= keys[-1]['t']:
        return list(keys[-1]['p'])
    i = bisect.bisect_right([k['t'] for k in keys], tau) - 1
    a, b = keys[i], keys[i + 1]
    if a['interp'] == 'hold':
        return list(a['p'])
    u = _ease(a['ease'], (tau - a['t']) / (b['t'] - a['t']))
    if a['interp'] == 'linear':
        return [x + (y - x) * u for x, y in zip(a['p'], b['p'])]
    if a['interp'] == 'bezier':
        c1 = [x + d for x, d in zip(a['p'], a['tout'])]
        c2 = [x + d for x, d in zip(b['p'], b['tin'])]
        w = (1 - u) ** 3, 3 * (1 - u) ** 2 * u, 3 * (1 - u) * u * u, u ** 3
        return [w[0] * p + w[1] * q + w[2] * r + w[3] * s for p, q, r, s in zip(a['p'], c1, c2, b['p'])]
    p0 = keys[max(i - 1, 0)]['p']
    p3 = keys[min(i + 2, len(keys) - 1)]['p']
    return [0.5 * (2 * p1 + (-q0 + p2) * u + (2 * q0 - 5 * p1 + 4 * p2 - q3) * u * u
                   + (-q0 + 3 * p1 - 3 * p2 + q3) * u ** 3)
            for q0, p1, p2, q3 in zip(p0, a['p'], b['p'], p3)]


def _curve(points, tau):
    if tau <= points[0]['t']:
        return float(points[0]['value'])
    if tau >= points[-1]['t']:
        return float(points[-1]['value'])
    i = bisect.bisect_right([p['t'] for p in points], tau) - 1
    a, b = points[i], points[i + 1]
    return a['value'] + (b['value'] - a['value']) * ((tau - a['t']) / (b['t'] - a['t']))


def sample(document, track_id, time):
    """Sample a validated document. Returns {'position': [f, r, u], 'parameters': {name: value}}."""
    track = next(t for t in document['tracks'] if t['id'] == track_id)
    keys = _resolve(document, track)
    loop, speed = track.get('loop', 'none'), track.get('speed', 1)
    tau = _map_time(time, keys[0]['t'], keys[-1]['t'], loop, speed)
    result = {'position': _position(keys, tau), 'parameters': {}}
    for name, curve in track.get('parameters', {}).items():
        if name in PARAMETER_RANGES:
            result['parameters'][name] = _curve(curve, _map_time(time, curve[0]['t'], curve[-1]['t'], loop, speed))
    return result


def curve_points(document, track_id, steps=240):
    """Sample one pass of a validated track across its keyframe span (for drawing), in canonical f/r/u.

    Loop modes draw a single pass: the final sample stops just before the wrap to the first key."""
    track = next(t for t in document['tracks'] if t['id'] == track_id)
    keys = track['keyframes']
    first, last, speed = float(keys[0]['t']), float(keys[-1]['t']), float(track.get('speed', 1))
    if last <= first:
        return [sample(document, track_id, first / speed)['position']]
    points = []
    for step in range(steps + 1):
        tau = min(first + (last - first) * step / steps, last - 1e-9)
        points.append(sample(document, track_id, tau / speed)['position'])
    return points


def to_engine(position, engine):
    """Convert canonical [front, right, up] to an engine-specific list at the edge."""
    f, r, u = position
    if engine == 'unity_m':
        return [r, u, f]
    if engine == 'unreal_cm':
        return [100 * f, 100 * r, 100 * u]
    if engine == 'adm_xyz':
        return [r, f, u]
    raise ValueError('Unknown engine conversion')
