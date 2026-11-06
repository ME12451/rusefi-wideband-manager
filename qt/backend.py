"""Common Windows/Linux CAN service and shared C analytics binding."""
import collections
import ctypes as C
import os
from pathlib import Path
import struct
import threading
import time

ACK=0x727573
ENTER,ERASE,DATA,REBOOT,INDEX,STATUS,PING,SENSOR,BITRATE=(0x0ef00000,0x0ef15a5a,0x0ef20000,0x0ef30000,0x0ef40000,0x0ef50000,0x0ef60000,0x0ef70000,0x0ef80000)
REQUEST,REPLY=0x0efb0000,0x0efc0000
INFO,CONFIG,APPLY,DEFAULTS,IDENTIFY,RESET,DIAG1,DIAG2,RGET,RSET=range(1,11)
SENSORS=['LSU 4.9','LSU 4.2','LSU ADV','FAE LSU 4.9','LSU 4.9 Rtrim SW']
STATES=['Preheat','Warmup','Closed loop','Heater fault','Overtemperature','Undertemperature','Heating disabled','CAN silent']

class Core:
    def __init__(self):
        self.lib=C.CDLL(str(Path(__file__).with_name('libwideband.dll' if os.name=='nt' else 'libwideband.so')))
        self.lib.wb_frame.argtypes=[C.c_uint32,C.c_int,C.c_void_p,C.c_uint,C.c_int,C.c_uint32]
        self.lib.wb_points.argtypes=[C.c_uint,C.POINTER(C.c_double),C.c_uint]
        self.lib.wb_bounds.argtypes=[C.c_int,C.c_int,C.c_double,C.c_double,C.c_int,C.POINTER(C.c_double),C.POINTER(C.c_double)]
        self.lib.wb_image.argtypes=[C.c_void_p,C.c_uint]
        self.lib.wb_backup.argtypes=[C.c_void_p,C.c_uint,C.c_void_p,C.c_uint]
        self.lib.wb_restore.argtypes=[C.c_void_p,C.c_uint,C.c_void_p,C.POINTER(C.c_uint16)]
    def feed(self,frame,ms,tx=False):
        ident,ext,data=frame
        self.lib.wb_frame(ident,ext,data,len(data),tx,ms)
    def points(self,index):
        buf=(C.c_double*(3000*15))();n=self.lib.wb_points(index,buf,3000)
        return [tuple(buf[i*15:(i+1)*15]) for i in range(n)]
    def bounds(self,metric,automatic,values):
        lo=C.c_double();hi=C.c_double()
        self.lib.wb_bounds(metric,automatic,min(values,default=0),max(values,default=0),len(values),C.byref(lo),C.byref(hi))
        return lo.value,hi.value
    def backup(self,hw,cfg,trim):
        out=C.create_string_buffer(20)
        if not self.lib.wb_backup(out,hw,bytes(cfg),trim):raise ValueError('Invalid F0 configuration/Rtrim for backup')
        return out.raw
    def restore(self,data):
        cfg=C.create_string_buffer(5);trim=C.c_uint16()
        if not self.lib.wb_restore(data,len(data),cfg,C.byref(trim)):raise ValueError('Invalid backup format, values or CRC; nothing written')
        return list(cfg.raw[:4]),trim.value
    def valid_image(self,data):return bool(self.lib.wb_image(data,len(data)))
    def events(self):
        buf=(C.c_uint32*(1024*3))();n=self.lib.wb_events(buf,1024)
        return [tuple(buf[i*3:i*3+3]) for i in range(n)]
    def stats(self):
        buf=(C.c_double*(2048*8))();n=self.lib.wb_stats(buf,2048)
        return [tuple(buf[i*8:i*8+8]) for i in range(n)]

class Link:
    """Receiver feeds a bounded UI queue and a condition-protected reply inbox."""
    def __init__(self,transport):
        self.transport=transport;self.cv=threading.Condition();self.frames=collections.deque(maxlen=8192)
        self.inbox=collections.deque(maxlen=8192);self.sequence=0;self.error=None;self.stop=False;self.txlock=threading.Lock();self.ecu=False
        self.thread=threading.Thread(target=self.receive,daemon=True);self.thread.start()
    def receive(self):
        try:
            while not self.stop:
                f=self.transport.recv()
                if f:
                    with self.cv:
                        self.sequence+=1;self.inbox.append((self.sequence,f));self.frames.append((False,f,time.monotonic()))
                        if f[1] and f[0]==STATUS:self.ecu=True
                        self.cv.notify_all()
        except Exception as e:
            if not self.stop:self.error=str(e)
            with self.cv:self.cv.notify_all()
    def send(self,frame):
        if self.stop or self.error:raise OSError(self.error or 'CAN disconnected')
        ident,ext,data=frame
        if len(data)>8 or not 0<=ident<=(0x1fffffff if ext else 0x7ff):raise ValueError('Invalid CAN frame')
        with self.txlock:self.transport.send(frame)
        with self.cv:self.frames.append((True,frame,time.monotonic()))
    def exchange(self,frame,predicate,timeout=1.8):
        with self.cv:before=self.sequence
        self.send(frame);deadline=time.monotonic()+timeout
        with self.cv:
            while True:
                for seq,f in self.inbox:
                    if seq>before and predicate(f):return f
                if self.error or self.stop:raise OSError(self.error or 'CAN disconnected')
                remaining=deadline-time.monotonic()
                if remaining<=0:raise TimeoutError('No matching CAN reply; result unknown. No automatic write retry.')
                self.cv.wait(remaining)
    def drain(self):
        with self.cv:items=list(self.frames);self.frames.clear();return items
    def close(self):
        self.stop=True
        with self.cv:self.cv.notify_all()
        self.thread.join(.5);self.transport.close()

