#include "../common/globals.h"
#include "../common/messagebox_custom.h"
#include "../common/version.h"

#include <winhttp.h>
#include <commctrl.h> // PBM_SETPOS

#include "update.h"
#include "resource.h"

const wchar_t* kUpdateURL = L"http://rawgit.com/White-Tiger/T-Clock/master/src/version";
//const wchar_t* kUpdateURL = L"http://cdn.rawgit.com/White-Tiger/T-Clock/master/src/version";
const wchar_t* kDownloadURL = L"https://github.com/White-Tiger/T-Clock/releases";
#define UPDATE_BUFFER 2048
#define WM_DOWNLOAD_RESULT WM_USER /**< 0 if successful, otherwise WINHTTP_CALLBACK_STATUS_* or HTTP status code */

#ifndef PBM_SETSTATE // Vista+
#	define PBM_SETSTATE (WM_USER+16)
#	define PBM_GETSTATE (WM_USER+17)
#	define PBST_NORMAL 0x0001
#	define PBST_ERROR  0x0002
#	define PBST_PAUSED 0x0003
#endif

static MessageBoxCustomData update_notify_data_ = {
	NULL, NULL,
	NULL,
	{
		{L"Update", NULL, 0},
		{L"Skip version", NULL, 0},
		{L"Close", NULL, 0},
		{0},
	},
	{
		{L"Regularly check for updates", {3, 3, 105, 8}, 0, 0},
		{L"include beta versions", {3+105, 3, 80, 8}, BST_MBC_AUTODISABLE, 0},
	},
	ID_MBC3
};

typedef struct {
	HWND update_window;
	HWND status_text;
	HICON icon_update;
	HICON icon_update_small;
	HINTERNET session, connection, request;
	volatile int ref_count;
	int type;
	int next_version[2];
	char buffer[UPDATE_BUFFER + 1];
} UpdateData;

void UpdateCheck_Progress(UpdateData* data, int progress) {
	if(IsWindow(data->update_window)){
		HWND control;
		control = GetDlgItem(data->update_window, IDC_PROGRESS);
		SendMessage(control, PBM_SETPOS, progress, 0);
		if(progress >= 100) {
			control = GetDlgItem(data->update_window, IDCANCEL);
			Button_SetText(control, L"Close");
		}
	}
}

void UpdateCheck_Free(UpdateData* data) {
	if(!data)
		return;
	UpdateCheck_Progress(data, 100);
	if(data->session) {
		WinHttpCloseHandle(data->request);
		WinHttpCloseHandle(data->connection);
		WinHttpCloseHandle(data->session);
		data->session = NULL;
	}
	if(--data->ref_count == 0)
		free(data);
}

int UpdateCheck_WriteDescription(wchar_t* out, int size, const char* text) {
	int written = 0;
	wchar_t* pos = out;
	char* nl;
	do{
		nl = strchr(text, '\n');
		if(!nl++)
			nl = strchr(text, '\0');
		pos += swprintf(pos, size-written, FMT("   %.*hs"), nl-text, text);
		text = nl;;
		written = pos-out;
	}while(*nl);
	return written;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/aa383138%28v=vs.85%29.aspx
void CALLBACK HttpProgress(HINTERNET hInternet, DWORD_PTR userdata, unsigned long status, void* info, unsigned long info_len) {
	UpdateData* data = (UpdateData*)userdata;
	switch(status){
	case WINHTTP_CALLBACK_STATUS_DETECTING_PROXY:
		SetWindowText(data->status_text, L"detecting proxy...");
		UpdateCheck_Progress(data, 10);
		break;
	case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
		SetWindowText(data->status_text, L"resolving hostname...");
		UpdateCheck_Progress(data, 20);
		break;
	case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
		SetWindowText(data->status_text, L"connecting...");
		UpdateCheck_Progress(data, 30);
		break;
	case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
		SetWindowText(data->status_text, L"connected");
		UpdateCheck_Progress(data, 40);
		break;
	case WINHTTP_CALLBACK_STATUS_REDIRECT:
		SetWindowText(data->status_text, L"redirecting...");
		UpdateCheck_Progress(data, 50);
		break;
	case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
		SetWindowText(data->status_text, L"sending request...");
		UpdateCheck_Progress(data, 60);
		break;
	case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
		SetWindowText(data->status_text, L"receiving data...");
		UpdateCheck_Progress(data, 70);
		break;
//	case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
//		SetWindowText(data->status_text, L"closing connection...");
//		break;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:{
		DWORD http_status;
		DWORD size = sizeof(http_status);
		WinHttpQueryHeaders(hInternet, WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER, NULL, &http_status, &size, WINHTTP_NO_HEADER_INDEX);
		if(http_status != HTTP_STATUS_OK){
			PostMessage(data->update_window, WM_DOWNLOAD_RESULT, status, http_status);
			UpdateCheck_Free(data);
			break;
		}
		WinHttpQueryDataAvailable(hInternet, NULL);
		UpdateCheck_Progress(data, 75);
		break;}
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE: // still called after header error
		WinHttpReceiveResponse(hInternet, NULL);
		break;
	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
		SetWindowText(data->status_text, L"data...");
		WinHttpReadData(hInternet, data->buffer, sizeof(data->buffer)-1, NULL);
		UpdateCheck_Progress(data, 90);
		break;}
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		if(info_len){
//			WinHttpReadData(hInternet, data->buffer, sizeof(data->buffer), NULL);
			data->buffer[info_len] = '\0';
		}
		PostMessage(data->update_window, WM_DOWNLOAD_RESULT, 0, 0);
		UpdateCheck_Free(data);
		break;
	case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		PostMessage(data->update_window, WM_DOWNLOAD_RESULT, status, (LPARAM)info);
		UpdateCheck_Free(data);
		break;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		break;
	}
}

