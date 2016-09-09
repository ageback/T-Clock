/*-------------------------------------------
  newapi.c - Kazubon 1999
  GradientFill and Layerd Window
---------------------------------------------*/
#include "../common/globals.h"
#include <uxtheme.h>
#include "../common/utl.h"

#include "newapi.h"

int IsWow64(){
	int ret=0;
	#ifndef _WIN64
	typedef BOOL (WINAPI *IsWow64Process_t)(HANDLE hProcess,BOOL* iswow64);
	IsWow64Process_t pIsWow64Process=(IsWow64Process_t)GetProcAddress(GetModuleHandleA("kernel32"),"IsWow64Process");
	if(pIsWow64Process){
		pIsWow64Process(GetCurrentProcess(),&ret);
	}
	#endif // _WIN64
	return ret;
}

static HMODULE hmodUxTheme = NULL;

/// UxTheme macros
#define THEME_FUNC_RETRIEVE(name) \
	p##name = (p##name##_t)GetProcAddress(hmodUxTheme, #name);\
	if(!p##name){\
		FreeLibrary(hmodUxTheme); hmodUxTheme = NULL;\
		return;\
	}
#define THEME_FUNC_RELEASE(name) \
	p##name = NULL
#define THEME_FUNC_CHECK(name,ret) \
	if(!p##name){\
		InitDrawTheme();\
		if(!p##name) return ret;\
	}
#define THEME_FUNC_CHECK_THEME(name,hwnd,ret) \
	THEME_FUNC_CHECK(name,ret)\
	if(!hClockTheme){\
		hClockTheme = pOpenThemeData(hwnd,(m_theme_version>=TV_10 ? VSCLASS_TASKBAND2 : VSCLASS_CLOCK));\
		m_theme_clock_part = (m_theme_version>=TV_10 ? 5 : CLP_TIME);\
		if(!hClockTheme) return ret;\
	}
#define THEME_FUNC_DEFINE(name,ret,params) \
	typedef ret (WINAPI* p##name##_t)params;\
	p##name##_t p##name=NULL
/// UxTheme defines
static HTHEME hClockTheme = NULL;
THEME_FUNC_DEFINE(CloseThemeData,HRESULT,(HTHEME hTheme));
THEME_FUNC_DEFINE(OpenThemeData,HTHEME,(HWND hwnd, LPCWSTR pszClassList));

THEME_FUNC_DEFINE(IsThemeActive,BOOL,());
THEME_FUNC_DEFINE(SetWindowTheme,HRESULT,(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList));
THEME_FUNC_DEFINE(GetThemeColor,HRESULT,(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF* pColor));
//THEME_FUNC_DEFINE(IsThemePartDefined,BOOL,(HTHEME hTheme, int iPartId, int iStateId));
//THEME_FUNC_DEFINE(IsThemeBackgroundPartiallyTransparent,BOOL,(HTHEME hTheme, int iPartId, int iStateId));
THEME_FUNC_DEFINE(DrawThemeBackground,HRESULT,(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect));
THEME_FUNC_DEFINE(DrawThemeParentBackground,HRESULT,(HWND hwnd, HDC hdc, RECT* prc));

static struct {
	DWORD flags;
	COLORREF color;
	BYTE alpha;
	char had_style;
} m_layered = {0, 0, 0, -1};
static uint16_t m_theme_version = 0;
static char m_theme_clock_part = CLP_TIME;
#define TV_2000 0x0500
#define TV_XP 0x0501
#define TV_XP_64 0x0502
#define TV_Vista 0x0600
#define TV_7 0x0601
#define TV_8 0x0602
#define TV_8_1 0x0603
#define TV_10_beta 0x0604
#define TV_10 0x0a00

static void RefreshRebar(HWND hwndBar);

void EndNewAPI(HWND hwndClock) {
	if(hwndClock && m_layered.had_style != -1) {
		HWND hwnd = GetParent(GetParent(hwndClock));
		if(!m_layered.had_style) {
			LONG_PTR exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
			if(exstyle & WS_EX_LAYERED) {
				exstyle &= ~WS_EX_LAYERED;
				SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
				RefreshRebar(hwnd);
			}
		} else {
			SetLayeredWindowAttributes(hwnd, m_layered.color, m_layered.alpha, m_layered.flags);
		}
	}
	
	/// DrawTheme
	THEME_FUNC_RELEASE(DrawThemeParentBackground);
	THEME_FUNC_RELEASE(DrawThemeBackground);
//	THEME_FUNC_RELEASE(IsThemeBackgroundPartiallyTransparent);
//	THEME_FUNC_RELEASE(IsThemePartDefined);
	THEME_FUNC_RELEASE(GetThemeColor);
	THEME_FUNC_RELEASE(IsThemeActive);
	THEME_FUNC_RELEASE(SetWindowTheme);
	THEME_FUNC_RELEASE(OpenThemeData);
	if(hClockTheme){
		pCloseThemeData(hClockTheme); hClockTheme=NULL;
	}
	THEME_FUNC_RELEASE(CloseThemeData);
	if(hmodUxTheme) FreeLibrary(hmodUxTheme);
	hmodUxTheme = NULL;
}
/*
void GradientFillClock(HDC hdc, RECT* prc, COLORREF col1, COLORREF col2) {
	TRIVERTEX vert[2];
	GRADIENT_RECT gRect;

	vert[0].x      = prc->left;
	vert[0].y      = prc->top;
	vert[0].Red    = (COLOR16)GetRValue(col1) * 256;
	vert[0].Green  = (COLOR16)GetGValue(col1) * 256;
	vert[0].Blue   = (COLOR16)GetBValue(col1) * 256;
	vert[0].Alpha  = 0x0000;
	vert[1].x      = prc->right;
	vert[1].y      = prc->bottom;
	vert[1].Red    = (COLOR16)GetRValue(col2) * 256;
	vert[1].Green  = (COLOR16)GetGValue(col2) * 256;
	vert[1].Blue   = (COLOR16)GetBValue(col2) * 256;
	vert[1].Alpha  = 0x0000;
	gRect.UpperLeft  = 0;
	gRect.LowerRight = 1;

	GdiGradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
}
*/
void SetLayeredTaskbar(HWND hwndClock, int alpha, int clear_taskbar)
{
	LONG_PTR exstyle;
	HWND hwnd;
	
	alpha = 255 - (alpha * 255 / 100);
	if(alpha < 8)
		alpha = 8;
	else if(alpha > 255)
		alpha = 255;
	
	hwnd = GetParent(GetParent(hwndClock));
	
	exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if(m_layered.had_style == -1) {
		typedef BOOL (WINAPI* GetLayeredWindowAttributes_t)(HWND hwnd, COLORREF* pcrKey, BYTE* pbAlpha, DWORD* pdwFlags);
		GetLayeredWindowAttributes_t pGetLayeredWindowAttributes = (GetLayeredWindowAttributes_t)GetProcAddress(GetModuleHandleA("user32"), "GetLayeredWindowAttributes");
		if(alpha == 255 && !clear_taskbar)
			return; // nothing to do
		m_layered.had_style = (exstyle & WS_EX_LAYERED ? 1 : 0);
		if(pGetLayeredWindowAttributes)
			pGetLayeredWindowAttributes(hwnd, &m_layered.color, &m_layered.alpha, &m_layered.flags);
	}
	
	if(!m_layered.had_style) {
		if(alpha < 255 || clear_taskbar)
			exstyle |= WS_EX_LAYERED;
		else
			exstyle &= ~WS_EX_LAYERED;
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exstyle);
	}
	
	RefreshRebar(hwnd);
	if(clear_taskbar)
		SetLayeredWindowAttributes(hwnd, GetSysColor(COLOR_3DFACE), (BYTE)alpha, LWA_COLORKEY|LWA_ALPHA);
	else
		SetLayeredWindowAttributes(hwnd, 0, (BYTE)alpha, LWA_ALPHA);
}

/*--------------------------------------------------
    redraw ReBarWindow32 forcely
----------------------------------------------------*/
void RefreshRebar(HWND hwndBar)
{
	HWND hwnd;
	char classname[80];
	
	hwnd = GetWindow(hwndBar, GW_CHILD);
	while(hwnd) {
		GetClassNameA(hwnd, classname, 80);
		if(strcasecmp(classname, "ReBarWindow32") == 0) {
			InvalidateRect(hwnd, NULL, TRUE);
			hwnd = GetWindow(hwnd, GW_CHILD);
			while(hwnd) {
				InvalidateRect(hwnd, NULL, TRUE);
				hwnd = GetWindow(hwnd, GW_HWNDNEXT);
			}
			break;
		}
		hwnd = GetWindow(hwnd, GW_HWNDNEXT);
	}
}

//void TC2DrawBlt(HDC dhdc, int dx, int dy, int dw, int dh, HDC shdc, int sx, int sy, int sw, int sh, BOOL useTrans)
//{
//	if(useTrans) GdiTransparentBlt(dhdc, dx, dy, dw, dh, shdc, sx, sy, sw, sh, RGB(255, 0, 255));
//	else StretchBlt(dhdc, dx, dy, dw, dh, shdc, sx, sy, sw, sh, SRCCOPY);
//}

/// DrawTheme

void InitDrawTheme()
{
	if(m_theme_version)
		return;
	m_theme_version = GetVersion() & 0xffff;
	m_theme_version = ((m_theme_version & 0xff) << 8 | m_theme_version >> 8);
	hmodUxTheme = LoadLibraryA("uxtheme");
	if(hmodUxTheme){
		THEME_FUNC_RETRIEVE(CloseThemeData);
		THEME_FUNC_RETRIEVE(OpenThemeData);
		THEME_FUNC_RETRIEVE(IsThemeActive);
		THEME_FUNC_RETRIEVE(SetWindowTheme);
		THEME_FUNC_RETRIEVE(GetThemeColor);
//		THEME_FUNC_RETRIEVE(IsThemePartDefined);
//		THEME_FUNC_RETRIEVE(IsThemeBackgroundPartiallyTransparent);
		THEME_FUNC_RETRIEVE(DrawThemeBackground);
		THEME_FUNC_RETRIEVE(DrawThemeParentBackground);
	}
}

void ReloadXPClockTheme()
{
	THEME_FUNC_CHECK(OpenThemeData,)
	if(hClockTheme){
		pCloseThemeData(hClockTheme);
		hClockTheme=NULL;
	}
}
COLORREF GetXPClockColor(HWND hwndClock, int state)
{
	COLORREF ret;
	THEME_FUNC_CHECK_THEME(GetThemeColor, hwndClock, 0x00FFFFFF)
	pGetThemeColor(hClockTheme, m_theme_clock_part, state, TMT_TEXTCOLOR, &ret);
	return ret;
}
COLORREF GetXPClockColorBG(HWND hwndClock, int state)
{
	COLORREF ret;
	THEME_FUNC_CHECK_THEME(GetThemeColor, hwndClock, 0x00000000)
	pGetThemeColor(hClockTheme, m_theme_clock_part, state, TMT_BACKGROUND, &ret);
	return ret;
}
void DrawXPClockBackground(HWND hwnd, HDC hdc, RECT* prc)
{
	THEME_FUNC_CHECK(DrawThemeParentBackground,)
	THEME_FUNC_CHECK_THEME(DrawThemeBackground,hwnd,)
	pDrawThemeParentBackground(hwnd, hdc, prc);
	if(m_theme_version >= TV_7)
		pDrawThemeBackground(hClockTheme, hdc, m_theme_clock_part, CLS_NORMAL, prc, NULL);
}
void DrawXPClockHover(HWND hwnd, HDC hdc, RECT* prc)
{
	//THEME_FUNC_CHECK(DrawThemeParentBackground,)
	THEME_FUNC_CHECK_THEME(DrawThemeBackground,hwnd,)
	//pDrawThemeParentBackground(hwnd, hdc, prc);
	if(m_theme_version >= TV_7)
		pDrawThemeBackground(hClockTheme, hdc, m_theme_clock_part, CLS_HOT, prc, NULL);
}

BOOL IsXPThemeActive(){
//	HIGHCONTRAST highcontContrast = {sizeof(HIGHCONTRAST)};
	THEME_FUNC_CHECK(IsThemeActive,0)
//	SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &highcontContrast, 0);
//	if(highcontContrast.dwFlags & HCF_HIGHCONTRASTON)
//		return 0;
	return pIsThemeActive();
}
HRESULT SetXPWindowTheme(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList){
	THEME_FUNC_CHECK(SetWindowTheme,S_FALSE)
	return pSetWindowTheme(hwnd,pszSubAppName,pszSubIdList);
}


HICON GetStockIcon(SHSTOCKICONID siid, unsigned flag) {
	SHGetStockIconInfo_t SHGetStockIconInfo;
	SHSTOCKICONINFO icon;
	SHGetStockIconInfo = (SHGetStockIconInfo_t)GetProcAddress(GetModuleHandleA("shell32"), "SHGetStockIconInfo");
	if(SHGetStockIconInfo) {
		icon.cbSize = sizeof(icon);
		if(SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | flag, &icon) == S_OK)
			return icon.hIcon;
	} else {
		switch(siid) {
		case SIID_WARNING:
			icon.hIcon = LoadIcon(NULL, IDI_WARNING);
			break;
		case SIID_SHIELD:
		case SIID_INFO:
			icon.hIcon = LoadIcon(NULL, IDI_INFORMATION);
			break;
		case SIID_ERROR:
			icon.hIcon = LoadIcon(NULL, IDI_ERROR);
			break;
		default:
			return NULL;
		}
		return CopyIcon(icon.hIcon);
	}
	return NULL;
}
