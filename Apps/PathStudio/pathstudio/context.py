"""Read-only scene context: reference objects shown in the viewports so paths can be authored in place.

Two sources, never written into a path document:
* a context file (`*.sfctx.json`, schema org.nplaudio.spatialfabric.context v1, see Specs/Context-v1.md);
* scene discovery, which exactly locates spatial bindings (stage-normalized position). Other discovery records
  only carry engine-world transforms, so they are counted as "unplaced" rather than guessed into stage space.
"""
import json
import math
from pathlib import Path
import re

SCHEMA = 'org.nplaudio.spatialfabric.context'
KINDS = ('box', 'sphere', 'point', 'listener')
MAX_OBJECTS = 4096
POSITION_LIMIT = 64.0
MAX_BYTES = 8 * 1024 * 1024
ID = re.compile(r'^[A-Za-z0-9_./:-]{1,128}\Z')


def _number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _vector(value, low, high):
    return isinstance(value, list) and len(value) == 3 and all(_number(v) and low <= v <= high for v in value)


def validate(document):
    """Sorted list of (code, path); empty means valid."""
    errors = []
    if not isinstance(document, dict):
        return [('bad_schema', '$')]
    if document.get('schema') != SCHEMA:
        errors.append(('bad_schema', '$.schema'))
    if document.get('schema_version') != 1 or isinstance(document.get('schema_version'), bool):
        errors.append(('bad_version', '$.schema_version'))
    if document.get('space') != 'canonical':
        errors.append(('bad_space', '$.space'))
    objects = document.get('objects')
    if not isinstance(objects, list) or len(objects) > MAX_OBJECTS:
        errors.append(('bad_objects', '$.objects'))
        objects = []
    seen = set()
    for index, obj in enumerate(objects):
        where = f'$.objects[{index}]'
        if not isinstance(obj, dict):
            errors.append(('bad_object', where))
            continue
        if not isinstance(obj.get('id'), str) or not ID.match(obj['id']):
            errors.append(('bad_id', where + '.id'))
        elif obj['id'] in seen:
            errors.append(('duplicate_id', where + '.id'))
        else:
            seen.add(obj['id'])
        kind = obj.get('kind')
        if kind not in KINDS:
            errors.append(('bad_kind', where + '.kind'))
        if not _vector(obj.get('position'), -POSITION_LIMIT, POSITION_LIMIT):
            errors.append(('bad_position', where + '.position'))
        if 'label' in obj and (not isinstance(obj['label'], str) or len(obj['label']) > 256):
            errors.append(('bad_label', where + '.label'))
        if kind == 'box' and not _vector(obj.get('half_extents'), 0, POSITION_LIMIT):
            errors.append(('bad_extent', where + '.half_extents'))
        if kind == 'sphere' and not (_number(obj.get('radius')) and 0 <= obj['radius'] <= POSITION_LIMIT):
            errors.append(('bad_extent', where + '.radius'))
        if 'yaw_degrees' in obj and not (_number(obj['yaw_degrees']) and -360 <= obj['yaw_degrees'] <= 360):
            errors.append(('bad_yaw', where + '.yaw_degrees'))
    return sorted(set(errors))


def load_context(path):
    """Read and validate a context file; returns drawable objects. Raises ValueError with a readable message."""
    path = Path(path)
    if path.stat().st_size > MAX_BYTES:
        raise ValueError('Context file is larger than the 8 MiB limit')
    try:
        document = json.loads(path.read_text(encoding='utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f'Not a valid UTF-8 JSON document: {error}') from error
    problems = validate(document)
    if problems:
        code, where = problems[0]
        raise ValueError(f'Invalid context file ({len(problems)} problem(s)); first: {code} at {where}')
    return objects_from_document(document)


def objects_from_document(document):
    objects = []
    for obj in document['objects']:
        entry = {'id': obj['id'], 'kind': obj['kind'], 'label': obj.get('label') or obj['id'],
                 'position': [float(v) for v in obj['position']], 'source': 'file'}
        if obj['kind'] == 'box':
            entry['half_extents'] = [float(v) for v in obj['half_extents']]
        elif obj['kind'] == 'sphere':
            entry['radius'] = float(obj['radius'])
        elif obj['kind'] == 'listener':
            entry['yaw_degrees'] = float(obj.get('yaw_degrees', 0.0))
        objects.append(entry)
    return objects


def objects_from_scene(scene):
    """(placed objects, unplaced count) from a scene-v1 snapshot; only exactly located records are placed."""
    rows = scene.get('objects') if isinstance(scene, dict) else None
    placed, unplaced = [], 0
    for row in rows if isinstance(rows, list) else []:
        if not isinstance(row, dict) or not isinstance(row.get('key'), str):
            continue
        position = row.get('stage_normalized')
        if row.get('kind') == 'spatial_binding' and _vector(position, -POSITION_LIMIT, POSITION_LIMIT):
            ident = row.get('default_object_id')
            label = row.get('label') or row.get('actor_label') or (f'object {ident}' if isinstance(ident, int) else row['key'])
            placed.append({'id': row['key'][:128], 'kind': 'point', 'label': str(label)[:256],
                           'position': [float(v) for v in position], 'source': 'scene'})
        elif row.get('kind') in ('listener', 'playback', 'live_input', 'stage') and isinstance(row.get('world_transform'), dict):
            unplaced += 1
    return placed, unplaced
