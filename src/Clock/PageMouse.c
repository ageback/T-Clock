/*--------------------------------------------------------
//-------------------+++--> pagemouse.c - KAZUBON 1997-1998
//-------------------------------------------------------*/
// Modified by Stoic Joker: Saturday, March 6, 2010 - 8:11:17pm
#include "tclock.h"
#include <shlobj.h>//SHBrowseForFolder

static void OnInit(HWND hDlg);
static void OnApply(HWND hDlg);
static void OnDestroy();
/** \brief browse for a file or folder triggered by control \p id
 * \param hDlg
 * \param id control id of browse button
 * \remark \p id - 1 must be an edit control or dropdown that receives the chosen file
 * \remark When \p id equals \c IDC_DROPFILESAPPSANSHO , it'll browse for either a file or folder depending on current selection in \c IDC_DROPFILES
 * \sa IDC_DROPFILESAPPSANSHO, IDC_MOUSEFILEBROWSE */
static void OnFileBrowse(HWND hDlg, WORD id);
static void InitMouseControls(HWND hDlg);

static const wchar_t* m_mouseButton[]={
	L"Left",
	L"Right",
	L"Middle",
	L"Button 4",
	L"Button 5",
};
static const short m_mouseButtonCount = _countof(m_mouseButton);
static const wchar_t* m_mouseClick[]={
	L"Single",
	L"Double",
};
//static const int m_mouseClickCount = _countof(m_mouseClick);
#define m_mouseClickCount _countof(m_mouseClick)
typedef struct{
	const int id;
	const wchar_t* name;
} action_t;
static const action_t m_mouseAction[]={
//	{MOUSEFUNC_NONE,L"<unknown>"},
	{MOUSEFUNC_MENU,L"Context Menu"},
	{MOUSEFUNC_TIMER,L"Timer"},
	{IDM_TIMEWATCH,L"Timer watch"},
	{MOUSEFUNC_CLIPBOARD,L"Copy date/time by format ->"},
	{MOUSEFUNC_EXEC,L"Execute command ->"},
	{MOUSEFUNC_SCREENSAVER,L"Screensaver"},
	{MOUSEFUNC_SHOWCALENDER,L"Calendar"},
	{MOUSEFUNC_SHOWPROPERTY,L"T-Clock Options"},
	{IDM_RECYCLEBIN,L"Open Recycle Bin"},
	{IDM_RECYCLEBIN_PURGE,L"Empty Recycle Bin"},
	{IDM_STOPWATCH,L"Stopwatch"},
	{IDM_PROP_ALARM,L"Alarms"},
	{IDM_SNTP,L"Time Sync (SNTP) options"},
	{IDM_SNTP_SYNC,L"Synchronize time"},
	{IDM_FWD_DATETIME,L"Adjust date/time"},
	{IDM_DATETIME_EX,L"Adjust date/time ex"},
	{IDM_FWD_STACKED,L"Show windows stacked"},
	{IDM_FWD_SIDEBYSIDE,L"Show windows side by side"},
	{IDM_FWD_UNDO,L"Undo last window change"},
};
static const int m_mouseActionCount = _countof(m_mouseAction);

//----------------------+++--> Mouse Click Date Configuration,
typedef struct { //--+++--> Manipulation, & Storage Structure.
	int func[m_mouseClickCount];
	wchar_t data[m_mouseClickCount][MAX_PATH];
} CLICKDATA;
static CLICKDATA* m_pData=NULL;

static char m_bTransition=0;
static void SendPSChanged(HWND hDlg){
	if(!m_bTransition)
		SendMessage(GetParent(hDlg), PSM_CHANGED, (WPARAM)(hDlg), 0);
}

