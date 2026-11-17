"""Per-cue command feedback: what was sent, what the engine acknowledged, and its observed state. No Tk, no sockets."""
RESULTS = ('received', 'ignored', 'rejected', 'failed')
NO_ACK_AFTER = 1.5  # seconds before a missing acknowledgement is reported as missing


class CueFeedback:
    def __init__(self):
        self.state = {}   # base -> (playing, received_at) from /state or the playing field of an /ack
        self.ack = {}     # base -> dict(command, result, playing, detail, at)
        self.sent = {}    # base -> (command, at)

    def clear(self): self.state.clear(); self.ack.clear(); self.sent.clear()

    def command_sent(self, base, command, now): self.sent[base] = (command, now)

    def on_message(self, message, now, bases):
        """Consume /state and /ack for known cue bases. Returns a one-line summary for an ack, else None."""
        address, args = message.address, message.args
        if address.endswith('/state') and len(args) == 1 and args[0][0] in ('f', 'i') and address[:-6] in bases and args[0][1] in (0, 1):
            self.state[address[:-6]] = (bool(args[0][1]), now); return None
        if address.endswith('/ack') and address[:-4] in bases:
            if tuple(t for t, _ in args) != ('s', 's', 'i', 's') or args[1][1] not in RESULTS or args[2][1] not in (0, 1): return None
            base = address[:-4]; command, result, playing, detail = [v for _, v in args]
            self.ack[base] = {'command': command, 'result': result, 'playing': bool(playing), 'detail': detail, 'at': now}
            self.state[base] = (bool(playing), now)
            pending = self.sent.get(base)
            if pending and command == pending[0] and now >= pending[1]: self.sent.pop(base)
            return f'{base}: {command} {result.upper()}' + (f' ({detail})' if detail else '')
        return None

    def state_text(self, base, live, now):
        state = self.state.get(base)
        if not state: return 'Unknown — nothing reported yet'
        word = ('LIVE' if live else 'PLAYING') if state[0] else 'stopped'
        return f'{word} · {max(0, now - state[1]):.0f}s ago'

    def ack_text(self, base, now, monitoring):
        sent, ack = self.sent.get(base), self.ack.get(base)
        if sent and (not ack or ack['at'] < sent[1] or ack['command'] != sent[0]):
            if not monitoring: return f'sent {sent[0]} — start the monitor to see acknowledgements'
            age = now - sent[1]
            if age < NO_ACK_AFTER: return f'sent {sent[0]} … waiting for acknowledgement'
            return f'NO ACKNOWLEDGEMENT for {sent[0]} after {age:.0f}s — engine not running, wrong port, or no ack support'
        if not ack: return '—'
        text = f'{ack["command"]}: {ack["result"].upper()}'
        if ack['detail']: text += f' — {ack["detail"]}'
        return f'{text} · {max(0, now - ack["at"]):.0f}s ago'
