#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdarg.h>
#include "protocol.h"
#define APP "rusEFI Wideband Manager 0.1"
#define WM_FRAME (WM_APP+1)
#define WM_LOG (WM_APP+2)
#define WM_DONE (WM_APP+3)
#define WM_PROGRESS (WM_APP+4)
static HWND win,adapter,endpoint,rate,hw,indexBox,sensor,modrate,voltage,gain,heat,modules,live,monitor,status,progress;
static HFONT font;
static HANDLE serial=INVALID_HANDLE_VALUE,thread;
static HMODULE dll;
static volatile LONG connected,stopping,busy,heating,ackSeq,bitSeq,ecuSeen,rxFault;
static DWORD heatLast,ackTime;
static int kind,bitHw,bitValue,opHw,opValue,opType;
static uint8_t image[25600],heatV=120,heatGain=100;
static char linebuf[256];static int linepos;
static CRITICAL_SECTION txLock;
static int discovered[256];static DWORD seen[256],dataSeen[256],diagSeen[256];static Frame values[256],diags[256];
static FILE *csv;
/* PCAN-Basic classic ABI, default struct alignment. */
typedef struct {uint32_t id;uint8_t type,len,data[8];} PcanMsg;
typedef uint32_t (__stdcall *InitFn)(uint16_t,uint16_t,uint8_t,uint32_t,uint16_t);
typedef uint32_t (__stdcall *UninitFn)(uint16_t);
typedef uint32_t (__stdcall *ReadFn)(uint16_t,PcanMsg*,void*);
typedef uint32_t (__stdcall *WriteFn)(uint16_t,PcanMsg*);
static InitFn pcInit;static UninitFn pcUninit;static ReadFn pcRead;static WriteFn pcWrite;static uint16_t channel;
static void logline(const char *fmt,...){char *s=malloc(1024);if(!s)return;va_list a;va_start(a,fmt);vsnprintf(s,1024,fmt,a);va_end(a);if(!PostMessage(win,WM_LOG,0,(LPARAM)s))free(s);}
static void err(const char *s){MessageBox(win,s,APP,MB_OK|MB_ICONERROR);}
static int getnum(HWND h,int min,int max,int *v){char b[64],*end;GetWindowText(h,b,sizeof b);long n=strtol(b,&end,10);if(!b[0]||*end||n<min||n>max){err("Zahlenwert ausserhalb des erlaubten Bereichs.");return 0;}*v=(int)n;return 1;}
static int rawwrite(const char *s){DWORD n=0;return WriteFile(serial,s,(DWORD)strlen(s),&n,NULL)&&n==strlen(s);}
static int sendframe(Frame *f){int ok=0;EnterCriticalSection(&txLock);if(connected&&!rxFault){if(kind==0){PcanMsg m={0};m.id=f->id;m.type=f->ext?2:0;m.len=f->len;memcpy(m.data,f->data,f->len);ok=pcWrite(channel,&m)==0;}else{char s[40];slcan_encode(f,s);ok=rawwrite(s);}}LeaveCriticalSection(&txLock);if(!ok)logline("TX fehlgeschlagen (CAN-Verbindung pruefen).");else{Frame *copy=malloc(sizeof *copy);if(copy){*copy=*f;if(!PostMessage(win,WM_FRAME,1,(LPARAM)copy))free(copy);}}return ok;}
static int sendcmd(uint32_t id,int n,int a,int b){Frame f={0};f.id=id;f.ext=1;f.len=(uint8_t)n;f.data[0]=(uint8_t)a;f.data[1]=(uint8_t)b;return sendframe(&f);}
static void received(Frame *f){
 if(f->ext&&f->id==WB_ACK&&f->len==0){ackTime=GetTickCount();InterlockedIncrement(&ackSeq);}
 if(f->ext&&f->id==WB_BITRATE+1&&f->len==2){bitHw=f->data[0];bitValue=f->data[1];InterlockedIncrement(&bitSeq);}
 if(f->ext&&f->id==WB_STATUS&&f->len>=2)InterlockedExchange(&ecuSeen,1);
 Frame *copy=malloc(sizeof *copy);if(copy){*copy=*f;if(!PostMessage(win,WM_FRAME,0,(LPARAM)copy))free(copy);}
}
static DWORD WINAPI rxworker(void *unused){(void)unused;while(!stopping){if(kind==0){PcanMsg m={0};uint32_t r=pcRead(channel,&m,NULL);if(r==0){if(m.len<=8&&!(m.type&(1|0x40|0x80))){Frame f={0};f.id=m.id;f.ext=!!(m.type&2);f.len=m.len;memcpy(f.data,m.data,m.len);received(&f);}}else if(r==0x20)Sleep(2);else{logline("PCAN Lesefehler 0x%08X. Verbindung neu herstellen.",r);InterlockedExchange(&rxFault,1);break;}}
 else{char b[256];DWORD n=0;if(!ReadFile(serial,b,sizeof b,&n,NULL)){logline("COM-Lesefehler. Verbindung neu herstellen.");InterlockedExchange(&rxFault,1);break;}for(DWORD i=0;i<n;i++){if(b[i]=='\r'){linebuf[linepos]=0;Frame f;if(slcan_decode(linebuf,&f))received(&f);linepos=0;}else if(b[i]=='\a'){logline("SLCAN-Adapter meldet Fehler (BEL).");InterlockedExchange(&rxFault,1);}else if(linepos<250)linebuf[linepos++]=b[i];else linepos=0;}}}return 0;}
