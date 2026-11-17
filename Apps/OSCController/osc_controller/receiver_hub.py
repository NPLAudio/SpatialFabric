"""Share exact endpoint listeners between monitor/tracker; start changes transactionally."""
import socket
import time
from .model import endpoint
from .protocol import Message, encode, decode
from .transport import Receiver


class ReceiverHub:
    def __init__(self):self.listeners={};self.owners={}

    def endpoints(self,owner=None):
        return sorted(self.owners.get(owner,set())) if owner else sorted(self.listeners)

    def start(self,owner,targets):
        self.configure({owner:targets})
        return self.endpoints(owner)

    def configure(self,changes):
        owners=dict(self.owners)
        for owner,targets in changes.items():
            targets={endpoint(ip,port,listen=True) for ip,port in targets}
            if not targets:raise ValueError('Choose at least one receive port')
            if len(targets)>8:raise ValueError('At most eight receiver endpoints per owner')
            owners[owner]=targets
        targets=set().union(*owners.values())
        created={}
        try:
            for target in sorted(targets):
                if target not in self.listeners:
                    receiver=Receiver();receiver.start(*target);created[target]=receiver
        except Exception:
            for receiver in created.values():receiver.stop()
            raise
        self.listeners.update(created);self.owners=owners;self._prune()

    def _prune(self):
        used=set().union(*self.owners.values()) if self.owners else set()
        for target in list(self.listeners):
            if target not in used:self.listeners.pop(target).stop()

    def stop_owner(self,owner):self.owners.pop(owner,None);self._prune()

    def poll(self,maximum=512):
        events=[];share=max(1,maximum//max(1,len(self.listeners)))
        for target,receiver in self.listeners.items():
            events.extend((target,event) for event in receiver.poll(share))
        return events

    @property
    def dropped(self):return sum(r.dropped for r in self.listeners.values())

    def close(self):
        self.owners.clear();self._prune()


def local_self_test():
    """An isolated ephemeral-port test, never a Play request or a fixed-port bind."""
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sender:
        receiver.bind(('127.0.0.1',0));receiver.settimeout(.5)
        message=Message('/spatialfabric/controller/self-test',(('s',str(time.monotonic_ns())),))
        sender.sendto(encode(message),receiver.getsockname())
        return decode(receiver.recvfrom(65507)[0])==[message]


def loopback_destination_status(ip,port):
    """A bind probe is diagnostic evidence only, not an OSC identity/acknowledgement."""
    ip,port=endpoint(ip,port)
    import ipaddress
    if not ipaddress.IPv4Address(ip).is_loopback:return 'Remote target: cannot inspect its listener from this PC'
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as probe:
        if hasattr(socket,'SO_EXCLUSIVEADDRUSE'):probe.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
        try:probe.bind((ip,port))
        except OSError as error:
            if error.errno in (98,10048) or getattr(error,'winerror',None)==10048:
                return f'{ip}:{port} is occupied by a local listener (identity not verified)'
            return f'{ip}:{port}: probe inconclusive: {error}'
    return f'{ip}:{port} appears UNBOUND — start Unreal Playback OSC / check its configured listen IP'
