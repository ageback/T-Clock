#include "../common/globals.h"
#include <commctrl.h>
#include "control_extensions.h"

/*
	GENERIC
*/

void ComboBox_AddStringOnce(HWND box, const wchar_t* str, int select, int def_select)
{
	int sel;
	if(!str[0]){
		if(!select || def_select == -1)
			ComboBox_SetText(box, str);
		else
			ComboBox_SetCurSel(box, def_select);
		return;
	}
	sel = ComboBox_FindStringExact(box, -1, str);
	if(sel == -1){
		sel = ComboBox_AddString(box, str);
	}
	if(select)
		ComboBox_SetCurSel(box, sel);
}

int CALLBACK SortString_LV(HWND list, int column, int flags, intptr_t item1, intptr_t item2, intptr_t unused) {
	int order;
	wchar_t str1[128];
	wchar_t str2[_countof(str1)];
	LVITEM item;
	
	(void)unused;
	
	item.mask = LVIF_TEXT;
	item.cchTextMax = _countof(str1);
	item.iSubItem = column;
	
	item.iItem = (int)item1;
	item.pszText = str1;
	ListView_GetItem(list, &item);
	
	item.iItem = (int)item2;
	item.pszText = str2;
	ListView_GetItem(list, &item);
	
	if(flags & SORT_INSENSITIVE)
		order = wcsncasecmp(str1, str2, _countof(str1));
	else
		order = wcsncmp(str1, str2, _countof(str1));
	if(flags & SORT_DEC)
		order = -order;
	return order;
}

typedef struct sort_wrapper_t {
	sort_func_t func;
	intptr_t user;
	HWND hwnd;
	int column;
	int flags;
} sort_wrapper_t;

static int CALLBACK SortWrapper_(LPARAM item1, LPARAM item2, LPARAM wrapper_) {
	sort_wrapper_t* wrapper = (sort_wrapper_t*)wrapper_;
	return wrapper->func(wrapper->hwnd, wrapper->column, wrapper->flags, item1, item2, wrapper->user);
}

void ListView_SortItemsExEx(HWND list, int column, sort_func_t func, intptr_t user, int flags) {
	sort_wrapper_t wrapper;
	LONG_PTR last_sort = 0;
	LVCOLUMN col;
	const wchar_t kAscending[]        = L" â¶",
	kDescending[_countof(kAscending)] = L" â·";
	const wchar_t* indicator;
	wchar_t column_name[64];
	wrapper.func = func;
	wrapper.user = user;
	wrapper.hwnd = list;
	wrapper.column = column;
	wrapper.flags = flags;
	if((wrapper.flags & SORT_NEXT) == SORT_NEXT) {
		last_sort = GetWindowLongPtr(list, GWLP_USERDATA);
		if(last_sort) {
			column = (last_sort & 0x00ffffff);
			if(column == wrapper.column) {
				wrapper.flags &= ~(SORT_ASC | SORT_DEC);
				if(flags & (SORT_ASC | SORT_DEC)) { // toggle
					if(last_sort & SORT_DEC)
						wrapper.flags |= SORT_ASC;
					else
						wrapper.flags |= SORT_DEC;
				} else { // refresh sort
					if(last_sort & SORT_DEC)
						wrapper.flags |= SORT_DEC;
				}
			}
		}
	}
	if(wrapper.flags & SORT_CUSTOMPARAM)
		ListView_SortItems(list, SortWrapper_, &wrapper);
	else
		ListView_SortItemsEx(list, SortWrapper_, &wrapper);
	if(wrapper.flags & SORT_REMEMBER) {
		col.mask = LVCF_TEXT;
		col.pszText = column_name;
		col.cchTextMax = _countof(column_name);
		if(last_sort) {
			if(ListView_GetColumn(list, column, &col)) {
				col.iOrder = (int)wcslen(column_name) - (_countof(kAscending) - 1);
				if(col.iOrder >= 0 && column_name[col.iOrder] == kAscending[0]) {
					column_name[col.iOrder] = '\0';
					ListView_SetColumn(list, column, &col);
				}
			}
		}
		last_sort = wrapper.column;
		if(wrapper.flags & SORT_DEC) {
			indicator = kDescending;
			last_sort |= SORT_DEC;
		} else {
			indicator = kAscending;
			last_sort |= SORT_ASC;
		}
		
		if(ListView_GetColumn(list, wrapper.column, &col)) {
			col.iOrder = (int)wcslen(column_name);
			if(col.iOrder + _countof(kAscending) <= _countof(column_name)) {
				memcpy(column_name+col.iOrder, indicator, sizeof(kAscending));
				ListView_SetColumn(list, wrapper.column, &col);
			}
		}
		SetWindowLongPtr(list, GWLP_USERDATA, last_sort);
	}
}

