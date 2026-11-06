#!/usr/bin/env python3
"""Compile actual manager dispatcher and actual F0 config functions with a fake HAL.
Only include directives are replaced; production function bodies are unchanged.
"""
from pathlib import Path
import subprocess,tempfile,re,os
root=Path(__file__).resolve().parents[1]
fw=root/'firmware-f0/firmware'
service=re.sub(r'^#include .*$', '', (fw/'manager_service.cpp').read_text(),flags=re.M)
port=(fw/'boards/f0_module/port.cpp').read_text().split('#include "../../../for_rusefi/manager_can.h"')[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp'
 p.write_text('#include "service_stubs.h"\n'+port+'\n'+service+r'''
static CANRxFrame request(uint8_t op,uint8_t seq=1){CANRxFrame f;f.EID=WBM_REQUEST;f.data8[0]=op;f.data8[1]=testHw;f.data8[2]=seq;return f;}
static uint16_t u16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
int main(){
 void* marker=mmap(reinterpret_cast<void*>(0x08000000),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(marker!=MAP_FAILED);
 auto mark=reinterpret_cast<uint32_t*>(wbo::CanBitrateBootloaderMarkerAddress);*mark=wbo::CanBitrateBootloaderMarker;
 savedConfig=config;ManagerInit();auto f=request(WBM_INFO);assert(ManagerRequest(f));assert(lastTx.EID==(WBM_REPLY_BASE|testHw));assert(lastTx.data8[0]==WBM_INFO&&lastTx.data8[1]==1&&lastTx.data8[3]==WBM_BOARD_F0&&lastTx.data8[4]==1);
 int n=transmits;f.data8[1]=3;assert(!ManagerRequest(f)&&transmits==n);f=request(WBM_INFO);f.IDE=0;assert(!ManagerRequest(f));f=request(WBM_INFO);f.DLC=7;assert(!ManagerRequest(f));f=request(WBM_INFO);f.RTR=1;assert(!ManagerRequest(f));
 f=request(WBM_APPLY,2);f.data8[3]=255;f.data8[4]=3;f.data8[5]=1;f.data8[6]=3;f.data8[7]=0;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_INVALID&&flashWrites==0);
 f.data8[7]=WBM_CONFIRM;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_OK&&flashWrites==1);assert(config.afr[0].RusEfiIdx==255&&config.sensorType==SensorType::LSU49_FAE);assert(savedRate==wbo::CanBitrate::Mbit1);assert(lastTx.data8[7]==0); // pending differs from active
 assert(ManagerRequest(f)&&flashWrites==1); // duplicate apply cannot re-erase
 auto read=request(WBM_CONFIG,3);assert(ManagerRequest(read));assert(lastTx.data8[3]==255&&lastTx.data8[5]==1);
 f.data8[2]=4;f.data8[4]=4;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_INVALID&&flashWrites==1);f.data8[2]=5;f.data8[4]=0;f.data8[6]=4;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_INVALID&&flashWrites==1);
 *mark=0;f.data8[2]=6;f.data8[6]=3;f.data8[5]=0;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_BOOTLOADER_REQUIRED&&flashWrites==1);*mark=wbo::CanBitrateBootloaderMarker;
 f.data8[2]=7;failFlash=true;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_FLASH_ERROR&&config.afr[0].RusEfiIdx==savedConfig.afr[0].RusEfiIdx);failFlash=false;
 f=request(WBM_DEFAULTS,8);f.data8[3]=WBM_CONFIRM;assert(ManagerRequest(f));assert(lastTx.data8[2]==0&&config.afr[0].RusEfiIdx==0&&savedRate==wbo::CanBitrate::Mbit1);
 const unsigned expected[]={2,0,3,4,0,1,5,6,7};for(testHw=0;testHw<9;testHw++){f=request(WBM_DEFAULTS,20+testHw);f.data8[3]=WBM_CONFIRM;assert(ManagerRequest(f));assert(config.afr[0].RusEfiIdx==expected[testHw]);}testHw=4;
 f=request(WBM_IDENTIFY,9);f.data8[3]=31;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_INVALID);f.data8[3]=10;assert(ManagerRequest(f));assert(ManagerIdentifyActive());testTime+=10001;assert(!ManagerIdentifyActive());
 f=request(WBM_DIAG1,10);assert(ManagerRequest(f));assert(u16(lastTx.data8+3)==120&&int16_t(u16(lastTx.data8+5))==-2500&&lastTx.data8[7]==1);sampler.ma=100;assert(ManagerRequest(f));assert(u16(lastTx.data8+5)==32767);sampler.ma=-100;assert(ManagerRequest(f));assert(int16_t(u16(lastTx.data8+5))==-32768);
 f=request(WBM_DIAG2,11);assert(ManagerRequest(f));assert(u16(lastTx.data8+3)==1250&&u16(lastTx.data8+5)==7500&&lastTx.data8[7]==120);
 f=request(99,12);assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_UNSUPPORTED);
 f=request(WBM_RESET,13);assert(ManagerRequest(f));assert(resets==0);f.data8[3]=WBM_CONFIRM;txResult=1;assert(ManagerRequest(f));assert(resets==0);txResult=0;assert(ManagerRequest(f));assert(resets==1);

 f=request(WBM_RSET,30);f.data8[3]=0x39;f.data8[4]=0x30;f.data8[5]=4;f.data8[6]=WBM_CONFIRM;assert(ManagerRequest(f));assert(lastTx.data8[2]==0&&u16(lastTx.data8+3)==12345&&lastTx.data8[5]==4&&lastTx.data8[6]==1);
 f=request(WBM_RGET,31);assert(ManagerRequest(f));assert(u16(lastTx.data8+3)==12345&&lastTx.data8[5]==4);
 f=request(WBM_RSET,32);f.data8[5]=4;f.data8[6]=WBM_CONFIRM;assert(ManagerRequest(f));assert(lastTx.data8[2]==WBM_INVALID); // SW mode requires nonzero valid trim
 munmap(marker,4096);std::cout<<"Actual F0 CAN dispatcher/config tests passed\n";
}
''')
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'tests/hal'),'-I'+str(root/'src'),'-I'+str(root/'firmware-f0/for_rusefi'),str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)
