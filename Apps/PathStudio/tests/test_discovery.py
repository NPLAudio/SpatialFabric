import json
from pathlib import Path
import socket
import sys
import threading
import time
import unittest

APP = Path(__file__).resolve().parents[1]
HUB = APP.parents[1]
sys.path.insert(0, str(APP))
sys.path.insert(0, str(HUB / 'Apps/OSCController'))
from pathstudio import discovery, model  # noqa: E402
from osc_controller.simulator import EngineSimulator, SimCue  # noqa: E402

SCENE = json.loads((HUB / 'Examples/OSCSceneQuery/sample_scene.json').read_text(encoding='utf-8'))


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


class DiscoveryTests(unittest.TestCase):
    def test_sample_scene_yields_only_bindable_targets(self):
        targets = discovery.targets_from_scene(SCENE)
        self.assertEqual(['object'], [t.kind for t in targets])  # playback cues have no position to path
        target = targets[0]
        self.assertEqual('Moving object', target.label)
        self.assertEqual(1, target.object_id)
        self.assertEqual((0.5, 0.25, 0.1), target.position)
        self.assertEqual({'kind': 'object', 'key': target.key, 'label': 'Moving object', 'object_id': 1}, target.path_target())

    def test_targets_pass_path_validation(self):
        target = discovery.targets_from_scene(SCENE)[0]
        doc = {'schema': model.SCHEMA, 'schema_version': 1, 'space': 'canonical',
               'tracks': [{'id': 't', 'target': target.path_target(), 'keyframes': [{'t': 0, 'position': [0, 0, 0]}]}]}
        self.assertEqual([], model.validate(doc))

    def test_malformed_scenes_are_skipped_not_fatal(self):
        for scene in (None, [], {'objects': 5}, {'objects': [None, 3, {'kind': 'spatial_binding'}, {'key': ''},
                                                              {'kind': 'spatial_binding', 'key': 'k', 'default_object_id': 0, 'stage_normalized': [1, float('nan'), 0]}]}):
            targets = discovery.targets_from_scene(scene)
            for t in targets:
                self.assertIsNone(t.object_id)
                self.assertIsNone(t.position)

    def test_listener_rows_and_long_values_are_bounded(self):
        scene = {'objects': [{'kind': 'listener', 'key': 'L' * 400, 'actor_label': 'M' * 400}]}
        target = discovery.targets_from_scene(scene)[0]
        self.assertEqual('listener', target.kind)
        path = target.path_target()
        self.assertEqual(256, len(path['key']))
        self.assertEqual(256, len(path['label']))

    def test_real_udp_roundtrip_with_silent_simulator(self):
        port = free_port()
        simulator = EngineSimulator([SimCue('/demo/playback/mono')], objects=(1, 2, 3))
        simulator.start(('127.0.0.1', port))
        stop = threading.Event()

        def serve():
            while not stop.is_set():
                simulator.poll()
                time.sleep(0.002)

        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        try:
            _scene, targets = discovery.fetch_targets('127.0.0.1', port)
        finally:
            stop.set()
            thread.join(2)
            simulator.stop()
        self.assertEqual([1, 2, 3], sorted(t.object_id for t in targets))
        self.assertTrue(all(t.kind == 'object' for t in targets))
        self.assertEqual({'/sim/binding/1', '/sim/binding/2', '/sim/binding/3'}, {t.key for t in targets})
        self.assertEqual(['/spatialfabric/scene/query'], sorted(set(simulator.received)))  # read-only: only queries were sent


if __name__ == '__main__':
    unittest.main()