/*
	LINK CONTROLS
*/
//#define WM_LINKSETTARGET    WM_USER+0x4910

static short m_links = 0;
static HFONT m_link_font_hovered = NULL;
static HFONT m_link_font_underline;
static HCURSOR m_cursor_hand;
static WNDPROC m_proc_static;
static LRESULT CALLBACK LinkControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef struct{
	const wchar_t* target;
	unsigned char flags;
} link_data;

void LinkControl_Setup(HWND link_control, unsigned char flags, const wchar_t* target) {
	link_data* data;
	if(GetWindowLongPtr(link_control, GWLP_USERDATA))
		return;
	
	data = (link_data*)malloc(sizeof(link_data));
	data->target = target;
	data->flags = flags;
	SetWindowLongPtr(link_control, GWLP_USERDATA, (LONG_PTR)data);
	if(!m_links++){
		HFONT hfont;
		LOGFONT logft;
		m_cursor_hand = LoadCursor(NULL, IDC_HAND);
		hfont = GetWindowFont(link_control);
		GetObject(hfont, sizeof(logft), &logft);
		logft.lfUnderline = 1;
		m_link_font_underline = CreateFontIndirect(&logft);
	}
	m_proc_static = SubclassWindow(link_control, LinkControlProc);
}

LRESULT LinkControl_OnCtlColorStatic(HWND hwnd, WPARAM wParam, LPARAM lParam) {
	if(m_link_font_hovered)
		SetTextColor((HDC)wParam,RGB(255,0,0));
	else
		SetTextColor((HDC)wParam,RGB(0,0,255));
	return DefWindowProc(hwnd, WM_CTLCOLORSTATIC, wParam, lParam);
}

LRESULT CALLBACK LinkControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
	switch(msg){
	case WM_DESTROY:{
		link_data* data = (link_data*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		free(data);
		if(!--m_links)
			DeleteFont(m_link_font_underline);
		break;}
	//case WM_LINKSETTARGET:
	//	return 0;
	case WM_MOUSELEAVE: {
		HFONT hfont = m_link_font_hovered;
		m_link_font_hovered = NULL;
		SetWindowFont(hwnd, hfont, 1);
		return 0;}
	case WM_SETCURSOR:
		if(!m_link_font_hovered){
			TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, 0, 10};
			tme.hwndTrack = hwnd;
			m_link_font_hovered = GetWindowFont(hwnd);
			SetCursor(m_cursor_hand);
			SetWindowFont(hwnd, m_link_font_underline, 1);
			TrackMouseEvent(&tme);
		}
		return TRUE;
//	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:{
		wchar_t str[MAX_PATH];
		wchar_t* offset;
		link_data* data = (link_data*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		HWND parent = GetParent(hwnd);
		if(data->flags != LCF_NOTIFYONLY){
			str[0] = '\0';
			offset = str;
			if(data->flags & LCF_HTTP){
				offset += wsprintf(offset, FMT("http://"));
			}else if(data->flags & LCF_HTTPS){
				offset += wsprintf(offset, FMT("https://"));
			}else if(data->flags & LCF_MAIL){
				offset += wsprintf(offset, FMT("mailto:"));
			}else if(data->flags & LCF_RELATIVE){
				memcpy(offset, api.root, api.root_size);
				offset += api.root_len;
				*offset++ = '\\';
			}
			
			if(data->target){
				wcsncpy_s(offset, (_countof(str)-(offset-str)), data->target, _TRUNCATE);
			}else{
				GetWindowText(hwnd, offset, (int)(_countof(str)-(offset-str)));
			}
			
			if(str[0]){
				if(data->flags & LCF_PARAMS)
					api.ExecFile(str, parent);
				else
					api.Exec(str, NULL, parent);
			}
		}
		
		if(data->flags & LCF_NOTIFY)
			SendMessage(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd),STN_CLICKED), (LPARAM)hwnd);
		return 0;}
