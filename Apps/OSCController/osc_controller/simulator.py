"""Engine simulator: a stand-in for a SpatialFabric/AudioLoom engine so sender apps can be tested with no engine.

It implements the wire contract (cue /play /stop /loop /gain, observed /state, read-only scene discovery) and keeps
cue state, but it NEVER produces sound, opens a device or touches a microphone. 'Playing' here is simulated state.
Its transport semantics are written independently of debugger.Validator so the two can cross-check each other.
"""
import json
import re
import time
from datetime import datetime, timezone
from .model import endpoint
from .protocol import Message, encode
from .scene_query import QUERY, RESPONSE, SCHEMA
from .transport import Receiver, Sender

REQUEST_ID = re.compile(r'^[A-Za-z0-9_-]{1,64}$')


class SimCue:
    def __init__(self, base, kind='playback', fail_start=''):
        self.base = base; self.kind = kind; self.playing = False; self.loop = False; self.gain = 1.0
        self.last_error = ''; self.fail_start = fail_start   # fail_start = error text: the simulated device refuses to start


class EngineSimulator:
    def __init__(self, cues, objects=(1, 2, 3, 4), world='/Simulator/World'):
        self.cues = {c.base: c for c in cues}; self.objects = list(objects); self.world = world
        self.input = Receiver(); self.sender = Sender(); self.monitor = None; self.received = []

    def start(self, listen, monitor=None):
        self.input.start(*listen); self.monitor = endpoint(*monitor) if monitor else None
        return self.input.bound

    def stop(self): self.input.stop(); self.sender.close()

    # ---- semantics (AudioLoom wire rules): first float anywhere precedes first int ------------------------
    @staticmethod
    def _number(message):
        floats = [v for t, v in message.args if t == 'f']; ints = [v for t, v in message.args if t == 'i']
        return floats[0] if floats else (ints[0] if ints else None), bool(floats)

    def _state(self, cue, playing):
        if cue.playing != playing:
            cue.playing = playing
            if self.monitor: self.sender.send(*self.monitor, Message(cue.base + '/state', (('f', 1.0 if playing else 0.0),)))

    def _ack(self, cue, command, result, detail=''):
        command = ''.join(c if c.isascii() and (c.isalnum() or c in '_-') else '_' for c in command[:32])
        if self.monitor: self.sender.send(*self.monitor, Message(cue.base + '/ack', (('s', command), ('s', result), ('i', int(cue.playing)), ('s', detail))))

    def handle(self, message, peer):
        self.received.append(message.address)
        if message.address == QUERY: self._query(message, peer); return
        base = next((b for b in sorted(self.cues, key=len, reverse=True) if message.address.startswith(b + '/')), None)
        if base is None: return
        cue = self.cues[base]; op = message.address[len(base) + 1:]
        number, is_float = self._number(message)
        if op in ('state', 'ack'): self._ack(cue, op, 'ignored', 'engine-only message')
        elif op == 'status':
            if message.args: self._ack(cue, op, 'rejected', 'status takes no arguments'); return
            self._ack(cue, op, 'received')
            if self.monitor: self.sender.send(*self.monitor, Message(cue.base + '/state', (('f', float(cue.playing)),)))
        elif op == 'play':
            if message.args and number is None: self._ack(cue, op, 'ignored', 'non-numeric payload'); return
            if message.args and not (number >= 0.5 if is_float else number >= 1): self._ack(cue, op, 'ignored', 'below the start threshold'); return
            self._ack(cue, op, 'received')
            if cue.fail_start: cue.last_error = cue.fail_start; self._ack(cue, op, 'failed', cue.fail_start)
            else: cue.last_error = ''; self._state(cue, True)
        elif op == 'stop': self._state(cue, False); self._ack(cue, op, 'received')
        elif op == 'loop':
            if cue.kind != 'playback': self._ack(cue, op, 'rejected', 'loop does not apply to live inputs'); return
            if message.args and number is None: self._ack(cue, op, 'ignored', 'non-numeric payload'); return
            cue.loop = False if not message.args else (number > 0.5 if is_float else number >= 1)
            self._ack(cue, op, 'received')
        elif op == 'gain':
            if cue.kind != 'live': self._ack(cue, op, 'rejected', 'gain applies only to live inputs'); return
            if number is None or not 0 <= number <= 4: self._ack(cue, op, 'rejected', 'gain needs a finite value in 0..4'); return
            cue.gain = float(number); self._ack(cue, op, 'received')
        else: self._ack(cue, op, 'rejected', 'unknown command')

    # ---- read-only scene discovery (schema v1) ------------------------------------------------------------
    def _rows(self):
        rows = []
        for cue in self.cues.values():
            live = cue.kind == 'live'
            row = {'kind': 'live_input' if live else 'playback', 'key': '/sim/' + cue.base, 'actor_label': 'Sim', 'name': 'LiveInput' if live else 'AudioLoom',
                   'osc_base': cue.base, 'state_address': cue.base + '/state', 'playing': cue.playing, 'last_error': cue.last_error, 'registered': True,
                   'backend_available': True, 'controls': {'play': cue.base + '/play', 'stop': cue.base + '/stop', ('gain' if live else 'loop'): cue.base + ('/gain' if live else '/loop')}}
            row.update({'gain': cue.gain} if live else {'loop': cue.loop}); rows.append(row)
        for i in self.objects:
            rows.append({'kind': 'spatial_binding', 'key': f'/sim/binding/{i}', 'default_object_id': i, 'targets': [{'protocol': 'ADM', 'object_id': i}]})
        return sorted(rows, key=lambda r: r['key'])

    def _query(self, message, peer):
        values = [v for _, v in message.args]
        tags = ''.join(t for t, _ in message.args)
        if tags not in ('si', 'siii') or not REQUEST_ID.match(values[0]) or not 1024 <= values[1] <= 65535: return  # invalid: silent, as the engines are
        offset, limit = (0, 64) if tags == 'si' else (values[2], values[3])
        if not (0 <= offset <= 16384 and 1 <= limit <= 64): return
        rows = self._rows(); page = rows[offset:offset + limit]; end = offset + len(page)
        text = json.dumps({'schema': SCHEMA, 'schema_version': 1, 'engine': 'Simulator', 'world': self.world, 'world_type': 'Game', 'snapshot_utc': datetime.now(timezone.utc).isoformat(),
                           'playback_network': {'listening': True}, 'offset': offset, 'limit': limit, 'returned': len(page), 'total': len(rows), 'next_offset': end if end < len(rows) else -1, 'objects': page})
        chunks, current = [], ''
        for ch in text:
            if len((current + ch).encode('utf-8')) > 900: chunks.append(current); current = ''
            current += ch
        chunks.append(current)
        for index, chunk in enumerate(chunks): self.sender.send(peer[0], values[1], Message(RESPONSE, (('s', values[0]), ('i', index), ('i', len(chunks)), ('s', chunk))))

    def poll(self, maximum=256):
        for event in self.input.poll(maximum):
            if event[0] == 'message': self.handle(event[3], event[2])