static int slcommand(const char *s){char b;DWORD n,w;PurgeComm(serial,PURGE_RXCLEAR);if(!WriteFile(serial,s,(DWORD)strlen(s),&w,NULL)||w!=strlen(s))return 0;DWORD start=GetTickCount();while(GetTickCount()-start<1500){if(!ReadFile(serial,&b,1,&n,NULL))return 0;if(n&&b=='\a')return 0;if(n&&b=='\r')return 1;}return 0;}
static void disconnect(void){InterlockedExchange(&heating,0);if(!connected)return;InterlockedExchange(&stopping,1);WaitForSingleObject(thread,2500);CloseHandle(thread);thread=NULL;EnterCriticalSection(&txLock);if(kind==0)pcUninit(channel);else{rawwrite("C\r");CloseHandle(serial);serial=INVALID_HANDLE_VALUE;}InterlockedExchange(&connected,0);LeaveCriticalSection(&txLock);SetWindowText(status,"Getrennt");SetWindowText(heat,"Heiztelegramme starten (Broadcast)");}
static void connectbus(void){if(connected){disconnect();return;}int ep;kind=(int)SendMessage(adapter,CB_GETCURSEL,0,0);if(!getnum(endpoint,1,kind==0?16:256,&ep))return;int fast=SendMessage(rate,CB_GETCURSEL,0,0)==1;InterlockedExchange(&rxFault,0);
 if(kind==0){if(!dll)dll=LoadLibraryEx("PCANBasic.dll",NULL,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll){err("PCANBasic.dll (64 Bit) fehlt. Offiziellen PEAK-Treiber/API installieren oder DLL neben die EXE legen.");return;}pcInit=(InitFn)(void*)GetProcAddress(dll,"CAN_Initialize");pcUninit=(UninitFn)(void*)GetProcAddress(dll,"CAN_Uninitialize");pcRead=(ReadFn)(void*)GetProcAddress(dll,"CAN_Read");pcWrite=(WriteFn)(void*)GetProcAddress(dll,"CAN_Write");if(!pcInit||!pcUninit||!pcRead||!pcWrite){err("PCANBasic.dll unvollstaendig.");return;}channel=ep<=8?0x50+ep:0x500+ep;uint32_t r=pcInit(channel,fast?0x14:0x1c,0,0,0);if(r){char s[100];snprintf(s,sizeof s,"PCAN Initialisierung fehlgeschlagen: 0x%08X",r);err(s);return;}}
 else{char port[32];snprintf(port,sizeof port,"\\\\.\\COM%d",ep);serial=CreateFile(port,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);if(serial==INVALID_HANDLE_VALUE){err("COM-Port nicht verfuegbar.");return;}DCB d={0};d.DCBlength=sizeof d;GetCommState(serial,&d);d.BaudRate=115200;d.ByteSize=8;d.Parity=NOPARITY;d.StopBits=ONESTOPBIT;d.fBinary=TRUE;d.fOutxCtsFlow=FALSE;d.fOutxDsrFlow=FALSE;d.fDtrControl=DTR_CONTROL_ENABLE;d.fRtsControl=RTS_CONTROL_ENABLE;d.fOutX=FALSE;d.fInX=FALSE;COMMTIMEOUTS t={0};t.ReadIntervalTimeout=MAXDWORD;t.ReadTotalTimeoutConstant=20;t.WriteTotalTimeoutConstant=500;if(!SetCommState(serial,&d)||!SetCommTimeouts(serial,&t)||!slcommand("C\r")||!slcommand(fast?"S8\r":"S6\r")||!slcommand("O\r")){CloseHandle(serial);serial=INVALID_HANDLE_VALUE;err("SLCAN Initialisierung fehlgeschlagen. Erwartet Lawicel, 115200 Baud, 8N1.");return;}}
 memset(discovered,0,sizeof discovered);memset(dataSeen,0,sizeof dataSeen);memset(diagSeen,0,sizeof diagSeen);SendMessage(modules,LB_RESETCONTENT,0,0);InterlockedExchange(&ecuSeen,0);InterlockedExchange(&stopping,0);InterlockedExchange(&connected,1);linepos=0;thread=CreateThread(NULL,0,rxworker,NULL,0,NULL);if(!thread){disconnect();err("Empfangsthread konnte nicht starten.");return;}SetWindowText(status,"Verbunden - passiver Empfang aktiv");}