//	case WM_RBUTTONUP:
	case WM_LBUTTONUP:
//	case WM_RBUTTONDBLCLK:
	case WM_LBUTTONDBLCLK:
		return 0;
	}
	return CallWindowProc(m_proc_static, hwnd, msg, wParam, lParam);
}

/*
	COLOR BOXES
*/
static short m_color_boxes = 0;
static HBITMAP m_transparent_pattern;
static HBRUSH m_transparent_brush;

typedef struct {
	COLORREF col;
	const wchar_t* name;
} syscolor_t;
static const syscolor_t m_syscolor[]={
	{TCOLOR_DEFAULT,L"Default"},
	{TCOLOR_TRANSPARENT,L"Transparent"},
	{TCOLOR_THEME,L"Theme color"},
	{TCOLOR_THEME_DARK,L"Theme color (dark)"},
	{TCOLOR_THEME_ALPHA,L"Theme color w/ alpha"},
//	{TCOLOR_THEME_BG,L"Theme bg color"},
	
	{COLOR_3DFACE,L"3DFACE"},
	{COLOR_3DSHADOW,L"3DSHADOW"},
	{COLOR_3DHILIGHT,L"3DHILIGHT"},
	{COLOR_BTNTEXT,L"BTNTEXT"},
	{COLOR_WINDOWTEXT,L"WINDOWTEXT"},
	{COLOR_INFOTEXT,L"INFOTEXT"},
	{COLOR_INFOBK,L"INFOBK"},
};
static const size_t m_syscolor_num = _countof(m_syscolor);
static const COLORREF m_basecolor[]={
	0x00000080, 0x00008000, 0x00800000,
	0x00008080, 0x00800080, 0x00808000,
	0x000000FF, 0x0000FF00, 0x00FF0000,
	0x0000FFFF, 0x00FF00FF, 0x00FFFF00,
	0x00FFFFFF, 0x00C0C0C0, 0x00808080, 0x00000000,
};
static const size_t m_basecolor_num = _countof(m_basecolor);
static const size_t m_colorstotal = _countof(m_syscolor) + _countof(m_basecolor);

static COLORREF m_usercolors[16] = {
	0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF, 0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,
	0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF, 0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF
};

static WNDPROC m_proc_combo = NULL;
static LRESULT CALLBACK ColorBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ColorBox_Setup(ColorBox boxes[], size_t num)
{
	COLORREF col;
	size_t idx;
	while(num--) {
		if(!m_color_boxes++){
			const int box_size = 4;
			RECT rc;
			HDC wnd_dc = GetDC(boxes[num].hwnd);
			HDC draw_dc = CreateCompatibleDC(wnd_dc);
			
			m_transparent_pattern = CreateCompatibleBitmap(wnd_dc, box_size*2, box_size*2);
			ReleaseDC(boxes[num].hwnd, wnd_dc);
			SelectBitmap(draw_dc, m_transparent_pattern);
			rc.left=rc.top = 0;
			rc.right=rc.bottom = box_size;
			FillRect(draw_dc, &rc, GetStockBrush(GRAY_BRUSH));
			rc.left += box_size; rc.right += box_size;
			FillRect(draw_dc, &rc, GetStockBrush(WHITE_BRUSH));
			rc.top += box_size; rc.bottom += box_size;
			FillRect(draw_dc, &rc, GetStockBrush(GRAY_BRUSH));
			rc.left -= box_size; rc.right -= box_size;
			FillRect(draw_dc, &rc, GetStockBrush(WHITE_BRUSH));
			m_transparent_brush = CreatePatternBrush(m_transparent_pattern);
			DeleteDC(draw_dc);
		}
		m_proc_combo = SubclassWindow(boxes[num].hwnd, ColorBoxProc);
//		ComboBox_SetDroppedWidth(boxes[num].hwnd, 130);
		ComboBox_SetDroppedWidth(boxes[num].hwnd, 155);
		// add sys colors
		for(idx=0; idx<m_syscolor_num; ++idx){
			ComboBox_AddString(boxes[num].hwnd, (size_t)TCOLOR(m_syscolor[idx].col));
		}
		// add base colors
		for(idx=0; idx<m_basecolor_num; ++idx)
			ComboBox_AddString(boxes[num].hwnd, (size_t)m_basecolor[idx]);
		// select last used color
		col = boxes[num].color;
		for(idx=0; idx<m_colorstotal; ++idx) {
			if(col==(COLORREF)ComboBox_GetItemData(boxes[num].hwnd, idx))
				break;
		}
		if(idx==m_colorstotal) // Add the Selected Custom Color
			ComboBox_AddString(boxes[num].hwnd, (size_t)col);
		ComboBox_SetCurSel(boxes[num].hwnd, idx);
	}
}

