"""Bounded OSC 1.0 codec; no third-party dependency or network side effects."""
from dataclasses import dataclass
import math
import struct

MAX_PACKET = 65507
MAX_MESSAGES = 128
MAX_ARGS = 128


@dataclass(frozen=True)
class Message:
    address: str
    args: tuple = ()  # (type tag, value) pairs; explicit tags avoid bool/int ambiguity.


def valid_address(address):
    return (isinstance(address, str) and address.startswith('/') and len(address) > 1
            and '//' not in address and not address.endswith('/')
            and all(33 <= ord(c) <= 126 and c not in '#*,?[]{}' for c in address))


def _finite(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)): return False
    try: return math.isfinite(value)
    except OverflowError: return False


def _string(value):
    if not isinstance(value, str) or '\0' in value: raise ValueError('OSC strings cannot contain NUL')
    data = value.encode('utf-8') + b'\0'
    return data + b'\0' * (-len(data) % 4)


def encode(message):
    if not valid_address(message.address): raise ValueError('Use an absolute OSC path without spaces, patterns or empty segments')
    if len(message.args) > MAX_ARGS: raise ValueError('Too many OSC arguments')
    values, tags = bytearray(), ','
    for tag, value in message.args:
        tags += tag
        if tag == 'i':
            if isinstance(value, bool) or not isinstance(value, int) or not -(2**31) <= value < 2**31: raise ValueError('int32 out of range')
            values.extend(struct.pack('>i', value))
        elif tag == 'f':
            if not _finite(value): raise ValueError('Expected finite float')
            try: values.extend(struct.pack('>f', value))
            except (OverflowError, struct.error): raise ValueError('Float32 out of range') from None
        elif tag == 's': values.extend(_string(value))
        elif tag in ('T', 'F', 'N'): pass
        else: raise ValueError('Unsupported outgoing tag: ' + str(tag))
    packet = _string(message.address) + _string(tags) + values
    if len(packet) > MAX_PACKET: raise ValueError('OSC packet exceeds UDP limit')
    return packet


def decode(packet):
    if not isinstance(packet, bytes) or not 8 <= len(packet) <= MAX_PACKET or len(packet) % 4:
        raise ValueError('Invalid OSC datagram size')
    messages = []

    def parse(data, depth=0):
        if depth > 8 or len(data) < 8 or len(data) % 4: raise ValueError('Invalid/deep OSC packet')
        if data.startswith(b'#bundle\0'):
            if len(data) < 16: raise ValueError('Truncated bundle')
            pos = 16
            while pos < len(data):
                if pos + 4 > len(data): raise ValueError('Truncated bundle length')
                length = struct.unpack_from('>i', data, pos)[0]; pos += 4
                if length <= 0 or length > len(data) - pos: raise ValueError('Invalid bundle element length')
                parse(data[pos:pos + length], depth + 1); pos += length
            return
        pos = 0

        def string():
            nonlocal pos
            try: end = data.index(0, pos)
            except ValueError: raise ValueError('Unterminated OSC string') from None
            nxt = (end + 4) & ~3
            if nxt > len(data) or any(data[end:nxt]): raise ValueError('Invalid OSC string padding')
            try: value = data[pos:end].decode('utf-8')
            except UnicodeDecodeError: raise ValueError('Invalid UTF-8') from None
            pos = nxt
            return value

        def number(fmt):
            nonlocal pos
            size = struct.calcsize(fmt)
            if pos + size > len(data): raise ValueError('Truncated OSC argument')
            value = struct.unpack_from(fmt, data, pos)[0]; pos += size
            if isinstance(value, float) and not math.isfinite(value): raise ValueError('Non-finite OSC argument')
            return value

        address, tags = string(), string()
        if not valid_address(address) or not tags.startswith(',') or len(tags) > MAX_ARGS + 1:
            raise ValueError('Invalid address/type tags')
        args = []
        for tag in tags[1:]:
            if tag in 'ifhd': value = number({'i': '>i', 'f': '>f', 'h': '>q', 'd': '>d'}[tag])
            elif tag == 's': value = string()
            elif tag in ('T', 'F', 'N'): value = {'T': True, 'F': False, 'N': None}[tag]
            elif tag == 'b':
                size = number('>i')
                if size < 0 or size > len(data) - pos: raise ValueError('Invalid OSC blob')
                end = pos + size; nxt = (end + 3) & ~3
                if nxt > len(data) or any(data[end:nxt]): raise ValueError('Invalid blob padding')
                value = data[pos:end]; pos = nxt
            else: raise ValueError('Unsupported incoming tag: ' + tag)
            args.append((tag, value))
        if pos != len(data): raise ValueError('Unexpected OSC trailing bytes')
        if len(messages) >= MAX_MESSAGES: raise ValueError('Too many bundled messages')
        messages.append(Message(address, tuple(args)))

    parse(packet)  # No partial result escapes a malformed bundle.
    return messages


def parse_arguments(text):
    """One typed argument per line: f 0.5 / i 1 / s some text / T / F / N."""
    args = []
    for line in text.splitlines():
        line = line.strip()
        if not line: continue
        tag, _, value = line.partition(' ')
        if tag == 'f':
            try: parsed = float(value)
            except ValueError: raise ValueError('Invalid float: ' + value) from None
        elif tag == 'i':
            try: parsed = int(value)
            except ValueError: raise ValueError('Invalid integer: ' + value) from None
        elif tag == 's': parsed = value
        elif tag in ('T', 'F', 'N') and not value: parsed = None
        else: raise ValueError('Use f, i, s or a standalone T/F/N on each line')
        args.append((tag, parsed))
    result = tuple(args)
    encode(Message('/validate', result))
    return result


def spatial_messages(object_id, front, right, up, gain=1.0, width=0.0, mute=False):
    if isinstance(object_id, bool) or not isinstance(object_id, int) or not 1 <= object_id <= 99999: raise ValueError('Object ID must be 1..99999')
    values = (front, right, up, gain, width)
    if not all(_finite(v) for v in values): raise ValueError('Spatial values must be finite')
    if not all(-1 <= v <= 1 for v in (front, right, up)) or not 0 <= gain <= 2 or not 0 <= width <= 1:
        raise ValueError('Position -1..1, gain 0..2 and width 0..1 required')
    radius = math.sqrt(front*front + right*right + up*up)
    if radius > 1: front, right, up = (v / radius for v in (front, right, up)); radius = 1
    azimuth = math.degrees(math.atan2(-right, front))
    elevation = math.degrees(math.atan2(up, math.hypot(front, right)))
    base = f'/adm/obj/{object_id}'
    def floats(suffix, *args): return Message(base + suffix, tuple(('f', float(v)) for v in args))
    return [floats('/xyz', right, front, up), floats('/aed', azimuth, elevation, radius),
            floats('/gain', gain), floats('/w', width), Message(base + '/mute', (('i', int(mute)),))]
