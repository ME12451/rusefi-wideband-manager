#include <assert.h>
#include "../src/trace_math.h"
int main(void){
 TracePoint a={0};a.t=1000;a.diagT=990;a.lambda=10000;a.temp=780;a.esr=300;a.dc=450;a.heater=128;a.pump=64;a.valid=1;a.hasDiag=1;a.hasStandard=1;
 assert(trace_value(&a,TRACE_LAMBDA)==1.0);assert(trace_value(&a,TRACE_TEMP)==780);assert(fabs(trace_value(&a,TRACE_HEATER)-12800./255.)<1e-8);assert(trace_value(&a,TRACE_NERNST)==.45);assert(trace_usable(&a,TRACE_LAMBDA));assert(trace_usable(&a,TRACE_ESR));
 TracePoint b=a;b.t=1100;b.diagT=1101;assert(trace_linkable(&a,&b,TRACE_NERNST));
 b.t=1600;assert(!trace_linkable(&a,&b,TRACE_LAMBDA));
 b=a;b.valid=0;assert(!trace_usable(&b,TRACE_LAMBDA)&&trace_usable(&b,TRACE_TEMP));
 b=a;b.hasDiag=0;assert(!trace_usable(&b,TRACE_HEATER)&&trace_usable(&b,TRACE_LAMBDA));
 b=a;b.diagT=749;assert(!trace_usable(&b,TRACE_NERNST));b.diagT=1101;assert(!trace_usable(&b,TRACE_NERNST));
 b=a;b.hasStandard=0;assert(!trace_usable(&b,TRACE_TEMP)&&!trace_usable(&b,TRACE_LAMBDA)&&trace_usable(&b,TRACE_ESR));
 assert(trace_visible(1000,500,500));assert(!trace_visible(1000,499,500));assert(!trace_visible(1000,1001,500));assert(trace_visible(50,0xfffffff0u,100));
 double low,high;trace_bounds(TRACE_LAMBDA,0,0,0,0,&low,&high);assert(low==.6&&high==1.6);
 trace_bounds(TRACE_LAMBDA,1,1,1,2,&low,&high);assert(low<1&&high>1);
 trace_bounds(TRACE_HEATER,1,90,100,10,&low,&high);assert(high==100&&low<90);
 trace_bounds(TRACE_TEMP,1,0,0,10,&low,&high);assert(low==0&&high>0);
 // Diagnostic-only reception continues the trace, without inventing standard data.
 Frame d={0};d.id=0x191;d.len=8;d.data[0]=44;d.data[1]=1;d.data[2]=0xc2;d.data[3]=1;d.data[5]=2;d.data[6]=128;
 analytic_frame(&d,0,2000);analytic_diag(0,&d);assert(channels[0].count==1);const TracePoint *p=&channels[0].point[0];assert(!p->hasStandard&&p->hasDiag&&p->esr==300&&!trace_usable(p,TRACE_TEMP)&&trace_usable(p,TRACE_NERNST));
 Frame f={0};f.id=0x190;f.len=8;f.data[0]=0xa0;f.data[1]=1;f.data[2]=0x10;f.data[3]=0x27;f.data[4]=12;f.data[5]=3;
 analytic_frame(&f,0,2100);p=&channels[0].point[1];assert(p->hasStandard&&p->hasDiag&&p->lambda==10000);
 analytic_frame(&f,0,2400);p=&channels[0].point[2];assert(p->hasStandard&&!p->hasDiag);
 analytic_frame(&d,0,2700);analytic_diag(0,&d);p=&channels[0].point[3];assert(!p->hasStandard&&p->hasDiag&&!p->valid);
 puts("Trace units, dual-axis bounds, wrap-safe windows, invalid/stale data and diagnostic-only sampling passed.");return 0;
}
