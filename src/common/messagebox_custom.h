#ifndef MESSAGEBOXCUSTOM_H_
#define MESSAGEBOXCUSTOM_H_
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif

/** \brief send by \c MessageBoxCustom() to request MessageBoxCustomData structure.
 * \c wParam and \c lParam are unused
 * \sa MessageBoxCustom(), MessageBoxCustomData */
#define WMBC_INITDIALOG (WM_USER + 0x450)
/** send by \c MessageBoxCustom() before returning to inform about check states
 * \c wParam is a set of bits to indicate the check box states
 * \c wParam&1 = check1, \c wParam&2 = check2, \c wParam&4 = check3 ...
 * \sa MessageBoxCustom() */
#define WMBC_CHECKS     (WM_USER + 0x451)

/** use raw button style. Eg. BS_MBC_DEFAULT */
#define BS_MBC_RAW     (BS_MBC_RAWF | WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON)
#define BS_MBC_RAWF     0x00080000

/** auto disable checkbox based on previous checkbox */
#define BST_MBC_AUTODISABLE   0x00010000
/** use raw checkbox styles */
#define BST_MBC_RAW_STYLE     0x00100000

#define ID_MBC1 1000
#define ID_MBC2 1001
#define ID_MBC3 1002
#define ID_MBC4 1003

#define MBC_MAX_CHECKBOXES 2
#define MBC_CHECK1 1004
#define MBC_CHECK2 1005
#define MBC_CHECK_END (MBC_CHECK1 + MBC_MAX_CHECKBOXES)
typedef struct MessageBoxCustomData MessageBoxCustomData;

/**
 * \brief displays a customizable MessageBox which sends callbacks to its parent
 * \param parent required parent window that receives \c WMBC_INITDIALOG and \c WMBC_CHECKS callbacks
 * \param message
 * \param title
 * \param style message box style flags such as \c MB_ICONINFORMATION|MB_DEFBUTTON3
 * \return \c -1/0 on failure, on success one of \c ID_MBC1 to \c ID_MBC4 or \c close_id
 * \remark sends the callback message \c WMBC_INITDIALOG on creation to request \c MessageBoxCustomData structure
 * \remark set \c MessageBoxCustomData.close_id to desired value when user closes dialog (defaults to \c IDCANCEL)
 * \remark set \c MessageBoxCustomData.icon_title* to custom window icon \e (optional)
 * \remark set \c MessageBoxCustomData.icon_text to custom text icon \e (optional)
 * \sa MessageBoxCustomData, MB_ICONEXCLAMATION, MB_ICONWARNING, MB_ICONINFORMATION, MB_ICONASTERISK, MB_ICONQUESTION, MB_ICONSTOP, MB_ICONERROR, MB_ICONHAND, MB_DEFBUTTON1, MB_APPLMODAL, MB_SYSTEMMODAL, MB_TASKMODAL, MB_TOPMOST */
int MessageBoxCustom(HWND parent, const wchar_t* message, const wchar_t* title, unsigned style /* = MB_DEFBUTTON1 */);
/**
 * \brief works like \c MessageBoxCustom() but doesn't require a parent window
 * \param[in,out] settings settings to use; receives updated checkbox states on return
 * \param[in] message
 * \param[in] title
 * \param[in] style message box style flags such as \c MB_ICONINFORMATION|MB_DEFBUTTON3
 * \return \c -1/0 on failure, on success one of \c ID_MBC1 to \c ID_MBC4 or \c close_id
 * \sa MessageBoxCustom() */
int MessageBoxCustom_Direct(MessageBoxCustomData* settings, const wchar_t* message, const wchar_t* title, unsigned style /* = MB_DEFBUTTON1 */);

typedef struct MBC_Button {
	const wchar_t* text;
	HICON icon; /**< additional icon for the button */
	int style;  /**< button styles or zero for default */
} MBC_Button;

typedef struct MBC_Check {
	const wchar_t* text;
	RECT pos;  /**< checkbox offset and size in dialog units (left: left padding, top: vertical padding, right: width, bottom: height */
	int state; /**< checked states \sa BST_CHECKED, BST_UNCHECKED, BST_MBC_AUTODISABLE, BST_MBC_RAW_STYLE */
	int style; /**< checkbox styles to add/overwrite \sa BST_MBC_RAW_STYLE */
} MBC_Check;

struct MessageBoxCustomData {
	HICON icon_title_big; /**< custom title icon or NULL for parent's */
	HICON icon_title_small;
	HICON icon_text;      /**< custom message icon or NULL for default */
	MBC_Button button[4]; /**< \sa MBC_Button */
	MBC_Check check[MBC_MAX_CHECKBOXES]; /**< \sa MBC_Check */
	int close_id; /**< default close/ESC control ID */
};

#ifdef __cplusplus
}
#endif
#endif // MESSAGEBOXCUSTOM_H_
