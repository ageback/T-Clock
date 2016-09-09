#include "tcdll.h"
#include "clock_internal.h"
#include <sys/stat.h>

static const wchar_t m_regkey[] = L"Software\\Stoic Joker's\\T-Clock 2010"; /**< our registry key root */
static const size_t m_regkey_size = _countof(m_regkey); /**< size of \c m_regkey incl. trailing null \sa m_regkey */
static const wchar_t kInvalidKey[] = L"\1\b";

// misc

HWND Clock_GetClock(int uncached) {
	char classname[80];
	HWND taskbar, child;
	if(!uncached && gs_hwndClock && IsWindow(gs_hwndClock))
		return gs_hwndClock;
	
	taskbar = FindWindowA("Shell_TrayWnd", NULL);
	// find the clock window
	for(child=GetWindow(taskbar,GW_CHILD); child; child=GetWindow(child,GW_HWNDNEXT)) {
		GetClassNameA(child, classname, _countof(classname));
		if(!strcmp(classname,"TrayNotifyWnd")) {
			for(child=GetWindow(child,GW_CHILD); child; child=GetWindow(child,GW_HWNDNEXT)) {
				GetClassNameA(child, classname, _countof(classname));
				if(!strcmp(classname,"TrayClockWClass"))
					return child;
			}
			break;
		}
	}
	return NULL;
}
HWND Clock_GetCalendar() {
	HWND hwnd = FindWindowExA(NULL,NULL,"ClockFlyoutWindow",NULL);
	if(hwnd)
		return hwnd;
	hwnd = FindWindowExA(NULL, NULL, "Windows.UI.Core.CoreWindow", "Date and Time Information");
	if(hwnd){ // starts "invisible" full-size and becomes re-sized/moved and "visible" later
		union{
			RECT rc;
			POINT pt;
		} u;
		GetWindowRect(hwnd, &u.rc);
		// following is somehow required... 2px is enough though
		u.pt.x += 25;
		u.pt.y += 25;
		// IsWindowVisible()/IsWindowEnabled() are always true
		if(ChildWindowFromPointEx(GetDesktopWindow(), u.pt, CWP_SKIPDISABLED|CWP_SKIPTRANSPARENT) == hwnd)
			return hwnd;
	}
	return gs_hwndCalendar;
}

int Clock_Message(HWND parent, const wchar_t* msg, const wchar_t* title, UINT uType, UINT uBeep)
{
	MSGBOXPARAMS mbp = {sizeof(MSGBOXPARAMS)};
	mbp.hwndOwner = parent;
	mbp.hInstance = api.hInstance;
	mbp.lpszText = msg;
	mbp.lpszCaption = title;
	mbp.dwStyle = MB_USERICON | uType;
	mbp.lpszIcon = MAKEINTRESOURCE(IDI_MAIN);
	mbp.dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
	if(uBeep != 0xFFFFFFFF) MessageBeep(uBeep);
	return MessageBoxIndirect(&mbp);
}

