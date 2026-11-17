"""UDP transport: exact listen bind, bounded receive queue, no GUI access on worker thread."""
import queue
import socket
import threading
import time
from .model import endpoint
from .protocol import decode, encode, MAX_PACKET


class Sender:
    def __init__(self):
        self.socket = None

    def send(self, ip, port, message):
        target = endpoint(ip, port)
        packet = encode(message)
        if self.socket is None: self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        written = self.socket.sendto(packet, target)
        if written != len(packet): raise OSError('Incomplete UDP datagram send')
        return written  # Accepted by socket, NOT a delivery acknowledgement.

    def close(self):
        if self.socket is not None: self.socket.close(); self.socket = None


class Receiver:
    def __init__(self):
        self.events = queue.Queue(maxsize=2048)
        self.socket = None
        self.thread = None
        self.stop_event = threading.Event()
        self.dropped = 0
        self.bound = None

    def start(self, ip, port):
        self.stop()
        address = endpoint(ip, port, listen=True)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            # Never use reuse; on Windows also protect our binding from later reuse binds.
            if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
            sock.bind(address); sock.settimeout(0.1)
        except Exception:
            sock.close(); raise
        self.socket = sock
        self.bound = sock.getsockname()
        self.stop_event = threading.Event()
        while not self.events.empty():
            try: self.events.get_nowait()
            except queue.Empty: break
        self.thread = threading.Thread(target=self._run, args=(sock, self.stop_event), daemon=True, name='OSC-monitor')
        self.thread.start()
        return self.bound

    def _put(self, event):
        try: self.events.put_nowait(event)
        except queue.Full: self.dropped += 1

    def _run(self, sock, stopped):
        while not stopped.is_set():
            try: packet, peer = sock.recvfrom(MAX_PACKET + 1)
            except socket.timeout: continue
            except OSError: break
            try:
                messages = decode(packet)
                now = time.monotonic()
                for message in messages: self._put(('message', now, peer, message))
            except ValueError as error: self._put(('error', time.monotonic(), peer, str(error)))

    def poll(self, maximum=256):
        result = []
        for _ in range(maximum):
            try: result.append(self.events.get_nowait())
            except queue.Empty: break
        return result

    def stop(self):
        self.stop_event.set()
        if self.socket is not None:
            self.socket.close(); self.socket = None
        if self.thread is not None:
            self.thread.join(timeout=1); self.thread = None
        self.bound = None
