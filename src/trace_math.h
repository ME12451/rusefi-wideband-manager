#ifndef WB_TRACE_MATH_H
#define WB_TRACE_MATH_H
#include "analytics.h"
#include <math.h>
enum { TRACE_LAMBDA,TRACE_TEMP,TRACE_HEATER,TRACE_PUMP,TRACE_NERNST,TRACE_ESR,TRACE_METRICS };
static double trace_value(const TracePoint *p,int metric){switch(metric){case TRACE_LAMBDA:return p->lambda/10000.;case TRACE_TEMP:return p->temp;case TRACE_HEATER:return p->heater*100./255.;case TRACE_PUMP:return p->pump*100./255.;case TRACE_NERNST:return p->dc/1000.;default:return p->esr;}}
static int trace_usable(const TracePoint *p,int metric){if(metric==TRACE_LAMBDA)return p->hasStandard&&p->valid;if(metric==TRACE_TEMP)return p->hasStandard;int32_t delta=(int32_t)(p->t-p->diagT);return p->hasDiag&&delta>=-100&&delta<=250;}
static int trace_visible(uint32_t end,uint32_t time,uint32_t span){return (uint32_t)(end-time)<=span;}
static int trace_linkable(const TracePoint *a,const TracePoint *b,int metric){return trace_usable(a,metric)&&trace_usable(b,metric)&&(uint32_t)(b->t-a->t)<=350;}
static void trace_bounds(int metric,int automatic,double minimum,double maximum,int count,double *low,double *high){
 static const double defaults[TRACE_METRICS][2]={{.6,1.6},{0,1100},{0,100},{0,100},{0,1},{0,5000}};
 *low=defaults[metric][0];*high=defaults[metric][1];if(!automatic||!count)return;
 double floorSpan=metric==TRACE_LAMBDA?.1:metric==TRACE_NERNST?.1:metric==TRACE_ESR?50:20;
 double range=maximum-minimum;if(range<floorSpan)range=floorSpan;double mid=(minimum+maximum)*.5;
 *low=mid-range*.6;*high=mid+range*.6;
 if(metric!=TRACE_LAMBDA&&*low<0)*low=0;
 if((metric==TRACE_HEATER||metric==TRACE_PUMP)&&*high>100)*high=100;
 if(*high<=*low)*high=*low+floorSpan;
}
#endif