def main(argv=None):
    import argparse
    p = argparse.ArgumentParser(description='Simulated engine for testing sender apps. Produces no sound and opens no device.')
    p.add_argument('--listen', default='127.0.0.1:9000'); p.add_argument('--monitor', default='127.0.0.1:9001')
    p.add_argument('--cue', action='append', default=[], help='playback cue base (repeatable)'); p.add_argument('--live', action='append', default=[], help='live-input cue base (repeatable)')
    p.add_argument('--fail-start', action='append', default=[], help='BASE: that cue refuses to start (simulates an invalid device)')
    args = p.parse_args(argv)
    ep = lambda text: (text.rsplit(':', 1)[0], int(text.rsplit(':', 1)[1]))
    failing = set(args.fail_start)
    cues = [SimCue(b, 'playback', 'Simulated device refused to start' if b in failing else '') for b in (args.cue or ['/demo/playback/mono'])] + [SimCue(b, 'live', 'Simulated input unavailable' if b in failing else '') for b in args.live]
    simulator = EngineSimulator(cues); bound = simulator.start(ep(args.listen), ep(args.monitor) if args.monitor else None)
    print(f'Simulated engine on {bound[0]}:{bound[1]} (no audio, no devices). Cues: {", ".join(simulator.cues)}. Ctrl+C to stop.', flush=True)
    try:
        while True: simulator.poll(); time.sleep(0.005)
    except KeyboardInterrupt: pass
    simulator.stop()


if __name__ == '__main__': main()
