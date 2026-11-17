"""Sender-app debugger / test bed: what an app sends vs. what the running engine expects and does.

Pure validation (Validator) plus a poll-driven Debugger that owns its sockets. Nothing binds, sends or
forwards until you call start/configure; forwarding to an engine is a separate explicit switch because a
forwarded /play can start real playback or open a microphone on the engine machine.
"""
import difflib
import json
import math
import re
import socket
import time
from collections import deque
from dataclasses import dataclass, field
from .model import endpoint
from .protocol import MAX_PACKET, Message, decode, encode
from .scene_query import RESPONSE, SceneQueryError, SceneQuerySession
from .transport import Receiver, Sender

SEVERITY_ORDER = {'ok': 0, 'info': 1, 'warn': 2, 'error': 3}
NUMERIC_TAGS = ('f', 'i')
SCENE_QUERY = '/spatialfabric/scene/query'
ADM = re.compile(r'^/adm/obj/([^/]+)/(xyz|aed|gain|w|mute|name)$')
REQUEST_ID = re.compile(r'^[A-Za-z0-9_-]{1,64}$')
PLAYBACK_OPS = ('play', 'stop', 'loop', 'status')
LIVE_OPS = ('play', 'stop', 'gain', 'status')
ALL_OPS = ('play', 'stop', 'loop', 'gain', 'status')   # used when the base is not (yet) verified against an engine
SETTLE_SECONDS = 0.5      # let the engine process a forwarded command before re-reading its state
EXPECT_TIMEOUT = 3.0


@dataclass(frozen=True)
class Finding:
    severity: str   # ok | info | warn | error
    code: str
    text: str


def _tags(message): return ''.join(tag for tag, _ in message.args)


def _first_number(message):
    """AudioLoom rule shared by every engine: the first float anywhere wins over the first int."""
    for tag, value in message.args:
        if tag == 'f': return value
    for tag, value in message.args:
        if tag == 'i': return value
    return None


class Contract:
    """What a given engine accepts. 'defaults' knows only the wire contract; 'engine' also knows real cues/IDs."""
    def __init__(self, cues=None, objects=None, source='defaults', world='', rows=None):
        self.cues = cues or {}            # base -> 'playback' | 'live'
        self.objects = objects            # set[int] of object IDs the engine binds, or None if unknown
        self.source = source; self.world = world; self.rows = rows or {}

    @classmethod
    def from_scene(cls, scene):
        cues, rows, objects = {}, {}, set()
        for row in scene.get('objects', []):
            kind, base = row.get('kind'), row.get('osc_base')
            if kind in ('playback', 'live_input') and isinstance(base, str):
                cues[base] = 'live' if kind == 'live_input' else 'playback'; rows[base] = row
            elif kind in ('spatial_binding', 'binding'):   # Unreal says spatial_binding; Unity and Godot say binding
                for key in ('default_object_id',):
                    if type(row.get(key)) is int: objects.add(row[key])
                for target in row.get('targets', []) or []:
                    if isinstance(target, dict) and type(target.get('object_id')) is int: objects.add(target['object_id'])
        return cls(cues, objects or None, 'engine', str(scene.get('world', '')), rows)

    def addresses(self):
        return [f'{base}/{op}' for base, kind in self.cues.items() for op in (LIVE_OPS if kind == 'live' else PLAYBACK_OPS)]


@dataclass
class Verdict:
    findings: list
    intent: tuple = None   # (base, op, value) for effect verification

    @property
    def severity(self): return max((f.severity for f in self.findings), key=SEVERITY_ORDER.get, default='ok')


