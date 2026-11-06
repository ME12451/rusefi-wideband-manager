/* Linux binding to the same tested protocol/analytics code as the Windows UI. */
#include "../src/trace_math.h"
#include <limits.h>
void wb_clear(void){memset(channels,0,sizeof channels);memset(canStats,0,sizeof canStats);eventHead=eventCount=canStatOverflow=0;rxFrames=txFrames=0;}
void wb_frame(uint32_t id,int ext,const uint8_t *data,unsigned len,int tx,uint32_t now){if(len>8)return;Frame f={0};f.id=id;f.ext=!!ext;f.len=len;memcpy(f.data,data,len);analytic_frame(&f,tx,now);if(!tx&&!ext&&id>=0x191&&id<=0x38f&&(id&1)&&len==8)analytic_diag((id-0x190)/2,&f);}
void wb_tick(uint32_t now){analytic_tick(now);}
/* Flat ABI: t, six values, six usable flags, standard fresh, diagnostic fresh. */
unsigned wb_points(unsigned index,double *out,unsigned capacity){if(index>255)return 0;ChannelStats *c=&channels[index];unsigned n=c->count;if(n>capacity)n=capacity;for(unsigned i=0;i<n;i++){TracePoint *p=&c->point[(c->head+TRACE_SIZE-n+i)%TRACE_SIZE];double *r=out+i*15;r[0]=p->t;for(int k=0;k<6;k++){r[1+k]=trace_value(p,k);r[7+k]=trace_usable(p,k);}r[13]=p->hasStandard;r[14]=p->hasDiag;}return n;}
unsigned wb_events(uint32_t *out,unsigned capacity){unsigned n=eventCount;if(n>capacity)n=capacity;for(unsigned i=0;i<n;i++){Event e=events[(eventHead+EVENT_SIZE-n+i)%EVENT_SIZE];out[3*i]=e.t;out[3*i+1]=e.index;out[3*i+2]=e.code;}return n;}
unsigned wb_stats(double *out,unsigned capacity){unsigned n=0;for(unsigned i=0;i<CAN_STATS_SIZE&&n<capacity;i++){CanStat *s=&canStats[i];if(!s->used)continue;double *r=out+8*n++;r[0]=s->id;r[1]=s->ext;r[2]=s->len;r[3]=s->count;r[4]=s->first;r[5]=s->last;r[6]=s->maxGap;r[7]=s->last==s->first?0:(s->count-1)*1000./(uint32_t)(s->last-s->first);}return n;}
void wb_bounds(int metric,int automatic,double min,double max,int count,double *low,double *high){trace_bounds(metric,automatic,min,max,count,low,high);}
int wb_image(const uint8_t *p,unsigned n){return valid_image(p,n);}
int wb_backup(uint8_t *out,unsigned hw,const uint8_t *cfg,unsigned trim){return hw<=8&&trim<=UINT16_MAX&&backup_encode(out,hw,cfg,trim);}
int wb_restore(const uint8_t *in,unsigned n,uint8_t *cfg,uint16_t *trim){return backup_decode(in,n,cfg,trim);}
