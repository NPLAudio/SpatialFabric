"""Debugger + engine simulator tests. Loopback and ephemeral ports only; nothing here makes sound."""
import socket
import time
import unittest
from unittest.mock import patch
from osc_controller import debugger as dbg
from osc_controller.debugger import Contract, Debugger, Validator
from osc_controller.protocol import Message, encode
from osc_controller.simulator import EngineSimulator, SimCue


def msg(address, *args): return Message(address, tuple(args))
def f(v): return ('f', float(v))
def i(v): return ('i', int(v))
def s(v): return ('s', v)


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0)); return probe.getsockname()[1]


def codes(verdict): return [x.code for x in verdict.findings]


CUES = {'/demo/playback/mono': 'playback', '/audioloom/Stage/live/1': 'live'}


class ValidatorTests(unittest.TestCase):
    def setUp(self): self.v = Validator(Contract(dict(CUES), {1, 2}, 'engine', '/Game/Map'))

    def check(self, message, now=0.0): return self.v.check(message, now)

    def test_play_trigger_semantics(self):
        base = '/demo/playback/mono/play'
        self.assertEqual(self.check(msg(base)).intent, ('/demo/playback/mono', 'play', True))
        self.assertIsNotNone(self.check(msg(base, f(.5))).intent)
        self.assertIsNone(self.check(msg(base, f(.49))).intent)
        self.assertIsNotNone(self.check(msg(base, i(1))).intent)
        self.assertIsNone(self.check(msg(base, i(0))).intent)
        self.assertIsNotNone(self.check(msg(base, i(0), f(.9))).intent)   # first float anywhere wins
        self.assertIsNone(self.check(msg(base, i(1), f(.1))).intent)
        verdict = self.check(msg(base, s('go')))
        self.assertIn('nonnumeric', codes(verdict)); self.assertIsNone(verdict.intent)

    def test_loop_and_gain_semantics(self):
        loop = '/demo/playback/mono/loop'
        self.assertEqual(self.check(msg(loop)).intent[2], False)
        self.assertEqual(self.check(msg(loop, f(.5))).intent[2], False)
        self.assertEqual(self.check(msg(loop, f(.51))).intent[2], True)
        self.assertEqual(self.check(msg(loop, i(1))).intent[2], True)
        gain = '/audioloom/Stage/live/1/gain'
        self.assertEqual(self.check(msg(gain, f(2.5))).intent, ('/audioloom/Stage/live/1', 'gain', 2.5))
        for bad in (f(4.1), f(-.1), i(-1), f(float('inf'))):
            verdict = self.check(msg(gain, bad)); self.assertIsNone(verdict.intent); self.assertIn('gain-range', codes(verdict))
        self.assertIn('gain-missing', codes(self.check(msg(gain))))

    def test_wrong_op_for_cue_kind_explains_and_suggests(self):
        verdict = self.check(msg('/audioloom/Stage/live/1/loop', f(1)))
        self.assertEqual(verdict.severity, 'error'); self.assertIn('live input', verdict.findings[0].text)
        verdict = self.check(msg('/demo/playback/mono/gain', f(1))); self.assertIn('unknown-op', codes(verdict))
        self.assertIn('/play', self.check(msg('/demo/playback/mono/plya')).findings[0].text)

    def test_unknown_address_suggests_nearest_real_address(self):
        verdict = self.check(msg('/demo/playbak/mono/play'))
        self.assertEqual(verdict.severity, 'error'); self.assertIn('/demo/playback/mono/play', verdict.findings[0].text)

    def test_state_and_scene_response_are_engine_only(self):
        self.assertIn('engine-only', codes(self.check(msg('/demo/playback/mono/state', f(1)))))
        self.assertIn('engine-only', codes(self.check(msg('/spatialfabric/scene/response', s('a'), i(0), i(1), s('{}')))))

    def test_without_an_engine_contract_cue_bases_are_unverified_not_wrong(self):
        v = Validator()
        self.assertEqual(v.check(msg('/anything/play'), 0).findings[0].code, 'unverified-base')
        self.assertEqual(v.check(msg('/random/thing'), 0).severity, 'warn')

    def test_unsupported_tags_and_booleans_warn(self):
        verdict = self.check(Message('/demo/playback/mono/play', (('T', None),)))
        self.assertIn('tag-unsupported', codes(verdict))

    def test_adm_rules(self):
        self.assertEqual(self.check(msg('/adm/obj/1/xyz', f(0), f(1), f(0))).severity, 'ok')
        self.assertIn('adm-types', codes(self.check(msg('/adm/obj/1/xyz', f(0), f(1)))))
        self.assertIn('adm-types', codes(self.check(msg('/adm/obj/1/mute', f(1)))))
        self.assertIn('adm-sphere', codes(self.check(msg('/adm/obj/1/xyz', f(1), f(1), f(1)))))
        self.assertIn('adm-range', codes(self.check(msg('/adm/obj/1/xyz', f(2), f(0), f(0)))))
        self.assertIn('adm-unbound', codes(self.check(msg('/adm/obj/9/xyz', f(0), f(0), f(0)))))
        self.assertIn('adm-id', codes(self.check(msg('/adm/obj/0/xyz', f(0), f(0), f(0)))))
        self.assertIn('adm-id', codes(self.check(msg('/adm/obj/abc/xyz', f(0), f(0), f(0)))))
        self.assertIn('adm-aed-range', codes(self.check(msg('/adm/obj/1/aed', f(200), f(0), f(.5)))))
        self.assertEqual(self.check(msg('/adm/obj/1/aed', f(-90), f(10), f(.5))).severity, 'ok')
        self.assertIn('adm-mute', codes(self.check(msg('/adm/obj/1/mute', i(2)))))
        self.assertIn('adm-width', codes(self.check(msg('/adm/obj/1/w', f(1.5)))))
        self.assertEqual(self.check(msg('/adm/obj/1/name', s('Lead 日本'))).severity, 'ok')

    def test_scene_query_rules_match_the_engine_contract(self):
        q = '/spatialfabric/scene/query'
        self.assertEqual(self.check(msg(q, s('req-1'), i(9011))).severity, 'ok')
        self.assertEqual(Validator().check(msg(q, s('req_1'), i(9011), i(0), i(64)), 0).severity, 'ok')
        for bad in [(s('bad id!'), i(9011)), (s('a'), i(80)), (s('a'), i(9011), i(0), i(65)), (s('a'), i(9011), i(0), i(0)), (s('a'), i(9011), i(-1), i(4)), (s('a'), i(9011), i(0)), (s('a'), f(9011))]:
            with self.subTest(args=bad): self.assertEqual(Validator().check(msg(q, *bad), 0).severity, 'error')

    def test_rate_warning_and_type_drift(self):
        address = '/demo/playback/mono/loop'
        seen = []
        for n in range(130): seen += codes(self.v.check(msg(address, i(1)), 10 + n * 0.005))
        self.assertIn('rate', seen)
        self.assertIn('type-drift', codes(self.v.check(msg(address, f(1)), 11)))