class Validator:
    def __init__(self, contract=None, rate_warn_hz=120):
        self.contract = contract or Contract(); self.rate_warn_hz = rate_warn_hz
        self.stats = {}; self._times = {}; self._warned_second = {}

    # ---- message-level rules -------------------------------------------------------------------------
    def check(self, message, now):
        findings = []; intent = None
        add = lambda severity, code, text: findings.append(Finding(severity, code, text))
        address, tags = message.address, _tags(message)
        self._record(message, now, add)
        bad_tags = [t for t in tags if t not in 'ifs']
        if bad_tags: add('warn', 'tag-unsupported', f'Type tags {sorted(set(bad_tags))} are outside i/f/s; engines read only int32, float32 and string (booleans are not a Play/Loop command).')
        if address == SCENE_QUERY: self._scene_query(message, add)
        elif address == RESPONSE: add('warn', 'engine-only', 'Scene responses are sent BY the engine; a sender app should not emit them.')
        elif ADM.match(address): self._adm(message, add)
        else:
            intent = self._cue(message, add)
            if intent and self.contract.cues.get(intent[0]) is None: intent = None   # nothing to verify against without an engine cue
        if not any(f.severity in ('warn', 'error') for f in findings) and not findings: add('ok', 'ok', 'Conforms to the contract.')
        return Verdict(findings, intent)

    def _record(self, message, now, add):
        stat = self.stats.setdefault(message.address, {'count': 0, 'first': now, 'last': now, 'types': set()}) if (message.address in self.stats or len(self.stats) < 1024) else None
        if stat is None: return
        signature = _tags(message)
        if stat['types'] and signature not in stat['types']: add('info', 'type-drift', f'Argument types changed for this address: was {sorted(x or "(no args)" for x in stat["types"])}, now "{signature or "(no args)"}".')
        stat['types'].add(signature); stat['count'] += 1; stat['last'] = now
        times = self._times.setdefault(message.address, deque(maxlen=512)); times.append(now)
        recent = sum(1 for t in times if now - t <= 1.0)
        second = int(now)
        if recent > self.rate_warn_hz and self._warned_second.get(message.address) != second:
            self._warned_second[message.address] = second
            add('warn', 'rate', f'{recent} messages in the last second to this address (> {self.rate_warn_hz}); the engine drains a bounded queue and may drop.')

    def _scene_query(self, message, add):
        tags = _tags(message); a = [v for _, v in message.args]
        if tags not in ('si', 'siii'): add('error', 'query-shape', 'Scene query needs ",si" (request id, reply port) or ",siii" (+ offset, limit).'); return
        if not REQUEST_ID.match(a[0]): add('error', 'query-id', 'Request ID must be 1..64 characters of A-Z a-z 0-9 _ -; the engine stays silent otherwise.')
        if not 1024 <= a[1] <= 65535: add('error', 'query-port', 'Reply port must be 1024..65535; the engine stays silent otherwise.')
        if tags == 'siii':
            if not 0 <= a[2] <= 16384: add('error', 'query-offset', 'Offset must be 0..16384.')
            if not 1 <= a[3] <= 64: add('error', 'query-limit', 'Limit must be 1..64.')

    def _adm(self, message, add):
        object_id, suffix = ADM.match(message.address).groups()
        if not object_id.isdigit() or not 1 <= int(object_id) <= 2147483647:
            add('error', 'adm-id', f'Object ID "{object_id}" must be a positive integer.'); return
        if self.contract.objects is not None and int(object_id) not in self.contract.objects:
            add('warn', 'adm-unbound', f'Object {object_id} is not an ID the running engine binds (known: {sorted(self.contract.objects)[:8]}).')
        tags = _tags(message); values = [v for _, v in message.args]
        want = {'xyz': 'fff', 'aed': 'fff', 'gain': 'f', 'w': 'f', 'mute': 'i', 'name': 's'}[suffix]
        if tags != want:
            add('error', 'adm-types', f'/{suffix} takes "{want}" but got "{tags}".'); return
        if suffix == 'xyz':
            if any(not math.isfinite(v) for v in values): add('error', 'adm-nonfinite', 'Non-finite coordinate.')
            elif math.sqrt(sum(v * v for v in values)) > 1.0 + 1e-6: add('warn', 'adm-sphere', 'Position lies outside the unit sphere; renderers limit it to radius 1 (the engine does the same).')
            if any(abs(v) > 1.0 for v in values): add('warn', 'adm-range', 'xyz components are right/front/up in -1..1.')
        elif suffix == 'aed':
            azimuth, elevation, radius = values
            if not -180 <= azimuth <= 180 or not -90 <= elevation <= 90 or not 0 <= radius <= 1: add('warn', 'adm-aed-range', 'aed is azimuth -180..180 (positive left), elevation -90..90, radius 0..1.')
        elif suffix == 'gain' and not (math.isfinite(values[0]) and values[0] >= 0): add('warn', 'adm-gain', 'Gain must be finite and >= 0.')
        elif suffix == 'w' and not (math.isfinite(values[0]) and 0 <= values[0] <= 1): add('warn', 'adm-width', 'Width must be 0..1.')
        elif suffix == 'mute' and values[0] not in (0, 1): add('warn', 'adm-mute', 'Mute is int 0 or 1.')

    def _cue(self, message, add):
        address = message.address
        base, _, op = address.rpartition('/')
        kind = self.contract.cues.get(base)
        if kind is None:
            known = self.contract.addresses()
            if known:
                near = difflib.get_close_matches(address, known, n=1, cutoff=0.6)
                add('error', 'unknown-address', f'No such cue address in the running engine ({self.contract.world or "inventory"}).' + (f' Did you mean {near[0]}?' if near else ''))
            elif op in ALL_OPS: add('info', 'unverified-base', f'"{base}" is unverified: load the engine contract (scene query) to check it against real cues. Payload rules are still checked.')
            else:
                add('warn', 'unknown-address', 'Not a recognized SpatialFabric/AudioLoom address (play/stop/loop/gain, /adm/obj/..., scene query).'); return None
        ops = LIVE_OPS if kind == 'live' else PLAYBACK_OPS if kind == 'playback' else ALL_OPS
        if op in ('state', 'ack'): add('warn', 'engine-only', f'/{op} is sent BY the engine; the engine ignores it as input.'); return None
        if op not in ops:
            near = difflib.get_close_matches(op, ops, n=1, cutoff=0.4)
            hint = {'loop': 'Loop does not apply to a live input.', 'gain': 'Gain applies only to live-input cues.'}.get(op, '')
            add('error', 'unknown-op', f'"{op}" is not a command for a {kind} cue (valid: {", ".join(ops)}). {hint}' + (f' Did you mean /{near[0]}?' if near else '')); return None
        number = _first_number(message); has_args = bool(message.args)
        if op == 'status':
            if has_args: add('error', 'status-args', 'Status takes no arguments.'); return None
            add('ok', 'engine-action', 'Engine: acknowledges status and reports its observed state.'); return None
        if op == 'stop':
            if has_args: add('info', 'args-ignored', 'Stop ignores its arguments.')
            add('ok', 'engine-action', 'Engine: stops and cancels any pending replay.'); return (base, 'stop', None)
        if op == 'play':
            if not has_args: add('ok', 'engine-action', 'Engine: submits a start request (address-only starts).'); return (base, 'play', True)
            if number is None: add('warn', 'nonnumeric', 'Non-numeric payload: the engine will NOT start (use no args, float >= 0.5 or int >= 1).'); return None
            starts = (number >= 0.5) if isinstance(number, float) else (number >= 1)
            add('ok' if starts else 'info', 'engine-action', 'Engine: submits a start request.' if starts else 'Engine: ignores it (float < 0.5 / int < 1 does not start).')
            return (base, 'play', True) if starts else None
        if op == 'loop':
            if not has_args: add('info', 'engine-action', 'Engine: address-only /loop DISABLES looping.'); return (base, 'loop', False)
            if number is None: add('warn', 'nonnumeric', 'Non-numeric payload: the engine ignores it (use float or int).'); return None
            enable = (number > 0.5) if isinstance(number, float) else (number >= 1)
            add('ok', 'engine-action', f'Engine: loop {"ON" if enable else "OFF"} (float must be strictly > 0.5 to enable).'); return (base, 'loop', enable)
        if op == 'gain':
            if number is None: add('warn', 'gain-missing', 'Gain needs a numeric argument; the engine ignores this.'); return None
            if isinstance(number, float) and not math.isfinite(number) or not 0 <= number <= 4: add('warn', 'gain-range', f'Gain {number} is outside 0..4; the engine IGNORES it.'); return None
            add('ok', 'engine-action', f'Engine: sets live gain to {float(number):g}.'); return (base, 'gain', float(number))


