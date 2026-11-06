#!/usr/bin/env python3
"""Linux client against actual F0 service/config C++ and Linux transport streams."""
from pathlib import Path
import ctypes as C
import os
import queue
import re
import select
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'linux'))
from backend import *

class Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();folder=Path(cls.temp.name);fw=ROOT/'firmware-f0/firmware'
        service=re.sub(r'^#include .*$', '', (fw/'manager_service.cpp').read_text(),flags=re.M)
        port=(fw/'boards/f0_module/port.cpp').read_text().split('#include "../../../for_rusefi/manager_can.h"')[1]
        src=folder/'server.cpp'
        src.write_text('#include "service_stubs.h"\n'+port+'\n'+service+r'''
struct Node {Configuration live,saved;wbo::CanBitrate rate;uint8_t active;};
static Node nodes[2];
extern "C" {
void test_save(int n){nodes[n]={config,savedConfig,savedRate,managerActiveRate};}
void test_load(int n){config=nodes[n].live;savedConfig=nodes[n].saved;savedRate=nodes[n].rate;managerActiveRate=nodes[n].active;}
void test_index(int n){config.afr[0].RusEfiIdx=n;savedConfig=config;}
int test_init(){void* p=mmap(reinterpret_cast<void*>(0x08000000),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p==MAP_FAILED)return 0;*reinterpret_cast<uint32_t*>(wbo::CanBitrateBootloaderMarkerAddress)=wbo::CanBitrateBootloaderMarker;return 1;}
void test_reset(){config=Configuration{};savedConfig=config;savedRate=wbo::CanBitrate::Kbit500;failFlash=false;ManagerInit();}
void test_fail(int fail){failFlash=fail;}
int test_request(uint32_t id,const uint8_t* in,uint8_t* out){CANRxFrame f;f.EID=id;std::memcpy(f.data8,in,8);int before=transmits;ManagerRequest(f);if(transmits==before)return 0;std::memcpy(out,lastTx.data8,8);return 1;}
}
''')
        lib=folder/'server.so'
        subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-fPIC','-shared','-I'+str(ROOT/'tests/hal'),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'firmware-f0/for_rusefi'),str(src),'-o',str(lib)],check=True)
        cls.server=C.CDLL(str(lib));cls.server.test_request.argtypes=[C.c_uint32,C.c_void_p,C.c_void_p];assert cls.server.test_init()
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.server.test_reset();server=self.server
        class Transport:
            def __init__(self):self.queue=queue.Queue();self.sent=[];self.wrong=False;self.multi=False
            def send(self,f):
                self.sent.append(f)
                if f[0]&0xffff0000!=REQUEST:return
                if self.multi:
                    replies=[]
                    for n in range(2):
                        server.test_load(n);out=C.create_string_buffer(8)
                        if server.test_request(f[0],f[2],out):replies.append(out.raw)
                        server.test_save(n)
                    if len(replies)>1:raise AssertionError('More than one module replied')
                    if replies:self.queue.put((REPLY|(f[0]&0xffff),True,replies[0]))
                    return
                out=C.create_string_buffer(8)
                if server.test_request(f[0],f[2],out):
                    data=out.raw;reply=REPLY|(f[0]&0xffff)
                    # All of these must be ignored before the correct reply.
                    self.queue.put((reply^1,True,data));self.queue.put((0x0efc0004,True,data));self.queue.put((reply^256,True,data));self.queue.put((reply,False,data))
                    self.queue.put((reply,True,bytes((data[0],(data[1]+1)&255))+data[2:]))
                    if self.wrong and f[2][0]==RSET:data=data[:3]+bytes((data[3]^1,))+data[4:]
                    self.queue.put((reply,True,data))
            def recv(self):
                try:return self.queue.get(timeout=.02)
                except queue.Empty:return None
            def close(self):pass
        self.transport=Transport();self.link=Link(self.transport);self.service=Service(self.link);self.core=Core();self.core.lib.wb_clear()
    def tearDown(self):self.link.close()
    def test_actual_f0_read_write_rtrim_restore_defaults(self):
        cfg,trim,ready=self.service.detect((4,0));self.assertEqual(cfg,[0,0,0,3,0]);self.assertEqual(trim,0)
        cfg,trim,ready=self.service.rtrim((4,0),12345,4);self.assertEqual((cfg[1],trim,ready),(4,12345,True))
        cfg,trim,_=self.service.apply((4,0),[7,4,1,3]);self.assertEqual(cfg,[7,4,1,3,0])
        backup=self.core.backup(4,cfg,trim);target,value=self.core.restore(backup)
        self.service.rtrim((4,7),23456,4);self.service.apply((4,7),[99,4,0,1])
        got=self.service.restore((4,99),target,value);self.assertEqual(got[0][:4],target);self.assertEqual(got[1],12345)
        # Non-Bosch restore uses a valid temporary Bosch RSET, then final mode.
        got=self.service.restore((4,7),[8,1,1,3],0);self.assertEqual(got[0][:4],[8,1,1,3]);self.assertEqual(got[1],0)
        got=self.service.defaults((4,8));self.assertEqual(got[0][:4],[0,0,1,3]);self.assertNotIn((4,8),self.service.known)
        self.service.request(IDENTIFY,(4,0),bytes((10,)))
        a=self.service.request(DIAG1,(4,0));b=self.service.request(DIAG2,(4,0))
        self.assertEqual(struct.unpack('<HhB',a),(120,-2500,1));self.assertEqual(struct.unpack('<hHB',b),(1250,7500,120))
    def test_two_same_hw_modules_isolate_trim_index_and_restore(self):
        self.server.test_save(0);self.server.test_index(1);self.server.test_save(1);self.transport.multi=True
        self.service.detect((4,0));self.service.detect((4,1))
        self.service.rtrim((4,0),12345,4);self.service.rtrim((4,1),23456,4)
        self.assertEqual(self.service.read((4,0))[1],12345)
        self.assertEqual(self.service.read((4,1))[1],23456)
        self.service.apply((4,0),[7,4,1,3])
        self.assertNotIn((4,0),self.service.known);self.assertIn((4,7),self.service.known)
        self.assertEqual(self.service.read((4,1))[0][0],1)
        self.service.restore((4,7),[8,4,0,1],15000)
        self.assertEqual(self.service.read((4,8))[1],15000)
        self.assertEqual(self.service.read((4,1))[1],23456)
        before=len(self.transport.sent)
        with self.assertRaises(TimeoutError):self.service.detect((4,77))
        self.assertEqual(len(self.transport.sent),before+1)
        self.assertEqual(self.transport.sent[-1][0],REQUEST|(4<<8)|77)

    def test_flash_failure_no_retry_and_wrong_rtrim(self):
        self.service.detect((4,0));self.transport.wrong=True
        with self.assertRaisesRegex(ValueError,'ACK differs'):self.service.rtrim((4,0),12345,4)
        self.assertNotIn((4,0),self.service.known);self.transport.wrong=False;self.service.detect((4,0))
        self.server.test_fail(1);before=len(self.transport.sent)
        with self.assertRaisesRegex(OSError,'flash error'):self.service.apply((4,0),[7,0,0,3])
        self.assertEqual(len(self.transport.sent),before+1);self.assertNotIn((4,0),self.service.known)
    def test_backup_crc_and_actual_analytics(self):
        backup=self.core.backup(4,[0,0,0,3,0],0)
        for i in range(len(backup)):
            damaged=bytearray(backup);damaged[i]^=1
            with self.assertRaises(ValueError):self.core.restore(bytes(damaged))
        self.core.feed((0x190,False,bytes((0xa0,1))+struct.pack('<HH',10000,780)+b'\0\0'),1000)
        self.core.feed((0x191,False,struct.pack('<HHBBBB',320,450,128,2,160,2)),1000)
        p=self.core.points(0)[0];self.assertEqual(p[1:3],(1.,780.));self.assertAlmostEqual(p[3],160*100/255)
        self.assertAlmostEqual(p[5],.45);self.assertEqual(p[6],320);self.assertTrue(all(p[7:13]))
        self.core.feed((0x190,False,bytes((0xa0,0))+struct.pack('<HH',15000,780)+b'\0\0'),1500)
        p=self.core.points(0)[-1];self.assertFalse(p[7]);self.assertFalse(p[9])
    def test_socketcan_kernel_frame_format(self):
        a,b=socket.socketpair();a.settimeout(.1);transport=SocketCAN.__new__(SocketCAN);transport.sock=a
        try:
            transport.send((0x1234567,True,b'\x01\x02'));raw=b.recv(16)
            self.assertEqual(struct.unpack('=IB3x8s',raw),(0x81234567,2,b'\x01\x02'+b'\0'*6))
            b.send(struct.pack('=IB3x8s',0x190,2,b'\x01\x02'));self.assertEqual(transport.recv(),(0x190,False,b'\x01\x02'))
            for ident,dlc in [(0x40000190,2),(0x20000190,2),(0x190,9)]:b.send(struct.pack('=IB3x8s',ident,dlc,b''));self.assertIsNone(transport.recv())
        finally:a.close();b.close()
    def test_slcan_pseudo_terminal(self):
        master,slave=os.openpty();commands=[];finished=threading.Event();errors=[]
        def adapter():
            pending=b''
            try:
                while not finished.is_set():
                    if not select.select([master],[],[],.05)[0]:continue
                    pending+=os.read(master,4096)
                    while b'\r' in pending:
                        cmd,_,pending=pending.partition(b'\r');commands.append(cmd)
                        if cmd[:1] in (b'C',b'S',b'O'):os.write(master,b'\r')
            except OSError as e:
                if not finished.is_set():errors.append(e)
        thread=threading.Thread(target=adapter);thread.start();transport=None
        try:
            transport=SLCAN(os.ttyname(slave),1000000);self.assertEqual(commands[:3],[b'C',b'S8',b'O'])
            os.write(master,b't1902A001\rT0EFC0004800010203040506070001\r')
            self.assertEqual(transport.recv(),(0x190,False,b'\xa0\x01'))
            self.assertEqual(transport.recv(),(0x0efc0004,True,bytes.fromhex('0001020304050607')))
            transport.send((PING,True,b'\xff'));time.sleep(.03);self.assertIn(b'T0EF600001FF',commands)
            os.write(master,b'\x07')
            with self.assertRaisesRegex(OSError,'BEL'):transport.recv()
        finally:
            if transport:transport.close()
            finished.set();thread.join(1);os.close(master);os.close(slave)
        self.assertFalse(errors)
    def test_firmware_preflight_and_transfer_failure(self):
        image=ROOT/'firmware-f0/firmware/build/wideband_image.bin'
        if not image.exists():self.skipTest('Existing firmware build not present')
        data=image.read_bytes();self.assertTrue(self.core.valid_image(data));self.assertFalse(self.core.valid_image(data[:-1]));bad=bytearray(data);bad[200]^=1;self.assertFalse(self.core.valid_image(bytes(bad)))
        with self.assertRaises(ValueError):self.service.flash(4,b'bad',lambda _:None)
        class BootLink:
            def __init__(self,fail=False):self.frames=[];self.fail=fail
            def exchange(self,frame,predicate,timeout):
                self.frames.append(frame)
                if self.fail and frame[0]==ERASE:raise TimeoutError('erase failed')
                result=(ACK,True,b'');assert predicate(result);return result
        boot=BootLink();progress=[];Service(boot).flash(4,data,progress.append)
        self.assertEqual(boot.frames[0],(ENTER,True,b'\x04'));self.assertEqual(boot.frames[1],(ENTER,True,b''));self.assertEqual(boot.frames[2],(ERASE,True,b''))
        self.assertEqual(b''.join(f[2] for f in boot.frames if f[0]&0xffff0000==DATA),data);self.assertEqual(progress[-1],1)
        self.assertEqual(boot.frames[-1],(REBOOT,True,b''))
        boot=BootLink(True)
        with self.assertRaises(TimeoutError):Service(boot).flash(4,data,lambda _:None)
        self.assertFalse(any(f[0]&0xffff0000==DATA for f in boot.frames))

if __name__=='__main__':unittest.main(verbosity=2)
