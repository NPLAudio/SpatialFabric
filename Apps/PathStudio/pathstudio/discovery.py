"""Read-only scene discovery for binding tracks to engine objects.

Uses the hub's OSC scene-query client (no pip packages). It never sends playback commands,
starts audio, or changes the engine. Discovery happens only when the operator asks for it.
"""
import math
from dataclasses import dataclass
from pathlib import Path
import sys

MAX_TARGETS = 4096


@dataclass(frozen=True)
class Target:
    kind: str            # 'object' or 'listener' (Path v1 target kinds)
    key: str             # per-snapshot scene key
    label: str
    object_id: 'int | None' = None
    position: 'tuple | None' = None   # canonical front/right/up, when the engine reports it

    def path_target(self):
        target = {'kind': self.kind, 'key': self.key[:256]}
        if self.label:
            target['label'] = self.label[:256]
        if self.object_id is not None:
            target['object_id'] = self.object_id
        return target


def _vector(value):
    if isinstance(value, list) and len(value) == 3 and all(
            isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) for v in value):
        return tuple(float(v) for v in value)
    return None


def targets_from_scene(scene):
    """Extract bindable targets from a scene-v1 snapshot. Unknown/odd rows are skipped, never fatal."""
    rows = scene.get('objects') if isinstance(scene, dict) else None
    targets = []
    for row in rows if isinstance(rows, list) else []:
        if not isinstance(row, dict) or not isinstance(row.get('key'), str) or not row['key']:
            continue
        kind = row.get('kind')
        if kind == 'spatial_binding':
            object_id = row.get('default_object_id')
            object_id = object_id if type(object_id) is int and object_id >= 1 else None
            label = row.get('label') or row.get('actor_label') or (f'object {object_id}' if object_id else row['key'].rsplit('/', 1)[-1])
            targets.append(Target('object', row['key'], str(label), object_id, _vector(row.get('stage_normalized'))))
        elif kind == 'listener':
            label = row.get('actor_label') or row.get('name') or row['key'].rsplit('/', 1)[-1]
            targets.append(Target('listener', row['key'], str(label)))
        if len(targets) >= MAX_TARGETS:
            break
    return sorted(targets, key=lambda t: (t.kind, t.label.casefold(), t.key))


def _query_function():
    try:
        from osc_controller.scene_query import query_scene
    except ImportError:
        hub = Path(__file__).resolve().parents[2] / 'OSCController'
        if not (hub / 'osc_controller').is_dir():
            raise RuntimeError('Scene discovery needs the hub OSC Controller package (Apps/OSCController)') from None
        sys.path.insert(0, str(hub))
        from osc_controller.scene_query import query_scene
    return query_scene


def fetch_targets(host='127.0.0.1', port=8100, page_size=16):
    """Blocking read-only query. Raises ValueError/OSError/RuntimeError with a readable message."""
    scene = _query_function()(host, int(port), page_size)
    return scene, targets_from_scene(scene)
