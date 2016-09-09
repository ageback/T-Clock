#include "tclock.h"
#include "../common/utl.h"

typedef enum VERSION {
	FRESH = -1, /**< no version / new installation */
	/** \c v2.2.0#84(a507ca5)
	- we no longer use the "FontRotateDirection" as it's replaced by "Angle"
	- timers also work differently
	- text is centered by default */
	_2_2_0,
	/** \c v2.3.0#127(dff0300,63ba670#106)
	- T-Clock file structure changed
	- startup link must be updated */
	_2_3_0,
	/** \c v2.4.0#329(f512dbe)
	- alarms unified to always use 24h format internally
	- added distinct 24h format
	- "new line" format supports switched sides */
	_2_4_0,
	CURRENT, /**< current version */
} VERSION;

static int ParseSettings();
static void ConvertSettings(VERSION ver);
static void FirstTimeSetup(VERSION ver);

int CheckSettings(){
	int ret = ParseSettings(); // do first-time setup or check and convert settings
	InitFormat(); // initialize/reset automated Date/Time format
	CheckMouseMenu(); // add context menu action if undefined
	return ret;
}

enum{
	SFORMAT_NONE		=0x0000,
	SFORMAT_SILENT		=0x0001,
	// show update message box with these:
	SFORMAT_BASIC		=0x0002,
	SFORMAT_EFFICIENT	=0x0004,
	SFORMAT_LESSMEM		=0x0008,
	SFORMAT_FEATURE		=0x0010,
	SFORMAT_TEXTPOSITION=0x0020,
};
static const wchar_t* SFORMAT[]={
	L"different",//SFORMAT_BASIC
	L"more efficient",//SFORMAT_EFFICIENT
	L"using less memory",//SFORMAT_LESSMEM
	L"more feature rich",//SFORMAT_FEATURE
	L"positioning text differently",//SFORMAT_TEXTPOSITION
};

enum{
	SCOMPAT_NONE		=0x0000,
	SCOMPAT_FORMAT		=0x0001,
	SCOMPAT_FORMATALL	=0x0002,
	SCOMPAT_TIMERS		=0x0004,
};
static const wchar_t* SCOMPAT[]={
	L"some clock text options",
	L"all clock text options",
	L"your timers",
};

int ParseSettings(){
	wchar_t msg[1024];
	int updateflags = SFORMAT_NONE;
	int compatibilityflags = SCOMPAT_NONE;
	VERSION ver = api.GetInt(L"", L"Ver", 0);
	
//	api.SetStr(NULL, L"ExePath", api.root);
	api.GetStr(L"Clock", L"Font", msg, 80, L"");
	/// new installation?
	if(!*msg) {
		NONCLIENTMETRICS metrics = {sizeof(NONCLIENTMETRICS)};
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
		api.SetStr(L"Clock", L"Font", metrics.lfCaptionFont.lfFaceName);
		
		if(ver == 0){ // very likely a new installation
			FirstTimeSetup(FRESH);
			return 1;
		}
	}
	
	
	/// old installation, set update flags if any
	switch(ver) {
	case _2_2_0:
		updateflags |= SFORMAT_EFFICIENT|SFORMAT_LESSMEM|SFORMAT_FEATURE|SFORMAT_TEXTPOSITION;
		compatibilityflags |= SCOMPAT_FORMAT|SCOMPAT_TIMERS;
		/* fall through */
		
	case _2_3_0:
		//updateflags |= SFORMAT_SILENT;
		/* fall through */
		
	case _2_4_0:
		updateflags |= SFORMAT_SILENT;
		/* fall through */
		
	case CURRENT:
		break;
		
	default:{
		int ans = MessageBox(NULL,
							L"Seems like you've been using a newer version.\n"
							L"Some settings might not be readable\n"
							L"by this older version and you could loose them.\n"
							L"\n"
							L"Run this version anyway?", L"T-Clock downgraded?", MB_OKCANCEL|MB_ICONINFORMATION|MB_SETFOREGROUND);
		if(ans != IDOK)
			return -1;
		ConvertSettings(ver); // should do nothing, just downgrade our version number
		return 0;}
	}
	
	
	/// check if update is "required"
	if(updateflags){
		int ans;
		if(updateflags!=SFORMAT_SILENT){
			int flags;
			wchar_t* pos = msg;
			pos += wsprintf(pos,
							FMT("T-Clock stores now some of its settings differently.\n")
							FMT("Don't worry though, we'll convert your settings\n")
							FMT("to the new format and nothing should get lost.\n")
							FMT("\n")
							FMT("The new format is simply:"));
			for(flags=updateflags>>1/*ignore SFORMAT_SILENT*/,ans=0; flags; flags>>=1,++ans){
				if(flags&1)
					pos += wsprintf(pos, FMT("\n	+ %s"), SFORMAT[ans]);
			}
			if(compatibilityflags){
				pos += wsprintf(pos, FMT("\n\n")
								FMT("If you want to go back to an old version later on,\n")
								FMT("you could/will lose: (be warned)"));
				for(flags=compatibilityflags,ans=0; flags; flags>>=1,++ans){
					if(flags&1)
						pos += wsprintf(pos, FMT("\n	- %s"), SCOMPAT[ans]);
				}
			}
			pos += wsprintf(pos, FMT("\n\nRun T-Clock and convert settings now?"));
			ans = MessageBox(NULL, msg, L"You've just updated T-Clock", MB_OKCANCEL|MB_ICONINFORMATION|MB_SETFOREGROUND);
		}else // silent update
			ans = IDOK;
		if(ans != IDOK)
			return -1;
		ConvertSettings(ver);
		return 2;
	}
	return 0;
}