static int waitack(LONG before,DWORD timeout){DWORD t=GetTickCount();while(!stopping&&!rxFault&&GetTickCount()-t<timeout){if(ackSeq!=before)return 1;Sleep(2);}return 0;}
static int command_ack(uint32_t id,int n,int a,int b,DWORD timeout){LONG before=ackSeq;return sendcmd(id,n,a,b)&&waitack(before,timeout);}
static DWORD WINAPI operation(void *unused){(void)unused;int ok=0;if(opType==1||opType==2){ok=command_ack(opType==1?WB_INDEX:WB_SENSOR,2,opType==1?opValue:opHw,opType==1?opHw:opValue,1800);logline(ok?"Modul bestaetigt Befehl. Legacy-ACK ohne Absender; keine Konfigurationsruecklesung verfuegbar.":"Keine Bestaetigung. Ergebnis unbekannt; Modul/Bus pruefen.");}
 else if(opType==3){LONG before=bitSeq;if(sendcmd(WB_BITRATE,2,opHw,opValue)){DWORD t=GetTickCount();while(GetTickCount()-t<1800&&!stopping&&!rxFault){if(bitSeq!=before&&bitHw==opHw&&bitValue==opValue){ok=1;break;}Sleep(2);}}logline(ok?"Bitrate bestaetigt gespeichert. Erst nach Modulreset aktiv; Adapter dann passend umstellen.":"Keine passende Bitratenbestaetigung. Sonderfirmware und neuen Bootloader pruefen.");}
 else if(opType==4){
  logline("F0-Update startet. Nur ein Modul am Bus; Versorgung nicht unterbrechen.");
  if(!command_ack(WB_ENTER,1,opHw,0,1800))logline("Keine App-Antwort; versuche bereits wartenden F0-Bootloader.");
  /* App ACK precedes its 50-tick reset. Repeated empty start catches the 800 ms boot window. */
  Sleep(150);int boot=0;for(int i=0;i<20&&!stopping&&!rxFault;i++){if(command_ack(WB_ENTER,0,0,0,40)){boot=1;break;}}
  if(!boot)goto done;
  if(!command_ack(WB_ERASE,0,0,0,5000))goto done;
  for(unsigned off=0;off<sizeof image;off+=8){Frame f={0};f.id=WB_DATA|off;f.ext=1;f.len=8;memcpy(f.data,image+off,8);LONG seq=ackSeq;if(!sendframe(&f)||!waitack(seq,1800))goto done;if((off&255)==0)PostMessage(win,WM_PROGRESS,off*100/sizeof image,0);}
  if(!command_ack(WB_REBOOT,0,0,0,1800))goto done;
  Sleep(1300);sendcmd(WB_PING,1,opHw,0);ok=1;PostMessage(win,WM_PROGRESS,100,0);
 }
 done:if(opType==4)logline(ok?"Image uebertragen und Neustart bestaetigt. CRC wird im Bootloader geprueft; Pong/Livewerte nach Neustart kontrollieren. Kein Flash-Readback im Protokoll.":"UPDATE ABGEBROCHEN. Modulversorgung erhalten; Bootloader-Wiederherstellung kann erforderlich sein.");PostMessage(win,WM_DONE,ok,0);return 0;}
