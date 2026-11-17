"""Safe loading and saving of path documents (no network, no GUI)."""
import json
import os
from pathlib import Path

MAX_BYTES = 16 * 1024 * 1024


def new_document():
    return {'schema': 'org.nplaudio.spatialfabric.path', 'schema_version': 1, 'name': 'Untitled', 'space': 'canonical',
            'waypoints': [],
            'tracks': [{'id': 'track1', 'target': {'kind': 'object', 'key': 'object/key'}, 'loop': 'none', 'speed': 1,
                        'keyframes': [{'t': 0, 'position': [0, 0, 0]}, {'t': 4, 'position': [1, 0, 0]}]}]}


def load_document(path):
    """Read a JSON object, bounded in size. Semantic checks stay in model.validate."""
    path = Path(path)
    if path.stat().st_size > MAX_BYTES:
        raise ValueError('Path document is larger than the 16 MiB limit')
    try:
        data = json.loads(path.read_text(encoding='utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f'Not a valid UTF-8 JSON document: {error}') from error
    if not isinstance(data, dict):
        raise ValueError('A path document must be a JSON object')
    return data


def save_document(path, document):
    """Write atomically (temporary file in the same folder, then replace); LF line endings."""
    path = Path(path)
    text = json.dumps(document, indent=2, ensure_ascii=False) + '\n'
    if len(text.encode('utf-8')) > MAX_BYTES:
        raise ValueError('Path document is larger than the 16 MiB limit')
    temporary = path.with_name(path.name + '.tmp')
    try:
        with open(temporary, 'w', encoding='utf-8', newline='\n') as handle:
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()
