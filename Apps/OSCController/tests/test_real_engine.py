"""Opt-in: compare a sender against a REAL running engine (the Godot example, served headless and silent).

  set GODOT_EXE=C:/path/Godot_console.exe   set GODOT_PROJECT=C:/path/SpatialFabric-Godot
  python -m unittest tests.test_real_engine -v

The harness points every cue at an intentionally invalid device, so a forwarded /play can only fail and report why;
no sound is produced and no microphone can open.
"""
import os
import socket
import subprocess
import threading
import time
import unittest
from pathlib import Path
from osc_controller.debugger import Debugger
from osc_controller.model import load_profile
from osc_controller.receiver_hub import ReceiverHub
from osc_controller.tracker import ObjectTracker
from osc_controller.protocol import Message, encode

GODOT, PROJECT = os.environ.get('GODOT_EXE'), os.environ.get('GODOT_PROJECT')


def f(v): return ('f', float(v))


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0)); return probe.getsockname()[1]


@unittest.skipUnless(GODOT and PROJECT, 'Set GODOT_EXE and GODOT_PROJECT to compare against a real engine')
class RealEngine(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.process = subprocess.Popen([GODOT, '--headless', '--audio-driver', 'Dummy', '--path', PROJECT, '--script', 'res://Tests/Godot/serve_for_debugger.gd', '--', '60'],
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        cls.lines = []
        ready = threading.Event()

        def pump():
            for line in cls.process.stdout:
                cls.lines.append(line.strip())
                if 'SERVING' in line: ready.set()
        threading.Thread(target=pump, daemon=True).start()
        if not ready.wait(60): cls.process.kill(); raise RuntimeError('Engine never started serving: ' + '\n'.join(cls.lines[-10:]))

    @classmethod
    def tearDownClass(cls): cls.process.kill(); cls.process.wait(10)

    def setUp(self):
        self.debugger = Debugger(); self.listen_port = free_port()
        self.debugger.start(('127.0.0.1', self.listen_port), monitor=('127.0.0.1', 9001), query=('127.0.0.1', 9000), forward=('127.0.0.1', 9000))
        self.sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def tearDown(self): self.debugger.stop(); self.sender.close()

    def send(self, message): self.sender.sendto(encode(message), ('127.0.0.1', self.listen_port))

    def run_until(self, seconds, condition):
        end = time.monotonic() + seconds
        while time.monotonic() < end and not condition(): self.debugger.poll(); time.sleep(0.01)
        return condition()

    def effects(self): return {code: (sev, text) for code, sev, text in self.debugger.effects}

    def test_sender_compared_with_the_running_engine(self):
        self.debugger.refresh_contract()
        self.assertTrue(self.run_until(10, lambda: self.debugger.validator.contract.source == 'engine'), 'no scene response from the engine')
        contract = self.debugger.validator.contract
        self.assertEqual(contract.cues, {'/godot/playback/mono': 'playback', '/godot/playback/stereo': 'playback', '/godot/playback/replay': 'playback', '/godot/live/input': 'live'})
        self.assertEqual(contract.objects, {1, 2, 3, 4})
        self.send(Message('/godot/playback/stereo/loop', (f(0),)))
        self.send(Message('/godot/live/input/gain', (f(2.5),)))
        self.send(Message('/godot/playback/mono/play'))
        self.send(Message('/godot/live/input/play'))
        self.send(Message('/godot/playbak/mono/play'))                      # typo: caught, never forwarded
        self.assertTrue(self.run_until(25, lambda: len(self.debugger.effects) >= 4), str(self.debugger.effects))
        effects = self.effects()
        self.assertEqual(effects['effect-loop'][0], 'ok', effects['effect-loop'])
        self.assertEqual(effects['effect-gain'][0], 'ok', effects['effect-gain'])
        unconfirmed = [text for code, _, text in self.debugger.effects if code == 'effect-unconfirmed']
        self.assertEqual(len(unconfirmed), 2, self.debugger.effects)        # both starts fail on the invalid device, and the engine says why
        self.assertTrue(all('no default fallback' in text for text in unconfirmed), unconfirmed)
        typo = next(r for r in self.debugger.records if r.address == '/godot/playbak/mono/play')
        self.assertEqual(typo.severity, 'error'); self.assertFalse(typo.forwarded); self.assertIn('/godot/playback/mono/play', typo.findings[0].text)


    def test_controller_with_the_godot_profile_receives_the_engines_osc_output(self):
        """What the user saw: the controller must receive positions from the running engine on the profile's ports."""
        self.debugger.stop()   # setUp bound the state port for the debugger tests; the controller needs it here
        profile = load_profile(Path(__file__).resolve().parents[1] / 'profiles/spatialfabric-godot.json')
        hub = ReceiverHub(); tracker = ObjectTracker(); states = []
        try:
            hub.start('tracker', [(profile.tracker_ip, port) for port in profile.tracker_ports])
            hub.start('monitor', [(profile.monitor_ip, profile.monitor_port)])
            end = time.monotonic() + 10
            while time.monotonic() < end and len(tracker.objects) < 4:
                for (ip, port), event in hub.poll():
                    if event[0] == 'message': tracker.ingest(event[3], event[2][0], port, event[1])
                time.sleep(0.01)
        finally: hub.close()
        ids = sorted(key.rsplit(':', 1)[-1] for key in tracker.objects)   # keys look like ADM:ip:port:id
        self.assertEqual(ids, ['1', '2', '3', '4'], f'ids {ids} accepted={tracker.accepted} ignored={tracker.ignored}')
        self.assertGreater(tracker.accepted, 10)


if __name__ == '__main__': unittest.main()