static void startop(int type){if(!connected||rxFault){err("Zuerst eine funktionierende CAN-Verbindung herstellen.");return;}if(busy)return;if(!getnum(hw,0,8,&opHw))return;opType=type;
 if(type==1&&!getnum(indexBox,0,255,&opValue))return;
 if(type==2)opValue=(int)SendMessage(sensor,CB_GETCURSEL,0,0);
 if(type==3)opValue=(int)SendMessage(modrate,CB_GETCURSEL,0,0);
 if(type==1||type==2){if(MessageBox(win,"Einstellung dauerhaft schreiben? Hardware-ID muss eindeutig sein. Legacy-ACKs enthalten keinen Absender; waehrenddessen keine anderen Konfigurationswerkzeuge verwenden.",APP,MB_OKCANCEL|MB_ICONQUESTION)!=IDOK)return;}
 if(type==4){OPENFILENAME of={0};char path[MAX_PATH]="";of.lStructSize=sizeof of;of.hwndOwner=win;of.lpstrFilter="F0 Application BIN mit CRC\0*.bin\0";of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(!GetOpenFileName(&of))return;FILE *f=fopen(path,"rb");if(!f){err("Datei kann nicht gelesen werden.");return;}size_t n=fread(image,1,sizeof image,f);int extra=fgetc(f);fclose(f);if(extra!=EOF||!valid_image(image,n)){err("Kein gueltiges F0-App-Image: erwartet 25600 Bytes, Vektoren ab 0x08001800 und passende CRC32. Full-/Bootloader-Images werden abgelehnt.");return;}if(MessageBox(win,"F0-Modul mit passender Hardware/Firmware bestaetigt? Genau EIN Wideband-Modul am Bus anschliessen. Bootloader-Befehle sind Broadcast und koennen nicht auf ein Modul begrenzt werden. Anwendung wird geloescht und ersetzt. Versorgung stabil halten.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;InterlockedExchange(&heating,0);SetWindowText(heat,"Heiztelegramme starten (Broadcast)");}
 InterlockedExchange(&busy,1);EnableWindow(GetDlgItem(win,10),FALSE);EnableWindow(heat,FALSE);SendMessage(progress,PBM_SETPOS,0,0);HANDLE h=CreateThread(NULL,0,operation,NULL,0,NULL);if(h)CloseHandle(h);else{InterlockedExchange(&busy,0);EnableWindow(GetDlgItem(win,10),TRUE);EnableWindow(heat,TRUE);err("Arbeitsthread konnte nicht starten.");}}
static HWND control(const char *cls,const char *text,DWORD style,int x,int y,int w,int h,int id){HWND c=CreateWindowEx(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,NULL,NULL);SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;}
static HWND label(const char *text,int x,int y,int w){return control("STATIC",text,0,x,y,w,22,0);}
static HWND combo(int x,int y,int w,const char **items,int n){HWND c=control("COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,180,0);for(int i=0;i<n;i++)SendMessage(c,CB_ADDSTRING,0,(LPARAM)items[i]);SendMessage(c,CB_SETCURSEL,0,0);return c;}
static void refreshlive(void){int idx=0;char input[32],*end;GetWindowText(indexBox,input,sizeof input);long parsed=strtol(input,&end,10);if(!input[0]||*end||parsed<0||parsed>255)return;idx=(int)parsed;char s[1800];DWORD now=GetTickCount();Frame *f=&values[idx],*d=&diags[idx];const char *states[]={"Preheat","Warmup","Running","Sensor heizt nicht","Uebertemperatur","Untertemperatur","Heizen nicht erlaubt","CAN still"};
 if(!dataSeen[idx])snprintf(s,sizeof s,"Index %d: noch keine Messwerte. Standard-CAN-ID 0x%03X.",idx,0x190+2*idx);
 else snprintf(s,sizeof s,"Index %d | CAN 0x%03X / 0x%03X | Alter %.1f s%s\r\nLambda %.4f | AFR (14.7) %.2f | Temperatur %u C | %s\r\nProtokoll 0x%02X%s | Sensorflags 0x%02X\r\nDiagnose: %s | ESR %u Ohm | Nernst %.3f V\r\nPump PWM %.1f %% | Heater PWM %.1f %% | Diagnosealter %.1f s",
 idx,0x190+2*idx,0x191+2*idx,(now-dataSeen[idx])/1000.,now-dataSeen[idx]>1000?" - VERALTET":"",le16(f->data+2)/10000.,le16(f->data+2)*14.7/10000.,le16(f->data+4),(f->data[1]&1)?"gueltig":"UNGUELTIG",f->data[0],f->data[0]!=0xa0?" - UNBEKANNTE VERSION":"",f->data[1]&6,!diagSeen[idx]?"keine Daten":d->data[5]<8?states[d->data[5]]:"Unbekannt",le16(d->data),le16(d->data+2)/1000.,d->data[4]*100./255,d->data[6]*100./255,diagSeen[idx]?(now-diagSeen[idx])/1000.:0.);
 SetWindowText(live,s);}
static void displayframe(Frame *f,int tx){char s[180],data[30]="";for(int i=0;i<f->len;i++)sprintf(data+3*i,"%02X ",f->data[i]);snprintf(s,sizeof s,"%10lu %s %s %08X [%u] %s\r\n",GetTickCount(),tx?"TX":"RX",f->ext?"EXT":"STD",f->id,f->len,data);if(csv){fprintf(csv,"%lu,%s,%s,%08X,%u,%s\n",GetTickCount(),tx?"TX":"RX",f->ext?"EXT":"STD",f->id,f->len,data);}
 /* Bound the visible monitor; CSV retains all processed frames. */
 if(GetWindowTextLength(monitor)>45000){SetWindowText(monitor,"");}
 SendMessage(monitor,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);SendMessage(monitor,EM_REPLACESEL,0,(LPARAM)s);
 if(tx)return;
 if(f->ext&&f->id==WB_ACK&&f->len==8){int id=f->data[0];char row[150];snprintf(row,sizeof row,"HW %u | v%02X | Build %04u-%02u-%02u",id,f->data[1],2000+f->data[2],f->data[3],f->data[4]);seen[id]=GetTickCount();if(!discovered[id]){int pos=(int)SendMessage(modules,LB_ADDSTRING,0,(LPARAM)row);SendMessage(modules,LB_SETITEMDATA,pos,id);discovered[id]=1;}logline("Pong: %s (HW-ID ist nicht der CAN-Messindex)",row);}
 if(!f->ext&&f->id>=0x190&&f->id<=0x38f&&f->len==8){int idx=(f->id-0x190)/2;if(f->id&1){diags[idx]=*f;diagSeen[idx]=GetTickCount();}else{values[idx]=*f;dataSeen[idx]=GetTickCount();}}
}
static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{win=h;font=CreateFont(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");label(APP "  |  CAN setup & diagnostics",18,12,900);
 const char *ad[]={"PEAK PCAN-USB","SLCAN / Lawicel COM"},*rates[]={"500 kbit/s","1 Mbit/s"},*sensors[]={"LSU 4.9","LSU 4.2","LSU ADV","FAE LSU 4.9"};
 label("Adapter",18,48,100);adapter=combo(18,72,220,ad,2);label("Kanal / COM-Nr.",250,48,150);endpoint=control("EDIT","1",WS_BORDER|ES_NUMBER,250,72,120,25,0);label("Bus-Bitrate",385,48,130);rate=combo(385,72,145,rates,2);control("BUTTON","Verbinden / Trennen",0,550,70,185,29,10);control("BUTTON","Module suchen",0,750,70,180,29,11);
 label("Module (Pong; doppelte HW-IDs sind nicht unterscheidbar)",18,112,600);modules=control("LISTBOX","",WS_BORDER|LBS_NOTIFY|WS_VSCROLL,18,138,440,100,12);
 label("Ziel-Hardware-ID 0..8",480,112,220);hw=control("EDIT","4",WS_BORDER|ES_NUMBER,480,138,70,25,0);label("Messindex 0..255",570,112,180);indexBox=control("EDIT","0",WS_BORDER|ES_NUMBER,570,138,70,25,0);control("BUTTON","Index speichern",0,660,136,145,29,13);
 label("Hardware-ID: Loetbruecken. Mess-ID = 0x190 + 2 * Index.",480,177,500);label("Keine Zuordnung HW-ID <-> Messindex im CAN-Protokoll.",480,204,500);
 label("Sensortyp (Hardware-Eignung vorher pruefen)",18,252,410);sensor=combo(18,278,200,sensors,4);control("BUTTON","Sensor speichern",0,230,276,150,29,14);label("Modul-Bitrate (Sonderfirmware)",410,252,310);modrate=combo(410,278,155,rates,2);control("BUTTON","Bitrate speichern",0,580,276,150,29,15);
 label("Heizfreigabe: Broadcast alle Module, alle 50 ms. Ohne ECU-Sender verwenden.",18,318,930);label("Ist-Versorgung [V]",18,346,155);voltage=control("EDIT","12.0",WS_BORDER,180,344,70,25,0);label("Pump Gain [%] 0..100",270,346,190);gain=control("EDIT","100",WS_BORDER|ES_NUMBER,465,344,65,25,0);heat=control("BUTTON","Heiztelegramme starten (Broadcast)",0,550,342,370,29,16);
 live=control("STATIC","Noch keine Messwerte",WS_BORDER,18,388,912,130,0);
 control("BUTTON","F0 Firmware ueber CAN aktualisieren...",0,18,534,355,30,17);progress=control(PROGRESS_CLASS,"",0,390,536,330,25,0);control("BUTTON","CAN CSV starten/stoppen",0,737,534,193,30,18);
 monitor=control("EDIT","",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,18,580,912,155,0);SendMessage(monitor,EM_SETLIMITTEXT,60000,0);
 control("BUTTON","Alle Source-Settings / Grenzen",0,18,748,300,29,19);status=label("Getrennt | Windows x64 | F0 Update | Keine Hardwarevalidierung",335,754,600);SetTimer(h,1,50,NULL);break;}
 case WM_TIMER:if(heating&&connected&&!busy){if(ecuSeen||rxFault){InterlockedExchange(&heating,0);SetWindowText(heat,"Heiztelegramme starten (Broadcast)");logline("Heiztelegramme gestoppt: ECU-Sender oder CAN-Fehler erkannt.");}else if(GetTickCount()-heatLast>=50){Frame f={0};f.id=WB_STATUS;f.ext=1;f.len=3;f.data[0]=heatV;f.data[1]=1;f.data[2]=heatGain;if(!sendframe(&f))InterlockedExchange(&heating,0);heatLast=GetTickCount();}}{static unsigned tick;if(++tick%10==0)refreshlive();}break;
 case WM_COMMAND:switch(LOWORD(wp)){case 10:if(!busy)connectbus();break;case 11:if(connected&&!busy)sendcmd(WB_PING,1,255,0);break;case 12:if(HIWORD(wp)==LBN_SELCHANGE){int pos=(int)SendMessage(modules,LB_GETCURSEL,0,0);if(pos!=LB_ERR){char b[16];sprintf(b,"%d",(int)SendMessage(modules,LB_GETITEMDATA,pos,0));SetWindowText(hw,b);}}break;case 13:startop(1);break;case 14:startop(2);break;case 15:startop(3);break;case 17:startop(4);break;
 case 16:if(busy)break;if(heating){InterlockedExchange(&heating,0);sendcmd(WB_STATUS,3,heatV,0);SetWindowText(heat,"Heiztelegramme starten (Broadcast)");logline("Heizfreigabe beendet. Nach 250 ms gilt der firmwareeigene Timeout/Fallback.");}else if(connected&&!rxFault){char b[64],*end;GetWindowText(voltage,b,sizeof b);double v=strtod(b,&end);int g;if(!b[0]||*end||!(v>=0&&v<=25.5)){err("Versorgung muss 0..25.5 V sein (Dezimalpunkt).");break;}if(!getnum(gain,0,100,&g))break;if(ecuSeen){err("ECU-Statussender am Bus erkannt. Parallele Heizbefehle vermeiden; neu verbinden erst wenn ECU-Sender entfernt ist.");break;}if(MessageBox(h,"Heizung aller Module freigeben? Tatsaechliche Versorgungsspannung eingeben. Sensor muss trocken und fuer Heizen geeignet sein. CAN-Telegramm wirkt busweit.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)break;heatV=(uint8_t)(v*10+.5);heatGain=(uint8_t)g;InterlockedExchange(&heating,1);SetWindowText(heat,"Heiztelegramme stoppen");}break;
 case 18:if(csv){fclose(csv);csv=NULL;logline("CSV geschlossen.");}else{char path[MAX_PATH]="wideband-can.csv";OPENFILENAME of={0};of.lStructSize=sizeof of;of.hwndOwner=h;of.lpstrFilter="CSV\0*.csv\0";of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.Flags=OFN_OVERWRITEPROMPT;of.lpstrDefExt="csv";if(GetSaveFileName(&of)){csv=fopen(path,"w");if(csv)fprintf(csv,"host_tick_ms,direction,format,id_hex,dlc,data_hex\n");else err("CSV kann nicht angelegt werden.");}}break;
 case 19:MessageBox(h,"CAN dauerhaft: RusEfiIdx (0..255, AFR+EGT ab Startindex), SensorType (LSU49/42/ADV/FAE).\nBitrate: 500k/1M nur mit erweiterter F0-Firmware + passendem Bootloader; Neustart erforderlich. Keine Readback-Abfrage.\n\nCAN fluechtig: Heizfreigabe, externe Versorgungsspannung, Pump Gain (effektiv 0..100%). Status-Broadcast alle 50 ms, Timeout 250 ms. Force Heating ist kein gespeichertes Modulsetting.\n\nConfiguration aus boards/port.h, ohne CAN-Schreib-/Lesebefehl: AUX0/1 Kennlinien (je 8 Stuetzstellen + Werte), AUX-Quellen (AFR0/1, Lambda0/1, EGT0/1); pro AFR/EGT Kanal (je 4): RusEfiTx, RusEfiTxDiag, AemNetTx, RusEfiIdx, AemNetIdOffset. Diese erfordern separate serielle TS-Anbindung/Firmwareerweiterung.\n\nCompile-Time: Heizzeiten/Spannungsschwellen, Nernst-Ziel, Filter, PID/Sensorparameter, Kanalzahl und Hardware-Pins. Nicht zur Laufzeit konfigurierbar.\n\nHardware-ID: Loetbruecken 0..8. Kein CAN-Schreibbefehl. F1/OpenBLT-Updates werden hier nicht unterstuetzt.\n\nSiehe beiliegende SETTINGS.md und README.md fuer Protokoll und Grenzen.","Alle Einstellungen laut lokalem Source",MB_OK);break;}break;
 case WM_FRAME:{Frame *f=(Frame*)lp;displayframe(f,(int)wp);free(f);break;}
 case WM_LOG:{char *s=(char*)lp;SetWindowText(status,s);int n=GetWindowTextLength(monitor);SendMessage(monitor,EM_SETSEL,n,n);SendMessage(monitor,EM_REPLACESEL,0,(LPARAM)s);SendMessage(monitor,EM_REPLACESEL,0,(LPARAM)"\r\n");free(s);break;}
 case WM_PROGRESS:SendMessage(progress,PBM_SETPOS,wp,0);break;
 case WM_DONE:InterlockedExchange(&busy,0);EnableWindow(GetDlgItem(h,10),TRUE);EnableWindow(heat,TRUE);break;
 case WM_CLOSE:if(busy){err("CAN-Vorgang laeuft. Vor dem Schliessen Abschluss oder Timeout abwarten.");return 0;}disconnect();if(csv)fclose(csv);DestroyWindow(h);break;
 case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);break;
 default:return DefWindowProc(h,msg,wp,lp);}return 0;}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show){(void)prev;(void)cmd;SetProcessDPIAware();InitializeCriticalSection(&txLock);INITCOMMONCONTROLSEX cc={sizeof cc,ICC_PROGRESS_CLASS};InitCommonControlsEx(&cc);WNDCLASS wc={0};wc.lpfnWndProc=wndproc;wc.hInstance=inst;wc.lpszClassName="RusEFIWidebandManager";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);RegisterClass(&wc);HWND h=CreateWindow(wc.lpszClassName,APP,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,968,830,NULL,NULL,inst,NULL);if(!h)return 1;ShowWindow(h,show);MSG m;while(GetMessage(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessage(&m);}if(dll)FreeLibrary(dll);DeleteObject(font);DeleteCriticalSection(&txLock);return 0;}
