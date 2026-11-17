"""Acknowledgements tested without audio devices or engine playback."""
import json
import os
from pathlib import Path
import time
import unittest
from osc_controller.feedback import CueFeedback
from osc_controller.protocol import Message, decode, encode
from osc_controller.simulator import EngineSimulator, SimCue
from osc_controller.debugger import Contract, Debugger, Validator

BASE = '/demo/playback/mono'


class Capture:
    def __init__(self): self.messages = []
    def send(self, ip, port, message): self.messages.append(message)
    def close(self): pass


class FeedbackTests(unittest.TestCase):
    def test_pending_received_failed_and_status(self):
        f = CueFeedback(); f.command_sent(BASE, 'play', 1)
        self.assertIn('start the monitor', f.ack_text(BASE, 1, False))
        self.assertIn('waiting', f.ack_text(BASE, 2, True))
        self.assertIn('NO ACKNOWLEDGEMENT', f.ack_text(BASE, 3, True))
        ack = lambda command, result, playing, detail='': Message(BASE + '/ack', (('s', command), ('s', result), ('i', playing), ('s', detail)))
        f.on_message(ack('status', 'received', 0), 4, {BASE})
        self.assertIn('NO ACKNOWLEDGEMENT', f.ack_text(BASE, 4, True))
        f.on_message(ack('play', 'received', 0), 5, {BASE})
        self.assertIn('RECEIVED', f.ack_text(BASE, 5, True)); self.assertIn('stopped', f.state_text(BASE, False, 5))
        f.on_message(Message(BASE + '/state', (('f', 1.),)), 6, {BASE})
        self.assertIn('PLAYING', f.state_text(BASE, False, 6))
        f.on_message(ack('play', 'failed', 0, 'device unavailable'), 7, {BASE})
        self.assertIn('FAILED', f.ack_text(BASE, 7, True)); self.assertIn('device unavailable', f.ack_text(BASE, 7, True))
        self.assertIn('stopped', f.state_text(BASE, False, 7))
        f.clear(); self.assertFalse(f.state); self.assertFalse(f.sent)

    def test_unrelated_ack_does_not_resurrect_resolved_command(self):
        f = CueFeedback(); f.command_sent(BASE, 'stop', 1)
        f.on_message(Message(BASE + '/ack', (('s', 'stop'), ('s', 'received'), ('i', 0), ('s', ''))), 2, {BASE})
        self.assertNotIn(BASE, f.sent)
        f.on_message(Message(BASE + '/ack', (('s', 'status'), ('s', 'received'), ('i', 0), ('s', ''))), 3, {BASE})
        self.assertNotIn('NO ACKNOWLEDGEMENT', f.ack_text(BASE, 10, True))

    def test_malformed_and_unknown_acks_ignored(self):
        f = CueFeedback()
        for args in [(), (('s', 'play'), ('s', 'made-up'), ('i', 0), ('s', '')), (('s', 'play'), ('s', 'received'), ('i', 2), ('s', ''))]:
            self.assertIsNone(f.on_message(Message(BASE + '/ack', args), 1, {BASE}))
        f.on_message(Message('/other/state', (('i', 1),)), 1, {BASE})
        self.assertFalse(f.state); self.assertFalse(f.ack)

    def test_shared_golden_packets_and_decisions(self):
        fixture = next((p / 'Fixtures/ack-v1.json' for p in Path(__file__).resolve().parents if (p / 'Fixtures/ack-v1.json').exists()), None)
        if fixture is None: self.skipTest('shared interop checkout unavailable')
        data = json.loads(fixture.read_text(encoding='utf-8'))
        for row in data['vectors']:
            packet = bytes.fromhex(row['hex']); message = decode(packet)[0]
            self.assertEqual(encode(message), packet)
            f = CueFeedback(); base = row['address'][:-4]
            self.assertIsNotNone(f.on_message(message, 1, {base}))
            self.assertEqual(f.ack[base]['result'], row['result'])
        for row in data['decisions']:
            with self.subTest(row=row):
                cue = SimCue(BASE, row['kind']); sim = EngineSimulator([cue]); sim.sender.close(); sim.sender = Capture(); sim.monitor = ('127.0.0.1', 9001)
                sim.handle(Message(BASE + '/' + row['op'], tuple(tuple(a) for a in row['args'])), ('127.0.0.1', 9999))
                acks = [m for m in sim.sender.messages if m.address.endswith('/ack')]
                self.assertEqual(acks[0].args[1][1], row['result'])
                sim.stop()

    def test_simulated_failure_and_status(self):
        cue = SimCue(BASE, fail_start='No device'); sim = EngineSimulator([cue]); sim.sender.close(); sim.sender = Capture(); sim.monitor = ('127.0.0.1', 9001)
        sim.handle(Message(BASE + '/play'), ('127.0.0.1', 1))
        self.assertEqual([m.args[1][1] for m in sim.sender.messages], ['received', 'failed'])
        self.assertFalse(cue.playing)
        sim.sender.messages.clear(); sim.handle(Message(BASE + '/status'), ('127.0.0.1', 1))
        self.assertEqual([m.address for m in sim.sender.messages], [BASE + '/ack', BASE + '/state']); sim.stop()

    def test_udp_round_trip_received_playing_stopped_and_status(self):
        from osc_controller.transport import Receiver, Sender
        from osc_controller.debugger import _free_port
        monitor = Receiver(); monitor.start('127.0.0.1', _free_port())
        sim = EngineSimulator([SimCue(BASE)]); target = sim.start(('127.0.0.1', _free_port()), monitor.bound)
        sender = Sender(); feedback = CueFeedback()
        try:
            for command, want in [('play', True), ('stop', False), ('status', False)]:
                sent = time.monotonic(); feedback.command_sent(BASE, command, sent)
                sender.send(*target, Message(BASE + '/' + command))
                until = sent + 2
                while time.monotonic() < until:
                    sim.poll()
                    for event in monitor.poll():
                        if event[0] == 'message': feedback.on_message(event[3], event[1], {BASE})
                    ack = feedback.ack.get(BASE)
                    if ack and ack['command'] == command and feedback.state.get(BASE, (None,))[0] == want: break
                    time.sleep(.005)
                self.assertEqual(feedback.ack[BASE]['command'], command)
                self.assertEqual(feedback.ack[BASE]['result'], 'received')
                self.assertEqual(feedback.state[BASE][0], want)
        finally: sender.close(); sim.stop(); monitor.stop()

    def test_nested_unknown_suffix_rejected(self):
        sim = EngineSimulator([SimCue(BASE)]); sim.sender.close(); sim.sender = Capture(); sim.monitor = ('127.0.0.1', 9001)
        try:
            sim.handle(Message(BASE + '/foo/bar'), ('127.0.0.1', 1))
            self.assertEqual(sim.sender.messages[0].args[:2], (('s', 'foo_bar'), ('s', 'rejected')))
        finally: sim.stop()

    def test_debugger_status_and_engine_only(self):
        validator = Validator(Contract({BASE: 'playback'}))
        self.assertEqual(validator.check(Message(BASE + '/status'), 1).severity, 'ok')
        self.assertEqual(validator.check(Message(BASE + '/status', (('i', 1),)), 2).severity, 'error')
        self.assertEqual(validator.check(Message(BASE + '/ack'), 3).findings[0].code, 'engine-only')

    def test_debugger_records_engine_failure(self):
        debugger = Debugger()
        event = ('message', 1., ('127.0.0.1', 9000), Message(BASE + '/ack', (('s', 'play'), ('s', 'failed'), ('i', 0), ('s', 'No device'))))
        debugger.monitor.poll = lambda: [event]
        records = debugger.poll(1.)
        self.assertTrue(any(r.kind == 'engine-ack' and r.severity == 'error' for r in records))
        self.assertFalse(debugger.state_seen[BASE][0]); debugger.stop()


