// https://msdn.microsoft.com/en-us/library/windows/desktop/ff485973(v=vs.85).aspx
#include "tclock.h"

static const wchar_t kKeyTimers[] = L"Timers"; ///< "Timers"
static const wchar_t kKeyTimersTimer[] = L"Timers\\Timer"; ///< "Timers\Timer"
#define kTimerNameLen GEN_BUFF
#define WATCH_ITEM_SORT 123 ///< send as WM_COMMAND, with HIWORD 0(Toggle), 1(Asc), 2(Dec), 3(Refresh) and lParam with column index

typedef struct timer_t timer_t;
static int m_timers = 0;
static timer_t* m_timer = NULL; // Array of Currently Active Timers
static char m_watch_refresh;
static int m_watch_flags;
static unsigned char m_watch_alpha;
static char m_last_sorted;

static int TimerFindIndexById(int id);
static void TimerWatch_Hide(int id, int hide);
INT_PTR CALLBACK Window_Timer(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Window_TimerView(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void DialogTimer(int select_id) {
	CreateDialogParamOnce(&g_hDlgTimer, 0, MAKEINTRESOURCE(IDD_TIMER), NULL, Window_Timer, select_id);
}

void WatchTimer(int reset) {
	if(reset)
		TimerWatch_Hide(-1, 0);
	CreateDialogParamOnce(&g_hDlgTimerWatch, 0, MAKEINTRESOURCE(IDD_TIMERVIEW), NULL, Window_TimerView, 0);
}

void EndAllTimers() {
	m_timers = 0;
	free(m_timer) ,m_timer = NULL;
}

void UpdateTimerMenu(HMENU hMenu) {
	int count = api.GetInt(kKeyTimers, L"NumberOfTimers", 0);
	if(m_timers) {
//		EnableMenuItem(hMenu, IDM_TIMEWATCH, (MF_BYCOMMAND|MF_ENABLED));
		EnableMenuItem(hMenu, IDM_TIMEWATCHRESET, (MF_BYCOMMAND|MF_ENABLED));
	}
	if(count){
		int id;
		wchar_t buf[GEN_BUFF+16];
		wchar_t subkey[TNY_BUFF];
		size_t offset = wsprintf(subkey, kKeyTimersTimer);
		wcscpy(buf, L"    ");
		InsertMenu(hMenu,IDM_TIMER,MF_BYCOMMAND|MF_SEPARATOR,0,NULL);
		for(id=0; id<count; ++id){
			wchar_t* pos = buf+4;
			wsprintf(subkey+offset, FMT("%d"), id + 1);
			pos += api.GetStr(subkey, L"Name", pos, GEN_BUFF, L"");
			wsprintf(pos, FMT("	(%i"), id+1);
			InsertMenu(hMenu, IDM_TIMER, MF_BYCOMMAND|MF_STRING, IDM_I_TIMER+id, buf);
			if(TimerFindIndexById(id) != -1)
				CheckMenuItem(hMenu, IDM_I_TIMER+id, MF_BYCOMMAND|MF_CHECKED);
		}
		InsertMenu(hMenu, IDM_TIMER, MF_BYCOMMAND|MF_SEPARATOR, 0, NULL);
	}
}

void TimerMenuItemClick(HMENU hmenu, int itemid) {
	int id = itemid - IDM_I_TIMER;
	TimerEnable(id, !(GetMenuState(hmenu,itemid,MF_BYCOMMAND)&MF_CHECKED));
}

// structure for active timers
struct timer_t {
	int64_t expire;
	int id;
	char hidden;
};

static void TimerTrigger(HWND hwnd, int id);
static int64_t TimerParseExpire_(const wchar_t* subkey);
static void TimerWatch_Delete(int id);
/**
 * \brief deletes \p idx from \c m_timer and reallocates
 * \param idx */
static void TimerFree_(int idx) {
	timer_t* tnew = NULL;
	if(--m_timers){
		tnew = (timer_t*)malloc(sizeof(timer_t) * m_timers);
		if(!tnew){
			++m_timers;
			return;
		}
		memcpy(tnew, m_timer, (sizeof(timer_t) * idx));
		memcpy(tnew+idx, m_timer+idx+1, (sizeof(timer_t) * (m_timers-idx)));
	}
	free(m_timer);
	m_timer = tnew;
}
void TimerEnable(int id, int enable) {
	wchar_t subkey[TNY_BUFF];
	size_t offset;
	int idx;
	timer_t* tnew;
	int id_end = id + 1;
	
	if(id == -1) {
		id = 0;
		id_end = api.GetInt(kKeyTimers, L"NumberOfTimers", 0);
	}
	
	offset = wsprintf(subkey, kKeyTimersTimer);
	for(; id<id_end; ++id) {
		idx = TimerFindIndexById(id);
		wsprintf(subkey+offset, FMT("%d"), id + 1);
		
		if(enable == -1) {
			enable = (idx == -1);
		} else if(!enable == !(idx != -1)) {
			continue;
		}
		if(enable) {
			if(api.GetStr(subkey, L"Name", NULL, 0, NULL) <= 0)
				continue;
			tnew = (timer_t*)malloc(sizeof(timer_t) * (m_timers + 1));
			if(!tnew)
				continue;
			memcpy(tnew, m_timer, (sizeof(timer_t) * m_timers));
			
			tnew[m_timers].id = id;
			tnew[m_timers].hidden = 0;
			tnew[m_timers].expire = TimerParseExpire_(subkey);
			free(m_timer);
			m_timer = tnew;
			++m_timers;
		} else {
			TimerWatch_Delete(id);
			TimerFree_(idx);
		}
	}
}

void OnTimerTimer(HWND hwnd) {
	int64_t now;
	int idx;
	if(!m_timers)
		return;
	
	now = api.GetTickCount64();
	for(idx=0; idx<m_timers; ++idx) {
		if(now >= m_timer[idx].expire){
			int id = m_timer[idx].id;
			TimerTrigger(hwnd, id);
			TimerFree_(idx--);
			m_watch_refresh = 1;
		}
	}
}

static int TimerFindIndexById(int id) {
	int idx;
	for(idx=m_timers; idx--; ) {
		if(id == m_timer[idx].id)
			break;
	}
	return idx;
}
static int TimerGetHiddenCount() {
	int count = 0;
	int idx;
	for(idx=m_timers; idx--; ) {
		if(m_timer[idx].hidden)
			++count;
	}
	return count;
}
/**
 * \brief adjusts timer ids stored in \c m_timer to stay synchronized
 * \param reference_id start after \c reference_id
 * \param adjustment adjustment value, usually -1 */
static void TimerAdjustIds(int reference_id, int adjustment) {
	int idx;
	for(idx=m_timers; idx--; ) {
		if(m_timer[idx].id > reference_id)
			m_timer[idx].id += adjustment;
	}
	if(g_hDlgTimerWatch) {
		HWND list = GetDlgItem(g_hDlgTimerWatch, IDC_LIST);
		LVITEM item;
		int count;
		item.mask = LVIF_PARAM;
		item.iSubItem = 0;
		count = ListView_GetItemCount(list);
		for(item.iItem=0; item.iItem<count; ++item.iItem) {
			ListView_GetItem(list, &item);
			if(item.lParam > reference_id) {
				item.lParam += adjustment;
				ListView_SetItem(list, &item);
			}
		}
	}
}

static int64_t TimerParseExpire_(const wchar_t* subkey) {
	int64_t ret;
	ret = api.GetTickCount64();
	ret += api.GetInt(subkey, L"Seconds",0) *  1000;
	ret += api.GetInt(subkey, L"Minutes",0) * 60000;
	ret += api.GetInt(subkey, L"Hours",0) * 3600000;
	ret += api.GetInt(subkey, L"Days",0) * 86400000;
	return ret;
}

static void TimerTrigger(HWND hwnd, int id) {
	wchar_t subkey[TNY_BUFF];
	size_t offset;
	wchar_t fname[MAX_BUFF];
	offset = wsprintf(subkey, kKeyTimersTimer);
	wsprintf(subkey+offset, FMT("%d"), id + 1);
	api.GetStr(subkey, L"File", fname, _countof(fname), L"");
	PlayFile(hwnd, fname, api.GetInt(subkey, L"Repeat", 0)?-1:0);
	if(api.GetInt(subkey, L"Blink", 0))
		PostMessage(g_hwndClock, CLOCKM_BLINK, 0, 0);
}

static void TimerWatch_Hide(int id, int hide) {
	LVFINDINFO search;
	HWND list = NULL;
	int idx, end;
	search.flags = LVFI_PARAM;
	
	if(g_hDlgTimerWatch)
		list = GetDlgItem(g_hDlgTimerWatch, IDC_LIST);
	
	if(id == -1) {
		idx = 0;
		end = m_timers;
	} else {
		idx = TimerFindIndexById(id);
		if(idx == -1) {
			if(hide && list) {
				search.lParam = id;
				id = ListView_FindItem(list, -1, &search);
				if(id != -1)
					ListView_DeleteItem(list, id);
			}
			return;
		}
		end = idx + 1;
	}
	for(; idx<end; ++idx) {
		m_timer[idx].hidden = (char)hide;
		if(hide && list) {
			search.lParam = m_timer[idx].id;
			id = ListView_FindItem(list, -1, &search);
			if(id != -1)
				ListView_DeleteItem(list, id);
		}
	}
}
static void TimerWatch_Delete(int id) {
	if(g_hDlgTimerWatch) {
		HWND list = GetDlgItem(g_hDlgTimerWatch, IDC_LIST);
		LVFINDINFO lvFind;
		int sel;
		lvFind.flags = LVFI_PARAM;
		lvFind.lParam = id;
		sel = ListView_FindItem(list, -1, &lvFind);
		if(sel != -1)
			ListView_DeleteItem(list, sel);
	}
}
static void TimerWatchList_Restart(HWND list, int itemid) {
	LVITEM item;
	int itemid_end = itemid + 1;
	int idx;
	item.mask = LVIF_PARAM;
	item.iItem = itemid;
	item.iSubItem = 0;
	if(item.iItem == -1) {
		item.iItem = 0;
		itemid_end = ListView_GetItemCount(list);
	}
	for(; item.iItem<itemid_end; ++item.iItem) {
		ListView_GetItem(list, &item);
		idx = TimerFindIndexById((int)item.lParam);
		if(idx == -1) {
			TimerEnable((int)item.lParam, 1);
		} else {
			wchar_t subkey[TNY_BUFF];
			int offset = wsprintf(subkey, kKeyTimersTimer);
			wsprintf(subkey+offset, FMT("%d"), item.lParam + 1);
			m_timer[idx].expire = TimerParseExpire_(subkey);
		}
	}
}



static void UpdateNextCtrl(HWND hWnd, int iSpin, int iEdit, char bGoUp) {
	NMUPDOWN nmud;
	
	nmud.hdr.hwndFrom = GetDlgItem(hWnd, iSpin);
	nmud.hdr.idFrom = iSpin;
	nmud.hdr.code = UDN_DELTAPOS;
	if(bGoUp)nmud.iDelta = 4; // Fake Message Forces Update of Next Control!
	else nmud.iDelta = -4;   // Fake Message Forces Update of Next Control!
	nmud.iPos = GetDlgItemInt(hWnd, iEdit, NULL, 1);
	
	SendMessage(hWnd, WM_NOTIFY, iSpin, (LPARAM)&nmud);
}
// timer settings (for add/edit dialog)
typedef struct{
	int second;
	int minute;
	int hour;
	int day;
	wchar_t name[kTimerNameLen];
	wchar_t fname[MAX_BUFF];
	char bRepeat;
	char bBlink;
} timeropt_t;

static void OnTimerName(HWND hwnd) {
	HWND timer_cb = GetDlgItem(hwnd, IDC_TIMERNAME);
	wchar_t name[TNY_BUFF];
	int id, count;
	int running;
	
	ComboBox_GetText(timer_cb, name, _countof(name));
	count = ComboBox_GetCount(timer_cb);
	for(id=0; id<count; ++id){
		timeropt_t* pts;
		pts = (timeropt_t*)ComboBox_GetItemData(timer_cb, id);
		if(!wcscmp(name, pts->name)){
			SetDlgItemInt(hwnd, IDC_TIMERSECOND,	pts->second, 0);
			SetDlgItemInt(hwnd, IDC_TIMERMINUTE,	pts->minute, 0);
			SetDlgItemInt(hwnd, IDC_TIMERHOUR,		pts->hour,   0);
			SetDlgItemInt(hwnd, IDC_TIMERDAYS,		pts->day,    0);
			ComboBox_AddStringOnce(GetDlgItem(hwnd,IDC_TIMERFILE), pts->fname, 1, 0);
			CheckDlgButton(hwnd, IDC_TIMERREPEAT,	pts->bRepeat);
			CheckDlgButton(hwnd, IDC_TIMERBLINK,	pts->bBlink);
			break;
		}
	}
	if(id < count-1){
		running = (TimerFindIndexById(id) == -1 ? 0 : 1);
		SetDlgItemText(hwnd, IDOK, L"Start");
	}else{
		running = 0;
		SetDlgItemText(hwnd, IDOK, L"Create");
	}
	EnableDlgItem(hwnd, IDOK, !running);
	EnableDlgItem(hwnd, IDCB_STOPTIMER, running);
	EnableDlgItem(hwnd, IDC_TIMERDEL, (id < count-1));
}

static void OnTimerEditDestroy(HWND hwnd) {
	HWND timer_cb = GetDlgItem(hwnd, IDC_TIMERNAME);
	int idx;
	int count = ComboBox_GetCount(timer_cb);
	StopFile();
	for(idx=0; idx<count; ++idx) {
		free((void*)ComboBox_GetItemData(timer_cb,idx));
	}
	g_hDlgTimer = NULL;
}

static void OnTimerEditInit(HWND hwnd, int select_id) {
	HWND timer_cb = GetDlgItem(hwnd, IDC_TIMERNAME);
	HWND file_cb = GetDlgItem(hwnd, IDC_TIMERFILE);
	wchar_t subkey[TNY_BUFF];
	size_t offset;
	int id, count;
	timeropt_t* pts;
	
	SendMessage(hwnd, WM_SETICON, ICON_SMALL,(LPARAM)g_hIconTClock);
	SendMessage(hwnd, WM_SETICON, ICON_BIG,(LPARAM)g_hIconTClock);
	// init dialog items
	SendDlgItemMessage(hwnd, IDC_TIMERSECSPIN, UDM_SETRANGE32, 0,59); // 60 Seconds Max
	SendDlgItemMessage(hwnd, IDC_TIMERMINSPIN, UDM_SETRANGE32, 0,59); // 60 Minutes Max
	SendDlgItemMessage(hwnd, IDC_TIMERHORSPIN, UDM_SETRANGE32, 0,23); // 24 Hours Max
	SendDlgItemMessage(hwnd, IDC_TIMERDAYSPIN, UDM_SETRANGE32, 0,7); //  7 Days Max
	/// add default sound files to file dropdown
	ComboBoxArray_AddSoundFiles(&file_cb, 1);
	// add timer to combobox
	offset = wsprintf(subkey, kKeyTimersTimer);
	count = api.GetInt(kKeyTimers, L"NumberOfTimers", 0);
	for(id=0; id<count; ++id) {
		pts = (timeropt_t*)malloc(sizeof(timeropt_t));
		wsprintf(subkey+offset, FMT("%d"), id + 1);
		pts->second = api.GetInt(subkey, L"Seconds",  0);
		pts->minute = api.GetInt(subkey, L"Minutes", 10);
		pts->hour   = api.GetInt(subkey, L"Hours",    0);
		pts->day    = api.GetInt(subkey, L"Days",     0);
		api.GetStr(subkey, L"Name", pts->name, _countof(pts->name), L"");
		api.GetStr(subkey, L"File", pts->fname, _countof(pts->fname), L"");
		pts->bBlink = (char)api.GetInt(subkey, L"Blink", 0);
		pts->bRepeat = (char)api.GetInt(subkey, L"Repeat", 0);
		ComboBox_AddString(timer_cb, pts->name);
		ComboBox_SetItemData(timer_cb, id, pts);
	}
	// add "new timer" item
	pts = (timeropt_t*)calloc(1, sizeof(timeropt_t));
	wcscpy(pts->name, L"<Add New...>");
	ComboBox_AddString(timer_cb, pts->name);
	ComboBox_SetItemData(timer_cb, count, pts);
	ComboBox_SetCurSel(timer_cb, select_id);
	OnTimerName(hwnd);
	SendDlgItemMessage(hwnd, IDC_TIMERTEST, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIconPlay);
	SendDlgItemMessage(hwnd, IDC_TIMERDEL, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIconDel);
	
	api.PositionWindow(hwnd, 21);
}

static void OnOK(HWND hwnd) {
	HWND timer_cb = GetDlgItem(hwnd, IDC_TIMERNAME);
	int id, count, seconds, minutes, hours, days;
	wchar_t subkey[TNY_BUFF];
	size_t offset;
	wchar_t name[GEN_BUFF];
	wchar_t fname[MAX_PATH];
	
	offset = wsprintf(subkey, kKeyTimersTimer);
	ComboBox_GetText(timer_cb, name, _countof(name));
	
	count = ComboBox_GetCount(timer_cb);
	count -= 1; // Skip the Last One Because It's the New Timer Dummy Item
	
	for(id=0; id<count; ++id) {
		timeropt_t* pts;
		pts = (timeropt_t*)ComboBox_GetItemData(timer_cb, id);
		if(!wcscmp(pts->name, name)) {
			break;
		}
	}
	wsprintf(subkey+offset, FMT("%d"), id + 1);
	api.SetStr(subkey, L"Name", name);
	seconds = GetDlgItemInt(hwnd, IDC_TIMERSECOND, 0, 0);
	minutes = GetDlgItemInt(hwnd, IDC_TIMERMINUTE, 0, 0);
	hours   = GetDlgItemInt(hwnd, IDC_TIMERHOUR,   0, 0);
	days    = GetDlgItemInt(hwnd, IDC_TIMERDAYS,   0, 0);
	if(seconds>59) for(; seconds>59; seconds-=60,++minutes);
	if(minutes>59) for(; minutes>59; minutes-=60,++hours);
	if(hours>23) for(; hours>23; hours-=24,++days);
	if(days > 42)
		days = 7;
	api.SetInt(subkey, L"Seconds", seconds);
	api.SetInt(subkey, L"Minutes", minutes);
	api.SetInt(subkey, L"Hours",   hours);
	api.SetInt(subkey, L"Days",    days);
	
	GetDlgItemText(hwnd, IDC_TIMERFILE, fname, _countof(fname));
	if(fname[0] == '<')
		fname[0] = '\0';
	api.SetStr(subkey, L"File", fname);
	
	api.SetInt(subkey, L"Repeat", IsDlgButtonChecked(hwnd, IDC_TIMERREPEAT));
	api.SetInt(subkey, L"Blink",  IsDlgButtonChecked(hwnd, IDC_TIMERBLINK));
	if(id == count)
		api.SetInt(kKeyTimers, L"NumberOfTimers", id + 1);
	
	TimerEnable(id, 1);
}

static void OnBrowseAction(HWND hwnd, WORD id) {
	wchar_t deffile[MAX_PATH], fname[MAX_PATH];
	
	GetDlgItemText(hwnd, id - 1, deffile, MAX_PATH);
	if(!BrowseSoundFile(hwnd, deffile, fname)) // soundselect.c
		return;
	SetDlgItemText(hwnd, id - 1, fname);
	PostMessage(hwnd, WM_NEXTDLGCTL, 1, FALSE);
}

static void OnDel(HWND hwnd) {
	HWND timer_cb = GetDlgItem(hwnd, IDC_TIMERNAME);
	wchar_t subkey[TNY_BUFF];
	size_t offset;
	int id, count;
	int sel = ComboBox_GetCurSel(timer_cb);
	timeropt_t* pts;
	
	if(sel == -1)
		return;
	pts = (timeropt_t*)ComboBox_GetItemData(timer_cb, sel);
	offset = wsprintf(subkey, kKeyTimersTimer);
	count = ComboBox_GetCount(timer_cb) - 1;
	TimerEnable(sel, 0);
	TimerWatch_Delete(sel);
	TimerAdjustIds(sel, -1);
	
	for(id=sel+1; id<count; ++id) {
		pts = (timeropt_t*)ComboBox_GetItemData(timer_cb, id);
		wsprintf(subkey+offset, FMT("%d"), id); // we're 1 behind, as needed
		api.SetStr(subkey, L"Name",		pts->name);
/// @todo (White-Tiger#1#08/19/16): only store cumulative seconds in registry
		api.SetInt(subkey, L"Seconds",	pts->second);
		api.SetInt(subkey, L"Minutes",	pts->minute);
		api.SetInt(subkey, L"Hours",	pts->hour);
		api.SetInt(subkey, L"Days",	pts->day);
		api.SetStr(subkey, L"File",		pts->fname);
		api.SetInt(subkey, L"Repeat",	pts->bRepeat);
		api.SetInt(subkey, L"Blink",	pts->bBlink);
	}
	wsprintf(subkey+offset, FMT("%d"), count);
	api.DelKey(subkey);
	api.SetInt(kKeyTimers, L"NumberOfTimers", --count);
	free((void*)ComboBox_GetItemData(timer_cb,sel));
	ComboBox_DeleteString(timer_cb, sel);
	
	ComboBox_SetCurSel(timer_cb, (sel>0 ? sel-1 : sel));
	OnTimerName(hwnd);
	PostMessage(hwnd, WM_NEXTDLGCTL, 1, FALSE);
}

static void OnTest(HWND hwnd, WORD id) {
	wchar_t fname[MAX_PATH];
	
	GetDlgItemText(hwnd, id - 2, fname, _countof(fname));
	if(!fname[0])
		return;
	
	if((HICON)SendDlgItemMessage(hwnd, id, BM_GETIMAGE, IMAGE_ICON, 0) == g_hIconPlay) {
		if(PlayFile(hwnd, fname, 0)) {
			SendDlgItemMessage(hwnd, id, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIconStop);
			InvalidateRect(GetDlgItem(hwnd, id), NULL, FALSE);
		}
	} else StopFile();
}

static void OnStopTimer(HWND hWnd) {
	HWND timer_cb = GetDlgItem(hWnd, IDC_TIMERNAME);
	int sel = ComboBox_GetCurSel(timer_cb);
	if(sel == -1)
		return;
	
	TimerEnable(sel, 0);
	
	EnableDlgItem(hWnd, IDOK, 1);
	EnableDlgItemSafeFocus(hWnd, IDCB_STOPTIMER, 0, IDOK);
}



INT_PTR CALLBACK Window_Timer(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		OnTimerEditInit(hwnd, (int)lParam);
		return TRUE;
	case WM_DESTROY:
		OnTimerEditDestroy(hwnd);
		break;
	case WM_ACTIVATE:
		WM_ActivateTopmost(hwnd, wParam, lParam);
		break;
		
	case WM_COMMAND: {
		WORD id = LOWORD(wParam);
		WORD code = HIWORD(wParam);
		switch(id) {
		case IDC_TIMERDEL:
			OnDel(hwnd);
			break;
			
		case IDOK:
			OnOK(hwnd);
			/* fall through */
		case IDCANCEL:
			DestroyWindow(hwnd);
			break;
			
		case IDC_TIMERNAME:
			if(code == CBN_EDITCHANGE)
				OnTimerName(hwnd);
			else if(code == CBN_SELCHANGE)
				PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(id, CBN_EDITCHANGE), lParam);
			break;
			
		case IDCB_STOPTIMER:
			OnStopTimer(hwnd);
			break;
			
		case IDC_TIMERFILEBROWSE:
			OnBrowseAction(hwnd, id);
			break;
			
		case IDC_TIMERTEST:
			OnTest(hwnd, id);
			break;
		}
		return TRUE;}
		
	case WM_NOTIFY: {
		NMHDR* notify = (NMHDR*)lParam;
		NMUPDOWN* updown;
		int num;
		if(notify->code == UDN_DELTAPOS) {
			
			updown = (NMUPDOWN*)lParam;
			if(updown->iDelta > 0) {
				switch(updown->hdr.idFrom) {
				case IDC_TIMERSECSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERSECOND, NULL, TRUE);
					if(num == 59)
						UpdateNextCtrl(hwnd, IDC_TIMERMINSPIN, IDC_TIMERMINUTE, TRUE);
					break;
					
				case IDC_TIMERMINSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERMINUTE, NULL, TRUE);
					if(updown->iDelta == 4) {
						if(num < 59)
							SetDlgItemInt(hwnd, IDC_TIMERMINUTE, num+1, TRUE);
					}
					if(num == 59)
						UpdateNextCtrl(hwnd, IDC_TIMERHORSPIN, IDC_TIMERHOUR, TRUE);
					break;
					
				case IDC_TIMERHORSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERHOUR, NULL, TRUE);
					if(updown->iDelta == 4) {
						if(num < 23)
							SetDlgItemInt(hwnd, IDC_TIMERHOUR, num+1, TRUE);
					}
					if(num == 23)
						UpdateNextCtrl(hwnd, IDC_TIMERDAYSPIN, IDC_TIMERDAYS, TRUE);
					break;
					
				case IDC_TIMERDAYSPIN:
					if(updown->iDelta == 4) {
						num = GetDlgItemInt(hwnd, IDC_TIMERDAYS, NULL, TRUE);
						if(num < 7)
							SetDlgItemInt(hwnd, IDC_TIMERDAYS, num+1, TRUE);
					} break;
				}
			} else {
				switch(updown->hdr.idFrom) {
				case IDC_TIMERSECSPIN:
					if(updown->iDelta == -4) {
						num = GetDlgItemInt(hwnd, IDC_TIMERSECOND, NULL, TRUE);
						if(num > 0)
							SetDlgItemInt(hwnd, IDC_TIMERSECOND, num-1, TRUE);
					} break;
					
				case IDC_TIMERMINSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERMINUTE, NULL, TRUE);
					if(updown->iDelta == -4) {
						if(num > 0)
							SetDlgItemInt(hwnd, IDC_TIMERMINUTE, num-1, TRUE);
					}
					if(num == 0)
						UpdateNextCtrl(hwnd, IDC_TIMERSECSPIN, IDC_TIMERSECOND, FALSE);
					break;
					
				case IDC_TIMERHORSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERHOUR, NULL, TRUE);
					if(updown->iDelta == -4) {
						if(num > 0)
							SetDlgItemInt(hwnd, IDC_TIMERHOUR, num-1, TRUE);
					}
					if(num == 0)
						UpdateNextCtrl(hwnd, IDC_TIMERMINSPIN, IDC_TIMERMINUTE, FALSE);
					break;
					
				case IDC_TIMERDAYSPIN:
					num = GetDlgItemInt(hwnd, IDC_TIMERDAYS, NULL, TRUE);
					if(num == 0)
						UpdateNextCtrl(hwnd, IDC_TIMERHORSPIN, IDC_TIMERHOUR, FALSE);
					break;
				}
			}
		}
		return TRUE;}
		
	case MM_MCINOTIFY:
	case MM_WOM_DONE:
		StopFile();
		SendDlgItemMessage(hwnd, IDC_TIMERTEST, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_hIconPlay);
		return TRUE;
	}
	return FALSE;
}


