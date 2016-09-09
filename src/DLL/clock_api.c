#include "tcdll.h"
#include "clock_internal.h" // internal clock api functions

#ifdef __GNUC__
#	define SHARED __attribute__((section(".shared"),shared))
#else
#	define SHARED
#	pragma data_seg(".shared")
#endif
// shared variables must be initialized
SHARED wchar_t ms_root[MAX_PATH] = {0}; /**< \sa TClockAPI::root */
SHARED uint16_t ms_root_len = 0; /**< \sa TClockAPI::root_len */
SHARED REGSAM ms_reg_fullaccess = KEY_ALL_ACCESS | KEY_WOW64_64KEY;
SHARED REGSAM ms_reg_read = KEY_READ | KEY_WOW64_64KEY;
SHARED wchar_t ms_inifile[MAX_PATH] = {0};

SHARED HWND gs_hwndTClockMain = NULL;
SHARED HWND gs_hwndClock = NULL;
SHARED HWND gs_tray = NULL;
SHARED HWND gs_taskbar = NULL;
SHARED HWND gs_hwndCalendar = NULL;
SHARED unsigned short gs_tos = 0;
#ifndef __GNUC__
#	pragma data_seg()
#endif

const wchar_t* kConfigName = L"\\T-Clock.ini";

HHOOK m_hhook = NULL;

static ULONGLONG WINAPI GetTickCount64_Wrapper(){
	return GetTickCount();
}
// main.c
LRESULT CALLBACK Hook_CallWndProc(int nCode, WPARAM wParam, LPARAM lParam);

#define ClockAPI(func) Clock_##func,
TClockAPI api = {
	NULL, // hInstance (set in DllMain())
	0, // OS
	10, // desktop_button_size
	ms_root, // root
	0, // root_len
	0, // root_size
	// base
	ClockAPI(Inject)
	ClockAPI(InjectFinalize)
	ClockAPI(Exit)
	// misc
	ClockAPI(GetClock)
	ClockAPI(GetCalendar)
	ClockAPI(Message)
	ClockAPI(PositionWindow)
	NULL, // ClockAPI(GetTickCount64)
	ClockAPI(PathExists)
	ClockAPI(GetFileAndOption)
	ClockAPI(GetColor)
	ClockAPI(On_DWMCOLORIZATIONCOLORCHANGED)
	// registry
	ClockAPI(GetInt)
	ClockAPI(GetInt64)
	ClockAPI(GetIntEx)
	ClockAPI(GetSystemInt)
	ClockAPI(GetStr)
	ClockAPI(GetStrEx)
	ClockAPI(GetSystemStr)
	ClockAPI(SetInt)
	ClockAPI(SetInt64)
	ClockAPI(SetSystemInt)
	ClockAPI(SetStr)
	ClockAPI(SetSystemStr)
	ClockAPI(DelValue)
	ClockAPI(DelSystemValue)
	ClockAPI(DelKey)
	// exec
	ClockAPI(ShellExecute)
	ClockAPI(Exec)
	ClockAPI(ExecElevated)
	ClockAPI(ExecFile)
	// format stuff
	ClockAPI(GetFormat)
	ClockAPI(WriteFormatNum)
	// translation
//	ClockAPI(T)
//	ClockAPI(Translate)
//	ClockAPI(TranslateWindow)
};

static int ForceUTF_16(wchar_t* in_name, off_t file_size) {
	typedef struct UTF16file {
		wchar_t bom;
		wchar_t data[1];
	} UTF16file;
	char* in_data;
	UTF16file* out;
	FILE* in_fp,* out_fp;
	size_t out_len, len;
	wchar_t out_name[MAX_PATH];
	int ret = 0;
	
	in_fp = _wfopen(in_name, L"r+b");
	if(in_fp) {
		wchar_t bom = fgetwc(in_fp);
		if(bom != 0xfeff && bom != 0xfffe) {
			in_data = (char*)malloc(file_size + 1);
			out = (UTF16file*)malloc((file_size * sizeof(wchar_t) * 2) + sizeof(wchar_t)*2);
			if(in_data && out) {
				out->bom = 0xfeff;
				rewind(in_fp);
				file_size = (off_t)fread(in_data, 1, file_size, in_fp);
				if(file_size && !IsTextUnicode(in_data,file_size,NULL)) {
					in_data[file_size] = '\0';
					out_len = MultiByteToWideChar(CP_ACP, 0, in_data, file_size, out->data, file_size*2);
					if(!out_len)
						Clock_Message(NULL, L"MultiByteToWideChar() failed, INI might be damaged", L"T-Clock", MB_ICONERROR, MB_ICONERROR);
					out->data[out_len] = '\0';
				} else { // already Unicode, just assume little endian
					memcpy(out->data, in_data, file_size);
					out_len = file_size / sizeof(wchar_t);
				}
				len = wcslen(in_name);
				memcpy(out_name, in_name, (len*sizeof(wchar_t)));
				if(len < MAX_PATH - 4) {
					wcscpy(out_name+len, L".tmp");
				} else {
					out_name[len-1] = '$';
				}
				out_fp = _wfopen(out_name, L"wb");
				if(out_fp) {
					++out_len;
					out_len ^= fwrite(out, sizeof(wchar_t), out_len, out_fp);
					fclose(out_fp);
					if(!out_len) {
						_wunlink(in_name);
						_wrename(out_name, in_name);
					}
				}
			} else
				ret = 2;
			free(out);
			free(in_data);
		}
		fclose(in_fp);
	}
	return ret;
}