class SimulatorCrossCheck(unittest.TestCase):
    """Two independent implementations of the transport rules must agree on every payload."""
    PAYLOADS = [(), (f(0),), (f(.49),), (f(.5),), (f(.51),), (f(1),), (i(0),), (i(1),), (i(2),), (s('x'),), (i(0), f(.9)), (i(1), f(.1)), (f(.9), i(0)), (s('x'), i(1)),
                (f(4),), (f(4.5),), (f(-1),), (i(-1),), (i(4),), (i(5),), (f(2.5),)]

    def test_validator_intent_equals_simulator_behavior(self):
        for kind, ops in (('playback', ('play', 'stop', 'loop')), ('live', ('play', 'stop', 'gain'))):
            for op in ops:
                for payload in self.PAYLOADS:
                    with self.subTest(kind=kind, op=op, payload=payload):
                        base = '/x/' + kind; cue = SimCue(base, kind); sim = EngineSimulator([cue])
                        if op in ('stop', 'loop'): cue.playing = True; cue.loop = True      # make a change observable
                        if op == 'gain': cue.gain = 1.0
                        message = msg(base + '/' + op, *payload)
                        sim.handle(message, ('127.0.0.1', 1))
                        intent = Validator(Contract({base: kind})).check(message, 0).intent
                        if op == 'play': self.assertEqual(intent is not None, cue.playing, f'{payload}')
                        elif op == 'stop': self.assertEqual(intent is not None and cue.playing is False, True if intent else False)
                        elif op == 'loop':
                            if intent: self.assertEqual(intent[2], cue.loop)
                            else: self.assertEqual(cue.loop, True)       # validator says "engine ignores" => unchanged
                        elif op == 'gain':
                            if intent: self.assertEqual(intent[2], cue.gain)
                            else: self.assertEqual(cue.gain, 1.0)