//========================================================================================
//------------------------------------------------------+++--> Update UI, listview control:
static void UpdateUIList(HWND hDlg, int selButton, int selClick)   //---+++-->
{
	HWND hList=GetDlgItem(hDlg,IDC_LIST);
	LVITEM lvItem; // ListView item		Mouse Buttons:
	int button;  // mouse button				0=>Left, 1=>Right, 2=>Middle, 
	int click; // click count					3=>XButton 1, 4=>XButton 2
	(void)hDlg;
	lvItem.mask=LVIF_TEXT;
	lvItem.iItem=0;
	ListView_DeleteAllItems(hList); // cleanup ListView
	/*
	if(api.GetInt(REG_MOUSE,L"DropFiles",DF_RECYCLE)){
		char szApp[LRG_BUFF];
		lvItem.iSubItem=0;
		lvItem.pszText="Drag";
		ListView_InsertItem(hList, &lvItem);
		
		++lvItem.iSubItem;
		lvItem.pszText="DropFiles";
		ListView_SetItem(hList,&lvItem);
		
		++lvItem.iSubItem;
		lvItem.pszText=szApp;
		GetDlgItemText(hDlg,IDC_DROPFILES,szApp,LRG_BUFF);
		ListView_SetItem(hList,&lvItem);
		
		++lvItem.iSubItem;
		GetDlgItemText(hDlg,IDC_DROPFILESAPP,szApp,LRG_BUFF);
		ListView_SetItem(hList,&lvItem);
		++lvItem.iItem;
	}// */
	for(button=0; button<m_mouseButtonCount; ++button){
		for(click=0; click<m_mouseClickCount; ++click){
			int func=m_pData[button].func[click];
			if(func){
				lvItem.iSubItem = 0;
				lvItem.pszText = (wchar_t*)m_mouseButton[button];// we set it, so it's "const"
				ListView_InsertItem(hList, &lvItem);
				
				++lvItem.iSubItem;
				lvItem.pszText = (wchar_t*)m_mouseClick[click];
				ListView_SetItem(hList, &lvItem);
				
				++lvItem.iSubItem;
				lvItem.pszText = L"<unknown>";
				#ifdef _DEBUG
				{wchar_t* leak=malloc(16*sizeof(leak[0])); wsprintf(leak,FMT("#%i"),func); lvItem.pszText=leak;}
				#endif // _DEBUG
				{int iter; for(iter=0; iter<m_mouseActionCount; ++iter){
					if(func != m_mouseAction[iter].id)
						continue;
					lvItem.pszText = (wchar_t*)m_mouseAction[iter].name;
					break;
				}}
				ListView_SetItem(hList,&lvItem);
				
				++lvItem.iSubItem;
				if(func == MOUSEFUNC_CLIPBOARD || func <= MOUSEFUNCEXTRA_BEGIN)
					lvItem.pszText = m_pData[button].data[click];
				else
					lvItem.pszText = L"";
				ListView_SetItem(hList,&lvItem);
				if(button==selButton && click==selClick)
					ListView_SetItemState(hList,lvItem.iItem,LVIS_FOCUSED|LVIS_SELECTED,0x0F);
				++lvItem.iItem; // increment item index
			}
		} //- end of click count for
	} //-//- end of mouse button for
}
//====================================================================================================
//----------------------------------------------+++--> Update UI controls: combo boxes and radiobutton:
static void UpdateUIControls(HWND hDlg, int button, int click, int type)   //-------+++-->
{
	HWND btn_cb = GetDlgItem(hDlg, IDC_MOUSEBUTTON);
	HWND func_cb = GetDlgItem(hDlg, IDC_MOUSEFUNC);
	int func;
	if(m_bTransition) return;
	m_bTransition=1; // start transition
	if(button==-1){
		button=ComboBox_GetCurSel(btn_cb);
	}else{
		ComboBox_SetCurSel(btn_cb,button);
		if(click==-1){
			for(click=0; click<m_mouseClickCount && !m_pData[button].func[click]; ++click);
		}
	}
	if(click==-1){
		for(click=0; click<m_mouseClickCount && !IsDlgButtonChecked(hDlg,IDC_RADSINGLE+click); ++click);
	}
	if(click==m_mouseClickCount)
		click=0;
	CheckRadioButton(hDlg,IDC_RADSINGLE,IDC_RADDOUBLE,IDC_RADSINGLE+click);
	if(type==1){
		func = (int)ComboBox_GetItemData(func_cb,ComboBox_GetCurSel(func_cb));
		m_pData[button].func[click] = func;
	}else{
		int iter;
		func = m_pData[button].func[click];
		for(iter=0; iter<m_mouseActionCount+1; ++iter) {
			if(func==ComboBox_GetItemData(func_cb,iter)) {
				ComboBox_SetCurSel(func_cb,iter);
				break;
			}
		}
	}
	if(func == MOUSEFUNC_CLIPBOARD || func <= MOUSEFUNCEXTRA_BEGIN){
		EnableDlgItem(hDlg, IDC_MOUSEFILE, 1);
		if(!*m_pData[button].data[click] && func == MOUSEFUNC_CLIPBOARD)
			api.GetStr(L"Format", L"Format", m_pData[button].data[click], _countof(m_pData[button].data[click]), L"");
	}else
		EnableDlgItem(hDlg, IDC_MOUSEFILE, 0);
	EnableDlgItem(hDlg, IDC_MOUSEFILEBROWSE, (func <= MOUSEFUNCEXTRAFILE_BEGIN));
	SetDlgItemText(hDlg, IDC_MOUSEFILE, m_pData[button].data[click]);
	if(type!=2)
		UpdateUIList(hDlg,button,click); // little recursion here, will call UpdateUIControls later on selection change
	m_bTransition=0; // end transition
}
//================================================================================================
//-------------------------------------------+++--> Dialog Procedure for Mouse Tab Dialog Messages:
INT_PTR CALLBACK Page_Mouse(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)   //-----+++-->
{
	switch(message){
	case WM_INITDIALOG:{
		OnInit(hDlg);
		return TRUE;}
	case WM_DESTROY:
		OnDestroy(hDlg);
		break;
	case WM_COMMAND:{
		HWND control = (HWND)lParam;
		WORD id=LOWORD(wParam);
		WORD code=HIWORD(wParam);
		// "Drop files"
		if(id == IDC_DROPFILES && code == CBN_SELCHANGE){
			int iter, sel;
			sel = ComboBox_GetCurSel(control);
			SetDlgItemText(hDlg, IDC_LABDROPFILESAPP, MyString(sel>=3?IDS_LABFOLDER:IDS_LABPROGRAM));
			for(iter=IDC_LABDROPFILESAPP; iter<=IDC_DROPFILESAPPSANSHO; ++iter)
				EnableDlgItem(hDlg,iter,(sel>=2 && sel<=4));
			if(!m_bTransition){
				UpdateUIControls(hDlg,-1,-1,0);
				g_bApplyClock=1;
				SendPSChanged(hDlg);
			}
		// drop file path
		}else if(id==IDC_DROPFILESAPP && code==EN_CHANGE){
			g_bApplyClock=1;
			SendPSChanged(hDlg);
		}else if(id == IDC_DROPFILESAPPSANSHO){
			OnFileBrowse(hDlg, id);
		//  button
		}else if(id == IDC_MOUSEBUTTON && code == CBN_SELCHANGE){
			int button=ComboBox_GetCurSel(control);
			UpdateUIControls(hDlg,button,-1,0);
		// clicks
		}else if(id >= IDC_RADSINGLE && id <= IDC_RADDOUBLE){
			UpdateUIControls(hDlg,-1,id-IDC_RADSINGLE,0);
		//  Mouse Function
		}else if(id == IDC_MOUSEFUNC && code == CBN_SELCHANGE){
			UpdateUIControls(hDlg,-1,-1,1);
			SendPSChanged(hDlg);
		// Mouse Function - File
		}else if(id == IDC_MOUSEFILE && code == EN_CHANGE){
			int click;
			int button=ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_MOUSEBUTTON));
			for(click=0; click<m_mouseClickCount && !IsDlgButtonChecked(hDlg,IDC_RADSINGLE+click); ++click);
			if(click<m_mouseClickCount){
				Edit_GetText(control, m_pData[button].data[click], _countof(m_pData[button].data[click]));
				SendPSChanged(hDlg);
			}
		}else if(id == IDC_MOUSEFILEBROWSE){
			OnFileBrowse(hDlg, id);
		// tool tip
		}else if((id==IDC_TOOLTIP && code==EN_CHANGE) || id==IDCB_TOOLTIP){
			if(id==IDCB_TOOLTIP) EnableDlgItem(hDlg,IDC_TOOLTIP,IsDlgButtonChecked(hDlg,IDCB_TOOLTIP));
			if(!m_bTransition){
				g_bApplyClock=1;
				SendPSChanged(hDlg);
			}
		}else if(id==666){
			MessageBox(hDlg, L"You need to set at least one \"Context Menu\" action\nYou might be unable to control T-Clock otherwise", L"Invalid Setting", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		}
		return TRUE;}
	case WM_NOTIFY:{
		NMLISTVIEW* itm=(NMLISTVIEW*)lParam;
		switch(itm->hdr.code) {
		case PSN_KILLACTIVE:{
			int button,click;
			/// verify settings
			SetWindowLongPtr(hDlg,DWLP_MSGRESULT,1); // prevent close
			for(button=0; button<m_mouseButtonCount; ++button) {
				for(click=0; click<m_mouseClickCount; ++click) {
					if(m_pData[button].func[click]==MOUSEFUNC_MENU) {
						SetWindowLongPtr(hDlg,DWLP_MSGRESULT,0); // allow close
						return TRUE;
					}
				}
			}
			PostMessage(hDlg,WM_COMMAND,666,0); // show error message
			break;}
		case PSN_APPLY:
			OnApply(hDlg);
			break;
		case LVN_ITEMCHANGED:
			if(itm->uNewState&LVIS_SELECTED){
				HWND hList = GetDlgItem(hDlg, IDC_LIST);
				wchar_t value[TNY_BUFF];
				int button, click;
				LVITEM lvItem;
				lvItem.mask = LVIF_TEXT;
				lvItem.iItem = itm->iItem;
				lvItem.cchTextMax = _countof(value);
				lvItem.pszText = value;
				lvItem.iSubItem = 0;
				ListView_GetItem(hList, &lvItem);
				for(button=0; button<m_mouseButtonCount; ++button){
					if(wcscmp(m_mouseButton[button], value) == 0)
						break;
				}
				lvItem.iSubItem = 1;
				ListView_GetItem(hList, &lvItem);
				for(click=0; click<m_mouseClickCount; ++click){
					if(wcscmp(m_mouseClick[click], value) == 0)
						break;
				}
				UpdateUIControls(hDlg, button, click, 2);
				break;
			}
			break;
		}
		return TRUE;}
	}
	return FALSE;
}
//================================================================================================
//------------------------//----------------------------++--> Initialize Mouse Tab Dialog Controls:
void OnInit(HWND hDlg)   //-----------------------------------------------------+++-->
{
	HWND drop_cb = GetDlgItem(hDlg, IDC_DROPFILES);
	HWND listview = GetDlgItem(hDlg, IDC_LIST);
	wchar_t entry[3+4];
	wchar_t buf[LRG_BUFF];
	int button, click;
	LVCOLUMN lvCol;
	m_bTransition = 1; // start transition
	/// setup basic controls
	for(click=IDS_NONE; click<=IDS_MOVETO; ++click)
		ComboBox_AddString(drop_cb, MyString(click));
	ComboBox_SetCurSel(drop_cb, api.GetInt(REG_MOUSE,L"DropFiles",DF_RECYCLE));
	SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_DROPFILES,CBN_SELCHANGE), 0); // update IDC_DROPFILES related
	api.GetStr(REG_MOUSE, L"DropFilesApp", buf, _countof(buf), L"");
	SetDlgItemText(hDlg, IDC_DROPFILESAPP, buf);
	/// read mouse click settings
	entry[2] = '\0';
	m_pData = malloc(sizeof(CLICKDATA)*m_mouseButtonCount);
	for(button=0; button<m_mouseButtonCount; ++button) {
		for(click=0; click<m_mouseClickCount; ++click) {
			entry[0] = '0'+(char)button;
			entry[1] = '1'+(char)click;
			m_pData[button].func[click] = api.GetInt(REG_MOUSE, entry, MOUSEFUNC_NONE);
			m_pData[button].data[click][0] = '\0'; // clipboard format / execute file
			if(m_pData[button].func[click] == MOUSEFUNC_CLIPBOARD || m_pData[button].func[click] <= MOUSEFUNCEXTRA_BEGIN){
				memcpy(entry+2, L"Clip", 5*sizeof(wchar_t));
				api.GetStr(REG_MOUSE, entry, m_pData[button].data[click], _countof(m_pData[button].data[click]), L"");
				entry[2] = '\0';
			}
		}
	}
	// set mouse buttons/functions to combo boxes
	InitMouseControls(hDlg); // Populate Mouse Click Action DropDown Menu
	
	if(api.OS < TOS_VISTA){
		EnableDlgItem(hDlg, IDCB_TOOLTIP, 0);
		CheckDlgButton(hDlg,IDCB_TOOLTIP, 1);
	}else{
		CheckDlgButton(hDlg,IDCB_TOOLTIP, api.GetIntEx(L"Tooltip",L"bCustom",0));
		EnableDlgItem(hDlg,IDC_TOOLTIP, IsDlgButtonChecked(hDlg,IDCB_TOOLTIP));
	}
	api.GetStr(L"Tooltip", L"Tooltip", buf, _countof(buf), L"");
	if(!*buf)
		wcscpy(buf, TC_TOOLTIP);
	SetDlgItemText(hDlg, IDC_TOOLTIP, buf);
	/// setup list view
	ListView_SetExtendedListViewStyle(listview, (LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER));
	SetXPWindowTheme(listview, L"Explorer", NULL);
	
	lvCol.mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
	lvCol.iSubItem = 0;
	lvCol.pszText = L"Button";
	lvCol.fmt = LVCFMT_CENTER;
	lvCol.cx = 60;
	ListView_InsertColumn(listview, lvCol.iSubItem, &lvCol);
	/* begin 1st column workaround */
	ListView_InsertColumn(listview, lvCol.iSubItem+1, &lvCol);
	ListView_DeleteColumn(listview, 0);
	/* end */
	++lvCol.iSubItem;
	lvCol.pszText = L"Click type";
	lvCol.fmt = LVCFMT_CENTER;
	lvCol.cx = 70;
	ListView_InsertColumn(listview, lvCol.iSubItem, &lvCol);
	
	++lvCol.iSubItem;
	lvCol.pszText = L"Action";
	lvCol.fmt = LVCFMT_RIGHT;
	lvCol.cx = 160;
	ListView_InsertColumn(listview, lvCol.iSubItem, &lvCol);
	
	++lvCol.iSubItem;
	lvCol.pszText = L"Data";
	lvCol.fmt = LVCFMT_LEFT;
	lvCol.cx = 151;
	ListView_InsertColumn(listview, lvCol.iSubItem, &lvCol);
	m_bTransition = 0; // end transition
	/// select first mouse setup and UpdateMouseClickList
	UpdateUIControls(hDlg, 0, -1, 0); // pre-select first mouse button
}
//================================================================================================
//-------------------------//-------------------------+++--> Apply (Settings) Button Event Handler:
void OnApply(HWND hDlg)   //----------------------------------------------------------------+++-->
{
	wchar_t buf[LRG_BUFF], entry[3+4];
	int sel, button, click;
	sel = ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_DROPFILES));
	api.SetInt(REG_MOUSE, L"DropFiles",sel);
	GetDlgItemText(hDlg, IDC_DROPFILESAPP, buf, _countof(buf));
	api.SetStr(REG_MOUSE, L"DropFilesApp", buf);
	entry[2] = '\0';
	for(button=0; button<m_mouseButtonCount; ++button) {
		for(click=0; click<m_mouseClickCount; ++click) {
			entry[0] = '0'+(char)button;
			entry[1] = '1'+(char)click;
			if(m_pData[button].func[click]){
				api.SetInt(REG_MOUSE, entry, m_pData[button].func[click]);
				if(m_pData[button].func[click] == MOUSEFUNC_CLIPBOARD || m_pData[button].func[click] <= MOUSEFUNCEXTRA_BEGIN){
					/// @note : on next backward incompatible change, rename "Clip" to neutral "Data" or "Ex" and cleanup leftover entries
					memcpy(entry+2, L"Clip", 5*sizeof(wchar_t));
					api.SetStr(REG_MOUSE, entry, m_pData[button].data[click]);
					entry[2] = '\0';
				}
			}else{
				api.DelValue(REG_MOUSE, entry);
				memcpy(entry+2, L"Clip", 5*sizeof(wchar_t));
				api.DelValue(REG_MOUSE, entry);
				entry[2] = '\0';
			}
		}
	}
	if(api.OS >= TOS_VISTA) api.SetInt(L"Tooltip", L"bCustom", IsDlgButtonChecked(hDlg,IDCB_TOOLTIP));
	GetDlgItemText(hDlg, IDC_TOOLTIP, buf, _countof(buf));
	if(!wcscmp(buf, TC_TOOLTIP))
		buf[0] = '\0';
	api.SetStr(L"Tooltip", L"Tooltip", buf);
	// update list
	button = ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_MOUSEBUTTON));
	for(click=0; click<m_mouseClickCount && !IsDlgButtonChecked(hDlg,IDC_RADSINGLE+click); ++click);
	UpdateUIList(hDlg, button, click);
}
//=======================================================================================
//---------------------------//------------+++--> Free CLICKDATA Structure Memory on Exit:
void OnDestroy()   //--------------------------------------------------------------+++-->
{
	free(m_pData), m_pData=NULL;
}
/*--------------------------------------------------
--------------- Handle File Dropped on Clock Options
--------------------------------------------------*/
void OnFileBrowse(HWND hDlg, WORD id)
{
	wchar_t filter[80], deffile[MAX_PATH], fname[MAX_PATH];
	
	if(id==IDC_DROPFILESAPPSANSHO) {
		if(ComboBox_GetCurSel(GetDlgItem(hDlg,IDC_DROPFILES)) >= 3) {
			BROWSEINFO bi;
			LPITEMIDLIST pidl;
			memset(&bi, 0, sizeof(BROWSEINFO));
			bi.hwndOwner = hDlg;
			bi.ulFlags = BIF_RETURNONLYFSDIRS|BIF_USENEWUI;
			pidl = SHBrowseForFolder(&bi);
			if(pidl) {
				SHGetPathFromIDList(pidl, fname);
				SetDlgItemText(hDlg, id-1, fname);
				CoTaskMemFree(pidl);
				PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
				SendPSChanged(hDlg);
			}
			return;
		}
	}
	
	filter[0] = filter[1] = '\0';
	str0cat(filter, MyString(IDS_PROGRAMFILE));
	str0cat(filter, L"*.exe;*.cmd");
	str0cat(filter, MyString(IDS_ALLFILE));
	str0cat(filter, L"*.*");
	
	GetDlgItemText(hDlg, id-1, deffile, MAX_PATH);
	
	if(!SelectMyFile(hDlg, filter, 0, deffile, fname)) // propsheet.c
		return;
	SetDlgItemText(hDlg, id-1, fname);
	PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
	SendPSChanged(hDlg);
}
//================================================================================================
//-----------------------------------//------+++--> Populate the Mouse Click Actions DropDown Menu:
void InitMouseControls(HWND hDlg)   //------------------------------------------------------+++-->
{
	HWND btn_cb = GetDlgItem(hDlg, IDC_MOUSEBUTTON);
	HWND func_cb = GetDlgItem(hDlg, IDC_MOUSEFUNC);
	int iter;
	for(iter=0; iter<m_mouseButtonCount;++iter)
		ComboBox_AddString(btn_cb, m_mouseButton[iter]);
	ComboBox_SetDroppedWidth(func_cb, 180);
	ComboBox_AddString(func_cb, MyString(IDS_NONE));
	ComboBox_SetItemData(func_cb, 0, NULL);
	for(iter=0; iter<m_mouseActionCount; ++iter){
		ComboBox_AddString(func_cb, m_mouseAction[iter].name);
		ComboBox_SetItemData(func_cb, iter+1, m_mouseAction[iter].id);
	}
}
void CheckMouseMenu()
{
	short button,click;
	int hasmenu=0;
	union{
		uint32_t entryU;
		wchar_t entry[3];
	} u;
	const int bits = (8*sizeof(u.entry[0])); // 16
	u.entry[2] = '\0';
	for(button=0; button<m_mouseButtonCount; ++button) {
		for(click=0; click<m_mouseClickCount; ++click) {
			u.entryU = ('0'+button) | (('0'+click)<<bits);
			if(api.GetInt(REG_MOUSE,u.entry,0) == MOUSEFUNC_MENU) {
				hasmenu = 1;
				button = (short)m_mouseButtonCount;
				break;
			}
		}
	}
	if(!hasmenu){
		u.entryU = '1' | ('1'<<bits); // right, 1 click
		api.SetInt(REG_MOUSE, u.entry, MOUSEFUNC_MENU);
	}
}
