/*--------------------------------------------------------
// tclock.c : customize the tray clock -> KAZUBON 1997-2001
//--------------------------------------------------------*/
// Modified by Stoic Joker: Tuesday, March 2 2010 - 10:42:42
#include "tcdll.h"
#undef NTDDI_VERSION // allow our own runtime OS check
#define NTDDI_VERSION NTDDI_VISTA // used for drag&drop tooltip
#include <shlobj.h>//CFSTR_SHELLIDLIST
#include <process.h>//_beginthread

void CALLBACK OnDrawTimer(HWND hwnd, unsigned uMsg, uintptr_t idEvent, unsigned long dwTime);
void ReadStyleData(HWND hwnd, int preview);
void ReadFormatData(HWND hwnd, int preview);
void InitClock(HWND hwnd);
void EndClock();
int DestroyClock();
int UpdateClock(HWND hwnd, HFONT fnt);
int UpdateClockSize();
LRESULT OnCalcRect(HWND hwnd);
void OnCopy(HWND hwnd, LPARAM lParam);
void OnTooltipNeedText(UINT code, LPARAM lParam);
void DrawClockSub(HDC hdc, SYSTEMTIME* pt, int beat100);
static LRESULT CALLBACK Window_ClockTray_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static LRESULT CALLBACK Window_Clock_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static LRESULT CALLBACK Window_SecondaryClock_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
static LRESULT CALLBACK Window_SecondaryClock(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK Window_SecondaryTaskbarWorker_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

/*------------------------------------------------
  globals
--------------------------------------------------*/
#define MAX_MULTIMON_CLOCKS 4
int m_bMultimon;
ATOM m_multiClockClass=0;
typedef struct MultiClock {
	HWND worker;
	HWND clock;
	RECT workerRECT;
	long clock_base_width;
	long clock_base_height;
} MultiClock;
MultiClock m_multiClock[MAX_MULTIMON_CLOCKS];
int m_multiClocks=0;
/// draw variables
static HDC m_hdcClock=NULL;
static HDC m_hdcClockBG;
static char m_bHorizontalTaskbar=1;
static RECT m_rcClock = {0};
static RGBQUAD* m_color_start=NULL,* m_color_end;
static RGBQUAD* m_colorBG_start=NULL,* m_colorBG_end;
static HGDIOBJ m_oldfnt=NULL;
static HGDIOBJ m_oldbmp=NULL,m_oldbmpB=NULL;
/// text offsets
static int m_BORDER_MARGIN;
static double m_radian;
static int m_textheight,m_textwidth,m_textpadding;
static int m_leading;
static int m_vertfeed,m_vertpos;
static int m_horizfeed,m_horizpos;
/// colors
COLORREF m_basecolorBG, m_basecolorFont;
typedef struct BGRQUAD {
	BYTE rgbRed;
	BYTE rgbGreen;
	BYTE rgbBlue;
	BYTE rgbReserved;
} BGRQUAD;
typedef union COLOR {
	BGRQUAD quad;
	COLORREF ref;
} COLOR;
COLOR m_col;
COLOR m_col_normal;
COLOR m_col_hover;
COLOR m_colBG;

void ColorUpdate(unsigned text_color, unsigned back_color) {
	COLORREF old;
	m_col_normal.ref = m_col.ref = api.GetColor(text_color, 0);
	if(text_color == TCOLOR(TCOLOR_DEFAULT) || text_color == TCOLOR(TCOLOR_TRANSPARENT))
		m_col_hover.ref = api.GetColor(TCOLOR(TCOLOR_DEFAULT), TCOLORFLAG_HOVER);
	else
		m_col_hover = m_col_normal;
	old = m_colBG.ref;
	m_colBG.ref = api.GetColor(back_color, TCOLORFLAG_RAW1);
	if(m_colBG.ref != old)
		FillClockBG();
}

/*** misc variables ***/
static long m_clock_base_width; ///< original clock's width used by taskbar calculation
static long m_clock_base_height; ///< original clock's height used by taskbar calculation
static HWND m_clock_active = NULL;
#define WPARAM_SUBCLOCK 0x8000
int m_TipState=0;
HWND m_TipHwnd = NULL;
TOOLINFO m_TipInfo;
wchar_t m_format[256];
SYSTEMTIME m_LastTime={0};
int m_bDispSecond = 0;
int m_nDispBeat = 0;
enum{
	BLINK_NONE=0,
	BLINK_ON,
	BLINK_HOUR,
};
int m_BlinkState = BLINK_NONE;
int m_width=0, m_height=0, dvpos=0, dlineheight=0, dhpos=0;
char g_bHourZero;
char m_bNoClock=0;
static IDropTarget* m_droptarget;
/// drag&drop stuff
IDropTargetHelper* m_drophelper;
static FORMATETC m_mydroptextfmt={0,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
static FORMATETC m_mydropfmt={0,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
typedef struct{
	IDropTarget droptarget;
	IDataObject* dropobj;
	wchar_t* szTarget;
	HWND hwnd;
	LONG refs;
	int lasteffect;
	char disabled;
	char type;
} MyDragDrop_t;
///=============================================================================
///------------------------------------------------------+++--> drag&drop helper:
static void MyDragDrop__OnDropFiles_(MyDragDrop_t* self)   ///------------+++-->
{
	wchar_t fname[MAX_PATH];
	wchar_t app[MAX_PATH];
	wchar_t* buf,* pos;
	int i, num;
	STGMEDIUM med;
	if(self->dropobj->lpVtbl->GetData(self->dropobj,&m_mydropfmt,&med) != S_OK)
		return;
	
	num = DragQueryFile(med.hGlobal, (UINT)-1, NULL, 0);
	if(num <= 0)
		return;
	buf = malloc(MAX_PATH * sizeof(buf[0]) * num);
	if(!buf)
		return;
	pos = buf;
	for(i=0; i<num; ++i) {
		DragQueryFile(med.hGlobal, i, fname, _countof(fname));
		if(self->type == DF_RECYCLE || self->type == DF_COPY || self->type == DF_MOVE) {
			wcscpy(pos, fname);
			pos += wcslen(pos)+1;
		} else if(self->type == DF_OPEN) {
			if(num > 1)
				GetShortPathName(fname, pos, _countof(fname));
			else
				wcscpy(pos, fname);
			pos += wcslen(pos);
			if(num > 1 && i < (num-1))
				*pos++ = L' ';
		}
	}
	*pos = '\0';
	
	num = api.GetStr(REG_MOUSE, L"DropFilesApp", app, _countof(app), L"");
	
	if(self->type == DF_RECYCLE || self->type == DF_COPY || self->type == DF_MOVE) {
		SHFILEOPSTRUCT shfos = {0};
		shfos.hwnd = NULL;
		shfos.pFrom = buf;
		shfos.fFlags = FOF_ALLOWUNDO|FOF_NOCONFIRMATION;
		switch(self->type){
		case DF_COPY:
			shfos.wFunc = FO_COPY;
			shfos.pTo = app;
			break;
		case DF_MOVE:
			shfos.wFunc = FO_MOVE;
			shfos.pTo = app;
			break;
		default: // DF_RECYCLE:
			shfos.wFunc = FO_DELETE;
			break;
		}
		SHFileOperation(&shfos);
	} else if(self->type == DF_OPEN) {
		wchar_t* command = malloc((2+num+(pos-buf)) * sizeof(wchar_t));
		if(command){
			wsprintf(command, FMT("%s %s"), app, buf);
			api.ExecFile(command, NULL);
			free(command);
		}
	}
	free(buf);
}
#ifndef CFSTR_DROPDESCRIPTION
#	define CFSTR_DROPDESCRIPTION TEXT ("DropDescription")
typedef enum {
	DROPIMAGE_INVALID = -1,
	DROPIMAGE_NONE = 0,
	DROPIMAGE_COPY = DROPEFFECT_COPY,
	DROPIMAGE_MOVE = DROPEFFECT_MOVE,
	DROPIMAGE_LINK = DROPEFFECT_LINK,
	DROPIMAGE_LABEL = 6,
	DROPIMAGE_WARNING = 7,
	DROPIMAGE_NOIMAGE = 8,
} DROPIMAGETYPE;
typedef struct {
	DROPIMAGETYPE type;
	WCHAR szMessage[MAX_PATH];
	WCHAR szInsert[MAX_PATH];
} DROPDESCRIPTION;
#endif // CFSTR_DROPDESCRIPTION
static void MyDragDrop__SetDropTip_(MyDragDrop_t* self,int effect,const wchar_t* msg)
{
	if(api.OS >= TOS_VISTA && effect!=self->lasteffect){
		HGLOBAL hDesc=GlobalAlloc(GMEM_MOVEABLE,sizeof(DROPDESCRIPTION));
		if(hDesc){
			STGMEDIUM medium={TYMED_HGLOBAL};
			DROPDESCRIPTION* desc=GlobalLock(hDesc);
			medium.hGlobal=hDesc;
			desc->type=self->lasteffect=effect;
			if(msg){
				wcsncpy_s(desc->szMessage,MAX_PATH,msg,_TRUNCATE);
				wcsncpy_s(desc->szInsert,MAX_PATH,self->szTarget,_TRUNCATE);
			}else{
				desc->szMessage[0]='\0';
				desc->szInsert[0]='\0';
			}
			GlobalUnlock(hDesc);
			if(FAILED(self->dropobj->lpVtbl->SetData(self->dropobj,&m_mydroptextfmt,&medium,TRUE))){
				GlobalFree(hDesc);
			}
		}
	}
}
static void MyDragDrop__SetEffect_(MyDragDrop_t* self, DWORD grfKeyState, POINTL* pt, DWORD* pdwEffect){
	const wchar_t* msg=NULL;
	(void)grfKeyState;
	(void)pt;
	if(!self->disabled){
//		grfKeyState&=MK_CONTROL|MK_SHIFT|MK_ALT; // MK_BUTTON,MK_LBUTTON,MK_RBUTTON,MK_MBUTTON
//		if(grfKeyState==(MK_CONTROL|MK_SHIFT) || grfKeyState==MK_ALT){
//			*pdwEffect = DROPEFFECT_LINK; // Pin to... / Create link
//		}else if(grfKeyState==MK_CONTROL){
//			*pdwEffect = DROPEFFECT_COPY; // Open with... / Copy
//		}else
//			*pdwEffect = DROPEFFECT_MOVE; // Move
		switch(self->type){
		case DF_OPEN:
			msg=L"Open with %1";
			*pdwEffect=DROPEFFECT_COPY;
			break;
		case DF_COPY:
			msg=L"Copy to %1";
			*pdwEffect=DROPEFFECT_COPY;
			break;
		case DF_RECYCLE:
		case DF_MOVE:
			msg=L"Move to %1";
			*pdwEffect=DROPEFFECT_MOVE;
			break;
		default:
			*pdwEffect=DROPEFFECT_MOVE;
			return;
		}
	}else
		*pdwEffect = DROPEFFECT_NONE;
	MyDragDrop__SetDropTip_(self,*pdwEffect,msg);
}
///======================================================================================================================
///----------------------------------------------------------------------------+++--> IDropTarget callback implementation:
static HRESULT __stdcall MyDragDrop__QueryInterface(IDropTarget* droptarget, REFIID riid, void** ppvObject)   ///--+++-->
{ // unused
	(void)droptarget;
	(void)riid;
	(void)ppvObject;
	return E_NOINTERFACE;
}
static ULONG __stdcall MyDragDrop__AddRef(IDropTarget* droptarget)
{ // called on RegisterDragDrop
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	return InterlockedIncrement(&self->refs);
}
static ULONG __stdcall MyDragDrop__Release(IDropTarget* droptarget)
{ // called on RevokeDragDrop (frees object)
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	LONG ref=InterlockedDecrement(&self->refs);
	if(!ref){ // self destruct
		free(self->szTarget);
		free(self);
	}
	return ref;
}
static HRESULT __stdcall MyDragDrop__DragEnter(IDropTarget* droptarget, IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	if(pDataObject->lpVtbl->QueryGetData(pDataObject,&m_mydropfmt)!=S_OK)
		self->disabled|=0x80;
	else
		self->disabled&=~0x80;
	
	if(!self->disabled) // forward to helper for file preview (but only of not disabled to improve usability)
		m_drophelper->lpVtbl->DragEnter(m_drophelper,self->hwnd,pDataObject,(POINT*)&pt,*pdwEffect);
	self->dropobj=pDataObject;
	self->dropobj->lpVtbl->AddRef(self->dropobj);
	MyDragDrop__SetEffect_(self,grfKeyState,&pt,pdwEffect);
	return S_OK;
}
static HRESULT __stdcall MyDragDrop__DragOver(IDropTarget* droptarget, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	if(!self->disabled) // forward to helper for file preview (but only of not disabled to improve usability)
		m_drophelper->lpVtbl->DragOver(m_drophelper,(POINT*)&pt,*pdwEffect);
	MyDragDrop__SetEffect_(self,grfKeyState,&pt,pdwEffect);
	return S_OK;
}
static HRESULT __stdcall MyDragDrop__DragLeave(IDropTarget* droptarget)
{ // don't care
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	if(!self->disabled) // forward to helper for file preview (but only of not disabled to improve usability)
		m_drophelper->lpVtbl->DragLeave(m_drophelper);
	MyDragDrop__SetDropTip_(self,-1,NULL); // kill text
	self->dropobj->lpVtbl->Release(self->dropobj);
	return S_OK;
}
static HRESULT __stdcall MyDragDrop__Drop(IDropTarget* droptarget, IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
{
	MyDragDrop_t* self=(MyDragDrop_t*)droptarget;
	if(!self->disabled) // forward to helper for file preview (but only of not disabled to improve usability)
		m_drophelper->lpVtbl->Drop(m_drophelper,pDataObject,(POINT*)&pt,*pdwEffect);
	MyDragDrop__SetEffect_(self,grfKeyState,&pt,pdwEffect);
	MyDragDrop__OnDropFiles_(self);
	MyDragDrop__SetDropTip_(self,-1,NULL); // kill text
	self->dropobj->lpVtbl->Release(self->dropobj);
	return S_OK;
}
///=============================================================================================
///--------------------------------------------------------------+++--> drag&drop base functions:
static void MyDragDrop_Init(){   ///------------------------------------------------------+++-->
	m_mydroptextfmt.cfFormat=(CLIPFORMAT)RegisterClipboardFormat(CFSTR_DROPDESCRIPTION);
//	m_mydropfmt.cfFormat=(CLIPFORMAT)RegisterClipboardFormat(CFSTR_SHELLIDLIST);
	m_mydropfmt.cfFormat=CF_HDROP;
	CoCreateInstance(&CLSID_DragDropHelper,NULL,CLSCTX_INPROC_SERVER,&IID_IDropTargetHelper,(void**)&m_drophelper);
}
static void MyDragDrop_DeInit(){
	m_drophelper->lpVtbl->Release(m_drophelper);
}
static IDropTarget* MyDragDrop_Create(HWND hwnd){
	static IDropTargetVtbl dropfuncs={
		MyDragDrop__QueryInterface,
		MyDragDrop__AddRef,
		MyDragDrop__Release,
		MyDragDrop__DragEnter,
		MyDragDrop__DragOver,
		MyDragDrop__DragLeave,
		MyDragDrop__Drop,
	};
	MyDragDrop_t* self=calloc(1,sizeof(MyDragDrop_t));
	self->droptarget.lpVtbl=&dropfuncs;
	self->hwnd=hwnd;
	return (IDropTarget*)self;
}
static void MyDragDrop_Enable(IDropTarget* myobj, int bEnable){
	MyDragDrop_t* self=(MyDragDrop_t*)myobj;
	self->disabled=!bEnable;
	free(self->szTarget); self->szTarget=NULL;
	if(bEnable){
		self->type = (char)api.GetInt(REG_MOUSE, L"DropFiles", DF_RECYCLE);
		switch(self->type){
		case DF_NONE:
			self->disabled = 1;
			break;
		case DF_RECYCLE:
			self->szTarget = wcsdup(L"Recycle Bin");
			break;
		default:{
			wchar_t app[MAX_PATH];
			int size = api.GetStr(REG_MOUSE, L"DropFilesApp", app, _countof(app), L"");
			if(!size){
				self->disabled = 1;
			}else{
				get_title(app, app); // get only exe/folder name
				size = (int)(wcslen(app)+1) * sizeof(self->szTarget[0]);
				self->szTarget = malloc(size);
				if(self->szTarget)
					memcpy(self->szTarget, app, size);
			}
			break;}
		}
	}
}

//================================================================================================
//---------------------------------------------------------+++--> Create Mouse-Over ToolTip Window:
void CreateTip(HWND hwnd)   //--------------------------------------------------------------+++-->
{
	memset(&m_TipInfo, 0, sizeof(TOOLINFO));
	m_TipInfo.cbSize = sizeof(TOOLINFO);
	if(api.OS <= TOS_XP_64)
		m_TipInfo.cbSize -= sizeof(m_TipInfo.lpReserved);
	m_TipInfo.uFlags = TTF_IDISHWND | TTF_TRACK;
	m_TipInfo.hwnd = hwnd;
	m_TipInfo.uId = (UINT_PTR)hwnd;
	m_TipInfo.lpszText = LPSTR_TEXTCALLBACK_nowarn;
	m_TipHwnd = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, (WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX),
					CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, NULL,NULL, api.hInstance,NULL);
	if(!m_TipHwnd)
		return;
	
	SendMessage(m_TipHwnd, TTM_ADDTOOL, 0, (LPARAM)&m_TipInfo);
	SendMessage(m_TipHwnd, TTM_SETMAXTIPWIDTH, 0, 300);
	SendMessage(m_TipHwnd, TTM_TRACKPOSITION, 0, MAKELPARAM(0x7FFF,0x7FFF));
}
void DestroyTip()
{
	if(!m_TipHwnd) return;
	SendMessage(m_TipHwnd,TTM_DELTOOL,0,(LPARAM)&m_TipInfo);
	DestroyWindow(m_TipHwnd); m_TipHwnd=NULL;
}
void ShowTip(){
	SendMessage(m_TipHwnd, TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_TipInfo);
	api.PositionWindow(m_TipHwnd, 0);
}

void SubsSendResize(){
	typedef struct WINDOWPOS_fix {
		HWND hwnd;
		HWND hwndInsertAfter;
		RECT rect;
		UINT flags;
	} WINDOWPOS_fix;
	WINDOWPOS_fix pos;
	int clock_id;
	HWND taskbar;
	
	COMPILE_ASSERT(sizeof(WINDOWPOS) == sizeof(WINDOWPOS_fix));
	
	if(api.OS < TOS_WIN10)
		return;
	for(clock_id=0; clock_id<m_multiClocks; ++clock_id){
		// force a taskbar refresh
		taskbar = GetParent(m_multiClock[clock_id].worker);
		memset(&pos, 0, sizeof(pos));
		pos.flags = SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
		pos.hwnd = taskbar;
		GetClientRect(taskbar, &pos.rect);
		SendMessage(taskbar, WM_WINDOWPOSCHANGED, 0, (LPARAM)&pos);
	}
}
void SubsDestroy(){
	int clock_id;
	for(clock_id=0; clock_id<m_multiClocks; ++clock_id){
		if(IsWindow(m_multiClock[clock_id].worker)) {
			RemoveWindowSubclass(m_multiClock[clock_id].worker, Window_SecondaryTaskbarWorker_Hooked, clock_id);
			SendMessage(m_multiClock[clock_id].clock, WM_CLOSE, 0xDEED, 0); // handles RemoveWindowSubclass(m_multiClock[clock_id].clock)
		}
	}
	SubsSendResize();
	for(; m_multiClocks; )
		m_multiClock[--m_multiClocks].worker = NULL;
}
void SubsCreate(){
	char classname[GEN_BUFF];
	HWND hwndBar;
	HWND hwndChild;
	int clock_id;
	RECT rc;
	if(m_multiClocks || !m_bMultimon) return;
	// loop all secondary taskbars
	hwndBar=FindWindowExA(NULL,NULL,"Shell_SecondaryTrayWnd",NULL);
	while(hwndBar){
		hwndChild = GetWindow(hwndBar,GW_CHILD);
		while(hwndChild){
			GetClassNameA(hwndChild, classname, _countof(classname));
			if(!strcmp(classname,"WorkerW")){
				if(m_multiClocks==MAX_MULTIMON_CLOCKS)
					break;
				for(clock_id=0; clock_id<m_multiClocks && hwndChild!=m_multiClock[clock_id].worker; ++clock_id); // try to find existing
				if(clock_id==m_multiClocks){ // new
					m_multiClock[clock_id].worker = hwndChild;
					GetClientRect(hwndChild, &m_multiClock[clock_id].workerRECT);
					m_multiClock[clock_id].clock = FindWindowEx(hwndBar, NULL, L"ClockButton", NULL);
					if(m_multiClock[clock_id].clock) { // real clock, hook it
						GetClientRect(m_multiClock[clock_id].clock, &rc);
						m_multiClock[clock_id].clock_base_width = rc.right;
						m_multiClock[clock_id].clock_base_height = rc.bottom;
						SetWindowSubclass(m_multiClock[clock_id].clock, Window_SecondaryClock_Hooked, clock_id, (DWORD_PTR)&m_multiClock[clock_id]);
						SendMessage(m_multiClock[clock_id].clock, WM_CREATE, 0, 0);
					} else { // create our own clock
						m_multiClock[clock_id].clock_base_width = 0;
						m_multiClock[clock_id].clock_base_height = 0;
						if(!m_multiClockClass) {
							WNDCLASSEX wndclass = {sizeof(WNDCLASSEX), CS_PARENTDC, Window_SecondaryClock, 0, 0, 0/*hInstance*/, NULL, NULL, NULL, NULL, L"ClockButton", NULL};
							wndclass.hCursor = LoadCursor(NULL,IDC_ARROW);
							wndclass.hInstance = api.hInstance;
							m_multiClockClass = RegisterClassEx(&wndclass);
						}
						m_multiClock[clock_id].clock = CreateWindowEx(0, MAKEINTATOM(m_multiClockClass), NULL, WS_CHILD|WS_CLIPSIBLINGS|WS_VISIBLE, 0,0,5,5, hwndBar, 0, 0, NULL);
					}
					if(!m_multiClock[clock_id].clock)
						break;
					SetWindowSubclass(hwndChild, Window_SecondaryTaskbarWorker_Hooked, clock_id, (DWORD_PTR)&m_multiClock[clock_id]);
					++m_multiClocks;
				}
				break;
			}
			hwndChild = GetWindow(hwndChild,GW_HWNDNEXT);
		}
		hwndBar = FindWindowExA(NULL,hwndBar,"Shell_SecondaryTrayWnd",NULL);
	}
	SubsSendResize();
}
int SetupClockAPI(int version, TClockAPI* api); // clock_api.c
//================================================================================================
//---------------------------------------------------------------------+++--> Initialize the Clock:
void InitClock(HWND hwnd)   //--------------------------------------------------------------+++-->
{
	gs_hwndClock = hwnd;
	gs_tray = GetParent(hwnd);
	gs_taskbar = GetParent(gs_tray);
	LoadLibrary(L"T-Clock" ARCH_SUFFIX); // self-reference
	SetupClockAPI(CLOCK_API, NULL); // initialize API
	GetClientRect(hwnd, &m_rcClock); // use original clock size until we've loaded our settings
	m_clock_base_width = m_rcClock.right;
	m_clock_base_height = m_rcClock.bottom;
	{//"top" margin detection // 2px Win8 default (Vista+)
		RECT rcBar; GetClientRect(gs_taskbar,&rcBar);
		if(rcBar.right>rcBar.bottom){//horizontal
			m_BORDER_MARGIN = rcBar.bottom-m_rcClock.bottom;
		}else{//vertical
			m_BORDER_MARGIN = rcBar.right-m_rcClock.right;
		}
	}
	// Win 10 1st-anniversary update requires us to hook the tray
	SetWindowSubclass(gs_tray, Window_ClockTray_Hooked, 0, 0);
	SetWindowSubclass(hwnd, Window_Clock_Hooked, 0, 0);
	
	CreateTip(hwnd); // Create Mouse-Over ToolTip Window & Contents
	MyDragDrop_Init();
	m_droptarget = MyDragDrop_Create(hwnd);
	RegisterDragDrop(hwnd, m_droptarget);
	
	SendMessage(hwnd, CLOCKM_REFRESHCLOCK, 0, 0); // reads settings and creates clock
	SendMessage(hwnd, CLOCKM_REFRESHTASKBAR, 0, 0);
	if(!m_color_start) // fixes display issue when clock has same size as T-Clock (Win7 + default settings)
		UpdateClockSize();
}

extern HANDLE g_exit_lock; // main.c
EXTERN_C IMAGE_DOS_HEADER __ImageBase; ///< own dll handle
static void SelfDestruct(void* user)
{ // never crashed without this SelfDestruct stub, but better safe then sorry :P Crashing the explorer isn't that nice.
	(void)user;
	// make sure we're out of our hooked message loop
	SendMessage(gs_tray, WM_NULL, 0, 0);
	InvalidateRect(gs_hwndClock, NULL, TRUE);
	SendMessage(gs_hwndClock, WM_NULL, 0, 0);
	// refresh primary taskbar
	InvalidateRect(gs_taskbar, NULL, TRUE);
	SendMessage(gs_taskbar, WM_SIZE, SIZE_RESTORED, 0);
	// we could use FreeLibraryAndExitThread on XP+, but this "hack" should be ok for now.
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, &__ImageBase, 0, NULL); // die painfully
}
//==============================================================================
//---+++--> End Clock Procedure (Window_Clock_Hooked) - (Before?) Removing Hook:
void EndClock()   //------------------------------------------------------+++-->
{
	g_exit_lock = CreateSemaphore(NULL, 1, 1, kConfigName+1); // SendMessage() "fix"
	WaitForSingleObject(g_exit_lock, 0);
	SubsDestroy(); // <- causes our parents SendMessage() call to exit prematurely
	if(m_multiClockClass){
		UnregisterClass(MAKEINTATOM(m_multiClockClass), 0);
		m_multiClockClass = 0;
	}
	RevokeDragDrop(gs_hwndClock); // frees m_droptarget
	MyDragDrop_DeInit();
	DestroyTip();
	
	KillTimer(gs_hwndClock, TCLOCK_TIMER_ID);
	RemoveWindowSubclass(gs_tray, Window_ClockTray_Hooked, 0);
	RemoveWindowSubclass(gs_hwndClock, Window_Clock_Hooked, 0);
	// Windows uses timer ID 0 for every-minute drawing
	// make sure it is set (again)
	SetTimer(gs_hwndClock, WIN_CLOCK_TIMER_ID, 10*1000, NULL);
	
	DestroyClock();
	EndNewAPI(gs_hwndClock);
	
	_beginthread(SelfDestruct, 0, NULL);
}
static LRESULT CALLBACK Window_ClockTooltip_Hooked(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	(void)dwRefData;
	
	switch(uMsg) {
	case WM_DESTROY:
		RemoveWindowSubclass(hwnd, Window_ClockTooltip_Hooked, uIdSubclass);
		break;
	case WM_WINDOWPOSCHANGING: {
		WINDOWPOS* pwp = (WINDOWPOS*)lParam;
		pwp->flags |= SWP_NOMOVE;
		break;}
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
/*--------------------------------------------------------------
  subclass procedure of the clock's tray to control clock size
---------------------------------------------------------------*/
static LRESULT CALLBACK Window_ClockTray_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	(void)uIdSubclass, (void)dwRefData;
	switch(message) {
	case(WM_USER+100):{// send by windows to get tray size
		union {
			struct{
				int16_t width;
				int16_t height;
			} part;
			LRESULT combined;
		} size;
		if(m_bNoClock)
			break;
		size.combined = DefSubclassProc(hwnd, message, wParam, lParam);
		if(size.part.width > size.part.height)
			size.part.width += (int16_t)(m_rcClock.right - m_clock_base_width);
		else
			size.part.height += (int16_t)(m_rcClock.bottom - m_clock_base_height);
		return size.combined;}
	case WM_NOTIFY:{
		LRESULT ret;
		NMHDR* nmh = (NMHDR*)lParam;
		RECT clock_rc;
		POINT pos;
		HWND child;
		if(nmh->code != PGN_CALCSIZE || m_bNoClock)
			break;
		ret = DefSubclassProc(hwnd, message, wParam, lParam);
		GetClientRect(gs_hwndClock, &clock_rc);
		MapWindowPoints(gs_hwndClock, hwnd, (POINT*)&clock_rc, 1);
		clock_rc.right = clock_rc.left + m_clock_base_width;
		clock_rc.bottom = clock_rc.top + m_clock_base_height;
		for(child=GetWindow(hwnd,GW_CHILD); child; child=GetWindow(child, GW_HWNDNEXT)) {
			pos.x = pos.y = 0;
			MapWindowPoints(child, hwnd, &pos, 1);
			if(pos.x >= clock_rc.right) {
				pos.x += m_rcClock.right - m_clock_base_width;
				SetWindowPos(child, 0, pos.x, pos.y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
			} else if(pos.y >= clock_rc.bottom) {
				pos.y += m_rcClock.bottom - m_clock_base_height;
				SetWindowPos(child, 0, pos.x, pos.y, 0, 0, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
			}
		}
		return ret;}
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}
/*------------------------------------------------
  subclass procedure of the clock
--------------------------------------------------*/
static LRESULT CALLBACK Window_Clock_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	(void)dwRefData;
	switch(message){
	case WM_DESTROY:
		RemoveWindowSubclass(gs_tray, Window_ClockTray_Hooked, 0);
		RemoveWindowSubclass(hwnd, Window_Clock_Hooked, uIdSubclass);
		DestroyClock();
		break;
	case WM_WINDOWPOSCHANGING:{
		WINDOWPOS* pwp=(WINDOWPOS*)lParam;
		if(m_bNoClock) break;
		if(!(pwp->flags&SWP_NOSIZE) && !(pwp->flags&SWP_NOMOVE)){
			m_clock_base_width = pwp->cx; // keep base size in sync (eg. on "short time" format or DPI change)
			m_clock_base_height = pwp->cy;
			if(pwp->x >= pwp->y){ // horizontal
				pwp->cx = m_rcClock.right;
				if(m_height){ // explorer auto-updates our height, but we use custom
					pwp->cy = m_rcClock.bottom;
				}
			}else{
				pwp->cy = m_rcClock.bottom;
				if(m_width){ // explorer auto-updates our width, but we use custom
					pwp->cx = m_rcClock.right;
				}
			}
		}
		return 0;}
	case WM_WINDOWPOSCHANGED:{
		WINDOWPOS* pwp=(WINDOWPOS*)lParam;
		if(m_bNoClock) break;
		if(!(pwp->flags&SWP_NOSIZE)){
			UpdateClockSize();
		}
		return 0;}
	case WM_DWMCOLORIZATIONCOLORCHANGED://forwarded by T-Clock itself
		api.On_DWMCOLORIZATIONCOLORCHANGED((unsigned)wParam, (BOOL)lParam);
		/* fall through */
	case WM_THEMECHANGED:
		if(message == WM_THEMECHANGED) {
			ReloadXPClockTheme(hwnd);
			FillClockBG();
			if(m_TipState) // draws on-top
				FillClockBGHover();
		}
		/* fall through */
	case WM_SYSCOLORCHANGE:
		ColorUpdate(m_basecolorFont,m_basecolorBG);
		InvalidateRect(hwnd,NULL,0);
		break;
	case WM_TIMECHANGE:
	case WM_USER+101: {
		HDC hdc;
		if(m_bNoClock) break;
		hdc = GetDC(hwnd);
		DrawClock(hdc);
		ReleaseDC(hwnd, hdc);
		return 0;}
	case WM_USER+102: {
		if(api.OS >= TOS_WIN10_1) { // emulated click for Win10.1, older OS would require the "click" on the notify area with the clocks coordinates (parent)
			if(hwnd != m_clock_active)
				return SendMessage(m_clock_active, message, wParam, lParam);
			DefSubclassProc(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(4,4));
			SetTimer(hwnd, TCLOCK_TIMER_ID_CLICK, 25, NULL);
			return 0;
		}
		break;}
	case WM_ERASEBKGND:
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc;
		if(m_bNoClock) break;
		hdc=BeginPaint(hwnd,&ps);
		DrawClock(hdc);
		EndPaint(hwnd,&ps);
//		InvalidateRect(hwnd,NULL,0); /// uncomment for debugging purpose, eg. does our drawing flicker
		return 0;}
	case WM_TIMER: // Windows uses timer ID 0 to update every minute (tries to stay ~1 sec. close to full minute)
		if(wParam == TCLOCK_TIMER_ID_CLICK) {
			KillTimer(hwnd, wParam);
			DefSubclassProc(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4,4));
			return 0;
		}
		if(m_bNoClock) break;
		return 0;
	/* start of shared code used by multi-clock winproc
	Use `gs_hwndClock` to refer to main clock, `hwnd` for triggering window*/
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
		if(!(wParam&WPARAM_SUBCLOCK))
			m_clock_active = hwnd;
		gs_hwndCalendar = NULL; // GetCalendar() falls back to this
		gs_hwndCalendar = api.GetCalendar();
		SetForegroundWindow(gs_hwndTClockMain); // set T-Clock to foreground so we can open menus, etc.
		if(m_BlinkState){
			m_BlinkState=BLINK_NONE;
			InvalidateRect(gs_hwndClock, NULL, 1);
			PostMessage(gs_hwndTClockMain,MAINM_BLINKOFF,0,0);
			return 0;
		}
		PostMessage(gs_hwndTClockMain, message, wParam, lParam);
		return 0;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		PostMessage(gs_hwndTClockMain, message, wParam, lParam);
		return 0;
//	case WM_CONTEXTMENU:
//		PostMessage(g_hwndTClockMain, message, wParam, lParam);
//		return 0;
	case WM_MOUSEMOVE:
		if(!m_TipState){
			TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT)};
			tme.dwFlags = TME_HOVER|TME_LEAVE;
			tme.hwndTrack = hwnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			m_TipState = TrackMouseEvent(&tme);
			m_col = m_col_hover;
			FillClockBGHover();
			InvalidateRect(gs_hwndClock, NULL, 0);
			SendMessage((wParam & WPARAM_SUBCLOCK ? GetParent(hwnd) : gs_taskbar), WM_NCHITTEST, 0, GetMessagePos());
		}
		return 0;
	case WM_MOUSEHOVER:
		m_TipState = 2;
		if(api.OS >= TOS_WIN10_1 || (api.OS < TOS_VISTA || api.GetInt(L"Tooltip",L"bCustom",0))){
			ShowTip();//show custom/emulated tooltip
		}else{
			SendMessage(gs_hwndClock, WM_USER+103,1,0);//show system tooltip
			if(hwnd != gs_hwndClock) {
				HWND tooltip = FindWindowExA(NULL, NULL, "ClockTooltipWindow", NULL);
				if(tooltip) {
					api.PositionWindow(tooltip, 0);
					// hook to prevent any non-authorized move
					// (Windows sometimes moves the tooltip for unknown reason)
					SetWindowSubclass(tooltip, Window_ClockTooltip_Hooked, 0, 0);
				}
			}
		}
		return 0;
	case WM_MOUSELEAVE:
		if(m_TipState){
			if(m_TipState == 2){
				if(api.OS >= TOS_WIN10_1 || (api.OS < TOS_VISTA || api.GetInt(L"Tooltip",L"bCustom",0)))
					PostMessage(m_TipHwnd, TTM_TRACKACTIVATE , FALSE, (LPARAM)&m_TipInfo);//hide custom tooltip
				else
					PostMessage(gs_hwndClock, WM_USER+103,0,0);//hide system tooltip
			}
			m_TipState = 0;
			m_col = m_col_normal;
			FillClockBG();
			InvalidateRect(gs_hwndClock,NULL,0);
		}
		return 0;
	/* end of multi-clock shared code */
	case WM_NCHITTEST: // original clock uses this message for context menu and hover, etc. and we need our own "handler"
//		return HTTRANSPARENT; // Windows' clock uses this
//		return HTCAPTION; // xD
		return DefWindowProc(hwnd, message, wParam, lParam);
	case WM_MOUSEACTIVATE:
		return MA_ACTIVATE;
	case WM_NOTIFY: {
		UINT code=((LPNMHDR)lParam)->code;
		if(code==TTN_NEEDTEXTA || code==TTN_NEEDTEXTW) {
			if(api.OS >= TOS_WIN10_1 && !api.GetInt(L"Tooltip",L"bCustom",0))
				return DefSubclassProc(hwnd, message, wParam, lParam);
			OnTooltipNeedText(code,lParam);
		}
		return 0;}
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDM_EXIT:
			EndClock();
			break;
		case IDM_SHUTDOWN: {// unload 'exchndl' if loaded
			HMODULE exchndl = GetModuleHandleA("exchndl");
			if(exchndl)
				FreeLibrary(exchndl);
			break;}
		}
		return 0;
	case CLOCKM_REFRESHCLOCK: { // refresh the clock
		m_TipState = 0;
		SubsDestroy();
		ReadFormatData(hwnd,0);
		ReadStyleData(hwnd,0); // also creates/updates clock
		MyDragDrop_Enable(m_droptarget,1); // enable/disable DropFiles on clock
		SubsCreate();
		// "try" to setup our timer on every "manual" clock-refresh
		if(m_bNoClock) { // kill our timer; setup Windows'
			KillTimer(hwnd, TCLOCK_TIMER_ID);
			SetTimer(hwnd, WIN_CLOCK_TIMER_ID, 10*1000, NULL);
		} else { // kill Windows' timer; setup ours
			KillTimer(hwnd, WIN_CLOCK_TIMER_ID);
			SetTimer(hwnd, TCLOCK_TIMER_ID, 1000, OnDrawTimer);
		}
		return 0;}
	case CLOCKM_REFRESHCLOCKPREVIEW: // refresh the clock
		wParam = 1;
		ReadStyleData(hwnd,1); // also creates/updates clock
		/* fall through */
	case CLOCKM_REFRESHCLOCKPREVIEWFORMAT: // refresh the clock
		if(message==CLOCKM_REFRESHCLOCKPREVIEWFORMAT)
			ReadFormatData(hwnd,1); // also creates/updates clock because of preview
		/* fall through */
	case CLOCKM_REFRESHTASKBAR:{ // refresh other elements than clock (somehow required to actually change the clock's size)
		int alpha = api.GetIntEx((wParam ? L"Preview" : L"Taskbar"), L"AlphaTaskbar", 0);
		int clear_taskbar = api.GetIntEx(L"Taskbar", L"ClearTaskbar", 0);
		SetLayeredTaskbar(hwnd, alpha, clear_taskbar);
		PostMessage(gs_taskbar, WM_SIZE, SIZE_RESTORED, 0);
		InvalidateRect(gs_taskbar, NULL, 1);
		return 0;}
	case CLOCKM_BLINK: // blink the clock
		if(wParam)
			m_BlinkState|=BLINK_HOUR;
		else
			m_BlinkState|=BLINK_ON;;
		InvalidateRect(hwnd, NULL, 1);
		return 0;
	case CLOCKM_BLINKOFF: // stop blinking
		if(m_BlinkState){
			m_BlinkState&=~BLINK_ON;
			InvalidateRect(hwnd,NULL,1);
			PostMessage(gs_hwndTClockMain,MAINM_BLINKOFF,0,0);
		}
		return 0;
	case CLOCKM_COPY: // copy format to clipboard
		OnCopy(hwnd, lParam);
		return 0;
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}
/*------------------------------------------------
  window procedure of the secondary taskbar clock(s)
--------------------------------------------------*/
static LRESULT CALLBACK Window_SecondaryClock_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch(message){
	case WM_NCCREATE:{
		return 1; }
	case WM_CREATE:{
		SetWindowLongPtr(hwnd, GWLP_ID, 303); // same ID as original clock
		RegisterDragDrop(hwnd, m_droptarget); // setup DropFiles on sub-clock
		return 0; }
	case WM_CLOSE:{
		RevokeDragDrop(hwnd); // kill DropFiles on sub-clock
		if(dwRefData) { // hooked clock window
			RemoveWindowSubclass(hwnd, Window_SecondaryClock_Hooked, uIdSubclass);
			if(wParam != 0xDEED)
				return DefSubclassProc(hwnd, message, wParam, lParam);
		} else { // custom created clock window
			DestroyWindow(hwnd);
		}
		return 0; }
	case WM_WINDOWPOSCHANGING:{
		return 0;}
	case WM_WINDOWPOSCHANGED:
		return 0;
	case WM_ERASEBKGND:
		return 0;
	case WM_PAINT:{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		BitBlt(ps.hdc, 0,0,m_rcClock.right,m_rcClock.bottom, m_hdcClock, 0,0, SRCCOPY);
		EndPaint(hwnd, &ps);
		return 0;}
	/// clock features
	case WM_USER+102: // emulated click for Win10.1 to open calendar
		if(dwRefData) {
			DefSubclassProc(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(4,4));
			SetTimer(hwnd, TCLOCK_TIMER_ID_CLICK, 25, NULL);
		}
		return 0;
	case WM_TIMER:
		if(wParam == TCLOCK_TIMER_ID_CLICK) {
			KillTimer(hwnd, wParam);
			DefSubclassProc(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4,4));
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_XBUTTONDBLCLK:
		SetFocus(hwnd);
		/* fall through */
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		m_clock_active = hwnd;
		/* fall through */
	case WM_MOUSEMOVE:
	case WM_MOUSEHOVER:
	case WM_MOUSELEAVE:
		return Window_Clock_Hooked(hwnd, message, wParam|WPARAM_SUBCLOCK, lParam, 0, 0); // quite dangerous call
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
static LRESULT CALLBACK Window_SecondaryClock(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return Window_SecondaryClock_Hooked(hwnd, message, wParam, lParam, 0, 0);
}
/*------------------------------------------------
  subclass procedure of the secondary taskbar "worker" area (used to resize and allow own clock)
--------------------------------------------------*/
static LRESULT CALLBACK Window_SecondaryTaskbarWorker_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	MultiClock* self = (MultiClock*)dwRefData;
	switch(message) {
	case WM_DESTROY:
		RemoveWindowSubclass(hwnd, Window_SecondaryTaskbarWorker_Hooked, uIdSubclass);
		break;
	case WM_WINDOWPOSCHANGING:{
		int x,y,cx,cy;
		int offset_x, offset_y;
		WINDOWPOS* pwp = (WINDOWPOS*)lParam;
		if(m_bNoClock)
			break;
		offset_x = (m_rcClock.right + api.desktop_button_size) - self->clock_base_width;
		offset_y = (m_rcClock.bottom + api.desktop_button_size) - self->clock_base_height;
//		MessageBox(0,"WM_WINDOWPOSCHANGING",__FUNCTION__,0);
		if(!(pwp->flags&SWP_NOSIZE)){
			DefSubclassProc(hwnd, message, wParam, lParam); // adjusts left margin? (so changes size and pos)
			self->workerRECT.right = pwp->cx;
			self->workerRECT.bottom = pwp->cy;
			if(pwp->cx > pwp->cy){ // horizontal
				pwp->cx = pwp->cx - offset_x;
			}else{
				pwp->cy = pwp->cy - offset_y;
			}
		}
		if(!(pwp->flags&SWP_NOMOVE)){
			self->workerRECT.left = pwp->x;
			self->workerRECT.top = pwp->y;
		}
		cx = m_rcClock.right;
		cy = m_rcClock.bottom;
		if(self->workerRECT.right > self->workerRECT.bottom){ // horizontal
			x = self->workerRECT.left + self->workerRECT.right - offset_x;
			y = self->workerRECT.top + m_BORDER_MARGIN;
		}else{
			x = self->workerRECT.left + m_BORDER_MARGIN;
			y = self->workerRECT.top + self->workerRECT.bottom - offset_y;
		}
		SetWindowPos(self->clock, 0, x, y, cx, cy, 0);
		return 0;}
	case WM_WINDOWPOSCHANGED:{
		break;}
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}
//==========================================================================
//---------------------------------+++--> Retreive T-Clock's format settings:
void ReadFormatData(HWND hwnd, int preview)   //---------------------+++-->
{
	const wchar_t* section = (preview ? L"Preview" : L"Format");
	DWORD dwInfoFormat;
	m_bNoClock = (char)api.GetInt(section, L"NoClockCustomize", 0);
	// read format
	if(!api.GetStr(section,L"Format",m_format,_countof(m_format),L"\"config error\"") || !m_format[0]) {
		m_bNoClock = 1;
	}
	g_bHourZero = (char)api.GetInt(section, L"HourZero", 0);
	// parse format
	dwInfoFormat = FindFormat(m_format);
	m_bDispSecond = (dwInfoFormat&FORMAT_SECOND)? 1:0;
	m_nDispBeat = dwInfoFormat & (FORMAT_BEAT1 | FORMAT_BEAT2);
//	if(!m_bTimer) m_bTimer = (char)SetTimer(hwnd, TCLOCK_TIMER_ID, 1000, OnDrawTimer);
	GetLocalTime(&m_LastTime);
	InitFormat(section, &m_LastTime); // format.c
	// update clock if preview
	if(preview){
		UpdateClock(hwnd,NULL);
		InvalidateRect(hwnd, NULL, 0);
		InvalidateRect(gs_tray, NULL, 1);
	}
}
//=========================================================================
//---------------------------------+++--> Retreive T-Clock's style settings:
void ReadStyleData(HWND hwnd, int preview)   //---------------------+++-->
{
	const wchar_t* section = (preview ? L"Preview" : L"Clock");
	wchar_t fontname[80];
	LONG weight, italic;
	int fontsize, angle;
	BYTE fontquality;
	HFONT hFon;
	/// read style
	m_basecolorFont = api.GetInt(section, L"ForeColor", TCOLOR(TCOLOR_DEFAULT));
	m_basecolorBG = api.GetInt(section, L"BackColor", TCOLOR(TCOLOR_DEFAULT));
	ColorUpdate(m_basecolorFont,m_basecolorBG);
	angle=api.GetInt(section, L"Angle", 0)%360;
	if(angle<0) angle+=360;
	m_radian=(double)angle*3.14159265358979323/180.;// ye π doesn't need to be that long :P
	dlineheight = api.GetInt(section, L"LineHeight", 0);
	m_height = api.GetInt(section, L"ClockHeight", 0);
	m_width = api.GetInt(section, L"ClockWidth", 0);
	dhpos = api.GetInt(section, L"HorizPos", 0);
	dvpos = api.GetInt(section, L"VertPos", 0);
	/// font
	api.GetStr(section, L"Font", fontname, _countof(fontname), L"Arial");
	fontsize = api.GetInt(section, L"FontSize", 9);
	if(fontsize>100 || fontsize<=0) fontsize=9;
	italic = api.GetInt(section, L"Italic", 0);
	weight = api.GetInt(section, L"Bold", 0);
	switch(weight){
	case 0: break;
	case 1: weight=FW_BOLD; break;
	default:
		weight=FW_SEMIBOLD;
	}
	fontquality = (BYTE)api.GetInt(section, L"FontQuality", CLEARTYPE_QUALITY);
	hFon = CreateMyFont(fontname, fontsize, weight, italic, angle*10, fontquality);
	/// misc
	m_bMultimon = api.GetInt(L"Desktop", L"Multimon", 1);
	/// create/update clock
	UpdateClock(hwnd, hFon);
	InvalidateRect(hwnd, NULL, 0);
	InvalidateRect(gs_tray, NULL, 1);
}

/*------------------------------------------------
   get date/time and beat to display
--------------------------------------------------*/
void GetDisplayTime(SYSTEMTIME* pt, int* beat100)
{
	if(beat100) {
		GetSystemTime(pt);
		if(++pt->wHour>23) // beat time is UTC+1
			pt->wHour=0;
		*beat100 = pt->wHour*3600 + pt->wMinute*60 + pt->wSecond;
		*beat100 = (*beat100 * 1000) / 864;
	}
	GetLocalTime(pt);
}

/*--------------------------------------------------
------------------------------------------- WM_TIMER
--------------------------------------------------*/
void CALLBACK OnDrawTimer(HWND hwnd, unsigned uMsg, uintptr_t idEvent, unsigned long dwTime)
{
	SYSTEMTIME t;
	int beat100 = 0;
	int bRedraw;
	static char s_calibration = 0;
	static int s_lastbeat = -1;
	
	(void)uMsg; (void)idEvent; (void)dwTime;
	
	GetDisplayTime(&t, m_nDispBeat ? &beat100 : NULL);
	
	if(t.wMilliseconds >= 128) {
		s_calibration = 1;
		SetTimer(hwnd, TCLOCK_TIMER_ID, 1001 - t.wMilliseconds, OnDrawTimer);
	} else if(s_calibration) {
		s_calibration = 0;
		SetTimer(hwnd, TCLOCK_TIMER_ID, 1000, OnDrawTimer);
	}
	
	bRedraw = 0;
	if(m_BlinkState){
		if(m_LastTime.wMinute && m_BlinkState&BLINK_HOUR)
			m_BlinkState^=BLINK_HOUR; // disable hourly blink
		bRedraw = 1;
		/* --+++--> This Will Disable the AutoHide...
				abd.cbSize = sizeof(APPBARDATA);
				abd.hWnd = FindWindow("Shell_TrayWnd","");
				abd.lParam = ABS_ALWAYSONTOP;
				SHAppBarMessage(ABM_SETSTATE, &abd); ...Which Ain't What We're After! <+-*/
		
	}
	
	if(m_bDispSecond || m_LastTime.wMinute!=t.wMinute)
		bRedraw = 1;
	else if(m_nDispBeat){
		if(!(m_nDispBeat&FORMAT_BEAT2))
			beat100 /= 100;
		bRedraw |= (s_lastbeat != beat100);
		s_lastbeat = beat100;
	}
	
	if(m_LastTime.wDay!=t.wDay || m_LastTime.wMonth!=t.wMonth || m_LastTime.wYear!=t.wYear) {
		InitFormat(L"Format", &t); // format.c
		UpdateClock(hwnd,NULL); // resize clock
		SendMessage(hwnd,CLOCKM_REFRESHTASKBAR,0,0); // reposition clock (inform taskbar about new size)
		bRedraw = 0;
	}
	
	memcpy(&m_LastTime, &t, sizeof(t));
	
	if(bRedraw)
		InvalidateRect(hwnd,NULL,0);
}

void FillClockBG()
{
	RGBQUAD* color;
	BYTE alpha;
	union{
		BGRQUAD quad;
		COLORREF ref;
	} col;
	if(!m_colorBG_start) return;
	if(m_colBG.ref==TCOLOR(TCOLOR_DEFAULT)){
		if(IsXPThemeActive()){
			DrawXPClockBackground(gs_hwndClock,m_hdcClockBG,&m_rcClock);
			return;
		}
		col.ref=GetSysColor(COLOR_3DFACE);
	}else
		col.ref=m_colBG.ref;
	// fill with color (Win8 uses ~0x37 alpha)
//	col.ref=0x37000000;//Win8 like
	alpha=255-col.quad.rgbReserved;
	col.ref=alpha<<24|(col.quad.rgbBlue*alpha/255)|(col.quad.rgbGreen*alpha/255)<<8|(col.quad.rgbRed*alpha/255)<<16;
	for(color=m_colorBG_start; color<m_colorBG_end; ++color)
		*(unsigned*)color=col.ref;
}
void FillClockBGHover()
{
	if(!m_colorBG_start) return;
	if(IsXPThemeActive()) {
		DrawXPClockHover(gs_hwndClock,m_hdcClockBG,&m_rcClock);
	}
}
void CalculateClockTextPosition(){
	double cos_=cos(m_radian);
	double sin_=sin(m_radian);
	int textwidth=(int)(sin_*(m_textheight+m_leading));
	int textheight=(int)(cos_*(m_textheight+m_leading));
	/// use width / height based on angle.
	GetClientRect(gs_taskbar, &m_rcClock);
	m_bHorizontalTaskbar=m_rcClock.right>m_rcClock.bottom;
	if(m_bHorizontalTaskbar){
		m_rcClock.bottom-=m_BORDER_MARGIN;//2px top
		if(m_height){//user-defined height
			m_rcClock.bottom=m_textheight;
		}else
			textheight+=m_BORDER_MARGIN; // ignore vertical margin on center calculation
		m_rcClock.right=abs((int)(cos_*m_textwidth))+abs((int)(sin_*m_textheight)) + m_textpadding;
	}else{
		m_rcClock.right-=m_BORDER_MARGIN;//2px top
		if(m_width){//user-defined height
			m_rcClock.right=m_textwidth;
		}else
			textwidth+=m_BORDER_MARGIN; // ignore horizontal margin on center calculation
		m_rcClock.bottom=abs((int)(cos_*m_textheight))+abs((int)(sin_*m_textwidth)) + m_textpadding;
	}
	m_rcClock.right+=m_width;
	m_rcClock.bottom+=m_height;
	if(m_rcClock.right<5) m_rcClock.right=5;
	if(m_rcClock.bottom<5) m_rcClock.bottom=5;
	/// position
	m_vertpos=(m_rcClock.bottom-textheight+(int)(cos_*dlineheight))/2 + dvpos;
	m_horizpos=(m_rcClock.right-textwidth+(int)(sin_*dlineheight))/2 + dhpos;
}
void CalculateClockTextSize(){
	SYSTEMTIME time;
	int beat100=0;
	wchar_t buf[FORMAT_MAX_SIZE], *pos, *str;
	unsigned len;
	SIZE sz;
	TEXTMETRIC tm;
	/// fake time/date to use up most possible space for (static) size calculation
//	GetDisplayTime(&time, m_nDispBeat ? &beat100 : NULL);
	time.wYear = 2222;
	time.wMonth = 12;
	time.wDayOfWeek = 3;
	time.wDay = 22;
	time.wHour = 10;
	time.wMinute = time.wSecond = 22;
	time.wMilliseconds = 666;
	beat100 = 666;
	len = MakeFormat(buf, m_format, &time, beat100);
	/// calc size
	GetTextMetrics(m_hdcClock,&tm);
	m_textpadding = tm.tmAveCharWidth*2;
	m_textheight = m_textwidth=0;
	m_leading = tm.tmInternalLeading;
	m_vertfeed = tm.tmHeight-m_leading+dlineheight;
	for(pos=buf; *pos; ){
		for(str=pos; *pos&&*pos!='\n'; ++pos);
		len = (unsigned)(pos-str);
		if(*pos=='\n') *pos++ = '\0';
		/// width
		if(GetTextExtentPoint32(m_hdcClock,str,len,&sz) == 0)
			sz.cx = (len * tm.tmAveCharWidth);
		if(m_textwidth < sz.cx)
			m_textwidth = sz.cx;
		///height
		m_textheight+=m_vertfeed;
	}
	m_horizfeed = (int)(sin(m_radian) * m_vertfeed);
	m_vertfeed = (int)(cos(m_radian) * m_vertfeed);
	CalculateClockTextPosition();
}
int DestroyClock()
{
	if(m_oldfnt){
		DeleteObject(SelectObject(m_hdcClock,m_oldfnt));
		m_oldfnt=NULL;
	}
	if(m_oldbmpB){
		DeleteObject(SelectObject(m_hdcClockBG,m_oldbmpB));
		m_oldbmpB=NULL;
		m_colorBG_start=NULL;
	}
	if(m_oldbmp){
		DeleteObject(SelectObject(m_hdcClock,m_oldbmp));
		m_oldbmp=NULL;
		m_color_start=NULL;
	}
	if(m_hdcClock){
		DeleteDC(m_hdcClockBG);
		DeleteDC(m_hdcClock);
		m_hdcClock=NULL;
	}
	memset(&m_rcClock,0,sizeof(m_rcClock));
	return 0;
}
int UpdateClock(HWND hwnd, HFONT fnt)
{
	if(m_bNoClock)
		return 0;
	if(!m_hdcClock){
		HDC hdc=GetDC(NULL);
		m_hdcClock=CreateCompatibleDC(hdc);
		m_hdcClockBG=CreateCompatibleDC(hdc);
		ReleaseDC(NULL,hdc);
		SetBkMode(m_hdcClock,TRANSPARENT);
		SetTextAlign(m_hdcClock,TA_CENTER|TA_TOP);
		SetTextColor(m_hdcClock,0x00000000);
		// select a 1x1 placeholder bitmap for proper clock text size calculation
		m_oldbmp = SelectObject(m_hdcClock, CreateBitmap(1,1,1,32,NULL));
	}
	if(fnt){
		if(!m_oldfnt)
			m_oldfnt=SelectObject(m_hdcClock,fnt);
		else
			DeleteObject(SelectObject(m_hdcClock,fnt));
	}
	CalculateClockTextSize();
	// height-only change is buggy without this +1
	SetWindowPos(hwnd, HWND_TOP, 0,0, m_rcClock.right+1, m_rcClock.bottom, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE|SWP_NOCOPYBITS);
	return 1;
}
int UpdateClockSize()
{
	static BITMAPINFO bmi={{sizeof(BITMAPINFO),0,0,1,32,BI_RGB},};
	HBITMAP hbm;
	if(!m_oldfnt)
		return 0;
	bmi.bmiHeader.biWidth=m_rcClock.right;
	bmi.bmiHeader.biHeight=m_rcClock.bottom;
	/// create/select text bitmap
	hbm=CreateDIBSection(m_hdcClock,&bmi,DIB_RGB_COLORS,(void**)&m_color_start,NULL,0);
	if(!hbm) return DestroyClock();
	m_color_end=m_color_start+(m_rcClock.right*m_rcClock.bottom);
	if(!m_oldbmp) m_oldbmp=SelectObject(m_hdcClock,hbm);
	else DeleteObject(SelectObject(m_hdcClock,hbm));
	/// create/select background bitmap
	hbm=CreateDIBSection(m_hdcClockBG,&bmi,DIB_RGB_COLORS,(void**)&m_colorBG_start,NULL,0);
	if(!hbm) return DestroyClock();
	m_colorBG_end=m_colorBG_start+(m_rcClock.right*m_rcClock.bottom);
	if(!m_oldbmpB) m_oldbmpB=SelectObject(m_hdcClockBG,hbm);
	else DeleteObject(SelectObject(m_hdcClockBG,hbm));
	FillClockBG();
	CalculateClockTextPosition();
	SubsSendResize();
	return 1;
}


/*------------------------------------------------
  draw the clock
--------------------------------------------------*/
void DrawClock(HDC hdc)
{
	SYSTEMTIME t;
	int beat100=0;
	if(!m_color_start)
		return;
	GetDisplayTime(&t, m_nDispBeat ? &beat100 : NULL);
	DrawClockSub(hdc, &t, beat100);
}
void DrawClockSub(HDC hdc, SYSTEMTIME* pt, int beat100)
{
	RGBQUAD* color,* back;
	wchar_t buf[FORMAT_MAX_SIZE],* pos,* str;
	unsigned len;
	int vpos,hpos;
	const unsigned opacity=255-m_col.quad.rgbReserved;
	for(color=m_color_start; color<m_color_end; ++color)
		*(unsigned*)color=0xFFFFFFFF;
	len=MakeFormat(buf, m_format, pt, beat100);
	
	vpos=m_vertpos;
	hpos=m_horizpos;
	for(pos=buf; *pos; ){
		for(str=pos; *pos&&*pos!='\n'; ++pos);
		len=(unsigned)(pos-str);
		if(*pos=='\n') {*pos++='\0';}
		ExtTextOut(m_hdcClock, hpos, vpos, 0, NULL, str, len, NULL);
		vpos+=m_vertfeed;
		hpos+=m_horizfeed;
	}
	GdiFlush();//flush before bit manipulation
	for(color=m_color_start,back=m_colorBG_start; color<m_color_end; ++color,++back){
		if(!color->rgbReserved){
			unsigned channel;
			unsigned trans=(255-(color->rgbGreen+color->rgbBlue+color->rgbRed)/3)*opacity/255;
			unsigned bgtrans=255-trans;
			channel=m_col.quad.rgbBlue*trans/0xE7 + back->rgbBlue*bgtrans/255;
			color->rgbBlue=(channel>255?255:(BYTE)channel);
			channel=m_col.quad.rgbGreen*trans/0xE7 + back->rgbGreen*bgtrans/255;
			color->rgbGreen=(channel>255?255:(BYTE)channel);
			channel=m_col.quad.rgbRed*trans/0xE7 + back->rgbRed*bgtrans/255;
			color->rgbRed=(channel>255?255:(BYTE)channel);
			
			channel=back->rgbReserved+trans;
			color->rgbReserved=(channel>255?255:(BYTE)channel);
		}else{
			*(unsigned*)color=*(unsigned*)back;
		}
	}
	if(!m_BlinkState || pt->wSecond%2){
//		BLENDFUNCTION fnc={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
//		GdiAlphaBlend(hdc,0,0,g_rcClock.right,g_rcClock.bottom,g_hdcClock,0,0,g_rcClock.right,g_rcClock.bottom,fnc);
//		BitBlt(hdc,0,0,g_rcClock.right,g_rcClock.bottom,g_hdcClock,0,0,SRCPAINT);
		BitBlt(hdc,0,0,m_rcClock.right,m_rcClock.bottom,m_hdcClock,0,0,SRCCOPY);
		if(m_multiClocks){
			vpos = 0;
			do{
				hdc = GetDC(m_multiClock[vpos].clock);
				BitBlt(hdc, 0,0,m_rcClock.right,m_rcClock.bottom, m_hdcClock, 0,0, SRCCOPY);
				ReleaseDC(m_multiClock[vpos].clock, hdc);
			}while(++vpos < m_multiClocks);
		}
	}else{
		BitBlt(hdc,0,0,m_rcClock.right,m_rcClock.bottom,m_hdcClock,0,0,NOTSRCCOPY);
		if(m_multiClocks){
			vpos = 0;
			do{
				hdc = GetDC(m_multiClock[vpos].clock);
				BitBlt(hdc, 0,0,m_rcClock.right,m_rcClock.bottom, m_hdcClock, 0,0, NOTSRCCOPY);
				ReleaseDC(m_multiClock[vpos].clock, hdc);
			}while(++vpos < m_multiClocks);
		}
	}
}

void OnTooltipNeedText(UINT code, LPARAM lParam)
{
	SYSTEMTIME t;
	int beat100;
	wchar_t fmt[256], str[FORMAT_MAX_SIZE];
	
	api.GetStr(L"Tooltip", L"Tooltip", fmt, _countof(fmt), L"");
	if(!*fmt)
		memcpy(fmt, TC_TOOLTIP, sizeof(TC_TOOLTIP));
	
	GetDisplayTime(&t, &beat100);
	MakeFormat(str, fmt, &t, beat100);
	if(code == TTN_NEEDTEXTA){
		NMTTDISPINFOA* tooltip = (NMTTDISPINFOA*)lParam;
		WideCharToMultiByte(CP_ACP, 0, str, -1, tooltip->szText, _countof(tooltip->szText), NULL, NULL);
	} else {
		NMTTDISPINFOW* tooltip = (NMTTDISPINFOW*)lParam;
		wcsncpy_s(tooltip->szText, _countof(tooltip->szText), str, _TRUNCATE);
	}
}

/*--------------------------------------------------
------------------- copy date/time text to clipboard
--------------------------------------------------*/
void OnCopy(HWND hwnd, LPARAM lParam)
{
	SYSTEMTIME t;	HGLOBAL hg;
	wchar_t entry[7], fmt[MAX_PATH], s[FORMAT_MAX_SIZE], *pbuf;
	int beat100;
	size_t size;
	
	GetDisplayTime(&t, &beat100);
	entry[0] = '0' + (wchar_t)LOWORD(lParam);
	entry[1] = '0' + (wchar_t)HIWORD(lParam);
	wcscpy(entry+2, L"Clip");
	api.GetStr(REG_MOUSE, entry, fmt, _countof(fmt), L"");
	if(!*fmt)
		wcscpy(fmt, m_format);
	
	MakeFormat(s, fmt, &t, beat100);
	size = (wcslen(s) + 1) * sizeof(s[0]);
	
	if(!OpenClipboard(hwnd))
		return;
	EmptyClipboard();
	hg = GlobalAlloc(GMEM_DDESHARE, size);
	if(hg) {
		pbuf = (wchar_t*)GlobalLock(hg);
		memcpy(pbuf, s, size);
		GlobalUnlock(hg);
		SetClipboardData(CF_UNICODETEXT, hg);
	}
	CloseClipboard();
}
