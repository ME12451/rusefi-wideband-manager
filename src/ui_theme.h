/* Native GDI theme. No bitmap assets or runtime framework dependencies. */
#define UI_BG RGB(240,244,249)
#define UI_INK RGB(29,47,67)
#define UI_MUTED RGB(102,119,139)
#define UI_BORDER RGB(219,227,237)
#define UI_NAVY RGB(17,34,54)
#define UI_ACCENT RGB(0,139,149)
#define UI_RED RGB(208,68,72)
static HFONT uiTitleFont,uiSmallFont,uiValueFont,uiPlotFont;
static HBRUSH uiBackgroundBrush,uiWhiteBrush,uiNavyBrush;
static void ui_text(HDC dc,const char *s,RECT r,HFONT f,COLORREF color,UINT flags){HGDIOBJ old=SelectObject(dc,f);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawText(dc,s,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);}
static void ui_fill(HDC dc,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);}
static void ui_outline(HDC dc,RECT r,COLORREF c,int radius){HPEN p=CreatePen(PS_SOLID,1,c);HGDIOBJ oldp=SelectObject(dc,p),oldb=SelectObject(dc,GetStockObject(NULL_BRUSH));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldp);SelectObject(dc,oldb);DeleteObject(p);}
static BOOL CALLBACK ui_font_child(HWND child,LPARAM unused){(void)unused;char cls[40];GetClassName(GetParent(child),cls,sizeof cls);SendMessage(child,WM_SETFONT,(WPARAM)(!strcmp(cls,"WidebandTrace")?uiPlotFont:GetDlgCtrlID(child)==109?uiSmallFont:font),TRUE);return TRUE;}
static void ui_theme_refresh(void){
 if(uiTitleFont){DeleteObject(uiTitleFont);}
 if(uiSmallFont){DeleteObject(uiSmallFont);}
 if(uiValueFont){DeleteObject(uiValueFont);}
 if(uiPlotFont){DeleteObject(uiPlotFont);}
 uiTitleFont=CreateFont(-px(25),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
 uiSmallFont=CreateFont(-px(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
 uiValueFont=CreateFont(-px(36),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
 uiPlotFont=CreateFont(-px(16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
 if(!uiBackgroundBrush){uiBackgroundBrush=CreateSolidBrush(UI_BG);uiWhiteBrush=CreateSolidBrush(RGB(255,255,255));uiNavyBrush=CreateSolidBrush(UI_NAVY);}
 if(toolsWindow)EnumChildWindows(toolsWindow,ui_font_child,0);
 if(mainGraph)EnumChildWindows(mainGraph,ui_font_child,0);
 if(live)SendMessage(live,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);
 if(status)SendMessage(status,WM_SETFONT,(WPARAM)uiSmallFont,TRUE);
}
static void ui_theme_dispose(void){DeleteObject(uiTitleFont);DeleteObject(uiSmallFont);DeleteObject(uiValueFont);DeleteObject(uiPlotFont);DeleteObject(uiBackgroundBrush);DeleteObject(uiWhiteBrush);DeleteObject(uiNavyBrush);}
static int ui_draw_button(const DRAWITEMSTRUCT *d){
 if(d->CtlType!=ODT_BUTTON)return 0;
 int id=(int)d->CtlID;int primary=id==10||id==20;COLORREF bg=primary?UI_ACCENT:RGB(255,255,255),fg=primary?RGB(255,255,255):UI_INK;
 if(d->itemState&ODS_SELECTED)bg=primary?RGB(0,111,121):RGB(229,237,245);
 if(d->itemState&ODS_DISABLED){bg=RGB(232,236,241);fg=RGB(139,150,163);}
 ui_fill(d->hDC,d->rcItem,bg);ui_outline(d->hDC,d->rcItem,primary?bg:UI_BORDER,px(8));char text[160];GetWindowText(d->hwndItem,text,sizeof text);
 ui_text(d->hDC,text,d->rcItem,font,fg,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
 if(d->itemState&ODS_FOCUS){RECT r=d->rcItem;InflateRect(&r,-4,-4);DrawFocusRect(d->hDC,&r);}return 1;
}
static LRESULT ui_control_color(UINT msg,WPARAM wp){HDC dc=(HDC)wp;SetTextColor(dc,UI_INK);SetBkColor(dc,RGB(255,255,255));if(msg==WM_CTLCOLORSTATIC)SetBkMode(dc,TRANSPARENT);return (LRESULT)uiWhiteBrush;}
