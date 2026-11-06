"""Adapter-specific code only. All module communication remains classic CAN."""
import time
import os
from pathlib import Path
import serial
import can

class CANTransport:
    def __init__(self,kind,channel,bitrate):
        if kind not in ('pcan','socketcan'):raise ValueError('Unsupported CAN adapter')
        if kind=='socketcan' and os.name=='nt':raise ValueError('SocketCAN is available on Linux only')
        self._dll_dirs=[]
        if kind=='pcan' and os.name=='nt':
            folders=[Path(__file__).resolve().parent.parent,Path(__file__).resolve().parent]
            os.environ['PATH']=os.pathsep.join(str(folder) for folder in folders)+os.pathsep+os.environ.get('PATH','')
            self._dll_dirs=[os.add_dll_directory(str(folder)) for folder in folders]
        self.bus=can.Bus(interface=kind,channel=channel,bitrate=bitrate,receive_own_messages=False,ignore_config=True)
    def send(self,frame):
        ident,extended,data=frame
        self.bus.send(can.Message(arbitration_id=ident,is_extended_id=extended,data=data,check=True),timeout=1)
    def recv(self):
        message=self.bus.recv(.1)
        if message is None:return None
        if message.is_error_frame:
            # Never pass controller errors off as valid measurements.
            raise OSError('CAN controller reported an error frame')
        if message.is_remote_frame or message.is_fd or message.dlc>8:return None
        return message.arbitration_id,message.is_extended_id,bytes(message.data)
    def close(self):
        self.bus.shutdown()
        for handle in self._dll_dirs:handle.close()

class SLCAN:
    """Local Lawicel port via pyserial, with checked initialization and BEL errors."""
    def __init__(self,path,bitrate):
        if '://' in path or '@' in path:raise ValueError('Use a local serial CAN adapter port, without URL or baud suffix')
        self.port=serial.Serial(path,115200,timeout=.1,write_timeout=1);self.buf=bytearray()
        try:
            self.port.reset_input_buffer()
            for command in (b'C\r',b'S8\r' if bitrate==1000000 else b'S6\r',b'O\r'):
                self.write(command);deadline=time.monotonic()+1.5;response=bytearray()
                while b'\r' not in response:
                    if time.monotonic()>deadline:raise TimeoutError('SLCAN initialization timeout; expected Lawicel 115200 baud, 8N1')
                    response.extend(self.port.read(1))
                    if b'\x07' in response:raise OSError('SLCAN initialization rejected (BEL)')
        except Exception:self.port.close();raise
    def write(self,data):
        if self.port.write(data)!=len(data):raise OSError('SLCAN transmit timeout')
    def send(self,frame):
        ident,extended,data=frame
        self.write(((('T%08X' if extended else 't%03X')%ident)+str(len(data))+data.hex().upper()+'\r').encode('ascii'))
    def recv(self):
        deadline=time.monotonic()+.1
        while True:
            if b'\x07' in self.buf:self.buf.clear();raise OSError('SLCAN adapter reported BEL error')
            if b'\r' in self.buf:
                line,_,rest=self.buf.partition(b'\r');self.buf=bytearray(rest)
                if not line or line[:1] not in (b't',b'T'):continue
                try:
                    digits=8 if line[0]==84 else 3;ident=int(line[1:1+digits],16);length=int(line[1+digits:2+digits],16)
                    end=digits+2+length*2
                    if length>8 or len(line) not in (end,end+4):continue
                    if ident>(0x1fffffff if digits==8 else 0x7ff):continue
                    if len(line)==end+4:int(line[-4:],16)
                    return ident,digits==8,bytes.fromhex(line[digits+2:end].decode('ascii'))
                except ValueError:continue
            if len(self.buf)>256:self.buf.clear();raise OSError('Malformed SLCAN stream')
            if time.monotonic()>deadline:return None
            self.buf.extend(self.port.read(max(1,min(self.port.in_waiting,4096))))
    def close(self):
        try:self.write(b'C\r')
        finally:self.port.close()

def create_transport(kind,channel,bitrate):
    return SLCAN(channel,bitrate) if kind=='slcan' else CANTransport(kind,channel,bitrate)