static void FixShortInt(const wchar_t* section, const wchar_t* entry) {
	int val = api.GetInt(section, entry, 0);
	if(val)
		api.SetInt(section, entry, (short)val);
}
void ConvertSettings(VERSION ver) {
	wchar_t buf[MAX_PATH];
	int idx, idx2;
	size_t len;
	
	switch(ver){
	case FRESH:
	case _2_2_0:
		// update font rotate (from none,left,right to 0-360 angle)
		api.GetStr(L"Clock", L"FontRotateDirection", buf, _countof(buf), L"");
		switch(*buf){
		case 'L': api.SetInt(L"Clock", L"Angle", 90); break;
		case 'R': api.SetInt(L"Clock", L"Angle", 270); break;
		}
		api.DelValue(L"Clock", L"FontRotateDirection");
		// Reset text position; we now center text automatically (and it's relative to that, not the upper area)
		// All settings were supposed to be one-off converts, not repeating if "Ver" resets, but that wouldn't work here
		api.SetInt(L"Clock", L"ClockHeight", 0);
		api.SetInt(L"Clock", L"ClockWidth", 0);
		api.SetInt(L"Clock", L"HorizPos", 0);
		api.SetInt(L"Clock", L"VertPos", 0);
		// remove "ID" from "Timers" as no longer used
		len = wsprintf(buf, FMT("Timers\\Timer"));
		idx2 = api.GetInt(L"Timers", L"NumberOfTimers", 0);
		for(idx=0; idx<idx2; ){
			wsprintf(buf+len, FMT("%d"), ++idx);
			api.DelValue(buf, L"ID");
		}
		// fix for short values stored improperly as int but read as short. required since commit e4406d0
		FixShortInt(L"Clock", L"LineHeight");
		FixShortInt(L"Taskbar", L"AlphaTaskbar");
		FixShortInt(L"Calendar", L"ViewMonths");
		FixShortInt(L"Calendar", L"ViewMonthsPast");
		/* fall through */
		
	case _2_3_0:
		if(GetStartupFile(NULL,buf)){
			DeleteFile(buf);
			AddStartup(NULL);
		}
		/* fall through */
		
	case _2_4_0:{
		int is12h, hour;
		// convert alarms to use 24h format
		len = wsprintf(buf, FMT("Alarm"));
		idx2 = GetAlarmNum();
		for(idx=0; idx<idx2; ){
			wsprintf(buf+len, FMT("%d"), ++idx);
			is12h = api.GetInt(buf, L"Hour12", 0);
			if(is12h){ // convert to 24h format
				hour = api.GetInt(buf, L"Hour", 12);
				api.SetInt(buf, L"Hour", _12hTo24h(hour,api.GetInt(buf,L"PM",0)));
				api.SetInt(buf, L"12h", is12h);
			}
			api.DelValue(buf, L"Hour12");
			api.DelValue(buf, L"PM");
		}
		// make sure "new line" format was set to one or zero
		/// @note : on next backward incompatible change, remove "Kaigyo" key and this "if"
		if(api.GetInt(L"Format", L"Lf", -1) == -1)
			api.SetInt(L"Format", L"Lf", (api.GetInt(L"Format",L"Kaigyo",0) ? BST_CHECKED : BST_UNCHECKED));
		// convert old 12h switch - h/w(±) -> HH/W
		if(!api.GetInt(L"Format",L"Hour12",1)){
			char converted = 0;
			wchar_t fmt[MAX_FORMAT];
			size_t fmtlen;
			wchar_t* pos;
			fmtlen = api.GetStr(L"Format", L"CustomFormat", fmt, _countof(fmt), L"");
			for(pos=fmt; *pos; ){
				if(pos[0] == '"') {
					do{
						for(++pos; *pos&&*pos++!='"'; );
					}while(*pos == '"');
					if(!*pos)
						break;
				}
				if(pos[0] == 'S'){ // only format that also includes "h"
					int width, padding;
					++pos;
					api.GetFormat((const wchar_t**)&pos, &width, &padding);
					continue;
				}
				if(pos[0] == 'h'){
					++converted;
					pos[0] = 'H';
					if(pos[1] == 'h'){
						pos[1] = 'H';
						++pos;
					}else if(fmtlen+1 < MAX_FORMAT){
						++pos;
						++fmtlen; // we also copy null char
						memmove(pos+1, pos, ((fmtlen-(pos-fmt)) * sizeof pos[0]));
						*pos = 'H';
					}
				}else if(pos[0] == 'w' && (pos[1]=='+'||pos[1]=='-')){
					++converted;
					pos[0] = 'W';
				}
				++pos;
			}
			if(converted){
				api.SetStr(L"Format", L"CustomFormat", fmt);
				if(api.GetInt(L"Format", L"Custom", 0)){
					api.SetStr(L"Format", L"Format", fmt);
				}
			}
		}}
		/* fall through */
		/// @note : on next backward incompatible change, remove unused "Timers/Timer#/Active" value
		
	case CURRENT:
		break;
	}
	FirstTimeSetup(ver);
}

