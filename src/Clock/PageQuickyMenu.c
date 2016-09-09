//============================================================================
//  PageQuickyMenu.c  -  Stoic Joker 2006  ====================================
//==============================================================================
#include "tclock.h"
//------------------------------------------------------------------------------
/* Modified by Stoic Joker: Sunday, 03/14/2010 @ 10:48:18AM */

void BrowseForTargetFile(HWND);
void DeleteMenuItem(HWND);
void SaveNewMenuOptions(HWND hDlg);

#define SendPSChanged(hDlg) SendMessage(GetParent(hDlg), PSM_CHANGED, (WPARAM)(hDlg), 0)
/*-------------------------------------------------
End edit and go back to Quicky-list
-------------------------------------------------*/
void EndQuickyEdit(HWND hDlg)
{
	HWND hParent=GetParent(hDlg);
	int i;
	for(i=IDC_QMEN_GROUP1; i<=IDC_QMEN_LIST; ++i) {
		ShowDlgItem(hParent,i,1);
	}
	SendMessage(hParent,WM_COMMAND,IDM_QMEM_REFRESH,0);
	SetFocus(GetDlgItem(hParent,IDC_QMEN_LIST));
	DestroyWindow(hDlg);
}
//===============================================================================================
//----------------------------------+++--> Dialog Procedure for Menu Item Details Dialog Messages:
INT_PTR CALLBACK Page_QuickyMenu(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)   //-++-->
{
	(void)lParam;
	switch(message) {
	case WM_INITDIALOG:{
		wchar_t tmp[LRG_BUFF];
		int idx = (int)lParam;
		HWND hParent = GetParent(hDlg);
		SetWindowLongPtr(hDlg, GWLP_USERDATA, idx);
		wsprintf(tmp, FMT("Item #%i"), idx+1);
		SetDlgItemText(hDlg, IDC_MID_TASKNUM, tmp);
		ListView_GetItemText(GetDlgItem(hParent,IDC_QMEN_LIST), idx, 1, tmp, _countof(tmp));
		SetDlgItemText(hDlg, IDC_MID_TARGET, tmp);
		if(!tmp[0]){ // new item
			EnableDlgItem(hDlg, IDC_MID_DELETE, 0);
		}else
			ListView_GetItemText(GetDlgItem(hParent,IDC_QMEN_LIST), idx, 0, tmp, _countof(tmp));
		SetDlgItemText(hDlg, IDC_MID_MENUTEXT, tmp);
		ListView_GetItemText(GetDlgItem(hParent,IDC_QMEN_LIST), idx, 2, tmp, _countof(tmp));
		SetDlgItemText(hDlg, IDC_MID_SWITCHES, tmp);
		SetFocus(GetDlgItem(hDlg,IDC_MID_MENUTEXT));
		return TRUE;}
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDC_MID_TARGETSEL:
			BrowseForTargetFile(hDlg);
			break;
		case IDC_MID_SAVE:
			SaveNewMenuOptions(hDlg);
			break;
		case IDC_MID_DELETE:
			DeleteMenuItem(hDlg);
			/* fall through */
		case IDC_MID_CANCEL:
			EndQuickyEdit(hDlg);
		}
		return TRUE;
	}
	return FALSE;
}
/*-------------------------------------------------------------
Save the New Menu Item Options - From the Menu Item Details Tab
-------------------------------------------------------------*/
void SaveNewMenuOptions(HWND hDlg)
{
	/// @note : on next backward incompatible change, also change how we store QuickyMenu
	wchar_t key[TNY_BUFF];
	int offset = 9;
	wchar_t szmText[TNY_BUFF];
	wchar_t szmTarget[LRG_BUFF];
	wchar_t szmSwitches[LRG_BUFF];
	GetDlgItemText(hDlg, IDC_MID_MENUTEXT, szmText, _countof(szmText));
	GetDlgItemText(hDlg, IDC_MID_TARGET,   szmTarget, _countof(szmTarget));
	GetDlgItemText(hDlg, IDC_MID_SWITCHES, szmSwitches, _countof(szmSwitches));
	memcpy(key, L"MenuItem-", offset*sizeof(wchar_t));
	offset += wsprintf(key+offset, FMT("%i"), GetWindowLong(hDlg,GWLP_USERDATA));
	if((wcslen(szmText)) && (wcslen(szmTarget))) {
		api.SetInt(L"QuickyMenu\\MenuItems", key, 1);
		
		wcscpy(key+offset, L"-Text");
		api.SetStr(L"QuickyMenu\\MenuItems", key, szmText);
		
		wcscpy(key+offset, L"-Target");
		api.SetStr(L"QuickyMenu\\MenuItems", key, szmTarget);
		
		wcscpy(key+offset, L"-Switches");
		api.SetStr(L"QuickyMenu\\MenuItems", key, szmSwitches);
		
		EndQuickyEdit(hDlg);
	} else {
		DeleteMenuItem(hDlg);
		wsprintf(szmSwitches, FMT("%s%s"),
			(!wcslen(szmText)?L"* Menu text can't be empty!\n":L""),
			(!wcslen(szmTarget)?L"* Target file must be filled!":L""));
		MessageBox(0, szmSwitches, L"ERROR: Missing Information!", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	}
}
/*---------------------------------------------------------------------------
-------------- DELETE a Menu Item - From the ListBox Control and the Registry
---------------------------------------------------------------------------*/
void DeleteMenuItem(HWND hDlg)
{
	wchar_t key[TNY_BUFF];
	int offset=9;
	memcpy(key, L"MenuItem-", offset * sizeof key[0]);
	offset += wsprintf(key+offset, FMT("%i"), GetWindowLong(hDlg,GWLP_USERDATA));
	api.DelValue(L"QuickyMenu\\MenuItems", key);
	
	wcscpy(key+offset, L"-Text");
	api.DelValue(L"QuickyMenu\\MenuItems", key);
	
	wcscpy(key+offset, L"-Target");
	api.DelValue(L"QuickyMenu\\MenuItems", key);
	
	wcscpy(key+offset, L"-Switches");
	api.DelValue(L"QuickyMenu\\MenuItems", key);
}
//================================================================================================
//-------------------------------------//------+++--> Browse to the Quicky Menu Item's Target File:
void BrowseForTargetFile(HWND hBft)   //----------------------------------------------------+++-->
{
	wchar_t szFile[MAX_PATH];
	
	if(SelectMyFile(hBft, L"Program Files (*.exe)\0*.exe\0" L"All Files (*.*)\0*.*\0", 0, NULL, szFile)) {
		SetDlgItemText(hBft, IDC_MID_TARGET, szFile);
	}
}
