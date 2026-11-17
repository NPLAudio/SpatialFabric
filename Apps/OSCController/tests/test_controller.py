import copy
import json
import math
from pathlib import Path
import socket
import struct
import tempfile
import time
import unittest
from osc_controller.protocol import Message, encode, decode, parse_arguments, spatial_messages
from osc_controller.model import Cue, Profile, endpoint, load_profile, save_profile
from osc_controller.transport import Sender, Receiver


class ProtocolTests(unittest.TestCase):
    def test_numeric_string_bool_and_address_only_roundtrip(self):
        for message in [Message('/cue/stop'), Message('/cue/play', (('f',1.0),('i',-42),('s','Hello 世界'),('T',None),('F',None),('N',None)))]:
            decoded=decode(encode(message))[0]
            self.assertEqual(decoded.address,message.address)
            self.assertEqual([t for t,_ in decoded.args],[t for t,_ in message.args])
        self.assertEqual(encode(Message('/sf',(('f',1.0),))),b'/sf\0,f\0\0\x3f\x80\0\0')

    def test_rejects_bad_addresses_ranges_and_floats(self):
        for address in ('cue','/cue/','/a//b','/a b','/x*','/x{y}','/☃'):
            with self.subTest(address=address),self.assertRaises(ValueError):encode(Message(address))
        for value in (float('nan'),float('inf'),1e300,10**1000,True):
            with self.subTest(value=str(value)[:30]),self.assertRaises(ValueError):encode(Message('/x',(('f',value),)))
        for value in (2**31,-2**31-1,True,'1'):
            with self.assertRaises(ValueError):encode(Message('/x',(('i',value),)))
        with self.assertRaises(ValueError):encode(Message('/x',(('s','bad\0value'),)))

    def test_all_truncated_prefixes_and_bad_padding(self):
        packet=encode(Message('/sf',(('f',1.),)))
        for length in range(len(packet)):
            with self.subTest(length=length),self.assertRaises(ValueError):decode(packet[:length])
        bad=bytearray(packet);bad[7]=1
        with self.assertRaises(ValueError):decode(bytes(bad))
        with self.assertRaises(ValueError):decode(packet+b'\0\0\0\0')

    def test_bundles_are_atomic_and_bounded(self):
        packet=encode(Message('/sf',(('f',1.),)))
        def bundle(value):return b'#bundle\0'+struct.pack('>Q',1)+struct.pack('>i',len(value))+value
        self.assertEqual(decode(bundle(packet)),[Message('/sf',(('f',1.),))])
        bad=bytearray(packet);bad[5]=ord('q')
        with self.assertRaises(ValueError):decode(bundle(packet)+struct.pack('>i',len(bad))+bad)
        nested=packet
        for _ in range(9):nested=bundle(nested)
        with self.assertRaises(ValueError):decode(nested)
        many=b'#bundle\0'+struct.pack('>Q',1)+(struct.pack('>i',len(packet))+packet)*129
        with self.assertRaises(ValueError):decode(many)

    def test_extended_incoming_types_and_forged_blob(self):
        packet=b'/sf\0,hd\0'+struct.pack('>qd',123456789,0.25)
        self.assertEqual(decode(packet)[0].args,(('h',123456789),('d',.25)))
        with self.assertRaises(ValueError):decode(b'/sf\0,b\0\0'+struct.pack('>i',2**31-1))
        self.assertEqual(decode(b'/sf\0,b\0\0'+struct.pack('>i',3)+b'abc\0')[0].args,(('b',b'abc'),))

    def test_argument_editor_literal_not_eval(self):
        self.assertEqual(parse_arguments('f 0.5\ni -1\ns Hello world\nT'),(('f',.5),('i',-1),('s','Hello world'),('T',None)))
        for text in ('f NaN','i 1.2','q 1','T yes','__import__("os")'):
            with self.assertRaises(ValueError):parse_arguments(text)

    def test_spatial_mapping_and_radius(self):
        messages=spatial_messages(4,1,1,1)
        xyz=[v for _,v in messages[0].args];aed=[v for _,v in messages[1].args]
        self.assertAlmostEqual(sum(v*v for v in xyz),1)
        self.assertAlmostEqual(aed[2],1)
        self.assertAlmostEqual(aed[0],-45)
        self.assertEqual([v for _,v in spatial_messages(1,1,0,0)[0].args],[0,1,0])
        self.assertEqual([v for _,v in spatial_messages(1,0,1,0)[0].args],[1,0,0])
        for params in [(0,0,0,0),(1,2,0,0),(1,float('nan'),0,0)]:
            with self.assertRaises(ValueError):spatial_messages(*params)