@unittest.skipUnless(os.environ.get('OSC_CONTROLLER_GUI_TESTS') == '1', 'Opt-in Tk tests')
class CueDialogTests(unittest.TestCase):
    def test_monitor_start_retains_unconfirmed_sends(self):
        from unittest.mock import patch
        from osc_controller.ui import Controller
        app = Controller(); app.withdraw()
        try:
            app.feedback.command_sent(BASE, 'stop', 1)
            with patch.object(app.receivers, 'start'):
                app.start_monitor()
            self.assertIn('NO ACKNOWLEDGEMENT', app.feedback.ack_text(BASE, 10, True))
            with patch.object(app.receivers, 'configure'):
                app.start_all_receivers()
            self.assertIn(BASE, app.feedback.sent)
        finally: app.close()

    def test_readonly_type_and_validation(self):
        import tkinter as tk
        from osc_controller.cue_dialog import CueDialog, KIND_LABELS
        root = tk.Tk(); root.withdraw()
        try:
            dialog = CueDialog(root, wait=False)
            self.assertEqual(str(dialog.kind_box['state']), 'readonly')
            self.assertEqual(len(dialog.kind_box['values']), 2)
            dialog.name.set('Test live'); dialog.kind.set(KIND_LABELS['live']); dialog._kind_changed()
            cue = dialog.accept(); self.assertEqual(cue.kind, 'live'); self.assertEqual(cue.name, 'Test live')
        finally: root.destroy()
