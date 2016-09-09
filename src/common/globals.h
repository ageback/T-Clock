#ifndef TCLOCK_GLOBAL_H
#define TCLOCK_GLOBAL_H

#define _CRT_SECURE_NO_DEPRECATE 1 // SHUT-UP About the New String Functions Already!!!

#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif

#include <ws2tcpip.h> // getaddrinfo, Winsock2.h
#include <wspiapi.h> // for Windows 2000 mainly
#include <windows.h>  // Required by the fact it runs on Windows.
#include <windowsx.h>// usability macros: Edit_*, ComboBox_*, SubclassWindow, etc.
#include <tchar.h>

#ifndef ComboBox_SetDroppedWidth
#	define ComboBox_SetDroppedWidth(hwndCtl, width) ((int)(DWORD)SNDMSG((hwndCtl),CB_SETDROPPEDWIDTH,(WPARAM)(int)(width),(LPARAM)0))
#endif

#ifndef MCM_SIZERECTTOMIN
#	define MCM_SIZERECTTOMIN (MCM_FIRST+29)
#	define MonthCal_SizeRectToMin(hmc, prc) SNDMSG (hmc, MCM_SIZERECTTOMIN, 0,(LPARAM) (prc))
#endif

#ifndef SEE_MASK_NOASYNC // missing in GCC 4.6.3 (Ubuntu 12.04)
#	define SEE_MASK_NOASYNC SEE_MASK_FLAG_DDEWAIT
#endif // SEE_MASK_NOASYNC

#ifdef __GNUC__
#	define COMPILER_STRING "GCC"
#	include <inttypes.h>
#	define __pragma(x) // MSVC pragmas, safe to ignore since we use them only to fix MSVC bugs...
	static const GUID CLSID_Shell = {0x13709620,0xc279,0x11ce,{0xa4,0x9e,0x44,0x45,0x53,0x54,0,1}};
	static const GUID IID_IShellDispatch4 = {0xefd84b2d,0x4bcf,0x4298,{0xbe,0x25,0xeb,0x54,0x2a,0x59,0xfb,0xda}};
	static const GUID CLSID_DragDropHelper = {0x4657278a,0x411b,0x11d2,{0x83,0x9a,0,0xc0,0x4f,0xd9,0x18,0xd0}};
	static const GUID IID_IDropTargetHelper = {0x4657278b,0x411b,0x11d2,{0x83,0x9a,0,0xc0,0x4f,0xd9,0x18,0xd0}};
	/* Linux is still at 2 or 4 sometimes (4 as of Debian Jessie) */
#	if __MINGW64_VERSION_MAJOR < 4
	/* fix non-conforming 'swprintf' from MSVC without size argument */
#		define swprintf _snwprintf
#	endif
#else // _MSC_VER
#	define COMPILER_STRING "MSVC"
#	define strdup _strdup
#	define wcsdup _wcsdup
#	define ltoa _ltoa
#	define inline __inline
#	define strcasecmp _stricmp
#	define strncasecmp _strnicmp
#	define localtime_r(tt,tm) (localtime_s((tm),(tt)) ? NULL : (tm))
#	define gmtime_r(tt,tm) (gmtime_s((tm),(tt)) ? NULL : (tm))
#	ifndef PRIi64
#		define PRIi64 "I64i"
#		define SCNi64 PRIi64
#	endif
#	ifndef PRIx64
#		define PRIx64 "I64x"
#		define SCNx64 PRIx64
#	endif
#endif // __GNUC__ / _MSC_VER
#define wcscasecmp _wcsicmp
#define wcsncasecmp _wcsnicmp

/* HACK: GCC isn't smart enough to check wchar_t* format functions,
 * so implement a workaround which is activated by defining `PRINTF_TEST`
 * during compilation for a mostly working syntax check */
