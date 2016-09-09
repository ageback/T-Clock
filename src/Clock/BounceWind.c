//================================================================================
//--+++--> BounceWind.c  -  Stoic Joker 2010  =====================================
//================= Last Modified by Stoic Joker: Wednesday, 12/22/2010 @ 11:29:24pm
#include "tclock.h" //---------------{ Stoic Joker 2006-2010 }---------------+++-->
//-----------------//-------------------------------------------------------+++-->
#include <time.h> // Required for Randomizing the Bounce Height -----------+++-->
#include <process.h> // _beginthread

static int m_iDelta, m_iSpeed, m_iPause, m_iShort, m_iSkew, m_iPaws, m_iBounce;
static int m_iScreenW, m_iScreenH, m_iBallX, m_iBallY, m_iDeltaX, m_iDeltaY;

enum{
	BFLAG_NONE		=0x00,
	BFLAG_RAND		=0x01,
	BFLAG_TOPMOST	=0x02,
};
static unsigned char m_flags;

static void OnOK(HWND hDlg);
static void OnInit(HWND hDlg, dlgmsg_t* dlg);
static INT_PTR CALLBACK Window_AlarmMsgConfigDlg(HWND, UINT, WPARAM, LPARAM);

static void BounceWindow(HWND hwnd,const RECT* rc);
static void CenterDoggie(HWND hwnd,const RECT* rc);
static void ParseSettings(wchar_t* data);

#define ID_DOGGIE 2419