enum WATCH_FLAG {
	WF_OVERLAY    = 0x01,
};
enum {
	kOpacity80 = 255 * 80 / 100,
	kOpacity50 = 255 * 50 / 100,
	kOpacity25 = 255 * 25 / 100,
};

static int GetTimerInfo(wchar_t* dst, int idx) {
	wchar_t* out = dst;
	if(idx < m_timers) {
		int64_t now = api.GetTickCount64();
		int days, hours, minutes, seconds;
		seconds = (int)((m_timer[idx].expire - now) / 1000);
		days = seconds/86400; seconds%=86400;
		hours = seconds/3600; seconds%=3600;
		minutes = seconds/60; seconds%=60;
		if(days)
			out += wsprintf(out, FMT("%d day%s "), days, (days==1 ? L"" : L"s"));
		out += wsprintf(out, FMT("%02d:%02d:%02d"), hours, minutes, seconds);
		return (int)(out-dst);
	}
	*out = '\0';
	return 0;
}
static void OnTimerWatchInit(HWND hwnd) {
	LVCOLUMN lvCol;
	HWND list = GetDlgItem(hwnd,IDC_LIST);
	
	m_watch_refresh = 0;
	m_watch_flags = api.GetInt(kKeyTimers, L"Options", 0);
	m_watch_alpha = (unsigned char)api.GetInt(kKeyTimers, L"Alpha", 255);
	SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
	m_last_sorted = 1; // time
	
	SendMessage(hwnd, WM_SETICON, ICON_SMALL,(LPARAM)g_hIconTClock);
	SendMessage(hwnd, WM_SETICON, ICON_BIG,(LPARAM)g_hIconTClock);
	
	ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
	SetXPWindowTheme(list, L"Explorer", NULL);
	
	lvCol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvCol.cx = 125;		 // Column Width
	lvCol.iSubItem = 0;      // Column Number
	lvCol.fmt = LVCFMT_CENTER; // Column Alignment
	lvCol.pszText = TEXT("Timer"); // Column Header Text
	ListView_InsertColumn(list, 0, &lvCol);
	
	lvCol.cx = 150;
	lvCol.iSubItem = 1;
	lvCol.fmt = LVCFMT_LEFT;
	lvCol.pszText = TEXT("Remaining");
	ListView_InsertColumn(list, 1, &lvCol);
	
	SetTimer(hwnd, 3, 285, NULL);
	api.PositionWindow(hwnd, 21);
	SendMessage(hwnd, WM_TIMER, 285, 0);
}
static int OnWatchTimer(HWND hwnd) {
	HWND list = GetDlgItem(hwnd,IDC_LIST);
	int offset;
	wchar_t subkey[TNY_BUFF];
	wchar_t str[kTimerNameLen > MIN_BUFF ? kTimerNameLen : MIN_BUFF];
	int count = 0;
	int added = 0;
	LVFINDINFO lvFind;
	LVITEM lvItem;
	int idx;
	
	lvFind.flags = LVFI_PARAM;
	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	lvItem.pszText = str;
	offset = wsprintf(subkey, kKeyTimersTimer);
	
	if(m_watch_refresh) {
		m_watch_refresh = 0;
		lvItem.pszText = L"expired";
		for(idx=ListView_GetItemCount(list); idx--; ) {
			lvItem.iItem = idx;
			ListView_SetItem(list, &lvItem);
		}
		lvItem.pszText = str;
	}
	
	for(idx=0; idx<m_timers; ++idx) {
		if(!m_timer[idx].hidden) {
			
			++count;
			lvFind.lParam = m_timer[idx].id;
			lvItem.iItem = ListView_FindItem(list, -1, &lvFind);
			if(lvItem.iItem == -1) {
				wsprintf(subkey+offset, FMT("%d"), m_timer[idx].id + 1);
				api.GetStr(subkey, L"Name", lvItem.pszText, _countof(str), L"");
				
//				lvFind.flags = LVFI_STRING;
//				lvFind.psz = lvItem.pszText;
//				lvItem.iItem = ListView_FindItem(list, -1, &lvFind);
//				lvFind.flags = LVFI_PARAM; // restore
				
				lvItem.lParam = m_timer[idx].id;
				lvItem.iSubItem = 0;
//				if(lvItem.iItem != -1) {
//					lvItem.mask = LVIF_PARAM;
//					ListView_SetItem(list, &lvItem);
//				} else {
					lvItem.mask = LVIF_TEXT | LVIF_PARAM;
					lvItem.iItem = INT_MAX; // at the end
					lvItem.iItem = ListView_InsertItem(list, &lvItem);
//				}
				++added;
				lvItem.mask = LVIF_TEXT; // restore
				lvItem.iSubItem = 1;
			}
			GetTimerInfo(lvItem.pszText, idx);
			ListView_SetItem(list, &lvItem);
		}
	}
	if(added) {
		Window_TimerView(hwnd, WM_COMMAND, MAKEWPARAM(WATCH_ITEM_SORT,3), m_last_sorted);
	}
	return count;
}
static void RemoveFromWatch(HWND hWnd, HWND list, int index) {
	const wchar_t message[] = L"Yes will cancel the timer & remove it from the Watch List\n"
							L"No will remove timer from Watch List only (timer continues)\n"
							L"Cancel will assume you hit delete accidentally (and do nothing)";
	wchar_t caption[TNY_BUFF + kTimerNameLen];
	wchar_t name[kTimerNameLen];
	LVITEM item;
	int idx;
	
	item.mask = LVIF_TEXT | LVIF_PARAM;
	item.iItem = index;
	item.iSubItem = 0;
	item.pszText = name;
	item.cchTextMax = _countof(name);
	ListView_GetItem(list, &item);
	
	idx = TimerFindIndexById((int)item.lParam);
	if(idx == -1) {
		ListView_DeleteItem(list, item.iItem);
		return;
	}
	
	wsprintf(caption, FMT("Cancel Timer (%s) Also?"), item.pszText);
	idx = MessageBox(hWnd, message, caption, MB_YESNOCANCEL|MB_ICONQUESTION|MB_SETFOREGROUND);
	
	item.mask = LVIF_PARAM;
	ListView_GetItem(list, &item); // get up-to-date data
	
	switch(idx) {
	case IDYES:
		TimerEnable((int)item.lParam, 0);
		break;
	case IDNO:
		TimerWatch_Hide((int)item.lParam, 1);
		break;
	}
}



