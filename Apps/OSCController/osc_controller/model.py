"""Portable, validated profiles. Loading/saving never sends or starts a listener."""
from dataclasses import asdict, dataclass, field
import ipaddress
import json
import math
from pathlib import Path
import os
import tempfile
from .protocol import valid_address


def endpoint(ip, port, listen=False):
    try: address = ipaddress.IPv4Address(ip)
    except ipaddress.AddressValueError: raise ValueError('Use a literal IPv4 address, e.g. 127.0.0.1') from None
    if address.is_multicast or (address.is_unspecified and not listen) or str(address) == '255.255.255.255':
        raise ValueError('Use unicast IP; 0.0.0.0 is allowed only for an explicit listener')
    if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535: raise ValueError('Port must be 1..65535')
    return str(address), port


CUE_KINDS = ('playback', 'live')


@dataclass
class Cue:
    name: str
    base: str
    kind: str = 'playback'  # 'live' = microphone/line-in cue: /play starts, /stop stops, /gain sets level

    def validate(self):
        if not isinstance(self.name, str) or not self.name.strip() or len(self.name) > 80: raise ValueError('Cue name must be 1..80 characters')
        if not valid_address(self.base) or len(self.base) > 256: raise ValueError('Cue base must be a valid OSC path, <=256 characters')
        if self.kind not in CUE_KINDS: raise ValueError('Cue kind must be playback or live')


@dataclass
class Profile:
    schema_version: int = 1
    playback_ip: str = '127.0.0.1'
    playback_port: int = 9000
    monitor_ip: str = '127.0.0.1'
    monitor_port: int = 9001
    spatial_ip: str = '127.0.0.1'
    spatial_port: int = 50018
    tracker_ip: str = '127.0.0.1'
    tracker_ports: list = field(default_factory=lambda: [50018, 9002])
    query_ip: str = '127.0.0.1'
    query_port: int = 8100
    query_reply_port: int = 9011
    object_id: int = 1
    front: float = 0.0
    right: float = 0.0
    up: float = 0.0
    gain: float = 1.0
    width: float = 0.0
    send_hz: int = 20
    cues: list = field(default_factory=lambda: [Cue('Warm percussion / channel 1', '/demo/playback/mono'),
        Cue('Stereo groove / corresponding channels', '/demo/playback/stereo'), Cue('Glass / channel 2 / replay', '/demo/playback/replay')])

    def validate(self):
        if type(self.schema_version) is not int or self.schema_version != 1: raise ValueError('Unsupported profile schema')
        endpoint(self.playback_ip, self.playback_port)
        endpoint(self.spatial_ip, self.spatial_port)
        endpoint(self.monitor_ip, self.monitor_port, listen=True)
        if not isinstance(self.tracker_ports, list) or not 1 <= len(self.tracker_ports) <= 8: raise ValueError('Choose 1..8 tracker ports')
        for port in self.tracker_ports: endpoint(self.tracker_ip, port, listen=True)
        if len(set(self.tracker_ports)) != len(self.tracker_ports): raise ValueError('Duplicate tracker ports')
        endpoint(self.query_ip, self.query_port)
        endpoint(self.tracker_ip, self.query_reply_port, listen=True)
        if self.query_reply_port < 1024: raise ValueError('Query reply port must be >=1024')
        if type(self.object_id) is not int or not 1 <= self.object_id <= 99999: raise ValueError('Object ID must be 1..99999')
        if type(self.send_hz) is not int or not 1 <= self.send_hz <= 60: raise ValueError('Send rate must be 1..60 Hz')
        for name, lo, hi in [('front', -1, 1), ('right', -1, 1), ('up', -1, 1), ('gain', 0, 2), ('width', 0, 1)]:
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not lo <= value <= hi or not math.isfinite(value):
                raise ValueError(f'{name} must be finite in {lo}..{hi}')
        if not isinstance(self.cues, list) or not 1 <= len(self.cues) <= 64: raise ValueError('Profile must have 1..64 cues')
        bases = []
        for cue in self.cues:
            if not isinstance(cue, Cue): raise ValueError('Invalid cue entry')
            cue.validate(); bases.append(cue.base)
        if len(set(bases)) != len(bases): raise ValueError('Duplicate cue bases: use one entry per OSC base')

    def to_dict(self):
        self.validate()
        return asdict(self)

    @classmethod
    def from_dict(cls, data):
        if not isinstance(data, dict) or set(data) - set(cls.__dataclass_fields__): raise ValueError('Invalid/unknown profile fields')
        values = dict(data)
        if 'cues' in values:
            if not isinstance(values['cues'], list): raise ValueError('Cues must be an array')
            try: values['cues'] = [Cue(**item) for item in values['cues']]
            except (TypeError, KeyError): raise ValueError('Invalid cue fields') from None
        try: result = cls(**values)
        except TypeError: raise ValueError('Invalid profile') from None
        result.validate()
        return result


def load_profile(path):
    path = Path(path)
    if path.stat().st_size > 256 * 1024: raise ValueError('Profile exceeds 256 KiB')
    return Profile.from_dict(json.loads(path.read_text(encoding='utf-8')))


def save_profile(path, profile):
    data = json.dumps(profile.to_dict(), indent=2, allow_nan=False) + '\n'
    path = Path(path)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=path.parent, prefix='.' + path.name, suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name); stream.write(data)
        os.replace(temporary, path)
    finally:
        if temporary and temporary.exists(): temporary.unlink()