LRESULT CALLBACK ColorBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
	switch(msg){
	case WM_DESTROY:{
		if(!--m_color_boxes){
			DeleteBrush(m_transparent_brush);
			DeleteBitmap(m_transparent_pattern);
		}
		break;}
	case WM_MEASUREITEM:
		return ColorBox_OnMeasureItem(wParam, lParam);
	case WM_DRAWITEM:
		return ColorBox_OnDrawItem(wParam, lParam);
	case WM_KEYDOWN:
		if(lParam&0x4000)
			break; // repeated msg, key was down
		if(wParam == 'C' || wParam == 'X' || wParam == 'V'){
			if(GetAsyncKeyState(VK_CONTROL)&0x8000){
				SendMessage(hwnd, (wParam=='V'?WM_PASTE:WM_COPY), 0, 0);
				return 0;
			}
		}
		break;
	case WM_CUT:
	case WM_COPY:{
		char color_str[8];
		HGLOBAL hg;
		char* pbuf;
		unsigned color = api.GetColor(ColorBox_GetColorRaw(hwnd), TCOLORFLAG_RAW2);
		color = ((color&0xff)<<16) | (color&0xff00) | ((color&0xff0000)>>16);
		__pragma(warning(suppress:4996)) sprintf(color_str, "#%06x", color);
		if(!OpenClipboard(hwnd))
			return 0;
		EmptyClipboard();
		hg = GlobalAlloc(GMEM_DDESHARE, 8);
		pbuf = (char*)GlobalLock(hg);
		memcpy(pbuf, color_str, 8);
		GlobalUnlock(hg);
		SetClipboardData(CF_TEXT, hg);
		CloseClipboard();
		return 0;}
	case WM_PASTE:{
		HGLOBAL hg;
		const char* pbuf;
		size_t size;
		if(!OpenClipboard(hwnd))
			return 0;
		for(;;){ // only once loop, MSVC can't handle do{}while(0)
			hg = GetClipboardData(CF_TEXT);
			if(!hg)
				break;
			pbuf = (const char*)GlobalLock(hg);
			if(!pbuf)
				break;
			size = GlobalSize(hg);
			if(size > 6){ // could be a hex color (HTML format)
				unsigned color = 0;
				if(pbuf[0] == '#' && size > 7)
					++pbuf;
				for(size=6; size; ){
					char c = (char)toupper(pbuf[--size]);
					color <<= 8;
					if(c >= 'A' && c <= 'F'){
						color |= c-('A'-10);
					}else if(c >= '0' && c <= '9'){
						color |= c-'0';
					}else
						break;
					c = (char)toupper(pbuf[size-1]); // in case we break;
					if(c >= 'A' && c <= 'F'){
						color |= (c-('A'-10)) << 4;
					}else if(c >= '0' && c <= '9'){
						color |= (c-'0') << 4;
					}else
						break;
					--size;
				}
				if(!size){ // valid hex value
					for(size=TCOLOR_BEGIN_; size<TCOLOR_END_; ++size){
						if(color == (api.GetColor(TCOLOR(size),TCOLORFLAG_RAW2) & 0xffffff))
							break;
					}
					if(size != TCOLOR_END_)
						ColorBox_SetColor(hwnd, TCOLOR(size));
					else
						ColorBox_SetColor(hwnd, color);
					PostMessage(GetParent(hwnd),WM_COMMAND, MAKEWPARAM(GetWindowID(hwnd),CBN_SELCHANGE), (LPARAM)hwnd);
				}
			}
			GlobalUnlock(hg);
			break;
		}
		CloseClipboard();
		return 0;}
	}
	return CallWindowProc(m_proc_combo, hwnd, msg, wParam, lParam);
}

