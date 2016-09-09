#ifndef TCLOCK_NEWAPI_H
#define TCLOCK_NEWAPI_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief unloads all new API functions and removes \c WS_EX_LAYERED from the taskbar of our clock \a hwndClock
 * \param hwndClock clock window (used only from within DLL) \e [optional]
 * \sa SetLayeredTaskbar() */
void EndNewAPI(HWND hwndClock);
/**
 * \brief checks if we're running under WoW64 (32bit app on 64bit OS)
 * \return boolean */
int IsWow64();
/**
 * \brief sets the taskbar \c WS_EX_LAYERED to enable transparency (when user chose to)
 * \param hwndClock clock window
 * \param opacity percent opacity
 * \param clear_taskbar make \c COLOR_3DFACE the transparent color
 * \sa EndNewAPI() */
void SetLayeredTaskbar(HWND hwndClock, int opacity, int clear_taskbar);
//void TC2DrawBlt(HDC dhdc, int dx, int dy, int dw, int dh, HDC shdc, int sx, int sy, int sw, int sh, BOOL useTrans);

// DrawTheme
#define VSCLASS_CLOCK L"Clock"
#define VSCLASS_TASKBAND2 L"TaskBand2" /**< Win10+ */

#ifndef CLP_TIME
#	define CLP_TIME 1
#	define CLS_NORMAL 1
#	define CLS_HOT 2
#	define CLS_PRESSED 3
//#	define TMT_COLOR 204
#	define TMT_BACKGROUND 1602
#	define TMT_WINDOWTEXT 1609
#	define TMT_CAPTIONTEXT 1610
#	define TMT_BTNTEXT 1619
#	define TMT_INFOTEXT 1624
#	define TMT_TEXTCOLOR 3803

#	define TMT_TRANSPARENTCOLOR 3809
#	define TMT_WINDOW 1606
#	define TMT_WINDOWFRAME 1607
#	define TMT_FILLCOLOR 3802
#endif

/**
 * \brief get current theme color for our clock text
 * \param hwndClock handle to our clock
 * \param state one of \c CLS_NORMAL, \c CLS_HOT or \c CLS_PRESSED
 * \return COLORREF with our text color
 * \sa GetXPClockColorBG(), ReloadXPClockTheme(), CLS_NORMAL, CLS_HOT, CLS_PRESSED */
COLORREF GetXPClockColor(HWND hwndClock, int state);
/**
 * \brief get current theme color for our clock background
 * \param hwndClock handle to our clock
 * \param state one of \c CLS_NORMAL, \c CLS_HOT or \c CLS_PRESSED
 * \return COLORREF with our background color
 * \remarks might not work properly on XP
 * \sa GetXPClockColor(), ReloadXPClockTheme(), CLS_NORMAL, CLS_HOT, CLS_PRESSED */
COLORREF GetXPClockColorBG(HWND hwndClock, int state);
/**
 * \brief reloads current theme information used by \c GetXPClockColor() and \c GetXPClockColorBG()
 * \sa GetXPClockColor(), GetXPClockColorBG() */
void ReloadXPClockTheme();
/**
 * \brief is theme engine active (always true for Win8+? or does it indicate that we're not running a high contrast theme?)
 * \return boolean
 * \sa SetXPWindowTheme(), GetXPClockColor(), GetXPClockColorBG() */
BOOL IsXPThemeActive();
/**
 * \brief enable 3rd party theme for window \a hwnd
 * \param hwnd target window
 * \param pszSubAppName other application's theme to use. Eg. "Explorer"
 * \param pszSubIdList theme ID to use. Default: \c NULL
 * \return \c S_OK on success, otherwise returns an \c HRESULT error code.
 * \sa SetWindowTheme() */