class Service:
    def __init__(self,link):self.link=link;self.transaction=0;self.known={}
    def request(self,op,hw,args=b''):
        if not 0<=hw<=8:raise ValueError('Hardware ID must be 0..8 and unique on the bus')
        self.transaction=(self.transaction+1)&255;t=self.transaction
        payload=bytes((op,hw,t))+bytes(args).ljust(5,b'\0')
        if len(payload)!=8:raise ValueError('Invalid service payload')
        _,_,data=self.link.exchange((REQUEST,True,payload),lambda f:f[1] and f[0]==REPLY|hw and len(f[2])==8 and f[2][:2]==bytes((op,t)))
        if data[2]:
            self.known.pop(hw,None)
            raise OSError('F0 rejected command: '+{1:'invalid request',2:'flash error',3:'bootloader required',4:'unsupported'}.get(data[2],str(data[2])))
        return data[3:]
    def detect(self,hw):
        self.known.pop(hw,None)
        info=self.request(INFO,hw)
        if info!=bytes((0xf0,1,0x7f,0,1)):raise ValueError('Incompatible board or CAN service protocol')
        self.known[hw]=True
        try:return self.read(hw)
        except Exception:self.known.pop(hw,None);raise
    def require(self,hw):
        if not self.known.get(hw):raise ValueError('Detect / read F0 first')
    def read(self,hw):
        cfg=self.request(CONFIG,hw);trim=self.request(RGET,hw)
        if cfg[1]>4 or cfg[2]>1 or cfg[3]>3 or cfg[4]>1:raise ValueError('Invalid configuration response')
        resistance=int.from_bytes(trim[:2],'little')
        if trim[2]>4 or trim[3]>1 or not (resistance==0 or 3000<=resistance<=30000):raise ValueError('Invalid Rtrim response')
        if trim[2]!=cfg[1]:raise ValueError('Configuration and Rtrim sensor types differ; read again')
        return list(cfg),resistance,bool(trim[3])
    def apply(self,hw,cfg):
        self.require(hw)
        if len(cfg)!=4 or not 0<=cfg[0]<=255 or not 0<=cfg[1]<=4 or cfg[2] not in (0,1) or not 0<=cfg[3]<=3:raise ValueError('Invalid configuration')
        try:
            reply=self.request(APPLY,hw,bytes(cfg)+b'\xa5')
            if reply[:4]!=bytes(cfg):raise ValueError('Configuration ACK differs from request')
            result=self.read(hw)
            if result[0][:4]!=cfg:raise ValueError('Configuration readback mismatch')
            return result
        except Exception:self.known.pop(hw,None);raise
    def rtrim(self,hw,value,sensor):
        self.require(hw)
        if not (value==0 or 3000<=value<=30000) or sensor not in (0,4) or (sensor==4 and value==0):raise ValueError('Invalid Bosch Rtrim setting')
        args=value.to_bytes(2,'little')+bytes((sensor,0xa5,0))
        try:
            reply=self.request(RSET,hw,args)
            if reply[:3]!=args[:3]:raise ValueError('Rtrim ACK differs from request')
            result=self.read(hw)
            if result[1]!=value or result[0][1]!=sensor:raise ValueError('Rtrim readback mismatch')
            return result
        except Exception:self.known.pop(hw,None);raise
    def restore(self,hw,cfg,trim):
        self.require(hw)
        try:
            # RSET accepts Bosch modes only; restore the trim in normal Bosch
            # mode first, then apply the final sensor/config in a second write.
            interim=cfg[1] if cfg[1] in (0,4) else 0
            args=trim.to_bytes(2,'little')+bytes((interim,0xa5,0))
            ack=self.request(RSET,hw,args)
            if ack[:3]!=args[:3]:raise ValueError('Restore Rtrim ACK mismatch')
            result=self.read(hw)
            if result[1]!=trim or result[0][1]!=interim:raise ValueError('Restore Rtrim readback mismatch')
            result=self.apply(hw,cfg)
            if result[1]!=trim:raise ValueError('Restore verification failed')
            return result
        except Exception:self.known.pop(hw,None);raise
    def defaults(self,hw):
        self.require(hw)
        try:
            ack=self.request(DEFAULTS,hw,b'\xa5');result=self.read(hw)
            if result[0][:4]!=list(ack[:4]):raise ValueError('Factory defaults readback mismatch')
            return result
        except Exception:self.known.pop(hw,None);raise
    def flash(self,hw,image,progress):
        if not 0<=hw<=8:raise ValueError('Hardware ID must be 0..8')
        if not Core().valid_image(image):raise ValueError('Invalid F0 application image: size, vectors or CRC')
        def command(ident,data=b'',timeout=1.8):
            return self.link.exchange((ident,True,data),lambda f:f[1] and f[0]==ACK and not f[2],timeout)
        try:command(ENTER,bytes((hw,)))
        except TimeoutError:pass
        time.sleep(.15)
        for attempt in range(20):
            try:command(ENTER,timeout=.04);break
            except TimeoutError:
                if attempt==19:raise
        command(ERASE,timeout=5)
        for offset in range(0,len(image),8):
            command(DATA|offset,image[offset:offset+8]);progress((offset+8)/len(image))
        command(REBOOT);self.known.clear()
