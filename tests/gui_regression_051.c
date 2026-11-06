/* Run with the desktop EXE in explicit --demo mode on an isolated desktop.
 * No adapter DLL or physical CAN hardware is used. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static HWND wait_window(const char *cls){for(int n=0;n<100;n++){HWND h=FindWindow(cls,NULL);if(h)return h;Sleep(100);}return NULL;}
static unsigned char *capture(HWND h,const char *file,size_t *size){RECT r;GetClientRect(h,&r);HDC dc=GetDC(h),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);HGDIOBJ old=SelectObject(mem,bmp);BitBlt(mem,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=r.right;bi.bmiHeader.biHeight=-r.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;*size=(size_t)r.right*r.bottom*4;unsigned char *bytes=malloc(*size);SelectObject(mem,old);assert(GetDIBits(mem,bmp,0,r.bottom,bytes,&bi,DIB_RGB_COLORS));if(file){BITMAPFILEHEADER bf={0};bf.bfType=0x4d42;bf.bfOffBits=sizeof bf+sizeof(BITMAPINFOHEADER);bf.bfSize=(DWORD)(bf.bfOffBits+*size);FILE *f=fopen(file,"wb");assert(f);fwrite(&bf,1,sizeof bf,f);fwrite(&bi.bmiHeader,1,sizeof bi.bmiHeader,f);fwrite(bytes,1,*size,f);fclose(f);}DeleteObject(bmp);DeleteDC(mem);ReleaseDC(h,dc);return bytes;}
static BOOL CALLBACK find_index(HWND h,LPARAM param){char cls[32],text[64];GetClassName(h,cls,sizeof cls);SendMessage(h,WM_GETTEXT,sizeof text,(LPARAM)text);if(!_stricmp(cls,"Edit")&&!strcmp(text,"0")){*(HWND*)param=h;return FALSE;}return TRUE;}
static void click(HWND parent,int id){SendMessage(parent,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)GetDlgItem(parent,id));Sleep(150);}
static void set_view(HWND tool,int view){SendMessage(GetDlgItem(tool,101),CB_SETCURSEL,view,0);SendMessage(tool,WM_COMMAND,MAKEWPARAM(101,CBN_SELCHANGE),(LPARAM)GetDlgItem(tool,101));Sleep(150);}
static void snapshot(HWND h,const char *name){size_t size;char path[MAX_PATH];snprintf(path,sizeof path,"Z:\\tmp\\wb-ui05-%s.bmp",name);free(capture(h,path,&size));}
static BOOL CALLBACK find_filename(HWND h,LPARAM param){char cls[48],text[128];GetClassName(h,cls,sizeof cls);SendMessage(h,WM_GETTEXT,sizeof text,(LPARAM)text);if(!_stricmp(cls,"Edit")&&IsWindowVisible(h)&&!(GetWindowLongPtr(h,GWL_STYLE)&ES_READONLY)){*(HWND*)param=h;}return TRUE;}
static void export_window(HWND graph,const char *path){
 DeleteFile(path);PostMessage(graph,WM_COMMAND,MAKEWPARAM(513,BN_CLICKED),(LPARAM)GetDlgItem(graph,513));
 HWND dialog=wait_window("#32770");assert(dialog);HWND edit=GetDlgItem(dialog,0x480);
 if(!edit){HWND combo=GetDlgItem(dialog,0x47c);edit=FindWindowEx(combo,NULL,"Edit",NULL);}
 if(!edit){snapshot(dialog,"save-dialog");EnumChildWindows(dialog,find_filename,(LPARAM)&edit);}assert(edit);
 SendMessage(edit,WM_SETTEXT,0,(LPARAM)path);PostMessage(dialog,WM_COMMAND,MAKEWPARAM(IDOK,BN_CLICKED),0);
 for(int n=0;n<50&&GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES;n++)Sleep(100);
 assert(GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES);Sleep(150);
}
static void compare_exports(const char *a,const char *b){FILE *fa=fopen(a,"rb"),*fb=fopen(b,"rb");assert(fa&&fb);int ca,cb,rows=0;do{ca=fgetc(fa);cb=fgetc(fb);assert(ca==cb);if(ca=='\n')rows++;}while(ca!=EOF);assert(rows>100&&rows<=602);fclose(fa);fclose(fb);}
static double last_export_time(const char *path){FILE *f=fopen(path,"r");assert(f);char line[1200];double result=-1;while(fgets(line,sizeof line,f)){if(!strncmp(line,"DEMO,",5)){char *p=strchr(line,',');p=strchr(p+1,',');result=strtod(p+1,NULL);}}fclose(f);return result;}

static RECT relative_rect(HWND parent,HWND child){RECT r;GetWindowRect(child,&r);MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;}
static HWND field(HWND parent,const char *cls,int x,int y){HWND h=NULL;HDC dc=GetDC(parent);int dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(parent,dc);x=MulDiv(x,dpi,96);y=MulDiv(y,dpi,96);while((h=FindWindowEx(parent,h,cls,NULL))){RECT r=relative_rect(parent,h);if(r.left==x&&r.top==y)return h;}return NULL;}
static void read_text(HWND h,char *out,int size){SendMessage(h,WM_GETTEXT,size,(LPARAM)out);}
static void equal_image(HWND main,const unsigned char *a,size_t size){size_t other;unsigned char *b=capture(main,NULL,&other);assert(size==other);unsigned different=0;for(size_t n=0;n<size;n+=4)if(memcmp(a+n,b+n,3))different++;free(b);if(different)fprintf(stderr,"scroll roundtrip changed %u pixels\n",different);assert(different==0);}
static int list_row(HWND list,const char *prefix){char text[256];wchar_t wide[256];int count=(int)SendMessage(list,LB_GETCOUNT,0,0);for(int i=0;i<count;i++){SendMessageW(list,LB_GETTEXT,i,(LPARAM)wide);WideCharToMultiByte(CP_UTF8,0,wide,-1,text,sizeof text,NULL,NULL);if(!strncmp(text,prefix,strlen(prefix)))return i;}return -1;}

int main(int argc,char **argv){
 HWND main=wait_window("RusEFIWidebandManager");assert(main);Sleep(700);HWND graph=GetDlgItem(main,210);assert(graph&&IsWindow(GetDlgItem(main,23)));
 if(argc>1&&!strcmp(argv[1],"can")){
  HWND endpoint=field(main,"Edit",24,226),hw=field(main,"Edit",24,522),index=field(main,"Edit",200,522),sensor=field(main,"ComboBox",24,652),rate=field(main,"ComboBox",24,778);
  assert(endpoint&&hw&&index&&sensor&&rate);SetWindowText(endpoint,"1");click(main,10);Sleep(400);click(main,11);Sleep(400);
  HWND list=GetDlgItem(main,12);assert(SendMessage(list,LB_GETCOUNT,0,0)==3);int hardware=list_row(list,"HW 4 |"),data0=list_row(list,"Data 0 |"),data1=list_row(list,"Data 1 |");assert(hardware>=0&&data0>=0&&data1>=0);
  click(main,11);Sleep(200);assert(SendMessage(list,LB_GETCOUNT,0,0)==3);
  SendMessage(list,LB_SETCURSEL,data1,0);SendMessage(main,WM_COMMAND,MAKEWPARAM(12,LBN_SELCHANGE),(LPARAM)list);char text[512];read_text(index,text,sizeof text);assert(!strcmp(text,"1"));read_text(hw,text,sizeof text);assert(!strcmp(text,"4"));Sleep(200);snapshot(main,"051-two-hw4-streams");
  click(main,10);SetWindowText(endpoint,"2");click(main,10);Sleep(400);SetWindowText(index,"0");SendMessage(sensor,CB_SETCURSEL,0,0);SendMessage(rate,CB_SETCURSEL,0,0);
  click(main,23);for(int n=0;n<40;n++){Sleep(100);read_text(index,text,sizeof text);if(!strcmp(text,"1")&&IsWindowEnabled(GetDlgItem(main,23)))break;}
  read_text(index,text,sizeof text);assert(!strcmp(text,"1"));assert(SendMessage(sensor,CB_GETCURSEL,0,0)==4);assert(SendMessage(rate,CB_GETCURSEL,0,0)==1);
  Sleep(200);HWND status=NULL,entry=NULL;while((entry=FindWindowEx(main,entry,"Edit",NULL))){read_text(entry,text,sizeof text);if(strstr(text,"Read F0 HW")){status=entry;break;}}assert(status);assert(strstr(text,"saved 1 Mbit/s")&&strstr(text,"active 500 kbit/s"));snapshot(main,"051-read-f0");
  assert(list_row(list,"Data 0 |")==-1);assert(list_row(list,"Data 1 |")==0);click(main,10);
  puts("Fake-PCAN GUI passed: two equal HW4 Pongs -> one HW row, separate data 0/1 rows, stream selection leaves target HW unchanged, Read F0 updates index/sensor/saved bitrate and reports active bitrate. No physical adapter used.");
 }else{
  ShowWindow(main,SW_RESTORE);SetWindowPos(main,NULL,0,0,1160,760,SWP_NOZORDER);Sleep(300);click(graph,511);SetFocus(graph);Sleep(300);
  RECT original=relative_rect(main,GetDlgItem(main,23));RECT client;GetClientRect(main,&client);assert(client.bottom<1000&&client.right<1280);
  size_t size;unsigned char *before=capture(main,NULL,&size);snapshot(main,"051-scroll-before");
  SendMessage(main,WM_VSCROLL,SB_BOTTOM,0);Sleep(150);RECT moved=relative_rect(main,GetDlgItem(main,23));assert(moved.top==original.top-(1000-client.bottom));snapshot(main,"051-scroll-bottom");
  for(int n=0;n<20;n++){
   SendMessage(main,WM_VSCROLL,SB_TOP,0);SendMessage(main,WM_HSCROLL,SB_BOTTOM,0);SendMessage(main,WM_VSCROLL,SB_PAGEDOWN,0);SendMessage(main,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);SendMessage(main,WM_HSCROLL,SB_TOP,0);SendMessage(main,WM_VSCROLL,SB_BOTTOM,0);
  }
  SendMessage(main,WM_HSCROLL,SB_TOP,0);SendMessage(main,WM_VSCROLL,SB_TOP,0);Sleep(400);RECT after=relative_rect(main,GetDlgItem(main,23));assert(EqualRect(&original,&after));RECT unchanged;GetClientRect(main,&unchanged);assert(EqualRect(&client,&unchanged));equal_image(main,before,size);free(before);snapshot(main,"051-scroll-returned");
  SendMessage(main,WM_HSCROLL,SB_BOTTOM,0);SendMessage(main,WM_VSCROLL,SB_BOTTOM,0);SetWindowPos(main,NULL,0,0,1450,1160,SWP_NOZORDER);Sleep(300);moved=relative_rect(main,GetDlgItem(main,23));assert(moved.left==original.left&&moved.top==original.top);
  for(int n=0;n<10;n++){SetWindowPos(main,NULL,0,0,1160,760,SWP_NOZORDER);SendMessage(main,WM_VSCROLL,SB_BOTTOM,0);SetWindowPos(main,NULL,0,0,1450,1160,SWP_NOZORDER);}SetWindowPos(main,NULL,0,0,1160,760,SWP_NOZORDER);Sleep(300);after=relative_rect(main,GetDlgItem(main,23));assert(EqualRect(&original,&after));
  puts("Native scroll regression passed: batched/composited repaint, 120 scroll actions, stable client area, exact child positions and pixel-identical return; resize while scrolled resets/clamps offsets. No hardware used.");
 }
 PostMessage(main,WM_CLOSE,0,0);return 0;
}
