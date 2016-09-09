/*-------------------------------------------
  propsheet.c - Kazubon 1997-2001
  show "Options for T-Clock"
---------------------------------------------*/
// Modified by Stoic Joker: Monday, 03/22/2010 @ 7:32:29pm
#include "tclock.h"

#define IDC_TAB					0x3020
#define ID_APPLY_NOW			0x3021
#define ID_WIZBACK				0x3023
#define ID_WIZNEXT				0x3024
#define ID_WIZFINISH			0x3025

static int CALLBACK PropSheetProc(HWND hDlg, UINT uMsg, LPARAM  lParam);
static LRESULT CALLBACK Window_PropertySheet_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// dialog procedures of each page
INT_PTR CALLBACK Page_About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Alarm(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Mouse(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Color(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Quicky(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Format(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_HotKey(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Page_Misc(HWND, UINT, WPARAM, LPARAM);

static WNDPROC m_oldWndProc; // to save default window procedure
static int m_startpage = 0; // page to open first

char g_bApplyTaskbar = 0;
char g_bApplyClock = 0;
//char g_bApplyClear = 0;

//================================================================================================
//----------------------------//-----------------------+++--> Show the (Tab Dialog) Property Sheet:
void MyPropertySheet(int page)   //---------------------------------------------------------+++-->
{
	const DLGPROC PageProc[]={
		Page_About, Page_Alarm,
		Page_Color, Page_Format, Page_Mouse,
		Page_Quicky,
		Page_HotKey, Page_Misc,
	};
	if(!g_hwndSheet || (g_hwndSheet != (HWND)(intptr_t)1 && !IsWindow(g_hwndSheet))) {
		#define PROPERTY_NUM _countof(PageProc)
		PROPSHEETPAGE psp[PROPERTY_NUM] = {{0}};
		PROPSHEETHEADER psh = {sizeof(PROPSHEETHEADER)};
		int i;
		// Allocate Clean Memory for Each Page
		for(i=0; i<PROPERTY_NUM; ++i) {
			psp[i].dwSize = sizeof(PROPSHEETPAGE);
			psp[i].hInstance = g_instance;
			psp[i].pfnDlgProc = PageProc[i];
			psp[i].pszTemplate = MAKEINTRESOURCE(PROPERTY_BASE+i);
		}
		
		// set data of property sheet
		psh.dwFlags = PSH_USEHICON | PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_USECALLBACK | PSH_NOCONTEXTHELP;
		psh.hInstance = g_instance;
		psh.hIcon = g_hIconTClock;
		psh.pszCaption = L"T-Clock Options";
		psh.nPages = PROPERTY_NUM;
		psh.nStartPage = (page==-1 ? m_startpage : page);
		psh.ppsp = psp;
		psh.pfnCallback = PropSheetProc;
		// show it !
		g_hwndSheet = (HWND)(intptr_t)1;
		g_hwndSheet = (HWND)PropertySheet(&psh);
	}
	api.PositionWindow(g_hwndSheet,21);
	SetForegroundWindow(g_hwndSheet);
}
//================================================================================================
//--------------------------------------------------------+++--> Property Sheet Callback Procedure:
int CALLBACK PropSheetProc(HWND hDlg, UINT uMsg, LPARAM  lParam)   //-----------------------+++-->
{
	(void)lParam;
	if(uMsg == PSCB_INITIALIZED) {
		// subclass the window
		m_oldWndProc = SubclassWindow(hDlg, Window_PropertySheet_Hooked);
	}
	return 0;
}
/*--------------------------------------------------------
   window procedure of subclassed property sheet
---------------------------------------------------------*/
LRESULT CALLBACK Window_PropertySheet_Hooked(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message){
	case WM_DESTROY:
		g_hwndSheet = NULL;
		m_startpage=(int)SendMessage((HWND)SendMessage(hwnd,PSM_GETTABCONTROL,0,0),TCM_GETCURSEL,0,0);
		if(m_startpage<0) m_startpage = 0;
		if(g_bApplyClock) {
			g_bApplyClock = 0;
			PostMessage(g_hwndClock, CLOCKM_REFRESHCLOCK, 0, 0);
		}
		if(g_bApplyTaskbar) {
			g_bApplyTaskbar = 0;
			PostMessage(g_hwndClock, CLOCKM_REFRESHTASKBAR, 0, 0);
		}
		if(g_hDlgTimer && IsWindow(g_hDlgTimer))
			SendMessage(g_hDlgTimer,WM_CLOSE,0,0);
		#ifndef _DEBUG
		EmptyWorkingSet(GetCurrentProcess());
		#endif
		break;
	case WM_ACTIVATE:
		WM_ActivateTopmost(hwnd, wParam, lParam);
		break;
	case WM_COMMAND:{
		LRESULT ret=CallWindowProc(m_oldWndProc, hwnd, message, wParam, lParam);
		switch(LOWORD(wParam)){
		case ID_APPLY_NOW:
			/// apply settings if applicable
			if(!IsWindowEnabled((HWND)lParam)){ // ID_APPLY_NOW disabled, settings applied
				if(g_bApplyClock) {
					g_bApplyClock = 0;
					PostMessage(g_hwndClock, CLOCKM_REFRESHCLOCK, 0, 0);
				}
				if(g_bApplyTaskbar) {
					g_bApplyTaskbar = 0;
					PostMessage(g_hwndClock, CLOCKM_REFRESHTASKBAR, 0, 0);
				}
			}
			break;
		case IDOK:
		case IDCANCEL:
			/// close property sheet (normally we'll have to check for !PropSheet_GetCurrentPageHwnd() and do DestroyWindow() for modeless sheets
			DestroyWindow(hwnd);
			break;
		}
		return ret;}
	case WM_SYSCOMMAND:// close by "x" button
		if((wParam & 0xfff0) == SC_CLOSE)
			PostMessage(hwnd, WM_COMMAND, IDCANCEL, 0); // WM_CLOSE also cancels
		break;
	}
	return CallWindowProc(m_oldWndProc, hwnd, message, wParam, lParam);
}

/*------------------------------------------------
   select file
--------------------------------------------------*/
BOOL SelectMyFile(HWND hDlg, const wchar_t* filter, DWORD filter_index, const wchar_t* deffile, wchar_t retfile[MAX_PATH])
{
	wchar_t initdir[MAX_PATH];
	OPENFILENAME ofn = {sizeof(ofn)};
	
	memcpy(initdir, api.root, api.root_size);
	if(deffile && deffile[0]) {
		WIN32_FIND_DATA fd;
		HANDLE hfind;
		hfind = FindFirstFile(deffile, &fd);
		if(hfind != INVALID_HANDLE_VALUE) {
			FindClose(hfind);
			wcsncpy_s(initdir, _countof(initdir), deffile, _TRUNCATE);
			del_title(initdir);
		}
	}
	
	retfile[0] = '\0';
	ofn.hwndOwner = hDlg;
	ofn.hInstance = g_instance;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = filter_index;
	ofn.lpstrFile = retfile;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = initdir;
	ofn.Flags = OFN_HIDEREADONLY | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NODEREFERENCELINKS;
	
	return GetOpenFileName(&ofn);
}
