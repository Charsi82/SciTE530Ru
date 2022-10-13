// twl_toolbar.cpp
// implements the following common controls
//   TImageList
//   TTabControl
//   TListView
//   TTreeView
#include <windows.h>
#include <commctrl.h>
#define IS_IMPLEMENTATION
#include "twl.h"
#include "twl_menu.h"
#include "twl_imagelist.h"
#include "twl_listview.h"
#include "twl_tab.h"
#include "twl_treeview.h"
#include <stdio.h>
#include <io.h>
#include "utf.h"
#include <Uxtheme.h>

void* ApplicationInstance();

static size_t gID = 445560;

static HWND create_common_control(TWin* form, pchar winclass, DWORD style, int height = -1)
{
	int w = CW_USEDEFAULT, h = CW_USEDEFAULT;
	if (height != -1) { w = 100; h = height; }
	return CreateWindowEx(0L,   // No extended styles.
		winclass, L"", WS_CHILD | style,
		0, 0, w, h,
		(HWND)form->handle(),                  // Parent window of the control.
		(HMENU)(void*)gID++,
		(HINSTANCE)ApplicationInstance(),             // Current instance.
		NULL);
}

static HICON load_icon(pchar file)
{
	return (HICON)LoadImage((HINSTANCE)ApplicationInstance(), file, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_LOADTRANSPARENT);
}

static HBITMAP load_bitmap(pchar file)
{
	HBITMAP res = (HBITMAP)LoadImage(NULL/*ApplicationInstance()*/, file, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_LOADTRANSPARENT);
	if (!res) {
		/*int _res =*/ 
		errno_t err = _waccess_s(file, 0);
		if (err == 0) log_add("'load_bitmap':file not exist");
	}
	return res;
}

///// TImageList class

static int icon_size(bool s)
{
	return s ? 16 : 32;
}

TImageList::TImageList(int cx, int cy)
{
	create(cx, cy);
	m_small_icons = cx == icon_size(true);
}

TImageList::TImageList(bool s /*= true*/)
	: m_small_icons(s)
{
	int cx = icon_size(s);
	int cy = cx;
	create(cx, cy);
}

void TImageList::create(int cx, int cy)
{
	m_handle = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, 0, 32);
}

int TImageList::add_icon(pchar iconfile)
{
	HICON hIcon = load_icon(iconfile);
	if (!hIcon) return -1;  // can't find icon
	return ImageList_AddIcon(m_handle, hIcon);
}

int TImageList::add(pchar bitmapfile, COLORREF mask_clr)
{
	HBITMAP hBitmap = load_bitmap(bitmapfile);
	if (!hBitmap) return -1;  // can't find bitmap
	if (mask_clr != 1)
		return ImageList_AddMasked(m_handle, hBitmap, mask_clr);
	else
		return ImageList_Add(m_handle, hBitmap, NULL);
}

int TImageList::load_icons_from_module(pchar mod, bool icon_small)
{
	int res = ExtractIconEx(mod, -1, 0, 0, 1);
	if (!res) return 0;
	HICON hIcon;
	int icon_cnt = 0;
	//while (ExtractIconEx(mod, icon_cnt++, icon_small ? 0 : &hIcon, icon_small ? &hIcon : 0, 1))
	while (icon_small ? ExtractIconEx(mod, icon_cnt++,  0, &hIcon, 1) : ExtractIconEx(mod, icon_cnt++, &hIcon, 0, 1))
		ImageList_AddIcon(m_handle, hIcon);
	return icon_cnt-1;
}

/*int TImageList::load_icons_from_module(pchar mod)
{
	HMODULE lib = LoadLibrary(mod);
	HINSTANCE hInst = GetModuleHandle(mod);
	HICON hIcon;
	int cx = icon_size(m_small_icons);
	int cy = cx;
	int i = 0;
	//while (hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(++i), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_DEFAULTSIZE))
	if (hIcon = (HICON)LoadImage(hInst, L"0", IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_DEFAULTSIZE))
		ImageList_AddIcon(m_handle, hIcon);
	while (hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(++i), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_DEFAULTSIZE))
		ImageList_AddIcon(m_handle, hIcon);
	if (lib) FreeLibrary(lib);
	return i-1;
}*/

void TImageList::set_back_colour(COLORREF clrRef)
{
	ImageList_SetBkColor(m_handle, clrRef);
}

