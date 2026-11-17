import json
import socket
import threading
import unittest
from osc_controller.protocol import Message,encode,decode
from osc_controller.scene_query import PageAssembler,SceneQuerySession,SceneQueryError,query_scene,SCHEMA,RESPONSE,QUERY


def page(offset=0,total=1,objects=None,next_offset=-1):
    if objects is None:objects=[{'kind':'playback','key':'/actor/component','osc_base':'/test/cue'}]
    return {'schema':SCHEMA,'schema_version':1,'world':'/Game/Scene','offset':offset,'limit':64,'returned':len(objects),'total':total,'next_offset':next_offset,'objects':objects,'playback_network':{'configured_listen_port':9000},'snapshot_utc':'test'}


def responses(request_id,data):
    text=json.dumps(data,ensure_ascii=True,separators=(',',':'));chunks=[text[i:i+900] for i in range(0,len(text),900)]
    return [Message(RESPONSE,(('s',request_id),('i',i),('i',len(chunks)),('s',chunk))) for i,chunk in enumerate(chunks)]


class AssemblyTests(unittest.TestCase):
    def test_out_of_order_duplicates_and_unicode(self):
        data=page();data['objects'][0]['name']='🎧 日本語'*500
        parts=responses('r1',data);a=PageAssembler('r1','127.0.0.1')
        self.assertIsNone(a.accept(parts[-1],'127.0.0.1'));self.assertIsNone(a.accept(parts[-1],'127.0.0.1'))
        result=None
        for p in parts[:-1]:result=a.accept(p,'127.0.0.1')
        self.assertEqual(result,data)

    def test_wrong_sender_and_id_ignored(self):
        p=responses('r1',page())[0];a=PageAssembler('r1','127.0.0.1')
        self.assertIsNone(a.accept(p,'127.0.0.2'))
        self.assertIsNone(a.accept(responses('r2',page())[0],'127.0.0.1'))
        self.assertEqual(a.chunks,{})

    def test_bounds_schema_and_nonfinite_rejected(self):
        for m in [Message(RESPONSE,(('s','r'),('i',0),('i',129),('s','x'))),Message(RESPONSE,(('s','r'),('i',-1),('i',1),('s','x'))),Message(RESPONSE,(('s','r'),('i',0),('i',1),('s','x'*901)))]:
            with self.assertRaises(SceneQueryError):PageAssembler('r','127.0.0.1').accept(m,'127.0.0.1')
        for data in [dict(page(),schema_version=2),dict(page(),error='page_too_large_reduce_limit'),dict(page(),world_time_seconds=float('nan'))]:
            with self.assertRaises(SceneQueryError):PageAssembler('r','127.0.0.1').accept(responses('r',data)[0],'127.0.0.1')

    def test_conflicting_chunk_count_or_payload_rejected(self):
        a=PageAssembler('r','127.0.0.1');a.accept(Message(RESPONSE,(('s','r'),('i',0),('i',2),('s','a'))),'127.0.0.1')
        with self.assertRaises(SceneQueryError):a.accept(Message(RESPONSE,(('s','r'),('i',0),('i',2),('s','b'))),'127.0.0.1')


class SessionTests(unittest.TestCase):
    def session(self):
        sent=[];s=SceneQuerySession('127.0.0.1',8100,9011,lambda *args:sent.append(args));return s,sent

    def deliver(self,s,data,now):
        for m in responses(s.assembler.request_id,data):s.accept(m,'127.0.0.1',now)

    def test_pages_assemble_and_never_send_transport(self):
        s,sent=self.session();s.tick(0);self.assertEqual(sent[0][2].address,QUERY)
        self.deliver(s,page(total=2,next_offset=1),.1);self.assertIsNone(s.done)
        s.tick(.5);self.assertEqual(sent[-1][2].args[2],('i',1))
        self.deliver(s,page(offset=1,total=2,objects=[{'kind':'stage','key':'/stage'}]),.6)
        self.assertEqual(len(s.done['objects']),2);self.assertTrue(s.done['paginated'])
        self.assertTrue(all(args[2].address==QUERY for args in sent))

    def test_timeout_is_bounded_and_retries_get_new_ids(self):
        s,sent=self.session()
        for now in (0,2,4,6):s.tick(now)
        self.assertEqual(len(sent),3);self.assertIn('timed out',s.error)
        self.assertEqual(len({args[2].args[0][1] for args in sent}),3)

    def test_scene_structure_changes_fail_without_partial_commit(self):
        s,_=self.session();s.tick(0);self.deliver(s,page(total=2,next_offset=1),.1);s.tick(.5)
        self.deliver(s,page(offset=1,total=3,objects=[{'kind':'stage','key':'/stage'}],next_offset=2),.6)
        self.assertIsNone(s.done);self.assertIn('changed',s.error)

    def test_duplicate_records_and_invalid_continuations_rejected(self):
        for data in [page(total=2,next_offset=0),page(total=2,objects=[{'kind':'stage','key':'/same'},{'kind':'stage','key':'/same'}])]:
            s,_=self.session();s.tick(0);self.deliver(s,data,.1);self.assertIsNotNone(s.error)

    def test_actual_udp_client_against_chunking_server(self):
        errors=[]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as server:
            server.bind(('127.0.0.1',0));server.settimeout(2)
            def serve():
                try:
                    packet,peer=server.recvfrom(65507);m=decode(packet)[0]
                    self.assertEqual(m.address,QUERY)
                    rid,reply_port,offset,limit=[v for _,v in m.args]
                    data=page();data['objects'][0]['name']='large '+('Unicode 日本語 '*200)
                    for response in reversed(responses(rid,data)):server.sendto(encode(response),(peer[0],reply_port))
                except Exception as error:errors.append(error)
            worker=threading.Thread(target=serve);worker.start()
            result=query_scene('127.0.0.1',server.getsockname()[1]);worker.join(timeout=3)
            self.assertEqual(result['total'],1);self.assertEqual(errors,[])


if __name__=='__main__':unittest.main()
