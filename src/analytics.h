#ifndef WB_ANALYTICS_H
#define WB_ANALYTICS_H
#include "protocol.h"
#include "service_protocol.h"
#define TRACE_SIZE 3000
#define EVENT_SIZE 1024
#define CAN_STATS_SIZE 2048
typedef struct {uint32_t t,diagT;uint16_t lambda,temp,esr,dc;uint8_t heater,pump,valid,hasDiag,hasStandard;} TracePoint;
typedef struct {TracePoint point[TRACE_SIZE];unsigned head,count;uint32_t lastSample,first,last,diagLast,warmStart,warmMs,frames;uint16_t latestEsr,latestDc,latestLambda,latestTemp;uint8_t latestHeater,latestPump,latestValid;uint8_t hasData,hasDiag,state,warming,stale,warmComplete;} ChannelStats;
typedef struct {uint32_t t;uint16_t index;uint8_t code;} Event;
typedef struct {uint32_t id,first,last,count,maxGap;uint8_t ext,len,used;} CanStat;
static ChannelStats channels[256];
static Event events[EVENT_SIZE];static unsigned eventHead,eventCount;
static CanStat canStats[CAN_STATS_SIZE];static unsigned canStatOverflow;
static uint64_t rxFrames,txFrames;
static void add_event(unsigned index,uint8_t code,uint32_t now){events[eventHead]=(Event){now,(uint16_t)index,code};eventHead=(eventHead+1)%EVENT_SIZE;if(eventCount<EVENT_SIZE)eventCount++;}
static int analytic_sample(ChannelStats *c,uint32_t now){
 if(c->count&&now-c->lastSample<100)return 0;
 c->lastSample=now;int fresh=c->hasData&&now-c->last<=250;
 TracePoint *p=&c->point[c->head];
 *p=(TracePoint){now,c->diagLast,c->latestLambda,c->latestTemp,c->latestEsr,c->latestDc,c->latestHeater,c->latestPump,(uint8_t)(fresh&&c->latestValid),(uint8_t)(c->hasDiag&&now-c->diagLast<=250),(uint8_t)fresh};
 c->head=(c->head+1)%TRACE_SIZE;if(c->count<TRACE_SIZE)c->count++;return 1;
}
/* codes 0..7 mirror state changes; 100=stream lost,101=stream returned,
 * 102=invalid lambda in closed loop. Initial non-error state is not a fault. */
static void analytic_frame(const Frame *f,int tx,uint32_t now){
 if(tx){txFrames++;return;}rxFrames++;
 unsigned slot=(unsigned)((f->id*2654435761u+(unsigned)f->ext)%CAN_STATS_SIZE);
 for(unsigned n=0;n<CAN_STATS_SIZE;n++){
  CanStat *s=&canStats[(slot+n)%CAN_STATS_SIZE];
  if(!s->used){*s=(CanStat){f->id,now,now,1,0,f->ext,f->len,1};break;}
  if(s->id==f->id&&s->ext==f->ext){uint32_t gap=now-s->last;if(gap>s->maxGap)s->maxGap=gap;s->last=now;s->count++;s->len=f->len;break;}
  if(n==CAN_STATS_SIZE-1)canStatOverflow++;
 }
 if(f->ext||f->id<0x190||f->id>0x38f||f->len!=8)return;
 unsigned idx=(f->id-0x190)/2;ChannelStats *c=&channels[idx];
 if(f->id&1){uint8_t st=f->data[5];
  if(!c->hasDiag||st!=c->state){
   add_event(idx,st,now);
   if(st==1){c->warmStart=now;c->warming=1;c->warmComplete=0;}
   if(st==2&&c->warming){c->warmMs=now-c->warmStart;c->warming=0;c->warmComplete=1;}
   if(st!=1&&st!=2)c->warming=0;
  }
  c->state=st;c->hasDiag=1;c->diagLast=now;
  return;
 }
 if(f->data[0]!=0xa0)return;
 if(!c->hasData){c->first=now;c->hasData=1;}else if(c->stale){add_event(idx,101,now);c->stale=0;}
 c->frames++;c->last=now;
 c->latestLambda=le16(f->data+2);c->latestTemp=le16(f->data+4);c->latestValid=f->data[1]&1;
 if(!analytic_sample(c,now)&&c->count){TracePoint *last=&c->point[(c->head+TRACE_SIZE-1)%TRACE_SIZE];if((uint32_t)(now-last->t)<=100){last->lambda=c->latestLambda;last->temp=c->latestTemp;last->valid=c->latestValid;last->hasStandard=1;}}
 if(!c->latestValid&&c->hasDiag&&c->state==2){static uint32_t lastWarning[256];if(now-lastWarning[idx]>5000){add_event(idx,102,now);lastWarning[idx]=now;}}
}
static void analytic_diag(unsigned idx,const Frame *f){
 ChannelStats *c=&channels[idx];c->latestEsr=le16(f->data);c->latestDc=le16(f->data+2);c->latestPump=f->data[4];c->latestHeater=f->data[6];analytic_sample(c,c->diagLast);
 if(c->count){TracePoint *p=&c->point[(c->head+TRACE_SIZE-1)%TRACE_SIZE];if((uint32_t)(c->diagLast-p->t)<=100){p->esr=c->latestEsr;p->dc=c->latestDc;p->pump=c->latestPump;p->heater=c->latestHeater;p->diagT=c->diagLast;p->hasDiag=1;}}
}
static void analytic_tick(uint32_t now){for(unsigned i=0;i<256;i++){ChannelStats *c=&channels[i];if(c->hasData&&!c->stale&&now-c->last>1000){c->stale=1;c->warming=0;add_event(i,100,now);}}}
static int backup_encode(uint8_t out[20],uint8_t hw,const uint8_t cfg[5],uint16_t trim){
 if(hw>8||!wbm_valid_config(cfg[1],cfg[2],cfg[3])||!(trim==0||(trim>=3000&&trim<=30000))||(cfg[1]==4&&!trim))return 0;
 memset(out,0,20);memcpy(out,"WBF0",4);out[4]=2;out[5]=WBM_BOARD_F0;out[6]=hw;memcpy(out+8,cfg,4);out[12]=(uint8_t)trim;out[13]=(uint8_t)(trim>>8);
 uint32_t crc=crc32wb(out,16);for(int i=0;i<4;i++)out[16+i]=(uint8_t)(crc>>(8*i));return 1;
}
static int backup_decode(const uint8_t *in,size_t size,uint8_t cfg[5],uint16_t *trim){
 if(size!=20||memcmp(in,"WBF0",4)||in[4]!=2||in[5]!=WBM_BOARD_F0||in[6]>8||in[7]||in[14]||in[15]||le32(in+16)!=crc32wb(in,16)||!wbm_valid_config(in[9],in[10],in[11]))return 0;
 uint16_t r=le16(in+12);if(!(r==0||(r>=3000&&r<=30000))||(in[9]==4&&!r))return 0;
 memcpy(cfg,in+8,4);cfg[4]=0xff;*trim=r;return 1;
}
#endif
