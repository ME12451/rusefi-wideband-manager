#!/usr/bin/env python3
"""Run actual native manager state machine against two actual F0 dispatchers.
Each fake HAL node has its own saved/live configuration but the same HW ID.
No firmware function body is replaced. No CAN hardware is used.
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];fw=root/'firmware-f0/firmware'
service=re.sub(r'^#include .*$', '', (fw/'manager_service.cpp').read_text(),flags=re.M)
port=(fw/'boards/f0_module/port.cpp').read_text().split('#include "../../../for_rusefi/manager_can.h"')[1]
ui=(root/'src/enhanced_ui.h').read_text()
globals_=ui[ui.index('static uint8_t boundIndex'):ui.index('static int main_target')]
request=ui[ui.index('static int request_service'):ui.index('static int enhanced_available')]
available=ui[ui.index('static int enhanced_available'):ui.index('static void tools_disconnected')]
frame=ui[ui.index('static void tools_frame'):ui.index('static void enhanced_apply')]
tick=ui[ui.index('static void tools_tick'):]
source=r'''
#include "service_stubs.h"
#undef WB_ACK
#include "protocol.h"
#include <vector>
#include <string>
'''+port+'\n'+service+r'''
struct Node {Configuration live,saved;wbo::CanBitrate rate=wbo::CanBitrate::Kbit500;uint8_t active=0;bool fail=false;int writes=0;};
static Node nodes[2];
using HWND=void*;using DWORD=uint32_t;using LPARAM=intptr_t;using LONG=long;
#define FALSE 0
#define TRUE 1
#define CB_SETCURSEL 1
#define BM_SETCHECK 2
#define BST_CHECKED 1
#define BST_UNCHECKED 0
static HWND win,heat,trimEdit,flagData,flagDiag,indexBox,sensor,modrate,toolNote,hw,modules,targetIndexBox,toolsWindow,targetLabel;
static LONG connected=1,rxFault,busy;static int demoMode,uiTarget;
static std::string message,indexText,trimText;static int sensorSelection=-1,rateSelection=-1;
static HWND GetDlgItem(HWND,int){return nullptr;}
static void EnableWindow(HWND,int){}
static LONG InterlockedExchange(LONG* p,LONG n){LONG old=*p;*p=n;return old;}
static DWORD GetTickCount(){return testTime;}
static void SetWindowText(HWND h,const char* s){if(h==indexBox)indexText=s;if(h==trimEdit)trimText=s;}
static intptr_t SendMessage(HWND h,int msg,int value,intptr_t){if(msg==CB_SETCURSEL){if(h==sensor)sensorSelection=value;if(h==modrate)rateSelection=value;}return 0;}
static void err(const char* s){message=s;}
static void service_note(const char* s){message=s;}
static int main_target(){return 4;}
static int selected_target_index(){return uiTarget;}
static void set_target_index(int index){uiTarget=index;}
static int IsWindowVisible(HWND){return 0;}
static void report_refresh(){}
static void analytic_tick(DWORD){}
static std::vector<Frame> queued,sent;
static int requests,responders;
static bool dropNextReply;
static int sendframe(Frame* f){
 assert((f->id&0xffff0000u)==WBM_INDEX_REQUEST_BASE); // Never HW-only fallback.
 sent.push_back(*f);requests++;responders=0;
 for(auto& node:nodes){
  config=node.live;savedConfig=node.saved;savedRate=node.rate;managerActiveRate=node.active;failFlash=node.fail;flashWrites=node.writes;
  CANRxFrame req;req.IDE=f->ext;req.RTR=0;req.EID=f->id;req.DLC=f->len;std::memcpy(req.data8,f->data,8);
  int before=transmits;bool handled=ManagerRequest(req);
  if(transmits>before){assert(handled);responders++;Frame reply{};reply.ext=1;reply.id=lastTx.EID;reply.len=lastTx.DLC;std::memcpy(reply.data,lastTx.data8,8);if(!dropNextReply)queued.push_back(reply);}
  node.live=config;node.saved=savedConfig;node.rate=savedRate;node.writes=flashWrites;
 }
 dropNextReply=false;return 1;
}
'''+globals_+r'''
static void service_end(){servicePending=0;serviceFlow=0;busy=0;}
'''+request+'\n'+available+'\n'+frame+'\n'+tick+r'''
static void pump(){unsigned count=0;while(!queued.empty()){assert(++count<30);Frame f=queued.front();queued.erase(queued.begin());tools_frame(&f);}}
static void read(int index){uiTarget=index;read_f0();pump();assert(enhanced_available()&&trimKnown[4]&&boundIndex[4]==index);}
static uint16_t trim(const Node& n){return wbo::DecodeCalibration(n.live.calibration);}
int main(){
 indexBox=(HWND)1;sensor=(HWND)2;modrate=(HWND)3;trimEdit=(HWND)4;
 void* marker=mmap(reinterpret_cast<void*>(0x08000000),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(marker!=MAP_FAILED);*reinterpret_cast<uint32_t*>(wbo::CanBitrateBootloaderMarkerAddress)=wbo::CanBitrateBootloaderMarker;
 for(int i=0;i<2;i++){nodes[i].live.afr[0].RusEfiIdx=i;nodes[i].live.sensorType=SensorType::LSU49_RTRIM_SW;nodes[i].live.calibration=wbo::MakeCalibrationRecord(i?23456:12345);nodes[i].saved=nodes[i].live;}
 read(0);assert(serviceConfig[4][0]==0&&trimValue[4]==12345&&indexText=="0"&&sensorSelection==4&&rateSelection==0&&trimText=="123.45");
 int count=requests;uint8_t args[5]={0x98,0x3a,4,WBM_CONFIRM,0}; // 15000
 assert(request_service(WBM_RSET,4,args));assert(responders==1);pump();assert(requests==count+2&&trim(nodes[0])==15000&&trim(nodes[1])==23456);
 uiTarget=1;assert(!enhanced_available());read(1);args[0]=0x90;args[1]=0x65; // 26000
 assert(request_service(WBM_RSET,4,args));pump();assert(trim(nodes[0])==15000&&trim(nodes[1])==26000&&trimValue[4]==26000);
 // Wrong index, v1 reply, transaction, and opcode must not complete a request.
 assert(request_service(WBM_CONFIG,4,nullptr));assert(queued.size()==1);Frame correct=queued.front(),wrong=correct;queued.clear();wrong.id=wbm_index_reply(4,0);tools_frame(&wrong);assert(servicePending);wrong=correct;wrong.id=WBM_REPLY_BASE|4;tools_frame(&wrong);assert(servicePending);wrong=correct;wrong.data[1]++;tools_frame(&wrong);assert(servicePending);wrong=correct;wrong.data[0]=WBM_RESET;tools_frame(&wrong);assert(servicePending);tools_frame(&correct);pump();assert(!busy);
 // No responder (or old firmware) means timeout, without legacy retry.
 count=requests;uiTarget=77;read_f0();assert(queued.empty());testTime+=2001;tools_tick();assert(!busy&&!servicePending&&requests==count+1&&!enhanced_available());
 // Changing index replies on OLD address, then verifies on NEW address.
 read(0);uint8_t cfg[5]={7,4,1,3,WBM_CONFIRM};count=sent.size();assert(request_service(WBM_APPLY,4,cfg));assert(queued[0].id==wbm_index_reply(4,0));pump();
 assert(sent[count].id==wbm_index_request(4,0)&&sent[count+1].id==wbm_index_request(4,7)&&sent[count+2].id==wbm_index_request(4,7));
 assert(nodes[0].live.afr[0].RusEfiIdx==7&&nodes[1].live.afr[0].RusEfiIdx==1&&uiTarget==7&&boundIndex[4]==7&&enhanced_available());
 // Old target address no longer responds; current address remains readable.
 uiTarget=0;read_f0();assert(queued.empty());testTime+=2001;tools_tick();read(7);
 // Flash failure is isolated to selected node and never retried.
 read(1);nodes[1].fail=true;count=requests;uint16_t other=trim(nodes[0]);args[0]=0x20;args[1]=0x4e;assert(request_service(WBM_RSET,4,args));pump();assert(requests==count+1&&!serviceKnown[4]&&trim(nodes[0])==other&&trim(nodes[1])==26000);nodes[1].fail=false;
 // Restore uses RSET + readback, APPLY + new-address readback.
 read(1);loadedConfig[0]=8;loadedConfig[1]=4;loadedConfig[2]=0;loadedConfig[3]=3;loadedTrim=20000;restoreStage=1;assert(request_service(WBM_RSET,4,args));pump();assert(!restoreStage&&uiTarget==8&&nodes[1].live.afr[0].RusEfiIdx==8&&trim(nodes[1])==20000&&trim(nodes[0])==other&&message.find("fully restored")!=std::string::npos);
 // Defaults may change address; reply still uses previous index.
 read(8);uint8_t confirm[5]={WBM_CONFIRM};assert(request_service(WBM_DEFAULTS,4,confirm));assert(queued[0].id==wbm_index_reply(4,8));pump();assert(uiTarget==0&&nodes[1].live.afr[0].RusEfiIdx==0&&nodes[0].live.afr[0].RusEfiIdx==7);
 // Rtrim mode changes require a fresh CONFIG read, still only on this target.
 read(0);args[0]=0x20;args[1]=0x4e;count=requests;assert(request_service(WBM_RSET,4,args));pump();assert(requests==count+4&&serviceConfig[4][1]==4&&trim(nodes[1])==20000&&trim(nodes[0])==other);
 // Dropped APPLY ACK: no automatic write repeat; board can be found at new index.
 read(7);cfg[0]=9;count=requests;dropNextReply=true;assert(request_service(WBM_APPLY,4,cfg));assert(queued.empty());testTime+=2001;tools_tick();assert(requests==count+1&&!enhanced_available()&&nodes[0].live.afr[0].RusEfiIdx==9&&nodes[1].live.afr[0].RusEfiIdx==0);read(9);
 // Bad selector in CAN ID and mismatched HW payload produce no reply/write.
 Frame malformed{};malformed.ext=1;malformed.id=wbm_index_request(5,9);malformed.len=8;malformed.data[0]=WBM_RSET;malformed.data[1]=4;assert(sendframe(&malformed));assert(responders==0);malformed.id=wbm_index_request(4,9);malformed.data[1]=5;assert(sendframe(&malformed));assert(responders==0);
 demoMode=1;count=requests;read_f0();assert(requests==count);demoMode=0;busy=1;read_f0();assert(requests==count);busy=0;
 munmap(marker,4096);std::cout<<"Indexed two-node actual client/firmware tests passed: isolated Rtrim/readback, stale cache, wrong replies, timeout/no legacy fallback, index changes, defaults, restore, flash failure, lost ACK and selector validation.\n";
}
'''
with tempfile.TemporaryDirectory() as folder:
 sourcefile=Path(folder)/'test.cpp';sourcefile.write_text(source)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-missing-field-initializers','-fsanitize=undefined','-I'+str(root/'tests/hal'),'-I'+str(root/'src'),'-I'+str(root/'firmware-f0/for_rusefi'),str(sourcefile),'-o',str(Path(folder)/'test')],check=True)
 subprocess.run([str(Path(folder)/'test')],check=True)
