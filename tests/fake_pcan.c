/* Test-only PCANBasic DLL. Never ship beside the production application. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../src/service_protocol.h"
#include "../src/protocol.h"
typedef struct {uint32_t id;uint8_t type,len,data[8];} PcanMsg;
static CRITICAL_SECTION lock;
static PcanMsg queue[64];static unsigned head,tail;static int single,flood;static uint8_t indices[2],sensors[2],rates[2],flags[2];static uint16_t trims[2];static DWORD lastFlood;
static void push(uint32_t id,int ext,const uint8_t *data,int size){if(head-tail>=64)return;PcanMsg *m=&queue[head++%64];memset(m,0,sizeof *m);m->id=id;m->type=ext?2:0;m->len=(uint8_t)size;memcpy(m->data,data,size);}
__declspec(dllexport) uint32_t __stdcall CAN_Initialize(uint16_t channel,uint16_t bitrate,uint8_t kind,uint32_t port,uint16_t irq){
 (void)bitrate;(void)kind;(void)port;(void)irq;EnterCriticalSection(&lock);head=tail=0;single=channel==0x52;flood=channel==0x53;lastFlood=0;indices[0]=0;indices[1]=1;sensors[0]=sensors[1]=4;rates[0]=0;rates[1]=1;flags[0]=flags[1]=3;trims[0]=12345;trims[1]=23456;
 uint8_t data[8]={0xa0,1,0x10,0x27,0x0c,3,0,0},diag[8]={0x2c,1,0xc2,1,128,2,150,2};
 if(!single){push(0x190,0,data,8);push(0x191,0,diag,8);}push(0x192,0,data,8);push(0x193,0,diag,8);LeaveCriticalSection(&lock);return 0;
}
__declspec(dllexport) uint32_t __stdcall CAN_Uninitialize(uint16_t channel){(void)channel;return 0;}
__declspec(dllexport) uint32_t __stdcall CAN_Read(uint16_t channel,PcanMsg *message,void *timestamp){
 (void)channel;(void)timestamp;EnterCriticalSection(&lock);if(flood&&head==tail&&GetTickCount()-lastFlood>=2){uint8_t data[8]={0xa0,1,0x10,0x27,0x0c,3,0,0},diag[8]={0x2c,1,0xc2,1,128,2,150,2};push(0x190,0,data,8);push(0x191,0,diag,8);lastFlood=GetTickCount();}uint32_t result=0x20;if(tail<head){*message=queue[tail++%64];result=0;}LeaveCriticalSection(&lock);return result;
}
__declspec(dllexport) uint32_t __stdcall CAN_Write(uint16_t channel,const PcanMsg *message){
 (void)channel;EnterCriticalSection(&lock);
 if(message->id==WB_PING){uint8_t pong[8]={4,0xa0,26,11,4,0,0,0};push(WB_ACK,1,pong,8);if(!single)push(WB_ACK,1,pong,8);}
 if((message->id&0xffff0000u)==WBM_INDEX_REQUEST_BASE&&message->len==8&&message->data[1]==4){
  for(int node=0;node<2;node++)if(message->id==wbm_index_request(4,indices[node])){
   uint8_t old=indices[node],op=message->data[0];uint8_t reply[8]={op,message->data[2],0,0,0,0,0,0};
   if(op==WBM_INFO){reply[3]=WBM_BOARD_F0;reply[4]=WBM_INDEX_VERSION;reply[5]=WBM_CAPS;reply[7]=1;}
   else if(op==WBM_RGET||op==WBM_RSET){if(op==WBM_RSET){if(message->data[6]!=WBM_CONFIRM)reply[2]=WBM_INVALID;else{trims[node]=le16(message->data+3);sensors[node]=message->data[5];}}reply[3]=(uint8_t)trims[node];reply[4]=(uint8_t)(trims[node]>>8);reply[5]=sensors[node];reply[6]=1;}
   else if(op==WBM_CONFIG||op==WBM_APPLY||op==WBM_DEFAULTS){
    if(op==WBM_APPLY){if(message->data[7]!=WBM_CONFIRM)reply[2]=WBM_INVALID;else{indices[node]=message->data[3];sensors[node]=message->data[4];rates[node]=message->data[5];flags[node]=message->data[6];}}
    if(op==WBM_DEFAULTS){indices[node]=0;sensors[node]=0;trims[node]=0;flags[node]=3;}
    reply[3]=indices[node];reply[4]=sensors[node];reply[5]=rates[node];reply[6]=flags[node];reply[7]=0;
   }else if(op==WBM_DIAG1){uint16_t nominal=(uint16_t)(1000+node);reply[3]=0x41;reply[4]=1;reply[5]=(uint8_t)nominal;reply[6]=(uint8_t)(nominal>>8);reply[7]=2;}
   else if(op==WBM_DIAG2){uint16_t measured=(uint16_t)(2000+node);reply[3]=(uint8_t)measured;reply[4]=(uint8_t)(measured>>8);reply[5]=0x0c;reply[6]=0x30;reply[7]=120;}
   else reply[2]=WBM_UNSUPPORTED;
   push(wbm_index_reply(4,old),1,reply,8);
  }
 }
 LeaveCriticalSection(&lock);return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)instance;(void)reserved;if(reason==DLL_PROCESS_ATTACH)InitializeCriticalSection(&lock);if(reason==DLL_PROCESS_DETACH)DeleteCriticalSection(&lock);return TRUE;}