//-----------------------------//--------------------------------------------+++-->
//-----------------------------//----+++--> Open Window Bounce Control Options Here:
int BounceWindOptions(HWND hDlg, dlgmsg_t* dlg)  //--------------------------+++-->
{
	INT_PTR ret = DialogBoxParam(0, MAKEINTRESOURCE(IDD_ALARMMSG), hDlg, Window_AlarmMsgConfigDlg, (LPARAM)dlg);
	if(ret == IDOK) {
		return 1;
	}
	return 0;
}
//---------------------------------------------------------------------------+++-->
// -------------------------------------------+++--> Alarm Message Dialog Procedure:
INT_PTR CALLBACK Window_AlarmMsgConfigDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch(message) {
	case WM_INITDIALOG:
		OnInit(hDlg,(dlgmsg_t*)lParam);
		return TRUE;
		
	case WM_COMMAND: {
			WORD id = LOWORD(wParam);
			switch(id) {
			case IDC_ALRMMSG_BOUN_ENABLE:{
				int control;
				int check=IsDlgButtonChecked(hDlg,IDC_ALRMMSG_BOUN_ENABLE);
				for(control=IDC_ALRMMSG_SPEED_ST; control<=IDC_ALRMMSG_PAWS_SPIN; ++control){
					EnableDlgItem(hDlg,control,check);
				}
				break;}
			case IDC_ALRMMSG_TEST:{
				dlgmsg_t* dlg=(dlgmsg_t*)GetWindowLongPtr(hDlg,DWLP_USER);
				m_iBounce = GetDlgItemInt(hDlg, IDC_ALRMMSG_BOUN, NULL, TRUE); // Get Users Attention!!!
				m_iSkew = GetDlgItemInt(hDlg, IDC_ALRMMSG_SKEW, NULL, TRUE); // Hop vs. Ricochet deviation
				m_iPaws = GetDlgItemInt(hDlg, IDC_ALRMMSG_PAWS, NULL, TRUE); // Sit! & Wait for User Input
				
				m_iSpeed = GetDlgItemInt(hDlg, IDC_ALRMMSG_SPEED, NULL, TRUE); // SetTimer->Milliseconds
				m_iDelta = GetDlgItemInt(hDlg, IDC_ALRMMSG_DELTA, NULL, TRUE); // Move X Pixels per Loop
				GetDlgItemText(hDlg, IDC_ALRMMSG_CAPT, dlg->name, _countof(dlg->name));
				GetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, dlg->message, _countof(dlg->message));
				m_flags=0;
				if(IsDlgButtonChecked(hDlg,IDC_ALRMMSG_RAND)) m_flags|=BFLAG_RAND;
				if(IsDlgButtonChecked(hDlg,IDC_ALRMMSG_TOPMOST)) m_flags|=BFLAG_TOPMOST;
				ReleaseTheHound(hDlg,dlg->name,dlg->message,NULL);
				break;}
				
			case IDOK:
				OnOK(hDlg);
				/* fall through */
			case IDCANCEL:
				EndDialog(hDlg, id);
				break;
			}
			return TRUE;
		}
	}
	return FALSE;
}
//------------------------//-------------------------------------------------+++-->
// -----------------------//----------------------------+++--> Initialize Dialoggie:
void OnInit(HWND hDlg, dlgmsg_t* dlg)   //--------------------------------------------------+++-->
{
	/*	bRandHop = TRUE; // Enable Randomized Hop Height
		iBounce = 3;   // Get Users Attention!!!
		iDelta = 42;  // Move X Pixels per Loop
		iSpeed = 90; // SetTimer->Milliseconds
		iSkew = 4; // Hop vs. Ricochet deviation
		iPaws = 3; // Sit! & Wait for User Input
	*/
	SetWindowLongPtr(hDlg,DWLP_USER,(LONG_PTR)dlg);
	ParseSettings(dlg->settings);
	
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_SPEED_SPIN, UDM_SETRANGE32, 0,200);
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_SPEED_SPIN, UDM_SETPOS32, 0, m_iSpeed);  // Movement Timer Rate
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_DELTA_SPIN, UDM_SETRANGE32, m_iSkew,42);
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_DELTA_SPIN, UDM_SETPOS32, 0, m_iDelta); // Pixel Distance Moved
	
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_SKEW_SPIN, UDM_SETRANGE32, 1,6);
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_SKEW_SPIN, UDM_SETPOS32, 0, m_iSkew); //----+++--> Skew Factor
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_PAWS_SPIN, UDM_SETRANGE32, 0,10);
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_PAWS_SPIN, UDM_SETPOS32, 0, m_iPaws); //-+++--> Paws/Sit! Time
	SendDlgItemMessage(hDlg, IDC_ALRMMSG_BOUN_SPIN, UDM_SETRANGE32, 1,10);
	if(!m_iBounce){
		int control;
		for(control=IDC_ALRMMSG_SPEED_ST; control<=IDC_ALRMMSG_PAWS_SPIN; ++control){
			EnableDlgItem(hDlg,control,0);
		}
		SendDlgItemMessage(hDlg, IDC_ALRMMSG_BOUN_SPIN, UDM_SETPOS32, 0, 3); // default bounce 3
	}else{
		CheckDlgButton(hDlg, IDC_ALRMMSG_BOUN_ENABLE, 1);
		SendDlgItemMessage(hDlg, IDC_ALRMMSG_BOUN_SPIN, UDM_SETPOS32, 0, m_iBounce); //--+++--> Bounce Time
	}
	SetDlgItemText(hDlg, IDC_ALRMMSG_CAPT, dlg->name);
	SetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, dlg->message);
	CheckDlgButton(hDlg, IDC_ALRMMSG_RAND, m_flags&BFLAG_RAND);
	CheckDlgButton(hDlg, IDC_ALRMMSG_TOPMOST, m_flags&BFLAG_TOPMOST);
	m_iScreenW = GetSystemMetrics(SM_CXSCREEN);
	m_iScreenH = GetSystemMetrics(SM_CYSCREEN);
	
	m_iShort = 100;
}
//-----------------------------//--------------------------------------------+++-->
//------------------+++--> ...And Here be the Part What Goes Boing, Boing, Boing!!!:
void BounceWindow(HWND hwnd,const RECT* rc)  //---------------------------------------------+++-->
{
	int iSizeW, iSizeH;
	
	m_iBallX += (m_iDeltaX / m_iSkew);
	m_iBallY += m_iDeltaY;
	iSizeW = rc->right - rc->left;
	iSizeH = rc->bottom - rc->top;
	
	if(m_iBallX < 0) {
		m_iBallX = 0;
		m_iDeltaX = m_iDelta;
	} else if((m_iBallX + iSizeW) > m_iScreenW) {
		m_iBallX = m_iScreenW - iSizeW;
		m_iDeltaX = -m_iDelta;
	}
	
	if(m_iBallY < m_iShort) {
		m_iBallY = m_iShort;
		m_iDeltaY = m_iDelta;
	} else if((m_iBallY + iSizeH) > m_iScreenH) {
		if(m_flags&BFLAG_RAND) {
			double dTmp;
			srand((unsigned)time(0));
			dTmp = rand() % 42 + 0;
			dTmp *= .01;
			m_iShort = (int)(m_iScreenH * dTmp); // Make Shorter Random Height Hops
		} else {
			m_iShort = 0;
		}
		m_iBallY = m_iScreenH - iSizeH;
		m_iDeltaY = -m_iDelta;
	}
	SetWindowPos(hwnd, HWND_TOP, m_iBallX, m_iBallY, 0, 0, SWP_NOSIZE);
}
//------------------------------//-------------------------------------------+++-->
//------------+++--> Snap Dialoggie Window to Center Screen and Wait for User Input:
void CenterDoggie(HWND hwnd,const RECT* rc)   //--------------------------------------------+++-->
{
	int iSizeW, iSizeH;
	int x, y;
	
	iSizeW = rc->right - rc->left;
	iSizeH = rc->bottom - rc->top;
	x = (m_iScreenW / 2) - (iSizeW / 2);
	y = (m_iScreenH / 2) - (iSizeH / 2);
	SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
}
//----------------------//---------------------------------------------------+++-->
// ---------------------//------------------------------+++--> Initialize Dialoggie:
void OnOK(HWND hDlg)   //----------------------------------------------------+++-->
{
	dlgmsg_t* dlg=(dlgmsg_t*)GetWindowLongPtr(hDlg,DWLP_USER);
	m_iBounce = GetDlgItemInt(hDlg, IDC_ALRMMSG_BOUN, NULL, TRUE); // Get Users Attention!!!
	if(!IsDlgButtonChecked(hDlg,IDC_ALRMMSG_BOUN_ENABLE))
		m_iBounce=0;
	m_iSkew = GetDlgItemInt(hDlg, IDC_ALRMMSG_SKEW, NULL, TRUE); // Hop vs. Ricochet deviation
	m_iPaws = GetDlgItemInt(hDlg, IDC_ALRMMSG_PAWS, NULL, TRUE); // Sit! & Wait for User Input
	
	m_iSpeed = GetDlgItemInt(hDlg, IDC_ALRMMSG_SPEED, NULL, TRUE); // SetTimer->Milliseconds
	m_iDelta = GetDlgItemInt(hDlg, IDC_ALRMMSG_DELTA, NULL, TRUE); // Move X Pixels per Loop
//	GetDlgItemText(hDlg, IDC_ALRMMSG_CAPT, dlg->name, _countof(dlg->name));
	GetDlgItemText(hDlg, IDC_ALRMMSG_TEXT, dlg->message, _countof(dlg->message));
	
	m_flags=0;
	if(IsDlgButtonChecked(hDlg,IDC_ALRMMSG_RAND)) m_flags|=BFLAG_RAND;
	if(IsDlgButtonChecked(hDlg,IDC_ALRMMSG_TOPMOST)) m_flags|=BFLAG_TOPMOST;
	
	if(m_iSkew < 1) m_iSkew = 1; // Divide by Zero = Bad...
	
	wsprintf(dlg->settings, FMT("%i,%i,%i,%i,%i,%hu"), m_iBounce, m_iSkew, m_iPaws, m_iSpeed, m_iDelta, m_flags);
}
//--------------------------------------------------------//--------//-------+++-->
//----------------------------+++--> Parse the Dialoggie Settings Out of the String:
void ParseSettings(wchar_t* data)   //------------------------------------------+++-->
{
	const wchar_t seps[] = L",";
	wchar_t* szToken, *nxToken;
	int i=0;
	
	if(!data[0])
		wcscpy(data, L"0,4,3,90,42,3"); // last one is "flags" BFLAG_RAND|BFLAG_TOPMOST = 3
		//			 0,1,2, 3, 4,5
	
	szToken = wcstok_s(data, seps, &nxToken);
	while(szToken != NULL) {
		switch(i++) {
		case 0:
			m_iBounce = _wtoi(szToken);
			break;
		case 1:
			m_iSkew = _wtoi(szToken);
			break;
		case 2:
			m_iPaws = _wtoi(szToken);
			break;
		case 3:
			m_iSpeed = _wtoi(szToken);
			break;
		case 4:
			m_iDelta = _wtoi(szToken);
			break;
		case 5: // flags
			m_flags = (unsigned char)_wtoi(szToken);
			break;
		}
		szToken = wcstok_s(NULL, seps, &nxToken);
	}
}