class ProfileTests(unittest.TestCase):
    def test_defaults_and_roundtrip(self):
        p=Profile();p.validate();self.assertEqual(len(p.cues),3)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'profile.json';save_profile(path,p)
            self.assertEqual(load_profile(path).to_dict(),p.to_dict())
            self.assertEqual(list(Path(directory).glob('*.tmp')),[])

    def test_endpoint_rules(self):
        self.assertEqual(endpoint('127.0.0.1',9000),('127.0.0.1',9000))
        self.assertEqual(endpoint('0.0.0.0',9001,True),('0.0.0.0',9001))
        for ip,port in [('localhost',9000),('::1',9000),('0.0.0.0',9000),('224.1.2.3',9000),('127.0.0.1',0),('127.0.0.1',65536),('127.0.0.1',True)]:
            with self.assertRaises(ValueError):endpoint(ip,port)

    def test_invalid_profiles_are_rejected(self):
        original=Profile().to_dict()
        for key,value in [('schema_version',2),('schema_version',True),('unknown',1),('front',float('nan')),('gain',10**1000),('send_hz',0),('object_id',0)]:
            data=copy.deepcopy(original);data[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):Profile.from_dict(data)
        data=copy.deepcopy(original);data['cues'].append(data['cues'][0])
        with self.assertRaises(ValueError):Profile.from_dict(data)
        with self.assertRaises(ValueError):Profile.from_dict({'cues':[{'name':'x','base':'bad'}]})
        with self.assertRaises(ValueError):Profile.from_dict({'cues':[]})

    def test_oversized_profile_and_atomic_save_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'profile.json';path.write_text(' '*262145)
            with self.assertRaises(ValueError):load_profile(path)
            path.write_text('keep');profile=Profile();profile.object_id=0
            with self.assertRaises(ValueError):save_profile(path,profile)
            self.assertEqual(path.read_text(),'keep')


class LiveCueTests(unittest.TestCase):
    def test_kind_defaults_to_playback_and_old_profiles_still_load(self):
        self.assertEqual(Cue('a', '/a').kind, 'playback')
        profile = Profile.from_dict({'schema_version': 1, 'cues': [{'name': 'Old', 'base': '/old'}]})
        self.assertEqual(profile.cues[0].kind, 'playback')

    def test_live_cue_round_trips_through_a_saved_profile(self):
        profile = Profile.from_dict({'schema_version': 1, 'cues': [{'name': 'Mic', 'base': '/audioloom/x/live/1', 'kind': 'live'}, {'name': 'Cue', 'base': '/demo/mono'}]})
        self.assertEqual([c.kind for c in profile.cues], ['live', 'playback'])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'p.json'
            save_profile(path, profile)
            self.assertEqual([c.kind for c in load_profile(path).cues], ['live', 'playback'])
            self.assertEqual(json.loads(path.read_text(encoding='utf-8'))['cues'][0]['kind'], 'live')

    def test_unknown_or_malformed_kind_is_rejected(self):
        for kind in ('mic', '', 'LIVE', None, 1, ['live']):
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                Profile.from_dict({'schema_version': 1, 'cues': [{'name': 'x', 'base': '/x', 'kind': kind}]})

    def test_live_commands_encode_as_documented(self):
        self.assertEqual(decode(encode(Message('/audioloom/a/live/1/play', (('f', 1.0),))))[0].address, '/audioloom/a/live/1/play')
        gain = decode(encode(Message('/audioloom/a/live/1/gain', (('f', 2.5),))))[0]
        self.assertEqual(gain.args, (('f', 2.5),))
        self.assertEqual(decode(encode(Message('/audioloom/a/live/1/stop')))[0].args, ())


class NetworkTests(unittest.TestCase):
    def free_port(self):
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1',0));return sock.getsockname()[1]

    def test_actual_loopback_send_receive_and_collision(self):
        receiver=Receiver();sender=Sender()
        try:
            bound=receiver.start('127.0.0.1',self.free_port());self.assertEqual(bound[0],'127.0.0.1')
            other=Receiver()
            with self.assertRaises(OSError):other.start(*bound)
            message=Message('/demo/playback/mono/state',(('f',1.),))
            sender.send(*bound,message)
            until=time.monotonic()+2;events=[]
            while not events and time.monotonic()<until:events=receiver.poll();time.sleep(.01)
            self.assertEqual(events[0][0],'message');self.assertEqual(events[0][3],message)
            receiver.stop()
            other.start(*bound);other.stop()
        finally:sender.close();receiver.stop()

    def test_malformed_datagram_rejected_without_partial_dispatch(self):
        receiver=Receiver()
        try:
            bound=receiver.start('127.0.0.1',self.free_port())
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:sock.sendto(b'bad',bound)
            until=time.monotonic()+2;events=[]
            while not events and time.monotonic()<until:events=receiver.poll();time.sleep(.01)
            self.assertEqual(events[0][0],'error')
        finally:receiver.stop()

    def test_receiver_queue_is_bounded_and_sender_is_lazy(self):
        sender=Sender();self.assertIsNone(sender.socket);sender.close()
        receiver=Receiver()
        for i in range(3000):receiver._put(('test',i))
        self.assertEqual(receiver.events.qsize(),2048);self.assertEqual(receiver.dropped,952)
        self.assertEqual(len(receiver.poll(12)),12)


if __name__=='__main__':unittest.main()
