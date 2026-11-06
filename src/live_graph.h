/* Shared, buffered native time-series widget used by main and tools windows. */
#include <windowsx.h>
#include "trace_math.h"
#define GRAPH_TIMER 73
#define GRAPH_EXPORT (WM_APP+32)
typedef struct {
 HWND toggles[TRACE_METRICS],timeBox,pauseButton,autoButton,exportButton;
 unsigned mask,span;int automatic,paused,mouseX,dragging,dragX,index;
 uint32_t pauseNow,end,dragOffset;unsigned frozenCount;
 TracePoint frozen[TRACE_SIZE];
 RECT plot;
} GraphState;
static const char *graphNames[]={"Lambda","Temp","Heater","Pump PWM","Nernst DC","ESR"};
static const char *graphUnits[]={"","C","%","%","V","Ohm"};
static const COLORREF graphColors[]={RGB(61,220,235),RGB(255,178,101),RGB(255,213,103),RGB(179,156,255),RGB(79,218,166),RGB(255,135,187)};
static const uint32_t graphSpans[]={15000,30000,60000,300000};
static int graph_index(const GraphState *g){return g->paused?g->index:selected_index();}
static unsigned graph_count(const GraphState *g){return g->paused?g->frozenCount:channels[graph_index(g)].count;}
static const TracePoint *graph_point(const GraphState *g,unsigned n){if(g->paused)return &g->frozen[n];const ChannelStats *c=&channels[graph_index(g)];return &c->point[(c->head+TRACE_SIZE-c->count+n)%TRACE_SIZE];}
static uint32_t graph_end(const GraphState *g){return g->paused?g->end:GetTickCount();}
static void graph_set_pause(HWND h,GraphState *g,int pause){
 if(pause&&!g->paused){g->index=selected_index();const ChannelStats *c=&channels[g->index];g->frozenCount=c->count;for(unsigned n=0;n<c->count;n++)g->frozen[n]=c->point[(c->head+TRACE_SIZE-c->count+n)%TRACE_SIZE];g->pauseNow=GetTickCount();g->end=g->pauseNow;}
 g->paused=pause;SetWindowText(g->pauseButton,pause?"Resume / Live":"Pause");InvalidateRect(g->pauseButton,NULL,TRUE);InvalidateRect(h,NULL,FALSE);
}
static void graph_pan(GraphState *g,int64_t offset){uint32_t max=g->frozenCount?g->pauseNow-g->frozen[0].t:0;if(offset<0)offset=0;if((uint64_t)offset>max)offset=max;g->end=g->pauseNow-(uint32_t)offset;}
static void graph_zoom(HWND h,GraphState *g,int direction){int current=(int)SendMessage(g->timeBox,CB_GETCURSEL,0,0);current+=direction;if(current<0)current=0;if(current>3)current=3;g->span=graphSpans[current];SendMessage(g->timeBox,CB_SETCURSEL,current,0);InvalidateRect(h,NULL,FALSE);}
static HWND graph_control(HWND h,const char *cls,const char *text,DWORD style,int id){HWND c=CreateWindowEx(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,10,10,h,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);SendMessage(c,WM_SETFONT,(WPARAM)uiPlotFont,TRUE);return c;}
static void graph_layout(HWND h,GraphState *g){RECT r;GetClientRect(h,&r);int p=px(12),row=px(30),bw=px(90),gap=px(6);for(int i=0;i<TRACE_METRICS;i++)MoveWindow(g->toggles[i],p+i*(bw+gap),p,bw,row,TRUE);MoveWindow(g->pauseButton,r.right-p-px(125),p,px(125),row,TRUE);int y=p+row+px(9);MoveWindow(g->timeBox,p+px(46),y,px(140),px(180),TRUE);MoveWindow(g->autoButton,p+px(202),y,px(128),row,TRUE);MoveWindow(g->exportButton,p+px(344),y,px(155),row,TRUE);int compact=r.bottom<px(360);g->plot=(RECT){px(86),y+row+px(compact?8:36),r.right-px(86),r.bottom-px(compact?24:42)};}
static int graph_nearest(const GraphState *g,uint32_t time){int result=-1;uint32_t closest=~0u,end=graph_end(g);unsigned count=graph_count(g);for(unsigned n=0;n<count;n++){const TracePoint *p=graph_point(g,n);if(!trace_visible(end,p->t,g->span))continue;int32_t delta=(int32_t)(p->t-time);uint32_t distance=(uint32_t)(delta<0?-(int64_t)delta:delta);if(distance<closest){closest=distance;result=(int)n;}}return result;}
static void graph_metric_bounds(const GraphState *g,int metric,double *low,double *high){double min=1e12,max=-1e12;int points=0;uint32_t end=graph_end(g);for(unsigned n=0;n<graph_count(g);n++){const TracePoint *p=graph_point(g,n);if(!trace_visible(end,p->t,g->span)||!trace_usable(p,metric))continue;double value=trace_value(p,metric);if(value<min)min=value;if(value>max)max=value;points++;}trace_bounds(metric,g->automatic,min,max,points,low,high);}
static int graph_y(double value,double low,double high,RECT r){double norm=(value-low)/(high-low);if(norm<0)norm=0;if(norm>1)norm=1;return r.bottom-(int)(norm*(r.bottom-r.top));}
static void graph_line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color,int style,int width){HPEN p=CreatePen(style,width,color);HGDIOBJ old=SelectObject(dc,p);MoveToEx(dc,x1,y1,NULL);LineTo(dc,x2,y2);SelectObject(dc,old);DeleteObject(p);}
static void graph_paint(HWND h,GraphState *g,HDC dc,RECT r){
 (void)h;
 const COLORREF bg=RGB(17,30,46),gridColor=RGB(43,59,78),muted=RGB(139,157,177);
 ui_fill(dc,r,bg);RECT note={px(15),px(55),px(52),px(80)};ui_text(dc,"Time",note,uiPlotFont,muted,DT_SINGLELINE|DT_VCENTER);
 RECT hint={px(530),px(52),r.right-px(12),px(86)};ui_text(dc,"Wheel: zoom   /   Drag: history",hint,uiPlotFont,muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
 uint32_t end=graph_end(g);int index=graph_index(g);char title[230];snprintf(title,sizeof title,"%s  |  Index %d  |  %u s  |  %s%s",g->paused?"PAUSE":"LIVE",index,g->span/1000,g->automatic?"Auto scales":"Fixed scales",demoMode?"  /  DEMO - simulated values":"");
 if(r.bottom>=px(360)){RECT titleR={px(14),g->plot.top-px(32),r.right-px(14),g->plot.top-px(5)};ui_text(dc,title,titleR,uiPlotFont,RGB(205,220,236),DT_SINGLELINE|DT_END_ELLIPSIS);}
 if(g->plot.right<=g->plot.left||g->plot.bottom-g->plot.top<px(90)){ui_text(dc,"Enlarge the window",r,font,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return;}
 unsigned groupMasks[]={1u<<TRACE_LAMBDA,(1u<<TRACE_TEMP)|(1u<<TRACE_ESR),(1u<<TRACE_HEATER)|(1u<<TRACE_PUMP),1u<<TRACE_NERNST};
 int lanes=0;for(int group=0;group<4;group++)if(g->mask&groupMasks[group])lanes++;
 if(!lanes){ui_text(dc,"Select at least one trace.",g->plot,font,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return;}
 int cursor=g->mouseX>=g->plot.left&&g->mouseX<=g->plot.right;uint32_t aim=end;if(cursor)aim=end-(uint32_t)((uint64_t)(g->plot.right-g->mouseX)*g->span/(g->plot.right-g->plot.left));int nearest=graph_nearest(g,aim);const TracePoint *reading=nearest>=0?graph_point(g,(unsigned)nearest):NULL;
 int lane=0,laneHeight=(g->plot.bottom-g->plot.top)/lanes;
 for(int group=0;group<4;group++){
  unsigned active=g->mask&groupMasks[group];if(!active)continue;
  RECT area={g->plot.left,g->plot.top+lane*laneHeight,g->plot.right,g->plot.top+(lane+1)*laneHeight-px(8)};lane++;
  int compact=laneHeight<px(65);RECT lineArea=area;lineArea.top+=px(compact?18:27);lineArea.bottom-=px(compact?0:5);if(lineArea.bottom<=lineArea.top)continue;
  RECT band={0,area.top,r.right,area.bottom};ui_fill(dc,band,lane%2?RGB(19,34,51):RGB(21,37,55));
  for(int t=0;t<=4;t++){int x=lineArea.left+(lineArea.right-lineArea.left)*t/4;graph_line(dc,x,lineArea.top,x,lineArea.bottom,gridColor,PS_SOLID,1);}
  for(int t=0;t<=2;t++){int y=lineArea.top+(lineArea.bottom-lineArea.top)*t/2;graph_line(dc,lineArea.left,y,lineArea.right,y,gridColor,PS_SOLID,1);}
  int metricPos=0;
  for(int metric=0;metric<TRACE_METRICS;metric++){
   if(!(active&(1u<<metric)))continue;
   double low,high;graph_metric_bounds(g,metric,&low,&high);COLORREF color=graphColors[metric];
   char legend[180];if(reading&&trace_usable(reading,metric)){snprintf(legend,sizeof legend,"%s  %.*f %s%s",graphNames[metric],metric==TRACE_LAMBDA?4:metric==TRACE_NERNST?3:1,trace_value(reading,metric),graphUnits[metric],!cursor&&(uint32_t)(end-reading->t)>1000?"  [STALE]":"");}else snprintf(legend,sizeof legend,"%s  -- %s",graphNames[metric],graphUnits[metric]);
   RECT lr={area.left+metricPos*px(280),area.top,area.right,area.top+px(24)};ui_text(dc,legend,lr,uiPlotFont,color,DT_SINGLELINE|DT_END_ELLIPSIS);
   int rightAxis=metricPos>0&&group==1;
   if(metricPos==0||rightAxis){for(int tick=0;tick<=2;tick++){if(lineArea.bottom-lineArea.top<px(36)&&tick!=1)continue;double value=high-(high-low)*tick/2;char text[30];snprintf(text,sizeof text,metric==TRACE_LAMBDA||metric==TRACE_NERNST?"%.2f":"%.0f",value);int y=lineArea.top+(lineArea.bottom-lineArea.top)*tick/2;RECT tr={rightAxis?lineArea.right+px(8):px(4),y-px(10),rightAxis?r.right-px(5):lineArea.left-px(10),y+px(12)};ui_text(dc,text,tr,uiPlotFont,color,(rightAxis?DT_LEFT:DT_RIGHT)|DT_SINGLELINE);}}
   if(metric==TRACE_LAMBDA&&low<1&&high>1){int y=graph_y(1,low,high,lineArea);graph_line(dc,lineArea.left,y,lineArea.right,y,RGB(71,116,125),PS_DOT,1);}
   HPEN pen=CreatePen(PS_SOLID,px(2)>0?px(2):1,color);HGDIOBJ old=SelectObject(dc,pen);int havePrevious=0;const TracePoint *previous=NULL;
   int clip=SaveDC(dc);IntersectClipRect(dc,lineArea.left,lineArea.top-1,lineArea.right+1,lineArea.bottom+1);
   for(unsigned n=0;n<graph_count(g);n++){
    const TracePoint *p=graph_point(g,n);if(!trace_visible(end,p->t,g->span))continue;
    int x=lineArea.right-(int)((uint64_t)(end-p->t)*(lineArea.right-lineArea.left)/g->span);
    if(!trace_usable(p,metric)){havePrevious=0;if(metric==TRACE_LAMBDA){graph_line(dc,x,lineArea.bottom-px(5),x,lineArea.bottom,RGB(229,98,103),PS_SOLID,1);}continue;}
    int y=graph_y(trace_value(p,metric),low,high,lineArea);
    if(havePrevious&&trace_linkable(previous,p,metric))LineTo(dc,x,y);else MoveToEx(dc,x,y,NULL);
    previous=p;havePrevious=1;
   }
   RestoreDC(dc,clip);SelectObject(dc,old);DeleteObject(pen);metricPos++;
  }
  if(cursor){graph_line(dc,g->mouseX,lineArea.top,g->mouseX,lineArea.bottom,RGB(214,228,243),PS_DOT,1);}
 }
 for(int tick=0;tick<=4;tick++){uint32_t t=end-(uint32_t)((uint64_t)g->span*(4-tick)/4);int32_t elapsed=(int32_t)(t-sessionStart);char text[40];if(elapsed<0)snprintf(text,sizeof text,"--");else snprintf(text,sizeof text,"%.1f s",elapsed/1000.);int x=g->plot.left+(g->plot.right-g->plot.left)*tick/4;RECT tr={x-px(45),g->plot.bottom+px(3),x+px(45),g->plot.bottom+px(25)};ui_text(dc,text,tr,uiPlotFont,muted,DT_CENTER|DT_SINGLELINE);}
 if(!graph_count(g)){RECT empty=g->plot;ui_text(dc,demoMode?"Starting demo...":"No data yet - connect CAN and select a measurement index.",empty,font,RGB(198,214,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
 RECT footer={px(14),r.bottom-px(20),r.right-px(14),r.bottom};char ft[180];if(cursor&&reading)snprintf(ft,sizeof ft,"Cursor %.3f s  /  Lambda %s  /  Diagnostics %s",(int32_t)(reading->t-sessionStart)/1000.,reading->valid?"valid":"INVALID",reading->hasDiag?"present":"missing / stale");else snprintf(ft,sizeof ft,"Time since program start  /  Red: invalid lambda  /  Gap: missing or stale data");if(r.bottom>=px(360))ui_text(dc,ft,footer,uiPlotFont,muted,DT_SINGLELINE|DT_END_ELLIPSIS);
}
static void graph_export(HWND h,const GraphState *g){
 char path[MAX_PATH];snprintf(path,sizeof path,"%swideband-index-%d.csv",demoMode?"demo-":"",graph_index(g));
 OPENFILENAME of={0};of.lStructSize=sizeof of;of.hwndOwner=GetAncestor(h,GA_ROOT);of.lpstrFilter="CSV\0*.csv\0";of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.lpstrDefExt="csv";of.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
 if(!GetSaveFileName(&of))return;
 FILE *f=fopen(path,"w");if(!f){MessageBox(h,"Cannot create CSV file.",APP,MB_OK|MB_ICONERROR);return;}
 fprintf(f,"source,host_tick_ms,elapsed_s,index,lambda,lambda_valid,temp_C,heater_percent,pump_percent,esr_ohm,nernst_V,std_fresh,diag_fresh\n");
 uint32_t end=graph_end(g);
 for(unsigned n=0;n<graph_count(g);n++){
  const TracePoint *p=graph_point(g,n);if(!trace_visible(end,p->t,g->span))continue;
  char field[TRACE_METRICS][40]={{0}};
  for(int m=0;m<TRACE_METRICS;m++)if(trace_usable(p,m))snprintf(field[m],sizeof field[m],m==TRACE_LAMBDA?"%.4f":m==TRACE_NERNST?"%.3f":"%.2f",trace_value(p,m));
  fprintf(f,"%s,%u,%.3f,%d,%s,%u,%s,%s,%s,%s,%s,%u,%u\n",demoMode?"DEMO":"CAN",p->t,(int32_t)(p->t-sessionStart)/1000.,graph_index(g),field[0],trace_usable(p,0)?1:0,field[1],field[2],field[3],field[5],field[4],p->hasStandard,p->hasDiag);
 }
 int ok=!ferror(f);if(fclose(f))ok=0;if(!ok)MessageBox(h,"Could not write the complete CSV file.",APP,MB_OK|MB_ICONERROR);
}
static LRESULT CALLBACK graph_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 GraphState *g=(GraphState*)GetWindowLongPtr(h,GWLP_USERDATA);
 switch(msg){
 case WM_CREATE:{SetWindowLongPtr(h,GWL_STYLE,GetWindowLongPtr(h,GWL_STYLE)|WS_CLIPCHILDREN);g=calloc(1,sizeof *g);if(!g)return -1;SetWindowLongPtr(h,GWLP_USERDATA,(LONG_PTR)g);g->mask=63;g->span=60000;g->mouseX=-1;for(int i=0;i<TRACE_METRICS;i++)g->toggles[i]=graph_control(h,"BUTTON",graphNames[i],BS_OWNERDRAW,500+i);g->timeBox=graph_control(h,"COMBOBOX","",CBS_DROPDOWNLIST,510);const char *times[]={"15 seconds","30 seconds","60 seconds","5 minutes"};for(int i=0;i<4;i++)SendMessage(g->timeBox,CB_ADDSTRING,0,(LPARAM)times[i]);SendMessage(g->timeBox,CB_SETCURSEL,2,0);g->pauseButton=graph_control(h,"BUTTON","Pause",BS_OWNERDRAW,511);g->autoButton=graph_control(h,"BUTTON","Auto scales: off",BS_OWNERDRAW,512);g->exportButton=graph_control(h,"BUTTON","CSV time window",BS_OWNERDRAW,513);graph_layout(h,g);SetTimer(h,GRAPH_TIMER,100,NULL);return 0;}
 case WM_SIZE:if(g)graph_layout(h,g);InvalidateRect(h,NULL,FALSE);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_TIMER:if(g&&!g->paused&&IsWindowVisible(h)&&!IsIconic(GetAncestor(h,GA_ROOT)))InvalidateRect(h,NULL,FALSE);return 0;
 case WM_DRAWITEM:{const DRAWITEMSTRUCT *d=(const DRAWITEMSTRUCT*)lp;if(!g)return 0;int id=(int)d->CtlID,active=id>=500&&id<506?!!(g->mask&(1u<<(id-500))):id==511?g->paused:id==512?g->automatic:0;COLORREF bg=active?RGB(43,65,86):RGB(27,44,63),fg=id>=500&&id<506?graphColors[id-500]:RGB(219,231,243);if(id>=500&&id<506&&!active)fg=RGB(110,130,150);if(d->itemState&ODS_SELECTED)bg=RGB(55,77,98);ui_fill(d->hDC,d->rcItem,bg);ui_outline(d->hDC,d->rcItem,active?fg:RGB(53,72,91),px(6));char text[80];GetWindowText(d->hwndItem,text,sizeof text);ui_text(d->hDC,text,d->rcItem,uiPlotFont,fg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(d->itemState&ODS_FOCUS){RECT r=d->rcItem;InflateRect(&r,-4,-4);DrawFocusRect(d->hDC,&r);}return TRUE;}
 case WM_COMMAND:if(!g)return 0;{int id=LOWORD(wp);if(id>=500&&id<506){g->mask^=1u<<(id-500);InvalidateRect(g->toggles[id-500],NULL,TRUE);}else if(id==510&&HIWORD(wp)==CBN_SELCHANGE)g->span=graphSpans[SendMessage(g->timeBox,CB_GETCURSEL,0,0)];else if(id==511)graph_set_pause(h,g,!g->paused);else if(id==512){g->automatic=!g->automatic;SetWindowText(g->autoButton,g->automatic?"Auto scales: on":"Auto scales: off");}else if(id==513)graph_export(h,g);InvalidateRect(h,NULL,FALSE);}return 0;
 case WM_MOUSEMOVE:if(g){int x=GET_X_LPARAM(lp);g->mouseX=x;if(g->dragging){int width=g->plot.right-g->plot.left;if(width>0)graph_pan(g,(int64_t)g->dragOffset+(int64_t)(x-g->dragX)*g->span/width);}TRACKMOUSEEVENT t={sizeof t,TME_LEAVE,h,0};TrackMouseEvent(&t);InvalidateRect(h,NULL,FALSE);}return 0;
 case WM_MOUSELEAVE:if(g&&!g->dragging){g->mouseX=-1;InvalidateRect(h,NULL,FALSE);}return 0;
 case WM_LBUTTONDOWN:if(g&&GET_Y_LPARAM(lp)>=g->plot.top){SetFocus(h);graph_set_pause(h,g,1);g->dragging=1;g->dragX=GET_X_LPARAM(lp);g->dragOffset=g->pauseNow-g->end;SetCapture(h);}return 0;
 case WM_LBUTTONUP:if(g){g->dragging=0;ReleaseCapture();}return 0;
 case WM_MOUSEWHEEL:if(g){if(GET_KEYSTATE_WPARAM(wp)&MK_SHIFT){graph_set_pause(h,g,1);graph_pan(g,(int64_t)(g->pauseNow-g->end)+(GET_WHEEL_DELTA_WPARAM(wp)>0?1:-1)*(int64_t)g->span/10);}else graph_zoom(h,g,GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1);InvalidateRect(h,NULL,FALSE);}return 0;
 case WM_KEYDOWN:if(g){if(wp==VK_SPACE)graph_set_pause(h,g,!g->paused);else if(wp==VK_HOME)graph_set_pause(h,g,0);else if(wp==VK_ADD||wp==VK_OEM_PLUS)graph_zoom(h,g,-1);else if(wp==VK_SUBTRACT||wp==VK_OEM_MINUS)graph_zoom(h,g,1);}return 0;
 case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:return ui_control_color(msg,wp);
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);if(g&&r.right>0&&r.bottom>0){HDC mem=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);HGDIOBJ old=SelectObject(mem,bitmap);graph_paint(h,g,mem,r);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);}EndPaint(h,&ps);return 0;}
 case WM_DESTROY:KillTimer(h,GRAPH_TIMER);SetWindowLongPtr(h,GWLP_USERDATA,0);free(g);return 0;
 default:return DefWindowProc(h,msg,wp,lp);
 }
}
static void graph_register(HINSTANCE inst){WNDCLASS wc={0};wc.hInstance=inst;wc.lpfnWndProc=graph_proc;wc.lpszClassName="WidebandTrace";wc.hCursor=LoadCursor(NULL,IDC_CROSS);RegisterClass(&wc);}
