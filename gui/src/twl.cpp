// compile@ gcc -c -D_WINDOWS_ $(FileNameExt)
// TWL.CPP
/*
 * Main classes of YAWL
 * Steve Donovan, 2003
 * This is GPL'd software, and the usual disclaimers apply.
 * See LICENCE
 * Based on the Tiny, Terminal or Toy Windows Library
 *  Steve Donovan, 1998
 *  based on SWL, 1995.
*/
#define NO_STRICT
#include "twl.h"
#ifdef _WIN32
# include <commctrl.h>
#else
# define LOW_WORD WORD
# define GWL_USERDATA 0
#endif

#include <string>
#include <stdlib.h>
#include <ctype.h>
#include "twl_cntrls.h"
#include <cassert> // for assert
#define MAX_CMD_LINE 120
#define N_CMD_LINE 20
#define EW_CLASSNAME L"EVNTWNDCLSS"
#define POP_WNDCLSS L"WNDCLSS"

static HINSTANCE hInst;
static int CmdShow;
static HANDLE hAccel = 0, hModeless = 0;
wchar_t obuff[BUFSIZE];
static wchar_t buff[MAX_CMD_LINE];
static Point g_mouse_pt, g_mouse_pt_right;

#include <fstream>
std::string UTF8FromString(const std::wstring& s);

#define LOG_ON

#ifdef LOG_ON
class Log
{
private:
	std::string txt_log;
public:
	Log() = default;
	~Log() {
		char pFilename[MAX_PATH]{};
		GetModuleFileNameA(NULL, pFilename, MAX_PATH);
		std::string path = pFilename;
		path.erase(path.find_last_of('\\') + 1);
		std::ofstream outf(path.append("log.txt").c_str());
		outf << txt_log;
		outf.flush();
		outf.close();
	}
	void add(const char* txt) {
		txt_log.append(txt).append("\n");
	};
};

Log g_log;

void log_add(const char* s, int val)
{
	char buf[128];
	sprintf_s(buf, s, val);
	g_log.add(buf);
}
#else
void log_add(const char*, int) {}
#endif

typedef unsigned char byte;

void* ApplicationInstance() { return hInst; }

void subclass_control(TControl* ctrl); // in twl_cntrls.cpp
void remove_subclass_control(TControl* ctrl);

// Miscelaneous functions!!
COLORREF RGBF(float r, float g, float b)
{
	return RGB(byte(255 * r), byte(255 * g), byte(255 * b));
}

int exec(pchar s, int mode)
{
	return (int)(size_t)(ShellExecute(0, L"open", s, NULL, NULL, mode)) > 31;
}

Rect::Rect(const TEventWindow* pwin)
{
	pwin->get_client_rect(*this);
}

bool Rect::is_inside(const Point& p) const
{
	return PtInRect(static_cast<const RECT*>(this), (POINT)p);
}

Point Rect::corner(int idx) const
{
	int x, y;
	switch (idx) {
	case 0: x = left; y = top; break;
	case 1: x = right; y = top; break;
	case 2: x = right; y = bottom; break;
	case 3: x = left; y = bottom; break;
	default: x = y = 0; break;
	}
	return Point(x, y);
}

long Rect::width() const
{
	return right - left;
}

long Rect::height() const
{
	return bottom - top;
}

void Rect::offset_by(int dx, int dy)
{
	OffsetRect(static_cast<RECT*>(this), dx, dy);
}

/// TDC ///////////////

TDC::TDC()
{
	m_hdc = m_pen = m_font = m_brush = NULL;
	m_twin = NULL; m_flags = 0;
}

TDC::~TDC() = default;

void TDC::get(TWin * pw)
//---------------------
{
	if (!pw) pw = m_twin;
	m_hdc = GetDC(pw->handle());
}

void TDC::release(TWin * pw)
//--------------------------
{
	if (!pw) pw = m_twin;
	ReleaseDC(pw->handle(), m_hdc);
}

void TDC::kill()
//--------------
{
	DeleteDC(m_hdc);
}

Handle TDC::select(Handle obj)
//---------------------------
{
	return SelectObject(m_hdc, obj);
}

void TDC::select_stock(int val)
//----------------------------
{
	select(GetStockObject(val));
}

void TDC::xor_pen(bool on_off)
//-----------------
{
	SetROP2(m_hdc, !on_off ? R2_COPYPEN : R2_XORPEN);
}

// this changes both the _pen_ and the _text_ colour
void TDC::set_colour(float r, float g, float b)
//----------------------------
{
	COLORREF rgb = RGBF(r, g, b);
	get();
	SetTextColor(m_hdc, rgb);
	if (m_pen) DeleteObject(m_pen);
	m_pen = CreatePen(PS_SOLID, 0, rgb);
	select(m_pen);
	release();
}

void TDC::set_text_align(int flags)
//---------------------------------
{
	get();
	SetTextAlign(m_hdc, TA_UPDATECP | flags);
	release();
}

void TDC::get_text_extent(pchar text, int& w, int& h, TFont * font)
//----------------------------------------------------------------
{
	SIZE sz;
	HFONT oldfont = 0;
	get();
	if (font) oldfont = select(*font);
	GetTextExtentPoint32(m_hdc, text, wcslen(text), &sz);
	if (font) select(oldfont);
	release();
	w = sz.cx;
	h = sz.cy;
}

// wrappers around common graphics calls
void TDC::draw_text(pchar msg)
//---------------------------------------------------
{
	TextOut(m_hdc, 0, 0, msg, wcslen(msg));
}