void TImageList::load_shell_icons()
{
	load_icons_from_module(L"%windir%\\shell32.dll");
	set_back_colour(CLR_NONE);
}

////// TListView

TListViewB::TListViewB(TWin* form, bool large_icons, bool multiple_columns, bool single_select)
{
	DWORD style = WS_CHILD;
	if (large_icons) {
		style |= (LVS_ICON | LVS_AUTOARRANGE);
	}
	else {
		style |= LVS_REPORT;
		if (single_select) {
			style |= LVS_SINGLESEL;
		}
		if (!multiple_columns) {
			style |= LVS_NOCOLUMNHEADER;
			//add_column("*",1000);
		}
	}

	style |= LVS_SHOWSELALWAYS;

	// Create the list view control.
	set(create_common_control(form, WC_LISTVIEW, style));
	parent_win = form;
	m_custom_paint = false;
	m_has_images = false;
	m_last_col = 0;
	m_last_row = -1;
	m_bg = 0;
	m_fg = 0;
	send_msg(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT); // Set style
}

void TListViewB::set_image_list(TImageList* il_small, TImageList* il_large)
{
	if (il_small) send_msg(LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)il_small->handle());
	if (il_large) send_msg(LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)il_large->handle());
	m_has_images = true;
}

void TListViewB::add_column(pchar label, int width)
{
	LVCOLUMN lvc{};
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;   // left-align, by default
	lvc.cx = width;
	lvc.pszText = (wchar_t*)label;
	lvc.iSubItem = m_last_col;

	ListView_InsertColumn((HWND)m_hwnd, m_last_col, &lvc);
	m_last_col++;
}

void TListViewB::set_foreground(COLORREF colour)
{
	send_msg(LVM_SETTEXTCOLOR, 0, (LPARAM)colour);
	m_fg = colour;
}

void TListViewB::set_background(COLORREF colour)
{
	send_msg(LVM_SETBKCOLOR, 0, (LPARAM)colour);
	m_bg = colour;
	m_custom_paint = true;
}

void TListViewB::set_theme(bool explorer)
{
	SetWindowTheme((HWND)handle(), explorer ? L"Explorer" : L"Normal", NULL);
}

int TListViewB::columns()
{
	return m_last_col;
}

void TListViewB::autosize_column(int col, bool by_contents)
{
	ListView_SetColumnWidth((HWND)m_hwnd, col, by_contents ? LVSCW_AUTOSIZE : LVSCW_AUTOSIZE_USEHEADER);
}

void TListViewB::start_items()
{
	m_last_row = -1;
}

int TListViewB::add_item_at(int i, pchar text, int idx, void* data)
{
	LVITEM lvi{};
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
	if (m_has_images) lvi.mask |= LVIF_IMAGE;
	lvi.state = 0;
	lvi.stateMask = 0;
	lvi.pszText = (wchar_t*)text;
	lvi.lParam = (LPARAM)data;
	lvi.iItem = i;
	lvi.iImage = idx;                // image list index
	lvi.iSubItem = 0;

	ListView_InsertItem((HWND)m_hwnd, &lvi);
	return i;
}

int TListViewB::add_item(pchar text, int idx, void* data)
{
	m_last_row++;
	return add_item_at(m_last_row, text, idx, data);
}

void TListViewB::add_subitem(int i, pchar text, int idx)
{
	ListView_SetItemText((HWND)m_hwnd, i, idx, (wchar_t*)text);
}

void TListViewB::delete_item(int i)
{
	ListView_DeleteItem((HWND)m_hwnd, i);
}