@dataclass
class Record:
    time: float
    kind: str           # message | malformed | engine-state | effect | info
    peer: str
    address: str = ''
    args: str = ''
    findings: list = field(default_factory=list)
    forwarded: bool = False
    raw: str = ''

    @property
    def severity(self): return max((f.severity for f in self.findings), key=SEVERITY_ORDER.get, default='ok')


class _RawReceiver(Receiver):
    """Receiver that keeps the bytes of rejected datagrams so malformed packets can be shown."""
    def _run(self, sock, stopped):
        while not stopped.is_set():
            try: packet, peer = sock.recvfrom(MAX_PACKET + 1)
            except socket.timeout: continue
            except OSError: break
            now = time.monotonic()
            try:
                for message in decode(packet): self._put(('message', now, peer, message))
            except ValueError as error: self._put(('error', now, peer, str(error), packet[:96].hex()))


def _free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0)); return probe.getsockname()[1]


def format_args(message):
    return '  '.join(f'{t}:{v:.6g}' if isinstance(v, float) else f'{t}:{v}' for t, v in message.args)[:300]


class Debugger:
    """Poll-driven; call poll(time.monotonic()) from a GUI timer or a loop."""
    def __init__(self):
        self.listen = _RawReceiver(); self.monitor = _RawReceiver(); self.reply = _RawReceiver(); self.sender = Sender()
        self.validator = Validator(); self.records = deque(maxlen=5000)
        self.counts = {s: 0 for s in SEVERITY_ORDER}; self.forward_to = None; self.query_target = None; self.reply_port = None
        self.session = None; self.refresh_reason = None; self.expectations = []; self.effects = []
        self.state_seen = {}; self.started = None; self.total = 0; self.malformed = 0; self.forwarded = 0; self._pending = []

    # ---- lifecycle -----------------------------------------------------------------------------------
    def start(self, listen, monitor=None, query=None, reply_port=None, forward=None):
        """All-or-nothing bind. forward is an explicit destination; it can never equal the listen endpoint."""
        listen = endpoint(*listen, listen=True)
        if forward is not None:
            forward = endpoint(*forward)
            if forward == listen or (forward[1] == listen[1] and forward[0] in ('127.0.0.1', listen[0])): raise ValueError('Forward destination equals the listen endpoint (that would loop)')
        self.stop()
        try:
            self.listen.start(*listen)
            if monitor: self.monitor.start(*monitor)
            if query:
                self.query_target = endpoint(*query); self.reply_port = reply_port or _free_port()
                import ipaddress  # Replies come from the engine: bind loopback only for a local engine.
                self.reply.start('127.0.0.1' if ipaddress.IPv4Address(self.query_target[0]).is_loopback else '0.0.0.0', self.reply_port); self.reply_port = self.reply.bound[1]
        except Exception:
            self.stop(); raise
        self.forward_to = forward; self.started = time.monotonic()

    def stop(self):
        for receiver in (self.listen, self.monitor, self.reply): receiver.stop()
        self.sender.close(); self.session = None; self.forward_to = None; self.query_target = None

    @property
    def listening(self): return self.listen.bound is not None

    # ---- engine contract -----------------------------------------------------------------------------
    def refresh_contract(self, reason='manual'):
        """Explicit read-only scene query (never starts audio or changes configuration)."""
        if not self.query_target: raise ValueError('Set the engine query endpoint first')
        if self.session is not None and self.session.done is None and not self.session.error: return
        ip, port = self.query_target
        self.session = SceneQuerySession(ip, port, self.reply_port, lambda host, p, m: self.sender.send(host, p, m))
        self.refresh_reason = reason

    def _finish_session(self, now):
        session = self.session
        if session is None or (session.done is None and not session.error): return
        self.session = None
        if session.error:
            self._note('info', 'contract-failed', f'Engine contract not loaded: {session.error}'); return
        self.validator.contract = Contract.from_scene(session.done)
        if self.refresh_reason != 'verify': self._note('info', 'contract', f'Engine contract loaded: {len(self.validator.contract.cues)} cue(s), objects {sorted(self.validator.contract.objects or [])[:8]} in {self.validator.contract.world}')
        self._evaluate(now)

    # ---- polling -------------------------------------------------------------------------------------
    def poll(self, now=None):
        now = time.monotonic() if now is None else now; fresh = []
        if self.session is not None: self.session.tick(now)
        for event in self.reply.poll():
            if event[0] == 'message' and self.session is not None and event[3].address == RESPONSE: self.session.accept(event[3], event[2][0], event[1])
        self._finish_session(now)
        for event in self.monitor.poll():
            if event[0] != 'message': continue
            message = event[3]
            if message.address.endswith('/ack'):
                valid = _tags(message) == 'ssis' and message.args[1][1] in ('received', 'ignored', 'rejected', 'failed') and message.args[2][1] in (0, 1)
                if valid:
                    command, result, playing, detail = [v for _, v in message.args]
                    self.state_seen[message.address[:-4]] = (bool(playing), event[1])
                    severity = 'error' if result == 'failed' else 'warn' if result == 'rejected' else 'info'
                    finding = Finding(severity, 'ack-' + result, f'{command}: {result}' + (f' — {detail}' if detail else ''))
                else: finding = Finding('warn', 'ack-invalid', 'Malformed engine acknowledgement (expected ,ssis command result playing detail).')
                fresh.append(self._add(Record(event[1], 'engine-ack', f'{event[2][0]}:{event[2][1]}', message.address, format_args(message), [finding])))
            elif message.address.endswith('/state') and len(message.args) == 1 and message.args[0][0] in ('f', 'i') and message.args[0][1] in (0, 1):
                base = message.address[:-6]; self.state_seen[base] = (bool(message.args[0][1]), now)
                fresh.append(self._add(Record(now, 'engine-state', f'{event[2][0]}:{event[2][1]}', message.address, format_args(message), [Finding('info', 'state', f'Engine reports {"LIVE/PLAYING" if message.args[0][1] else "stopped"} (observed worker state).')])))
        for event in self.listen.poll(512):
            peer = f'{event[2][0]}:{event[2][1]}'
            if event[0] == 'error':
                self.malformed += 1
                fresh.append(self._add(Record(event[1], 'malformed', peer, '', '', [Finding('error', 'malformed', f'Datagram rejected: {event[3]}. The engine drops it silently.')], raw=event[4])))
                continue
            message = event[3]; verdict = self.validator.check(message, event[1]); record = Record(event[1], 'message', peer, message.address, format_args(message), verdict.findings)
            if self.forward_to is not None and verdict.severity != 'error':
                try: self.sender.send(*self.forward_to, message); record.forwarded = True; self.forwarded += 1
                except (OSError, ValueError) as error: record.findings.append(Finding('error', 'forward-failed', str(error)))
            if record.forwarded and verdict.intent and self.query_target: self.expectations.append({'intent': verdict.intent, 'at': event[1] + SETTLE_SECONDS, 'deadline': event[1] + EXPECT_TIMEOUT, 'sent': event[1], 'requested': False})
            fresh.append(self._add(record))
        for pending in self.expectations:
            if not pending['requested'] and now >= pending['at'] and self.query_target:
                pending['requested'] = True; self.refresh_contract('verify')
        if self.expectations and self.session is None: self._evaluate(now)
        drained, self._pending = self._pending, []   # includes engine-effect and contract notes, not just packets
        return drained

    def _evaluate(self, now):
        remaining = []
        for pending in self.expectations:
            base, op, value = pending['intent']
            if not pending['requested'] or now < pending['at'] : remaining.append(pending); continue
            row = self.validator.contract.rows.get(base)
            if row is None:
                self._effect(now, 'error', 'effect-missing', f'{base}: the engine no longer lists this cue.'); continue
            state = self.state_seen.get(base)
            if op == 'play':
                if (state and state[0] and state[1] >= pending['sent']) or row.get('playing'): self._effect(now, 'ok', 'effect-confirmed', f'{base}/play: engine reports it is running.')
                elif now < pending['deadline']:
                    pending['at'] = now + 0.5; pending['requested'] = False; remaining.append(pending); continue  # re-read the engine shortly, not in a hot loop
                else:
                    why = [f'{k}={row[k]!r}' for k in ('last_error', 'backend_available', 'backend_status', 'pcm_prepared', 'registered', 'worker_state') if k in row and row[k] not in ('', None)]
                    self._effect(now, 'warn', 'effect-unconfirmed', f'{base}/play: the engine never reported running within {EXPECT_TIMEOUT:g}s. Engine says: ' + (', '.join(why) or 'no diagnostic fields'))
            elif op == 'stop':
                self._effect(now, 'error' if row.get('playing') else 'ok', 'effect-stop', f'{base}/stop: engine reports {"STILL running" if row.get("playing") else "not running"}.')
            elif op == 'loop':
                got = row.get('loop'); self._effect(now, 'ok' if got == value else 'error', 'effect-loop', f'{base}/loop: sent {value}, engine reports loop={got!r}.')
            elif op == 'gain':
                got = row.get('gain'); good = isinstance(got, (int, float)) and abs(got - value) <= 1e-3
                self._effect(now, 'ok' if good else 'error', 'effect-gain', f'{base}/gain: sent {value:g}, engine reports gain={got!r}.')
        self.expectations = remaining

    def _effect(self, now, severity, code, text):
        self.effects.append((code, severity, text)); self._add(Record(now, 'effect', 'engine', findings=[Finding(severity, code, text)]))

    def _note(self, severity, code, text): self._add(Record(time.monotonic(), 'info', 'debugger', findings=[Finding(severity, code, text)]))

    def _add(self, record):
        self.records.append(record); self._pending.append(record); self.total += 1; self.counts[record.severity] += 1; return record

    # ---- reporting -----------------------------------------------------------------------------------
    def report(self, last=200):
        contract = self.validator.contract
        return {'schema': 'org.nplaudio.spatialfabric.debugger', 'schema_version': 1,
                'contract': {'source': contract.source, 'world': contract.world, 'cues': contract.cues, 'objects': sorted(contract.objects) if contract.objects is not None else None},
                'totals': {'records': self.total, 'malformed': self.malformed, 'forwarded': self.forwarded, **self.counts},
                'addresses': {a: {'count': s['count'], 'types': sorted(s['types'])} for a, s in sorted(self.validator.stats.items())},
                'effects': [{'code': c, 'severity': s, 'text': t} for c, s, t in self.effects],
                'records': [{'t': round(r.time - (self.started or 0), 3), 'kind': r.kind, 'peer': r.peer, 'address': r.address, 'args': r.args, 'forwarded': r.forwarded, 'raw': r.raw,
                             'findings': [{'severity': f.severity, 'code': f.code, 'text': f.text} for f in r.findings]} for r in list(self.records)[-last:]]}

    def worst(self): return max((r.severity for r in self.records), key=SEVERITY_ORDER.get, default='ok')