void TDC::move_to(int x, int y)
//----------------------------
{
	MoveToEx(m_hdc, x, y, NULL);
}

void TDC::line_to(int x, int y)
//-----------------------------
{
	LineTo(m_hdc, x, y/*,NULL*/);
}

void TDC::rectangle(const Rect & rt)
//---------------------------------
{
	Rectangle(m_hdc, rt.left, rt.top, rt.right, rt.bottom);
}

void TDC::polyline(Point * pts, int npoints)
{
	Polyline(m_hdc, pts, npoints);
}

void TDC::draw_focus_rect(const Rect & rt)
{
	DrawFocusRect(m_hdc, (const RECT*)&rt);
}

void TDC::draw_line(const Point & p1, const Point & p2)
{
	POINT pts[] = { {p1.x,p1.y},{p2.x,p2.y} };
	Polyline(m_hdc, pts, 2);
}

//// TGDIObj

void TGDIObj::destroy()
{
	if (m_hand) DeleteObject(m_hand); m_hand = NULL;
}

//// TFont ////////
#define PLF ((LOGFONT *)m_pfont)

TFont::TFont()
{
	m_pfont = /*(void *)*/ new LOGFONT;
}

TFont::TFont(const TFont & f)
{
	m_pfont =/* (void *)*/ new LOGFONT;
	memcpy(m_pfont, f.m_pfont, sizeof(LOGFONT));
};

TFont::~TFont()
{
	delete /*(LOGFONT *)*/ m_pfont;
}

void TFont::set(pchar spec, int sz, int ftype)
//-------------------------------------------
{
	LOGFONT& lf = */*(LOGFONT *)*/m_pfont;  // define an alias...
	int wt = FW_NORMAL;
	lf.lfHeight = sz;
	if (ftype & BOLD)  wt = FW_BOLD;
	lf.lfWeight = wt;
	lf.lfItalic = (ftype & ITALIC) ? TRUE : FALSE;
	lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lf.lfCharSet = ANSI_CHARSET;
	lf.lfQuality = PROOF_QUALITY;
	lf.lfEscapement = 0;
	lf.lfOrientation = 0;
	lf.lfUnderline = 0;
	lf.lfStrikeOut = 0;
	lstrcpy(lf.lfFaceName, spec);
	create();
}

/// copy constructor
TFont& TFont::operator = (const TFont & f)
//--------------------------------------
{
	memcpy(m_pfont, f.m_pfont, sizeof(LOGFONT));
	create();
	return *this;
}

void TFont::create()
{
	destroy();
	m_hand = CreateFontIndirect(/*(LOGFONT *)*/m_pfont);
}

void  TFont::set_name(pchar name)
{
	lstrcpy(PLF->lfFaceName, name); create();
}

void  TFont::set_size(int pts)
{
	PLF->lfHeight = pts;      create();
}

void  TFont::set_bold()
{
	PLF->lfWeight = FW_BOLD;  create();
}

void  TFont::set_italic()
{
	PLF->lfItalic = TRUE;     create();
}

////// TWin ///////////
TWin::TWin(TWin * parent, pchar winclss, pchar text, int id, DWORD styleEx)
//------------------------------------------------------------------------
{
	DWORD err;
	DWORD style = WS_CHILD | WS_VISIBLE | styleEx;
	HWND hwndChild = CreateWindowEx(WS_EX_LEFT, winclss, text, style,
		0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
		parent->m_hwnd, (HMENU)id, hInst, NULL);
	if (hwndChild == NULL) err = GetLastError();
	set(hwndChild);
}

TWin::~TWin()
{
	//log_add("~TWin");
}

void TWin::update()
//-----------------
{
	UpdateWindow(m_hwnd);
}

void TWin::invalidate(Rect * lprt)
{
	InvalidateRect(m_hwnd, (LPRECT)lprt, TRUE);
}

void TWin::align(Alignment a, int size)
{
	m_align = a;
	if (size > 0 && a != Alignment::alNone) {
		if (a == Alignment::alRight || a == Alignment::alLeft)
			resize(size, 0);
		else
			resize(0, size);
	}
}

void TWin::get_client_rect(Rect & rt) const
{
	GetClientRect(m_hwnd, (LPRECT)&rt);
}

void TWin::get_rect(Rect & rt, bool use_parent_client)
//--------------------------------------------------
{
	GetWindowRect(m_hwnd, (LPRECT)&rt);
	if (use_parent_client) {
		HWND hp = GetParent(m_hwnd);
		MapWindowPoints(NULL, hp, (LPPOINT)&rt, 2);
	}
}

void TWin::map_points(Point * pt, int n, TWin * target_wnd /*= PARENT_WND*/)
{
	HWND hwndTo = (target_wnd == PARENT_WND) ? GetParent(m_hwnd) : target_wnd->handle();
	MapWindowPoints(m_hwnd, hwndTo, (LPPOINT)pt, n);
}

int TWin::width()
//---------------
{
	Rect rt; get_client_rect(rt);
	return rt.right - rt.left;
}

int TWin::height()
//----------------
{
	Rect rt; get_client_rect(rt);
	return rt.bottom - rt.top;
}

void TWin::set_text(pchar str)
//---------------------------
{
	SetWindowText(m_hwnd, str);
}

pchar TWin::get_text(wchar_t* str, int sz)
//-------------------------------------
{
	GetWindowText(m_hwnd, str, sz);
	return str;
}

