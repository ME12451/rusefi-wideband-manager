static void workspace_paint(HWND h,HDC dc){
 RECT r;GetClientRect(h,&r);ui_fill(dc,r,UI_BG);int width=r.right>px(MAIN_WIDTH)?r.right:px(MAIN_WIDTH),height=r.bottom>px(MAIN_HEIGHT)?r.bottom:px(MAIN_HEIGHT);
 int saved=SaveDC(dc);SetViewportOrgEx(dc,-scrollX,-scrollY,NULL);
 ui_fill(dc,(RECT){0,0,width,px(72)},UI_NAVY);
 ui_text(dc,"rusEFI  /  WIDEBAND",(RECT){px(24),px(9),px(640),px(42)},uiTitleFont,RGB(245,249,253),DT_SINGLELINE);
 ui_text(dc,"CAN diagnostics & configuration  /  Manager 0.5.3",(RECT){px(24),px(43),px(650),px(66)},uiSmallFont,RGB(153,176,199),DT_SINGLELINE);
 const char *state=demoMode?"DEMO / SIMULATED":connected?(rxFault?"CAN ERROR":"CAN CONNECTED"):"OFFLINE";COLORREF color=demoMode?RGB(255,193,104):rxFault?RGB(255,124,128):connected?RGB(81,218,177):RGB(180,197,214);
 ui_text(dc,state,(RECT){width-px(540),px(24),width-px(380),px(54)},uiSmallFont,color,DT_RIGHT|DT_SINGLELINE);
 RECT sidebar={px(12),px(76),px(332),height-px(4)};ui_fill(dc,sidebar,RGB(255,255,255));ui_outline(dc,sidebar,UI_BORDER,px(12));
 ui_text(dc,"CONNECTION",(RECT){px(24),px(78),px(320),px(98)},uiSmallFont,UI_MUTED,DT_SINGLELINE);
 ui_text(dc,"MODULE / SETTINGS",(RECT){px(24),px(366),px(320),px(389)},uiSmallFont,UI_MUTED,DT_SINGLELINE);
 ui_text(dc,"LIVE-MONITOR",(RECT){px(344),px(78),width-px(24),px(101)},uiSmallFont,UI_MUTED,DT_SINGLELINE);
 RECT heater={px(344),height-px(136),width-px(24),height-px(76)};ui_fill(dc,heater,RGB(255,255,255));ui_outline(dc,heater,UI_BORDER,px(12));
 RECT detail={px(344),height-px(72),width-px(24),height-px(28)};ui_fill(dc,detail,RGB(255,255,255));ui_outline(dc,detail,UI_BORDER,px(12));
 ui_fill(dc,(RECT){px(344),height-px(24),width,height},RGB(255,255,255));RestoreDC(dc,saved);
}
static LRESULT CALLBACK metric_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_ERASEBKGND)return 1;
 if(msg!=WM_PAINT)return DefWindowProc(h,msg,wp,lp);
 PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right>0?r.right:1,r.bottom>0?r.bottom:1);HGDIOBJ old=SelectObject(mem,bmp);
 ui_fill(mem,r,RGB(255,255,255));ui_outline(mem,r,UI_BORDER,px(10));int card=GetDlgCtrlID(h)-200,index=selected_index();ChannelStats *c=&channels[index];uint32_t now=GetTickCount();const char *labels[]={"LAMBDA","SENSOR TEMPERATURE","HEATER","NERNST DC"};const char *units[]={"","C","%","V"};char value[48]="--",foot[80]="No data";int have=card<2?c->hasData:c->hasDiag;uint32_t age=have?now-(card<2?c->last:c->diagLast):0;COLORREF accent=card==0?UI_ACCENT:card==1?RGB(211,129,43):card==2?RGB(184,143,30):RGB(36,157,112);
 if(have){Frame *f=&values[index],*d=&diags[index];if(card==0)snprintf(value,sizeof value,"%.4f",le16(f->data+2)/10000.);if(card==1)snprintf(value,sizeof value,"%u",le16(f->data+4));if(card==2)snprintf(value,sizeof value,"%.1f",d->data[6]*100./255);if(card==3)snprintf(value,sizeof value,"%.3f",le16(d->data+2)/1000.);snprintf(foot,sizeof foot,"Index %d  /  %.1f s%s",index,age/1000.,age>1000?"  STALE":"");if(card==0)snprintf(foot,sizeof foot,"AFR (14.7) %.2f%s",le16(f->data+2)*14.7/10000.,age>1000?" / STALE":"");if(card==0&&!(f->data[1]&1)){accent=UI_RED;snprintf(foot,sizeof foot,"Lambda INVALID");}}
 if(have&&age>1000)accent=UI_MUTED;
 ui_fill(mem,(RECT){px(1),px(12),px(4),r.bottom-px(12)},accent);
 ui_text(mem,labels[card],(RECT){px(14),px(10),r.right-px(8),px(31)},uiSmallFont,UI_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
 ui_text(mem,value,(RECT){px(14),px(29),r.right-px(37),px(73)},uiValueFont,accent,DT_SINGLELINE|DT_END_ELLIPSIS);
 ui_text(mem,units[card],(RECT){r.right-px(35),px(46),r.right-px(9),px(72)},uiSmallFont,UI_MUTED,DT_SINGLELINE);
 if(r.bottom>=px(88))ui_text(mem,foot,(RECT){px(14),px(74),r.right-px(8),r.bottom-px(3)},uiSmallFont,UI_MUTED,DT_SINGLELINE|DT_END_ELLIPSIS);
 BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;
}
static void workspace_register(HINSTANCE inst){WNDCLASS wc={0};wc.hInstance=inst;wc.lpfnWndProc=metric_proc;wc.lpszClassName="WidebandMetric";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClass(&wc);}
/* Explicit preview mode. Never sends CAN or modifies a module. */
static void demo_sample(uint32_t time,int index){
 double t=(time-sessionStart)/1000.;uint16_t lambda=(uint16_t)((1+.055*sin(t*1.9+index)+.013*sin(t*7.7))*10000),temp=(uint16_t)(780+18*sin(t*.11+index)),esr=(uint16_t)(300+45*sin(t*.4+index)),dc=(uint16_t)(450+14*sin(t*2.4));
 Frame f={0};f.id=0x190+2*index;f.len=8;f.data[0]=0xa0;f.data[1]=3;f.data[2]=(uint8_t)lambda;f.data[3]=(uint8_t)(lambda>>8);f.data[4]=(uint8_t)temp;f.data[5]=(uint8_t)(temp>>8);analytic_frame(&f,0,time);values[index]=f;dataSeen[index]=time;
 Frame d={0};d.id=f.id+1;d.len=8;d.data[0]=(uint8_t)esr;d.data[1]=(uint8_t)(esr>>8);d.data[2]=(uint8_t)dc;d.data[3]=(uint8_t)(dc>>8);d.data[4]=(uint8_t)(128+40*sin(t*1.7));d.data[5]=2;d.data[6]=(uint8_t)(155+22*sin(t*.25+index));analytic_frame(&d,0,time);analytic_diag((unsigned)index,&d);diags[index]=d;diagSeen[index]=time;
}
static void demo_seed(void){sessionStart=GetTickCount()-120000;for(unsigned n=0;n<=1200;n++)for(int i=0;i<3;i++)demo_sample(sessionStart+n*100,i);SendMessage(modules,LB_ADDSTRING,0,(LPARAM)"DEMO: data indices 0 / 1 / 2");}
static void demo_tick(void){static uint32_t last;uint32_t now=GetTickCount();if(now-last<100)return;last=now;for(int i=0;i<3;i++)demo_sample(now,i);}
