"""Versioned OSC scene discovery, reusable by GUI and command-line clients. No audio commands."""
import ipaddress
import json
import math
import socket
import time
import uuid
from .model import endpoint
from .protocol import Message, encode, decode

QUERY = '/spatialfabric/scene/query'
RESPONSE = '/spatialfabric/scene/response'
SCHEMA = 'org.nplaudio.spatialfabric.scene'
MAX_PAGE_BYTES = 96 * 1024


class SceneQueryError(ValueError): pass


class PageAssembler:
    def __init__(self, request_id, sender_ip):
        self.request_id=request_id;self.sender_ip=sender_ip;self.total=None;self.chunks={};self.bytes=0

    def accept(self,message,peer_ip):
        if message.address!=RESPONSE or peer_ip!=self.sender_ip:return None
        if len(message.args)!=4 or tuple(t for t,_ in message.args)!=('s','i','i','s'):raise SceneQueryError('Invalid response argument types')
        request_id,index,total,chunk=[v for _,v in message.args]
        if request_id!=self.request_id:return None  # Delayed datagrams from an earlier page/retry.
        if type(index) is not int or type(total) is not int or not 1<=total<=128 or not 0<=index<total:raise SceneQueryError('Invalid chunk sequence')
        if not isinstance(chunk,str) or len(chunk.encode('utf-8'))>900:raise SceneQueryError('Oversized/invalid chunk')
        if self.total is not None and self.total!=total:raise SceneQueryError('Chunk count changed within request')
        self.total=total
        if index in self.chunks:
            if self.chunks[index]!=chunk:raise SceneQueryError('Conflicting duplicate chunk')
            return None
        self.bytes+=len(chunk.encode('utf-8'))
        if self.bytes>MAX_PAGE_BYTES:raise SceneQueryError('Page exceeds byte limit')
        self.chunks[index]=chunk
        if len(self.chunks)!=total:return None
        try:
            page=json.loads(''.join(self.chunks[i] for i in range(total)))
            def finite_json(value,depth=0):
                if depth>32:raise ValueError('JSON nesting limit')
                if isinstance(value,float) and not math.isfinite(value):raise ValueError('Non-finite JSON number')
                if isinstance(value,dict):
                    for child in value.values():finite_json(child,depth+1)
                elif isinstance(value,list):
                    for child in value:finite_json(child,depth+1)
            finite_json(page)
        except (ValueError,TypeError,RecursionError):raise SceneQueryError('Reassembled response is not bounded finite JSON') from None
        if not isinstance(page,dict) or page.get('schema')!=SCHEMA or type(page.get('schema_version')) is not int or page['schema_version']!=1:
            raise SceneQueryError('Unsupported scene schema')
        if 'error' in page:raise SceneQueryError('Unreal: '+str(page['error']))
        return page


class SceneQuerySession:
    """GUI-friendly state machine. tick/accept must be called by one thread."""
    def __init__(self,ip,port,reply_port,send,page_size=64):
        self.ip,self.port=endpoint(ip,port);endpoint('127.0.0.1',reply_port)
        if not 1024<=reply_port<=65535 or type(page_size) is not int or not 1<=page_size<=64:raise SceneQueryError('Invalid reply port/page size')
        self.reply_port=reply_port;self.send=send;self.page_size=page_size
        self.offset=0;self.total=None;self.world=None;self.objects=[];self.keys=set();self.page_times=[]
        self.assembler=None;self.deadline=0;self.next_send=0;self.attempt=0;self.done=None;self.error=None;self.bytes=0

    def tick(self,now):
        if self.done is not None or self.error:return
        try:
            if self.assembler is not None and now>=self.deadline:
                if self.attempt>=3:raise SceneQueryError('Scene query timed out: start Unreal OSC on 8100 or 9000; check reply binding, scene-query permissions and firewall')
                self.assembler=None
            if self.assembler is None and now>=self.next_send:
                request_id=uuid.uuid4().hex
                self.assembler=PageAssembler(request_id,self.ip)
                self.send(self.ip,self.port,Message(QUERY,(('s',request_id),('i',self.reply_port),('i',self.offset),('i',self.page_size))))
                self.deadline=now+1.5;self.next_send=now+.3;self.attempt+=1
        except (ValueError,OSError) as error:self.error=str(error)

    def accept(self,message,peer_ip,now):
        if self.assembler is None or self.done is not None or self.error:return
        try:
            page=self.assembler.accept(message,peer_ip)
            if page is None:return
            for name in ('offset','limit','returned','total','next_offset'):
                if type(page.get(name)) is not int:raise SceneQueryError('Invalid page counters')
            rows=page.get('objects')
            if not isinstance(page.get('playback_network'),dict):raise SceneQueryError('Invalid playback network metadata')
            if not isinstance(rows,list) or not isinstance(page.get('world'),str) or page['offset']!=self.offset or page['returned']!=len(rows) or not 1<=page['limit']<=64 or len(rows)>page['limit'] or not 0<=page['total']<=16384:
                raise SceneQueryError('Invalid page shape or offset')
            if self.total is not None and (self.total!=page['total'] or self.world!=page['world']):raise SceneQueryError('Scene/world changed while paging; query again')
            self.total=page['total'];self.world=page['world']
            self.bytes+=self.assembler.bytes
            if self.bytes>8*1024*1024:raise SceneQueryError('Full inventory exceeds 8 MiB')
            for row in rows:
                if not isinstance(row,dict) or not isinstance(row.get('kind'),str) or not isinstance(row.get('key'),str) or not row['key'] or row['key'] in self.keys:
                    raise SceneQueryError('Duplicate/invalid record; scene may have changed while paging')
                self.keys.add(row['key']);self.objects.append(row)
            self.page_times.append(page.get('snapshot_utc'))
            next_offset=page['next_offset']
            if next_offset==-1:
                if len(self.objects)!=self.total:raise SceneQueryError('Incomplete inventory')
                self.done=dict(page,offset=0,returned=len(self.objects),objects=list(self.objects),page_snapshot_times=list(self.page_times),paginated=len(self.page_times)>1)
            elif next_offset!=self.offset+len(rows) or next_offset<=self.offset or next_offset>=self.total:
                raise SceneQueryError('Invalid pagination continuation')
            else:
                self.offset=next_offset;self.assembler=None;self.attempt=0;self.next_send=max(self.next_send,now+.3)
        except (ValueError,OSError) as error:self.error=str(error)


def query_scene(ip='127.0.0.1',port=8100,page_size=64):
    """Synchronous standalone example; replies use this socket's ephemeral port."""
    ip,port=endpoint(ip,port)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024*1024)
        sock.bind(('127.0.0.1' if ipaddress.IPv4Address(ip).is_loopback else '0.0.0.0',0));sock.settimeout(.05)
        session=SceneQuerySession(ip,port,sock.getsockname()[1],lambda host,p,message:sock.sendto(encode(message),(host,p)),page_size)
        while session.done is None and session.error is None:
            session.tick(time.monotonic())
            try:packet,peer=sock.recvfrom(65507)
            except socket.timeout:continue
            try:
                for message in decode(packet):session.accept(message,peer[0],time.monotonic())
            except ValueError:continue  # Malformed/unrelated datagrams cannot become scene data.
        if session.error:raise SceneQueryError(session.error)
        return session.done
