/*-------------------------------------------------------------
  sntp.c
  KAZUBON 1998-1999
               Special thanks to Tomoaki Nakashima
---------------------------------------------------------------*/
// Modified by Stoic Joker: Monday, 04/12/2010 @ 7:42:04pm
#include "tclock.h"

//===============================================================
struct NTP_Packet { // NTP (Network Time Protocol) Request Packet
	int Control_Word;
	int root_delay;
	int root_dispersion;
	int reference_identifier;
	__int64 reference_timestamp;
	__int64 originate_timestamp;
	__int64 receive_timestamp;
	int transmit_timestamp_seconds;
	int transmit_timestamp_fractions;
};

// PageHotKey.c
extern hotkey_t* tchk;
extern WNDPROC OldEditClassProc;
LRESULT APIENTRY SubClassEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static const char m_subkey[] = "SNTP";
static HWND m_dlg = NULL;
static BOOL m_bSaveLog;
static BOOL m_bMessage;
static DWORD m_dwTickCountOnSend = 0;

BOOL GetSetTimePermissions(void);

static void OnInit(HWND hDlg);
static void OnSanshoAlarm(HWND hDlg, WORD id);
static INT_PTR CALLBACK DlgProcSNTPConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
//================================================================================================
//---------------------------//----------------------------+++--> Save Request Results in SNTP.log:
void Log(const char* msg)   //--------------------------------------------------------------+++-->
{
	char logmsg[GEN_BUFF];
	size_t len;
	SYSTEMTIME st;
	
	GetLocalTime(&st);
	len = wsprintf(logmsg, "%d/%02d/%02d %02d:%02d:%02d ", st.wYear,
					st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	strncpy_s(logmsg+len, sizeof(logmsg)-len, msg, _TRUNCATE);
	
	// save to file
	if(m_bSaveLog) {
		char fname[MAX_PATH];
		HFILE hf;
		
		strcpy(fname, g_mydir);
		add_title(fname, "SNTP.log");
		hf = _lopen(fname, OF_WRITE);
		if(hf == HFILE_ERROR)
			hf = _lcreat(fname, 0);
		if(hf == HFILE_ERROR) return;
		_llseek(hf, 0, 2);
		_lwrite(hf, logmsg, lstrlen(logmsg));
		_lwrite(hf, "\r\n", 2);
		_lclose(hf);
	}
	
	if(m_dlg) { // IF Configure NTP Server is Open, Display Results in Sync History.
		LVITEM lvItem; //-----+++--> Even if Activity is Not Saved to the Log File.
		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 0; // Hold These at Zero So the File Loads Backwards
		lvItem.iItem = 0; //-----+++--> Which Puts the Most Recent Info on Top.
		lvItem.pszText = logmsg;
		ListView_InsertItem(GetDlgItem(m_dlg,IDC_LIST), &lvItem);
	}
	
	if(m_bMessage) {
		MessageBox(0, logmsg, "T-Clock Time Sync", MB_OK);
	}
}
//================================================================================================
//-------------------------------------------------------+++--> Set System Time With Received Data:
void SynchronizeSystemTime(DWORD seconds, DWORD fractions)   //-----------------------------+++-->
{
	BOOL bOk;
	char msg[GEN_BUFF];
	size_t msglen;
	char szWave[MAX_BUFF];
	SYSTEMTIME st;
	union{
		FILETIME ft;
		ULONGLONG ftqw;
	} tnew;
	union{
		FILETIME ft;
		ULONGLONG ftqw;
	} told;
	
	// current time
	GetSystemTimeAsFileTime(&told.ft);
	
	// NTP data -> FILETIME
	// seconds since 1900/01/01
	// + 100 nano-seconds from 1601/01/01 (FILETIME) to 1900/01/01 (NTP)
	tnew.ftqw = M32x32to64(seconds, 10000000) + 94354848000000000ULL;
	// + fractions ranging from 0 to MAXUINT(0xFFFFFFFF,4294967295) as rounded up milliseconds
	tnew.ftqw += (fractions / (429496+1) + 5) / 10 * 10000;
	// we now have our time with 500 nanosecond precession (as windows can only handle milliseconds)
	
	// set system time
	bOk = FileTimeToSystemTime(&tnew.ft, &st);
	if(bOk)
		bOk = SetSystemTime(&st);
	if(!bOk) {
		Log("failed to set time"); return;
	}
	// delayed or advanced
	bOk = (tnew.ftqw > told.ftqw);
	// get difference
	if(bOk) tnew.ftqw = tnew.ftqw - told.ftqw;
	else  tnew.ftqw = told.ftqw - tnew.ftqw;
	FileTimeToSystemTime(&tnew.ft, &st);
	
	// save log
	msglen = 13;
	memcpy(msg, "synchronized ", msglen+1);
	if(st.wYear == 1601 && st.wMonth == 1
	&& st.wDay == 1 && st.wHour == 0) {
		msg[msglen++] = (bOk?'+':'-');
		msglen += wsprintf(msg+msglen, "%02d:%02d.%03d ",
				 st.wMinute, st.wSecond, st.wMilliseconds);
	}
	
	GetMyRegStr(m_subkey, "Sound", szWave, MAX_BUFF, "");
	if(szWave[0]){
		PlayFile(g_hwndTClockMain, szWave, 0);
	}
	Log(msg);
}
//================================================================================================
//--------------------------------------------------+++--> Close Socket, and the WinSOCK Interface:
void SocketClose(SOCKET sock, const char* msgbuf)   //--------------------------------------+++-->
{

	if(sock != -1)
		closesocket(sock);
	
	if(msgbuf) Log(msgbuf);
	WSACleanup();
}

const char* GetServerPort(const char* server, char* host)
{
	char* pos = host;
	if(server[0]){
		strcpy(host, server);
		if(pos[0]=='[') for(; *pos && *pos!=']'; ++pos); // IPv6 address
		for(; *pos && *pos!=':'; ++pos);
		if(*pos==':') {
			*pos = '\0';
			return pos+1;
		}
	}
	return "";
}
//================================================================================================
//-------------------------+++--> Looks Like the Server Has SomeThing to Say - Find Out What it is:
void ReceiveSNTPReply(SOCKET sock)   //-----------------------------------------------------+++-->
{
	fd_set fds;
	struct timeval tv;
	struct NTP_Packet NTP_Recv;
	struct sockaddr_storage serv_addr;
	int serv_addr_len;
	int retval;
	
	FD_ZERO(&fds);
	FD_SET_nowarn(sock, &fds);
	tv.tv_sec = 1; tv.tv_usec = 0;

	retval = select((int)sock+1, &fds, NULL, NULL, &tv);
	if(retval == -1){
		char szErr[MIN_BUFF];
		wsprintf(szErr, "Receive SOCKET ERROR: %d", WSAGetLastError());
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		SocketClose(sock, szErr);
		return;
	}
	if(retval == 0){
		SocketClose(sock, "timeout");
		return;
	}
	serv_addr_len = sizeof(serv_addr);
	retval = recvfrom(sock, (char*)&NTP_Recv, sizeof(NTP_Recv), 0, (struct sockaddr*)&serv_addr, &serv_addr_len);
	
	//-------------------------------+++--> (Message Received) Now Set the System Time!
	SynchronizeSystemTime(ntohl(NTP_Recv.transmit_timestamp_seconds),
						  ntohl(NTP_Recv.transmit_timestamp_fractions));
	SocketClose(sock, NULL);
}
//================================================================================================
//-------------------------------------------------------------------+++--> Send SNTP Sync Request:
int SNTPSend(SOCKET sock, struct sockaddr_storage* serv_addr)
{
	struct NTP_Packet NTP_Send = {0};
	int retval;
	
	// init a packet
	NTP_Send.Control_Word = htonl(0x0B000000);
	
	// send a packet
	retval = sendto(sock, (char*)&NTP_Send, sizeof(NTP_Send), 0, (struct sockaddr*)serv_addr, sizeof(*serv_addr));
	// save tickcount
	m_dwTickCountOnSend = GetTickCount();
	return retval;
}

//================================================================================================
//-------------------------------------------------------------+++--> Open Socket for SNTP Session:
SOCKET OpenTimeSocket(const char* server)
{
	struct addrinfo hints = {0};
	struct addrinfo* addrs,* ap;
	struct sockaddr_storage serv_addr;
	char host[GEN_BUFF];
	const char* port;
	char szErr[MIN_BUFF];
	int retval;
	SOCKET sock = (SOCKET)-1;
	
	port = GetServerPort(server, host);
	if(!port[0]) port = "123";
	
	// get one server address
	hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
	hints.ai_family = AF_UNSPEC; /**< IPv4 and IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /**< datagram socket (UDP) */
	retval = getaddrinfo(host, port, &hints, &addrs);
	if(retval != 0){
		switch(retval){
		case EAI_SERVICE:
		case EAI_NONAME: strcpy(szErr,"host not found"); break;
		case EAI_MEMORY: strcpy(szErr,"out of memory"); break;
		default:
			wsprintf(szErr, "getaddrinfo ERROR: %d", WSAGetLastError());
			MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		}
		SocketClose((SOCKET)-1, szErr);
		return (SOCKET)-1;
	}
	
	// create client socket
	for (ap=addrs; ap; ap=ap->ai_next) {
		sock = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
		if(sock == -1)
			continue;
		memcpy(&serv_addr,ap->ai_addr,ap->ai_addrlen);
		#ifdef _DEBUG
		if(serv_addr.ss_family==AF_INET6){
			struct sockaddr_in6* addr = (struct sockaddr_in6*)&serv_addr;
			sprintf(host,"%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",ntohs(addr->sin6_addr.s6_addr16[0]),ntohs(addr->sin6_addr.s6_addr16[1]),ntohs(addr->sin6_addr.s6_addr16[2]),ntohs(addr->sin6_addr.s6_addr16[3]),ntohs(addr->sin6_addr.s6_addr16[4]),ntohs(addr->sin6_addr.s6_addr16[5]),ntohs(addr->sin6_addr.s6_addr16[6]),ntohs(addr->sin6_addr.s6_addr16[7]));
		}else{
			struct sockaddr_in* addr = (struct sockaddr_in*)&serv_addr;
			sprintf(host,"%hu.%hu.%hu.%hu",addr->sin_addr.s_net,addr->sin_addr.s_host,addr->sin_addr.s_lh,addr->sin_addr.s_impno);
		}
		sprintf(szErr,"[%s]:%hu\n",host,ntohs(((struct sockaddr_in*)&serv_addr)->sin_port));
		OutputDebugString(szErr);
		#endif // _DEBUG
		break;
//		if(connect(sock, ap->ai_addr, ap->ai_addrlen) != -1)
//			break; // success
//		closesocket(sock);
	}
	
	// check client socket
	if(sock == -1) {
		freeaddrinfo(addrs);
		wsprintf(szErr, "socket ERROR: %d", WSAGetLastError());
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		SocketClose(sock, szErr);
		return (SOCKET)-1;
	}
	freeaddrinfo(addrs);
	
	// send data
	retval = SNTPSend(sock, &serv_addr);
	if(retval == -1) {
		wsprintf(szErr, "SNTPSend ERROR: %d", WSAGetLastError());
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		SocketClose(sock, szErr);
		return (SOCKET)-1;
	}
	return sock;
}
//====================//========================================================================
// Required for SNTP/UDP Socket Operation TimeOut Thread Only ===================================
#include <process.h>   //--+++-->				 <--+++--<<<<< SNTP Code Starts Here >>>>>--+++-->
//====================//===========================================================================
void SyncTimeNow()   //============================================================================
{
	WORD wVersionRequested = MAKEWORD(2,2);
	WSADATA wsaData; // Okay...Now We Want WinSock v2.2
	char server[GEN_BUFF];
	char szErr[GEN_BUFF];
	SOCKET sock;
	int retval;
	
	if(!m_dlg) {
		m_bSaveLog = GetMyRegLongEx(m_subkey, "SaveLog", 0);
		m_bMessage = GetMyRegLongEx(m_subkey, "MessageBox", 0);
	}
	GetMyRegStrEx(m_subkey, "Server", server, sizeof(server), "");
	if(!server[0]) {
		wsprintf(szErr, "No SNTP Server Specified!");
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		if(!m_dlg) NetTimeConfigDialog();
		return;
	}
	
	retval = WSAStartup(wVersionRequested, &wsaData);
	if(retval) { //-----------------------------------------+++--> If WinSock Startup Fails...
		wsprintf(szErr, "Error initializing WinSock");
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		return;
	}
	
	if(wsaData.wVersion != wVersionRequested) { //-+++-> Check WinSOCKET's Version:
		wsprintf(szErr, "WinSock version not supported");
		MessageBox(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR);
		return;
	}
	
	sock = OpenTimeSocket(server);
	if(sock != -1)
		ReceiveSNTPReply(sock);
}
//=================================================================================================
/* SetSystemTime(&st); Requires SE_SYSTEMTIME_NAME Priviledge: Are you running as a limited user?

If so, you'll need to use the group policy editor (Run gpedit.msc as Administrator) to assign the
rights here:

(Computer Configuration\Windows Settings\Security Settings\Local Policies\User Rights Assignments
and add your username to "Change the system time"). I don't know of any specific registry key. */

// Note: Changing Privileges in a Token - Only Works IF the Account Has Access to that Priviledge
//---------------------------------------------------+++--> e.g. IS An Administrator.
//================================================================================================
//--------------------------------//---------------------+++--> Open the SNTP Configuration Dialog:
void NetTimeConfigDialog(void)   //---------------------------------------------------------+++-->
{
	DialogBox(0, MAKEINTRESOURCE(IDD_SNTPCONFIG), g_hwndTClockMain, DlgProcSNTPConfig);
}
//================================================================================================
//--------------------------//--+++--> Save Network Time Server Configuration Settings to Registry:
void OkaySave(HWND hDlg)   //---------------------------------------------------------------+++-->
{
	char szServer[GEN_BUFF];
	char szSound[MAX_PATH];
	char entry[TNY_BUFF];
	HWND hServer = GetDlgItem(hDlg,IDCBX_NTPSERVER);
	int i, count;
	
	SetMyRegLong(m_subkey, "SaveLog", IsDlgButtonChecked(hDlg, IDCBX_SNTPLOG));
	SetMyRegLong(m_subkey, "MessageBox", IsDlgButtonChecked(hDlg, IDCBX_SNTPMESSAGE));
	
	GetDlgItemText(hDlg, IDCE_SYNCSOUND, szSound, MAX_PATH);
	SetMyRegStr(m_subkey, "Sound", szSound);
	
	if(tchk[0].bValid) { // Synchronize System Clock With Remote Time Server
		RegisterHotKey(g_hwndTClockMain, HOT_TSYNC, tchk[0].fsMod, tchk[0].vk);
	} else {						// I'm Calling This One Mouser's HotKey...
		tchk[0].vk = 0;		   // I'm Not Explaining It You Either Already
		tchk[0].fsMod = 0;	  // Understand Why or You're Not Going Too...
		strcpy(tchk[0].szText, "None");
		UnregisterHotKey(g_hwndTClockMain, HOT_TSYNC);
	}
	SetMyRegLong("HotKeys\\HK5", "bValid", tchk[0].bValid);
	SetMyRegLong("HotKeys\\HK5", "fsMod",  tchk[0].fsMod);
	SetMyRegStr("HotKeys\\HK5", "szText", tchk[0].szText);
	SetMyRegLong("HotKeys\\HK5", "vk",  tchk[0].vk);
	
	
	ComboBox_GetText(hServer, szServer, sizeof(szServer));
	SetMyRegStr(m_subkey, "Server", szServer);
	
	if(szServer[0]) {
		int index = ComboBox_FindStringExact(hServer, -1, szServer);
		if(index){
			if(index != CB_ERR)
				ComboBox_DeleteString(hServer, index);
			ComboBox_InsertString(hServer, 0, szServer);
			ComboBox_SetCurSel(hServer, 0);
		}
	}
	count = ComboBox_GetCount(hServer);
	// removed deleted servers
	for(i=GetMyRegLong(m_subkey,"ServerNum",0); i>count; --i){
		wsprintf(entry, "Server%d", i);
		DelMyReg(m_subkey, entry);
	}
	// update server list
	for(i=0; i < count; ++i) {
		ComboBox_GetLBText(hServer, i, szServer);
		wsprintf(entry, "Server%d", i+1);
		SetMyRegStr(m_subkey, entry, szServer);
	}
	SetMyRegLong(m_subkey, "ServerNum", count);
}
//================================================================================================
//------------------------------------------------------+++--> SNTP Configuration Dialog Procedure:
INT_PTR CALLBACK DlgProcSNTPConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	
	switch(msg)  {
	case WM_INITDIALOG:
		m_dlg = hDlg;
		OnInit(hDlg);
		return TRUE;
	case WM_DESTROY:
		m_dlg = NULL;
		break;
		
	case WM_COMMAND:
		switch(LOWORD(wParam))  {
		case IDCB_SYNCNOW:{
			OkaySave(hDlg);
			SyncTimeNow();
			return TRUE;}
			
		case IDCB_SYNCSOUNDBROWSE:
			OnSanshoAlarm(hDlg, IDCE_SYNCSOUND);
			return TRUE;
			
		case IDCB_CLEAR:{
			char logfile[MAX_PATH];
			FILE* fp;
			HWND hList = GetDlgItem(hDlg,IDC_LIST);
			ListView_DeleteAllItems(hList);
			
			strcpy(logfile, g_mydir);
			add_title(logfile, "SNTP.log");
			fp = fopen(logfile, "w");
			if(fp) fclose(fp);
			return TRUE;}
		
		case IDCB_DELSERVER:{
			HWND hServer = GetDlgItem(hDlg,IDCBX_NTPSERVER);
			int index = ComboBox_GetCurSel(hServer);
			if(index != CB_ERR){
				ComboBox_DeleteString(hServer, index);
			}
			ComboBox_SetCurSel(hServer, 0);
			return TRUE;}
			
		case IDOK:
			OkaySave(hDlg);
			/* fall through */
		case IDCANCEL:
			if(tchk) {
				free(tchk);   // Free, and...? (Crash Unless You Include the Next Line)
				tchk = NULL; //<--+++--> Thank You Don Beusee for reminding me to do this.
			}
			EndDialog(hDlg, 1);
			return TRUE;
		}
	}
	return FALSE;
}
//=================//=====//>>>>>---------------------------------------------+++-->
#include <stdio.h> //-----//--+++--> Required Here For Log FILE Open Functions Only.
//=================//=====//======================================================================
//------------------------//---------------------------+++--> To-Do List for Dialog Initialization:
void OnInit(HWND hDlg)   //-----------------------------------------------------------------+++-->
{
	char str[MAX_PATH];
	FILE* stReport;
	LVCOLUMN lvCol;
	LVITEM lvItem;
	int i, count;
	HWND hList = GetDlgItem(hDlg,IDC_LIST);
	HWND hServer = GetDlgItem(hDlg,IDCBX_NTPSERVER);
	
	SetMyDialgPos(hDlg,21);
	
	// Get the List of Configured Time Servers:
	GetMyRegStr(m_subkey, "Server", str, sizeof(str), "");
	count = GetMyRegLong(m_subkey, "ServerNum", 0);
	for(i = 1; i <= count; i++) {
		char s[MAX_BUFF], entry[TNY_BUFF];
		
		wsprintf(entry, "Server%d", i);
		GetMyRegStr(m_subkey, entry, s, 80, "");
		if(s[0]) ComboBox_AddString(hServer, s);
	}
	if(!ComboBox_GetCount(hServer)){
		ComboBox_AddString(hServer,"europe.pool.ntp.org");
		ComboBox_AddString(hServer,"north-america.pool.ntp.org");
		ComboBox_AddString(hServer,"asia.pool.ntp.org");
		ComboBox_AddString(hServer,"oceania.pool.ntp.org");
		ComboBox_AddString(hServer,"south-america.pool.ntp.org");
		ComboBox_AddString(hServer,"africa.pool.ntp.org");
	}
	if(!str[0])
		strcpy(str,"pool.ntp.org");
	i = ComboBox_FindStringExact(hServer, -1, str);
	if(i == CB_ERR) {
		i = ComboBox_InsertString(hServer, 0, str);
	}
	ComboBox_SetCurSel(hServer, i);
	
	if(!g_hIconDel) {
		g_hIconDel = LoadImage(GetModuleHandle(NULL),
							   MAKEINTRESOURCE(IDI_DEL),
							   IMAGE_ICON, 16, 16,
							   LR_DEFAULTCOLOR);
	}
	SendDlgItemMessage(hDlg, IDCB_DELSERVER, BM_SETIMAGE,
					   IMAGE_ICON, (LPARAM)g_hIconDel);
					   
	// Get the Sync Sound File:
	GetMyRegStr(m_subkey, "Sound", str, sizeof(str), "");
	SetDlgItemText(hDlg, IDCE_SYNCSOUND, str);
	
	// Get the Confirmation Options:
	m_bSaveLog = GetMyRegLongEx(m_subkey, "SaveLog", 0);
	CheckDlgButton(hDlg, IDCBX_SNTPLOG, m_bSaveLog);
	m_bMessage = GetMyRegLongEx(m_subkey, "MessageBox", 0);
	CheckDlgButton(hDlg, IDCBX_SNTPMESSAGE, m_bMessage);
	
	// Load & Display the Configured Synchronization HotKey:
	tchk = (hotkey_t*)malloc(sizeof(hotkey_t));
	tchk[0].bValid = GetMyRegLongEx("HotKeys\\HK5", "bValid", 0);
	GetMyRegStrEx("HotKeys\\HK5", "szText", tchk[0].szText, TNY_BUFF, "None");
	tchk[0].fsMod = GetMyRegLongEx("HotKeys\\HK5", "fsMod", 0);
	tchk[0].vk = GetMyRegLongEx("HotKeys\\HK5", "vk", 0);
	
	SetDlgItemText(hDlg, IDCE_SYNCHOTKEY, tchk[0].szText);
	
	// Subclass the Edit Controls
	OldEditClassProc  = (WNDPROC)GetWindowLongPtr(GetDlgItem(hDlg, IDCE_SYNCHOTKEY), GWLP_WNDPROC);
	SetWindowLongPtr(GetDlgItem(hDlg, IDCE_SYNCHOTKEY), GWLP_WNDPROC, (LONG_PTR)SubClassEditProc);
	
	// init listview
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
	SetXPWindowTheme(hList,L"Explorer",NULL);
	
	lvCol.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvCol.cx = 280; //-+-// Set Column Width in Pixels
	lvCol.iSubItem = 0; // This is the First & Only Column
	lvCol.pszText = "Synchronization History"; // Header Text
	ListView_InsertColumn(hList, 0, &lvCol);
	
	// Test For: SE_SYSTEMTIME_NAME Priviledge Before Enabling Sync Now Button:
	EnableDlgItem(hDlg, IDCB_SYNCNOW, GetSetTimePermissions());
	
	// Load the Time Synchronization Log File:
	strcpy(str, g_mydir);
	add_title(str, "SNTP.log");
	
	stReport = fopen(str, "r");
	if(stReport) {
		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 0; // Hold These at Zero So the File Loads Backwards
		lvItem.iItem = 0; //-----+++--> Which Puts the Most Recent Info on Top.
		
		for(;;) {   // (for) Ever Basically.
			if(fgets(str, sizeof(str), stReport)) {
				str[strcspn(str,"\n")] = '\0'; // Remove the Newline Character
				lvItem.pszText = str;
				ListView_InsertItem(hList, &lvItem);
			}else
				break;
		}
		fclose(stReport);
	}
}
//================================================================================================
//----------------------------------------//---------------+++--> Browse for Sync Event Sound File:
void OnSanshoAlarm(HWND hDlg, WORD id)   //-------------------------------------------------+++-->
{
	char deffile[MAX_PATH], fname[MAX_PATH];
	
	GetDlgItemText(hDlg, id, deffile, MAX_PATH);
	if(!BrowseSoundFile(hDlg, deffile, fname)) // soundselect.c
		return;
		
	SetDlgItemText(hDlg, id, fname);
	PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
}