#ifdef __MINGW32__
	__attribute__((format(ms_printf,2,3)))
	WINUSERAPI int WINAPIV wsprintfA(char* buf, const char* fmt, ...);
	/*
	__attribute__((format(ms_wprintf,2,3)))
	WINUSERAPI int WINAPIV wsprintfW(wchar_t* buf, const wchar_t* fmt, ...);
	__attribute__((format(ms_wprintf,3,4)))
	int swprintf(wchar_t* buf, size_t num, const wchar_t* fmt, ...); // */
#endif
#ifdef PRINTF_TEST
#	define FMT(x) x
#	define wsprintfW(buf,fmt,...) wsprintfA((char*)(buf), fmt, ##__VA_ARGS__)
#	define swprintf(buf,num,fmt,...) snprintf((char*)(buf), num, fmt, ##__VA_ARGS__)
#else
#	ifdef UNICODE
#		define FMT_(x) L ## x
#		define FMT(x) FMT_(x)
#	else
#		define FMT(x) x
#	endif
#endif

#define ARCH_SUFFIX_32 L""
#define ARCH_SUFFIX_64 L"64"
#ifndef _WIN64
#	define ARCH_SUFFIX ARCH_SUFFIX_32
#	define TCLOCK_SUFFIX L""
#else
#	define ARCH_SUFFIX ARCH_SUFFIX_64
#	define TCLOCK_SUFFIX L" x64"
#endif // _WIN64

#define ABT_TITLE L"T-Clock Redux" TCLOCK_SUFFIX L" - " L(VER_SHORT_DOTS) L" build " L(STR(VER_REVISION))
#define ABT_TCLOCK L"T-Clock 2010 is Stoic Joker's rewrite of their code which allows it to run on Windows XP and up. While he removed some of T-Clock's previous functionality. He felt this makes it a more \"Administrator Friendly\" application as it no longer required elevated privileges to run.\n\nT-Clock Redux tries to continue Stoic Joker's efforts."
#define CONF_START L"T-Clock Redux" TCLOCK_SUFFIX
#define CONF_START_OLD L"Stoic Joker's T-Clock 2010" TCLOCK_SUFFIX

#define TC_TOOLTIP L"\"T-Clock\"\\nLDATE"

extern HINSTANCE g_instance;
#include "clock.h"
#include "win2k_compat.h"

#define REG_MOUSE L"Mouse"
enum{ // Drop&File enum / registry settings
	DF_NONE=0,
	DF_RECYCLE, // default
	DF_OPEN,
	DF_COPY,
	DF_MOVE,
};

#define UPDATE_TIMESTAMP L"UpdT" /**< setting name for 64bit timestamp */
#define UPDATE_RELEASE   L"UpdR" /**< setting name for release */
#define UPDATE_BETA      L"UpdB" /**< setting name for beta */

// messages to send the main/helper app
#define MAINMFIRST					MAINM_CLOCKINIT
#define MAINM_CLOCKINIT				(WM_USER)
#define MAINM_ERROR					(WM_USER+1)
#define MAINM_EXIT					(WM_USER+2)
#define MAINM_STOPSOUND				(WM_USER+3)
#define MAINM_BLINKOFF				(WM_USER+4)
#define MAINMLAST					MAINM_BLINKOFF
// messages to send the clock
#define CLOCKM_REFRESHCLOCK					(WM_USER+1)
#define CLOCKM_REFRESHTASKBAR				(WM_USER+2)
#define CLOCKM_BLINK						(WM_USER+3)
#define CLOCKM_COPY							(WM_USER+4)
#define CLOCKM_REFRESHDESKTOP				(WM_USER+5)
//#define CLOCKM_REFRESHCLEARTASKBAR			(WM_USER+6)
#define CLOCKM_REFRESHCLOCKPREVIEW			(WM_USER+7)
#define CLOCKM_REFRESHCLOCKPREVIEWFORMAT	(WM_USER+8)
#define CLOCKM_BLINKOFF						(WM_USER+9)

// Global Buffer Size Labels
#define TNY_BUFF	32
#define MIN_BUFF	64
#define GEN_BUFF	128
#define LRG_BUFF	256
#define MAX_BUFF	1024

#define MAX_FORMAT 256