void FirstTimeSetup(VERSION from_version) {
	union{
		uint32_t entryU;
		wchar_t entry[3];
	} u;
	const int bits = (8*sizeof(u.entry[0])); // 16
	HWND hwnd;
	RECT rc;
	
	switch(from_version) {
	case FRESH:
		u.entry[2] = '\0';
		u.entryU = '0' | ('1'<<bits); // left, 1 click
		api.SetInt(REG_MOUSE, u.entry, MOUSEFUNC_SHOWCALENDER);
		u.entryU = '1' | ('1'<<bits); // right, 1 click
		api.SetInt(REG_MOUSE, u.entry, MOUSEFUNC_MENU);
		u.entryU = '2' | ('1'<<bits); // middle, 1 click
		api.SetInt(REG_MOUSE, u.entry, IDM_STOPWATCH);
		
		u.entryU = (api.OS >= TOS_VISTA ? BST_INDETERMINATE : BST_UNCHECKED);
		if(!u.entryU) { /// @todo : XP: measure taskbar height to choose font size and or multiline/singleline (small vs "normal" taskbar)
			hwnd = FindWindowA("Shell_TrayWnd", NULL);
			if(hwnd) {
				GetClientRect(hwnd, &rc);
				if(rc.bottom > rc.right) // vertical taskbar
					u.entryU = BST_CHECKED;
			}
		}
		api.SetInt(L"Format", L"Lf", u.entryU);
		/* fall through */
	case _2_2_0:
	case _2_3_0:
	case _2_4_0:
		/* fall through */
	case CURRENT:
		break;
	}
	api.SetInt(L"", L"Ver", CURRENT);
}