void TListViewB::select_item(int i)
{
	if (i != -1)
	{
		ListView_SetItemState((HWND)m_hwnd, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible((HWND)m_hwnd, i, true);
	}
	else
		ListView_SetItemState((HWND)m_hwnd, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

void TListViewB::get_item_text(int i, wchar_t* buff, int buffsize)
{
	ListView_GetItemText((HWND)m_hwnd, i, 0, buff, buffsize);
}

void* TListViewB::get_item_data(int i)
{
	LVITEM lvi;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = i;
	lvi.iSubItem = 0;
	ListView_GetItem((HWND)m_hwnd, &lvi);
	return (void*)lvi.lParam;
}

int TListViewB::selected_id()
{
	return (int)send_msg(LVM_GETNEXTITEM, (WPARAM)(-1), LVNI_FOCUSED);
}

int TListViewB::next_selected_id(int i)
{
	return (int)send_msg(LVM_GETNEXTITEM, i, LVNI_SELECTED);
}

int TListViewB::count()
{
	return (int)send_msg(LVM_GETITEMCOUNT);
}

int TListViewB::selected_count()
{
	return (int)send_msg(LVM_GETSELECTEDCOUNT);
}

void TListViewB::clear()
{
	send_msg(LVM_DELETEALLITEMS);
	m_last_row = -1;
}

static int list_custom_draw(void* lParam, COLORREF fg, COLORREF bg)
{
	LPNMLVCUSTOMDRAW  lplvcd = (LPNMLVCUSTOMDRAW)lParam;

	if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT)
		// Request prepaint notifications for each item.
		return CDRF_NOTIFYITEMDRAW;

	if (lplvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
		lplvcd->clrText = fg;
		lplvcd->clrTextBk = bg;
		return CDRF_NEWFONT;
	}
	return 0;
}

int TListViewB::handle_notify(void* lparam)
{
	LPNMHDR np = (LPNMHDR)lparam;
	int id = selected_id();
	switch (np->code) {
	case LVN_ITEMCHANGED:
		handle_select(id);
		return 1;
	case NM_DBLCLK:
	{
		LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lparam;
		LVHITTESTINFO pInfo;
		pInfo.pt = lpnmitem->ptAction;
		ListView_SubItemHitTest((HWND)handle(), &pInfo);

		int i = pInfo.iItem;
		int j = pInfo.iSubItem;
		wchar_t buffer[10]{};
		LVITEM item;
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.iItem = i;
		item.iSubItem = j;
		item.cchTextMax = sizeof(buffer) / sizeof(buffer[0]);
		item.pszText = buffer;
		ListView_GetItem((HWND)handle(), &item);
		if (i > -1)
			handle_double_click(i, j, UTF8FromString(std::wstring(buffer)).c_str());
		return 1;
	}
	case LVN_KEYDOWN:
		handle_onkey(((LPNMLVKEYDOWN)lparam)->wVKey);
		return 0;  // ignored, anyway
	case NM_RCLICK:
		//send_msg(WM_CHAR,VK_ESCAPE,0);
		return handle_rclick(id);
	case NM_SETFOCUS:
		handle_onfocus(true);
		return 1;
	case NM_KILLFOCUS:
		handle_onfocus(false);
		return 1;
	case NM_CUSTOMDRAW:
		if (m_custom_paint) {
			return list_custom_draw(lparam, m_fg, m_bg);
		}
		return 0;
	}
	return 0;
}

TListView::TListView(TEventWindow* form, bool large_icons, bool multiple_columns, bool single_select)
	: TListViewB(form, large_icons, multiple_columns, single_select),
	m_form(form), m_on_select(NULL), m_on_key(NULL), m_on_double_click(NULL)
{
}

void TListView::handle_select(intptr_t i)
{
	if (m_on_select) {
		(m_form->*m_on_select)(i);
	}
}

void TListView::handle_double_click(int row, int col, const char* s)
{
	if (m_on_double_click) {
		(m_form->*m_on_double_click)(row, col, s);
	}
}

void TListView::handle_onkey(int i)
{
	if (m_on_key) {
		(m_form->*m_on_key)(i);
	}
}

TTabControl::TTabControl(TEventWindow* _form, bool multiline) : form(_form)
{
	// Create the tab control.
	DWORD style = WS_CHILD;
	if (multiline) style |= TCS_MULTILINE;
	set(create_common_control(form, WC_TABCONTROL, style | TCS_TOOLTIPS | TCS_TABS, 25));
	send_msg(WM_SETFONT, (WPARAM)::GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);
	has_image = false;
	m_index = 0;
}

void TTabControl::add(pchar caption, void* data, int image_idx /*= -1*/)
{
	TCITEM item{};
	item.mask = TCIF_TEXT | TCIF_PARAM;
	item.pszText = (wchar_t*)caption;
	item.lParam = (LPARAM)data;
	if (has_image)
	{
		item.mask |= TCIF_IMAGE;
		item.iImage = image_idx;
	}
	send_msg(TCM_INSERTITEM, m_index++, (LPARAM)&item);
}

void* TTabControl::get_data(int idx)
{
	if (idx == -1) idx = selected();
	TCITEM item{};
	item.mask = TCIF_PARAM;
	send_msg(TCM_GETITEM, idx, (LPARAM)&item);
	return (void*)item.lParam;
}

void TTabControl::remove(int idx /*= -1*/)
{
	send_msg(idx > -1 ? TCM_DELETEITEM : TCM_DELETEALLITEMS, idx);
}

int TTabControl::getRowCount() const {
	return send_msg(TCM_GETROWCOUNT);
}

void TTabControl::selected(int idx)
{
	send_msg(TCM_SETCURSEL, idx);
	NMHDR nmh;
	nmh.code = TCN_SELCHANGE;
	nmh.idFrom = GetDlgCtrlID((HWND)handle());
	nmh.hwndFrom = (HWND)handle();
	SendMessage(GetParent((HWND)handle()), WM_NOTIFY, nmh.idFrom, (LPARAM)&nmh);
}

int TTabControl::selected()
{
	return (int)send_msg(TCM_GETCURSEL);
}

int TTabControl::handle_notify(void* p)
{
	LPNMHDR np = (LPNMHDR)p;
	int id = selected();
	switch (np->code) {
	case TCN_SELCHANGE:
		handle_select(id);
		return 1;
	case NM_RCLICK:
		return handle_rclick(id);
	case TTN_NEEDTEXT:
		LPNMTTDISPINFO ttn = (LPNMTTDISPINFO)p;
		TCHAR buf[MAX_PATH]{};
		TCITEM item{};
		item.mask = TCIF_TEXT;
		item.pszText = buf;
		item.cchTextMax = MAX_PATH;
		send_msg(TCM_GETITEM, np->idFrom, (LPARAM)&item);
		lstrcpy(buf, item.pszText);
		ttn->lpszText = buf;
		return 1;
	}
	return 0;
}

int TTabControl::load_icons(pchar path, bool small_size) {
	TImageList il(small_size);
	int icons_loaded = il.load_icons_from_module(path, small_size);
	TabCtrl_SetImageList((HWND)handle(), il.handle());
	has_image = true;
	return icons_loaded;
}

TTreeView::TTreeView(TWin* form, bool has_lines)
	: m_form(form), m_has_images(false)
{
	DWORD style = TVS_HASBUTTONS;
	//style |= TVS_INFOTIP;
	if (has_lines) style |= (TVS_HASLINES | TVS_LINESATROOT);
	set(create_common_control(form, WC_TREEVIEW, style));
}

void TTreeView::set_theme(bool explorer)
{
	SetWindowTheme((HWND)handle(), explorer?L"Explorer":L"Normal", NULL);
}

void TTreeView::makeLabelEditable(bool toBeEnabled)
{
	DWORD dwNewStyle = (DWORD)GetWindowLongPtr((HWND)handle(), GWL_STYLE);
	if (toBeEnabled)
		dwNewStyle |= TVS_EDITLABELS;
	else
		dwNewStyle &= ~TVS_EDITLABELS;
	::SetWindowLongPtr((HWND)handle(), GWL_STYLE, dwNewStyle);
}

void TTreeView::set_image_list(TImageList* il, bool normal)
{
	send_msg(TVM_SETIMAGELIST, normal ? TVSIL_NORMAL : TVSIL_STATE, (LPARAM)il->handle());
	m_has_images = true;
}

int TTreeView::load_icons(pchar path, bool small_size) {
	TImageList il(small_size);
	int icons_loaded = il.load_icons_from_module(path, small_size);
	set_image_list(&il);
	return icons_loaded;
}

void TTreeView::set_foreground(COLORREF clr)
{
	send_msg(TVM_SETTEXTCOLOR, 0, clr);
}

void TTreeView::set_background(COLORREF clr)
{
	send_msg(TVM_SETBKCOLOR, 0, clr);
}

Handle TTreeView::get_root()
{
	return TreeView_GetRoot((HWND)handle()); // send_msg(TVM_get, 0, clr);
}

Handle TTreeView::get_next(Handle itm)
{
	return TreeView_GetNextSibling((HWND)handle(), itm); // send_msg(TVM_get, 0, clr);
}

void TTreeView::iterate_childs(Handle itm)
{
	for (Handle tvProj = get_root();
		tvProj != NULL;
		tvProj = get_next(tvProj))
	{
		iterate_childs(tvProj);

		TVITEM item_p{};
		item_p.mask = TVIF_PARAM;
		item_p.hItem = (HTREEITEM)tvProj;
		TreeView_GetItem((HWND)handle(), &item_p);
		if(int data = item_p.lParam)
			clean_data(data);
	}
}

void TTreeView::remove_item(Handle itm)
{
	iterate_childs(itm);
	TreeView_DeleteItem((HWND)handle(), itm);
}

void TTreeView::clear()
{
	iterate_childs(get_root());
	TreeView_DeleteAllItems((HWND)handle());
}

Handle TTreeView::add_item(pchar caption, Handle parent, int idx1, int idx2, Handle data)
{
	TVITEM item{};
	item.mask = TVIF_TEXT | TVIF_CHILDREN;
	if (m_has_images) {
		item.mask |= (TVIF_IMAGE | TVIF_SELECTEDIMAGE);
		if (idx2 == -1) idx2 = idx1;
		item.iImage = idx1;
		item.iSelectedImage = idx2;
	}
	item.pszText = (wchar_t*)caption;
	item.cchTextMax = MAX_PATH;
	item.cChildren = 0;
	if (data)
	{
		item.mask |= TVIF_PARAM;
		item.lParam = (LPARAM)data;
	}

	TVINSERTSTRUCT tvsi{};
	tvsi.item = item;
	tvsi.hInsertAfter = TVI_LAST; //(HTREEITEM)parent;

	if (parent)
	{
		tvsi.hParent = (HTREEITEM)parent;

		// для родителького элемента указываем что он риодительский 
		TVITEM item_p{};
		item_p.mask = TVIF_CHILDREN;
		item_p.hItem = (HTREEITEM)parent;
		item_p.cChildren = 1;
		TreeView_SetItem((HWND)handle(), &item_p);
	}
	else
	{
		tvsi.hParent = TVI_ROOT;
	}
	return TreeView_InsertItem((HWND)handle(), &tvsi);
}

int TTreeView::get_itm_data(Handle pn)
{
	//if (pn == NULL) pn = selected();
	TVITEM item{};
	item.mask = TVIF_PARAM;
	item.hItem = (HTREEITEM)pn;
	item.lParam = 0;
	send_msg(TVM_GETITEM, 0, (LPARAM)&item);
	return (int)item.lParam;
}

void TTreeView::select(Handle p)
{
	send_msg(TVM_SELECTITEM, TVGN_CARET, (LPARAM)p);
}

Handle TTreeView::get_selected()
{
	return TreeView_GetSelection((HWND)handle());
}

void TTreeView::set_itm_text(void* itm, pchar str) {
	TVITEM tvi{};
	tvi.pszText = (LPWSTR)str;
	tvi.cchTextMax = MAX_PATH;
	tvi.mask = TVIF_TEXT;
	tvi.hItem = (HTREEITEM)itm;
	send_msg(TVM_SETITEM, 0, (LPARAM)&tvi);
}

gui_string TTreeView::get_itm_text(Handle itm) {
	TCHAR buffer[MAX_PATH]{};
	TVITEM tvi{};
	tvi.pszText = buffer;
	tvi.cchTextMax = MAX_PATH;
	tvi.mask = TVIF_TEXT;
	tvi.hItem = (HTREEITEM)itm;
	send_msg(TVM_GETITEM, 0, (LPARAM)&tvi);
	return buffer;
}

int TTreeView::handle_notify(void* p)
{
	NMTREEVIEW* np = (NMTREEVIEW*)p;
	Handle item = np->itemNew.hItem;
	switch (np->hdr.code) {
	case NM_RCLICK:
	{
		return handle_rclick(get_selected());
	}
	case NM_DBLCLK:
	{
		handle_dbclick(get_selected());
		break;
	}
	case TVN_SELCHANGED:
	{
		//if (GetActiveWindow() == (HWND)handle()) return 0;
		handle_select(get_selected());
		break;
	}

	//case TVN_GETINFOTIP:
	//{
	//	LPNMTVGETINFOTIP lpGetInfoTip = (LPNMTVGETINFOTIP)p;
	//	lstrcpy(lpGetInfoTip->pszText, L"tiptext");
	//}

	//case TVN_BEGINLABELEDIT:
	//{
	//	auto hEdit = TreeView_GetEditControl((HWND)handle());
	//	SetFocus(hEdit);
	//	break;
	//}
	case TVN_ENDLABELEDIT:
	{
		LPNMTVDISPINFO lp = (LPNMTVDISPINFO)p;
		if (lstrlen(lp->item.pszText))
		{
			return 1;
		}
	}
	}
	return 0;
}
