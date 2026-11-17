import math
import socket
import time
import unittest
from osc_controller.protocol import Message
from osc_controller.receiver_hub import ReceiverHub,local_self_test,loopback_destination_status
from osc_controller.tracker import ObjectTracker,project
from osc_controller.transport import Sender


def msg(address,*values):return Message(address,tuple(('f',float(v)) for v in values))


class TrackerTests(unittest.TestCase):
    def test_xyz_aed_and_custom_axes(self):
        t=ObjectTracker()
        t.ingest(msg('/adm/obj/1/xyz',.2,.4,.6),'127.0.0.1',50018,1)
        obj=next(iter(t.objects.values()));self.assertEqual(obj.position,(.4,.2,.6))
        t.ingest(msg('/adm/obj/1/aed',90,0,1),'127.0.0.1',50018,1.01)
        self.assertEqual(obj.position,(.4,.2,.6));self.assertEqual(len(obj.samples),1)
        t.ingest(msg('/adm/obj/1/aed',90,0,1),'127.0.0.1',50018,3)
        self.assertAlmostEqual(obj.position[0],0);self.assertAlmostEqual(obj.position[1],-1)
        t.ingest(msg('/demo/main-stage/101/xyz',.4,.2,.6),'127.0.0.1',9002,3)
        self.assertEqual(list(t.objects.values())[1].position,(.4,.2,.6))

    def test_metadata_sources_are_distinct_and_bounded(self):
        t=ObjectTracker(maximum=2)
        for port in (50018,50019):t.ingest(msg('/adm/obj/1/xyz',0,1,0),'127.0.0.1',port,port)
        self.assertEqual(len(t.objects),2)
        t.ingest(Message('/adm/obj/1/name',(('s','A'*200),)),'127.0.0.1',50019,50020)
        self.assertEqual(len(list(t.objects.values())[1].name),80)
        t.ingest(msg('/adm/obj/2/xyz',0,0,0),'127.0.0.1',50018,60000)
        self.assertEqual(len(t.objects),2)
        self.assertNotIn('ADM:127.0.0.1:50018:1',t.objects)

    def test_invalid_and_unrelated_messages_do_not_create_objects(self):
        t=ObjectTracker()
        for m in [msg('/adm/obj/1/xyz',1),msg('/adm/obj/1/aed',0,0,-1),msg('/adm/obj/1/xyz',math.nan,0,0),Message('/adm/obj/1/name',(('i',1),)),msg('/other/xyz',1,2,3)]:
            self.assertFalse(t.ingest(m,'127.0.0.1',50018,1))
        self.assertEqual(len(t.objects),0)

    def test_trails_rate_and_projection(self):
        t=ObjectTracker()
        for i in range(400):t.ingest(msg('/adm/obj/1/xyz',i/400,0,0),'127.0.0.1',50018,i*.05)
        obj=next(iter(t.objects.values()));self.assertEqual(len(obj.trail),240)
        self.assertAlmostEqual(obj.rate(19.95),20,places=3)
        self.assertEqual(obj.rate(30),0)
        self.assertEqual(project((1,2,3),'Top'),(2,1));self.assertEqual(project((1,2,3),'Front'),(2,3));self.assertEqual(project((1,2,3),'Side'),(1,3))
        t.clear();self.assertEqual(t.objects,{})


class HubTests(unittest.TestCase):
    def free_ports(self,n):
        sockets=[]
        try:
            for _ in range(n):
                s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.bind(('127.0.0.1',0));sockets.append(s)
            return [s.getsockname()[1] for s in sockets]
        finally:
            for s in sockets:s.close()

    def test_three_ports_shared_socket_and_owner_stop(self):
        hub=ReceiverHub();sender=Sender();ports=self.free_ports(3)
        try:
            targets=[('127.0.0.1',p) for p in ports]
            hub.configure({'monitor':targets[:1],'tracker':targets})
            self.assertEqual(len(hub.listeners),3)
            for i,target in enumerate(targets):sender.send(*target,msg(f'/test/{i}',i))
            events=[];until=time.monotonic()+2
            while len(events)<3 and time.monotonic()<until:events.extend(hub.poll());time.sleep(.01)
            self.assertEqual({target[1] for target,_ in events},set(ports))
            hub.stop_owner('monitor');self.assertEqual(len(hub.listeners),3)
            hub.stop_owner('tracker');self.assertEqual(hub.endpoints(),[])
        finally:hub.close();sender.close()

    def test_failed_multi_bind_preserves_previous_listeners(self):
        hub=ReceiverHub();ports=self.free_ports(3)
        try:
            hub.start('monitor',[('127.0.0.1',ports[0])])
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as occupied:
                occupied.bind(('127.0.0.1',ports[2]))
                with self.assertRaises(OSError):hub.start('tracker',[('127.0.0.1',p) for p in ports[1:]])
            self.assertEqual(hub.endpoints(),[('127.0.0.1',ports[0])])
            self.assertEqual(hub.endpoints('tracker'),[])
        finally:hub.close()

    def test_loopback_diagnostics(self):
        self.assertTrue(local_self_test())
        port=self.free_ports(1)[0]
        self.assertIn('UNBOUND',loopback_destination_status('127.0.0.1',port))
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as occupied:
            occupied.bind(('127.0.0.1',port))
            self.assertNotIn('UNBOUND',loopback_destination_status('127.0.0.1',port))


if __name__=='__main__':unittest.main()
