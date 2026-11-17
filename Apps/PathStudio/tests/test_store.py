import json
from pathlib import Path
import sys
import tempfile
import unittest

APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP))
from pathstudio import model, store  # noqa: E402


class StoreTests(unittest.TestCase):
    def test_new_document_is_valid(self):
        self.assertEqual([], model.validate(store.new_document()))

    def test_roundtrip_is_atomic_lf_utf8(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'a.sfpath.json'
            document = store.new_document()
            document['name'] = 'Caf\u00e9 \u65e5\u672c\u8a9e'
            store.save_document(path, document)
            self.assertEqual(document, store.load_document(path))
            self.assertNotIn(b'\r', path.read_bytes())
            self.assertEqual([path.name], [p.name for p in Path(temp).iterdir()])

    def test_failed_save_keeps_original_and_removes_temporary(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'a.json'
            store.save_document(path, store.new_document())
            before = path.read_bytes()
            with self.assertRaises(TypeError):
                store.save_document(path, {'bad': object()})
            self.assertEqual(before, path.read_bytes())
            self.assertEqual(['a.json'], [p.name for p in Path(temp).iterdir()])

    def test_rejects_bad_json_nonobject_and_oversize(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'x.json'
            for data in (b'{', b'[]', b'\xff\xfe'):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    store.load_document(path)
            path.write_text(json.dumps({'a': 1}))
            old = store.MAX_BYTES
            store.MAX_BYTES = 3
            try:
                with self.assertRaises(ValueError):
                    store.load_document(path)
            finally:
                store.MAX_BYTES = old


if __name__ == '__main__':
    unittest.main()
