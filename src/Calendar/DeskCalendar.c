#include "../common/globals.h"
#include "../common/newapi.h"
#include "../common/resource.h"
#include "../common/control_extensions.h"
#include <commctrl.h>
#include <time.h>
//other
#include "../common/calendar.inc"
HINSTANCE g_instance;
TClockAPI api;
BOOL m_bAutoClose;
BOOL m_bTopMost;
//===========================================================================================
//----------------------------------------------------+++--> Get the Current Day of the Year:
void GetDayOfYearTitle(wchar_t* szTitle, int ivMonths)   //-------------------------------+++-->
{
	struct tm today;
	wchar_t szDoY[8];
	time_t ts = time(NULL);
	
	localtime_r(&ts, &today);
//	wcsftime(szDoY, 8, L"%#j", &today); // <--{OutPut}--> Day 95
	wcsftime(szDoY, 8, L"%j", &today);   // <--{OutPut}--> Day 095
	
	if(api.OS < TOS_VISTA && ivMonths==1) {
		wsprintf(szTitle, FMT("Calendar:  Day: %s"), szDoY);
	} else {
		wsprintf(szTitle, FMT("T-Clock: Calendar  Day: %s"), szDoY);
	}
}
LRESULT CALLBACK Window_Calendar(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
	case WM_CREATE:{
		int iMonths,iMonthsPast;
		DWORD dwCalStyle;
		RECT rc; HWND hCal;
		size_t idx;
		size_t dirty = 0;
		
		iMonths = api.GetIntEx(L"Calendar", L"ViewMonths", 3);
		iMonthsPast = api.GetIntEx(L"Calendar", L"ViewMonthsPast", 1);
		
		if(api.GetInt(L"Calendar", L"ShowDayOfYear", 1)) {
			wchar_t szTitle[32];
			GetDayOfYearTitle(szTitle,iMonths);
			SetWindowText(hwnd,szTitle);
		}
		
		dwCalStyle = WS_CHILD|WS_VISIBLE;
		if(api.GetInt(L"Calendar",L"ShowWeekNums",0))
			dwCalStyle|=MCS_WEEKNUMBERS;
		
		hCal = CreateWindowEx(0, MONTHCAL_CLASS, L"", dwCalStyle, 0,0,0,0, hwnd, NULL, NULL, NULL);
		if(!hCal) return -1;
		
		for(idx=0; idx<CALENDAR_COLOR_NUM; ++idx){
			unsigned color = api.GetInt(L"Calendar", g_calendar_color[idx].reg, TCOLOR(TCOLOR_DEFAULT));
			if(color != TCOLOR(TCOLOR_DEFAULT)){
				dirty |= (1<<idx);
				MonthCal_SetColor(hCal, g_calendar_color[idx].mcsc, api.GetColor(color,0));
			}
		}
		if(dirty&~1)
			SetXPWindowTheme(hCal, NULL, L"");
		
		MonthCal_GetMinReqRect(hCal,&rc);//size for a single month
		if(iMonths>1){
			#define CALBORDER 4
			#define CALTODAYTEXTHEIGHT 13
			rc.right+=CALBORDER;
			rc.bottom-=CALTODAYTEXTHEIGHT;
			if(api.OS <= TOS_XP){
				rc.right += 2;
				rc.bottom += 2;
			}
			switch(iMonths){
			/*case 1:*/ case 2: case 3:
			case 4: case 5:
				rc.right*=iMonths;
				break;
			case 6:
				rc.bottom*=3;
				/* fall through */
			case 7: case 8:
				rc.right*=2;
				if(iMonths!=6) rc.bottom*=4;
				break;
			case 9:
				rc.bottom*=3;
			case 11: case 12:
				rc.right*=3;
				if(iMonths!=9) rc.bottom*=4;
				break;
			case 10:
				rc.right*=5;
				rc.bottom*=2;
				break;
			}
			rc.right-=CALBORDER;
			rc.bottom+=CALTODAYTEXTHEIGHT;
			if(api.OS >= TOS_VISTA)
				MonthCal_SizeRectToMin(hCal,&rc);//removes some empty space.. (eg at 4 months)
		}else{
			if(rc.right<(LONG)MonthCal_GetMaxTodayWidth(hCal))
				rc.right=MonthCal_GetMaxTodayWidth(hCal);
		}
		SetWindowPos(hCal,HWND_TOP,0,0,rc.right,rc.bottom,SWP_NOZORDER|SWP_NOACTIVATE);
		if(iMonthsPast){
			SYSTEMTIME st,stnew; MonthCal_GetCurSel(hCal,&st); stnew=st;
			stnew.wDay=1;
			if(iMonthsPast>=stnew.wMonth){ --stnew.wYear; stnew.wMonth+=12; }
			stnew.wMonth-=(short)iMonthsPast;
			if(stnew.wMonth>12){ ++stnew.wYear; stnew.wMonth-=12; }  // in case iMonthsPast is negative
			MonthCal_SetCurSel(hCal,&stnew);
			MonthCal_SetCurSel(hCal,&st);
		}
		SetFocus(hCal);
		AdjustWindowRectEx(&rc, (WS_CAPTION | WS_POPUP | WS_SYSMENU | WS_VISIBLE), FALSE, 0);
		SetWindowPos(hwnd, HWND_TOP, 0,0, (rc.right - rc.left), (rc.bottom - rc.top), SWP_NOMOVE);//force to be on top
		if(m_bTopMost)
			SetWindowPos(hwnd, HWND_TOPMOST, 0,0, 0,0, (SWP_NOMOVE | SWP_NOSIZE));
		api.PositionWindow(hwnd, (api.OS<TOS_WIN10 && api.OS>=TOS_WIN7 ? 11 : 0));
		if(m_bAutoClose && GetForegroundWindow() != hwnd)
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		return 0;}
	case WM_ACTIVATE:
		if(!m_bTopMost){
			WM_ActivateTopmost(hwnd, wParam, lParam);
		}
		if(m_bAutoClose){
			if(LOWORD(wParam) == WA_INACTIVE){
				PostMessage(hwnd, WM_CLOSE, 0, 0);//adds a little more delay which is good
			}
		}
		break;
	case WM_DESTROY:
		Sleep(50);//we needed more delay... 50ms looks good (to allow T-Clock's FindWindow to work) 100ms is slower then Vista's native calendar
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
//======================================================
//-------------------------------+++--> Open "Calendar":
HWND CreateCalender(HWND hwnd)   //---------------+++-->
{
	INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_DATE_CLASSES};
	WNDCLASSEX wcx = {sizeof(wcx)};
	ATOM calclass;
	m_bAutoClose = api.GetInt(L"Calendar", L"CloseCalendar", 1);
	m_bTopMost = api.GetInt(L"Calendar", L"CalendarTopMost", 0);
	InitCommonControlsEx(&icex);
	
	wcx.style = 0;
	wcx.lpfnWndProc = Window_Calendar;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = g_instance;
	wcx.hIcon = NULL;
	wcx.hIcon = LoadIcon(g_instance,MAKEINTRESOURCE(IDI_MAIN));
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)COLOR_WINDOWFRAME;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = L"ClockFlyoutWindow";
	wcx.hIconSm = NULL;
	calclass = RegisterClassEx(&wcx);
	hwnd = CreateWindowEx(0, MAKEINTATOM(calclass), L"T-Clock: Calendar", (WS_CAPTION|WS_POPUP|WS_SYSMENU|WS_VISIBLE), 0,0,0,0, hwnd, 0, g_instance, NULL);
	return hwnd;
}

//int main(int argc,char* argv[])
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow)
{
	MSG msg;
	BOOL bRet;
	(void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;// don't warn me about they being unused
	g_instance = hInstance;
	if(LoadClockAPI(L"T-Clock" ARCH_SUFFIX, &api))
		return 2;
	if(!CreateCalender(0))
		return 1;
	while((bRet=GetMessage(&msg,NULL,0,0))!=0){
		if(bRet==-1){//handle the error and possibly exit
			break;
		}else{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	EndNewAPI(NULL);
	return 0;
}