void Clock_PositionWindow(HWND hwnd, int padding) {
	HMONITOR monitor;
	POINT cursor_pos;
	MONITORINFO moni = {sizeof(moni)};
	int wProp, hProp;
	HWND hwnd_clock;
	#define horizontal moni.cbSize
	
	GetWindowRect(hwnd, &moni.rcWork); // Options dialog dimensions
	wProp = moni.rcWork.right-moni.rcWork.left;  //----------+++--> Width
	hProp = moni.rcWork.bottom-moni.rcWork.top; //----------+++--> Height
	
	GetCursorPos(&cursor_pos);
	monitor = MonitorFromPoint(cursor_pos,MONITOR_DEFAULTTONEAREST);
	GetMonitorInfo(monitor, &moni);
	
	horizontal = 1;
	if(moni.rcWork.top!=moni.rcMonitor.top || moni.rcWork.bottom!=moni.rcMonitor.bottom) { // taskbar is horizontal
		moni.rcMonitor.left=moni.rcWork.right-wProp-padding;
		if(moni.rcWork.top!=moni.rcMonitor.top) // top
			moni.rcMonitor.top=moni.rcWork.top+padding;
		else // bottom
			moni.rcMonitor.top=moni.rcWork.bottom-hProp-padding;
	}else if(moni.rcWork.left!=moni.rcMonitor.left || moni.rcWork.right!=moni.rcMonitor.right){ // vertical
		horizontal = 0;
		moni.rcMonitor.top=moni.rcWork.bottom-hProp-padding;
		if(moni.rcWork.left!=moni.rcMonitor.left) // left
			moni.rcMonitor.left=moni.rcWork.left+padding;
		else // right
			moni.rcMonitor.left=moni.rcWork.right-wProp-padding;
	}else{ // autohide taskbar
		MONITORINFO taskbarMoni;
		HWND taskbar = FindWindowA("Shell_TrayWnd", NULL);
		RECT taskbarRC;
		int tbW,tbH;
		if(!taskbar)
			return;
		taskbarMoni.cbSize=sizeof(moni);
		GetMonitorInfo(MonitorFromWindow(taskbar,MONITOR_DEFAULTTONEAREST),&taskbarMoni); // correct monitor for single monitor setup, probably wrong for multimon
		GetWindowRect(taskbar,&taskbarRC);
		tbW=taskbarRC.right-taskbarRC.left;
		tbH=taskbarRC.bottom-taskbarRC.top;
		if(tbW > tbH){ // horizontal
			int diff=taskbarMoni.rcMonitor.top-taskbarRC.top;
			int visiblesize=taskbarMoni.rcMonitor.bottom-taskbarMoni.rcMonitor.top+diff;
			moni.rcMonitor.left=moni.rcWork.right-wProp-padding;
			if((diff>0 && diff>tbH/10) || !diff || (diff<0 && (visiblesize!=tbH && visiblesize>tbH/10))) // top
				moni.rcMonitor.top=moni.rcWork.top+padding+tbH;
			else // bottom
				moni.rcMonitor.top=moni.rcWork.bottom-hProp-padding-tbH;
		}else{
			int diff=taskbarMoni.rcMonitor.left-taskbarRC.left;
			int visiblesize=taskbarMoni.rcMonitor.right-taskbarMoni.rcMonitor.left+diff;
			horizontal = 0;
			moni.rcMonitor.top=moni.rcWork.bottom-hProp-padding;
			if((diff>0 && diff>tbW/10) || !diff || (diff<0 && (visiblesize!=tbW && visiblesize>tbW/10))) // left
				moni.rcMonitor.left=moni.rcWork.left+padding+tbW;
			else // right
				moni.rcMonitor.left=moni.rcWork.right-wProp-padding-tbW;
		}
	}
	
	// center small windows within clock dimension if possible
	hwnd_clock = Clock_GetClock(0);
	if(hwnd_clock) {
		int offset;
		GetWindowRect(hwnd_clock, &moni.rcWork);
		if(horizontal) {
			offset = ((moni.rcWork.right - moni.rcWork.left - wProp)>>1);
			if(MonitorFromWindow(hwnd_clock,MONITOR_DEFAULTTONEAREST) == monitor) {
				if((moni.rcMonitor.left - offset) > moni.rcWork.left)
					moni.rcMonitor.left = moni.rcWork.left + offset;
			} else {
				offset += api.desktop_button_size;
				if((moni.rcMonitor.left + wProp - offset) < moni.rcMonitor.right)
					moni.rcMonitor.left -= offset;
			}
		} else {
			offset = ((moni.rcWork.bottom - moni.rcWork.top - hProp)>>1);
			if(MonitorFromWindow(hwnd_clock,MONITOR_DEFAULTTONEAREST) == monitor) {
				if((moni.rcMonitor.top - offset) > moni.rcWork.top)
					moni.rcMonitor.top = moni.rcWork.top + offset;
			} else {
				offset += api.desktop_button_size;
				if((moni.rcMonitor.top + hProp - offset) < moni.rcMonitor.bottom)
					moni.rcMonitor.top -= offset;
			}
		}
	}
	#undef horizontal
	SetWindowPos(hwnd,HWND_TOP,moni.rcMonitor.left,moni.rcMonitor.top,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
}

#ifndef S_ISDIR
#	define S_ISDIR(mode) (mode&S_IFDIR)
#endif // S_ISDIR
int Clock_PathExists(const wchar_t* path){
	struct _stat st;
	if(_wstat(path, &st)==-1) return 0;
	return S_ISDIR(st.st_mode)?2:1;
}

int Clock_GetFileAndOption(const wchar_t* command, wchar_t* app, wchar_t* params) {
	int invalid_path = 0;
	const wchar_t* offset_params = NULL;
	const wchar_t* offset = command;
	wchar_t* out = app;
	
	for(;;) {
		if(*offset <= ' ') { // any non-printable char, incl. null
			*out = '\0';
			if(Clock_PathExists(app) == 1){
				offset_params = offset;
			}else{
				if(offset-command <= MAX_PATH-5){
					wcscpy(out, L".exe");
					if(Clock_PathExists(app) == 1)
						offset_params = offset;
					*out = '\0';
				}
			}
			for(; *offset == ' '; *out++ = *offset++);
			if(!*offset)
				break;
		}
		if(*offset == '/'){ // Windows still fails to work with "/" and relative paths
			++offset;
			*out++ = '\\';
			continue;
		}
		*out++ = *offset++;
	}
	if(!offset_params){ // no valid file found
		invalid_path = 1;
		offset_params = wcschr(command, ' ');
		if(!offset_params) // entire command
			offset_params = offset;
	}
	out[offset_params-offset] = '\0';
	
	if(params){
		for(; *offset_params == ' ';  ++offset_params);
		wcscpy(params, offset_params);
	}
	return invalid_path;
}

// registry

#ifdef RegDeleteKeyEx
#	undef RegDeleteKeyEx
#endif // RegDeleteKeyEx
#define RegDeleteKeyEx MyRegDeleteKeyEx
static LONG WINAPI MyRegDeleteKeyEx(HKEY hKey,wchar_t* lpSubKey,REGSAM samDesired,DWORD Reserved){
	typedef LONG (WINAPI* RegDeleteKeyEx_t)(HKEY hKey,wchar_t* lpSubKey,REGSAM samDesired,DWORD Reserved);
	static RegDeleteKeyEx_t pRegDeleteKeyEx=NULL;
	if(gs_tos >= TOS_XP_64){
		if(!pRegDeleteKeyEx)
			pRegDeleteKeyEx = (RegDeleteKeyEx_t)GetProcAddress(GetModuleHandleA("advapi32"), "RegDeleteKeyExW");
		if(pRegDeleteKeyEx)
			return pRegDeleteKeyEx(hKey,lpSubKey,samDesired,Reserved);
	}
	return RegDeleteKey(hKey,lpSubKey);
}

/**
 * \brief prepares \a key string for registry functions by prefixing \a section with \c m_regkey
 * \param[out] key resulting key to query or manipulate
 * \param[in] section
 * \return boolean
 * \remarks call once for every Get/Set* function and before using \c m_inifile
 * \sa m_regkey, m_inifile */
static int PrepareMyRegKey_(wchar_t key[80], const wchar_t* section) {
	size_t section_len = (section ? wcslen(section)+1 : 0);
	
	if(m_regkey_size+section_len > 80){
		#ifdef _DEBUG
		MessageBoxA(NULL, "settings key too huge", "PrepareMyRegKey", MB_SETFOREGROUND);
		#endif
		return 0;
	}
	
	if(ms_inifile[0]){
		if(section_len > 1)
			memcpy(key, section, section_len*sizeof(wchar_t));
		else
			wcscpy(key, L"Main");
		return 1;
	}

	memcpy(key, m_regkey, m_regkey_size*sizeof(wchar_t));
	if(section_len > 1){
		key[m_regkey_size-1] = '\\';
		memcpy(key+m_regkey_size, section, section_len*sizeof(wchar_t));
	}
	return 1;
}

static int GetInt_(HKEY rootkey, const wchar_t* section, const wchar_t* entry, int defval, int* val) {
	wchar_t key[80];
	HKEY hkey;
	int result = -1;
	
	if(!rootkey && PrepareMyRegKey_(key,section)){
		section = key;
		if(ms_inifile[0]) {
			wchar_t val[11+1]; // -2147483648 \0
			GetPrivateProfileString(section, entry, kInvalidKey, val, _countof(val), ms_inifile);
			if(val[0] != kInvalidKey[0] || val[1] != kInvalidKey[1]) {
				result = 0;
				defval = _wtoi(val);
			}
		} else {
			rootkey = HKEY_CURRENT_USER;
		}
	}
	if(rootkey && RegOpenKeyEx(rootkey,section,0,ms_reg_read,&hkey) == ERROR_SUCCESS) {
		int val;
		DWORD regtype, size = sizeof(val);
		if(RegQueryValueEx(hkey,entry,0,&regtype,(BYTE*)&val,&size)==ERROR_SUCCESS && regtype==REG_DWORD) {
			result = 0;
			defval = val;
		}
		RegCloseKey(hkey);
	}
	*val = defval;
	return result;
}

int Clock_GetSystemInt(HKEY rootkey, const wchar_t* section, const wchar_t* entry, int defval) {
	int val;
	GetInt_(rootkey, section, entry, defval, &val);
	return val;
}

int Clock_GetInt(const wchar_t* section, const wchar_t* entry, int defval) {
	int val;
	GetInt_(0, section, entry, defval, &val);
	return val;
}

int64_t Clock_GetInt64(const wchar_t* section, const wchar_t* entry, int64_t defval) {
	wchar_t key[80];
	HKEY hkey;
	
	if(PrepareMyRegKey_(key,section)){
		if(ms_inifile[0]) {
			wchar_t s[16+1]; // max: 19+1+1(decimal), 16+1(hexadecimal)
			int ret = GetPrivateProfileString(key, entry, L"", s, _countof(s), ms_inifile);
			if(ret)
				swscanf(s, _T("%") _T(SCNx64), &defval);
		} else {
			if(RegOpenKeyEx(HKEY_CURRENT_USER,key,0,ms_reg_read,&hkey) == ERROR_SUCCESS) {
				int64_t val;
				DWORD regtype, size = sizeof(val);
				if(RegQueryValueEx(hkey,entry,0,&regtype,(BYTE*)&val,&size)==ERROR_SUCCESS && regtype==REG_QWORD)
					defval = val;
				RegCloseKey(hkey);
			}
		}
	}
	return defval;
}

int Clock_GetIntEx(const wchar_t* section, const wchar_t* entry, int defval) {
	int val;
	if(GetInt_(0, section, entry, defval, &val) != 0)
		Clock_SetInt(section, entry, defval);
	return val;
}

int Clock_GetSystemStr(HKEY rootkey, const wchar_t* section, const wchar_t* entry, wchar_t* val, int len, const wchar_t* defval) {
	HKEY hkey;
	DWORD regtype, size;
	int ret = -1;
	
	if(RegOpenKeyEx(rootkey,section,0,ms_reg_read,&hkey) == ERROR_SUCCESS) {
		size = len * sizeof(val[0]);
		if(RegQueryValueEx(hkey, entry, 0, &regtype, (BYTE*)val, &size) == ERROR_SUCCESS) {
			ret = size / sizeof(val[0]);
			if(ret) {
				--ret;
				if(val && val[ret] != '\0') {
					if(ret+1 < len) ++ret;
					val[ret] = '\0';
				}
			}
		}
		RegCloseKey(hkey);
	}
	if(val) {
		if(ret == -1 && defval) {
			ret = (int)wcslen(defval);
			if(ret >= len)
				ret = len - 1;
			memcpy(val, defval, ret * sizeof(val[0]));
			val[ret] = '\0';
		}
		if(ret <= 0)
			val[0] = '\0';
	}
	return ret;
}

static int GetPrivateProfileStringLength(const wchar_t* section, const wchar_t* entry, int len_expected, const wchar_t* ini) {
	wchar_t dummy[3];
	wchar_t* dummy_large;
	int ret;
	ret = GetPrivateProfileString(section, entry, kInvalidKey, dummy, _countof(dummy), ini);
	if(ret == (_countof(dummy) - 1) && len_expected) {
		ret = (len_expected % 1024);
		if(ret)
			len_expected += (1024 - ret);
		ret = len_expected;
		for(;;){
			dummy_large = malloc(len_expected * sizeof(wchar_t));
			if(!dummy_large)
				break;
			ret = GetPrivateProfileString(section, entry, NULL, dummy_large, len_expected, ini);
			free(dummy_large);
			if(ret != (len_expected - 1))
				break;
			len_expected += 1024;
		}
	} else if(dummy[0] == kInvalidKey[0] && dummy[1] == kInvalidKey[1]) {
		ret = -1;
	}
	return ret;
}
int Clock_GetStr(const wchar_t* section, const wchar_t* entry, wchar_t* val, int len, const wchar_t* defval) {
	wchar_t key[80];
	int ret = -1;
	
	if(PrepareMyRegKey_(key,section)){
		if(ms_inifile[0]) {
			if(val) {
				ret = GetPrivateProfileString(key, entry, kInvalidKey, val, len, ms_inifile);
				if(val[0] == kInvalidKey[0] && val[1] == kInvalidKey[1])
					ret = -1;
			} else {
				ret = GetPrivateProfileStringLength(key, entry, len, ms_inifile);
			}
		} else {
			return Clock_GetSystemStr(HKEY_CURRENT_USER, key, entry, val, len, defval);
		}
	}
	if(val) {
		if(ret == -1 && defval) {
			ret = (int)wcslen(defval);
			if(ret >= len)
				ret = len - 1;
			memcpy(val, defval, ret * sizeof(val[0]));
			val[ret] = '\0';
		}
		if(ret <= 0)
			val[0] = '\0';
	}
	return ret;
}

int Clock_GetStrEx(const wchar_t* section, const wchar_t* entry, wchar_t* val, int len, const wchar_t* defval) {
	int ret = Clock_GetStr(section, entry, val, len, NULL);
	if(ret == -1){
		Clock_SetStr(section, entry, defval);
		ret = (int)wcslen(defval);
		if(ret >= len)
			ret = len - 1;
		if(val) {
			memcpy(val, defval, ret * sizeof(val[0]));
			val[ret] = '\0';
		}
	}
	return ret;
}

int Clock_SetSystemInt(HKEY rootkey, const wchar_t* section, const wchar_t* entry, int val) {
	HKEY hkey;
	int ret = 0;
	
	if(RegCreateKeyEx(rootkey,section,0,NULL,0,ms_reg_fullaccess,NULL,&hkey,NULL) == ERROR_SUCCESS) {
		if(RegSetValueEx(hkey, entry, 0, REG_DWORD, (const BYTE*)&val, sizeof(val)) == ERROR_SUCCESS) {
			ret = 1;
		}
		RegCloseKey(hkey);
	}
	return ret;
}

int Clock_SetInt(const wchar_t* section, const wchar_t* entry, int val) {
	wchar_t key[80];
	
	if(!PrepareMyRegKey_(key,section))
		return 0;
	if(ms_inifile[0]) {
		wchar_t s[11+1]; // -2147483648 \0
		swprintf(s, _countof(s), FMT("%d"), val);
		return WritePrivateProfileString(key, entry, s, ms_inifile);
	}
	return Clock_SetSystemInt(HKEY_CURRENT_USER, key, entry, val);
}

int Clock_SetInt64(const wchar_t* section, const wchar_t* entry, int64_t val) {
	wchar_t key[80];
	HKEY hkey;
	int ret = 0;
	
	if(PrepareMyRegKey_(key,section)){
		if(ms_inifile[0]) {
			wchar_t s[16+1]; // max: 19+1+1(decimal), 16+1(hexadecimal)
			swprintf(s, _countof(s), FMT("%") FMT(PRIx64), val);
			return WritePrivateProfileString(key, entry, s, ms_inifile);
		} else {
			if(RegCreateKeyEx(HKEY_CURRENT_USER,key,0,NULL,0,ms_reg_fullaccess,NULL,&hkey,NULL) == ERROR_SUCCESS) {
				if(RegSetValueEx(hkey, entry, 0, REG_QWORD, (const BYTE*)&val, sizeof(val)) == ERROR_SUCCESS) {
					ret = 1;
				}
				RegCloseKey(hkey);
			}
		}
	}
	return ret;
}

int Clock_SetSystemStr(HKEY rootkey, const wchar_t* section, const wchar_t* entry, const wchar_t* val) {
	HKEY hkey;
	int ret = 0;
	
	if(RegCreateKeyEx(rootkey,section,0,NULL,0,ms_reg_fullaccess,NULL,&hkey,NULL) == ERROR_SUCCESS) {
		if(RegSetValueEx(hkey, entry, 0, REG_SZ, (const BYTE*)val, (DWORD)(wcslen(val)*sizeof(val[0])+sizeof(val[0]))) == ERROR_SUCCESS) {
			ret = 1;
		}
		RegCloseKey(hkey);
	}
	return ret;
}

int Clock_SetStr(const wchar_t* section, const wchar_t* entry, const wchar_t* val) {
	wchar_t key[80];
	if(!PrepareMyRegKey_(key,section))
		return 0;
	
	if(ms_inifile[0]) {
		size_t size = wcslen(val);
		if(size && (val[0] < '0' || val[size-1] <= 0x20)) {
			int ret;
			wchar_t* val_mod;
			size = (wcslen(val)+1) * sizeof(val[0]);
			val_mod = (wchar_t*)malloc(size + 2*sizeof(val[0]));
			val_mod[0] = '"';
			memcpy(val_mod+1, val, size);
			size /= sizeof(val[0]);
			val_mod[size] = '"';
			val_mod[1 + size] = '\0';
			ret = WritePrivateProfileString(key, entry, val_mod, ms_inifile);
			free(val_mod);
			return ret;
		}
		return WritePrivateProfileString(key, entry, val, ms_inifile);
	}
	return Clock_SetSystemStr(HKEY_CURRENT_USER, key, entry, val);
}

int Clock_DelSystemValue(HKEY rootkey, const wchar_t* section, const wchar_t* entry) {
	HKEY hkey;
	int ret = 0;
	if(RegOpenKeyEx(rootkey,section,0,ms_reg_fullaccess,&hkey) == ERROR_SUCCESS) {
		if(RegDeleteValue(hkey, entry) == ERROR_SUCCESS)
			ret = 1;
		RegCloseKey(hkey);
	}
	return ret;
}

int Clock_DelValue(const wchar_t* section, const wchar_t* entry) {
	wchar_t key[80];
	if(!PrepareMyRegKey_(key,section))
		return 0;
	
	if(ms_inifile[0])
		return WritePrivateProfileString(key, entry, NULL, ms_inifile);
	return Clock_DelSystemValue(HKEY_CURRENT_USER, key, entry);
}

int Clock_DelKey(const wchar_t* section) {
	wchar_t key[80];
	
	if(!PrepareMyRegKey_(key,section))
		return 0;
	if(ms_inifile[0])
		return WritePrivateProfileString(key, NULL, NULL, ms_inifile);
	return RegDeleteKeyEx(HKEY_CURRENT_USER,key,KEY_WOW64_64KEY,0) == ERROR_SUCCESS;
}

// exec

int Clock_ShellExecute(const wchar_t* method, const wchar_t* app, const wchar_t* params, HWND parent, int show, HANDLE* hProcess) {
	int ret = -1;
	SHELLEXECUTEINFO sei = {sizeof(sei)};
	sei.hwnd = parent;
	sei.lpVerb = method;
	sei.lpFile = app;
	sei.lpParameters = params;
	sei.nShow = show;
	if(*app){
		if(hProcess)
			sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
		if(ShellExecuteEx(&sei)) {
			ret = 0;
		}else if(GetLastError() == ERROR_CANCELLED) {// UAC dialog user canceled
			ret = 1;
		}
	}
	if(hProcess)
		*hProcess = sei.hProcess;
	return ret;
}
int Clock_Exec(const wchar_t* app, const wchar_t* params, HWND parent) {
	return Clock_ShellExecute(NULL, app, params, parent, SW_SHOWNORMAL, NULL);
}
int Clock_ExecElevated(const wchar_t* app, const wchar_t* params, HWND parent)
{
	return Clock_ShellExecute(L"runas", app, params, parent, SW_SHOWNORMAL, NULL);
}
int Clock_ExecFile(const wchar_t* command, HWND parent) {
	wchar_t app[MAX_PATH], params[MAX_PATH];
	if(!command[0])
		return -1;
	// if(parent) SetForegroundWindow(parent);
	Clock_GetFileAndOption(command,app,params);
	return Clock_ShellExecute(NULL,app,(params[0]?params:NULL),parent,SW_SHOWNORMAL, NULL);
}

// format stuff

wchar_t Clock_GetFormat(const wchar_t** offset, int* minimum, int* padding) {
	wchar_t specifier;
	const wchar_t* pos = *offset;
	int pad = 0;
	int min = 0;
	// padding
	for(; *pos=='_'; ++pad,++pos);
	specifier = *pos;
	if(specifier){
		// min / zeros
		do{
			++min; ++pos;
		}while(*pos == specifier);
	}
	//
	*padding = pad;
	*offset = pos;
	*minimum = min;
	return specifier;
}
int Clock_WriteFormatNum(wchar_t* buffer, int number, int minimum, int padding) {
	wchar_t* out = buffer;
	wchar_t negative = '\0';
	int num, nums = 1;
	if(number < 0){
		number = -number;
		negative = '-';
	}
	// count chars && init
	for(num=number; num>=10; ++nums,num/=10);
	minimum -= nums;
	padding += (minimum<0 ? minimum : 0);
	// write padding
	for(; padding>0; --padding)
		*out++ = ' ';
	// minus sign
	if(negative)
		*out++ = negative;
	// write zero padding
	for(; minimum>0; --minimum)
		*out++ = '0';
	// write number
	_ltow(number, out, 10);
	out += nums;
	return (int)(out - buffer);
}