def main(argv=None):
    import argparse, sys
    p = argparse.ArgumentParser(description='Verify what a sender app emits against the OSC contract and a running engine.')
    p.add_argument('--listen', default='127.0.0.1:9400', help='where your sender app should send (default 127.0.0.1:9400)')
    p.add_argument('--engine-query', help='engine scene-query input ip:port (read-only) to load its real cues/IDs, e.g. 127.0.0.1:9000')
    p.add_argument('--monitor', help='ip:port to receive the engine /state replies, e.g. 127.0.0.1:9001')
    p.add_argument('--forward', help='ip:port of the engine input to FORWARD to (may start playback/microphone there). Off by default.')
    p.add_argument('--duration', type=float, default=30.0); p.add_argument('--report'); p.add_argument('--fail-on', choices=('warn', 'error'), default='error')
    args = p.parse_args(argv)
    ep = lambda text: (text.rsplit(':', 1)[0], int(text.rsplit(':', 1)[1]))
    debugger = Debugger()
    debugger.start(ep(args.listen), monitor=ep(args.monitor) if args.monitor else None, query=ep(args.engine_query) if args.engine_query else None, forward=ep(args.forward) if args.forward else None)
    print(f'Debugger listening on {args.listen} for {args.duration:g}s. Point the sender app here.', flush=True)
    if args.engine_query: debugger.refresh_contract('startup')
    end = time.monotonic() + args.duration
    try:
        while time.monotonic() < end:
            for record in debugger.poll():
                worst = record.severity
                print(f'[{worst.upper():5}] {record.kind:12} {record.address} {record.args}  ' + ' | '.join(f.text for f in record.findings if f.severity != 'ok' or len(record.findings) == 1), flush=True)
            time.sleep(0.01)
    except KeyboardInterrupt: pass
    debugger.poll(); worst = debugger.worst(); report = debugger.report(); debugger.stop()
    if args.report:
        with open(args.report, 'w', encoding='utf-8') as stream: json.dump(report, stream, indent=2)
    print(f'Done: {report["totals"]}')
    return 1 if SEVERITY_ORDER[worst] >= SEVERITY_ORDER[args.fail_on] else 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