DLL_EXPORT int SetupClockAPI(int version, TClockAPI* _api){
	wchar_t own_path[_countof(ms_root)];
	OSVERSIONINFO osvi = {sizeof(OSVERSIONINFO)};
	HANDLE api_lock;
	struct _stat st;
	
	if(version != CLOCK_API)
		return -1;
	
	//
	// synchronized code
	//
	api_lock = CreateSemaphore(NULL, 1, 1, kConfigName+1);
	if(!api_lock)
		return -2;
	if(WaitForSingleObject(api_lock, 5000) == WAIT_TIMEOUT) {
		CloseHandle(api_lock);
		return -3;
	}
	if(!ms_root_len){ // initialize once. (only use ms_/gs_ variables!!!)
		GetModuleFileName(api.hInstance, own_path, _countof(own_path));
		GetLongPathName(own_path, ms_root, _countof(ms_root));
		del_title(ms_root); del_title(ms_root);
		ms_root_len = (uint16_t)wcslen(ms_root);
		DBGOUT("root: %ls\n", ms_root);
		
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724832%28v=vs.85%29.aspx
		if(GetVersionEx(&osvi)){
			switch(osvi.dwMajorVersion){
			case 0: case 1: case 2: case 3: case 4:
				break;
			case 5: // 2000 - XP
				switch(osvi.dwMinorVersion ){
				case 0:
					gs_tos = TOS_2000; break;
				case 1:
					gs_tos = TOS_XP; break;
				default: // 2+
					gs_tos = TOS_XP_64;
				}
				break;
			case 6: // Vista - Win10(preview)
				switch(osvi.dwMinorVersion ){
				case 0:
					gs_tos = TOS_VISTA; break;
				case 1:
					gs_tos = TOS_WIN7; break;
				case 2:
					gs_tos = TOS_WIN8; break;
				case 3:
					gs_tos = TOS_WIN8_1; break;
				case 4:
					gs_tos = TOS_WIN10; break; // old insider builds
				default:
					gs_tos = TOS_NEWER;
				}
				break;
			case 10: // Win 10 (newer versions)
				gs_tos = TOS_WIN10; // build 9926
				// Windows 10 Anniversary Update (Version 1607)
				//	first build: 11082
				//	breaking build (likely): 14291
				if(osvi.dwBuildNumber >= 14291)
					gs_tos = TOS_WIN10_1;
				break;
			default:
				gs_tos = TOS_NEWER;
			}
		}
		//DBGMSG_("ver: %ld.%ld.%ld.%ld\ngs_tos: %hu", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.dwPlatformId, gs_tos);
		
		if(gs_tos < TOS_XP_64) { // Win2000/XP 32bit fix
			ms_reg_fullaccess &= ~KEY_WOW64_64KEY;
			ms_reg_read &= ~KEY_WOW64_64KEY;
		}
		
		memcpy(ms_inifile, ms_root, (ms_root_len+1)*sizeof(ms_root[0]));
		wcscat(ms_inifile, kConfigName);
		if(_wstat(ms_inifile,&st) != -1) {
			switch(ForceUTF_16(ms_inifile,st.st_size)) {
			case 0:
				break;
			default:
				ReleaseSemaphore(api_lock, 1, NULL);
				CloseHandle(api_lock);
				return -4;
			}
		} else {
			ms_inifile[0] = '\0';
		}
	}
	ReleaseSemaphore(api_lock, 1, NULL);
	CloseHandle(api_lock);
	//
	// end of synchronized code
	//
	
	api.OS = gs_tos;
	api.root_len = ms_root_len;
	api.root_size = (ms_root_len+1) * sizeof(ms_root[0]);
	api.GetTickCount64 = (GetTickCount64_t)GetProcAddress(GetModuleHandleA("kernel32"), "GetTickCount64");
	if(!api.GetTickCount64)
		api.GetTickCount64 = GetTickCount64_Wrapper;
	
	if(gs_tos >= TOS_WIN10) {
		api.desktop_button_size = 4+1;
	} else if(gs_tos >= TOS_WIN8) {
		api.desktop_button_size = 8;
	} // default 10
	
	if(_api) // NULL if internally called
		memcpy(_api, &api, sizeof(TClockAPI));
	
//	#if defined(_DEBUG)
	{
		HMODULE exchndl;
		#ifdef _WIN64
		#	define EXCHNDL_PATH L"\\.debug\\64\\exchndl"
		#else
		#	define EXCHNDL_PATH L"\\.debug\\exchndl"
		#endif
		memcpy(own_path, ms_root, api.root_size);
		memcpy(own_path + ms_root_len, EXCHNDL_PATH, sizeof(EXCHNDL_PATH));
		exchndl = GetModuleHandleA("exchndl");
		if(!exchndl)
			exchndl = LoadLibraryEx(own_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH); // intended leak
		if(exchndl) {
			typedef BOOL (APIENTRY *pExcHndlSetLogFileNameA)(const char* szLogFileName);
			char logpath[MAX_PATH];
			pExcHndlSetLogFileNameA ExcHndlSetLogFileNameA = (pExcHndlSetLogFileNameA)GetProcAddress(exchndl, "ExcHndlSetLogFileNameA");
			if(ExcHndlSetLogFileNameA) {
				memcpy(own_path + ms_root_len, L"\\Crash.log", (10+1)*sizeof(wchar_t));
				WideCharToMultiByte(CP_ACP, 0, own_path, ms_root_len+(10+1), logpath, _countof(logpath), NULL, NULL);
				ExcHndlSetLogFileNameA(logpath);
			}
		}
	}
//	#endif
	return 0;
}

