"""Bounded, receive-only spatial state. Internal axes: front, right, up."""
from collections import deque
from dataclasses import dataclass, field
import math
import re

PALETTE = ('#54d8f5','#ffbf69','#bf9aff','#66e0ad','#ff809a','#acd474','#87aaff','#ff9f70')


@dataclass
class TrackedObject:
    key: str
    protocol: str
    object_id: str
    peer: str
    port: int
    name: str = ''
    position: tuple | None = None
    position_time: float = 0.0
    last_xyz: float = -math.inf
    gain: float = 1.0
    width: float = 0.0
    mute: bool = False
    seen: float = 0.0
    source: str = 'osc'
    trail: deque = field(default_factory=lambda: deque(maxlen=240))
    samples: deque = field(default_factory=lambda: deque(maxlen=32))

    @property
    def color(self): return PALETTE[int(self.object_id) % len(PALETTE)]

    def rate(self, now):
        samples = [t for t in self.samples if now-t < 2]
        return (len(samples)-1)/(samples[-1]-samples[0]) if len(samples)>1 and samples[-1]>samples[0] else 0.0

    @property
    def aed(self):
        if self.position is None: return (0.,0.,0.)
        front,right,up=self.position
        return (math.degrees(math.atan2(-right,front)), math.degrees(math.atan2(up,math.hypot(front,right))), math.hypot(front,right,up))


class ObjectTracker:
    def __init__(self, maximum=256):
        self.objects={};self.maximum=maximum;self.accepted=0;self.ignored=0

    def clear(self): self.objects.clear();self.accepted=self.ignored=0

    def ingest(self,message,peer,port,now):
        adm=re.fullmatch(r'/adm/obj/([1-9]\d{0,9})/(xyz|aed|gain|w|mute|name)',message.address)
        custom=re.fullmatch(r'/demo/[^/]{1,80}/([1-9]\d{0,9})/xyz',message.address)
        if adm: object_id,kind=adm.groups();protocol='ADM'
        elif custom: object_id=custom[1];kind='xyz';protocol='Custom'
        else: self.ignored+=1;return False
        values=[v for _,v in message.args]
        numeric=all(tag in ('f','i','d','h') and isinstance(v,(int,float)) and not isinstance(v,bool) and math.isfinite(v) for tag,v in message.args)
        if kind in ('xyz','aed'):
            if not numeric or len(values)!=3 or any(abs(v)>1e6 for v in values): self.ignored+=1;return False
            if kind=='aed' and values[2]<0: self.ignored+=1;return False
        elif kind in ('gain','w','mute'):
            if not numeric or len(values)!=1: self.ignored+=1;return False
        elif kind=='name' and (len(values)!=1 or message.args[0][0]!='s'):
            self.ignored+=1;return False
        key=f'{protocol}:{peer}:{port}:{object_id}'
        if key not in self.objects:
            if len(self.objects)>=self.maximum:
                self.objects.pop(min(self.objects,key=lambda k:self.objects[k].seen))
            self.objects[key]=TrackedObject(key,protocol,object_id,peer,port)
        obj=self.objects[key];obj.seen=now;self.accepted+=1
        if kind=='name':obj.name=values[0][:80]
        elif kind=='gain':obj.gain=values[0]
        elif kind=='w':obj.width=values[0]
        elif kind=='mute':obj.mute=bool(values[0])
        elif kind=='aed':
            if now-obj.last_xyz<1:return True  # Both mode: prefer XYZ; don't double rate/trail.
            az,el,radius=values;az=math.radians(az);el=math.radians(el)
            self._position(obj,(radius*math.cos(el)*math.cos(az),-radius*math.cos(el)*math.sin(az),radius*math.sin(el)),now)
        elif protocol=='ADM':
            right,front,up=values;obj.last_xyz=now;self._position(obj,(front,right,up),now)
        else:self._position(obj,tuple(values),now)  # Example Custom: stage-normalized front/right/up.
        return True

    def apply_scene(self,scene,sender_ip,now):
        # Query snapshots are explicit Scene objects, never fabricated OSC telemetry.
        self.objects={k:o for k,o in self.objects.items() if o.protocol!='Scene'}
        for row in scene.get('objects',[]):
            point=row.get('stage_normalized')
            if row.get('kind')!='spatial_binding' or not isinstance(point,list) or len(point)!=3:continue
            if not all(type(v) in (int,float) and -1<=v<=1 and math.isfinite(v) for v in point):continue
            if len(self.objects)>=self.maximum:self.objects.pop(min(self.objects,key=lambda k:self.objects[k].seen))
            raw_id=row.get('default_object_id',1)
            if type(raw_id) is not int or not 1<=raw_id<=2147483647:raw_id=1
            key='Scene:'+row['key'];object_id=str(raw_id)
            name=row.get('label','');name=name[:80] if isinstance(name,str) else ''
            obj=TrackedObject(key,'Scene',object_id,sender_ip,0,name=name,source='scene_query',seen=now)
            obj.position=tuple(point);obj.position_time=now;obj.trail.append((now,*point))
            gain,width=row.get('gain',1.0),row.get('width',0.0)
            obj.gain=gain if type(gain) in (int,float) and 0<=gain<=2 else 1.0
            obj.width=width if type(width) in (int,float) and 0<=width<=1 else 0.0
            obj.mute=bool(row.get('mute',False))
            self.objects[key]=obj

    def _position(self,obj,point,now):
        obj.source='osc'
        obj.position=point;obj.position_time=now;obj.samples.append(now)
        if not obj.trail or obj.trail[-1][1:]!=point:obj.trail.append((now,*point))


def project(point,mode):
    front,right,up=point
    if mode=='Top':return right,front
    if mode=='Front':return right,up
    if mode=='Side':return front,up
    # Fixed isometric view, front/back depth stays visible without a 3D library.
    return (right-front)*0.70710678,up*0.81649658+(right+front)*0.40824829