static wchar_t* m_caption = NULL;
//---------------------------------------------------------------------------+++-->
//------------------------------------------+++--> Ricochet Doggie Window Procedure:
VOID CALLBACK DoggieProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)   //
{
	static RECT m_rcMsg;
	static HWND m_hWndBoing=NULL;
	(void)hWnd; (void)uMsg; (void)idEvent; (void)dwTime;
	m_iPause += m_iSpeed;
	
	if(m_iPause >= (m_iBounce * 1000)) { // Check for Sit! Command.
		if(m_iPause < ((m_iBounce * 1000) + 101)) CenterDoggie(m_hWndBoing,&m_rcMsg);
		m_iPause += m_iSpeed;
		if(m_iPause >= ((m_iBounce * 1000)+(m_iPaws * 1000))) { // Give User X Seconds to Read
			m_iPause = 0; //---------------------------+++--> and Respond to Alarm Message.
		}
	} else if(m_caption) { // Bounce Wildly for X Seconds!
		m_hWndBoing = FindWindow(NULL, m_caption);
		if(m_hWndBoing){
			GetWindowRect(m_hWndBoing,&m_rcMsg);
			BounceWindow(m_hWndBoing,&m_rcMsg);
		}
	}
}


typedef struct{
	wchar_t* title;
	wchar_t* msg;
	HWND hwnd;
} bounce_t;
static void __cdecl MessageThread(void* param){
	bounce_t* data = (bounce_t*)param;
	if(!*data->msg){
		time_t tt = time(NULL);
		free(data->msg);
		data->msg = malloc(128 * sizeof data->msg[0]);
		if(data->msg)
			wcsftime(data->msg, 128, L"Your alarm expired on\n%A %H:%M", localtime(&tt));
	}
	api.Message(NULL, data->msg, data->title, MB_OK|MB_SETFOREGROUND|(m_flags&BFLAG_TOPMOST?MB_SYSTEMMODAL:0), MB_OK);
	KillTimer(data->hwnd, ID_DOGGIE);
	m_caption = NULL;
	free(data->msg);
	free(data->title);
	free(data);
	PostMessage(g_hwndClock, CLOCKM_BLINKOFF, 0, 0);
}
//---------------------------------------------------------------------------+++-->
//-------------------------------------------+++--> The Alarm Just Let the Dogz Out:
void ReleaseTheHound(HWND hwnd, const wchar_t* title, const wchar_t* text, wchar_t* settings)
{
	bounce_t* data;
	m_iScreenW = GetSystemMetrics(SM_CXSCREEN);
	m_iScreenH = GetSystemMetrics(SM_CYSCREEN);
	m_iShort = 100;
	
	if(settings) // If This is a Live Alarm & Not a Test, Parse the Settings Input String.
		ParseSettings(settings);
		
	m_iDeltaX = m_iDeltaY = m_iDelta;
	m_iBallX = m_iBallY = 0;
	m_iPause = 0; // Hang Time Between Rompings...
	
	if(m_iSkew < 1) m_iSkew = 1; // Divide by Zero = Bad...
	
	if(m_iSpeed > 100) { // Timer Cannot be Set
		m_iSpeed = 0;  // Any Faster Than Zero!
	} else { // Convert High Number Into Short Time Slice.
		m_iSpeed = 100 - m_iSpeed;
	} //-//-++-> Now Set the Converted Short Time Slice.
	
	if(m_caption){
		return;
	}
	SetTimer(hwnd, ID_DOGGIE, m_iSpeed, DoggieProc);
	if(m_iSpeed < 10) m_iSpeed = 10; // Required to Ensure Paws.
	// our notify
	data = (bounce_t*)malloc(sizeof(bounce_t));
	data->title = wcsdup(title);
	data->msg = wcsdup(text);
	data->hwnd = hwnd;
	m_caption = data->title;
	_beginthread(MessageThread,0,data); // frees data
}