void Clock_Inject(HWND hwnd)
{
	HWND hwndBar, hwndClock;
	DWORD dwThreadId;
	
	hwndClock = Clock_GetClock(1);
	gs_hwndTClockMain = hwnd;
	if(gs_hwndClock && IsWindow(gs_hwndClock) && gs_hwndClock==hwndClock){
		SendMessage(gs_hwndTClockMain, MAINM_CLOCKINIT, 0, (LPARAM)gs_hwndClock);
		return; // already hooked / old instance
	}
	gs_hwndClock = NULL;
	
	// find the taskbar
	hwndBar = FindWindowA("Shell_TrayWnd", NULL);
	if(!hwndBar) {
		SendMessage(gs_hwndTClockMain, MAINM_ERROR, 0, 1);
		return;
	}
	
	// get thread ID of taskbar (explorer) - Specal thanks to T.Iwata.
	dwThreadId = GetWindowThreadProcessId(hwndBar, NULL);
	
	if(!dwThreadId) {
		SendMessage(gs_hwndTClockMain, MAINM_ERROR, 0, 2);
		return;
	}
	
	// install an hook to thread of taskbar
	m_hhook = SetWindowsHookEx(WH_CALLWNDPROC, Hook_CallWndProc, api.hInstance, dwThreadId);
	if(!m_hhook) {
		SendMessage(gs_hwndTClockMain, MAINM_ERROR, 0, 3);
		return;
	}
	
	// refresh the taskbar
	PostMessage(hwndBar, WM_SIZE, SIZE_RESTORED, 0);
	
	SendMessage(hwndClock, WM_NULL, 0, 0);
}

void Clock_InjectFinalize()
{
	// uninstall my hook
	if(m_hhook){
		UnhookWindowsHookEx(m_hhook);
		m_hhook=NULL;
	}
}

void Clock_Exit()
{
	Clock_InjectFinalize(); // uninstall hook helper if any
	if(gs_hwndClock && IsWindow(gs_hwndClock)){
		HANDLE lock;
		SendMessage(gs_hwndClock,WM_COMMAND,IDM_EXIT,0); // kill our clock
		lock = OpenSemaphore(SYNCHRONIZE|SEMAPHORE_MODIFY_STATE, 0, kConfigName+1);
		WaitForSingleObject(lock, INFINITE);
		ReleaseSemaphore(lock, 1, NULL);
		CloseHandle(lock);
		Sleep(1); // hopefully useless sleep
		
		gs_taskbar = gs_tray = NULL;
		gs_hwndClock = NULL;
	}
}
