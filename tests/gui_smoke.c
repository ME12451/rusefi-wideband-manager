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
int main(int argc,char **argv){HWND main=wait_window("RusEFIWidebandManager");assert(main);if(argc>1&&!strcmp(argv[1],"close")){PostMessage(main,WM_CLOSE,0,0);return 0;}Sleep(800);HWND graph=GetDlgItem(main,210);assert(graph);if(argc>1&&!strcmp(argv[1],"empty")){assert(IsWindowEnabled(GetDlgItem(main,10)));snapshot(main,"empty");puts("Disconnected startup captured; no adapter connected.");PostMessage(main,WM_CLOSE,0,0);return 0;}
 assert(!IsWindowEnabled(GetDlgItem(main,10)));assert(!IsWindowEnabled(GetDlgItem(main,23)));assert(!IsWindowEnabled(GetDlgItem(main,16)));assert(!IsWindowEnabled(GetDlgItem(main,17)));
 snapshot(main,"live");
 click(graph,511);char label[64];GetWindowText(GetDlgItem(graph,511),label,sizeof label);assert(strstr(label,"Resume"));
 size_t aSize,bSize;unsigned char *a=capture(graph,NULL,&aSize);Sleep(500);unsigned char *b=capture(graph,NULL,&bSize);assert(aSize==bSize);for(size_t n=0;n<aSize;n+=4)assert(!memcmp(a+n,b+n,3));free(a);free(b);snapshot(graph,"paused");
 export_window(graph,"Z:\\tmp\\wb-ui05-export-a.csv");Sleep(400);export_window(graph,"Z:\\tmp\\wb-ui05-export-b.csv");compare_exports("Z:\\tmp\\wb-ui05-export-a.csv","Z:\\tmp\\wb-ui05-export-b.csv");
 SendMessage(graph,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(400,300));SendMessage(graph,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(550,300));SendMessage(graph,WM_LBUTTONUP,0,MAKELPARAM(550,300));Sleep(100);snapshot(graph,"panned");
 export_window(graph,"Z:\\tmp\\wb-ui05-export-pan.csv");assert(last_export_time("Z:\\tmp\\wb-ui05-export-pan.csv")<last_export_time("Z:\\tmp\\wb-ui05-export-a.csv")-1);
 SendMessage(graph,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);Sleep(100);assert(SendMessage(GetDlgItem(graph,510),CB_GETCURSEL,0,0)==1);
 for(int i=0;i<6;i++)click(graph,500+i);snapshot(graph,"no-traces");for(int i=0;i<6;i++)click(graph,500+i);
 click(graph,512);snapshot(graph,"autoscale");
 HWND index=NULL;EnumChildWindows(main,find_index,(LPARAM)&index);assert(index);SetWindowText(index,"1");click(graph,511);Sleep(500);snapshot(main,"index1");
 SetWindowPos(main,NULL,0,0,1280,800,SWP_NOZORDER);Sleep(400);RECT rect;GetClientRect(graph,&rect);assert(rect.right>=800&&rect.bottom>=250);snapshot(main,"small");
 click(main,20);HWND tools=wait_window("WidebandF0Tools");assert(tools);set_view(tools,1);assert(IsWindowVisible(GetDlgItem(tools,121)));snapshot(tools,"graph-window");
 set_view(tools,7);assert(!IsWindowVisible(GetDlgItem(tools,121)));snapshot(tools,"console");set_view(tools,6);snapshot(tools,"rtrim");
 PostMessage(tools,WM_CLOSE,0,0);Sleep(100);PostMessage(main,WM_CLOSE,0,0);puts("Wine GUI smoke: live view, frozen buffer and identical paused CSV exports, history drag and exported time range, zoom, all traces off/on, auto scales, index switch, 1280x800 resize, tool graph and console passed. No CAN hardware used.");return 0;}
