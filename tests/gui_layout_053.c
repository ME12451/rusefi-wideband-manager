/* Run with the desktop EXE in --demo mode or with the test-only fake PCAN DLL on an isolated desktop.
 * No physical CAN hardware is used. */
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


static BOOL CALLBACK inside_main(HWND h,LPARAM param){HWND main=(HWND)param;if(GetParent(h)!=main||!IsWindowVisible(h))return TRUE;RECT r=relative_rect(main,h),client;GetClientRect(main,&client);if(r.left<0||r.top<0||r.right>client.right||r.bottom>client.bottom){char cls[60];GetClassName(h,cls,sizeof cls);fprintf(stderr,"clipped %s id=%d rect=%ld,%ld,%ld,%ld client=%ld,%ld\n",cls,GetDlgCtrlID(h),r.left,r.top,r.right,r.bottom,client.right,client.bottom);assert(0);}return TRUE;}
static void choose(HWND tools,int view){SendMessage(GetDlgItem(tools,101),CB_SETCURSEL,view,0);PostMessage(tools,WM_COMMAND,MAKEWPARAM(101,CBN_SELCHANGE),(LPARAM)GetDlgItem(tools,101));Sleep(180);assert(IsWindowVisible(GetDlgItem(tools,101)));assert(IsWindowVisible(GetDlgItem(tools,124)));assert(IsWindowVisible(GetDlgItem(tools,131)));RECT address=relative_rect(tools,GetDlgItem(tools,131));RECT nav=relative_rect(tools,GetDlgItem(tools,101));assert(address.top>=nav.bottom);assert(IsWindowVisible(GetDlgItem(tools,121))==(view==1));}
int main(int argc,char **argv){
 SetProcessDPIAware();
 HWND main=wait_window("RusEFIWidebandManager");assert(main);Sleep(700);ShowWindow(main,SW_RESTORE);SetWindowPos(main,NULL,0,0,1920,1032,SWP_NOZORDER);Sleep(350);
 HDC dc=GetDC(main);int dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(main,dc);fprintf(stderr,"1080p layout at %d DPI\n",dpi);
 LONG_PTR styles=GetWindowLongPtr(main,GWL_STYLE);assert(!(styles&(WS_HSCROLL|WS_VSCROLL)));EnumChildWindows(main,inside_main,(LPARAM)main);snapshot(main,"052-main");
 if(argc>1&&!strcmp(argv[1],"flood")){HWND endpoint=field(main,"Edit",24,180);assert(endpoint);SetWindowText(endpoint,"3");click(main,10);Sleep(500);}
 click(main,22);HWND tools=wait_window("WidebandF0Tools");assert(tools&&!IsZoomed(tools));RECT popup;GetWindowRect(tools,&popup);assert(popup.right-popup.left<1700&&popup.bottom-popup.top<1000);
 RECT nav=relative_rect(tools,GetDlgItem(tools,101));assert(nav.top>=0);assert(SendMessage(GetDlgItem(tools,101),CB_GETCURSEL,0,0)==7);snapshot(tools,"052-console");if(argc>1&&!strcmp(argv[1],"flood")){HWND edit=NULL;int got=0;while((edit=FindWindowEx(tools,edit,"Edit",NULL)))if(IsWindowVisible(edit)&&GetWindowLongPtr(edit,GWL_STYLE)&WS_VSCROLL)got=(int)SendMessage(edit,WM_GETTEXTLENGTH,0,0);assert(got>100);}
 for(int n=0;n<3;n++)for(int view=0;view<8;view++)choose(tools,view);
 choose(tools,4);snapshot(tools,"052-configuration");choose(tools,7);HWND focused=NULL;HWND child=NULL;while((child=FindWindowEx(tools,child,"Edit",NULL))){if(IsWindowVisible(child)&&GetWindowLongPtr(child,GWL_STYLE)&WS_VSCROLL)focused=child;}
 assert(focused);PostMessage(focused,WM_KEYDOWN,VK_ESCAPE,0);for(int n=0;n<20&&IsWindowVisible(tools);n++)Sleep(100);assert(!IsWindowVisible(tools));
 click(main,20);assert(IsWindowVisible(tools)&&!IsZoomed(tools));assert(SendMessage(GetDlgItem(tools,101),CB_GETCURSEL,0,0)==4);click(tools,124);assert(!IsWindowVisible(tools));
 click(main,22);ShowWindow(tools,SW_MAXIMIZE);Sleep(100);click(main,20);assert(!IsZoomed(tools));click(tools,124);
 if(argc>1&&!strcmp(argv[1],"flood"))click(main,10);
 SetWindowPos(main,NULL,0,0,900,600,SWP_NOZORDER);Sleep(150);SendMessage(main,WM_HSCROLL,SB_BOTTOM,0);SendMessage(main,WM_VSCROLL,SB_BOTTOM,0);SetWindowPos(main,NULL,0,0,1920,1032,SWP_NOZORDER);Sleep(150);assert(!(GetWindowLongPtr(main,GWL_STYLE)&(WS_HSCROLL|WS_VSCROLL)));EnumChildWindows(main,inside_main,(LPARAM)main);
 PostMessage(main,WM_CLOSE,0,0);puts("0.5.3 GUI passed: 1080p controls fully visible without root scrollbars; normal half-area popup; 24 console/view switches; Escape from monitor; Back / Close; reopen configuration restores normal size. Tested without physical hardware.");return 0;
}