// font quality / smoothing
#define DEFAULT_QUALITY				0
#define DRAFT_QUALITY				1
#define PROOF_QUALITY				2
#define NONANTIALIASED_QUALITY		3
#define ANTIALIASED_QUALITY			4
#define CLEARTYPE_QUALITY			5
#define CLEARTYPE_NATURAL_QUALITY	6

// own defines to suppress warning C4306 in Windows' API
#define LPSTR_TEXTCALLBACK_nowarn ((LPTSTR)(intptr_t)-1L)
#define HWND_BROADCAST_nowarn ((HWND)(intptr_t)0xffff)
#define HWND_TOPMOST_nowarn ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST_nowarn ((HWND)(intptr_t)-2)
#define HWND_MESSAGE_nowarn ((HWND)(intptr_t)-3)

// suppress warning C4127 about const expression in do{}while(0);
#define FD_SET_nowarn(fd, set) __pragma(warning(suppress:4127)) FD_SET(fd, set);

// socket defines currently missing in Windows
#define s6_addr16 u.Word
#define EAI_AGAIN		WSATRY_AGAIN			/**< A temporary failure in name resolution occurred */
#define EAI_BADFLAGS	WSAEINVAL				/**< An invalid value was provided for the ai_flags member of the pHints parameter */
#define EAI_FAIL		WSANO_RECOVERY			/**< A nonrecoverable failure in name resolution occurred */
#define EAI_FAMILY		WSAEAFNOSUPPORT			/**< The ai_family member of the pHints parameter is not supported */
#define EAI_MEMORY		WSA_NOT_ENOUGH_MEMORY	/**< A memory allocation failure occurred */
#define EAI_NONAME		WSAHOST_NOT_FOUND		/**< The name does not resolve for the supplied parameters or the pNodeName and pServiceName parameters were not provided */
#define EAI_SERVICE		WSATYPE_NOT_FOUND		/**< The pServiceName parameter is not supported for the specified ai_socktype member of the pHints parameter */
#define EAI_SOCKTYPE	WSAESOCKTNOSUPPORT		/**< The ai_socktype member of the pHints parameter is not supported */
#ifndef AI_ADDRCONFIG /* since we didn't define _WIN32_WINNT >= 0x0600 */
#	define AI_ADDRCONFIG	0x00000400
#	define AI_V4MAPPED		0x00000800
#endif

#ifdef __GNUC__
//#	define DLL_EXPORT __attribute__((visibility("default")))
#	define DLL_EXPORT __attribute__((dllexport))
#else
#	define DLL_EXPORT __declspec(dllexport)
#endif

/// displays a message using sprintf()
#define DBGMSG_(fmt,...) __pragma(warning(suppress:4127)) do{static char _dbgbuf[1024]; sprintf(_dbgbuf,fmt,##__VA_ARGS__); MessageBoxA(0,_dbgbuf,"Debug",MB_SYSTEMMODAL|MB_SETFOREGROUND);}while(0)
/// outputs a debug message using sprintf()
#define DBGOUT_(fmt,...) __pragma(warning(suppress:4127)) do{static char _dbgbuf[1024]; sprintf(_dbgbuf,fmt,##__VA_ARGS__); OutputDebugStringA(_dbgbuf);}while(0)
#ifdef _DEBUG
#	define DBGMSG DBGMSG_ /**< DEBUG; \sa DBGMSG_ */
#	define DBGOUT DBGOUT_ /**< DEBUG; \sa DBGOUT_ */
#else
#	define DBGMSG(fmt,...) /**< nop; RELEASE \sa DBGMSG_ */
#	define DBGOUT(fmt,...) /**< nop; RELEASE \sa DBGOUT_ */
#endif // _DEBUG
#ifdef _MSC_VER
#	define COMPILE_ASSERT(condition) {extern char assertion_error[1 / !!(condition)];}
#else
#	define COMPILE_ASSERT(condition) {extern char assertion_error[1 / !!(condition)]; (void)assertion_error;}
#endif

#endif // TCLOCK_GLOBAL_H
