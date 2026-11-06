/* F0-only CAN tools. All state and service transactions live on the GUI thread. */
#include <math.h>
static HWND trimEdit,trimLabel,toolClose,targetIndexBox,targetLabel,targetHint;
static HWND toolView,toolReport,graph,filterEdit,filterType,filterPause,flagData,flagDiag,toolNote;
static HWND toolButtons[8];
static const char *toolNames[]={"Dashboard / Warmup diagnostics","Live graph","Event history / Warnings","CAN statistics / Filter","F0 configuration / Backup","Extended diagnostics","Rtrim / Sensor selection","Raw CAN monitor"};
static const char *stateNames[]={"Preheat","Warmup","Closed loop","Heater fault","Overtemperature","Undertemperature","Heating disabled","CAN silent"};
static uint8_t boundIndex[9],addressIndexValue,requestIndex,serviceRouteIndex;
static uint8_t serviceConfig[9][5],serviceKnown[9],boardKnown[9],serviceTx,requestOp,requestHw,requestTx,requestData[5];
static uint8_t extraData[9][2][5],loadedConfig[5],expectedConfig[5];
static uint32_t extraSeen[9][2],configSeen[9],requestTime,refreshTime;
static int toolLayoutActive;
static int servicePending,serviceFlow,loadedValid,restoreSource=-1,verifyAfterApply,displayView;
static int serviceFlagData=1,serviceFlagDiag=1;
static uint16_t trimValue[9],loadedTrim,expectedTrim;
static uint8_t expectedTrimSensor;
static uint8_t trimKnown[9],trimSensor[9],trimReady[9];
static int trimVerify,restoreStage;
static char serviceMessage[1024]="Detect / read F0 checks the board and CAN extension. Old firmware does not respond to new commands.";
static int main_target(void){char b[32],*e;GetWindowText(hw,b,sizeof b);long n=strtol(b,&e,10);return b[0]&&!*e&&n>=0&&n<=8?(int)n:-1;}
static int selected_index(void){char b[32],*e;GetWindowText(indexBox,b,sizeof b);long n=strtol(b,&e,10);return b[0]&&!*e&&n>=0&&n<=255?(int)n:0;}
static int selected_target_index(void){return targetIndexBox?(int)SendMessage(targetIndexBox,CB_GETCURSEL,0,0):addressIndexValue;}
static void set_target_index(int index){if(addressIndexValue!=(uint8_t)index){if(trimEdit)SetWindowText(trimEdit,"");int target=main_target();if(target>=0){memset(extraSeen[target],0,sizeof extraSeen[target]);if(!busy)boardKnown[target]=serviceKnown[target]=trimKnown[target]=0;}}addressIndexValue=(uint8_t)index;if(targetIndexBox)SendMessage(targetIndexBox,CB_SETCURSEL,index,0);}
static const char *event_name(uint8_t c){return c<8?stateNames[c]:c==100?"WARNING: measurement data lost":c==101?"Measurement data restored":c==102?"WARNING: invalid lambda in closed loop":"Unknown status";}
static void service_note(const char *s){snprintf(serviceMessage,sizeof serviceMessage,"%s",s);if(toolNote)SetWindowText(toolNote,s);logline("%s",s);}
static void service_end(void){servicePending=0;serviceFlow=0;InterlockedExchange(&busy,0);EnableWindow(GetDlgItem(win,10),TRUE);EnableWindow(heat,TRUE);EnableWindow(GetDlgItem(win,23),!demoMode);EnableWindow(hw,TRUE);EnableWindow(modules,TRUE);if(targetIndexBox)EnableWindow(targetIndexBox,TRUE);}
static int request_service(uint8_t op,int target,const uint8_t *args){
 if(!connected||rxFault||target<0||target>8){err("Connect CAN and select a valid hardware ID.");return 0;}
 if(servicePending)return 0;
 if(!busy){int index=selected_target_index();if(index<0||index>255){err("Select a target data index.");return 0;}serviceRouteIndex=(uint8_t)index;}
 requestIndex=serviceRouteIndex;
 Frame f={0};f.ext=1;f.id=wbm_index_request((uint8_t)target,requestIndex);f.len=8;f.data[0]=op;f.data[1]=(uint8_t)target;f.data[2]=++serviceTx;
 if(args)memcpy(f.data+3,args,5);
 requestOp=op;requestHw=(uint8_t)target;requestTx=f.data[2];memcpy(requestData,f.data+3,5);requestTime=GetTickCount();servicePending=1;
 InterlockedExchange(&busy,1);EnableWindow(GetDlgItem(win,10),FALSE);EnableWindow(heat,FALSE);EnableWindow(GetDlgItem(win,23),FALSE);EnableWindow(hw,FALSE);EnableWindow(modules,FALSE);if(targetIndexBox)EnableWindow(targetIndexBox,FALSE);
 if(!sendframe(&f)){service_end();service_note("Could not send CAN request.");return 0;}return 1;
}
static void read_f0(void){
 if(demoMode||busy)return;
 int target=main_target();
 if(!connected||rxFault||target<0){err("Connect CAN and select a valid hardware ID.");return;}
 int index=selected_target_index();if(index<0||index>255){err("Select a target data index.");return;}
 boundIndex[target]=(uint8_t)index;boardKnown[target]=serviceKnown[target]=trimKnown[target]=0;
 memset(extraSeen[target],0,sizeof extraSeen[target]);
 verifyAfterApply=trimVerify=restoreStage=0;
 service_note("Reading F0 by Hardware ID AND target data index. Requires indexed firmware; no HW-only fallback.");
 request_service(WBM_INFO,target,NULL);
}
static int enhanced_available(void){int target=main_target();return connected&&!rxFault&&target>=0&&boardKnown[target]&&serviceKnown[target]&&boundIndex[target]==selected_target_index();}
static void tools_disconnected(void){memset(serviceKnown,0,sizeof serviceKnown);memset(boardKnown,0,sizeof boardKnown);memset(extraSeen,0,sizeof extraSeen);memset(trimKnown,0,sizeof trimKnown);servicePending=0;serviceFlow=0;verifyAfterApply=0;trimVerify=restoreStage=0;}
static void tools_frame(const Frame *f){
 if(!f->ext||!servicePending||!wbm_index_match(f->id,f->len,f->data,requestHw,requestIndex,requestOp,requestTx))return;
 int target=requestHw,op=requestOp;servicePending=0;
 if(f->data[2]!=WBM_OK){trimVerify=0;restoreStage=0;char m[200];snprintf(m,sizeof m,"F0 HW %d: command %d rejected (code %u: 1=invalid, 2=flash error, 3=bootloader missing, 4=unsupported).",target,op,f->data[2]);serviceKnown[target]=0;verifyAfterApply=0;service_end();service_note(m);return;}
 if(op==WBM_INFO){
  if(f->data[3]!=WBM_BOARD_F0||f->data[4]!=WBM_INDEX_VERSION||f->data[5]!=WBM_CAPS||f->data[6]!=0||f->data[7]!=1){boardKnown[target]=0;service_end();service_note("Incompatible board or CAN protocol. F0 configuration remains disabled.");return;}
  boardKnown[target]=1;serviceFlow=1;request_service(WBM_CONFIG,target,NULL);return;
 }
 if(op==WBM_CONFIG||op==WBM_APPLY||op==WBM_DEFAULTS){
  if(!wbm_valid_config(f->data[4],f->data[5],f->data[6])||f->data[7]>1||(op==WBM_CONFIG&&f->data[3]!=requestIndex)){serviceKnown[target]=0;service_end();service_note("Invalid configuration response.");return;}
  memcpy(serviceConfig[target],f->data+3,5);serviceKnown[target]=1;configSeen[target]=GetTickCount();
  if(op==WBM_APPLY||op==WBM_DEFAULTS){memcpy(expectedConfig,f->data+3,5);if(op==WBM_APPLY&&memcmp(expectedConfig,requestData,4)){serviceKnown[target]=0;service_end();service_note("Response differs from the requested configuration. Read again.");return;}verifyAfterApply=1;serviceRouteIndex=expectedConfig[0];boundIndex[target]=serviceRouteIndex;set_target_index(serviceRouteIndex);serviceKnown[target]=trimKnown[target]=0;request_service(WBM_CONFIG,target,NULL);return;}
  if(verifyAfterApply){verifyAfterApply=0;if(memcmp(serviceConfig[target],expectedConfig,4)){serviceKnown[target]=0;service_end();service_note("Readback does not match the saved configuration.");return;}service_note("Configuration saved and verified by targeted readback. Bitrate changes take effect after restart. Restart the module after changing sensor type.");}
  else {char m[300];snprintf(m,sizeof m,"Read F0 HW %d: index %u | sensor %u | saved %s | active %s. HW + target index must be unique.",target,serviceConfig[target][0],serviceConfig[target][1],serviceConfig[target][2]?"1 Mbit/s":"500 kbit/s",serviceConfig[target][4]?"1 Mbit/s":"500 kbit/s");service_note(m);}
  if(main_target()==target&&selected_target_index()==boundIndex[target]){char b[16];sprintf(b,"%u",serviceConfig[target][0]);SetWindowText(indexBox,b);SendMessage(sensor,CB_SETCURSEL,serviceConfig[target][1],0);SendMessage(modrate,CB_SETCURSEL,serviceConfig[target][2],0);serviceFlagData=!!(serviceConfig[target][3]&1);serviceFlagDiag=!!(serviceConfig[target][3]&2);if(flagData)SendMessage(flagData,BM_SETCHECK,serviceFlagData?BST_CHECKED:BST_UNCHECKED,0);if(flagDiag)SendMessage(flagDiag,BM_SETCHECK,serviceFlagDiag?BST_CHECKED:BST_UNCHECKED,0);}
 }
 if(op==WBM_CONFIG){request_service(WBM_RGET,target,NULL);return;}
 if(op==WBM_RGET||op==WBM_RSET){
  uint16_t value=le16(f->data+3);if(!(value==0||(value>=3000&&value<=30000))||f->data[5]>4||f->data[6]>1){trimKnown[target]=0;service_end();service_note("Invalid Rtrim response.");return;}
  trimValue[target]=value;trimSensor[target]=f->data[5];trimReady[target]=f->data[6];trimKnown[target]=1;
  if(op==WBM_RSET){if(value!=le16(requestData)||trimSensor[target]!=requestData[2]){restoreStage=0;service_end();service_note("Rtrim response differs from the write request. Read again.");return;}expectedTrim=value;expectedTrimSensor=trimSensor[target];trimVerify=1;request_service(WBM_RGET,target,NULL);return;}
  if(trimVerify){trimVerify=0;if(value!=expectedTrim||trimSensor[target]!=expectedTrimSensor){restoreStage=0;service_end();service_note("Rtrim readback mismatch. Restore incomplete.");return;}service_note("Rtrim saved and verified over CAN.");}
  if(main_target()==target&&trimEdit){char text[32];snprintf(text,sizeof text,"%.2f",value/100.);SetWindowText(trimEdit,text);}
  if(restoreStage==1){restoreStage=2;uint8_t args[5]={loadedConfig[0],loadedConfig[1],loadedConfig[2],loadedConfig[3],WBM_CONFIRM};request_service(WBM_APPLY,target,args);return;}
  if(restoreStage==2){restoreStage=0;if(memcmp(serviceConfig[target],loadedConfig,4)||value!=loadedTrim||trimSensor[target]!=loadedConfig[1]){service_end();service_note("Restore verification failed; read settings again. Partial changes may have been applied.");return;}service_note("Backup fully restored and verified (including sensor type and Rtrim).");}
  if(serviceKnown[target]&&serviceConfig[target][1]!=trimSensor[target]){serviceKnown[target]=0;if(serviceFlow==2){trimKnown[target]=0;service_end();service_note("Rtrim sensor does not match configuration; read this target again.");return;}serviceFlow=2;request_service(WBM_CONFIG,target,NULL);return;}
 }
 if(op==WBM_DIAG1||op==WBM_DIAG2){int slot=op-WBM_DIAG1;memcpy(extraData[target][slot],f->data+3,5);extraSeen[target][slot]=GetTickCount();if(op==WBM_DIAG1){request_service(WBM_DIAG2,target,NULL);return;}service_note("Extended F0 diagnostics read (snapshot).");}
 if(op==WBM_IDENTIFY)service_note("Identification confirmed: LEDs alternate for 10 seconds.");
 if(op==WBM_RESET){serviceKnown[target]=0;boardKnown[target]=0;service_note("Restart confirmed. Adjust adapter bitrate if necessary, then detect / read again.");}
 service_end();
}
static void enhanced_apply(uint8_t cfg[5]){
 int target=main_target();if(!enhanced_available()||busy){err("Detect / read F0 first; wait for the current operation to finish.");return;}
 if(cfg[0]!=boundIndex[target]&&streamObserved[cfg[0]]&&(channels[cfg[0]].hasData||channels[cfg[0]].hasDiag)){err("Destination data index has already been observed. Choose an unused index or reconnect after removing the conflict.");return;}
 if(!wbm_valid_config(cfg[1],cfg[2],cfg[3])){err("Invalid F0 configuration.");return;}
 if(MessageBox(win,"Save F0 configuration and verify by readback? The HW + target index pair must be unique on the bus. Check sensor compatibility. A changed saved bitrate takes effect at the next reset.",APP,MB_OKCANCEL|MB_ICONQUESTION)!=IDOK)return;
 uint8_t a[5]={cfg[0],cfg[1],cfg[2],cfg[3],WBM_CONFIRM};verifyAfterApply=0;request_service(WBM_APPLY,target,a);
}
static void enhanced_legacy(int type){
 if(busy)return;
 int target=main_target();uint8_t cfg[5];memcpy(cfg,serviceConfig[target],5);
 if(type==1){int n;if(!getnum(indexBox,0,255,&n))return;cfg[0]=(uint8_t)n;}
 if(type==2)cfg[1]=(uint8_t)SendMessage(sensor,CB_GETCURSEL,0,0);
 if(type==3)cfg[2]=(uint8_t)SendMessage(modrate,CB_GETCURSEL,0,0);
 enhanced_apply(cfg);
}
static int monitor_accept(const Frame *f){
 if(filterPause&&SendMessage(filterPause,BM_GETCHECK,0,0)==BST_CHECKED)return 0;
 if(filterType){int type=(int)SendMessage(filterType,CB_GETCURSEL,0,0);if((type==1&&f->ext)||(type==2&&!f->ext))return 0;}
 if(filterEdit){char b[32],*e;GetWindowText(filterEdit,b,sizeof b);if(b[0]){unsigned long id=strtoul(b,&e,16);if(*e||id>0x1fffffff||f->id!=id)return 0;}}
 return 1;
}
static int choose_file(int save,const char *filter,const char *defext,char path[MAX_PATH]){OPENFILENAME of={0};of.lStructSize=sizeof of;of.hwndOwner=toolsWindow&&IsWindowVisible(toolsWindow)?toolsWindow:win;of.lpstrFilter=filter;of.lpstrFile=path;of.nMaxFile=MAX_PATH;of.lpstrDefExt=defext;of.Flags=save?OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST:OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;return save?GetSaveFileName(&of):GetOpenFileName(&of);}
static void backup_save(void){int target=main_target();if(!enhanced_available()||busy){err("Read the current F0 configuration first.");return;}char path[MAX_PATH]="wideband-f0.wbf";if(!choose_file(1,"F0 CAN configuration\0*.wbf\0","wbf",path))return;uint8_t data[20];if(!trimKnown[target]){err("Read Rtrim and configuration first.");return;}if(!backup_encode(data,(uint8_t)target,serviceConfig[target],trimValue[target]))return;FILE *f=fopen(path,"wb");if(!f){err("Cannot create backup.");return;}int ok=fwrite(data,1,20,f)==20;if(fclose(f))ok=0;if(!ok)err("Could not write the complete backup.");else service_note("Last read F0 configuration saved with CRC. Measurements and heater enable are not persistent settings.");}
static void backup_load(void){char path[MAX_PATH]="";if(!choose_file(0,"F0 CAN configuration\0*.wbf\0","wbf",path))return;uint8_t b[21];FILE *f=fopen(path,"rb");if(!f){err("Cannot read file.");return;}size_t n=fread(b,1,sizeof b,f);fclose(f);if(!backup_decode(b,n,loadedConfig,&loadedTrim)){err("Invalid F0 backup (format/values/CRC). Nothing was written.");return;}loadedValid=1;restoreSource=b[6];service_note("Backup loaded. Comparison shows differences; restoring requires confirmation before writing.");}
static void export_events(void){char path[MAX_PATH]="wideband-events.csv";if(!choose_file(1,"CSV\0*.csv\0","csv",path))return;FILE *f=fopen(path,"w");if(!f){err("Export failed.");return;}fprintf(f,"host_tick_ms,index,code,event\n");for(unsigned n=0;n<eventCount;n++){Event *e=&events[(eventHead+EVENT_SIZE-eventCount+n)%EVENT_SIZE];fprintf(f,"%u,%u,%u,%s\n",e->t,e->index,e->code,event_name(e->code));}if(fclose(f))err("Could not write the complete CSV file.");}
static void export_trace(void){char path[MAX_PATH]="wideband-traces.csv";if(!choose_file(1,"CSV\0*.csv\0","csv",path))return;FILE *f=fopen(path,"w");if(!f){err("Export failed.");return;}fprintf(f,"host_tick_ms,index,lambda,valid,temp_C,heater_percent,pump_percent,esr_ohm,nernst_V,diag_fresh,std_fresh\n");for(unsigned i=0;i<256;i++){ChannelStats *c=&channels[i];for(unsigned n=0;n<c->count;n++){TracePoint *p=&c->point[(c->head+TRACE_SIZE-c->count+n)%TRACE_SIZE];fprintf(f,"%u,%u,%.4f,%u,%u,%.2f,%.2f,%u,%.3f,%u,%u\n",p->t,i,p->lambda/10000.,p->valid,p->temp,p->heater*100./255,p->pump*100./255,p->esr,p->dc/1000.,p->hasDiag,p->hasStandard);}}if(fclose(f))err("Could not write the complete CSV file.");}
static void export_stats(void){char path[MAX_PATH]="wideband-can-stats.csv";if(!choose_file(1,"CSV\0*.csv\0","csv",path))return;FILE *f=fopen(path,"w");if(!f){err("Export failed.");return;}fprintf(f,"format,id_hex,count,last_dlc,average_hz,max_gap_ms\n");for(unsigned n=0;n<CAN_STATS_SIZE;n++){CanStat *s=&canStats[n];if(s->used)fprintf(f,"%s,%08X,%u,%u,%.2f,%u\n",s->ext?"EXT":"STD",s->id,s->count,s->len,s->last==s->first?0:(s->count-1)*1000./(s->last-s->first),s->maxGap);}if(fclose(f))err("Could not write the complete CSV file.");}
static const char *cfg_sensor(unsigned n){const char *names[]={"LSU 4.9","LSU 4.2","LSU ADV","FAE LSU 4.9","LSU 4.9 Rtrim SW"};return n<5?names[n]:"invalid";}
static void report_refresh(void){if(!toolReport)return;char *b=calloc(1,100000);if(!b)return;size_t used=0;DWORD now=GetTickCount();int view=(int)SendMessage(toolView,CB_GETCURSEL,0,0),target=main_target();int targetIndex=selected_target_index();
 if(targetLabel){char label[90];snprintf(label,sizeof label,"HW %d / target data index",target);SetWindowText(targetLabel,label);}
#define REPORT_APPEND(...) do{if(used<97000){int n=snprintf(b+used,100000-used,__VA_ARGS__);if(n>0)used+=(size_t)n;}}while(0)
 if(view==0){REPORT_APPEND("MULTI-MODULE DASHBOARD  /  Traffic %llu RX / %llu TX\r\nTargets are HW ID + data index. A cached read belongs to that exact pair.\r\n\r\n",(unsigned long long)rxFrames,(unsigned long long)txFrames);for(unsigned i=0;i<256;i++){ChannelStats *c=&channels[i];if(!c->hasData&&!c->hasDiag)continue;Frame *f=&values[i],*d=&diags[i];int mapped=-1;for(int h=0;h<9;h++)if(serviceKnown[h]&&boundIndex[h]==i&&serviceConfig[h][0]==i)mapped=h;REPORT_APPEND("Index %u | HW %s",i,mapped<0?"unknown":"");if(mapped>=0)REPORT_APPEND("%d",mapped);REPORT_APPEND(" | %s | Lambda %.4f %s | %u C\r\n",c->stale?"OFFLINE":"Receiving",le16(f->data+2)/10000.,(f->data[1]&1)?"valid":"invalid",le16(f->data+4));REPORT_APPEND("  Status %s / Data age %.1fs / Diagnostic age %.1fs / Heater %.1f%% / ESR %u Ohm\r\n",!c->hasDiag?"unknown":c->state<8?stateNames[c->state]:"unknown",c->hasData?(now-c->last)/1000.:0.,c->hasDiag?(now-c->diagLast)/1000.:0.,d->data[6]*100./255,le16(d->data));if(c->warming)REPORT_APPEND("  Time since warmup was first observed: %.1f s\r\n",(now-c->warmStart)/1000.);else if(c->warmComplete)REPORT_APPEND("  Observed warmup time Warmup -> Closed loop: %.2f s\r\n",c->warmMs/1000.);else REPORT_APPEND("  Warmup time: no complete cycle observed\r\n");REPORT_APPEND("\r\n");}}
 if(view==2){REPORT_APPEND("EVENTS / WARNINGS / last %u events in this session\r\nTime = PC millisecond counter. No error history is stored on the module.\r\n\r\n",eventCount);for(unsigned n=0;n<eventCount;n++){Event *e=&events[(eventHead+EVENT_SIZE-1-n)%EVENT_SIZE];REPORT_APPEND("%10u ms | Index %u | %s\r\n",e->t,e->index,event_name(e->code));}}
 if(view==3){REPORT_APPEND("CAN STATISTICS / RX %llu / TX %llu / untracked IDs: %u\r\nFilter/pause apply only to the visible raw monitor. CSV and analysis continue.\r\nRates use PC timestamps, not controller bus-load or error counters.\r\n\r\n",(unsigned long long)rxFrames,(unsigned long long)txFrames,canStatOverflow);for(unsigned n=0;n<CAN_STATS_SIZE;n++){CanStat *c=&canStats[n];if(c->used)REPORT_APPEND("%s %08X / %u Frames / DLC %u / %.1f Hz / max. gap %u ms / age %.1f s\r\n",c->ext?"EXT":"STD",c->id,c->count,c->len,c->last==c->first?0:(c->count-1)*1000./(c->last-c->first),c->maxGap,(now-c->last)/1000.);}}
 if(view==4){REPORT_APPEND("F0 CAN CONFIGURATION / HW %d / target data index %d\r\n\r\n",target,targetIndex);if(target<0||!boardKnown[target]||!serviceKnown[target]||boundIndex[target]!=targetIndex)REPORT_APPEND("No confirmed F0 configuration. Use 'Detect / read F0'.\r\n");else{uint8_t *c=serviceConfig[target];REPORT_APPEND("Board STM32F042 / f0_module / CAN service v2 (HW + data index) / 1 wideband channel\r\nMeasurement index %u (CAN %03X / %03X)\r\nSensor %s\r\nSaved bitrate %s / active since startup %s\r\nStandard TX %s / Diagnostic TX %s\r\nLast readback %.1f s ago\r\n\r\n",c[0],0x190+2*c[0],0x191+2*c[0],cfg_sensor(c[1]),c[2]?"1 Mbit/s":"500 kbit/s",c[4]?"1 Mbit/s":"500 kbit/s",c[3]&1?"on":"off",c[3]&2?"on":"off",(now-configSeen[target])/1000.);REPORT_APPEND("'Write settings': index, sensor and module bitrate from main window; TX flags from checkboxes here.\r\nFactory defaults: reset sensor type/index/TX; saved bitrate is retained.\r\n");if(loadedValid){REPORT_APPEND("\r\nRtrim: module %.2f Ohm / file %.2f Ohm %s\r\n",trimValue[target]/100.,loadedTrim/100.,trimValue[target]==loadedTrim?"same":"DIFFERENT");REPORT_APPEND("\r\nBACKUP COMPARISON (source file HW %d)\r\n",restoreSource);const char *names[]={"Data index","Sensor type","Bitrate","Tx-Flags"};for(int i=0;i<4;i++)REPORT_APPEND("%s: module %u / file %u %s\r\n",names[i],c[i],loadedConfig[i],c[i]==loadedConfig[i]?"same":"DIFFERENT");}}}
 if(view==5){REPORT_APPEND("EXTENDED F0 DIAGNOSTICS / HW %d / target data index %d\r\nTargeted snapshot; 'Read diagnostics' updates both packets.\r\n\r\n",target,targetIndex);if(target>=0&&boundIndex[target]==targetIndex&&extraSeen[target][0]&&extraSeen[target][1]){uint8_t *a=extraData[target][0],*d=extraData[target][1];REPORT_APPEND("Nernst AC: %.3f V\r\nMeasured pump current: %.3f mA\r\nTarget pump current: %.3f mA\r\nEffective heater voltage (controller value): %.3f V\r\nSupply received over CAN (no local voltage sensor): %.1f V\r\nHeater state: %u (0 Preheat, 1 Warmup, 2 Closed loop, 3 Stopped)\r\nData age: packet 1 %.1f s / packet 2 %.1f s\r\n",le16(a)/1000.,(int16_t)le16(d)/1000.,(int16_t)le16(a+2)/1000.,le16(d+2)/1000.,d[4]/10.,a[4],(now-extraSeen[target][0])/1000.,(now-extraSeen[target][1])/1000.);}else REPORT_APPEND("No diagnostic response yet. Detect / read the board first.\r\n");}
 if(view==6){REPORT_APPEND("SELECTABLE RTRIM / BOSCH LSU 4.9 / HW %d / target data index %d\r\n\r\n",target,targetIndex);if(target<0||!trimKnown[target]||boundIndex[target]!=targetIndex)REPORT_APPEND("Rtrim not read yet. Use 'Detect / read F0'.\r\n");else REPORT_APPEND("Sensor type %s\r\nSaved Rtrim %.2f Ohm\r\nCorrection %s / Calibration %s\r\n\r\n",cfg_sensor(trimSensor[target]),trimValue[target]/100.,trimSensor[target]==4?"Software enabled":"disabled (different sensor type)",trimReady[target]?"ready":"MISSING - no valid lambda");REPORT_APPEND("Select 'LSU 4.9 Rtrim SW' in the main window for software correction.\r\nResistance: labeled value of your own sensor, 30.00..300.00 Ohm (0.01 Ohm).\r\nDo not guess. Software mode requires the physical trim resistor to be absent.\r\n\r\n'Save Rtrim' applies the Bosch mode selected in the main window and the resistance field.\r\n'Physical Rtrim' selects Bosch LSU4.9 + software value 0. Hardware must match.\r\nNormal LSU4.9 leaves software correction disabled; other sensor types do not use it either.\r\nChanges take effect only after verified saving. Do not change while tuning an engine.\r\n");}
 SetWindowText(toolReport,b);free(b);
#undef REPORT_APPEND
}
static HWND tool_control(const char *cls,const char *txt,DWORD style,int id){if(!strcmp(cls,"BUTTON")&&(style&0xf)==BS_PUSHBUTTON)style=(style&~0xf)|BS_OWNERDRAW;HWND h=CreateWindowEx(0,cls,txt,WS_CHILD|WS_VISIBLE|style,0,0,10,10,toolsWindow,(HMENU)(INT_PTR)id,NULL,NULL);SendMessage(h,WM_SETFONT,(WPARAM)font,TRUE);return h;}
static void tool_layout(void){
 if(!toolsWindow||toolLayoutActive)return;
 toolLayoutActive=1;RECT r;GetClientRect(toolsWindow,&r);int w=r.right,h=r.bottom;
 int p=px(12),row=px(34),gap=px(6),chosen=(int)SendMessage(toolView,CB_GETCURSEL,0,0);
 MoveWindow(toolView,p,p,w-px(180),px(240),TRUE);
 MoveWindow(toolClose,w-px(156),p,px(144),row,TRUE);
 ShowWindow(toolView,SW_SHOW);ShowWindow(toolClose,SW_SHOW);
 MoveWindow(targetLabel,p,p+row+gap,px(290),row,TRUE);MoveWindow(targetIndexBox,p+px(300),p+row+gap,px(110),px(240),TRUE);MoveWindow(targetHint,p+px(425),p+row+gap,w-px(450),row,TRUE);
 ShowWindow(targetLabel,SW_SHOW);ShowWindow(targetIndexBox,SW_SHOW);ShowWindow(targetHint,SW_SHOW);
 if(chosen==1){
  for(int i=0;i<8;i++)ShowWindow(toolButtons[i],SW_HIDE);
  HWND hide[]={toolReport,monitor,toolNote,filterEdit,filterType,filterPause,flagData,flagDiag,trimEdit,trimLabel};
  for(unsigned i=0;i<sizeof hide/sizeof *hide;i++)ShowWindow(hide[i],SW_HIDE);
  MoveWindow(graph,p,p+2*(row+gap),w-2*p,h-(p+2*(row+gap))-p,TRUE);ShowWindow(graph,SW_SHOW);
  toolLayoutActive=0;return;
 }
 int y=p+2*(row+gap);
 for(int i=0;i<8;i++){int column=i%4,line=i/4;MoveWindow(toolButtons[i],p+column*(w-2*p)/4,y+line*(row+gap),(w-2*p)/4-gap,row,TRUE);ShowWindow(toolButtons[i],SW_SHOW);}
 y+=2*(row+gap);
 MoveWindow(filterEdit,p,y,px(180),row,TRUE);MoveWindow(filterType,p+px(192),y,px(190),row*5,TRUE);MoveWindow(filterPause,p+px(394),y,w-px(418),row,TRUE);
 MoveWindow(flagData,p,y,px(285),row,TRUE);MoveWindow(flagDiag,p+px(300),y,px(285),row,TRUE);
 MoveWindow(trimLabel,p,y,px(350),row,TRUE);MoveWindow(trimEdit,p+px(362),y,px(180),row,TRUE);
 int haveRow=chosen==3||chosen==4||chosen==6||chosen==7;
 if(haveRow)y+=row+gap;
 MoveWindow(toolNote,p,y,w-2*p,px(44),TRUE);ShowWindow(toolNote,SW_SHOW);y+=px(52);
 int body=h-y-p;if(body<20)body=20;
 MoveWindow(toolReport,p,y,w-2*p,body,TRUE);MoveWindow(monitor,p,y,w-2*p,body,TRUE);
 ShowWindow(toolReport,chosen==7?SW_HIDE:SW_SHOW);ShowWindow(monitor,chosen==7?SW_SHOW:SW_HIDE);ShowWindow(graph,SW_HIDE);
 ShowWindow(filterEdit,chosen==3||chosen==7?SW_SHOW:SW_HIDE);ShowWindow(filterType,chosen==3||chosen==7?SW_SHOW:SW_HIDE);ShowWindow(filterPause,chosen==3||chosen==7?SW_SHOW:SW_HIDE);
 ShowWindow(flagData,chosen==4?SW_SHOW:SW_HIDE);ShowWindow(flagDiag,chosen==4?SW_SHOW:SW_HIDE);
 ShowWindow(trimEdit,chosen==6?SW_SHOW:SW_HIDE);ShowWindow(trimLabel,chosen==6?SW_SHOW:SW_HIDE);
 int view=chosen;
 const char *texts[8][8]={
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"},
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"},
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"},
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"},
 {"Detect / read F0","Save backup","Load / compare","Restore backup","Write settings","Factory defaults","Identify for 10s","Restart"},
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"},
 {"Detect / read F0","Read Rtrim","Save Rtrim","Physical Rtrim","Save backup","Load backup","Restore backup","Help"},
 {"Detect / read F0","Identify for 10s","Restart","Read diagnostics","Traces CSV","Events CSV","CAN statistics CSV","Features / Help"}};
 for(int i=0;i<8;i++){SetWindowText(toolButtons[i],texts[view][i]);}
 toolLayoutActive=0;RedrawWindow(toolsWindow,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_ERASE);
}
static void trim_write(int physical){
 int target=main_target();if(!enhanced_available()||!trimKnown[target]||busy){err("Detect / read F0 first.");return;}
 int type=physical?0:(int)SendMessage(sensor,CB_GETCURSEL,0,0);char b[64],*end;uint16_t value=0;
 if(!physical){GetWindowText(trimEdit,b,sizeof b);for(char *p=b;*p;p++)if(*p==',')*p='.';double ohms=strtod(b,&end),scaled=ohms*100.;if(!b[0]||*end||!(ohms>=30&&ohms<=300)||fabs(scaled-floor(scaled+.5))>0.00001){err("Enter your own sensor value with at most 2 decimal places: 30.00..300.00 Ohm.");return;}value=(uint16_t)(scaled+.5);}
 if(type!=0&&type!=4){err("Rtrim settings require Bosch LSU4.9 or LSU4.9 Rtrim SW.");return;}
 if(MessageBox(toolsWindow,physical?"Switch to a physical trim resistor? Hardware must match. Software correction will be disabled.":"Save the labeled sensor value permanently? Use software mode only without a physical trim resistor. Sensor and mode must match.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;
 uint8_t args[5]={(uint8_t)value,(uint8_t)(value>>8),(uint8_t)type,WBM_CONFIRM,0};request_service(WBM_RSET,target,args);
}
static void restore_backup(void){
 if(!loadedValid){err("Load a backup and check the comparison first.");return;}
 if(!enhanced_available()||!trimKnown[main_target()]||busy){err("Detect / read F0 first.");return;}
 if(loadedConfig[0]!=boundIndex[main_target()]&&streamObserved[loadedConfig[0]]){err("Backup destination index is already observed. Choose an unused index before restoring.");return;}
 if(MessageBox(toolsWindow,"Restore the full F0 configuration including sensor type and Rtrim? Hardware and sensor must match. Saving uses two verified steps; an error may leave partial changes. Do not interrupt power.",APP,MB_OKCANCEL|MB_ICONWARNING)!=IDOK)return;
 restoreStage=1;uint8_t args[5]={(uint8_t)loadedTrim,(uint8_t)(loadedTrim>>8),(uint8_t)(loadedConfig[1]==4?4:0),WBM_CONFIRM,0};request_service(WBM_RSET,main_target(),args);
}
static void tool_action(int n){int target=main_target(),view=(int)SendMessage(toolView,CB_GETCURSEL,0,0);if(n==0){read_f0();return;}
 if(view==6){if(n==1){if(enhanced_available()&&!busy)request_service(WBM_RGET,target,NULL);else err("Detect / read F0 first.");return;}if(n==2){trim_write(0);return;}if(n==3){trim_write(1);return;}if(n==4){backup_save();return;}if(n==5){backup_load();return;}if(n==6){restore_backup();return;}n=7;}
 if(view==4){if(n==1){backup_save();return;}if(n==2){backup_load();return;}if(n==3){restore_backup();return;}if(n==4){if(!enhanced_available()){err("Detect / read F0 first.");return;}uint8_t cfg[5];int index;if(!getnum(indexBox,0,255,&index))return;cfg[0]=(uint8_t)index;cfg[1]=(uint8_t)SendMessage(sensor,CB_GETCURSEL,0,0);cfg[2]=(uint8_t)SendMessage(modrate,CB_GETCURSEL,0,0);cfg[3]=(SendMessage(flagData,BM_GETCHECK,0,0)==BST_CHECKED?1:0)|(SendMessage(flagDiag,BM_GETCHECK,0,0)==BST_CHECKED?2:0);cfg[4]=0;enhanced_apply(cfg);return;}if(n==5){if(!enhanced_available()||busy){err("Detect / read F0 first.");return;}static const uint8_t defaults[]={2,0,3,4,0,1,5,6,7};if(defaults[target]!=boundIndex[target]&&streamObserved[defaults[target]]){err("Factory default index is already observed. Disconnect the conflict before resetting this target.");return;}if(MessageBox(toolsWindow,"Write F0 factory defaults? Index follows solder bridges; sensor follows firmware default; TX enabled. Saved bitrate is retained. Back up the current configuration first.",APP,MB_OKCANCEL|MB_ICONQUESTION)==IDOK){uint8_t args[5]={WBM_CONFIRM};request_service(WBM_DEFAULTS,target,args);}return;}n=n==6?1:2;}
 if(n==4){export_trace();return;}if(n==5){export_events();return;}if(n==6){export_stats();return;}if(n==7){MessageBox(toolsWindow,"F0 / CAN only. Dashboard, traces (up to 3000 points per index), events (1024), CAN statistics and CSV use standard frames.\n\nExtended service features require the new indexed F0 Manager firmware: configuration read/write/backup/restore, TX flags, board identification, LED identification, restart and additional diagnostics. No EGT, AEMNet, analog or TunerStudio interface.\n\nSet Hardware ID in the main window and the current target data index in this popup, then Detect / read F0. The main Data index field selects the graph and the new index when saving. Use unique HW + target index pairs. Indexed requests never fall back to HW-only commands. See README.md for requirements and limitations.",APP,MB_OK);return;}
 if(!enhanced_available()||busy){err("Detect / read F0 first; no concurrent operations.");return;}uint8_t args[5]={0};
 if(n==1){args[0]=10;request_service(WBM_IDENTIFY,target,args);}if(n==2){if(MessageBox(toolsWindow,"Restart F0? The saved bitrate will become active. Then adjust adapter bitrate if necessary and detect again.",APP,MB_OKCANCEL|MB_ICONQUESTION)==IDOK){args[0]=WBM_CONFIRM;request_service(WBM_RESET,target,args);}}if(n==3)request_service(WBM_DIAG1,target,NULL);
}
static LRESULT CALLBACK tool_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{toolsWindow=h;toolView=tool_control("COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL,101);for(int i=0;i<8;i++)SendMessage(toolView,CB_ADDSTRING,0,(LPARAM)toolNames[i]);SendMessage(toolView,CB_SETCURSEL,displayView,0);toolClose=tool_control("BUTTON","Back / Close",0,124);
 targetLabel=tool_control("STATIC","HW / target data index",0,130);targetIndexBox=tool_control("COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL,131);
 for(int index=0;index<256;index++){char text[12];sprintf(text,"%d",index);SendMessage(targetIndexBox,CB_ADDSTRING,0,(LPARAM)text);}SendMessage(targetIndexBox,CB_SETCURSEL,addressIndexValue,0);EnableWindow(targetIndexBox,!busy);
 targetHint=tool_control("STATIC","Indexed firmware required",0,132);SendMessage(targetHint,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);for(int i=0;i<8;i++)toolButtons[i]=tool_control("BUTTON","",0,110+i);
 filterEdit=tool_control("EDIT","",WS_BORDER|ES_AUTOHSCROLL,104);SendMessage(filterEdit,EM_SETCUEBANNER,0,(LPARAM)L"CAN ID hex / blank=all");filterType=tool_control("COMBOBOX","",CBS_DROPDOWNLIST,105);const char *types[]={"STD + EXT","STD only","EXT only"};for(int i=0;i<3;i++)SendMessage(filterType,CB_ADDSTRING,0,(LPARAM)types[i]);SendMessage(filterType,CB_SETCURSEL,0,0);filterPause=tool_control("BUTTON","Pause raw monitor",BS_AUTOCHECKBOX,106);
 flagData=tool_control("BUTTON","Send standard data",BS_AUTOCHECKBOX,107);flagDiag=tool_control("BUTTON","Send diagnostic data",BS_AUTOCHECKBOX,108);SendMessage(flagData,BM_SETCHECK,serviceFlagData?BST_CHECKED:BST_UNCHECKED,0);SendMessage(flagDiag,BM_SETCHECK,serviceFlagDiag?BST_CHECKED:BST_UNCHECKED,0);
 trimLabel=tool_control("STATIC","Sensor Rtrim [Ohm] (30.00..300.00)",0,122);trimEdit=tool_control("EDIT","",WS_BORDER|ES_AUTOHSCROLL,123);
 toolNote=tool_control("EDIT",serviceMessage,ES_MULTILINE|ES_READONLY,109);SendMessage(toolNote,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);toolReport=tool_control("EDIT","",WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOHSCROLL,120);SendMessage(toolReport,EM_SETLIMITTEXT,100000,0);graph=tool_control("WidebandTrace","",0,121);SetParent(monitor,h);tool_layout();report_refresh();break;}
 case WM_SIZE:tool_layout();break;
 case WM_DRAWITEM:if(ui_draw_button((const DRAWITEMSTRUCT*)lp))return TRUE;break;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:return ui_control_color(msg,wp);
 case WM_GETMINMAXINFO:{MINMAXINFO *m=(MINMAXINFO*)lp;m->ptMinTrackSize.x=px(700);m->ptMinTrackSize.y=px(400);break;}
 case WM_COMMAND:if(LOWORD(wp)==131&&HIWORD(wp)==CBN_SELCHANGE){if(!busy){set_target_index(selected_target_index());report_refresh();}return 0;}else if(LOWORD(wp)==124){ShowWindow(h,SW_HIDE);SetForegroundWindow(win);}else if(LOWORD(wp)>=110&&LOWORD(wp)<=117){tool_action(LOWORD(wp)-110);report_refresh();}else if(LOWORD(wp)==101&&HIWORD(wp)==CBN_SELCHANGE){displayView=(int)SendMessage(toolView,CB_GETCURSEL,0,0);tool_layout();report_refresh();}else if(LOWORD(wp)==102||LOWORD(wp)==103)InvalidateRect(graph,NULL,TRUE);break;
 case WM_CLOSE:ShowWindow(h,SW_HIDE);SetForegroundWindow(win);break;
 case WM_DESTROY:toolsWindow=NULL;break;
 default:return DefWindowProc(h,msg,wp,lp);}return 0;}
static void open_tools(void){
 if(!toolsWindow){
  HINSTANCE inst=GetModuleHandle(NULL);WNDCLASS wc={0};wc.hInstance=inst;wc.lpfnWndProc=tool_proc;wc.lpszClassName="WidebandF0Tools";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=uiBackgroundBrush;RegisterClass(&wc);
  MONITORINFO mi={0};mi.cbSize=sizeof mi;GetMonitorInfo(MonitorFromWindow(win,MONITOR_DEFAULTTONEAREST),&mi);RECT work=mi.rcWork;
  int width=(work.right-work.left)*7/10,height=(work.bottom-work.top)*7/10;
  if(width<px(700))width=px(700);
  if(height<px(400))height=px(400);
  if(width>work.right-work.left)width=work.right-work.left;
  if(height>work.bottom-work.top)height=work.bottom-work.top;
  int x=work.left+(work.right-work.left-width)/2,y=work.top+(work.bottom-work.top-height)/2;
  CreateWindowEx(WS_EX_COMPOSITED,wc.lpszClassName,"F0 CAN Dashboard / Diagnostics / Configuration",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,x,y,width,height,win,NULL,inst,NULL);
 }
 if(IsZoomed(toolsWindow)||IsIconic(toolsWindow))ShowWindow(toolsWindow,SW_RESTORE);
 ShowWindow(toolsWindow,SW_SHOWNORMAL);SetForegroundWindow(toolsWindow);
}
static void tools_tick(void){DWORD now=GetTickCount();if(servicePending&&now-requestTime>2000){serviceKnown[requestHw]=0;if(requestOp==WBM_INFO)boardKnown[requestHw]=0;verifyAfterApply=0;restoreStage=0;trimVerify=0;service_end();service_note("No matching F0 service response. Old firmware, wrong ID or bus error; write result unknown. No automatic write retry.");}if(now-refreshTime<500)return;refreshTime=now;if(targetLabel){char label[90];snprintf(label,sizeof label,"HW %d / target data index",main_target());SetWindowText(targetLabel,label);}analytic_tick(now);if(toolsWindow&&IsWindowVisible(toolsWindow)){if(displayView!=1&&displayView!=7)report_refresh();}}