// These guys work with the specified _child_ of the window

void TWin::set_text(int id, pchar str)
//-------------------------------------
{
	SetDlgItemText(m_hwnd, id, str);
}

void TWin::set_int(int id, int val)
//-------------------------------------
{
	SetDlgItemInt(m_hwnd, id, val, TRUE);
}

pchar TWin::get_text(int id, wchar_t* str, int sz)
//--------------------------------------------
{
	GetDlgItemText(m_hwnd, id, str, sz);  return str;
}

int TWin::get_int(int id)
//-----------------------
{
	BOOL success;
	return (int)GetDlgItemInt(m_hwnd, id, &success, TRUE);
}

TWin* TWin::get_twin(int id)
//--------------------
{
	HWND hwnd = GetDlgItem(m_hwnd, id);
	if (hwnd) {
		// Extract the 'this' pointer, if it exists
		TWin* pwin = (TWin*)GetWindowLongPtr(hwnd, 0);
		// if not, then just wrap up the handle
		if (!pwin) pwin = new TWin(hwnd);
		return pwin;
	}
	else return NULL;
}

TWin* TWin::get_active_window()
{
	return new TWin(GetActiveWindow());
}

TWin* TWin::get_foreground_window()
{
	return new TWin(GetForegroundWindow());
}

void TWin::to_foreground()
{
	SetForegroundWindow(m_hwnd);
}

void TWin::set_focus()
{
	SetFocus((HWND)m_hwnd);
}

void TWin::mouse_capture(bool do_grab)
{
	if (do_grab) SetCapture(m_hwnd);
	else ReleaseCapture();
}

void TWin::close()
{
	send_msg(WM_CLOSE);
}

int TWin::get_id()
//------------
{
	return (int)GetWindowLongPtr(m_hwnd, GWL_ID);
}


void TWin::resize(int x0, int y0, int w, int h)
//--------------------------------------------
{
	MoveWindow(m_hwnd, x0, y0, w, h, TRUE);
}

void TWin::resize(const Rect & rt)
//--------------------------------------------
{
	MoveWindow(m_hwnd, rt.left, rt.top, rt.right - rt.left, rt.bottom - rt.top, TRUE);
}


void TWin::resize(int w, int h)
//------------------------------
{
	SetWindowPos(handle(), NULL, 0, 0, w, h, SWP_NOMOVE);
}

void TWin::move(int x0, int y0)
//------------------------------
{
	SetWindowPos(handle(), NULL, x0, y0, 0, 0, SWP_NOSIZE);
}

void TWin::show(int how)
//--------------------------------
{
	ShowWindow(m_hwnd, how);
}

void TWin::hide()
{
	ShowWindow(m_hwnd, SW_HIDE);
}

bool TWin::visible()
{
	return IsWindowVisible(m_hwnd);
}

void TWin::set_parent(TWin * w)
{
	SetParent(m_hwnd, w ? w->handle() : NULL);
}

void TWin::set_style(DWORD s)
//---------------------------
{
	SetWindowLongPtr(m_hwnd, GWL_STYLE, s);
}

LRESULT TWin::send_msg(UINT msg, WPARAM wparam, LPARAM lparam) const
//---------------------------------------------------
{
	return SendMessage(m_hwnd, msg, wparam, lparam);
}

TWin* TWin::create_child(pchar winclss, pchar text, int id, DWORD styleEx)
//------------------------------------------------------------------------
{
	return new TWin(this, winclss, text, id, styleEx);
}

int TWin::message(pchar msg, int type)
//-------------------------------------
{
	int flags;
	const wchar_t* title;
	if (type == MSG_ERROR) { flags = MB_ICONERROR | MB_OK; title = L"Error"; }
	else if (type == MSG_WARNING) { flags = MB_ICONEXCLAMATION | MB_OKCANCEL; title = L"Warning"; }
	else if (type == MSG_QUERY) { flags = MB_YESNO; title = L"Query"; }
	else { flags = MB_OK; title = L"Message"; }
	int retval = (type == MSG_QUERY) ? IDYES : IDOK;
	return MessageBox(m_hwnd, msg, title, flags) == retval;
}

