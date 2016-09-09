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
	int64_t reference_timestamp;
	int64_t originate_timestamp;
	int64_t receive_timestamp;
	int transmit_timestamp_seconds;
	int transmit_timestamp_fractions;
};

static const wchar_t m_subkey[] = L"SNTP";
static char m_flags;
static DWORD m_dwTickCountOnSend = 0;

enum{
	SNTPF_LOG		=0x01, /**< write to log file */
	SNTPF_MESSAGE	=0x02, /**< display info message on sync */
	SNTPF_UAC		=0x04, /**< we require UAC elevation */
};

BOOL GetSetTimePermissions();

static void OnBrowseAction(HWND hDlg, WORD id);
static INT_PTR CALLBACK Window_SNTPConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
//================================================================================================
//---------------------------//----------------------------+++--> Save Request Results in SNTP.log:
void Log(const char* msg)   //--------------------------------------------------------------+++-->
{
	char logmsg[GEN_BUFF];
	size_t len;
	SYSTEMTIME st;
	
	GetLocalTime(&st);
	len = wsprintfA(logmsg, "%d/%02d/%02d %02d:%02d:%02d ", st.wYear,
					st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	strncpy_s(logmsg+len, _countof(logmsg)-len, msg, _TRUNCATE);
	
	// save to file
	if(m_flags&SNTPF_LOG) {
		wchar_t fname[MAX_PATH];
		FILE* fp;
		
		memcpy(fname, api.root, api.root_size);
		add_title(fname, L"SNTP.log");
		fp = _wfopen(fname, L"ab");
		if(!fp)
			return;
		fputs(logmsg, fp);
		fwrite("\r\n", 1, 2, fp);
		fclose(fp);
	}
	
	if(g_hDlgSNTP) { // IF Configure NTP Server is Open, Display Results in Sync History.
		LVITEMA lvItem; //-----+++--> Even if Activity is Not Saved to the Log File.
		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 0; // Hold These at Zero So the File Loads Backwards
		lvItem.iItem = 0; //-----+++--> Which Puts the Most Recent Info on Top.
		lvItem.pszText = logmsg;
		SendMessageA(GetDlgItem(g_hDlgSNTP,IDC_LIST), LVM_INSERTITEMA, 0, (LPARAM)&lvItem);
	}
	
	if(m_flags&SNTPF_MESSAGE) {
		MessageBoxA(0, logmsg, "T-Clock Time Sync", MB_OK|MB_SETFOREGROUND);
	}
}
//================================================================================================
//-------------------------------------------------------+++--> Set System Time With Received Data:
void SynchronizeSystemTime(DWORD seconds, DWORD fractions)   //-----------------------------+++-->
{
	BOOL bOk;
	char msg[GEN_BUFF];
	size_t msglen;
	wchar_t szWave[MAX_BUFF];
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
	tnew.ftqw = UInt32x32To64(seconds, 10000000) + 94354848000000000ULL;
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
		msglen += wsprintfA(msg+msglen, "%02d:%02d.%03d ",
				 st.wMinute, st.wSecond, st.wMilliseconds);
	}
	
	api.GetStr(m_subkey, L"Sound", szWave, _countof(szWave), L"");
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
		wsprintfA(szErr, "Receive SOCKET ERROR: %d", WSAGetLastError());
		MessageBoxA(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND); // @todo : SocketClose can also show this message
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
SOCKET OpenTimeSocket(const wchar_t* server)
{
	struct addrinfo hints = {0};
	struct addrinfo* addrs,* ap;
	struct sockaddr_storage serv_addr;
	char szErr[GEN_BUFF];
	char host[GEN_BUFF];
	const char* port;
	int retval;
	SOCKET sock = (SOCKET)-1;
	
	WideCharToMultiByte(CP_ACP, 0, server, -1, szErr, _countof(szErr), NULL, NULL);
	port = GetServerPort(szErr, host);
	if(!port[0]) port = "123";
	
	// get one server address
	hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
	hints.ai_family = AF_UNSPEC; /**< IPv4 and IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /**< datagram socket (UDP) */
	retval = getaddrinfo(host, port, &hints, &addrs);
	if(retval != 0){
		switch(retval){
		case EAI_SERVICE:
		case EAI_NONAME: strcpy(szErr, "host not found"); break;
		case EAI_MEMORY: strcpy(szErr, "out of memory"); break;
		default:
			wsprintfA(szErr, "getaddrinfo ERROR: %d", WSAGetLastError());
			MessageBoxA(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
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
		DBGOUT(szErr);
		#endif // _DEBUG
		break;
//		if(connect(sock, ap->ai_addr, ap->ai_addrlen) != -1)
//			break; // success
//		closesocket(sock);
	}
	
	// check client socket
	if(sock == -1) {
		freeaddrinfo(addrs);
		wsprintfA(szErr, "socket ERROR: %d", WSAGetLastError());
		MessageBoxA(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		SocketClose(sock, szErr);
		return (SOCKET)-1;
	}
	freeaddrinfo(addrs);
	
	// send data
	retval = SNTPSend(sock, &serv_addr);
	if(retval == -1) {
		wsprintfA(szErr, "SNTPSend ERROR: %d", WSAGetLastError());
		MessageBoxA(0, szErr, "Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		SocketClose(sock, szErr);
		return (SOCKET)-1;
	}
	return sock;
}
//====================//========================================================================
//---------------------------------------+++--><--+++--<<<<< SNTP Code Starts Here >>>>>--+++-->
void SyncTimeNow()
{
	WORD wVersionRequested = MAKEWORD(2,2);
	WSADATA wsaData; // Okay...Now We Want WinSock v2.2
	wchar_t server[GEN_BUFF];
	wchar_t szErr[GEN_BUFF];
	SOCKET sock;
	int retval;
	
	if(!g_hDlgSNTP) {
		m_flags = 0;
		if(api.GetIntEx(m_subkey, L"SaveLog", 0))
			m_flags |= SNTPF_LOG;
		if(api.GetIntEx(m_subkey, L"MessageBox", 0))
			m_flags |= SNTPF_MESSAGE;
	}
	api.GetStrEx(m_subkey, L"Server", server, _countof(server), L"");
	if(!server[0]) {
		wsprintf(szErr, FMT("No SNTP Server Specified!"));
		MessageBox(0, szErr, L"Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		if(!g_hDlgSNTP)
			NetTimeConfigDialog(0);
		return;
	}
	
	retval = WSAStartup(wVersionRequested, &wsaData);
	if(retval) { //-----------------------------------------+++--> If WinSock Startup Fails...
		wsprintf(szErr, FMT("Error initializing WinSock"));
		MessageBox(0, szErr, L"Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
		return;
	}
	
	if(wsaData.wVersion != wVersionRequested) { //-+++-> Check WinSOCKET's Version:
		wsprintf(szErr, FMT("WinSock version not supported"));
		MessageBox(0, szErr, L"Time Sync Failed", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
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
void NetTimeConfigDialog(int justElevated)   //---------------------------------------------------------+++-->
{
	CreateDialogParamOnce(&g_hDlgSNTP, 0, MAKEINTRESOURCE(IDD_SNTPCONFIG), NULL, Window_SNTPConfig, (LPARAM)justElevated);
}
//================================================================================================
//--------------------------//--+++--> Save Network Time Server Configuration Settings to Registry:
void OkaySave(HWND hDlg)   //---------------------------------------------------------------+++-->
{
	wchar_t szServer[GEN_BUFF];
	wchar_t szSound[MAX_PATH];
	wchar_t entry[TNY_BUFF];
	HWND hServer = GetDlgItem(hDlg,IDCBX_NTPSERVER);
	int i, count;
	
	api.SetInt(m_subkey, L"SaveLog", m_flags&SNTPF_LOG);
	api.SetInt(m_subkey, L"MessageBox", m_flags&SNTPF_MESSAGE);
	
	GetDlgItemText(hDlg, IDCBX_SYNCSOUND, szSound, _countof(szSound));
	api.SetStr(m_subkey, L"Sound", szSound);
	
	ComboBox_GetText(hServer, szServer, _countof(szServer));
	api.SetStr(m_subkey, L"Server", szServer);
	
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
	for(i=api.GetInt(m_subkey,L"ServerNum",0); i>count; --i){
		wsprintf(entry, FMT("Server%d"), i);
		api.DelValue(m_subkey, entry);
	}
	// update server list
	for(i=0; i < count; ++i) {
		ComboBox_GetLBText(hServer, i, szServer);
		wsprintf(entry, FMT("Server%d"), i+1);
		api.SetStr(m_subkey, entry, szServer);
	}
	api.SetInt(m_subkey, L"ServerNum", count);
}


static void OnSNTPInit(HWND hDlg, LPARAM just_elevated) {
	union {
		wchar_t w[MAX_BUFF];
		char a[MAX_BUFF];
	} str;
	FILE* stReport;
	LVCOLUMN lvCol;
	LVITEMA lvItem;
	int i, count;
	HWND hList = GetDlgItem(hDlg,IDC_LIST);
	HWND hServer = GetDlgItem(hDlg,IDCBX_NTPSERVER);
	HWND sound_cb = GetDlgItem(hDlg,IDCBX_SYNCSOUND);
	HWND button_sync = GetDlgItem(hDlg, IDCB_SYNCNOW);
	
	api.PositionWindow(hDlg,21);
	
	// Get the List of Configured Time Servers:
	count = api.GetInt(m_subkey, L"ServerNum", 0);
	for(i = 1; i <= count; i++) {
		wchar_t entry[TNY_BUFF];
		wsprintf(entry, FMT("Server%d"), i);
		api.GetStr(m_subkey, entry, str.w, _countof(str.w), L"");
		if(str.w[0])
			ComboBox_AddString(hServer, str.w);
	}
	if(!ComboBox_GetCount(hServer)){
		ComboBox_AddString(hServer, L"pool.ntp.org");
		ComboBox_AddString(hServer, L"europe.pool.ntp.org");
		ComboBox_AddString(hServer, L"north-america.pool.ntp.org");
		ComboBox_AddString(hServer, L"asia.pool.ntp.org");
		ComboBox_AddString(hServer, L"oceania.pool.ntp.org");
		ComboBox_AddString(hServer, L"south-america.pool.ntp.org");
		ComboBox_AddString(hServer, L"africa.pool.ntp.org");
		ComboBox_AddString(hServer, L"time1.google.com");
		ComboBox_AddString(hServer, L"time2.google.com");
		ComboBox_AddString(hServer, L"time3.google.com");
		ComboBox_AddString(hServer, L"time4.google.com");
		ComboBox_AddString(hServer, L"time.nist.gov");
		ComboBox_AddString(hServer, L"time.windows.com");
	}
	api.GetStr(m_subkey, L"Server", str.w, _countof(str.w), L"");
	ComboBox_AddStringOnce(hServer, str.w, 1, 0);
	
	SendDlgItemMessage(hDlg, IDCB_DELSERVER, BM_SETIMAGE,
					   IMAGE_ICON, (LPARAM)g_hIconDel);
					   
	// Get the Sync Sound File:
	api.GetStr(m_subkey, L"Sound", str.w, _countof(str.w), L"");
	ComboBox_SetText(sound_cb, str.w);
	ComboBoxArray_AddSoundFiles(&sound_cb, 1);
	
	// Get the Confirmation Options:
	m_flags = 0;
	if(api.GetIntEx(m_subkey, L"SaveLog", 0)){
		CheckDlgButton(hDlg, IDCBX_SNTPLOG, 1);
		m_flags |= SNTPF_LOG;
	}
	if(api.GetIntEx(m_subkey, L"MessageBox", 0)){
		CheckDlgButton(hDlg, IDCBX_SNTPMESSAGE, 1);
		m_flags |= SNTPF_MESSAGE;
	}
	
	// init listview
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
	SetXPWindowTheme(hList, L"Explorer", NULL);
	
	lvCol.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvCol.cx = 280; //-+-// Set Column Width in Pixels
	lvCol.iSubItem = 0; // This is the First & Only Column
	lvCol.pszText = L"Synchronization History"; // Header Text
	ListView_InsertColumn(hList, 0, &lvCol);
	
	// Test For: SE_SYSTEMTIME_NAME Priviledge Before Enabling Sync Now Button:
	if(!HaveSetTimePermissions()){
		m_flags |= SNTPF_UAC;
		Button_SetElevationRequiredState(button_sync, 1);
	}
	
	// Load the Time Synchronization Log File:
	memcpy(str.w, api.root, api.root_size);
	add_title(str.w, L"SNTP.log");
	
	stReport = _wfopen(str.w, L"rb");
	if(stReport) {
		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 0; // Hold These at Zero So the File Loads Backwards
		lvItem.iItem = 0; //-----+++--> Which Puts the Most Recent Info on Top.
		
		for(;;) {   // (for) Ever Basically.
			if(fgets(str.a, sizeof(str.a), stReport)) {
				str.a[strcspn(str.a,"\r")] = '\0'; // remove carriage return
				str.a[strcspn(str.a,"\n")] = '\0'; // remove linefeed
				lvItem.pszText = str.a;
				SendMessageA(hList, LVM_INSERTITEMA, 0, (LPARAM)&lvItem);
			}else
				break;
		}
		fclose(stReport);
	}
	
	if(just_elevated && !(m_flags & SNTPF_UAC))
		PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDCB_SYNCNOW,BN_CLICKED), (LPARAM)button_sync);
}
//================================================================================================
//------------------------------------------------------+++--> SNTP Configuration Dialog Procedure:
INT_PTR CALLBACK Window_SNTPConfig(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)  {
	case WM_INITDIALOG:
		OnSNTPInit(hDlg, lParam);
		return TRUE;
	case WM_DESTROY:
		g_hDlgSNTP = NULL;
		break;
	case WM_CLOSE:
		if(!lParam && wParam == 1)
			OkaySave(hDlg);
		break;
	case WM_ACTIVATE:
		WM_ActivateTopmost(hDlg, wParam, lParam);
		break;
		
	case WM_COMMAND:
		switch(LOWORD(wParam))  {
		case IDCB_SYNCNOW:{
			if(m_flags&SNTPF_UAC){
				api.ExecElevated(GetClockExe(),L"/UAC /SyncOpt",hDlg);
				return TRUE;
			}
			OkaySave(hDlg);
			SyncTimeNow();
			return TRUE;}
			
		case IDCBX_SNTPLOG:
			m_flags ^= SNTPF_LOG;
			return TRUE;
		case IDCBX_SNTPMESSAGE:
			m_flags ^= SNTPF_MESSAGE;
			return TRUE;
			
		case IDCB_SYNCSOUNDBROWSE:
			OnBrowseAction(hDlg, IDCBX_SYNCSOUND);
			return TRUE;
			
		case IDCB_CLEAR:{
			wchar_t logfile[MAX_PATH];
			FILE* fp;
			HWND hList = GetDlgItem(hDlg, IDC_LIST);
			ListView_DeleteAllItems(hList);
			
			memcpy(logfile, api.root, api.root_size);
			add_title(logfile, L"SNTP.log");
			fp = _wfopen(logfile, L"wb");
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
			DestroyWindow(hDlg);
			return TRUE;
		}
	}
	return FALSE;
}
//================================================================================================
//----------------------------------------//---------------+++--> Browse for Sync Event Sound File:
void OnBrowseAction(HWND hDlg, WORD id)   //-------------------------------------------------+++-->
{
	wchar_t deffile[MAX_PATH], fname[MAX_PATH];
	
	GetDlgItemText(hDlg, id, deffile, MAX_PATH);
	if(!BrowseSoundFile(hDlg, deffile, fname)) // soundselect.c
		return;
		
	SetDlgItemText(hDlg, id, fname);
	PostMessage(hDlg, WM_NEXTDLGCTL, 1, FALSE);
}