HRESULT SetXPWindowTheme(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
/**
 * \brief draw parent background for \a hwnd
 * \param hwnd window to paint
 * \param hdc \c DC of window to paint
 * \param prc \c RECT structure with region to paint
 * \sa DrawXPClockHover(), DrawThemeParentBackground() */
void DrawXPClockBackground(HWND hwnd, HDC hdc, RECT* prc);
/**
 * \brief draw parent background hover for \a hwnd using \c CLP_TIME and \c CLS_HOT
 * \param hwnd window to paint hover background
 * \param hdc \c DC of window to paint
 * \param prc \c RECT structure with region to paint
 * \sa DrawXPClockBackground(), DrawThemeParentBackground() */
void DrawXPClockHover(HWND hwnd, HDC hdc, RECT* prc);

// UAC and stuff

#ifndef BCM_SETSHIELD /* _WIN32_WINNT >= 0x0600 */
#	define BCM_SETSHIELD            (BCM_FIRST + 0x000C)
/** \brief add or remove a UAC shield icon for a button \remark this macro is only useful on Windows Vista+ */
#	define Button_SetElevationRequiredState(hwnd, fRequired) \
		(LRESULT)SNDMSG((hwnd), BCM_SETSHIELD, 0, (LPARAM)fRequired)
#endif

#ifndef MSGFLT_ADD
#	define MSGFLT_ADD 1
#	define MSGFLT_REMOVE 2
#endif
typedef BOOL (WINAPI* ChangeWindowMessageFilter_t)(UINT message, DWORD dwFlag); ///< type of \c ChangeWindowMessageFilter() found in \c user32 since Vista

#ifndef SHGSI_ICONLOCATION
	typedef struct SHSTOCKICONINFO {
		DWORD cbSize;
		HICON hIcon;
		int iSysImageIndex;
		int iIcon;
		WCHAR szPath[MAX_PATH];
	} SHSTOCKICONINFO;

	#define SHGSI_ICONLOCATION 0
	#define SHGSI_ICON SHGFI_ICON
	#define SHGSI_SYSICONINDEX SHGFI_SYSICONINDEX
	#define SHGSI_LINKOVERLAY SHGFI_LINKOVERLAY
	#define SHGSI_SELECTED SHGFI_SELECTED
	#define SHGSI_LARGEICON SHGFI_LARGEICON
	#define SHGSI_SMALLICON SHGFI_SMALLICON
	#define SHGSI_SHELLICONSIZE SHGFI_SHELLICONSIZE

	typedef enum SHSTOCKICONID {
		SIID_DOCNOASSOC = 0,
		SIID_DOCASSOC = 1,
		SIID_APPLICATION = 2,
		SIID_FOLDER = 3,
		SIID_FOLDEROPEN = 4,
		SIID_DRIVE525 = 5,
		SIID_DRIVE35 = 6,
		SIID_DRIVEREMOVE = 7,
		SIID_DRIVEFIXED = 8,
		SIID_DRIVENET = 9,
		SIID_DRIVENETDISABLED = 10,
		SIID_DRIVECD = 11,
		SIID_DRIVERAM = 12,
		SIID_WORLD = 13,
		SIID_SERVER = 15,
		SIID_PRINTER = 16,
		SIID_MYNETWORK = 17,
		SIID_FIND = 22,
		SIID_HELP = 23,
		SIID_SHARE = 28,
		SIID_LINK = 29,
		SIID_SLOWFILE = 30,
		SIID_RECYCLER = 31,
		SIID_RECYCLERFULL = 32,
		SIID_MEDIACDAUDIO = 40,
		SIID_LOCK = 47,
		SIID_AUTOLIST = 49,
		SIID_PRINTERNET = 50,
		SIID_SERVERSHARE = 51,
		SIID_PRINTERFAX = 52,
		SIID_PRINTERFAXNET = 53,
		SIID_PRINTERFILE = 54,
		SIID_STACK = 55,
		SIID_MEDIASVCD = 56,
		SIID_STUFFEDFOLDER = 57,
		SIID_DRIVEUNKNOWN = 58,
		SIID_DRIVEDVD = 59,
		SIID_MEDIADVD = 60,
		SIID_MEDIADVDRAM = 61,
		SIID_MEDIADVDRW = 62,
		SIID_MEDIADVDR = 63,
		SIID_MEDIADVDROM = 64,
		SIID_MEDIACDAUDIOPLUS = 65,
		SIID_MEDIACDRW = 66,
		SIID_MEDIACDR = 67,
		SIID_MEDIACDBURN = 68,
		SIID_MEDIABLANKCD = 69,
		SIID_MEDIACDROM = 70,
		SIID_AUDIOFILES = 71,
		SIID_IMAGEFILES = 72,
		SIID_VIDEOFILES = 73,
		SIID_MIXEDFILES = 74,
		SIID_FOLDERBACK = 75,
		SIID_FOLDERFRONT = 76,
		SIID_SHIELD = 77,
		SIID_WARNING = 78,
		SIID_INFO = 79,
		SIID_ERROR = 80,
		SIID_KEY = 81,
		SIID_SOFTWARE = 82,
		SIID_RENAME = 83,
		SIID_DELETE = 84,
		SIID_MEDIAAUDIODVD = 85,
		SIID_MEDIAMOVIEDVD = 86,
		SIID_MEDIAENHANCEDCD = 87,
		SIID_MEDIAENHANCEDDVD = 88,
		SIID_MEDIAHDDVD = 89,
		SIID_MEDIABLURAY = 90,
		SIID_MEDIAVCD = 91,
		SIID_MEDIADVDPLUSR = 92,
		SIID_MEDIADVDPLUSRW = 93,
		SIID_DESKTOPPC = 94,
		SIID_MOBILEPC = 95,
		SIID_USERS = 96,
		SIID_MEDIASMARTMEDIA = 97,
		SIID_MEDIACOMPACTFLASH = 98,
		SIID_DEVICECELLPHONE = 99,
		SIID_DEVICECAMERA = 100,
		SIID_DEVICEVIDEOCAMERA = 101,
		SIID_DEVICEAUDIOPLAYER = 102,
		SIID_NETWORKCONNECT = 103,
		SIID_INTERNET = 104,
		SIID_ZIPFILE = 105,
		SIID_SETTINGS = 106,
		
		SIID_DRIVEHDDVD = 132,
		SIID_DRIVEBD = 133,
		SIID_MEDIAHDDVDROM = 134,
		SIID_MEDIAHDDVDR = 135,
		SIID_MEDIAHDDVDRAM = 136,
		SIID_MEDIABDROM = 137,
		SIID_MEDIABDR = 138,
		SIID_MEDIABDRE = 139,
		SIID_CLUSTEREDDRIVE = 140,
		
		SIID_MAX_ICONS = 175
	} SHSTOCKICONID;
#endif // SHGSI_ICONLOCATION

typedef HRESULT (* SHGetStockIconInfo_t)(SHSTOCKICONID siid, UINT uFlags, SHSTOCKICONINFO* psii); ///< type of \c SHGetStockIconInfo() found in \c shell32 since Vista
/**
 * \brief custom function to get a stock icon using \c SHGetStockIconInfo()
 * \param siid requested icon
 * \param flag anyone of \c SHGSI_LARGEICON \c SHGSI_SMALLICON \c SHGSI_SHELLICONSIZE, \c 0 equals \c SHGSI_LARGEICON
 * \return icon handle or \c NULL on unsupported platforms; use \c DestroyIcon() to free it
 * \remark comes with a pre-Vista fallback for \c SIID_WARNING, \c SIID_INFO, \c SIID_ERROR and \c SIID_SHIELD
 * \sa SHSTOCKICONID, SHGSI_LARGEICON, SHGSI_SMALLICON, SHGSI_SHELLICONSIZE, DestroyIcon() */
HICON GetStockIcon(SHSTOCKICONID siid, unsigned flag);

#ifdef __cplusplus
}
#endif
#endif // TCLOCK_NEWAPI_H
