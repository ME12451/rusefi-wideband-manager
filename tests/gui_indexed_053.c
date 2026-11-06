/* Run with the desktop EXE and the test-only fake PCAN DLL on an isolated desktop.
 * Two HW4 modules are simulated; no physical CAN hardware is used. */
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


static void choose_target(HWND tools,int index){SendMessage(GetDlgItem(tools,131),CB_SETCURSEL,index,0);SendMessage(tools,WM_COMMAND,MAKEWPARAM(131,CBN_SELCHANGE),(LPARAM)GetDlgItem(tools,131));Sleep(100);}
static void read_target(HWND tools,HWND main,const char *expected){click(tools,110);char text[128];for(int n=0;n<40;n++){Sleep(100);read_text(GetDlgItem(tools,123),text,sizeof text);if(!strcmp(text,expected)&&IsWindowEnabled(GetDlgItem(main,23)))return;}assert(0);}
static void save_trim(HWND tools,HWND main,const char *value){SetWindowText(GetDlgItem(tools,123),value);PostMessage(tools,WM_COMMAND,MAKEWPARAM(112,BN_CLICKED),(LPARAM)GetDlgItem(tools,112));HWND dialog=wait_window("#32770");assert(dialog);PostMessage(dialog,WM_COMMAND,IDOK,0);char text[128];for(int n=0;n<40;n++){Sleep(100);read_text(GetDlgItem(tools,123),text,sizeof text);if(!strcmp(text,value)&&IsWindowEnabled(GetDlgItem(main,23)))return;}assert(0);}
int main(void){
 HWND main=wait_window("RusEFIWidebandManager");assert(main);Sleep(600);click(main,10);Sleep(400);click(main,11);click(main,20);HWND tools=wait_window("WidebandF0Tools");assert(tools);set_view(tools,6);
 choose_target(tools,0);read_target(tools,main,"123.45");SetWindowText(GetDlgItem(main,24),"3");SetWindowText(GetDlgItem(main,24),"4");char changedHW[4096];read_text(GetDlgItem(tools,120),changedHW,sizeof changedHW);assert(strstr(changedHW,"Rtrim not read"));read_text(GetDlgItem(tools,123),changedHW,sizeof changedHW);assert(!changedHW[0]);read_target(tools,main,"123.45");save_trim(tools,main,"150.00");choose_target(tools,1);read_target(tools,main,"234.56");save_trim(tools,main,"260.00");choose_target(tools,0);read_target(tools,main,"150.00");snapshot(tools,"053-target0");
 set_view(tools,5);click(tools,113);Sleep(500);char diagnostic[4096];read_text(GetDlgItem(tools,120),diagnostic,sizeof diagnostic);assert(strstr(diagnostic,"Measured pump current: 2.000 mA"));assert(strstr(diagnostic,"Target pump current: 1.000 mA"));choose_target(tools,1);read_text(GetDlgItem(tools,120),diagnostic,sizeof diagnostic);assert(strstr(diagnostic,"No diagnostic response"));read_target(tools,main,"260.00");read_text(GetDlgItem(tools,120),diagnostic,sizeof diagnostic);assert(strstr(diagnostic,"No diagnostic response"));choose_target(tools,0);read_target(tools,main,"150.00");set_view(tools,6);
 // The destination field is separate from the selected current address.
 HWND index=field(main,"Edit",180,414);assert(index);SetWindowText(index,"7");PostMessage(main,WM_COMMAND,MAKEWPARAM(13,BN_CLICKED),(LPARAM)GetDlgItem(main,13));HWND dialog=wait_window("#32770");assert(dialog);PostMessage(dialog,WM_COMMAND,IDOK,0);for(int n=0;n<40&&SendMessage(GetDlgItem(tools,131),CB_GETCURSEL,0,0)!=7;n++)Sleep(100);assert(SendMessage(GetDlgItem(tools,131),CB_GETCURSEL,0,0)==7);Sleep(300);read_target(tools,main,"150.00");
 choose_target(tools,1);read_target(tools,main,"260.00");snapshot(tools,"053-target1");
 choose_target(tools,22);click(tools,110);Sleep(2300);assert(IsWindowEnabled(GetDlgItem(main,23)));char report[4096];read_text(GetDlgItem(tools,120),report,sizeof report);assert(strstr(report,"Rtrim not read"));
 click(tools,124);click(main,10);PostMessage(main,WM_CLOSE,0,0);puts("Indexed GUI passed: two HW4 nodes, independent Rtrim 150/260 Ohm, stale target requires read, target index independent from destination field, index-change route verification, unknown target timeout. Fake CAN only.");return 0;
}
