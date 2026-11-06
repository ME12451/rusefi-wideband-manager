#include <assert.h>
#include "../src/analytics.h"
static Frame data(unsigned idx,unsigned lambda){Frame f={0};f.id=0x190+idx*2;f.len=8;f.data[0]=0xa0;f.data[1]=1;f.data[2]=lambda;f.data[3]=lambda>>8;f.data[4]=0x20;f.data[5]=3;return f;}
static Frame diag(unsigned idx,unsigned st){Frame f={0};f.id=0x191+idx*2;f.len=8;f.data[0]=100;f.data[2]=0xc2;f.data[3]=1;f.data[5]=st;f.data[6]=128;return f;}
int main(void){
 uint16_t trim;uint8_t cfg[5]={255,3,1,3,0},encoded[20],decoded[5];assert(backup_encode(encoded,8,cfg,12345));assert(backup_decode(encoded,20,decoded,&trim));assert(!memcmp(cfg,decoded,4));assert(decoded[4]==255);
 for(unsigned n=0;n<160;n++){uint8_t broken[20];memcpy(broken,encoded,20);broken[n/8]^=1<<(n%8);assert(!backup_decode(broken,20,decoded,&trim));}
 assert(!backup_decode(encoded,19,decoded,&trim));cfg[1]=5;assert(!backup_encode(encoded,4,cfg,12345));assert(!wbm_valid_config(0,2,3));assert(!wbm_valid_config(0,1,4));
 uint8_t reply[8]={WBM_CONFIG,17,0,0,0,0,0,0};assert(wbm_match(WBM_REPLY_BASE|4,8,reply,4,WBM_CONFIG,17));assert(!wbm_match(WBM_REPLY_BASE|5,8,reply,4,WBM_CONFIG,17));assert(!wbm_match(WBM_REPLY_BASE|4,7,reply,4,WBM_CONFIG,17));assert(!wbm_match(WBM_REPLY_BASE|4,8,reply,4,WBM_CONFIG,18));assert(!wbm_match(WBM_REPLY_BASE|4,8,reply,4,WBM_APPLY,17));
 Frame f=data(0,10000);analytic_frame(&f,0,1000);assert(channels[0].count==1&&channels[0].last==1000);
 analytic_frame(&f,0,1010);assert(channels[0].count==1);Frame d=diag(0,1);analytic_frame(&d,0,1100);analytic_diag(0,&d);assert(channels[0].warming);d.data[5]=2;analytic_frame(&d,0,5100);analytic_diag(0,&d);assert(channels[0].warmComplete&&channels[0].warmMs==4000);
 analytic_frame(&f,0,5100);assert(channels[0].point[(channels[0].head+TRACE_SIZE-1)%TRACE_SIZE].esr==100);analytic_tick(6101);assert(channels[0].stale&&!channels[0].warming);assert(events[(eventHead+EVENT_SIZE-1)%EVENT_SIZE].code==100);analytic_frame(&f,0,6200);assert(!channels[0].stale&&events[(eventHead+EVENT_SIZE-1)%EVENT_SIZE].code==101);
 d=diag(1,2);analytic_frame(&d,0,7000);assert(!channels[1].warmComplete); // joining already-running sensor cannot invent warmup
 for(unsigned n=0;n<TRACE_SIZE+100;n++){analytic_frame(&f,0,10000+n*100);}
 assert(channels[0].count==TRACE_SIZE&&channels[0].head<TRACE_SIZE);
 for(unsigned n=0;n<EVENT_SIZE+100;n++){add_event(255,4,n);}
 assert(eventCount==EVENT_SIZE);
 uint64_t rx=rxFrames,tx=txFrames;analytic_frame(&f,1,900000);assert(rxFrames==rx&&txFrames==tx+1);
 f=data(2,10000);f.data[0]=0xff;analytic_frame(&f,0,901000);assert(!channels[2].hasData);
 puts("Backup CRC/validation, reply correlation, multi-channel analytics, warmup, gaps and rings passed.");return 0;
}
