"""Shared UI actions, independent of Windows/Linux adapter APIs."""
from pathlib import Path
import csv
import math
import struct
from backend import *

class Actions:
    def require_link(self):
        if self.demo:raise ValueError('Demo mode blocks CAN transmission')
        if not self.link or self.link.error:raise ValueError('Connect CAN first')

    def target(self):return (self.hw.get_value_as_int(),self.target_index.get_value_as_int())

    def settings(self):return [self.index.get_value_as_int(),self.sensor.get_active(),self.module_rate.get_active(),int(self.standard.get_active())+2*int(self.diag.get_active())]

    def detect(self):
        hw=self.target();self.work(lambda:self.service.detect(hw),lambda r:self.result(hw,r))

    def find_modules(self):self.work(lambda:self.link.send((PING,True,b'\xff')))

    def save_config(self):
        hw=self.target();cfg=self.settings()
        if self.confirm('Save configuration permanently and verify readback? Hardware ID and target data index must identify one module. Check sensor compatibility. Bitrate changes apply after restart; restart after changing sensor type.'):
            self.work(lambda:self.service.apply(hw,cfg),lambda r:self.result(hw,r))

    def save_backup(self):
        hw=self.target()
        try:
            self.require_link();self.service.require(hw)
            if hw not in self.configs:raise ValueError('Detect / read F0 first')
            cfg,trim,_=self.configs[hw];data=self.core.backup(hw[0],cfg,trim)
            path=self.choose('Save verified F0 backup',True,'wideband-f0.wbf')
            if path:Path(path).write_bytes(data);self.note('Verified configuration/Rtrim backup saved with CRC.')
        except Exception as e:self.error(str(e))

    def save_trim(self):
        try:
            from decimal import Decimal
            resistance=Decimal(self.trim.get_text().replace(",","."));hundred=resistance*100
            if not hundred.is_finite() or hundred!=hundred.to_integral() or not 3000<=hundred<=30000:raise ValueError('Use your sensor label value, 30.00..300.00 Ohm, at most two decimals.')
            hw=self.target();sensor=self.sensor.get_active();value=int(hundred)
            if sensor not in (0,4):raise ValueError('Rtrim requires Bosch LSU4.9 or LSU4.9 Rtrim SW.')
            if self.confirm('Save labeled sensor Rtrim and selected Bosch mode? Software mode requires the physical trim resistor to be absent. Sensor and mode must match.'):
                self.work(lambda:self.service.rtrim(hw,value,sensor),lambda r:self.result(hw,r))
        except Exception as e:self.error(str(e))

    def physical_trim(self):
        hw=self.target()
        if self.confirm('Select Bosch LSU4.9 with a physical trim resistor? Hardware must match. Software correction will be disabled.'):
            self.work(lambda:self.service.rtrim(hw,0,0),lambda r:self.result(hw,r))

    def identify(self):
        hw=self.target()
        def run():self.service.require(hw);return self.service.request(IDENTIFY,hw,bytes((10,)))
        self.work(run,lambda _:self.note('Identification acknowledged: LEDs alternate for 10 seconds.'))

    def restart(self):
        hw=self.target()
        def run():
            self.service.require(hw);self.service.request(RESET,hw,b'\xa5');self.service.known.pop(hw,None)
        if self.confirm('Restart F0? Saved bitrate becomes active. Adjust adapter/interface bitrate afterwards if necessary.'):
            self.work(run,lambda _:self.note('Restart acknowledged. Detect / read F0 again.'))

    def defaults(self):
        hw=self.target()
        if self.confirm('Write factory defaults? Index follows solder bridges, sensor follows firmware default, TX enabled. Saved bitrate is retained. Back up first.'):
            self.work(lambda:self.service.defaults(hw),lambda r:self.result(hw,r))

    def legacy(self,kind):
        hw=self.target();value=self.settings()[{1:0,2:1,3:2}[kind]]
        def run():
            ident={1:INDEX,2:SENSOR,3:BITRATE}[kind];data=bytes((value,hw[0])) if kind==1 else bytes((hw[0],value))
            self.link.exchange((ident,True,data),lambda f:f[1] and ((kind==3 and f[0]==BITRATE+1 and f[2]==bytes((hw[0],value))) or (kind!=3 and f[0]==ACK and not f[2])))
        if self.confirm('Write legacy setting permanently? Hardware ID must be unique. Legacy ACKs have no sender ID or readback. Do not use other configuration tools concurrently.'):
            self.work(run,lambda _:self.note('Legacy command acknowledged; no configuration readback. Bitrate changes require reset.'))

    def flash(self):
        path=self.choose('Select F0 application BIN with CRC')
        if not path:return
        try:
            data=Path(path).read_bytes();hw=self.target()
            if not self.core.valid_image(data):raise ValueError('Invalid F0 app image: expected 25600 bytes, vectors at 0x08001800 and matching CRC. Full/bootloader images rejected.')
            if self.confirm('Update F0 application? Connect exactly ONE compatible module to the bus. Bootloader commands are broadcast. Application will be erased and replaced. Keep power stable.'):
                self.flash_work(lambda:self.service.flash(hw[0],data,lambda x:self.bridge.progress.emit(x)),lambda _:self.note('Image transferred and restart acknowledged. Check Pong/live data; protocol has no flash readback.'))
        except Exception as e:self.error(str(e))

    def stop_heat(self):
        if self.heating and self.link and not self.link.ecu:
            try:self.link.send((STATUS,True,bytes((120,0,0))))
            except Exception:pass
        self.heating=False
        if hasattr(self,'heat_button'):self.heat_button.set_label('Start heater messages')

    def toggle_heat(self):
        if self.heating:self.stop_heat();self.note('Heater messages stopped; firmware timeout/fallback applies after 250 ms.');return
        try:
            self.require_link();v=float(self.voltage.get_text().replace(",","."))
            if not 0<=v<=25.5:raise ValueError('Supply must be 0..25.5 V, decimal point.')
            if self.link.ecu:raise ValueError('ECU status sender detected. Remove ECU sender and reconnect before bench heater control.')
            if self.confirm('Enable heating on all modules? Enter actual supply voltage. Sensor must be dry and suitable for heating. Commands apply to the entire bus.'):
                self.heat_values=bytes((int(v*10+.5),1,self.gain.get_value_as_int()));self.heating=True;self.heat_button.set_label('Stop heater messages')
        except Exception as e:self.error(str(e))

    def heat_tick(self):
        if self.heating:
            if not self.link or self.link.error or self.link.ecu:self.stop_heat();self.note('Heater messages stopped: ECU sender or CAN error.')
            else:
                try:self.link.send((STATUS,True,self.heat_values))
                except Exception as e:self.stop_heat();self.note(str(e))
        return True

    def toggle_csv(self):
        if self.raw_csv:self.raw_csv.close();self.raw_csv=None;self.note('Raw CAN CSV closed.');return
        path=self.choose('Save raw CAN CSV',True,'wideband-can.csv')
        if path:
            try:self.raw_csv=open(path,'w',newline='');csv.writer(self.raw_csv).writerow(['elapsed_s','direction','format','id_hex','data_hex']);self.note('Raw CAN CSV recording started.')
            except Exception as e:self.error(str(e))

    def export_events(self):
        path=self.choose('Export session events',True,'wideband-events.csv')
        if path:
            try:
                with open(path,'w',newline='') as f:
                    writer=csv.writer(f);writer.writerow(['elapsed_ms','index','event_code']);writer.writerows(self.core.events())
            except Exception as e:self.error(str(e))

    def export_stats(self):
        path=self.choose('Export CAN statistics',True,'wideband-can-statistics.csv')
        if path:
            try:
                with open(path,'w',newline='') as f:
                    writer=csv.writer(f);writer.writerow(['id','ext','dlc','count','first_ms','last_ms','max_gap_ms','average_hz']);writer.writerows(self.core.stats())
            except Exception as e:self.error(str(e))

    def demo_frames(self,ms):
        for idx in range(3):
            phase=ms/1000+idx
            standard=bytes((0xa0,1))+struct.pack('<HH',int((1+.035*math.sin(phase*.8)+idx*.02)*10000),int(790+15*math.sin(phase*.1)))+b'\0\0'
            diag=struct.pack('<HHBBBB',int(300+40*math.sin(phase*.2)),int(450+15*math.sin(phase*.8)),int(100+15*math.sin(phase)),2,int(145+20*math.sin(phase*.3)),2)
            self.core.feed((0x190+2*idx,False,standard),ms);self.core.feed((0x191+2*idx,False,diag),ms)