static int CALLBACK SortTime(HWND list, int column, int flags, intptr_t id1, intptr_t id2, intptr_t user) {
	int order;
	int64_t time1;
	int64_t time2;
	
	(void)list, (void)column, (void)user;
	
	order = TimerFindIndexById((int)id1);
	if(order != -1)
		time1 = m_timer[order].expire;
	else
		time1 = 0;
	
	order = TimerFindIndexById((int)id2);
	if(order != -1)
		time2 = m_timer[order].expire;
	else
		time2 = 0;
	
	order = (int)(time1 - time2);
	
	if(flags & SORT_DEC)
		order = -order;
	return order;
}
INT_PTR CALLBACK Window_TimerView(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:{
		OnTimerWatchInit(hwnd);
		return TRUE;}
	case WM_DESTROY:
		if(m_watch_flags)
			api.SetInt(kKeyTimers, L"Options", m_watch_flags);
		else
			api.DelValue(kKeyTimers, L"Options");
		if(m_watch_alpha != 255)
			api.SetInt(kKeyTimers, L"Alpha", m_watch_alpha);
		else
			api.DelValue(kKeyTimers, L"Alpha");
		g_hDlgTimerWatch = NULL;
		return 0;
	case WM_WINDOWPOSCHANGED:{
		WINDOWPOS* info = (WINDOWPOS*)lParam;
		RECT rc;
		if(!(info->flags&SWP_NOSIZE) || info->flags&SWP_FRAMECHANGED){
			GetClientRect(hwnd, &rc);
			SetWindowPos(GetDlgItem(hwnd,IDC_LIST), HWND_TOP, 0, 0, rc.right, rc.bottom, (SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER));
		}
		return 1;}
	case WM_ACTIVATE:
		#define ADD_OVERLAY_EXSTYLE (WS_EX_TRANSPARENT /*| WS_EX_LAYERED*/ | WS_EX_TOOLWINDOW)
		#define REM_OVERLAY_STYLE (WS_CAPTION | WS_SYSMENU)
		if(LOWORD(wParam) != WA_INACTIVE){
			if(m_watch_flags & WF_OVERLAY) {
				LONG_PTR style;
				style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
				style &= ~ADD_OVERLAY_EXSTYLE;
				SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
				style = GetWindowLongPtr(hwnd, GWL_STYLE);
				style |= REM_OVERLAY_STYLE;
				SetWindowLongPtr(hwnd, GWL_STYLE, style);
				SetWindowPos(hwnd, HWND_TOPMOST_nowarn, 0,0, 0,0, (SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
				SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
				// Win2k fix, window title isn't redrawn
				// XP fix, list view header isn't redrawn
				RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			} else {
				SetWindowPos(hwnd, HWND_TOPMOST_nowarn, 0,0,0,0, (SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
			}
		}else{
			if(m_watch_flags & WF_OVERLAY) {
				LONG_PTR style;
				style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
				style |= ADD_OVERLAY_EXSTYLE;
				SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
				style = GetWindowLongPtr(hwnd, GWL_STYLE);
				style &= ~REM_OVERLAY_STYLE;
				SetWindowLongPtr(hwnd, GWL_STYLE, style);
				SetWindowPos(hwnd, 0, 0,0, 0,0, (SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER));
				SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
			} else {
				SetWindowPos(hwnd, HWND_NOTOPMOST_nowarn, 0,0,0,0, (SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
				// actually it should be lParam, but that's "always" NULL for other process' windows
				SetWindowPos(GetForegroundWindow(), HWND_TOP, 0,0,0,0, (SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE));
			}
		}
		break;
	case WM_TIMER:
		if(!OnWatchTimer(hwnd)) { // When the Last Monitored Timer
//			DestroyWindow(hwnd);
		}
		break;
	case WM_COMMAND: {
		HWND list = GetDlgItem(hwnd, IDC_LIST);
		LVITEM item;
		switch(LOWORD(wParam)) {
		case WATCH_ITEM_SORT:
			item.iItem = 0;
			switch(HIWORD(wParam)) {
			case 1:
				item.iItem = SORT_REMEMBER | SORT_ASC;
				break;
			case 2:
				item.iItem = SORT_REMEMBER | SORT_DEC;
				break;
			case 3:
				item.iItem = SORT_NEXT;
				break;
			}
			if(!item.iItem)
				item.iItem = SORT_NEXT | SORT_ASC;
			switch(lParam) {
			case 0:
				ListView_SortItemsExEx(list, 0, SortString_LV, 0, item.iItem | SORT_INSENSITIVE);
				break;
			case 1:
				ListView_SortItemsExEx(list, 1, SortTime, 0, item.iItem | SORT_CUSTOMPARAM);
				break;
			}
			break;
		case IDM_TIMER_RESTART:
			item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
			if(item.iItem != -1) {
				TimerWatchList_Restart(list, item.iItem);
			}
			break;
		case IDM_TIMER_STOP:
			item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
			if(item.iItem != -1) {
				item.mask = LVIF_PARAM;
				item.iSubItem = 0;
				ListView_GetItem(list, &item);
				TimerEnable((int)item.lParam, 0);
				ListView_DeleteItem(list, item.iItem);
			}
			break;
		case IDM_TIMER_HIDE:
			item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
			if(item.iItem != -1) {
				item.mask = LVIF_PARAM;
				item.iSubItem = 0;
				ListView_GetItem(list, &item);
				TimerWatch_Hide((int)item.lParam, 1);
			}
			break;
		case IDM_TIMER_EDIT:
			item.iItem = ListView_GetNextItem(list, -1, LVNI_SELECTED);
			if(item.iItem != -1) {
				item.mask = LVIF_PARAM;
				item.iSubItem = 0;
				ListView_GetItem(list, &item);
				DialogTimer((int)item.lParam);
			}
			break;
		case IDM_TIMER_CLEANUP:
			ListView_DeleteAllItems(list);
			break;
		case IDM_TIMER_ALL_RESTART:
			TimerWatchList_Restart(list, -1);
			break;
		case IDM_TIMER_RESTORE:
			TimerWatch_Hide(-1, 0);
			break;
		case IDM_TIMER_ALL_START:
			TimerEnable(-1, 1);
			break;
		case IDM_TIMER_ALL_STOP:
			ListView_DeleteAllItems(list);
			TimerEnable(-1, 0);
			break;
		case IDM_TIMER:
			DialogTimer(0);
			break;
		case IDM_TIMER_OPT_OVERLAY:
			m_watch_flags ^= WF_OVERLAY;
			SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
			break;
		case IDM_TIMER_OPT_OPACITY_80:
			if(m_watch_alpha == kOpacity80)
				m_watch_alpha = 255;
			else
				m_watch_alpha = kOpacity80;
			SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
			break;
		case IDM_TIMER_OPT_OPACITY_50:
			if(m_watch_alpha == kOpacity50)
				m_watch_alpha = 255;
			else
				m_watch_alpha = kOpacity50;
			SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
			break;
		case IDM_TIMER_OPT_OPACITY_25:
			if(m_watch_alpha == kOpacity25)
				m_watch_alpha = 255;
			else
				m_watch_alpha = kOpacity25;
			SetLayeredWindowAttributes(hwnd, 0, m_watch_alpha, LWA_ALPHA);
			break;
		case IDCANCEL:
			DestroyWindow(hwnd);
			break;
		}
		break;}
	case WM_NOTIFY:{
		NMHDR* notify = (NMHDR*)lParam;
		if(notify->code == HDN_ITEMCLICK || notify->code == HDN_ITEMDBLCLICK) {
			NMHEADER* col = (NMHEADER*)notify;
			m_last_sorted = (char)col->iItem;
			Window_TimerView(hwnd, WM_COMMAND, MAKEWPARAM(WATCH_ITEM_SORT,0), col->iItem);
			break;
		} else if(notify->code == LVN_KEYDOWN) {
			NMLVKEYDOWN* nmkey = (NMLVKEYDOWN*)notify;
			HWND list = GetDlgItem(hwnd, IDC_LIST);
			switch(nmkey->wVKey) {
			case 'R':
				if(GetAsyncKeyState(VK_SHIFT)&0x8000)
					Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_ALL_RESTART, 0);
				else
					Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_RESTART, 0);
				return 1;
			case 'S':
				Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_STOP, 0);
				return 1;
			case 'H':
				Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_HIDE, 0);
				return 1;
			case 'E':
				Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_EDIT, 0);
				return 1;
			case 'C':
				Window_TimerView(hwnd, WM_COMMAND, IDM_TIMER_CLEANUP, 0);
				return 1;
			case VK_DELETE:{
				int sel = ListView_GetNextItem(list, -1, LVNI_SELECTED);
				if(sel != -1) {
					RemoveFromWatch(hwnd, list, sel);
				}
				return 1;}
			}
		} else if(notify->code == NM_RCLICK) {
			NMITEMACTIVATE* activate = (NMITEMACTIVATE*)notify;
			HMENU menu;
			int num;
			char timer = (activate->iItem >= 0 && activate->iItem <= 0x4000/*likely header*/);
			HWND list = GetDlgItem(hwnd, IDC_LIST);
			MENUITEMINFO menuitem;
			wchar_t str[TNY_BUFF];
			menuitem.cbSize = sizeof(menuitem);
			menuitem.fMask = MIIM_STRING;
			
			menu = LoadMenu(g_instance, MAKEINTRESOURCE(IDR_TIMER));
			if(!timer) {
				for(num=GROUP_TIMER_BEGIN; num<=GROUP_TIMER_END; ++num) {
					RemoveMenu(menu, num, MF_BYCOMMAND);
				}
			} else {
				LVITEM item;
				item.mask = LVIF_PARAM;
				item.iItem = activate->iItem;
				item.iSubItem = 0;
				ListView_GetItem(list, &item);
				if(TimerFindIndexById((int)item.lParam) == -1) {
					menuitem.dwTypeData = L"Remove\tS";
					SetMenuItemInfo(menu, IDM_TIMER_STOP, 0, &menuitem);
					EnableMenuItem(menu, IDM_TIMER_HIDE, MF_GRAYED | MF_BYCOMMAND);
				}
			}
			if(!ListView_GetItemCount(list)) {
				EnableMenuItem(menu, IDM_TIMER_CLEANUP, MF_GRAYED | MF_BYCOMMAND);
				EnableMenuItem(menu, IDM_TIMER_ALL_RESTART, MF_GRAYED | MF_BYCOMMAND);
			}
			num = TimerGetHiddenCount();
			if(num) {
				wsprintf(str, L"Restore hidden (%i)", num);
				menuitem.dwTypeData = str;
				SetMenuItemInfo(menu, IDM_TIMER_RESTORE, 0, &menuitem);
				EnableMenuItem(menu, IDM_TIMER_RESTORE, MF_ENABLED | MF_BYCOMMAND);
			}
			if(!m_timers) {
				EnableMenuItem(menu, IDM_TIMER_ALL_STOP, MF_GRAYED | MF_BYCOMMAND);
			}
			
			
			CheckMenuItem(menu, IDM_TIMER_OPT_OVERLAY, (m_watch_flags & WF_OVERLAY ? MF_CHECKED : 0) | MF_BYCOMMAND);
			CheckMenuItem(menu, IDM_TIMER_OPT_OPACITY_80, (m_watch_alpha == kOpacity80 ? MF_CHECKED : 0) | MF_BYCOMMAND);
			CheckMenuItem(menu, IDM_TIMER_OPT_OPACITY_50, (m_watch_alpha == kOpacity50 ? MF_CHECKED : 0) | MF_BYCOMMAND);
			CheckMenuItem(menu, IDM_TIMER_OPT_OPACITY_25, (m_watch_alpha == kOpacity25 ? MF_CHECKED : 0) | MF_BYCOMMAND);
			
			UpdateTimerMenu(menu);
			// http://support.microsoft.com/kb/135788
			SetForegroundWindow(hwnd);
			GetCursorPos(&activate->ptAction); // Microsoft fails to set it when a header is clicked. We'll also need screen coordinates
			num = TrackPopupMenu(GetSubMenu(menu,0), TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD, activate->ptAction.x, activate->ptAction.y, 0, hwnd, NULL);
			if(num < IDM_I_TIMER){
				Window_TimerView(hwnd, WM_COMMAND, num, 0);
			} else {
				TimerMenuItemClick(menu, num);
			}
			PostMessage(hwnd, WM_NULL, 0, 0);
			DestroyMenu(menu);
		}
		break;}
	}
	return 0;
}