class DebuggerTestBed(unittest.TestCase):
    def setUp(self):
        self._settle = dbg.SETTLE_SECONDS, dbg.EXPECT_TIMEOUT
        dbg.SETTLE_SECONDS, dbg.EXPECT_TIMEOUT = 0.05, 0.9
        self.mono, self.live, self.bad = SimCue('/demo/playback/mono'), SimCue('/audioloom/Stage/live/1', 'live'), SimCue('/demo/playback/glass', fail_start='Simulated device refused to start')
        self.sim = EngineSimulator([self.mono, self.live, self.bad])
        self.state_port, self.listen_port = free_port(), free_port()
        self.sim_port = self.sim.start(('127.0.0.1', free_port()), ('127.0.0.1', self.state_port))[1]
        self.sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); self.debugger = Debugger()

    def tearDown(self):
        dbg.SETTLE_SECONDS, dbg.EXPECT_TIMEOUT = self._settle
        self.debugger.stop(); self.sim.stop(); self.sender.close()

    def start(self, forward=True):
        self.debugger.start(('127.0.0.1', self.listen_port), monitor=('127.0.0.1', self.state_port), query=('127.0.0.1', self.sim_port),
                            forward=('127.0.0.1', self.sim_port) if forward else None)

    def send(self, message): self.sender.sendto(encode(message), ('127.0.0.1', self.listen_port))

    def run_for(self, seconds, until=None):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.sim.poll(); self.debugger.poll()
            if until and until(): break
            time.sleep(0.005)

    def contract(self):
        self.debugger.refresh_contract(); self.run_for(2, lambda: self.debugger.validator.contract.source == 'engine')
        self.assertEqual(self.debugger.validator.contract.source, 'engine')

    def effects(self): return {c: (sev, text) for c, sev, text in self.debugger.effects}

    def test_loads_the_real_contract_from_the_engine(self):
        self.start(); self.contract()
        contract = self.debugger.validator.contract
        self.assertEqual(contract.cues, {'/demo/playback/mono': 'playback', '/audioloom/Stage/live/1': 'live', '/demo/playback/glass': 'playback'})
        self.assertEqual(contract.objects, {1, 2, 3, 4})

    def test_forwarding_is_off_unless_explicitly_enabled(self):
        self.start(forward=False); self.contract()
        self.send(msg('/demo/playback/mono/play')); self.run_for(.4)
        self.assertEqual(self.sim.received.count('/demo/playback/mono/play'), 0); self.assertFalse(self.mono.playing)
        self.assertEqual(self.debugger.forwarded, 0)
        self.assertEqual(self.debugger.records[-1].findings[0].code, 'engine-action')

    def test_forward_to_the_listen_endpoint_is_refused(self):
        with self.assertRaises(ValueError): self.debugger.start(('127.0.0.1', self.listen_port), forward=('127.0.0.1', self.listen_port))
        self.assertFalse(self.debugger.listening)

    def test_play_is_confirmed_by_state_and_engine_inventory(self):
        self.start(); self.contract()
        self.send(msg('/demo/playback/mono/play')); self.run_for(3, lambda: 'effect-confirmed' in self.effects())
        self.assertTrue(self.mono.playing); self.assertEqual(self.effects()['effect-confirmed'][0], 'ok')
        self.assertIn('/demo/playback/mono/play', self.effects()['effect-confirmed'][1])

    def test_engine_that_refuses_to_start_is_reported_with_its_own_diagnostic(self):
        self.start(); self.contract()
        self.send(msg('/demo/playback/glass/play')); self.run_for(4, lambda: 'effect-unconfirmed' in self.effects())
        severity, text = self.effects()['effect-unconfirmed']
        self.assertEqual(severity, 'warn'); self.assertIn('Simulated device refused to start', text)
        self.assertFalse(self.bad.playing)

    def test_loop_gain_and_stop_effects_are_compared_with_the_engine(self):
        self.start(); self.contract()
        self.send(msg('/demo/playback/mono/loop', f(1))); self.send(msg('/audioloom/Stage/live/1/gain', f(2.5)))
        self.run_for(3, lambda: len(self.debugger.effects) >= 2)
        self.assertEqual(self.effects()['effect-loop'][0], 'ok'); self.assertTrue(self.mono.loop)
        self.assertEqual(self.effects()['effect-gain'][0], 'ok'); self.assertEqual(self.live.gain, 2.5)
        self.send(msg('/demo/playback/mono/play')); self.run_for(3, lambda: 'effect-confirmed' in self.effects())
        self.send(msg('/demo/playback/mono/stop')); self.run_for(3, lambda: 'effect-stop' in self.effects())
        self.assertEqual(self.effects()['effect-stop'][0], 'ok'); self.assertFalse(self.mono.playing)

    def test_a_sender_that_disagrees_with_the_engine_is_caught_before_forwarding(self):
        self.start(); self.contract()
        self.send(msg('/demo/playbak/mono/play')); self.send(msg('/audioloom/Stage/live/1/gain', f(9)))
        self.run_for(.5, lambda: self.debugger.counts['error'] >= 1 and len(self.debugger.records) >= 4)
        self.assertEqual(self.sim.received.count('/demo/playbak/mono/play'), 0)   # errors are never forwarded
        self.assertGreaterEqual(self.debugger.counts['error'], 1)
        self.assertEqual(self.live.gain, 1.0)

    def test_malformed_datagrams_are_shown_with_their_bytes(self):
        self.start(forward=False)
        self.sender.sendto(b'/sf\0,f\0\0\x00\x00', ('127.0.0.1', self.listen_port))     # truncated float
        self.run_for(1, lambda: self.debugger.malformed)
        record = next(r for r in self.debugger.records if r.kind == 'malformed')
        self.assertEqual(record.severity, 'error'); self.assertTrue(record.raw.startswith('2f736600'))

    def test_report_is_serializable_and_summarizes_the_session(self):
        import json
        self.start(); self.contract()
        self.send(msg('/demo/playback/mono/loop', f(1))); self.run_for(2, lambda: self.debugger.effects)
        report = json.loads(json.dumps(self.debugger.report()))
        self.assertEqual(report['schema_version'], 1); self.assertEqual(report['contract']['source'], 'engine')
        self.assertIn('/demo/playback/mono/loop', report['addresses']); self.assertGreaterEqual(report['totals']['forwarded'], 1)

    def test_start_failure_binds_nothing(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as hog:
            hog.bind(('127.0.0.1', 0)); busy = hog.getsockname()[1]
            with self.assertRaises(OSError): self.debugger.start(('127.0.0.1', self.listen_port), monitor=('127.0.0.1', busy))
        self.assertFalse(self.debugger.listening)



class CommandLine(unittest.TestCase):
    def test_cli_reports_and_exit_code_reflects_findings(self):
        import json, os, subprocess, sys, tempfile, threading
        port = free_port()
        folder = tempfile.mkdtemp(); report = os.path.join(folder, 'r.json')
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        process = subprocess.Popen([sys.executable, '-m', 'osc_controller.debugger', '--listen', f'127.0.0.1:{port}', '--duration', '1.5', '--report', report, '--fail-on', 'error'],
                                   cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        time.sleep(0.6)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
            sender.sendto(encode(msg('/adm/obj/1/xyz', f(0), f(1), f(0))), ('127.0.0.1', port))
            sender.sendto(encode(msg('/adm/obj/1/xyz', f(0), f(1))), ('127.0.0.1', port))     # wrong arity -> error
        output, _ = process.communicate(timeout=20)
        data = json.load(open(report))
        self.assertEqual(process.returncode, 1, output)
        self.assertEqual(data['totals']['error'], 1); self.assertEqual(data['totals']['forwarded'], 0)
        self.assertIn('adm-types', [f['code'] for r in data['records'] for f in r['findings']])


if __name__ == '__main__': unittest.main()