void TWin::on_top()  // *add 0.9.4
{
	SetWindowPos(handle(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}


//----TEventWindow class member definitions---------------------
TEventWindow::TEventWindow(pchar caption, TWin * parent, DWORD style_extra, int is_child, DWORD style_override)
//-------------------------------------------------------------------------------------------------------
{
	m_children = new ChildList();
	m_client = NULL;
	m_tool_bar = NULL;
	m_style_extra = style_extra;
	set_defaults();
	if (style_override != -1) {
		m_style = style_override;
	}
	create_window(caption, parent, is_child);
	//m_dc->set_text_align(0);
	enable_resize(true);
	cursor(CursorType::ARROW);
}

void TEventWindow::create_window(pchar caption, TWin* parent, bool is_child)
//-------------------------------------------------------------------------
{
	HWND hParent;
	void* CreatParms[2]{};
	m_dc = new TDC;
	CreatParms[0] = (void*)this;
	if (parent) {
		hParent = parent->handle();
		if (is_child) m_style = WS_CHILD;
	}
	else hParent = NULL;
	m_hwnd = CreateWindowEx(m_style_extra, EW_CLASSNAME, caption, m_style,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		hParent, NULL, hInst, CreatParms);
	set_window();
}

void TEventWindow::set_defaults()
//-------------------------------
{
	m_style = WS_OVERLAPPEDWINDOW;
	//m_bkgnd_brush =  GetStockObject (GRAY_BRUSH);
	m_bk_color = GetSysColor(COLOR_BTNFACE);
	m_bkgnd_brush = CreateSolidBrush(m_bk_color);
	m_hmenu = NULL;
	m_hpopup_menu = NULL;
	m_timer = 0;
	m_dispatcher = NULL;
	m_child_messages = false;
}

void TEventWindow::add_handler(AbstractMessageHandler * m_hand)
{
	if (!m_dispatcher) m_dispatcher = m_hand;
	else m_dispatcher->add_handler(m_hand);
}

void TEventWindow::add_accelerator(Handle accel)
{
	hAccel = accel;
}

void TEventWindow::update_data()
{
	get_handler()->write();
}

void TEventWindow::update_controls()
{
	get_handler()->read();
}

bool TEventWindow::cant_resize()
{
	return !m_do_resize;
}

int TEventWindow::metrics(int ntype)
//----------------------------------
// Encapsulates what we need from GetSystemMetrics() and GetTextMetrics()
{
	if (ntype < TM_CAPTION_HEIGHT) { // text metrics
		TEXTMETRIC tm;
		GetTextMetrics(get_dc()->get_hdc(), &tm);
		if (ntype == TM_CHAR_HEIGHT) return tm.tmHeight; else
			if (ntype == TM_CHAR_WIDTH)  return tm.tmMaxCharWidth;
	}
	else {
		switch (ntype) {
		case TM_CAPTION_HEIGHT: return GetSystemMetrics(SM_CYMINIMIZED);
		case TM_MENU_HEIGHT: return GetSystemMetrics(SM_CYMENU);
		case TM_CLIENT_EXTRA:
			if (m_style_extra & WS_EX_PALETTEWINDOW) {
				return GetSystemMetrics(SM_CYSMCAPTION);
			}
			else {
				return metrics(TM_CAPTION_HEIGHT) + ((m_hmenu != NULL) ? metrics(TM_MENU_HEIGHT) : 0);
			}
		case TM_SCREEN_WIDTH:  return GetSystemMetrics(SM_CXMAXIMIZED);
		case TM_SCREEN_HEIGHT: return GetSystemMetrics(SM_CYMAXIMIZED);
		default: return 0;
		}
	}
	return 0;
}

void TEventWindow::client_resize(int cwidth, int cheight)
{
	// *SJD* This allows for menus etc
	int sz = 0;
	if (!(m_style & WS_CHILD))
		sz = metrics(TM_CLIENT_EXTRA);
	resize(cwidth, cheight + sz);
}

void TEventWindow::enable_resize(bool do_resize, int w, int h)
{
	m_do_resize = do_resize;
	m_fixed_size.x = w;
	m_fixed_size.y = h;
}

POINT TEventWindow::fixed_size()
{
	return m_fixed_size;
}

//*1 new cursor types
void TEventWindow::cursor(CursorType curs)
{
	HCURSOR new_cursor = 0;
	if (curs == CursorType::RESTORE) new_cursor = m_old_cursor;
	else {
		m_old_cursor = GetCursor();
		switch (curs) {
		case CursorType::ARROW: new_cursor = LoadCursor(NULL, IDC_ARROW); break;
		case CursorType::HOURGLASS: new_cursor = LoadCursor(NULL, IDC_WAIT); break;
		case CursorType::SIZE_VERT: new_cursor = LoadCursor(NULL, IDC_SIZENS); break;
		case CursorType::SIZE_HORZ: new_cursor = LoadCursor(NULL, IDC_SIZEWE); break;
		case CursorType::CROSS: new_cursor = LoadCursor(NULL, IDC_CROSS); break;
		//case CursorType::HAND: new_cursor = LoadCursor(NULL,IDC_HAND); break;
		case CursorType::UPARROW: new_cursor = LoadCursor(NULL, IDC_UPARROW); break;
		}
	}
	SetCursor(new_cursor);
}

//*6 The toolbar goes into the window list; it isn't autosized
// because it has no alignment.
//+ We add a separate notify handler for the tooltip control
void TEventWindow::set_toolbar(TWin * tb, TNotifyWin * tth)
{
	m_tool_bar = tb;
	add(tth);
}

bool TEventWindow::check_notify(LPARAM lParam, int& ret)
{
	LPNMHDR ph = (LPNMHDR)lParam;
	for (TWin* w : *m_children) {
		if (ph->hwndFrom == w->handle())
			if (TNotifyWin* pnw = dynamic_cast<TNotifyWin*>(w)) {
				ret = pnw->handle_notify(ph);
				return ret;
			}
			else if (TMemo* m = dynamic_cast<TMemo*>(w)) {
				ret = m->handle_notify(ph);
				return ret;
			}
	}
	return false;
}

void TEventWindow::set_window()
//-----------------------------
{
	set(m_hwnd);
}

void TEventWindow::set_background(float r, float g, float b)
//-----------------------------------------
{
	COLORREF rgb = RGBF(r, g, b);
	m_bkgnd_brush = CreateSolidBrush(rgb);
	get_dc()->get(this);
	SetBkColor(get_dc()->get_hdc(), rgb);
	get_dc()->select(m_bkgnd_brush);
	m_bk_color = rgb;
	get_dc()->release(this);
	invalidate();
}

void TEventWindow::set_menu(pchar res)
//------------------------------------
{
	if (m_hmenu) DestroyMenu(m_hmenu);
	set_menu(LoadMenu(hInst, res));
}

void TEventWindow::set_menu(Handle menu)
//------------------------------------
{
	m_hmenu = menu;
	SetMenu(m_hwnd, menu);
}

void TEventWindow::set_popup_menu(Handle menu)
//--------------------------------------------
{
	m_hpopup_menu = menu;
}

void TEventWindow::last_mouse_pos(int& x, int& y)
//-----------------------------------------------
{
	POINT pt{ g_mouse_pt_right.x, g_mouse_pt_right.y };
	//pt.x = g_mouse_pt_right.x;
	//pt.y = g_mouse_pt_right.y;
	ScreenToClient(m_hwnd, &pt);
	x = pt.x;
	y = pt.y;
}

void TEventWindow::check_menu(int id, bool check)
{
	CheckMenuItem(m_hmenu, id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED));
}

void TEventWindow::show(int how)
//-----------------------------------
{
	// default:  use the 'nCmdShow' we were _originally_ passed
	if (how == 0) how = CmdShow;
	ShowWindow(m_hwnd, how);
}

void TEventWindow::create_timer(int msec)
//---------------------------------------
{
	if (m_timer) kill_timer();
	m_timer = SetTimer(m_hwnd, 1, msec, NULL);
}

void TEventWindow::kill_timer()
//-----------------------------
{
	KillTimer(m_hwnd, m_timer);
}

const int WM_QUIT_LOOP = 0x999;

/*
int _TranslateAccelerator(HWND h, HACCEL acc, MSG* msg)
{
	int ret = TranslateAccelerator(h,acc,msg);
	if (ret)
		ret = 1;
	return ret;
}
*/

// Message Loop!
// *NB* this loop shd cover ALL cases, controlled by global
// variables like mdi_sysaccell,  accell, hModelessDlg, etc.
int TEventWindow::run()
//---------------------
{
	BOOL bRet;
	MSG msg;
	while (bRet = GetMessage(&msg, NULL, 0, 0)) {
		if (bRet == -1) {
			// handle the error and possibly exit
		}
		else {
			if (msg.message == WM_QUIT_LOOP) return int(msg.wParam);
			if (!hAccel || !TranslateAccelerator(m_hwnd, hAccel, &msg)) {
				if (!hModeless || !IsDialogMessage(hModeless, &msg)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}
	}
	return int(msg.wParam);
}

void TEventWindow::quit(int retcode)
{
	PostMessage(m_hwnd, WM_QUIT_LOOP, retcode, 0);
}

// Place holder functions - no functionality offered at this level!
TEventWindow::~TEventWindow()
{
	//log_add("~TEventWindow");
	if (m_timer) kill_timer();
	/*if (m_dc)*/ delete m_dc;
	/*if (m_client)*/ delete m_client;
	delete m_children;
	DestroyWindow((HWND)m_hwnd);
}

void TEventWindow::set_icon(pchar file)
{
	HANDLE hIcon = LoadImage(hInst, file, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	SetClassLongPtr(m_hwnd, GCLP_HICON, (LPARAM)hIcon);
}

void TEventWindow::set_icon_from_window(TWin * win)
{
	HICON hIcon = (HICON)GetClassLongPtr((HWND)win->handle(), GCLP_HICON);
	SetClassLongPtr(m_hwnd, GCLP_HICON, /*(long)*/(LPARAM)hIcon);
}

// *change 0.6.0 support for VCL-style alignment of windows
void TEventWindow::size()
{
	if (m_children->size() == 0) return;
	int n = (int)m_children->size();
	Rect m;
	get_client_rect(m);
	if (m_tool_bar) m.top += m_tool_bar->height();  //*6
	// we will only be resizing _visible_ windows with explicit alignments.
	for (const auto& win : *m_children) {
		if (win->align() == Alignment::alNone || !win->visible()) n--;
	}
	if (n == 0) return;
	HDWP hdwp = BeginDeferWindowPos(n);
	for (const auto& win : *m_children) {
		if (!win->visible()) continue; //*new
		Rect wrt;
		//win->get_client_rect(wrt);
		win->get_rect(wrt, true);
		int left = m.left, top = m.top, w = wrt.width(), h = wrt.height();
		int width_m = m.width(), height_m = m.height();
		switch (win->align()) {
		case Alignment::alTop:
			w = width_m;
			m.top += h;
			break;
		case Alignment::alBottom:
			m.bottom -= h;
			top = m.bottom;
			w = width_m;
			break;
		case Alignment::alLeft:
			h = height_m;
			m.left += w;
			break;
		case Alignment::alRight:
			m.right -= w;
			left = m.right;
			h = height_m;
			break;
		case Alignment::alClient:
			h = height_m;
			w = width_m;
			break;
		case Alignment::alNone: continue;  // don't try to resize anything w/ no alignment.
		} // switch(...)
		DeferWindowPos(hdwp, win->handle(), NULL, left, top, w, h, SWP_NOZORDER);
	} // for(...)
	EndDeferWindowPos(hdwp);
}

// *add 0.6.0 can add a control to the child list directly
void TEventWindow::add(TWin * win)
{
	if (m_client)
		m_children->push_front(win);
	else
		m_children->push_back(win);
	win->show();
}

void TEventWindow::remove(TWin * win)
{
	m_children->remove(win);
	win->hide();
	size();
}

// *change 0.6.0 set_client(), focus() has moved up from TFrameWindow
//*5 Note the special way that m_client is managed w.r.t the child list.
void TEventWindow::set_client(TWin * cli, bool do_subclass)
{
	if (m_client) {
		if (do_subclass) {
			// *NOTE* this is very dubious code, not all client windows are controls!
			remove_subclass_control((TControl*)m_client);
		}
		m_client->hide();
		m_client = NULL;
	}
	if (cli) {
		// client window also goes into the child list; there can _only_
		// be one such, and it must be at the end of the list.
		if (m_children->size() > 0 && m_children->back()->align() == Alignment::alClient)
			m_children->pop_back();
		add(cli);
		m_client = cli;
		m_client->align(Alignment::alClient);
		focus();
		if (do_subclass) subclass_control((TControl*)cli);
	} // else m_children->pop_back();
}

void TEventWindow::focus()
{
	if (m_client) m_client->set_focus();
}

bool TEventWindow::command(int, int) { return true; }
bool TEventWindow::sys_command(int) { return false; }
void TEventWindow::paint(TDC&) {}
void TEventWindow::ncpaint(TDC&) {}
void TEventWindow::mouse_down(Point&) {}
void TEventWindow::mouse_up(Point&) { }
void TEventWindow::right_mouse_down(Point&) {}
void TEventWindow::mouse_move(Point&) { }
void TEventWindow::keydown(int) { }
void TEventWindow::destroy() {  }
void TEventWindow::timer() { }
int  TEventWindow::notify(int id, void* ph) { return 0; }
int  TEventWindow::handle_user(WPARAM wparam, LPARAM lparam) { return 0; }
void TEventWindow::scroll(int code, int posn) { };

////// Members of TFrameWindow
TFrameWindow::TFrameWindow(pchar caption, bool has_status, TWin * cli)
	: TEventWindow(caption, NULL)
{
	set_client(cli);
	if (has_status) {
		HWND hStatus = CreateStatusWindow(WS_CHILD | WS_BORDER | WS_VISIBLE,
			L"", m_hwnd, 1);
		m_status = new TWin(hStatus);
		m_status->align(Alignment::alBottom);
		m_status->resize(0, 20);
		add(m_status);
		int SBParts[2]{};
		SBParts[0] = width() / 2;
		SBParts[1] = -1;
		set_status_fields(SBParts, 2);
	}
	else m_status = NULL;
}

TFrameWindow::~TFrameWindow()
{
	//log_add("~TFrameWindow");
	/*if (m_status)*/ delete m_status;
}

void TFrameWindow::set_status_fields(int* parts, int n)
{
	if (m_status) m_status->send_msg(SB_SETPARTS, n, (LPARAM)parts);
}

void TFrameWindow::set_status_text(int id, pchar txt)
{
	if (m_status) m_status->send_msg(SB_SETTEXT, id, (LPARAM)txt);
}

void TFrameWindow::client_resize(int cwidth, int cheight)
{
	if (m_status) cheight += m_status->height();
	TEventWindow::client_resize(cwidth, cheight);
}

void TFrameWindow::destroy()
//....overrides the default w/ application-closing behaviour!!
{
	// send_msg(WM_CLOSE);
	view_child_messages(false);
	//  set_client(NULL);    //???
	PostQuitMessage(0);
}

/// Windows controls - TControl ////////////
TControl::TControl(TWin * parent, pchar classname, pchar text, int id, DWORD style)
	:TWin(parent, classname, text, id, style), m_wnd_proc(NULL), m_data(NULL)
{
	m_colour = RGB(0, 0, 0);
	m_font = NULL;
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LPARAM)this);
	m_parent = (TEventWindow*)parent;
}

TControl::~TControl()
{
	//log_add("~TControl");
	//auto name = UTF8FromString(type_name());
	//log_add(name.data());
};

TControl* TControl::user_data(Handle handle)
{
	return (TControl*)GetWindowLongPtr(handle, GWLP_USERDATA);
}

void TControl::calc_size()
{
	int cx, cy;
	m_parent->get_dc()->get_text_extent(get_text(), cx, cy, m_font);
	resize(int(1.05 * cx), int(1.05 * cy));
}

bool TControl::is_type(pchar tname)
{
	return wcscmp(type_name(), tname) == 0;
}

void TControl::set_font(TFont * fnt)
{
	m_font = fnt;
	calc_size();
	if (m_font)
		send_msg(WM_SETFONT, (WPARAM)m_font->handle(), (LPARAM)TRUE);
}

void TControl::set_colour(float r, float g, float b)
{
	m_colour = RGBF(r, g, b);
	update();
}


TLabel::TLabel(TWin * parent, pchar text, int id)
//--------------------------------------
	: TControl(parent, L"STATIC", text, id, 0x0)
{ }

TEdit::TEdit(TWin * parent, pchar text, int id, DWORD style)
//-----------------------------------------------
	: TControl(parent, L"EDIT", text, id, style | WS_BORDER | ES_AUTOHSCROLL)
{  }

void TEdit::set_selection(int start, int finish)
{
	send_msg(EM_SETSEL, start, finish);
}

//---------------------------------------------
TComboBox::TComboBox(TWin * parent, int id)
	: TControl(parent, WC_COMBOBOX, L"", id, CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP)
{  }

#include "windowsx.h"
void TComboBox::reset()
{
	send_msg(CB_RESETCONTENT);
}

int TComboBox::add_string(pchar str)
{
	return send_msg(CB_ADDSTRING, 0, (LPARAM)str);
}

void TComboBox::ins_string(int id, pchar str)
{
	send_msg(CB_INSERTSTRING, id, (LPARAM)str);
}

void TComboBox::set_data(int idx, int data)
{
	send_msg(CB_SETITEMDATA, idx, (LPARAM)data);
}

int TComboBox::get_data(int idx)
{
	return send_msg(CB_GETITEMDATA, idx);
}

void TComboBox::set_cursel(int id)
{
	send_msg(CB_SETCURSEL, id);
}

int TComboBox::get_cursel()
{
	return send_msg(CB_GETCURSEL);
}

void TComboBox::set_height(int sz)
{
	send_msg(CB_SETITEMHEIGHT, 0, (LPARAM)sz);
}

int TComboBox::count()
{
	return send_msg(CB_GETCOUNT);
}

//-----------------Dialog boxes--------------------------
TDialog::~TDialog()
{
	FreeProcInstance(/*(DLGPROC)*/m_lpfnBox);
}

bool TDialog::was_successful()
{
	if (modeless()) return (bool)handle(); else return m_ret;
}

DLGFN DialogProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

void TDialog::go()
//-----------------
{
	HWND hdlg = 0, hOwner;

	hOwner = (m_owner) ? m_owner->handle() : NULL; //GetDesktopWindow();
	m_lpfnBox = /*(void FAR *)*/MakeProcInstance((DLGPROC)DialogProc, hInst);
	if (modeless()) {
		hdlg = CreateDialogParam(hInst, m_name, hOwner, (DLGPROC)m_lpfnBox,/*(long)*/(LPARAM)this);
		hModeless = hdlg;
	}
	else {
		m_ret = (int)DialogBoxParam(hInst, m_name, hOwner, (DLGPROC)m_lpfnBox,/*(long)*/(LPARAM)this);
		m_hwnd = 0;  // thereafter, this object is not a valid window...
	}
	if (hdlg) ShowWindow(hdlg, SW_SHOW);
}

TWin* TDialog::field(int id)
{
	return new TWin(GetDlgItem(m_hwnd, id));
}

//....Modeless Dialog Box procedure.........................
DLGFN DialogProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int ret;
	TDialog* This = (TDialog*)GetWindowLongPtr(hdlg, DWLP_USER);

	switch (msg)
	{
	case WM_INITDIALOG:
		//..... 'This' pointer passed as param from CreateDialogParam()
		SetWindowLongPtr(hdlg, DWLP_USER, lParam);

		This = (TDialog*)lParam;
		if (This->modeless()) hModeless = hdlg;
		This->set(hdlg);
		return This->init();

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			ret = This->finis();
			if (!ret) return FALSE;  // no, we're not finished yet!
			[[fallthrough]];
		case IDCANCEL:
			if (This->modeless()) SendMessage(hdlg, WM_CLOSE, 0, 0L);
			else EndDialog(hdlg, wParam == IDOK);
			break;
		default:
			This->command(LOWORD(wParam));
			break;
		}
		return TRUE;

	case WM_CLOSE:
		DestroyWindow(hdlg);
		hModeless = 0;
		break;
	}
	return FALSE;  // we did not process the message...
}