/*------------------------------------------------
--------------------------------------------------*/
LRESULT ColorBox_OnMeasureItem(WPARAM wParam, LPARAM lParam) {
	MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
	(void)wParam;
//	pmis->itemWidth = MulDiv(80, LOWORD(GetDialogBaseUnits()), 4);
	pmis->itemHeight = MulDiv(7, HIWORD(GetDialogBaseUnits()), 8);
	return 1;
}
LRESULT ColorBox_OnDrawItem(WPARAM wParam, LPARAM lParam) {
	const int color_margin = 1;
	DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
	HBRUSH hbr;
	COLORREF col;
	TEXTMETRIC tm;
	int vert_pos;
	wchar_t hexcolor[8];
	const wchar_t* label;
	BLENDFUNCTION blend = {AC_SRC_OVER, 0, 0, 0};
	HDC draw_dc;
	HBITMAP draw_hbmp;
	RECT draw_rc = {0, 0, 30, 0/*bottom*/};
	RECT color_rc;
	HBRUSH oldBrush;
	
	(void)wParam;
	
	draw_rc.bottom = pdis->rcItem.bottom-pdis->rcItem.top - color_margin*2;
	color_rc.left = pdis->rcItem.left + color_margin;
	color_rc.top = pdis->rcItem.top + color_margin;
	color_rc.right = color_rc.left + draw_rc.right;
	color_rc.bottom = color_rc.top + draw_rc.bottom;
	
	if(IsWindowEnabled(pdis->hwndItem)) {
//		col = api.GetColor((COLORREF)pdis->itemData, TCOLORFLAG_RAW);
//		if((col&TCOLOR_MASK)==TCOLOR_MAGIC)
//			col &= 0x00ffffff;
		col = api.GetColor((COLORREF)pdis->itemData,0);
	} else
		col = GetSysColor(COLOR_3DFACE);
	
	if(pdis->itemAction & (ODA_DRAWENTIRE|ODA_SELECT)) {
		// brush alignment & fix
		if(pdis->itemAction & ODA_DRAWENTIRE && !(pdis->itemState & ODS_COMBOBOXEDIT)) {
			SetBrushOrgEx(pdis->hDC, color_rc.left+2, color_rc.top+1, NULL);
		}else
			SetBrushOrgEx(pdis->hDC, color_rc.left+1, color_rc.top, NULL);
		// print sys color names and fill bg
		if(pdis->itemState & ODS_SELECTED){
			SetBkColor(pdis->hDC, GetSysColor(COLOR_HIGHLIGHT));
			SetTextColor(pdis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
		}else{
			SetBkColor(pdis->hDC, GetSysColor(COLOR_WINDOW));
			SetTextColor(pdis->hDC, GetSysColor(COLOR_WINDOWTEXT));
		}
		if(pdis->itemID < m_syscolor_num)
			label = m_syscolor[pdis->itemID].name;
		else{
			unsigned color = ((col&0xff)<<16) | (col&0xff00) | ((col&0xff0000)>>16);
			wsprintf(hexcolor, FMT("#%06x"), color);
			label = hexcolor;
		}
		GetTextMetrics(pdis->hDC, &tm);
		vert_pos = (pdis->rcItem.bottom - pdis->rcItem.top - tm.tmHeight)/2;
		ExtTextOut(pdis->hDC, pdis->rcItem.left + 34, pdis->rcItem.top + vert_pos, ETO_CLIPPED|ETO_OPAQUE, &pdis->rcItem,
				label, (int)wcslen(label), NULL);
		
		// draw color
		hbr = CreateSolidBrush(col&0x00ffffff);
		blend.SourceConstantAlpha = 255 - (col>>24);
		if(blend.SourceConstantAlpha != 255){ // with transparency
			draw_dc = CreateCompatibleDC(pdis->hDC);
			draw_hbmp = CreateCompatibleBitmap(pdis->hDC, draw_rc.right, draw_rc.bottom);
			SelectBitmap(draw_dc, draw_hbmp);
			oldBrush = SelectBrush(pdis->hDC, m_transparent_brush);
//			RoundRect(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom, 8,5);
			Rectangle(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom);
			SelectBrush(pdis->hDC, oldBrush);
			oldBrush = SelectBrush(draw_dc, hbr);
//			RoundRect(draw_dc, draw_rc.left, draw_rc.top, draw_rc.right, draw_rc.bottom, 8,5);
			Rectangle(draw_dc, draw_rc.left, draw_rc.top, draw_rc.right, draw_rc.bottom);
			SelectBrush(draw_dc, oldBrush);
			AlphaBlend(pdis->hDC, color_rc.left,color_rc.top,draw_rc.right,draw_rc.bottom, draw_dc, 0,0,draw_rc.right,draw_rc.bottom, blend);
			DeleteDC(draw_dc);
			DeleteObject(draw_hbmp);
		}else{ // simple draw
			oldBrush = SelectBrush(pdis->hDC, hbr);
//			RoundRect(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom, 8,5);
			Rectangle(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom);
			SelectBrush(pdis->hDC, oldBrush);
		}
		DeleteObject(hbr);
		// "Default" color, draw right side (transparency)
		if(!pdis->itemID){
			oldBrush = SelectBrush(pdis->hDC, m_transparent_brush);
			color_rc.left += draw_rc.right/2 + 1;
//			RoundRect(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom, 8,5);
			Rectangle(pdis->hDC, color_rc.left, color_rc.top, color_rc.right, color_rc.bottom);
			SelectBrush(pdis->hDC, oldBrush);
		}
		
		// draw focus rect / selection
		if(pdis->itemState & ODS_FOCUS)
			DrawFocusRect(pdis->hDC, &pdis->rcItem);
	}
	return 1;
}

int ColorBox_ChooseColor(HWND button)
{
	CHOOSECOLOR cc = {sizeof(CHOOSECOLOR)};
	COLORREF color;
	HWND color_cb = GetWindow(button, GW_HWNDPREV);
	size_t idx;
	
	color = api.GetColor((COLORREF)ComboBox_GetItemData(color_cb, ComboBox_GetCurSel(color_cb)), TCOLORFLAG_RAW2);
	
	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms646908%28v=vs.85%29.aspx
	//   https://msdn.microsoft.com/en-us/library/ms646869%28v=vs.85%29.aspx
	cc.hwndOwner = GetParent(button);
	cc.rgbResult = color & 0x00ffffff;
	cc.lpCustColors = m_usercolors;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	
	if(!ChooseColor(&cc))
		return 0;
	
	idx = ComboBox_GetCurSel(color_cb);
	if(idx != (size_t)ColorBox_SetColor(color_cb, cc.rgbResult) || cc.rgbResult != color)
		PostMessage(cc.hwndOwner,WM_COMMAND, MAKEWPARAM(GetWindowID(color_cb),CBN_SELCHANGE), (LPARAM)color_cb);
	
	return 1;
}

int ColorBox_SetColor(HWND box, COLORREF new_color){
	size_t idx;
	
	for(idx=0; idx<m_colorstotal; ++idx) {
		if(new_color == (COLORREF)ComboBox_GetItemData(box,idx))
			break;
	}
	if(idx == m_colorstotal){
		if(ComboBox_GetCount(box) == m_colorstotal)
			ComboBox_AddString(box, (size_t)new_color);
		else
			ComboBox_SetItemData(box, idx, new_color);
	}
	ComboBox_SetCurSel(box, idx);
	return (int)idx;
}