static INT_PTR CALLBACK Window_UpdateCheckDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	const wchar_t* kValueNextVersion[2] = {UPDATE_RELEASE, UPDATE_BETA};
	const char* kVersionType[2] = {"Release", "Beta"};
	UpdateData* data;
	int child_id;
//	int child_notify;
	
	switch(uMsg) {
	case WM_INITDIALOG:{
		wchar_t szHost[256];
		URL_COMPONENTS urlComp = {sizeof(urlComp)};
		urlComp.lpszHostName = szHost;
		urlComp.dwHostNameLength = sizeof(szHost)/sizeof(szHost[0]);
		urlComp.dwUrlPathLength = 1;
		urlComp.dwSchemeLength = 1;
		
		if(lParam != UPDATE_SHOW) // code in WM_WINDOWPOSCHANGING isn't enough...
			SetWindowLongPtr(hDlg, GWL_EXSTYLE, WS_EX_NOACTIVATE);
		
		data = calloc(1, sizeof(UpdateData));
		if(!data)
			goto error;
		data->ref_count = 2;
		data->type = lParam;
		data->update_window = hDlg;
		data->status_text = GetDlgItem(hDlg, IDC_STATUS);
		data->icon_update = LoadImage(g_instance, MAKEINTRESOURCE(IDI_UPDATE), IMAGE_ICON, 0,0, LR_DEFAULTSIZE);
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)data->icon_update);
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)data->icon_update);
		data->icon_update_small = LoadImage(g_instance, MAKEINTRESOURCE(IDI_UPDATE_S), IMAGE_ICON, 0,0, LR_DEFAULTSIZE);
		data->next_version[0] = api.GetInt(NULL, kValueNextVersion[0], 1);
		#if VER_IsReleaseOrHigher() // release build, opt-in beta check
		data->next_version[1] = api.GetInt(NULL, kValueNextVersion[1], 0);
		#else // non-release build, force beta check
		data->next_version[1] = 1;
		#endif
		SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)data);
		
		data->session = WinHttpOpen(L"T-Clock/" L(VER_SHORT_DOTS), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
		if(!data->session)
			goto error;
		WinHttpSetTimeouts(data->session, 30000, 10000, 30000, 30000);
		WinHttpSetStatusCallback(data->session, HttpProgress, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
		WinHttpCrackUrl(kUpdateURL, 0, 0, &urlComp);
		data->connection = WinHttpConnect(data->session, urlComp.lpszHostName, urlComp.nPort, 0);
		if(!data->connection)
			goto error;
		data->request = WinHttpOpenRequest(data->connection, L"GET", urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, (urlComp.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0));
		if(!data->request)
			goto error;
		WinHttpSendRequest(data->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, (DWORD_PTR)data);
		return 1;
		error:
		SendMessage(hDlg, WM_DOWNLOAD_RESULT, 1, 0);
		UpdateCheck_Free(data); // frees 2nd reference (callback)
		if(lParam != UPDATE_SHOW)
			EndDialog(hDlg, IDCANCEL);
		return 1;}
	case WM_WINDOWPOSCHANGING:{
		WINDOWPOS* wndpos = (WINDOWPOS*)lParam;
		data = (UpdateData*)GetWindowLongPtr(hDlg,DWLP_USER);
		if(data && data->type != UPDATE_SHOW) {
			wndpos->flags &= ~SWP_SHOWWINDOW;
			wndpos->flags |= SWP_NOACTIVATE|SWP_NOZORDER;
		}
		return 1;}
	case WM_WINDOWPOSCHANGED:
		return 1;
	case WM_DESTROY:
		data = (UpdateData*)GetWindowLongPtr(hDlg,DWLP_USER);
		DestroyIcon(data->icon_update_small);
		DestroyIcon(data->icon_update);
		UpdateCheck_Free(data);
		return 1;
	case WMBC_INITDIALOG:
		data = (UpdateData*)GetWindowLongPtr(hDlg,DWLP_USER);
		update_notify_data_.button[0].icon = data->icon_update_small;
		// update check
		update_notify_data_.check[0].state = 0;
		if(data->next_version[0])
			update_notify_data_.check[0].state |= BST_CHECKED;
		// beta check
		update_notify_data_.check[1].state = BST_MBC_AUTODISABLE;
		if(data->next_version[1])
			update_notify_data_.check[1].state |= BST_CHECKED;
		#if !VER_IsReleaseOrHigher() // non-release build, forced beta check
		update_notify_data_.check[1].state &= ~BST_MBC_AUTODISABLE;
		update_notify_data_.check[1].style = WS_DISABLED;
		#endif
		SetWindowLongPtr(hDlg, DWLP_MSGRESULT, (LONG_PTR)&update_notify_data_);
		return 1;
	case WMBC_CHECKS:
		data = (UpdateData*)GetWindowLongPtr(hDlg,DWLP_USER);
		if(!data->next_version[0] != !(wParam&1)) {
			data->next_version[0] = (wParam&1);
			api.SetInt(NULL, kValueNextVersion[0], data->next_version[0]);
		}
		if(!data->next_version[1] != !(wParam&2)) {
			data->next_version[1] = (wParam&2 ? 1 : 0);
			api.SetInt(NULL, kValueNextVersion[1], data->next_version[1]);
		}
		return 1;
	case WM_DOWNLOAD_RESULT:
		data = (UpdateData*)GetWindowLongPtr(hDlg,DWLP_USER);
		if(wParam == 0) {
			int idx = 0;
			char* pos;
			unsigned version[2];
			unsigned version_next[2];
			const char* version_str[2] = {0};
			char* version_text[2];
			
			// parse version data
			pos = data->buffer;
			for(;;) {
				if(!version_str[1]) {
					version_text[0] = pos;
					pos = strchr(version_text[0], '\n');
					if(!pos || pos-version_text[0] > 21)
						break;
					*pos++ = '\0';
					version_text[1] = strchr(version_text[0], '|');
					if(!version_text[1])
						break;
					*version_text[1]++ = '\0';
					for(idx=0; idx<2; ++idx) {
						if(!version_str[0]) {
							// revision
							version_next[idx] = data->next_version[idx];
							if(version_next[idx] && version_next[idx] < VER_REVISION)
								version_next[idx] = VER_REVISION;
							version[idx] = atoi(version_text[idx]);
							if(version[idx] <= version_next[idx])
								version_next[idx] = 0;
						} else {
							// version string
							version_str[idx] = version_text[idx];
						}
					}
					if(!version_str[0]) {
						version_str[0] = (char*)1;
						continue;
					}
				}
				// version description
				version_text[0] = strchr(pos, '|');
				if(!version_text[0])
					break;
				++version_text[0];
				version_text[1] = strchr(version_text[0], '|');
				if(!version_text[1])
					break;
				*version_text[1]++ = '\0';
				idx = -1;
				break;
			}
			if(idx != -1) {
				SendMessage(hDlg, WM_DOWNLOAD_RESULT, 2, 0);
				return 1;
			}
			// show version
			if(data->type == UPDATE_SHOW) {
				version_next[0] = version[0];
				version_next[1] = version[1];
			}
			if(version_next[0] || version_next[1]) {
				if(data->type != UPDATE_SILENT) {
					wchar_t* abovebehind;
					wchar_t* msg,* msg_pos,* msg_end;
					msg = msg_pos = (wchar_t*)malloc((512*UPDATE_BUFFER) * sizeof(msg[0]));
					msg_end = msg + UPDATE_BUFFER;
					if(!msg) {
						SendMessage(hDlg, WM_DOWNLOAD_RESULT, 2, 0);
						return 1;
					}
					ShowWindow(hDlg, SW_HIDE);
					idx = version[0]-VER_REVISION;
					abovebehind = (idx<0 ? L"above" : L"behind");
					msg_pos += swprintf(msg_pos, msg_end-msg_pos, FMT("Your version: ") FMT(VER_REVISION_TAG) FMT("	(%i change(s) %s stable)\n\n"), abs(idx), abovebehind);
					for(idx=0; idx<2; ++idx) {
						if(version_next[idx]) {
							msg_pos += swprintf(msg_pos, msg_end-msg_pos, FMT("%hs:	v%hs#%u\n"), kVersionType[idx], version_str[idx], version[idx]);
							msg_pos += UpdateCheck_WriteDescription(msg_pos, msg_end-msg_pos, version_text[idx]);
							if(!idx)
								msg_pos += swprintf(msg_pos, msg_end-msg_pos, FMT("\n"));
						}
					}
					child_id = MessageBoxCustom(hDlg, msg, L"T-Clock updates", MB_ICONINFORMATION|MB_SETFOREGROUND);
					switch(child_id) {
					case ID_MBC1:
						api.ExecFile(kDownloadURL, hDlg);
						break;
					case ID_MBC2:
						for(idx=0; idx<2; ++idx) {
							if(version[idx] && data->next_version[idx])
								api.SetInt(NULL, kValueNextVersion[idx], version[idx]);
						}
						break;
					case ID_MBC3:
						break;
					}
					free(msg);
				}
				EndDialog(hDlg, IDYES);
			} else {
				EndDialog(hDlg, IDNO);
			}
		} else if(data && data->type == UPDATE_SHOW) {
			wchar_t text[64];
			wchar_t* text_pos;
			size_t maxlen;
			text_pos = text + swprintf(text, _countof(text), FMT("<ERROR> "));
			maxlen = _countof(text) + text_pos - text;
			
			SendMessage(GetDlgItem(hDlg,IDC_PROGRESS), PBM_SETSTATE, PBST_ERROR, 0);
			switch(wParam) {
			case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
				swprintf(text_pos, maxlen, FMT("got HTTP status: %i"), (int)lParam);
				break;
			case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
				if(lParam & WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR)
					swprintf(text_pos, maxlen, FMT("SSL error"));
				else
					swprintf(text_pos, maxlen, FMT("invalid SSL certificate"));
				break;
			case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
				swprintf(text_pos, maxlen, FMT("connection issues"));
				break;
			case 2:
				swprintf(text_pos, maxlen, FMT("invalid response"));
				break;
			/* case 1 */
			default:
				swprintf(text_pos, maxlen, FMT("failed to check for updates"));
			}
			SetWindowText(data->status_text, text);
		} else {
			EndDialog(hDlg, IDCANCEL);
		}
		return 1;
	case WM_COMMAND:
		child_id = LOWORD(wParam);
//		child_notify = HIWORD(wParam);
		switch(child_id) {
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;
		}
		return 1;
	}
	return 0;
}

int UpdateCheck(int type) {
	int ret;
	HANDLE updatelock = NULL;
	if(type != UPDATE_SILENT) {
		updatelock = CreateMutex(NULL, 0, kUpdateURL);
		if(GetLastError() == ERROR_ALREADY_EXISTS){
			CloseHandle(updatelock);
			return IDCANCEL;
		}
	}
	ret = DialogBoxParam(g_instance, MAKEINTRESOURCE(IDD_UPDATECHECK), NULL, Window_UpdateCheckDlg, type);
	CloseHandle(updatelock);
	return ret;
}