WNDFN WndProc(Handle hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


//----------------Window Registration----------------------

static bool been_registered = false;

void RegisterEventWindow(HANDLE hIcon = 0, HANDLE hCurs = 0)
//------------------------------------------------------
{
	WNDCLASS    wndclass{};

	wndclass.style =
		CS_HREDRAW | //перерисовывать окно при изменении вертикальных размеров
		CS_VREDRAW | //перерисовывать всё окно при изменении ширины 
		CS_OWNDC | //у каждого окна уникальный контекст устройства
		DS_LOCALEDIT;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = sizeof(void*);
	wndclass.hInstance = hInst;
	wndclass.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = hCurs ? hCurs : NULL;
	wndclass.hbrBackground = NULL; //GetStockObject(LTGRAY_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = EW_CLASSNAME;

	RegisterClass(&wndclass);

	been_registered = true;
}

void UnregisterEventWindow()
{
	if (been_registered) {
		UnregisterClass(EW_CLASSNAME, hInst);
		been_registered = false;
	}
}

static HMODULE m_hRichEditDll = NULL;
void on_destroy();
void on_attach();
extern "C"  // inhibit C++ name mangling
BOOL APIENTRY DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpvReserved   // reserved
)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		m_hRichEditDll = LoadLibrary(L"riched32.dll");
		hInst = hinstDLL;
		RegisterEventWindow();
		on_attach();
		CmdShow = SW_SHOW;
		break;
	case DLL_PROCESS_DETACH:
		UnregisterEventWindow();  // though it is important only on NT platform...
		FreeLibrary(m_hRichEditDll);
		on_destroy();
		break;
	};
	return TRUE;
}

