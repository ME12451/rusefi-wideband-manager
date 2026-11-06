#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdarg.h>
#include "protocol.h"
#include "service_protocol.h"
#include "analytics.h"
#define APP "rusEFI Wideband Manager 0.5.3"
#define WM_FRAME (WM_APP+1)
#define WM_LOG (WM_APP+2)
#define WM_DONE (WM_APP+3)
#define WM_PROGRESS (WM_APP+4)
static HWND win,adapter,endpoint,rate,hw,indexBox,sensor,modrate,voltage,gain,heat,modules,live,monitor,status,progress;
static HFONT font;
static HWND mainGraph,metricCards[4];
static uint32_t sessionStart;
static int demoMode;
static int selected_index(void);
static void ui_theme_refresh(void);
/* Layout coordinates are logical pixels at 96 DPI. Scroll instead of shrinking text. */
typedef struct { HWND handle; int x,y,w,h; } UiRect;
static UiRect ui[80];
static int uiCount,uiDpi=96,scrollX,scrollY,layoutActive;
#define MAIN_WIDTH 1120
#define MAIN_HEIGHT 636
static int px(int n){return MulDiv(n,uiDpi,96);}
static void layout(void){
 if(layoutActive||!win)return;
 layoutActive=1;
 RECT r;GetClientRect(win,&r);
 SCROLLINFO si={0};si.cbSize=sizeof si;
 /* Scrollbars are only a fallback below the compact minimum. Re-read the
  * client after non-client bar changes before positioning any controls. */
 for(int pass=0;pass<4;pass++){
  RECT before=r;si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;si.nMin=0;
  si.nMax=px(MAIN_WIDTH)-1;si.nPage=(UINT)r.right;si.nPos=scrollX;SetScrollInfo(win,SB_HORZ,&si,FALSE);
  si.nMax=px(MAIN_HEIGHT)-1;si.nPage=(UINT)r.bottom;si.nPos=scrollY;SetScrollInfo(win,SB_VERT,&si,FALSE);
  GetClientRect(win,&r);if(EqualRect(&before,&r))break;
 }
 si.fMask=SIF_POS;si.nPos=0;scrollX=GetScrollInfo(win,SB_HORZ,&si)?si.nPos:0;si.nPos=0;scrollY=GetScrollInfo(win,SB_VERT,&si)?si.nPos:0;
 int width=r.right>px(MAIN_WIDTH)?r.right:px(MAIN_WIDTH);
 /* Move the entire tree before allowing any control to repaint/copy pixels. */
 UiRect placed[80];int placedCount=0;
 for(int i=0;i<uiCount;i++){
  UiRect *c=&ui[i];if(GetParent(c->handle)!=win)continue;
  int x=px(c->x),w=px(c->w),height=px(c->h),y=px(c->y),id=GetDlgCtrlID(c->handle);
  if(c->handle==mainGraph){w=width-px(368);}
  int canvasHeight=r.bottom>px(MAIN_HEIGHT)?r.bottom:px(MAIN_HEIGHT);
  if(c->handle==mainGraph){int compact=canvasHeight<px(760);y=px(compact?196:212);height=canvasHeight-px(compact?344:360);}
  if(c->x>=344&&c->y>=500){y=canvasHeight-px(MAIN_HEIGHT-c->y);if(c->handle==live||c->handle==status||c->handle==heat)w=width-x-px(24);}
  if(id==18)x=width-px(364);
  if(id==22)x=width-px(198);
  for(int n=0;n<4;n++)if(c->handle==metricCards[n]){int cardWidth=width-px(368);w=(cardWidth-px(36))/4;x=px(344)+n*(w+px(12));if(canvasHeight<px(760))height=px(80);}
  placed[placedCount++]=(UiRect){c->handle,x-scrollX,y-scrollY,w,height};
 }
 HDWP batch=BeginDeferWindowPos(placedCount);
 UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS;
 for(int i=0;i<placedCount&&batch;i++){UiRect *c=&placed[i];batch=DeferWindowPos(batch,c->handle,NULL,c->x,c->y,c->w,c->h,flags);}
 if(!batch||!EndDeferWindowPos(batch))for(int i=0;i<placedCount;i++){UiRect *c=&placed[i];SetWindowPos(c->handle,NULL,c->x,c->y,c->w,c->h,flags);}
 RedrawWindow(win,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME);
 layoutActive=0;
}
static void updatefont(void){
 HFONT old=font;
 font=CreateFont(-px(20),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
 for(int i=0;i<uiCount;i++)SendMessage(ui[i].handle,WM_SETFONT,(WPARAM)font,TRUE);
 ui_theme_refresh();
 if(old)DeleteObject(old);
}
static HANDLE serial=INVALID_HANDLE_VALUE,thread;
static HMODULE dll;
static volatile LONG connected,stopping,busy,heating,ackSeq,bitSeq,ecuSeen,rxFault;
static DWORD heatLast,ackTime;
static int kind,bitHw,bitValue,opHw,opValue,opType;
static uint8_t image[25600],heatV=120,heatGain=100;
static char linebuf[256];static int linepos;
static CRITICAL_SECTION txLock;
static int discovered[256];static uint8_t streamObserved[256];static DWORD seen[256],dataSeen[256],diagSeen[256];static Frame values[256],diags[256];
static FILE *csv;
static HWND toolsWindow;
static void open_tools(void);
static void tools_tick(void);
static void tools_frame(const Frame *f);
static void tools_disconnected(void);
static void read_f0(void);
static int monitor_accept(const Frame *f);
static void enhanced_legacy(int type);
static int enhanced_available(void);
#include "ui_theme.h"
/* PCAN-Basic classic ABI, default struct alignment. */
typedef struct {uint32_t id;uint8_t type,len,data[8];} PcanMsg;
typedef uint32_t (__stdcall *InitFn)(uint16_t,uint16_t,uint8_t,uint32_t,uint16_t);
typedef uint32_t (__stdcall *UninitFn)(uint16_t);
typedef uint32_t (__stdcall *ReadFn)(uint16_t,PcanMsg*,void*);
typedef uint32_t (__stdcall *WriteFn)(uint16_t,PcanMsg*);
static InitFn pcInit;static UninitFn pcUninit;static ReadFn pcRead;static WriteFn pcWrite;static uint16_t channel;
static void logline(const char *fmt,...){char *s=malloc(1024);if(!s)return;va_list a;va_start(a,fmt);vsnprintf(s,1024,fmt,a);va_end(a);if(!PostMessage(win,WM_LOG,0,(LPARAM)s))free(s);}
static void err(const char *s){MessageBox(win,s,APP,MB_OK|MB_ICONERROR);}
static int getnum(HWND h,int min,int max,int *v){char b[64],*end;GetWindowText(h,b,sizeof b);long n=strtol(b,&end,10);if(!b[0]||*end||n<min||n>max){err("Number outside the allowed range.");return 0;}*v=(int)n;return 1;}
static int rawwrite(const char *s){DWORD n=0;return WriteFile(serial,s,(DWORD)strlen(s),&n,NULL)&&n==strlen(s);}
static int sendframe(Frame *f){if(demoMode){logline("DEMO: CAN transmission is disabled.");return 0;}int ok=0;EnterCriticalSection(&txLock);if(connected&&!rxFault){if(kind==0){PcanMsg m={0};m.id=f->id;m.type=f->ext?2:0;m.len=f->len;memcpy(m.data,f->data,f->len);ok=pcWrite(channel,&m)==0;}else{char s[40];slcan_encode(f,s);ok=rawwrite(s);}}LeaveCriticalSection(&txLock);if(!ok)logline("TX failed (check CAN connection).");else{Frame *copy=malloc(sizeof *copy);if(copy){*copy=*f;if(!PostMessage(win,WM_FRAME,1,(LPARAM)copy))free(copy);}}return ok;}
static int sendcmd(uint32_t id,int n,int a,int b){Frame f={0};f.id=id;f.ext=1;f.len=(uint8_t)n;f.data[0]=(uint8_t)a;f.data[1]=(uint8_t)b;return sendframe(&f);}
static void received(Frame *f){
 if(f->ext&&f->id==WB_ACK&&f->len==0){ackTime=GetTickCount();InterlockedIncrement(&ackSeq);}
 if(f->ext&&f->id==WB_BITRATE+1&&f->len==2){bitHw=f->data[0];bitValue=f->data[1];InterlockedIncrement(&bitSeq);}
 if(f->ext&&f->id==WB_STATUS&&f->len>=2)InterlockedExchange(&ecuSeen,1);
 Frame *copy=malloc(sizeof *copy);if(copy){*copy=*f;if(!PostMessage(win,WM_FRAME,0,(LPARAM)copy))free(copy);}
}
static DWORD WINAPI rxworker(void *unused){(void)unused;while(!stopping){if(kind==0){PcanMsg m={0};uint32_t r=pcRead(channel,&m,NULL);if(r==0){if(m.len<=8&&!(m.type&(1|0x40|0x80))){Frame f={0};f.id=m.id;f.ext=!!(m.type&2);f.len=m.len;memcpy(f.data,m.data,m.len);received(&f);}}else if(r==0x20)Sleep(2);else{logline("PCAN read error 0x%08X. Reconnect.",r);InterlockedExchange(&rxFault,1);break;}}
 else{char b[256];DWORD n=0;if(!ReadFile(serial,b,sizeof b,&n,NULL)){logline("COM read error. Reconnect.");InterlockedExchange(&rxFault,1);break;}for(DWORD i=0;i<n;i++){if(b[i]=='\r'){linebuf[linepos]=0;Frame f;if(slcan_decode(linebuf,&f))received(&f);linepos=0;}else if(b[i]=='\a'){logline("SLCAN adapter reported an error (BEL).");InterlockedExchange(&rxFault,1);}else if(linepos<250)linebuf[linepos++]=b[i];else linepos=0;}}}return 0;}
static int slcommand(const char *s){char b;DWORD n,w;PurgeComm(serial,PURGE_RXCLEAR);if(!WriteFile(serial,s,(DWORD)strlen(s),&w,NULL)||w!=strlen(s))return 0;DWORD start=GetTickCount();while(GetTickCount()-start<1500){if(!ReadFile(serial,&b,1,&n,NULL))return 0;if(n&&b=='\a')return 0;if(n&&b=='\r')return 1;}return 0;}
static void disconnect(void){tools_disconnected();InterlockedExchange(&heating,0);if(!connected)return;InterlockedExchange(&stopping,1);WaitForSingleObject(thread,2500);CloseHandle(thread);thread=NULL;EnterCriticalSection(&txLock);if(kind==0)pcUninit(channel);else{rawwrite("C\r");CloseHandle(serial);serial=INVALID_HANDLE_VALUE;}InterlockedExchange(&connected,0);LeaveCriticalSection(&txLock);SetWindowText(status,"Disconnected");SetWindowText(heat,"Start heater messages (broadcast)");}
static void connectbus(void){if(demoMode){err("Demo mode: restart without --demo to connect CAN.");return;}if(connected){disconnect();return;}int ep;kind=(int)SendMessage(adapter,CB_GETCURSEL,0,0);if(!getnum(endpoint,1,kind==0?16:256,&ep))return;int fast=SendMessage(rate,CB_GETCURSEL,0,0)==1;InterlockedExchange(&rxFault,0);
 if(kind==0){if(!dll)dll=LoadLibraryEx("PCANBasic.dll",NULL,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll){err("PCANBasic.dll (64 bit) missing. Install the official PEAK driver/API or place the DLL next to the EXE.");return;}pcInit=(InitFn)(void*)GetProcAddress(dll,"CAN_Initialize");pcUninit=(UninitFn)(void*)GetProcAddress(dll,"CAN_Uninitialize");pcRead=(ReadFn)(void*)GetProcAddress(dll,"CAN_Read");pcWrite=(WriteFn)(void*)GetProcAddress(dll,"CAN_Write");if(!pcInit||!pcUninit||!pcRead||!pcWrite){err("Incomplete PCANBasic.dll.");return;}channel=ep<=8?0x50+ep:0x500+ep;uint32_t r=pcInit(channel,fast?0x14:0x1c,0,0,0);if(r){char s[100];snprintf(s,sizeof s,"PCAN initialization failed: 0x%08X",r);err(s);return;}}
 else{char port[32];snprintf(port,sizeof port,"\\\\.\\COM%d",ep);serial=CreateFile(port,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);if(serial==INVALID_HANDLE_VALUE){err("COM port unavailable.");return;}DCB d={0};d.DCBlength=sizeof d;GetCommState(serial,&d);d.BaudRate=115200;d.ByteSize=8;d.Parity=NOPARITY;d.StopBits=ONESTOPBIT;d.fBinary=TRUE;d.fOutxCtsFlow=FALSE;d.fOutxDsrFlow=FALSE;d.fDtrControl=DTR_CONTROL_ENABLE;d.fRtsControl=RTS_CONTROL_ENABLE;d.fOutX=FALSE;d.fInX=FALSE;COMMTIMEOUTS t={0};t.ReadIntervalTimeout=MAXDWORD;t.ReadTotalTimeoutConstant=20;t.WriteTotalTimeoutConstant=500;if(!SetCommState(serial,&d)||!SetCommTimeouts(serial,&t)||!slcommand("C\r")||!slcommand(fast?"S8\r":"S6\r")||!slcommand("O\r")){CloseHandle(serial);serial=INVALID_HANDLE_VALUE;err("SLCAN initialization failed. Expected Lawicel, 115200 baud, 8N1.");return;}}
 memset(discovered,0,sizeof discovered);memset(streamObserved,0,sizeof streamObserved);memset(dataSeen,0,sizeof dataSeen);memset(diagSeen,0,sizeof diagSeen);SendMessage(modules,LB_RESETCONTENT,0,0);InterlockedExchange(&ecuSeen,0);InterlockedExchange(&stopping,0);InterlockedExchange(&connected,1);linepos=0;thread=CreateThread(NULL,0,rxworker,NULL,0,NULL);if(!thread){disconnect();err("Could not start receive thread.");return;}SetWindowText(status,"Connected - passive reception active");}
static int waitack(LONG before,DWORD timeout){DWORD t=GetTickCount();while(!stopping&&!rxFault&&GetTickCount()-t<timeout){if(ackSeq!=before)return 1;Sleep(2);}return 0;}
static int command_ack(uint32_t id,int n,int a,int b,DWORD timeout){LONG before=ackSeq;return sendcmd(id,n,a,b)&&waitack(before,timeout);}
static DWORD WINAPI operation(void *unused){(void)unused;int ok=0;if(opType==1||opType==2){ok=command_ack(opType==1?WB_INDEX:WB_SENSOR,2,opType==1?opValue:opHw,opType==1?opHw:opValue,1800);logline(ok?"Module acknowledged command. Legacy ACK has no sender ID; configuration readback unavailable.":"No acknowledgement. Result unknown; check module/bus.");}
 else if(opType==3){LONG before=bitSeq;if(sendcmd(WB_BITRATE,2,opHw,opValue)){DWORD t=GetTickCount();while(GetTickCount()-t<1800&&!stopping&&!rxFault){if(bitSeq!=before&&bitHw==opHw&&bitValue==opValue){ok=1;break;}Sleep(2);}}logline(ok?"Bitrate save acknowledged. Active after module reset; adjust adapter bitrate then.":"No matching bitrate acknowledgement. Check compatible firmware and bootloader.");}
 else if(opType==4){
  logline("Starting F0 update. Only one module on the bus; do not interrupt power.");
  if(!command_ack(WB_ENTER,1,opHw,0,1800))logline("No application response; trying an F0 bootloader already waiting.");
  /* App ACK precedes its 50-tick reset. Repeated empty start catches the 800 ms boot window. */
  Sleep(150);int boot=0;for(int i=0;i<20&&!stopping&&!rxFault;i++){if(command_ack(WB_ENTER,0,0,0,40)){boot=1;break;}}
  if(!boot)goto done;
  if(!command_ack(WB_ERASE,0,0,0,5000))goto done;
  for(unsigned off=0;off<sizeof image;off+=8){Frame f={0};f.id=WB_DATA|off;f.ext=1;f.len=8;memcpy(f.data,image+off,8);LONG seq=ackSeq;if(!sendframe(&f)||!waitack(seq,1800))goto done;if((off&255)==0)PostMessage(win,WM_PROGRESS,off*100/sizeof image,0);}
  if(!command_ack(WB_REBOOT,0,0,0,1800))goto done;
  Sleep(1300);sendcmd(WB_PING,1,opHw,0);ok=1;PostMessage(win,WM_PROGRESS,100,0);
 }
 done:if(opType==4)logline(ok?"Image transferred and restart acknowledged. Bootloader checks CRC; check Pong/live values after restart. Protocol has no flash readback.":"UPDATE ABORTED. Keep module powered; bootloader recovery may be required.");PostMessage(win,WM_DONE,ok,0);return 0;}
static void startop(int type){if(type>=1&&type<=3){if(enhanced_available())enhanced_legacy(type);else err("Read the exact HW + target data index with indexed firmware first. No legacy write fallback.");return;}if(!connected||rxFault){err("Establish a working CAN connection first.");return;}if(busy)return;if(!getnum(hw,0,8,&opHw))return;opType=type;
 if(type==1&&!getnum(indexBox,0,255,&opValue))return;
 if(type==2)opValue=(int)SendMessage(sensor,CB_GETCURSEL,0,0);
 if(type==3)opValue=(int)SendMessage(modrate,CB_GETCURSEL,0,0);
 if(type==1||type==2){if(MessageBox(win,"Save setting permanently? Hardware ID must be unique. Legacy ACKs have no sender ID; do not use other configuration tools during this operation.",APP,MB_OKCANCEL|MB_ICONQUESTION)!=IDOK)return;}
 if(type==4){OPENFILENAME of={0};char path[MAX_PATH]="";of.lStructSize=sizeof of;of.hwndOwner=win;of.lpstrFilter="F0 application BIN with CRC\0*.bin\0";of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(!GetOpenFileName(&of))return;FILE *f=fopen(path,"rb");if(!f){err("Cannot read file.");return;}size_t n=fread(image,1,sizeof image,f);int extra=fgetc(f);fclose(f);if(extra!=EOF||!valid_image(image,n)){err("Invalid F0 application image: expected 25600 bytes, vectors at 0x08001800 and matching CRC32. Full/bootloader images are rejected.");return;}if(MessageBox(win,"Confirmed F0 module with matching hardware/firmware? Connect exactly ONE wideband module to the bus. Bootloader commands are broadcast and cannot target a single module. The application will be erased and replaced. Keep power stable.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;InterlockedExchange(&heating,0);SetWindowText(heat,"Start heater messages (broadcast)");}
 InterlockedExchange(&busy,1);EnableWindow(GetDlgItem(win,10),FALSE);EnableWindow(heat,FALSE);SendMessage(progress,PBM_SETPOS,0,0);HANDLE h=CreateThread(NULL,0,operation,NULL,0,NULL);if(h)CloseHandle(h);else{InterlockedExchange(&busy,0);EnableWindow(GetDlgItem(win,10),TRUE);EnableWindow(heat,TRUE);err("Could not start worker thread.");}}
static HWND control(const char *cls,const char *text,DWORD style,int x,int y,int w,int h,int id){if(!strcmp(cls,"BUTTON")&&(style&0xf)==BS_PUSHBUTTON)style=(style&~0xf)|BS_OWNERDRAW;HWND c=CreateWindowEx(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,NULL,NULL);SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);if(uiCount<80)ui[uiCount++]=(UiRect){c,x,y,w,h};return c;}
static HWND label(const char *text,int x,int y,int w){return control("STATIC",text,0,x,y,w,22,0);}
static HWND combo(int x,int y,int w,const char **items,int n){HWND c=control("COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,240,0);for(int i=0;i<n;i++)SendMessage(c,CB_ADDSTRING,0,(LPARAM)items[i]);SendMessage(c,CB_SETCURSEL,0,0);return c;}
static void refreshlive(void){
 int idx=selected_index();char text[600];DWORD now=GetTickCount();ChannelStats *c=&channels[idx];Frame *f=&values[idx],*d=&diags[idx];
 const char *states[]={"Preheat","Warmup","Closed loop","Heater fault","Overtemperature","Undertemperature","Heating disabled","CAN silent"};
 if(!c->hasData)snprintf(text,sizeof text,"Index %d  /  CAN %03X / %03X  /  No measurement data yet",idx,0x190+2*idx,0x191+2*idx);
 else snprintf(text,sizeof text,"Index %d  /  CAN %03X / %03X  /  Protocol %02X  /  Data age %.1f s%s\r\n%s  /  Lambda %s  /  ESR %u Ohm  /  Pump PWM %.1f %%  /  Sensor flags %02X",idx,0x190+2*idx,0x191+2*idx,f->data[0],(now-c->last)/1000.,now-c->last>1000?" - STALE":"",!c->hasDiag?"No diagnostics":c->state<8?states[c->state]:"Unknown status",(f->data[1]&1)?"valid":"INVALID",le16(d->data),d->data[4]*100./255.,f->data[1]&14);
 SetWindowText(live,text);for(int i=0;i<4;i++)InvalidateRect(metricCards[i],NULL,FALSE);
}
#define DATA_ROW 0x100
static void stream_row(unsigned index,int stale){
 int tag=DATA_ROW|(int)index,pos=-1;char row[100],old[160];
 int count=(int)SendMessage(modules,LB_GETCOUNT,0,0);
 for(int i=0;i<count;i++)if((int)SendMessage(modules,LB_GETITEMDATA,i,0)==tag){pos=i;break;}
 snprintf(row,sizeof row,"Data %u | CAN %03X/%03X%s",index,0x190+2*index,0x191+2*index,stale?" | stale":"");
 if(pos>=0){wchar_t wide[160];SendMessageW(modules,LB_GETTEXT,pos,(LPARAM)wide);WideCharToMultiByte(CP_UTF8,0,wide,-1,old,sizeof old,NULL,NULL);if(!strcmp(old,row))return;}
 int selected=(int)SendMessage(modules,LB_GETCURSEL,0,0);
 if(pos>=0){SendMessage(modules,LB_DELETESTRING,pos,0);SendMessage(modules,LB_INSERTSTRING,pos,(LPARAM)row);}
 else pos=(int)SendMessage(modules,LB_ADDSTRING,0,(LPARAM)row);
 if(pos>=0){SendMessage(modules,LB_SETITEMDATA,pos,tag);if(selected>=0)SendMessage(modules,LB_SETCURSEL,selected,0);}
}
static void refresh_stream_rows(void){
 if(demoMode)return;
 DWORD now=GetTickCount();
 for(unsigned i=0;i<256;i++)if(streamObserved[i]){
  DWORD last=channels[i].hasData?channels[i].last:channels[i].diagLast;
  if(channels[i].hasDiag&&(int32_t)(channels[i].diagLast-last)>0)last=channels[i].diagLast;
  stream_row(i,(uint32_t)(now-last)>1000);
 }
}
static char monitorPending[32768];static size_t monitorPendingSize;
static void monitor_queue(const char *text){
 size_t n=strlen(text);if(n>=sizeof monitorPending)return;
 if(monitorPendingSize+n>=sizeof monitorPending){size_t drop=monitorPendingSize/2;while(drop<monitorPendingSize&&monitorPending[drop]!='\n')drop++;if(drop<monitorPendingSize)drop++;memmove(monitorPending,monitorPending+drop,monitorPendingSize-drop);monitorPendingSize-=drop;}
 memcpy(monitorPending+monitorPendingSize,text,n+1);monitorPendingSize+=n;
}
static void monitor_flush(void){
 if(!monitorPendingSize||!toolsWindow||!IsWindowVisible(toolsWindow)||!IsWindowVisible(monitor))return;
 HWND pause=GetDlgItem(toolsWindow,106);if(pause&&SendMessage(pause,BM_GETCHECK,0,0)==BST_CHECKED)return;
 SendMessage(monitor,WM_SETREDRAW,FALSE,0);
 if((size_t)GetWindowTextLength(monitor)+monitorPendingSize>59000)SetWindowText(monitor,"");
 SendMessage(monitor,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);SendMessage(monitor,EM_REPLACESEL,0,(LPARAM)monitorPending);
 monitorPendingSize=0;monitorPending[0]=0;SendMessage(monitor,WM_SETREDRAW,TRUE,0);InvalidateRect(monitor,NULL,FALSE);
}
static void displayframe(Frame *f,int tx){char s[180],data[30]="";for(int i=0;i<f->len;i++)sprintf(data+3*i,"%02X ",f->data[i]);snprintf(s,sizeof s,"%10lu %s %s %08X [%u] %s\r\n",GetTickCount(),tx?"TX":"RX",f->ext?"EXT":"STD",f->id,f->len,data);if(csv){fprintf(csv,"%lu,%s,%s,%08X,%u,%s\n",GetTickCount(),tx?"TX":"RX",f->ext?"EXT":"STD",f->id,f->len,data);}
 /* Bound the visible monitor; CSV retains all processed frames. */
 if(monitor_accept(f))monitor_queue(s);
 analytic_frame(f,tx,GetTickCount());
 if(tx)return;
 tools_frame(f);
 if(!f->ext&&(f->id&1)&&f->id>=0x191&&f->id<=0x38f&&f->len==8)analytic_diag((f->id-0x190)/2,f);
 if(f->ext&&f->id==WB_ACK&&f->len==8){int id=f->data[0];char row[150];snprintf(row,sizeof row,"HW %u | v%02X | %04u-%02u-%02u",id,f->data[1],2000+f->data[2],f->data[3],f->data[4]);seen[id]=GetTickCount();if(!discovered[id]){int pos=(int)SendMessage(modules,LB_ADDSTRING,0,(LPARAM)row);SendMessage(modules,LB_SETITEMDATA,pos,id);discovered[id]=1;}logline("Pong: %s (HW ID is not the CAN measurement index)",row);}
 if(!f->ext&&f->id>=0x190&&f->id<=0x38f&&f->len==8&&((f->id&1)||f->data[0]==0xa0)){int idx=(f->id-0x190)/2;if(f->id&1){diags[idx]=*f;diagSeen[idx]=GetTickCount();}else{values[idx]=*f;dataSeen[idx]=GetTickCount();}streamObserved[idx]=1;stream_row((unsigned)idx,0);}
}
#include "live_graph.h"
#include "enhanced_ui.h"
#include "workspace_ui.h"

static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{win=h;HDC dc=GetDC(h);uiDpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(h,dc);updatefont();
 const char *ad[]={"PEAK PCAN-USB","SLCAN / Lawicel COM"},*rates[]={"500 kbit/s","1 Mbit/s"},*sensors[]={"LSU 4.9","LSU 4.2","LSU ADV","FAE LSU 4.9","LSU 4.9 Rtrim SW"};
 label("Adapter",24,100,296);adapter=combo(24,122,296,ad,2);
 label("Channel / COM",24,158,140);endpoint=control("EDIT","1",WS_BORDER|ES_NUMBER,24,180,140,30,0);
 label("Bus bitrate",180,158,140);rate=combo(180,180,140,rates,2);
 control("BUTTON","Connect / Disconnect",0,24,216,296,32,10);control("BUTTON","Find modules",0,24,254,140,32,11);control("BUTTON","Read F0",0,180,254,140,32,23);
 modules=control("LISTBOX","",WS_BORDER|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,24,292,296,68,12);
 label("Hardware ID",24,390,140);hw=control("EDIT","4",WS_BORDER|ES_NUMBER,24,414,140,28,24);
 label("Data index",180,390,140);indexBox=control("EDIT","0",WS_BORDER|ES_NUMBER,180,414,140,28,0);
 control("BUTTON","Save index",0,24,448,296,28,13);
 label("Sensor type",24,482,296);sensor=combo(24,504,216,sensors,5);control("BUTTON","Save",0,248,504,72,30,14);
 label("Module bitrate after reset",24,540,296);modrate=combo(24,562,216,rates,2);control("BUTTON","Save",0,248,562,72,30,15);
 control("BUTTON","Configuration",0,24,598,144,28,20);control("BUTTON","Firmware...",0,176,598,144,28,17);
 control("BUTTON","CAN CSV",0,756,20,150,34,18);control("BUTTON","CAN console",0,922,20,174,34,22);
 for(int i=0;i<4;i++)metricCards[i]=control("WidebandMetric","",0,344+i*189,104,177,96,200+i);
 mainGraph=control("WidebandTrace","",0,344,212,752,276,210);
 label("Supply [V]",360,506,112);voltage=control("EDIT","12.0",WS_BORDER,360,528,96,30,0);
 label("Gain [%]",480,506,120);gain=control("EDIT","100",WS_BORDER|ES_NUMBER,480,528,96,30,0);
 heat=control("BUTTON","Start heater messages (broadcast)",0,600,520,496,36,16);
 live=control("STATIC","No measurement data yet",0,360,564,736,44,0);SendMessage(live,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);
 progress=control(PROGRESS_CLASS,"",0,24,630,296,4,0);
 monitor=control("EDIT","",WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,0,0,1,1,0);ShowWindow(monitor,SW_HIDE);SendMessage(monitor,EM_SETLIMITTEXT,60000,0);
 status=control("EDIT",demoMode?"DEMO - simulated values, no CAN connection":"Ready  /  Connect CAN and select a measurement index",ES_READONLY|ES_AUTOHSCROLL,344,612,752,22,0);SendMessage(status,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);
 if(demoMode){demo_seed();const int blocked[]={10,11,13,14,15,16,17,23};for(unsigned i=0;i<sizeof blocked/sizeof *blocked;i++)EnableWindow(GetDlgItem(h,blocked[i]),FALSE);}
 layout();SetTimer(h,1,50,NULL);break;}
 case WM_SIZE:layout();break;
 case WM_DPICHANGED:{uiDpi=HIWORD(wp);updatefont();RECT *r=(RECT*)lp;SetWindowPos(h,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout();break;}
 case WM_VSCROLL:
 case WM_HSCROLL:{int bar=msg==WM_VSCROLL?SB_VERT:SB_HORZ;SCROLLINFO si={0};si.cbSize=sizeof si;si.fMask=SIF_ALL;GetScrollInfo(h,bar,&si);int pos=si.nPos;switch(LOWORD(wp)){case SB_LINEUP:pos-=px(36);break;case SB_LINEDOWN:pos+=px(36);break;case SB_TOP:pos=si.nMin;break;case SB_BOTTOM:pos=si.nMax;break;case SB_PAGEUP:pos-=(int)si.nPage;break;case SB_PAGEDOWN:pos+=(int)si.nPage;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:pos=si.nTrackPos;break;}if(bar==SB_VERT)scrollY=pos;else scrollX=pos;layout();break;}
 case WM_MOUSEWHEEL:scrollY-=MulDiv(GET_WHEEL_DELTA_WPARAM(wp),px(108),WHEEL_DELTA);layout();break;
 case WM_TIMER:if(demoMode)demo_tick();tools_tick();if(heating&&connected&&!busy){if(ecuSeen||rxFault){InterlockedExchange(&heating,0);SetWindowText(heat,"Start heater messages (broadcast)");logline("Heater messages stopped: ECU sender or CAN error detected.");}else if(GetTickCount()-heatLast>=50){Frame f={0};f.id=WB_STATUS;f.ext=1;f.len=3;f.data[0]=heatV;f.data[1]=1;f.data[2]=heatGain;if(!sendframe(&f))InterlockedExchange(&heating,0);heatLast=GetTickCount();}}{static unsigned tick;if(++tick%2==0){refreshlive();monitor_flush();}if(tick%10==0){refresh_stream_rows();RECT nav={0,-scrollY,px(2000),px(72)-scrollY};InvalidateRect(h,&nav,FALSE);}}break;
 case WM_COMMAND:switch(LOWORD(wp)){case 24:if(HIWORD(wp)==EN_CHANGE&&!busy){int target=main_target();if(target>=0){boardKnown[target]=serviceKnown[target]=trimKnown[target]=0;memset(extraSeen[target],0,sizeof extraSeen[target]);}if(trimEdit)SetWindowText(trimEdit,"");report_refresh();}break;case 20:open_tools();SendMessage(toolView,CB_SETCURSEL,4,0);displayView=4;tool_layout();report_refresh();break;case 22:open_tools();SendMessage(toolView,CB_SETCURSEL,7,0);displayView=7;tool_layout();report_refresh();break;case 10:if(!busy)connectbus();break;case 11:if(connected&&!busy&&sendcmd(WB_PING,1,255,0))logline("Searching hardware IDs. Pong groups HW IDs. Select a Data row and HW ID, then Read F0 using indexed firmware.");break;case 23:read_f0();break;case 12:if(HIWORD(wp)==LBN_SELCHANGE&&!busy){int pos=(int)SendMessage(modules,LB_GETCURSEL,0,0);if(pos!=LB_ERR){int tag=(int)SendMessage(modules,LB_GETITEMDATA,pos,0);char b[16];sprintf(b,"%d",tag&255);if(tag&DATA_ROW){SetWindowText(indexBox,b);set_target_index(tag&255);logline("Selected live/target data index %d. Select the HW ID and Read F0 before editing this pair.",tag&255);}else SetWindowText(hw,b);}}break;case 13:startop(1);break;case 14:startop(2);break;case 15:startop(3);break;case 17:startop(4);break;
 case 16:if(busy)break;if(heating){InterlockedExchange(&heating,0);sendcmd(WB_STATUS,3,heatV,0);SetWindowText(heat,"Start heater messages (broadcast)");logline("Heater enable ended. Firmware timeout/fallback applies after 250 ms.");}else if(connected&&!rxFault){char b[64],*end;GetWindowText(voltage,b,sizeof b);double v=strtod(b,&end);int g;if(!b[0]||*end||!(v>=0&&v<=25.5)){err("Supply must be 0..25.5 V (decimal point).");break;}if(!getnum(gain,0,100,&g))break;if(ecuSeen){err("ECU status sender detected on the bus. Avoid concurrent heater commands; reconnect only after removing the ECU sender.");break;}if(MessageBox(h,"Enable heating on all modules? Enter the actual supply voltage. Sensor must be dry and suitable for heating. CAN message applies to the entire bus.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)break;heatV=(uint8_t)(v*10+.5);heatGain=(uint8_t)g;InterlockedExchange(&heating,1);SetWindowText(heat,"Stop heater messages");}break;
 case 18:if(csv){fclose(csv);csv=NULL;logline("CSV closed.");}else{char path[MAX_PATH]="wideband-can.csv";OPENFILENAME of={0};of.lStructSize=sizeof of;of.hwndOwner=h;of.lpstrFilter="CSV\0*.csv\0";of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.Flags=OFN_OVERWRITEPROMPT;of.lpstrDefExt="csv";if(GetSaveFileName(&of)){csv=fopen(path,"w");if(csv)fprintf(csv,"host_tick_ms,direction,format,id_hex,dlc,data_hex\n");else err("Cannot create CSV.");}}break;
 case 19:open_tools();break;}break;
 case GRAPH_EXPORT:export_trace();break;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right>0?r.right:1,r.bottom>0?r.bottom:1);HGDIOBJ old=SelectObject(mem,bmp);workspace_paint(h,mem);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);break;}
 case WM_ERASEBKGND:return 1;
 case WM_DRAWITEM:if(ui_draw_button((const DRAWITEMSTRUCT*)lp))return TRUE;break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:return ui_control_color(msg,wp);
 case WM_FRAME:{Frame *f=(Frame*)lp;displayframe(f,(int)wp);free(f);break;}
 case WM_LOG:{char *s=(char*)lp;SetWindowText(status,s);monitor_queue(s);monitor_queue("\r\n");free(s);break;}
 case WM_PROGRESS:SendMessage(progress,PBM_SETPOS,wp,0);break;
 case WM_DONE:InterlockedExchange(&busy,0);EnableWindow(GetDlgItem(h,10),TRUE);EnableWindow(heat,TRUE);break;
 case WM_CLOSE:if(busy){err("CAN operation in progress. Wait for completion or timeout before closing.");return 0;}disconnect();if(csv)fclose(csv);DestroyWindow(h);break;
 case WM_DESTROY:if(toolsWindow)DestroyWindow(toolsWindow);KillTimer(h,1);ui_theme_dispose();PostQuitMessage(0);break;
 default:return DefWindowProc(h,msg,wp,lp);}return 0;}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show){(void)prev;demoMode=strstr(cmd,"--demo")!=NULL;sessionStart=GetTickCount();/* Per-monitor v2 when available; system-aware fallback on older Windows. */
 typedef BOOL (WINAPI *DpiContextFn)(HANDLE);
 DpiContextFn dpiContext=(DpiContextFn)(void*)GetProcAddress(GetModuleHandle("user32.dll"),"SetProcessDpiAwarenessContext");
 if(!dpiContext||!dpiContext((HANDLE)(INT_PTR)-4))SetProcessDPIAware();
 InitializeCriticalSection(&txLock);INITCOMMONCONTROLSEX cc={sizeof cc,ICC_PROGRESS_CLASS};InitCommonControlsEx(&cc);graph_register(inst);workspace_register(inst);WNDCLASS wc={0};wc.lpfnWndProc=wndproc;wc.hInstance=inst;wc.lpszClassName="RusEFIWidebandManager";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);RegisterClass(&wc);RECT work;SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
 int width=work.right-work.left,height=work.bottom-work.top;
 HWND h=CreateWindowEx(WS_EX_COMPOSITED,wc.lpszClassName,APP,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL|WS_HSCROLL,work.left,work.top,width,height,NULL,NULL,inst,NULL);if(!h)return 1;ShowWindow(h,show==SW_SHOWMINIMIZED?show:SW_SHOWMAXIMIZED);MSG m;while(GetMessage(&m,NULL,0,0)>0){if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE&&toolsWindow&&IsWindowVisible(toolsWindow)&&(m.hwnd==toolsWindow||IsChild(toolsWindow,m.hwnd))){ShowWindow(toolsWindow,SW_HIDE);SetForegroundWindow(win);continue;}TranslateMessage(&m);DispatchMessage(&m);}if(dll)FreeLibrary(dll);DeleteObject(font);DeleteCriticalSection(&txLock);return 0;}
