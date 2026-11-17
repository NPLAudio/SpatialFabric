"""Pure document edits (no GUI). Callers checkpoint history first; every function either applies a
change that keeps the document valid for the edited field or raises ValueError and leaves it untouched."""
import math

from . import model

MIN_GAP = 0.001  # seconds between neighboring keyframes


def _track(document, ti):
    tracks = document.get('tracks') if isinstance(document, dict) else None
    if not isinstance(tracks, list) or not 0 <= ti < len(tracks) or not isinstance(tracks[ti], dict):
        raise ValueError('No such track')
    return tracks[ti]


def _keys(document, ti):
    keys = _track(document, ti).get('keyframes')
    if not isinstance(keys, list):
        raise ValueError('Track has no keyframes')
    return keys


def _key(document, ti, ki):
    keys = _keys(document, ti)
    if not isinstance(ki, int) or isinstance(ki, bool) or not 0 <= ki < len(keys) or not isinstance(keys[ki], dict):
        raise ValueError('No such keyframe')
    return keys[ki]


def _finite(*values):
    if not all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in values):
        raise ValueError('Value must be a finite number')


def clamp_time(document, ti, ki, t):
    """Nearest legal time for keyframe ki: between its neighbors (with MIN_GAP) and >= 0."""
    keys = _keys(document, ti)
    _key(document, ti, ki)
    _finite(t)
    low = 0.0 if ki == 0 else keys[ki - 1]['t'] + MIN_GAP
    high = math.inf if ki == len(keys) - 1 else keys[ki + 1]['t'] - MIN_GAP
    if high < low:
        raise ValueError('Neighboring keyframes are too close to move this one')
    return round(max(low, min(high, t)), 6)


def set_time(document, ti, ki, t):
    _key(document, ti, ki)["t"] = clamp_time(document, ti, ki, t)
    return document


def set_position(document, ti, ki, position):
    _finite(*position)
    if len(position) != 3 or any(abs(v) > model.POSITION_LIMIT for v in position):
        raise ValueError('Position must be three values within +/-16')
    key = _key(document, ti, ki)
    key.pop('waypoint', None)  # editing a waypoint-linked key detaches it
    key['position'] = [round(float(v), 6) for v in position]
    return document


def set_choice(document, ti, ki, field, value):
    allowed = {'interp': model.INTERPS, 'ease': model.EASES}
    if field not in allowed or value not in allowed[field]:
        raise ValueError('Unsupported value')
    _key(document, ti, ki)[field] = value
    return document


def add_keyframe(document, ti, t):
    """Insert a keyframe at track time t that initially lies on the current path. Returns its index."""
    _finite(t)
    if t < 0:
        raise ValueError('Time must be >= 0')
    if model.validate(document):
        raise ValueError('Fix validation problems before adding keyframes')
    track = _track(document, ti)
    keys = track['keyframes']
    if any(abs(key['t'] - t) < MIN_GAP for key in keys):
        raise ValueError('A keyframe already exists at that time')
    speed = track.get('speed', 1)
    position = model.sample(document, track['id'], t / speed)['position']
    index = sum(1 for key in keys if key['t'] < t)
    new = {'t': round(t, 6), 'position': [round(v, 6) for v in position]}
    if index > 0:
        previous = keys[index - 1]
        for field in ('interp', 'ease'):
            if field in previous:
                new[field] = previous[field]
    keys.insert(index, new)
    return index


def delete_keyframe(document, ti, ki):
    keys = _keys(document, ti)
    _key(document, ti, ki)
    if len(keys) <= 1:
        raise ValueError('A track needs at least one keyframe')
    del keys[ki]
    return document


def replace_keyframes(document, ti, keyframes):
    """Swap in generated keyframes; refused unless the resulting track is valid."""
    track = _track(document, ti)
    keyframes = [dict(k, position=list(k['position'])) if isinstance(k, dict) and isinstance(k.get('position'), list) else k for k in keyframes]
    trial = dict(document)
    trial['tracks'] = [dict(t, keyframes=keyframes) if index == ti else t for index, t in enumerate(document['tracks'])]
    if model.validate(trial):
        raise ValueError('Generated keyframes are not valid')
    track['keyframes'] = keyframes
    return document