//--------------Default Window Proc for EventWindow-----------------

WNDFN WndProc(Handle hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
//----------------------------------------------------------------------------
{
	//  static SWin w;
	static BOOL dragging = FALSE;
	static long MouseTime = 0;
	static UINT size_flags = 0;
	LPMINMAXINFO pSizeInfo;
	long ret = 0;

	TEventWindow* This = (TEventWindow*)GetWindowLongPtr(hwnd, 0);

	switch (msg)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCT lpCreat = (LPCREATESTRUCT)lParam;
		PVOID* lpUser = (PVOID*)lpCreat->lpCreateParams;

		//..... 'This' pointer passed as first word of creation parms
		This = (TEventWindow*)lpUser[0];
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)This);
		This->get_dc()->set_twin(This);
	}
	return 0;

	case WM_SIZE:
		This->size();
		return 0;

	case WM_GETMINMAXINFO:
		if (This && This->cant_resize()) {
			pSizeInfo = (LPMINMAXINFO)lParam;
			//pSizeInfo->ptMaxTrackSize = This->fixed_size();
			//pSizeInfo->ptMinTrackSize = This->fixed_size();
			pSizeInfo->ptMaxTrackSize = pSizeInfo->ptMinTrackSize = This->fixed_size();
		}
		return 0;

	case WM_PARENTNOTIFY:
		if (LOWORD(wParam) != WM_RBUTTONDOWN) break;
		g_mouse_pt_right.set(LOWORD(lParam), HIWORD(lParam));
		ClientToScreen(hwnd, (POINT*)&g_mouse_pt_right);
		lParam = 0;
		// pass through.....
		[[fallthrough]];
	case WM_CONTEXTMENU:
		if (This->m_hpopup_menu == NULL) break;
		if (lParam != 0) {
			g_mouse_pt_right.set(LOWORD(lParam), HIWORD(lParam));
		}
		TrackPopupMenu(This->m_hpopup_menu, TPM_LEFTALIGN | TPM_TOPALIGN,
			g_mouse_pt_right.x, g_mouse_pt_right.y, 0, hwnd, NULL);
		return 0;

	case WM_NOTIFY:
	{
		int ret, id = (int)wParam;
		if (!This->check_notify(lParam, ret))
			return This->notify(id, (void*)lParam);
		else return ret;
	}

	case WM_COMMAND:
		if (This->m_dispatcher) {
			if (This->m_dispatcher->dispatch(LOWORD(wParam), HIWORD(wParam), (Handle)lParam))
				return 0;
		}
		if (This->command(LOWORD(wParam), HIWORD(wParam))) return 0;
		else break;

	case WM_USER_PLUS:
		return This->handle_user(wParam, lParam);

	case WM_KEYDOWN:
		This->keydown(LOWORD(wParam));
		return 0;

	case WM_HSCROLL:
	case WM_VSCROLL:
		if (This->m_dispatcher) {
			int id = (int)GetWindowLongPtr((HWND)lParam, GWL_ID);
			This->m_dispatcher->dispatch(id, LOWORD(wParam), (Handle)lParam);
		}
		if (lParam)
		{
			int id = (int)GetWindowLongPtr((HWND)lParam, GWL_ID);
			switch (LOWORD(wParam))
			{
			//case SB_THUMBTRACK: // change position
			case SB_THUMBPOSITION: // stop changing
				This->scroll(id, HIWORD(wParam));
				break;
			case SB_ENDSCROLL: // end of scroll or click on scrollbar, wParam is 0
			{
				int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
				This->scroll(id, pos);
			}
			}
		}
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		TDC& dc = *This->m_dc;
		dc.set_hdc(BeginPaint(hwnd, &ps));
		This->paint(dc);
		dc.set_hdc(NULL);
		EndPaint(hwnd, &ps);
	}
	return 0;

	// Mouse messages....
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	{
		g_mouse_pt.set(LOWORD(lParam), HIWORD(lParam));
		//pt.to_logical(*This);
		switch (msg) {
		case WM_LBUTTONDOWN:
			This->mouse_down(g_mouse_pt);
			break;
		case WM_LBUTTONUP:
			This->mouse_up(g_mouse_pt);
			break;
		case WM_RBUTTONDOWN:
			This->right_mouse_down(g_mouse_pt);
			break;
		}
	}
	return 0;

	case WM_MOUSEMOVE:  // needs different treatment??
	{
		g_mouse_pt.set(LOWORD(lParam), HIWORD(lParam));
		This->mouse_move(g_mouse_pt);
	}
	return 0;

	case WM_ERASEBKGND:
	{
		RECT rt;
		GetClientRect(hwnd, &rt);
		FillRect((HDC)wParam, (LPRECT)&rt, This->m_bkgnd_brush);
	}
	return 0;

	// suspect this causes trouble
 // case WM_CTLCOLORSTATIC:
	//{
	// TControl *ctl = (TControl *)GetWindowLongPtr((HWND)lParam,GWLP_USERDATA);
	// SetBkColor((HDC)wParam, This->m_bk_color);
	// SetTextColor((HDC)wParam, ctl->get_colour());
	//}
	//return (LRESULT)This->m_bkgnd_brush;

	case WM_SETCURSOR:
		if (This) This->cursor(CursorType::ARROW);
		return 0;

	case WM_SETFOCUS:
		if (This) This->focus();
		return 0;

	case WM_ACTIVATE:
		if (This && !This->activate(wParam != WA_INACTIVE)) return 0;
		break;

	case WM_SYSCOMMAND:
		if (This->sys_command(LOWORD(wParam))) return 0; //?
		else break;

	case WM_TIMER:
		This->timer();
		return 0;

	case WM_CLOSE:
		This->on_close();
		if (!This->query_close()) return 0;
		break; // let DefWindowProc handle this...

	case WM_SHOWWINDOW:
		This->on_showhide(!IsWindowVisible(hwnd));
		break; // let DefWindowProc handle this...

	case WM_DESTROY:
		This->destroy();
		//if (This->m_hmenu) DestroyMenu(This->m_hmenu);  // but why here?
		return 0;

	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
