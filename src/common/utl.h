#ifndef TCLOCK_UTL_H
#define TCLOCK_UTL_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief checks if current process is in admin group
 * \return boolean */
int IsRunAsAdmin();
/**
 * \brief checks if current user is in admin group
 * \return boolean */
int IsUserInAdminGroup();
/**
 * \brief get parent process ID from \c pid
 * \param pid
 * \return parent process ID or 0 on failure
 * \sa GetCurrentProcessId() */
unsigned GetParentProcess(unsigned pid);
// clock related
/**
 * \brief refreshes taskbar and clock */
void RefreshUs();
// unsorted
/**
 * \brief converts a hex string to integer
 * \param p hex string to convert
 * \return parsed integer */
int atox(const char* p);
/**
 * \brief converts a hex string to integer
 * \param p hex string to convert
 * \return parsed integer */
int wtox(const wchar_t* p);
/**
 * \brief converts \p hour from 24h format to 12h
 * \param hour 24h format
 * \return 12h format */
int _24hTo12h(int hour);
/**
 * \brief converts \p hour from 12h format to 24h
 * \param hour 12h format
 * \param pm use when \p hour is (after)noon
 * \return 24h format */
int _12hTo24h(int hour, int pm);
/**
 * \brief forces hwnd to the foreground,
 * even when the current process isn't the foreground process.
 * (hooks into the current foreground process to complete)
 * \param hwnd target window
 * \sa SetForegroundWindow(), SetActiveWindow() */
void ForceForegroundWindow(HWND hwnd);
/**
 * \brief deletes the end of a path from \a path, eg. "C:/out.exe" becomes "C:/"
 * \param[in,out] path path to delete from
 * \sa add_title(), get_title() */
void del_title(wchar_t* path);
/**
 * \brief adds a title to a path, eg. "out.exe" to "C:" results "C:/out.exe"
 * \param[in,out] path to manipulate
 * \param[in] title to add (can be relative or absolute)
 * \sa get_title(), del_title() */
void add_title(wchar_t* path, const wchar_t* title);
/**
 * \brief get the title from path, eg. "out.exe" from "C:/out.exe"
 * \param[out] dst receives the title
 * \param[in] path to get title from
 * \sa add_title(), del_title() */
void get_title(wchar_t* dst, const wchar_t* path);
/**
 * \brief case-insensitively compares if ext matches the file extension of fname
 * \param fname file to test for \a ext
 * \param ext extension to find
 * \return 0 if both extensions match, otherwise the difference */
int ext_cmp(const wchar_t* fname, const wchar_t* ext);
/**
 * \brief adds a C string to a double-zero terminated string list. That is ABC,0,EFG,0,0
 * \param[in,out] list string list to add \a str to
 * \param[in] str C string to add */
void str0cat(wchar_t* list, const wchar_t* str);
/**
 * \brief returns a string with \a id from our string table
 * \param id of string to return
 * \return \c NULL on failure */
wchar_t* MyString(UINT id);
//void Pause(HWND hWnd, LPCTSTR pszArgs);
// HaveSetTimePerms.c
/**
 * \brief checks for \c SetSystemTime() permissions (\c SE_SYSTEMTIME_NAME)
 * \return boolean */
int HaveSetTimePermissions();
/**
 * \brief creates a dialog only if it isn't already there (\c CreateDialogParam)
 * \param[in,out] hwnd "previous" dialog \c HWND to be checked; Set to intermediate state during creation and final \c HWND on return
 * \param[in] hInstance
 * \param[in] lpTemplateName
 * \param[in] hWndParent
 * \param[in] lpDialogFunc
 * \param[in] dwInitParam
 * \return created or previous \p hwnd
 * \sa CreateDialogParam() */
HWND CreateDialogParamOnce(HWND* hwnd, HINSTANCE hInstance, const wchar_t* lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
/**
 * \brief creates an 32bit ARGB bitmap
 * \param hdc target DC of the newly created bitmap
 * \param width new bitmap's width
 * \param height new bitmap's height
 * \return created bitmap or \c NULL on error
 * \sa CreateDIBSection() */
HBITMAP CreateBitmapWithAlpha(HDC hdc, int width, int height);
/**
 * \brief convert \c HICON to 32bit ARGB \c HBITMAP for use by context menus and other controls
 * \param icon
 * \param size icon dimensions or \c 0 for "32x32" \c SM_C*ICON, \c -1 for "16x16" \c SM_C*SMICON, \c -2 for \c SM_C*MENUCHECK
 * \return bitmap or \c NULL on error
 * \sa GetSystemMetrics(), SM_CXICON, SM_CYICON, SM_CXSMICON, SM_CYSMICON */
HBITMAP GetBitmapFromIcon(HICON icon, int size);
#ifdef __cplusplus
}
#endif
#endif // TCLOCK_UTL_H
