/* gui_ext.cpp
This is a simple set of predefined GUI windows for SciTE,
built using the YAWL library.
Steve Donovan, 2007.
  */
#include <windows.h>
#include <string>
#include <vector>
#include <io.h>
#include <direct.h>
#include <memory>
#include "lua.hpp"
#include "utf.h"
#include "twl.h"
#include "twl_ini.h"
#include "twl_toolbar.h"
#include "twl_dialogs.h"
#include "twl_modal.h"
#include "twl_listview.h"
#include "twl_tab.h"
#include "twl_splitter.h"
#include "twl_treeview.h"

#define output(x) lua_pushstring(L, (x)); OutputMessage(L);
#define lua_pushwstring(L, str) lua_pushstring((L), UTF8FromString(str).c_str())
//void msgbox(const TCHAR* txt) { MessageBox(NULL, txt, L"dbg", 0); };

// vector of wide strings
typedef std::vector<gui_string> vecws;
#define EQ(s1,s2) (wcscmp((s1),(s2))==0)

const char* WINDOW_CLASS = "WINDOW*";
const int BUFFER_SIZE = 2 * 0xFFFF;

static const char* LIB_VERSION = "0.1.2";
static TWin* s_parent = NULL;
static TWin* s_last_parent = NULL;
static HWND hSciTE = NULL, hContent = NULL, hCode;
static WNDPROC old_scite_proc, old_scintilla_proc, old_content_proc;
static TWin* code_window = NULL;
static TWin* extra_window = NULL;
static TWin* content_window = NULL;
static TWin* extra_window_splitter = NULL;
static bool forced_resize = false;
static Rect m, cwb, extra;

class PaletteWindow;

void OutputMessage(lua_State* L);
void dump_stack(lua_State* L)
{
	if (!L) return;
	output("\r\n==== dump start ====");
	int sz = lua_gettop(L);
	lua_getglobal(L, "tostring");
	for (int i = 1; i <= sz; i++)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_pcall(L, 1, 1, 0);
		const char* str = lua_tolstring(L, -1, 0);
		if (str) {
			output(str);
		}
		else {
			output(lua_typename(L, lua_type(L, i)));
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	output("\r\n===== dump end ====");
}

#define DS dump_stack(L);

TWin* get_parent()
{
	return s_parent;
}

TWin* get_last_parent()
{
	return s_last_parent;
}

//TWin* get_desktop_window()
//{
//	static TWin* desk = NULL;
//	if (desk == NULL) {
//		desk = new TWin(GetDesktopWindow());
//	}
//	return desk;
//}

//  print version 
static int do_version(lua_State* L)
{
	lua_getglobal(L, "print");
	lua_pushstring(L, LIB_VERSION);
	lua_pcall(L, 1, 0, 0);
	return 0;
}

// show a message on the SciTE output window
void OutputMessage(lua_State* L)
{
	if (lua_isstring(L, -1)) {
		lua_getglobal(L, "print");
		lua_insert(L, -2);
		lua_pcall(L, 1, 0, 0);
	}
}

namespace
{
	// https://ilovelua.wordpress.com

	int errorHandler(lua_State* L)
	{
		//stack: err
		lua_getglobal(L, "debug"); // stack: err debug
		lua_getfield(L, -1, "traceback"); // stack: err debug debug.traceback

		// debug.traceback() возвращает 1 значение
		if (lua_pcall(L, 0, 1, 0))
		{
			lua_pushstring(L, "Error in debug.traceback() call: ");
			lua_insert(L, -2);
			lua_pushstring(L, "\n");
			lua_concat(L, 3); //"Error in debug.traceback() call: "+err+"\n"
			//OutputMessage(L);
		}
		else
		{
			// stack: err debug stackTrace
			lua_insert(L, -2); // stack: err stackTrace debug
			lua_pop(L, 1); // stack: err stackTrace
			lua_pushstring(L, "Error:"); // stack: err stackTrace "Error:"
			lua_insert(L, -3); // stack: "Error:" err stackTrace  
			lua_pushstring(L, "\n"); // stack: "Error:" err stackTrace "\n"
			lua_insert(L, -2); // stack: "Error:" err "\n" stackTrace
			lua_concat(L, 4); // stack: "Error:"+err+"\n"+stackTrace
		}
		return 1;
	}
#pragma optimize( "", off )
	// маленький помощник, чтобы самим не считать количество lua_push...() и lua_pop()
	class LuaStackGuard
	{
		lua_State* luaState_;
		int top_;
	public:
		explicit LuaStackGuard(lua_State* L) : luaState_(L)
		{
			top_ = lua_gettop(L);
		}

		~LuaStackGuard()
		{
			lua_settop(luaState_, top_);
		}
	};
#pragma optimize( "", on )
}

static int call_named_function(lua_State* L, const char* name, lua_Integer arg)
{
	LuaStackGuard lgs(L);
	lua_getglobal(L, name);
	if (lua_isfunction(L, -1)) {
		lua_pushinteger(L, arg);
		if (lua_pcall(L, 1, 1, 0)) {
			OutputMessage(L);
		}
		else {
			return lua_gettop(L) ? lua_toboolean(L, -1) : 0;
		}
	}
	return 0;
}

static void force_contents_resize()
{
	// get the code pane's extents, and don't try to resize it again!
	code_window->get_rect(m, true);
	if (cwb.right == m.right && cwb.left == m.left) return; // top and left is 0 every times
	int w = extra_window->width();
	int h = m.height();
	int sw = extra_window_splitter->width();
	extra = m;
	cwb = m;
	if (extra_window->align() == Alignment::alLeft) {
		// on the left goes the extra pane, followed by the splitter
		extra.right = extra.left + w;
		extra_window->resize(m.left, m.top, w, h);
		extra_window_splitter->resize(m.left + w, m.top, sw, h);
		cwb.left += w + sw;
	}
	else {
		int margin = m.right - w;
		extra.left = margin;
		extra_window->resize(margin, m.top, w, h);
		extra_window_splitter->resize(margin - sw, m.top, sw, h);
		cwb.right -= w + sw;
	}
	// and then the code pane; note the hack necessary to prevent a nasty recursion here.
	forced_resize = true;
	code_window->resize(cwb);
	forced_resize = false;
}

static LRESULT SciTEWndProc(HWND hwnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	switch (iMessage)
	{
	case WM_ACTIVATEAPP:
		//PaletteWindow::set_visibility(wParam);
		//set_visibility(wParam);
		//call_named_function(sL, "OnActivate", wParam);
		//floating toolbars may grab the focus, so restore it.
		if (wParam) code_window->set_focus();
		break;

	case WM_CLOSE:
		//call_named_function(sL, "OnClosing", 0); //!- disabled
		break;

	case WM_COMMAND:
		//if (call_named_function(sL, "OnCommand", LOWORD(wParam))) return TRUE;
		break;
	}
	return CallWindowProc(old_scite_proc, hwnd, iMessage, wParam, lParam);
}

static LRESULT ScintillaWndProc(HWND hwnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	if (iMessage == WM_SIZE) {
		if (extra_window) {
			if (!forced_resize) {
				force_contents_resize();
			}
		}
		if (extra_window_splitter)// fix hiding splitter when change horz size
			extra_window_splitter->invalidate();
	}
	return CallWindowProc(old_scintilla_proc, hwnd, iMessage, wParam, lParam);
}

static LRESULT ContentWndProc(HWND hwnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	if (iMessage == WM_SETCURSOR) {
		Point ptCursor;
		GetCursorPos(&ptCursor);
		Point ptClient = ptCursor;
		ScreenToClient(hContent, &ptClient);
		if (extra.is_inside(ptClient)) {
			return DefWindowProc(hSciTE, iMessage, wParam, lParam);
		}
	}
	return CallWindowProc(old_content_proc, hwnd, iMessage, wParam, lParam);
}

static void subclass_scite_window()
{
	static bool subclassed = false;
	if (!subclassed) {  // to prevent a recursion
		old_scite_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hSciTE, GWLP_WNDPROC, (LONG_PTR)SciTEWndProc));
		old_content_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hContent, GWLP_WNDPROC, (LONG_PTR)ContentWndProc));
		old_scintilla_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hCode, GWLP_WNDPROC, (LONG_PTR)ScintillaWndProc));
		subclassed = true;
	}
}

//////// dialogs functions
static inline COLORREF convert_colour_spec(const char* clrs)
{
	unsigned int r = 0, g = 0, b = 0;
	sscanf_s(clrs, "#%02x%02x%02x", &r, &g, &b);
	return RGB(r, g, b);
}

/** gui.colour_dlg(default_colour)
	@param default_colour  colour either in form '#RRGGBB" or as a 32-bit integer
	@return chosen colour, in same form as default_colour
*/
int do_colour_dlg(lua_State* L)
{
	bool in_rgb = lua_isstring(L, 1);
	COLORREF cval;
	if (in_rgb) {
		cval = convert_colour_spec(lua_tostring(L, 1));
	}
	else {
		cval = (COLORREF)luaL_optinteger(L, 1, 0);
	}
	TColourDialog dlg(get_parent(), cval);
	if (dlg.go()) {
		cval = dlg.result();
		if (in_rgb) {
			char buff[12];
			sprintf_s(buff, "#%02X%02X%02X", GetRValue(cval), GetGValue(cval), GetBValue(cval));
			lua_pushstring(L, buff);
		}
		else {
			lua_pushinteger(L, cval);
		}
		return 1;
	}
	return 0;
}

/** gui.message(message_string, window_type)
	@param message_string
	@param window_type (0 for plain message, 1 for warning box)
	MSG_ERROR=2,MSG_WARNING=1, MSG_QUERY=3;
*/

int do_message(lua_State* L)
{
	auto msg = StringFromUTF8(luaL_checkstring(L, 1));
	auto kind = StringFromUTF8(luaL_optstring(L, 2, "message"));
	int type = 0;
	//if (kind == L"message") type = 0; else
	if (kind == L"warning") type = 1; else
		if (kind == L"error") type = 2; else
			if (kind == L"query") type = 3;
	lua_pushboolean(L, get_parent()->message(msg.c_str(), type));
	return 1;
}

/* gui.open_dlg([sCaption = "Open File"][, sFilter = "All (*.*)|*.*"])
* @param sCaption [= "Open File"]
* @param sFilter  [= "All (*.*)|*.*"]
* @return sFileName or nil
*/
int do_open_dlg(lua_State* L)
{
	auto caption = StringFromUTF8(luaL_optstring(L, 1, "Open File"));
	auto filter = StringFromUTF8(luaL_optstring(L, 2, "All (*.*)|*.*"));
	TOpenFile tof(get_parent(), caption.c_str(), filter.c_str());
	if (tof.go()) {
		lua_pushwstring(L, tof.file_name());
	}
	else {
		lua_pushnil(L);
	}
	return 1;
}

/* gui.open_dlg([sCaption = "Save File"][, sFilter = "All (*.*)|*.*"])
* @param sCaption [= "Save File"]
* @param sFilter  [= "All (*.*)|*.*"]
* @return sFileName or nil
*/
int do_save_dlg(lua_State* L)
{
	auto caption = StringFromUTF8(luaL_optstring(L, 1, "Save File"));
	auto filter = StringFromUTF8(luaL_optstring(L, 2, "All (*.*)|*.*"));
	TSaveFile tof(get_parent(), caption.c_str(), filter.c_str());
	if (tof.go())
		lua_pushwstring(L, tof.file_name());
	else
		lua_pushnil(L);
	return 1;
}

// gui.select_dir_dlg([sDescription = ""][, sInitialdir = ""])
// @param sCaption [= ""]
// @param sFilter  [= ""]
// @return sFileName or nil
int do_select_dir_dlg(lua_State* L)
{
	auto descr = StringFromUTF8(luaL_optstring(L, 1, ""));
	auto initdir = StringFromUTF8(luaL_optstring(L, 2, ""));
	TSelectDir dir(get_parent(), descr.c_str(), initdir.c_str());
	if (dir.go())
		lua_pushwstring(L, dir.path());
	else
		lua_pushnil(L);
	return 1;
}

class PromptDlg : public TModalDlg {
public:
	gui_string m_val;
	pchar m_field_name;

	PromptDlg(TWin* parent, pchar field_name, pchar val)
		: TModalDlg(parent, L"Enter:"), m_val(val), m_field_name(field_name)
	{}

	void layout(Layout& lo)
	{
		lo << Field(m_field_name, m_val.data());
	}
};

/* gui.prompt_value( sPrompt_string [, default_value = ""])
* @param sPrompt_string
* @param sDefaultValue [=""]
*/
int do_prompt_value(lua_State* L)
{
	auto varname = StringFromUTF8(luaL_checkstring(L, 1));
	auto value = StringFromUTF8(luaL_optstring(L, 2, ""));
	PromptDlg dlg(get_parent(), varname.c_str(), value.c_str());
	if (dlg.show_modal()) {
		lua_pushwstring(L, dlg.m_val);
	}
	else {
		lua_pushnil(L);
	}
	return 1;
}
/// end dialos functions

/// others functions
static int append_file(lua_State* L, int idx, int attrib, bool look_for_dir, pchar value)
{
	if (((attrib & _A_SUBDIR) != 0) == look_for_dir) {
		if (look_for_dir && (EQ(value, L".") || EQ(value, L".."))) return idx;
		lua_pushinteger(L, idx);
		lua_pushwstring(L, value);
		lua_settable(L, -3);
		return idx + 1;
	}
	return idx;
}

static inline bool optboolean(lua_State* L, int idx, bool res = false)
{
	return lua_isnoneornil(L, idx) ? res : lua_toboolean(L, idx);
}

static int do_files(lua_State* L)
{
	struct _wfinddata_t c_file;
	gui_string mask = StringFromUTF8(luaL_checkstring(L, 1));
	bool look_for_dir = optboolean(L, 2);
	intptr_t hFile = _wfindfirst(mask.c_str(), &c_file);
	lua_newtable(L);
	if (hFile == -1) { return 1; }
	int i = 1;
	i = append_file(L, i, c_file.attrib, look_for_dir, c_file.name);
	while (_wfindnext(hFile, &c_file) == 0) {
		i = append_file(L, i, c_file.attrib, look_for_dir, c_file.name);
	}
	return 1;
}

static int do_chdir(lua_State* L)
{
	const char* dirname = luaL_checkstring(L, 1);
	int res = _chdir(dirname);
	lua_pushboolean(L, res == 0);
	return 1;
}

int do_run(lua_State* L)
{
	auto wsFile = StringFromUTF8(luaL_checkstring(L, 1));
	auto wsParameters = StringFromUTF8(lua_tostring(L, 2));
	auto wsDirectory = StringFromUTF8(lua_tostring(L, 3));
	lua_Integer res = (lua_Integer)ShellExecute(
		NULL,
		L"open",
		wsFile.c_str(),
		wsParameters.c_str(),
		wsDirectory.c_str(),
		SW_SHOWDEFAULT
	);
	if (res <= HINSTANCE_ERROR) {
		lua_pushboolean(L, 0);
		lua_pushinteger(L, res);
		return 2;
	}
	else {
		lua_pushinteger(L, res);
		return 1;
	}
}

std::string getAscii(unsigned char value);
std::string getHtmlName(unsigned char value);
int getHtmlNumber(unsigned char value);

/*
* @param uChar
* @return symbol, html number, html code
*/
int do_get_ascii(lua_State* L)
{
	unsigned char x = static_cast<unsigned char>(luaL_checkinteger(L, 1));
	std::string s = getAscii(x);
	lua_pushstring(L, s.c_str());

	char htmlNumber[8]{};
	if ((x >= 32 && x <= 126) || (x >= 160 /*&& x <= 255*/))
	{
		sprintf_s(htmlNumber, "&#%d", x);
	}
	else
	{
		int n = getHtmlNumber(x);
		if (n > -1)
		{
			sprintf_s(htmlNumber, "&#%d", n);
		}
		else
		{
			sprintf_s(htmlNumber, "");
		}
	}
	lua_pushstring(L, htmlNumber);

	std::string htmlName = getHtmlName(x);
	lua_pushstring(L, htmlName.c_str());
	return 3;
}
/// end others functions

/// windows functions

////// This acts as the top-level frame window containing these controls; it supports
////// adding extra buttons and actions.

template<typename T>
void lua_pushargs(lua_State* L, T value)
{
	lua_pushinteger(L, (lua_Integer)value);
}
void lua_pushargs(lua_State* L, const char* value)
{
	lua_pushstring(L, value);
}
void lua_pushargs(lua_State* L, bool value)
{
	lua_pushboolean(L, value);
}
void lua_pushargs(lua_State* L, void* value)
{
	lua_pushlightuserdata(L, value);
}
template<typename T, typename... Args>
void lua_pushargs(lua_State* L, T value, Args... args)
{
	lua_pushargs(L, value);
	if (sizeof...(args)) lua_pushargs(L, args...);
}

template<typename... Args>
int dispatch_ref(lua_State* L, int idx, Args... args)
{
	if (idx != 0) {
		LuaStackGuard sg(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
		lua_pushargs(L, args...);// stack: func arg1 arg2 ... argn
		lua_pushcfunction(L, errorHandler);// stack: func arg1 arg2 ... argn errorHandler
		int args_count = sizeof...(args);
		const int errorHandlerIndex = -(args_count + 2);
		lua_insert(L, errorHandlerIndex); //stack: errorHandler func arg1 arg2 ... argn
		if (lua_pcall(L, args_count, 1, errorHandlerIndex))
			OutputMessage(L);
		else
			return lua_toboolean(L, -1);
	}
	return 0;
}

void function_ref(lua_State* L, int idx, int* pr)
{
	if (*pr) luaL_unref(L, LUA_REGISTRYINDEX, *pr);
	lua_pushvalue(L, idx);
	*pr = luaL_ref(L, LUA_REGISTRYINDEX);
}

class LuaWindow : public TEventWindow
{
protected:
	lua_State* L;
	int onclose_idx;
	int onshow_idx;
	int on_command_idx;
	int on_scroll_idx;
public:
	LuaWindow(pchar caption, lua_State* l, TWin* parent, DWORD stylex = 0, bool is_child = false, DWORD style = -1)
		:TEventWindow(caption, parent, stylex, is_child, style), L(l),
		onclose_idx(0), onshow_idx(0), on_command_idx(0), on_scroll_idx(0)
	{}
	//virtual ~LuaWindow()
	//{}

	void set_on_command(int iarg)
	{
		function_ref(L, iarg, &on_command_idx);
	}

	bool command(int arg1, int arg2)
	{
		return dispatch_ref(L, on_command_idx, arg1, arg2);
	}

	void set_on_scroll(int iarg)
	{
		function_ref(L, iarg, &on_scroll_idx);
	}

	void scroll(int code, int posn)
	{
		dispatch_ref(L, on_scroll_idx, code, posn);
	}

	void handler(Item* item)
	{
		LuaStackGuard sg(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, (lua_Integer)item->data);
		std::string str = lua_tostring(L, -1);
		lua_getglobal(L, str.c_str());
		if (str._Starts_with("IDM_")) {
			if (lua_isinteger(L, -1)) {
				lua_getglobal(L, "scite");
				lua_getfield(L, -1, "MenuCommand"); // val, scite, scite.MenuCommand
				lua_pushvalue(L, -3);// val, scite, scite.MenuCommand, val
				if (lua_pcall(L, 1, 0, 0)) OutputMessage(L); // call scite.MenuCommand( cmdID )
			}
			else {
				lua_pushfstring(L, "unknown command '%s'", str.c_str());
				OutputMessage(L);
			}
		}
		else {
			if (lua_isfunction(L, -1)) {
				if (lua_pcall(L, 0, 0, 0)) OutputMessage(L);
			}
			else {
				lua_pushfstring(L, "can't find function '%s'", str.c_str());
				OutputMessage(L);
			}
		}
	}

	void set_on_close(int iarg)
	{
		function_ref(L, iarg, &onclose_idx);
	}

	void on_close()
	{
		dispatch_ref(L, onclose_idx, 0);
	}

	void set_on_show(int iarg)
	{
		function_ref(L, iarg, &onshow_idx);
	}

	void on_showhide(bool show)
	{
		dispatch_ref(L, onshow_idx, show);
	}
};

class PaletteWindow : public LuaWindow
{
protected:
	bool m_shown;
public:
	PaletteWindow(pchar caption, lua_State* l, DWORD stylex = 0, DWORD style = -1)
		: LuaWindow(caption, l, NULL, stylex, false, style), m_shown(false)
	{}
	void show(int how = SW_SHOW)
	{
		TEventWindow::show(how);
		m_shown = true;
	}

	void hide()
	{
		TEventWindow::hide();
		m_shown = false;
	}

	virtual bool query_close()
	{
		hide();
		return false;
	}

	void really_show() { TEventWindow::show(); }

	void really_hide() { TEventWindow::hide(); }

	bool is_shown() { return m_shown; }
};

struct WinWrap {
	TWin* window;
	void* data;
};

static int wrap_window(lua_State* L, TWin* win)
{
	WinWrap* wrp = (WinWrap*)lua_newuserdata(L, sizeof(WinWrap));
	wrp->window = win;
	wrp->data = NULL;
	luaL_getmetatable(L, WINDOW_CLASS);
	lua_setmetatable(L, -2);
	return 1;
}

static void throw_error(lua_State* L, pchar msg)
{
	lua_pushwstring(L, msg);
	lua_error(L);
}

static TWin* window_arg(lua_State* L, int idx = 1)
{
	WinWrap* wrp = (WinWrap*)lua_touserdata(L, idx);
	if (!wrp) throw_error(L, L"not a window");
	return (PaletteWindow*)wrp->window;
}

static void*& window_data(lua_State* L, int idx = 1)
{
	WinWrap* wrp = (WinWrap*)lua_touserdata(L, idx);
	if (!wrp) throw_error(L, L"not a window");
	return wrp->data;
}

class ContainerWindow : public PaletteWindow
{
public:
	ContainerWindow(pchar caption, lua_State* l)
		: PaletteWindow(caption, l)
	{
		set_icon_from_window(get_parent());
	}
	//virtual ~ContainerWindow() { log_add("~ContainerWindow");  };

	void dispatch(lua_Integer data, int ival)
	{
		if (data) {
			LuaStackGuard lsg(L);
			lua_rawgeti(L, LUA_REGISTRYINDEX, data);
			lua_pushinteger(L, ival);
			if (lua_pcall(L, 1, 0, 0)) {
				OutputMessage(L);
			}
		}
	}

	void on_button(Item* item)
	{
		dispatch((lua_Integer)item->data, item->id);  //listv->selected_id()
	}

	void add_buttons(lua_State* l)
	{
		int nargs = lua_gettop(l);
		int i = 2;
		TEW* panel = new TEW(NULL, this, 0, true);
		panel->align(Alignment::alBottom, 50);
		Layout layout(panel, this);
		while (i < nargs) {
#if LUA_VERSION_NUM < 502
			lua_getglobal(l, luaL_checkstring(l, i + 1));
			int itm_type = lua_type(l, -1);
#else
			int itm_type = lua_getglobal(l, luaL_checkstring(l, i + 1));
#endif
			if (itm_type == LUA_TFUNCTION)
			{
				int data = luaL_ref(l, LUA_REGISTRYINDEX);
				layout << Button(StringFromUTF8(luaL_checkstring(l, i)).c_str(), (EH)&ContainerWindow::on_button, (void*)data);
			}
			else {
				lua_pop(l, 1);
				output("not a function:");
				output(lua_tostring(l, i + 1));
			}
			i += 2;
		}
		layout.release();
		add(panel);
		size();
	}
};

// show window w
// w:show()
int window_show(lua_State* L)
{
	window_arg(L)->show();
	return 0;
}

// hide window w
// w:hide()
int window_hide(lua_State* L)
{
	window_arg(L)->hide();
	return 0;
}

/** set window size
	w:size(width, height)
	@param window
	@param width [=250]
	@param height [=300]
*/
int window_resize(lua_State* L)
{
	int w = (int)luaL_optinteger(L, 2, 250);
	int h = (int)luaL_optinteger(L, 3, 200);
	window_arg(L)->resize(w, h);
	return 0;
}

/** set window position
	w:position()
	@param self
	@param pos_x
	@param pos_y
*/
int window_position(lua_State* L)
{
	if (lua_gettop(L) > 1)
	{
		int x = (int)luaL_optinteger(L, 2, 10);
		int y = (int)luaL_optinteger(L, 3, 10);
		window_arg(L)->move(x, y);
		return 0;
	}
	else
	{
		TWin* win = (TWin*)window_arg(L);
		Rect rt;
		win->get_rect(rt, true);

		lua_pushinteger(L, rt.left);
		lua_pushinteger(L, rt.top);
		return 2;
	}
}

/*
* wnd:bounds()
* @return bVisible, left, top, width, height
*/
int window_get_bounds(lua_State* L)
{
	TWin* win = (TWin*)window_arg(L);
	Rect rt;
	win->get_rect(rt);
	lua_pushboolean(L, win->visible());
	lua_pushinteger(L, rt.left);
	lua_pushinteger(L, rt.top);
	lua_pushinteger(L, rt.width());
	lua_pushinteger(L, rt.height());
	return 5;
}

/** create new window
	w = gui.window( sCaption )
	@param strCaption
	@return window
*/
static ContainerWindow* pCWin=nullptr;
int new_window(lua_State* L)
{
	auto caption = StringFromUTF8(luaL_checkstring(L, 1));
	ContainerWindow* cw = new ContainerWindow(caption.c_str(), L);
	s_last_parent = cw;
	pCWin = cw;
	return wrap_window(L, cw);
}

class PanelWindow : public LuaWindow
{
public:
	explicit PanelWindow(lua_State* l) : LuaWindow(L"", l, get_parent(), 0, true) {};
//	virtual ~PanelWindow() { log_add("~PanelWindow"); };
};

/** create new panel
	gui.panel( iWidth )
	@param iWidth [=100]
	@return window
*/
int new_panel(lua_State* L)
{
	PanelWindow* pw = new PanelWindow(L);
	pw->align(Alignment::alLeft, (int)luaL_optinteger(L, 1, 100));
	s_last_parent = pw;
	return wrap_window(L, pw);
}

class LuaControl
{
protected:
	lua_State* L;
	int select_idx;
	int double_idx;
	int onkey_idx;
	int rclick_idx;
	int onclose_idx;
	int onfocus_idx;
	Handle m_hpopup_menu;

public:
	explicit LuaControl(lua_State* l)
		: L(l), select_idx(0), double_idx(0), onkey_idx(0), rclick_idx(0)
		, m_hpopup_menu(0), onclose_idx(0), onfocus_idx(0)
	{}

	void set_popup_menu(Handle menu)
	{
		m_hpopup_menu = menu;
	}

	void set_select(int iarg)
	{
		function_ref(L, iarg, &select_idx);
	}

	void set_double_click(int iarg)
	{
		function_ref(L, iarg, &double_idx);
	}

	void set_onkey(int iarg)
	{
		function_ref(L, iarg, &onkey_idx);
	}

	void set_rclick(int iarg)
	{
		function_ref(L, iarg, &rclick_idx);
	}

	void set_on_close(int iarg)
	{
		function_ref(L, iarg, &onclose_idx);
	}

	void set_on_focus(int iarg)
	{
		function_ref(L, iarg, &onfocus_idx);
	}
};

class TTabControlLua : public TTabControl, public LuaControl
{
public:
	TTabControlLua(TEventWindow* parent, lua_State* l, bool multiline = false)
		: TTabControl(parent, multiline), LuaControl(l)
	{}
	// implement
	void handle_select(int id) override
	{
		TWin* page = (TWin*)get_data(id);
		form->set_client(page);
		form->size();
		dispatch_ref(L, select_idx, id);
	}

	int handle_rclick(int id) override
	{
		if (m_hpopup_menu) {
			POINT p;
			GetCursorPos(&p);

			HWND hwnd = (HWND)(get_parent_win()->handle());
			TrackPopupMenu((HMENU)m_hpopup_menu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hwnd, NULL);
			return 1;
		}
		return 0;
	}
};

/* gui.tabbar(window)
* @return tabbar window
*/
int new_tabbar(lua_State* L)
{
	TEventWindow* form = (TEventWindow*)window_arg(L, 1);
	TTabControlLua* tab = new TTabControlLua(form, L, lua_toboolean(L, 2));
	tab->align(Alignment::alTop);
	form->add(tab);
	return wrap_window(L, tab);
}

/* tabbar:add_tab(sCaption, wndPanel)
* @param sCaption
* @param panel
*/
int tabbar_add(lua_State* L)
{
	if (TTabControl* tab = (TTabControl*)window_arg(L, 1))
		tab->add(StringFromUTF8(luaL_checkstring(L, 2)).c_str(), window_arg(L, 3), luaL_optinteger(L, 4, -1));
	return 0;
}

/* idx = tabbar:select_tab()
/* tabbar:select_tab(idx)
* @param sCaption
* @param panel
*/
int tabbar_sel(lua_State* L)
{
	if (TTabControl* tab = (TTabControl*)window_arg(L, 1))
	{
		if (lua_gettop(L) == 1)
		{
			lua_pushinteger(L, tab->selected());
			return 1;
		}
		tab->selected(luaL_checkinteger(L, 2));
	}
	return 0;
}

int window_remove(lua_State* L)
{
	TEventWindow* form = (TEventWindow*)window_arg(L, 1);
	form->remove(window_arg(L, 2));
	TWin* split = (TWin*)window_data(L, 2);
	if (split) {
		form->remove(split);
	}
	return 0;
}
/*
	@wnd_parent:add( child_window[, sAligment = "client"][, iSize = 100][, bSplitted = true])
	@param wnd
	@param sAligment [= "client"]
	@param iSize [= 100]
	@param bSplitted [= true]
*/
int window_add(lua_State* L)
{
	TEventWindow* cw = (TEventWindow*)window_arg(L, 1);
	TWin* child = window_arg(L, 2);
	std::string align = luaL_optstring(L, 3, "client");
	int sz = (int)luaL_optnumber(L, 4, 100);
	bool splitter = optboolean(L, 5, true);
	child->set_parent(cw);
	if (align == "top") {
		child->align(Alignment::alTop, sz);
	}
	else if (align == "bottom") {
		child->align(Alignment::alBottom, sz);
	}
	else if (align == "left") {
		child->align(Alignment::alLeft, sz);
	}
	else if (align == "none") {
		child->align(Alignment::alNone);
	}
	else if (align == "right") {
		child->align(Alignment::alRight, sz);
	}
	else {
		child->align(Alignment::alClient, sz);
	}
	cw->add(child);
	if (splitter && child->align() != Alignment::alClient) {
		TSplitter* split = new TSplitter(cw, child);
		cw->add(split);
		window_data(L, 2) = split;
	}
	return 0;
}

class TButtonLua : public TButton, public LuaControl
{
public:
	TButtonLua(TWin* parent, lua_State* l, pchar caption, int id)
		:TButton(parent, caption, id), LuaControl(l)
	{}
};

int new_button(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		auto caption = luaL_checkstring(L, 1);
		int cmd_id = luaL_optinteger(L, 2, 1);
		return wrap_window(L, new TButtonLua(p, L, StringFromUTF8(caption).c_str(), cmd_id));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.memo'");
		return 0;
	}
}

class TCheckBoxLua : public TCheckBox, public LuaControl
{
public:
	TCheckBoxLua(TWin* parent, pchar caption, lua_State* l, int id, bool is_auto)
		:TCheckBox(parent, caption, id, is_auto), LuaControl(l)
	{}
};

int new_checkbox(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		auto caption = luaL_checkstring(L, 1);
		int id_cmd = luaL_optinteger(L, 2, 0);
		bool is_auto = optboolean(L, 3);
		return wrap_window(L, new TCheckBoxLua(p, StringFromUTF8(caption).c_str(), L, id_cmd, is_auto));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.checkbox'");
		return 0;
	}
}

class TRadioButtonLua : public TRadioButton, public LuaControl
{
public:
	TRadioButtonLua(TWin* parent, pchar caption, lua_State* l, int id, bool is_auto)
		:TRadioButton(parent, caption, id, is_auto), LuaControl(l)
	{
		calc_size();
	}
};

int new_radiobutton(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		auto caption = luaL_checkstring(L, 1);
		int id_cmd = luaL_optinteger(L, 2, 0);
		bool is_auto = optboolean(L, 3);
		return wrap_window(L, new TRadioButtonLua(p, StringFromUTF8(caption).c_str(), L, id_cmd, is_auto));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.radiobutton'");
		return 0;
	}
}

// button, checkbox
int do_check(lua_State* L)
{
	TButton* btn = (TButton*)window_arg(L, 1);
	if (!btn) return 0;
	int args = lua_gettop(L);
	if (args > 1) {
		btn->check(!!lua_toboolean(L, 2));
		return 0;
	}
	lua_pushboolean(L, btn->check() ? 1 : 0);
	return 1;
}

int do_state(lua_State* L)
{
	TButton* btn = (TButton*)window_arg(L, 1);
	if (!btn) return 0;
	int args = lua_gettop(L);
	if (args > 1) {
		btn->state(!!lua_toboolean(L, 2));
		return 0;
	}
	lua_pushboolean(L, btn->state() ? 1 : 0);
	return 1;
}

class TLabelLua : public TLabel, public LuaControl
{
public:
	TLabelLua(TWin* parent, pchar caption, lua_State* l, int id)
		:TLabel(parent, caption, id), LuaControl(l)
	{
		calc_size();
	}
};

int new_label(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		auto caption = luaL_checkstring(L, 1);
		int id_cmd = luaL_optinteger(L, 2, -1);
		return wrap_window(L, new TLabelLua(p, StringFromUTF8(caption).c_str(), L, id_cmd));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.label'");
		return 0;
	}
}

class TTrackBarLua : public TTrackBar, public LuaControl
{
public:
	TTrackBarLua(TWin* parent, lua_State* l, DWORD style, int id) :
		TTrackBar(parent, style, id), LuaControl(l)
	{}
};

int new_trackbar(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		DWORD style = luaL_optinteger(L, 1, 0);
		int id_cmd = luaL_optinteger(L, 2, 0);
		return wrap_window(L, new TTrackBarLua(p, L, style, id_cmd));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.trackbar'");
		return 0;
	}
}

int trbar_get_pos(lua_State* L)
{
	if (TTrackBarLua* trb = dynamic_cast<TTrackBarLua*>(window_arg(L, 1)))
	{
		lua_pushinteger(L, trb->pos());
		return 1;
	}
	return 0;
}

int trbar_set_pos(lua_State* L)
{
	if (TTrackBarLua* trb = dynamic_cast<TTrackBarLua*>(window_arg(L, 1)))
		trb->pos(luaL_optinteger(L, 2, 0));
	return 0;
}

int trbar_sel_clear(lua_State* L)
{
	if (TTrackBarLua* trb = dynamic_cast<TTrackBarLua*>(window_arg(L, 1)))
		trb->sel_clear();
	return 0;
}

int trbar_set_range(lua_State* L)
{
	if (TTrackBarLua* trb = dynamic_cast<TTrackBarLua*>(window_arg(L, 1)))
	{
		int imin = luaL_optinteger(L, 2, 0);
		int imax = luaL_optinteger(L, 3, 100);
		trb->range(imin, imax);
	}
	return 0;
}

class TMemoLua : public TMemo, public LuaControl
{
public:
	TMemoLua(TWin* parent, lua_State* l, int id, bool do_scroll = false, bool plain = false)
		:TMemo(parent, id, do_scroll, plain), LuaControl(l)
	{}

	int handle_onkey(int id) override
	{
		return dispatch_ref(L, onkey_idx, id);
	}

	int handle_rclick() override
	{
		if (m_hpopup_menu) {
			POINT p;
			GetCursorPos(&p);

			HWND hwnd = (HWND)(get_parent_win()->handle());
			TrackPopupMenu((HMENU)m_hpopup_menu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hwnd, NULL);
			return 1;
		}
		return 0;
	}
};

int new_memo(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		TMemoLua* m = new TMemoLua(p, L, 1);
		return wrap_window(L, m);
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.memo'");
		return 0;
	}
}

int window_set_text(lua_State* L)
{
	if (TWin* win = (TWin*)window_arg(L, 1))
		win->set_text(StringFromUTF8(luaL_checkstring(L, 2)).c_str());
	return 0;
}

int window_get_text(lua_State* L)
{
	if (TWin* win = window_arg(L, 1))
	{
		wchar_t str[MAX_PATH];
		win->get_text(str, MAX_PATH);
		lua_pushwstring(L, str);
	}
	return 1;
}

int memo_set_colour(lua_State* L)
{
	if (TMemoLua* m = (TMemoLua*)window_arg(L, 1))
	{
		m->set_text_colour(convert_colour_spec(luaL_checkstring(L, 2))); // Must be only ASCII
		m->set_background_colour(convert_colour_spec(luaL_checkstring(L, 3)));
	}
	return 0;
}

static void shake_scite_descendants()
{
	Rect frt;
	get_parent()->get_rect(frt, false);
	get_parent()->send_msg(WM_SIZE, SIZE_RESTORED, (LPARAM)MAKELONG(frt.width(), frt.height()));
}

class SideSplitter : public TSplitterB
{
public:
	SideSplitter(TEventWindow* form, TWin* control)
		: TSplitterB(form, control, 5)
	{}

	void paint(TDC& dc)
	{
		Rect rt(this);
		dc.rectangle(rt);
	}

	void on_resize(const Rect& rt)
	{
		TSplitterB::on_resize(rt);
		shake_scite_descendants();
	}
};

// gui.set_panel() - hide sidebar panel
// gui.set_panel(parent_window, sAlignment) - show sidebar and attach to parent_window with sAlignment
static int do_set_panel(lua_State* L)
{
	if (content_window == NULL) {
		lua_pushstring(L, "Window subclassing was not successful");
		lua_error(L);
	}
	if (!lua_isuserdata(L, 1) && extra_window != NULL) {
		extra_window->hide();
		extra_window = NULL;
		extra_window_splitter->close();
		delete extra_window_splitter;
		extra_window_splitter = NULL;
		shake_scite_descendants();
	}
	else {
		extra_window = window_arg(L);
		auto align = StringFromUTF8(luaL_optstring(L, 2, "left"));
		if (align == L"left")
			extra_window->align(Alignment::alLeft);
		else
			extra_window->align(Alignment::alRight);

		extra_window->set_parent(content_window);
		extra_window->show();
		extra_window_splitter = new SideSplitter((TEventWindow*)content_window, extra_window);
		extra_window_splitter->show();
		force_contents_resize();
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
/* do_day_of_year */
#include <algorithm>
#include <ctime>

int     days
(
	int     day,
	int     month,
	int     year
)
{
	int     a = (14 - month) / 12;
	int     y = year - a;
	int     m = month + 12 * a - 3;

	return      day
		+ (153 * m + 2) / 5
		+ 365 * y
		+ y / 4
		- y / 100
		+ y / 400;
}

int do_day_of_year(lua_State* L)
{
	if (lua_gettop(L) == 3)
	{
		int d = std::clamp((int)luaL_checkinteger(L, 1), 1, 31);
		int m = std::clamp((int)luaL_checkinteger(L, 2), 1, 12);
		int y = luaL_checkinteger(L, 3);
		lua_pushinteger(L, (lua_Integer)days(d, m, y) - days(0, 1, y));
	}
	else {
		time_t seconds = time(NULL);
		tm timeinfo{};
		localtime_s(&timeinfo, &seconds);
		lua_pushinteger(L, (lua_Integer)timeinfo.tm_yday + 1);
	}
	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////
int do_test_function(lua_State* L)
{
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// 
//  this will allow us to hand keyboard focus over to editor
//  gui.pass_focus() переключаем фокус на редактор
static int do_pass_focus(lua_State* L)
{
	lua_getglobal(L, "editor");
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, "Focus");
	lua_pop(L, 1);
	if (code_window) {
		code_window->set_focus();
	}
	return 0;
}

// parent_wnd:client(child_wnd)
int window_client(lua_State* L)
{
	TEventWindow* cw = (TEventWindow*)window_arg(L, 1);
	TWin* child = window_arg(L, 2);
	if (!child) throw_error(L, L"must provide a child window");
	child->set_parent(cw);
	cw->set_client(child);
	return 0;
}

class TListViewLua : public TListViewB, public LuaControl
{
public:
	TListViewLua(TWin* parent, lua_State* l, bool multiple_columns = false, bool single_select = true)
		: TListViewB(parent, false, multiple_columns, single_select),
		LuaControl(l)
	{
		if (!multiple_columns) {
			add_column(L"*", 200);
		}
	}

	void clear()
	{
		int items_count = count();
		for (int i = 0; i < items_count; i++)
			if (lua_Integer ud_id = (lua_Integer)get_item_data(i))
				luaL_unref(L, LUA_REGISTRYINDEX, ud_id);
		TListViewB::clear();
	}

	// implement
	void handle_select(intptr_t id) override
	{
		dispatch_ref(L, select_idx, id);
	}

	void handle_double_click(int row, int col, const char* s) override
	{
		dispatch_ref(L, double_idx, row, col, s);
	}

	void handle_onkey(int id) override
	{
		dispatch_ref(L, onkey_idx, id);
	}

	int handle_rclick(int id) override
	{
		if (m_hpopup_menu) {
			POINT p;
			GetCursorPos(&p);

			HWND hwnd = (HWND)(get_parent_win()->handle());
			TrackPopupMenu((HMENU)m_hpopup_menu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hwnd, NULL);
			return 1;
		}
		return 0;
	}

	void handle_onfocus(bool yes) override
	{
		dispatch_ref(L, onfocus_idx, yes);
	}
};

class TTreeViewLua : public TTreeView, public LuaControl
{
public:
	TTreeViewLua(TWin* parent, lua_State* l, bool has_lines = true)
		:TTreeView(parent, has_lines), LuaControl(l)
	{}

	// implement
	void handle_select(void* itm) override
	{
		dispatch_ref(L, select_idx, itm);
	}

	void handle_dbclick(void* itm) override
	{
		dispatch_ref(L, double_idx, itm);
	}

	int handle_rclick(void*) override
	{
		if (m_hpopup_menu) {
			POINT p;
			GetCursorPos(&p);
			HWND hwnd = (HWND)(get_parent_win()->handle());
			TrackPopupMenu((HMENU)m_hpopup_menu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hwnd, NULL);
			return 1;
		}
		return 0;
	}

private:
	void clean_data(int idx)
	{
		log_add("unref tree item data");
		luaL_unref(L, LUA_REGISTRYINDEX, idx);
	}
};

// gui.tree([has_lines = true] [, editable = false])
// create new tree
int new_tree(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		bool has_lines = optboolean(L, 1, true);
		return wrap_window(L, new TTreeViewLua(p, L, has_lines));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.tree'");
		return 0;
	}
}

inline TTreeViewLua* check_treewnd(lua_State* L)
{
	return dynamic_cast<TTreeViewLua*>(window_arg(L));
}

//tree:set_iconlib( [path = "toolbar\\cool.dll"] [, small = true] )
//tabbar:set_iconlib( [path = "toolbar\\cool.dll"] [, small = true] )
int do_set_iconlib(lua_State* L)
{
	TTwinIconed* win = dynamic_cast<TTwinIconed*>(window_arg(L));
	if (win) {
		gui_string txt = StringFromUTF8(luaL_optstring(L, 2, "toolbar\\cool.dll"));
		bool small_size = optboolean(L, 3, true);
		int icons_loaded = win->load_icons(txt.c_str(), small_size);
		lua_pushinteger(L, icons_loaded);
		return 1;
	}
	else
		luaL_error(L, "do_tree_set_iconlib: there is no tree or tabar at 1st arg");
	return 0;
}

// tree_wnd:tree:tree_set_colour() -- set colors
int do_tree_set_colour(lua_State* L)
{
	if (auto tr = check_treewnd(L)) {
		tr->set_foreground(convert_colour_spec(luaL_checkstring(L, 2)));
		tr->set_background(convert_colour_spec(luaL_checkstring(L, 3)));
	}
	else
		luaL_error(L, "tree_get_item_selected: there is no tree at 1st arg");
	return 0;
}

// tree_wnd:tree:tree_get_item_selected() -- return handle of selected item
int tree_get_item_selected(lua_State* L)
{
	if (auto tr = check_treewnd(L)) {
		Handle sel_itm = tr->get_selected();
		if (sel_itm)
			lua_pushlightuserdata(L, sel_itm);
		else
			lua_pushnil(L);
		return 1;
	}
	else
		luaL_error(L, "tree_get_item_selected: there is no tree at 1st arg");
	return 0;
}

// tree_wnd:tree_set_item_text( item_ud, caption )
int tree_set_item_text(lua_State* L)
{
	if (auto tr = check_treewnd(L)) {
		Handle ud = lua_touserdata(L, 2);
		if (ud)
		{
			gui_string txt = StringFromUTF8(luaL_checkstring(L, 3));
			tr->set_itm_text(ud, txt.c_str());
		}
		else
			luaL_error(L, "tree_set_item_text: there is no tree at 2nd arg");
	}
	else
		luaL_error(L, "tree_set_item_text: there is no tree at 1st arg");
	return 0;
}

// tree_wnd:tree_get_item_text( item_ud )
int tree_get_item_text(lua_State* L)
{
	if (auto tr = check_treewnd(L)) {
		Handle ud = lua_touserdata(L, 2);
		if (ud) {
			const gui_string str = tr->get_itm_text(ud);
			lua_pushwstring(L, str);
			return 1;
		}
		else
			luaL_error(L, "tree_get_item_text: there is no treeitem at 2nd arg");
	}
	else
		luaL_error(L, "tree_get_item_text: there is no tree at 1st arg");
	return 0;
}

// tree_wnd:tree_get_item_data( item_ud )
int tree_get_item_data(lua_State* L)
{
	if (auto tr = check_treewnd(L)) {
		if (Handle ud = lua_touserdata(L, 2)) {
			if (int data = tr->get_itm_data(ud))
				lua_rawgeti(L, LUA_REGISTRYINDEX, data);
			else
				lua_pushnil(L);
			return 1;
		}
		else
			luaL_error(L, "tree_get_item_data: there is no treeitem at 2nd arg");
	}
	else
		luaL_error(L, "tree_get_item_data: there is no tree at 1st arg");
	return 0;
}

int do_tree_set_LabelEditable(lua_State* L)
{
	if (auto tr = check_treewnd(L))
		tr->makeLabelEditable(lua_toboolean(L, 2));
	else
		luaL_error(L, "tree_makeLabelEditable: there is no tree at 1st arg");
	return 0;
}

TListViewLua* list_window_arg(lua_State* L)
{
	return dynamic_cast<TListViewLua*>(window_arg(L));
}

int do_wnd_set_theme(lua_State* L)
{
	if (auto lst = list_window_arg(L))
		lst->set_theme(lua_toboolean(L, 2));
	else if (auto tree = check_treewnd(L))
		tree->set_theme(lua_toboolean(L, 2));
	else 
		luaL_error(L, "do_wnd_set_theme: there is no tree or list at 1st arg");
	return 0;
}

// list_wnd:select_item( id )
// tree_wnd:select_item( item_ud )
int window_select_item(lua_State* L)
{
	if (auto lv = list_window_arg(L)) {
		lv->select_item((int)luaL_checkinteger(L, 2));
	}
	else if (auto tr = check_treewnd(L)) {
		tr->select(lua_touserdata(L, 2));
	}
	return 0;
}

//static wchar_t** table_to_str_array(lua_State* L, int idx, int* psz = NULL)
inline void table_to_str_array(lua_State* L, int idx, vecws& res)
{
	if (!lua_istable(L, idx)) {
		throw_error(L, L"table argument expected");
	}
	lua_pushnil(L); // first key
	while (lua_next(L, idx) != 0) {
		res.emplace_back(StringFromUTF8(lua_tostring(L, -1)));
		lua_pop(L, 1);  /* removes `value'; keeps `key' for next iteration */
	}
}

// list_wnd:delete_item(index)
int window_delete_item(lua_State* L)
{
	if(auto lv = list_window_arg(L))
		lv->delete_item((int)luaL_checkinteger(L, 2));
	return 0;
}

void fill_menu(lua_State* L, Menu& mnu)
{
	vecws items;
	table_to_str_array(L, 2, items);
	for (auto& it : items) {
		size_t pos = it.find_first_of(L"|");
		if (pos == std::string::npos)
		{
			mnu.add_separator();
		}
		else
		{
			auto text = it.substr(0, pos);
			auto fun = it.substr(pos + 1);
			lua_pushwstring(L, fun);
			int data = luaL_ref(L, LUA_REGISTRYINDEX);
			mnu << Item(text.data(), (EH)&LuaWindow::handler, (void*)data);
		}
	}
}

int window_context_menu(lua_State* L)
{
	TWin* w = window_arg(L, 1);
	if (LuaWindow* cw = dynamic_cast<LuaWindow*> (w)) {
		ContextMenu mnu(cw);
		fill_menu(L, mnu);
	}
	else if (TTabControlLua* tc = dynamic_cast<TTabControlLua*>(w)) { //notify
		TEventWindow* parent = tc->get_parent_win();
		HMENU hm = CreatePopupMenu();
		Popup mnu(hm);
		fill_menu(L, mnu);
		tc->set_popup_menu(hm);
		mnu.get_menu_handler()->set_form(parent);
		parent->add_handler(mnu.get_menu_handler());
	}
	else if (TListViewLua* lv = dynamic_cast<TListViewLua*>(w)) { //notify
		TEventWindow* parent = lv->get_parent_win();
		HMENU hm = CreatePopupMenu();
		Popup mnu(hm);
		lv->set_popup_menu(hm);
		mnu.get_menu_handler()->set_form(parent);
		parent->add_handler(mnu.get_menu_handler());
		fill_menu(L, mnu);
	}
	else if (TTreeViewLua* tr = dynamic_cast<TTreeViewLua*>(w)) { //notify
		TEventWindow* parent = tr->get_parent_win();
		HMENU hm = CreatePopupMenu();
		Popup mnu(hm);
		fill_menu(L, mnu);
		tr->set_popup_menu(hm);
		mnu.get_menu_handler()->set_form(parent);
		parent->add_handler(mnu.get_menu_handler());
	}
	else if (TMemoLua* tm = dynamic_cast<TMemoLua*>(w)) { // tcontrol
		TEventWindow* parent = tm->get_parent_win();
		HMENU hm = CreatePopupMenu();
		Popup mnu(hm);
		fill_menu(L, mnu);
		tm->set_popup_menu(hm);
		mnu.get_menu_handler()->set_form(parent);
		parent->add_handler(mnu.get_menu_handler());
	}
	return 0;
}

int window_aux_item(lua_State* L, bool at_index)
{
	TWin* w = window_arg(L);
	if (TListViewLua* lv = dynamic_cast<TListViewLua*>(w)) {
		int next_arg, ipos;
		Handle data = NULL;
		if (at_index) {
			next_arg = 3;
			ipos = (int)luaL_checkinteger(L, 2);
		}
		else {
			next_arg = 2;
			ipos = lv->count();
		}
		if (!lua_isnoneornil(L, next_arg + 1)) {
			lua_pushvalue(L, next_arg + 1);
			data = (Handle)luaL_ref(L, LUA_REGISTRYINDEX);
		}
		if (lua_isstring(L, next_arg)) {
			lv->add_item_at(ipos, StringFromUTF8(luaL_checkstring(L, next_arg)).c_str(), 0, data); // single column init with string
		}
		else {
			vecws items;
			table_to_str_array(L, next_arg, items);
			const int _min = min(lv->columns(), items.size());
			int idx = lv->add_item_at(ipos, items.at(0).data(), 0, data); // init first column
			for (int i = 1; (i < _min) && items.at(i).size(); ++i) // init others
				lv->add_subitem(idx, items.at(i).data(), i);
		}
	}
	else if (TTreeViewLua* tv = dynamic_cast<TTreeViewLua*>(w)) {
		Handle parent = lua_touserdata(L, 3);
		int icon_idx = luaL_optinteger(L, 4, -1);
		int selicon_idx = luaL_optinteger(L, 5, -1);
		Handle data = NULL;
		if (!lua_isnoneornil(L, 6))
		{
			lua_pushvalue(L, 6);
			data = (Handle)luaL_ref(L, LUA_REGISTRYINDEX);
		}
		Handle h = tv->add_item(StringFromUTF8(luaL_checkstring(L, 2)).c_str(), parent, icon_idx, selicon_idx, data);
		if (h) {
			lua_pushlightuserdata(L, h);
			return 1;
		}
	}
	return 0;
}

// list_wnd:add_item(text, data)
// list_wnd:add_item({text1,text2}, data)
// tree:add_item( text [, parent_item = root][, id_icon = -1][, id_selicon = -1][, data = null] )
int window_add_item(lua_State* L)
{
	return window_aux_item(L, false);
}

// list_wnd:insert_item(index, string)
int window_insert_item(lua_State* L)
{
	window_aux_item(L, true);
	return 0;
}

// list_wnd:add_column( sTitle, iSize)
// @param sTitle
// @param iWidth
int window_add_column(lua_State* L)
{
	if (auto lv = list_window_arg(L))
		lv->add_column(StringFromUTF8(luaL_checkstring(L, 2)).c_str(), (int)luaL_checkinteger(L, 3));
	return 0;
}

// list_wnd:get_item_text( index )
int window_get_item_text(lua_State* L)
{
	if (auto lv = list_window_arg(L))
	{
		wchar_t* buff = new wchar_t[BUFFER_SIZE];
		lv->get_item_text((int)luaL_checkinteger(L, 2), buff, BUFFER_SIZE);
		lua_pushwstring(L, buff);
		delete[] buff;
		return 1;
	}
	return 0;
}

// list_wnd:get_item_data( index )
int window_get_item_data(lua_State* L)
{
	if (auto lv = list_window_arg(L))
	{
		void* data = list_window_arg(L)->get_item_data((int)luaL_checkinteger(L, 2));
		lua_rawgeti(L, LUA_REGISTRYINDEX, (lua_Integer)data);
		return 1;
	}
	return 0;
}

// list_wnd:set_list_colour( sForeColor, sBackColor )
int window_set_colour(lua_State* L)
{
	if (auto ls = list_window_arg(L)) {
		ls->set_foreground(convert_colour_spec(luaL_checkstring(L, 2)));
		ls->set_background(convert_colour_spec(luaL_checkstring(L, 3)));
	}
	return 0;
}

// list_wnd:get_selected_item(index)
// @return index
int window_selected_item(lua_State* L)
{
	if (auto lv = list_window_arg(L))
		lua_pushinteger(L, lv->selected_id());
	else
		lua_pushinteger(L, -1);
	return 1;
}

// list_wnd:get_selected_items(index)
// @return {idx1, idx2, idx3,...}
int window_selected_items(lua_State* L)
{
	TListViewLua* lv = list_window_arg(L);
	int i = -1;
	int idx = 0;
	lua_newtable(L);
	while (lv) {
		i = lv->next_selected_id(i);
		if (i < 0) break;
		lua_pushinteger(L, i);
		lua_rawseti(L, -2, ++idx);
	}
	return 1;
}

/*list_wnd:selected_count()
* @return number of selected items
*/
int window_selected_count(lua_State* L)
{
	if (auto lv = list_window_arg(L))
		lua_pushinteger(L, lv->selected_count());
	return 1;
}

/* gui.list([multiple_columns = false][, single_select = true])
* @param bMultiColumn [=false]
* @param bSingleSelect [=true]
* @return list window or error
*/
int new_list_window(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		bool multiple_columns = optboolean(L, 1);
		bool single_select = optboolean(L, 2, true);
		TListViewLua* lv = new TListViewLua(p, L, multiple_columns, single_select);
		return wrap_window(L, lv);
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.list'");
		return 0;
	}
}

/* list_wnd:autosize( index [, by_contents = false])
*  @param iIndex
*  @param bFlag [=false]
*/
int window_autosize(lua_State* L)
{
	list_window_arg(L)->autosize_column((int)luaL_checkinteger(L, 2), optboolean(L, 3));
	return 0;
}

//////////////////////////
class TListBoxLua :public TListBox, public LuaControl
{
public:
	TListBoxLua(TWin*  parent, lua_State* l, int id, bool is_sorted)
		: TListBox(parent ,id, is_sorted), LuaControl(l)
	{};
	void clear()
	{
		int items_count = count();
		for (int i = 0; i < items_count; i++)
			if (lua_Integer ud_id = (lua_Integer)get_data(i))
				luaL_unref(L, LUA_REGISTRYINDEX, ud_id);
		TListBox::clear();
	}
};

// create new listbox
int new_listbox(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		int id = luaL_checkinteger(L, 1);
		bool is_sorted = optboolean(L, 2);
		TListBoxLua* lb = new TListBoxLua(p, L, id, is_sorted);
		return wrap_window(L, lb);
	}
	else {
		luaL_error(L, "There is no parent panel to create 'new_listbox'");
		return 0;
	}
}

inline TListBoxLua* check_listbox(lua_State* L)
{
	return dynamic_cast<TListBoxLua*>(window_arg(L, 1));
}

int do_listbox_insert(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		int id = luaL_checkinteger(L, 2) - 1;
		const char* val = luaL_checkstring(L, 3);
		lb->insert(id, StringFromUTF8(val).c_str());
		if (!lua_isnil(L, 4))
		{
			lua_pushvalue(L, 4);
			int ud_id = luaL_ref(L, LUA_REGISTRYINDEX);
			lb->set_data(id, (void*)ud_id);
		}
	}
	else
		luaL_error(L, "argument 1 is not a ListBox");
	return 0;
}

int do_listbox_append(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		int id = lb->count();
		const char* val = luaL_checkstring(L, 2);
		lb->insert(id, StringFromUTF8(val).c_str());
		if (!lua_isnil(L, 3))
		{
			lua_pushvalue(L, 3);
			int ud_id = luaL_ref(L, LUA_REGISTRYINDEX);
			lb->set_data(id, (void*)ud_id);
		}
	}
	else
		luaL_error(L, "argument 1 is not a ListBox");
	return 0;
}

int do_listbox_remove(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		int id = luaL_checkinteger(L, 2) - 1;
		lua_Integer ud_id = (lua_Integer)lb->get_data(id);
		if (ud_id) {
			luaL_unref(L, LUA_REGISTRYINDEX, ud_id);
		}
		lb->remove(id);
	}
	else
		luaL_error(L, "argument 1 is not a ListBox");
	return 0;
}

/*list_wnd:count()
* listbox:count()
* @return count of elements
*/
int do_list_elements_count(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		lua_pushinteger(L, lb->count());
		return 1;
	}
	else if (auto lv = list_window_arg(L))
	{
		int sz = lv->count();
		lua_pushinteger(L, sz);
		return 1;
	}
	else
		luaL_error(L, "argument 1 is not a ListBox or ListView");
	return 0;
}

int do_listbox_get_text(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		int id = luaL_checkinteger(L, 2) - 1;
		TCHAR tmp[MAX_PATH]{};
		lb->get_text(id, tmp);
		lua_pushwstring(L, tmp);
		return 1;
	}
	else
		luaL_error(L, "argument 1 is not a ListBox");
	return 0;
}

int do_listbox_get_data(lua_State* L)
{
	if (auto lb = check_listbox(L))
	{
		int id = luaL_checkinteger(L, 2) - 1;
		if (lua_Integer ud_id = (lua_Integer)lb->get_data(id))
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, ud_id);
			return 1;
		}
	}
	else
		luaL_error(L, "argument 1 is not a ListBox");
	return 0;
}

////////////////////////// generic methods for window //////////////////////////

// list_wnd:clear()
// tree_wnd:clear()
// listbox_wnd:clear()
int window_clear(lua_State* L)
{
	TWin* w = window_arg(L);
	if (TListViewLua* lv = dynamic_cast<TListViewLua*>(w)) {
		lv->clear();
	}
	else if (TTreeViewLua* tr = dynamic_cast<TTreeViewLua*>(w)) {
		tr->clear();
	}
	else if (TListBoxLua* lb = dynamic_cast<TListBoxLua*>(w)) {
		lb->clear();
	}
	return 0;
}

int do_wnd_selection(lua_State* L)
{
	if (TTrackBarLua* trb = dynamic_cast<TTrackBarLua*>(window_arg(L, 1)))
	{
		if (lua_gettop(L) == 1)
		{
			lua_pushinteger(L, trb->sel_start());
			lua_pushinteger(L, trb->sel_end());
			return 2;
		}
		else
		{
			int imin = luaL_optinteger(L, 2, 0);
			int imax = luaL_optinteger(L, 3, 100);
			trb->selection(imin, imax);
		}
	}
	else if (auto lb = check_listbox(L))
	{
		if (lua_gettop(L) == 1)
		{
			lua_pushinteger(L, lb->selected() + 1);
			return 1;
		}
		else
		{
			// set selection
			int id = luaL_checkinteger(L, 2) - 1;
			lb->selected(id);
		}
	}
	return 0;
}

// wnd:on_select(function or <name global function>)
int window_on_select(lua_State* L)
{
	if (LuaControl* lc = dynamic_cast<LuaControl*>(window_arg(L, 1)))
		lc->set_select(2);
	else
		luaL_error(L, "argument 1 is not a LuaControl");
	return 0;
}

// wnd:on_double_click(function or <name global function>)
int window_on_double_click(lua_State* L)
{
	if (LuaControl* lc = dynamic_cast<LuaControl*>(window_arg(L, 1)))
		lc->set_double_click(2);
	else
		luaL_error(L, "argument 1 is not a LuaControl");
	return 0;
}

// wnd:on_close(function or <name global function>)
int window_on_close(lua_State* L)
{
	if (LuaWindow* cw = dynamic_cast<LuaWindow*>(window_arg(L)))
		cw->set_on_close(2);
	return 0;
}

// wnd:on_command(function or <name global function>)
int window_on_command(lua_State* L)
{
	if (LuaWindow* cw = dynamic_cast<LuaWindow*>(window_arg(L)))
		cw->set_on_command(2);
	return 0;
}

// wnd:on_scroll(function or <name global function>)
int window_on_scroll(lua_State* L)
{
	if (LuaWindow* cw = dynamic_cast<LuaWindow*>(window_arg(L)))
		cw->set_on_scroll(2);
	return 0;
}

// wnd:on_show(function or <name global function>)
int window_on_show(lua_State* L)
{
	if (LuaWindow* cw = dynamic_cast<LuaWindow*>(window_arg(L)))
		cw->set_on_show(2);
	return 0;
}

// wnd:on_focus(function or <name global function>)
int window_on_focus(lua_State* L)
{
	if (LuaControl* lc = dynamic_cast<LuaControl*>(window_arg(L, 1)))
		lc->set_on_focus(2);
	else
		luaL_error(L, "argument 1 is not a LuaControl");
	return 0;
}

// wnd:on_key(function or <name global function>)
int window_on_key(lua_State* L)
{
	TWin* w = window_arg(L);
	if (TListViewLua* lv = dynamic_cast<TListViewLua*>(w)) {
		lv->set_onkey(2);
	}
	else if (TMemoLua* m = dynamic_cast<TMemoLua*>(w)) {
		m->set_onkey(2);
	}
	return 0;
}

////////////////////////  INI FILE  //////////////////////////////
#define INIFILE_MT "inifile_mt"

int do_inifile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	bool in_cwd = optboolean(L, 2);
	IniFile** pini = (IniFile**)lua_newuserdata(L, sizeof(IniFile**));
	luaL_getmetatable(L, INIFILE_MT);
	lua_setmetatable(L, -2);
	*pini = new IniFile(StringFromUTF8(path).c_str(), in_cwd);
	return 1;
}

IniFile* check_inifile(lua_State* L, int idx = 1)
{
	IniFile** pini = (IniFile**)lua_touserdata(L, idx);
	if (!*pini) throw_error(L, L"there is not ini file");
	return *pini;
}

// ini:set_section(sect)
int do_set_section(lua_State* L)
{
	IniFile* pini = check_inifile(L);
	const char* sect = luaL_checkstring(L, 2);
	pini->set_section(StringFromUTF8(sect).c_str());
	return 0;
}

// ini:write_string(key, val)
int do_write_string(lua_State* L)
{
	IniFile* pini = check_inifile(L);
	const char* key = luaL_checkstring(L, 2);
	const char* val = lua_tostring(L, 3);
	pini->write_string(StringFromUTF8(key).c_str(), StringFromUTF8(val).c_str());
	return 0;
}

// ini:read_string(sect[, def=""])
int do_read_string(lua_State* L)
{
	IniFile* pini = check_inifile(L);
	const char* key = luaL_checkstring(L, 2);
	const char* def = luaL_optstring(L, 3, "");
	std::wstring str = pini->read_string(StringFromUTF8(key).c_str());
	lua_pushstring(L, str.size() ? UTF8FromString(str).c_str() : def);
	return 1;
}

// ini:read_number(sect[, def=0])
int do_read_number(lua_State* L)
{
	IniFile* pini = check_inifile(L);
	const char* key = luaL_checkstring(L, 2);
	lua_Number def = luaL_optnumber(L, 3, 0.0);
	lua_Number res = pini->read_number(StringFromUTF8(key).c_str(), def);
	lua_pushnumber(L, res);
	if (res == lua_tointeger(L, -1)) lua_pushinteger(L, res);
	return 1;
}

int do_ini_gc(lua_State* L)
{
	delete check_inifile(L);
	return 0;
}

////////////// combobox

class TComboBoxLua : public TComboBox, public LuaControl
{
public:
	TComboBoxLua(TWin* parent, lua_State* l, int id)
		: TComboBox(parent, id), LuaControl(l)
	{ }
};

int do_new_cbox(lua_State* L)
{
	if (TWin* p = get_last_parent()) {
		int id_cmd = luaL_optinteger(L, 1, -1);
		return wrap_window(L, new TComboBoxLua(p, L, id_cmd));
	}
	else {
		luaL_error(L, "There is no parent panel to create 'gui.combobox'");
		return 0;
	}
}

TComboBoxLua* check_combo(lua_State* L)
{
	return dynamic_cast<TComboBoxLua*>(window_arg(L, 1));
}

int do_cbox_clear(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		int cnt = cb->count();
		for (int idx = 0; idx < cnt; ++idx)
			if (int ud_id = cb->get_data(idx))
				luaL_unref(L, LUA_REGISTRYINDEX, ud_id);
		cb->reset();
	}
	return 0;
}

int do_cbox_append_string(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		const char* text = luaL_checkstring(L, 2);
		int idx = cb->add_string(StringFromUTF8(text).c_str());
		if (!lua_isnil(L, 3))
		{
			lua_pushvalue(L, 3);
			int ud_id = luaL_ref(L, LUA_REGISTRYINDEX);
			cb->set_data(idx, ud_id);
		}
	}
	return 0;
}

int do_cbox_insert_string(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		int id = luaL_checkinteger(L, 2);
		const char* text = luaL_checkstring(L, 3);
		cb->ins_string(id, StringFromUTF8(text).c_str());
		if (!lua_isnil(L, 4))
		{
			lua_pushvalue(L, 4);
			int ud_id = luaL_ref(L, LUA_REGISTRYINDEX);
			cb->set_data(id, ud_id);
		}
	}
	return 0;
}


int do_cbox_get_data(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		int id = cb->get_cursel();
		if (lua_Integer ud_id = (lua_Integer)cb->get_data(id))
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, ud_id);
			return 1;
		}
	}
	return 0;
}

int do_cbox_set_items_h(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		int sz = luaL_checkinteger(L, 2);
		cb->set_height(sz);
	}
	return 0;
}

int do_cb_setcursel(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		int sz = luaL_checkinteger(L, 2);
		cb->set_cursel(sz);
	}
	return 0;
}

int do_cb_getcursel(lua_State* L)
{
	if (auto cb = check_combo(L))
	{
		lua_pushinteger(L, cb->get_cursel());
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////

int do_log(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	log_add(text);
	return 0;
}

static const struct luaL_Reg gui[] = {
	{"log",do_log},
	// dialogs
	{"colour_dlg",do_colour_dlg},
	{"message",do_message},
	{"open_dlg",do_open_dlg},
	{"save_dlg",do_save_dlg},
	{"select_dir_dlg",do_select_dir_dlg},
	{"prompt_value",do_prompt_value},

	// others
	{"day_of_year",do_day_of_year},
	{"ini_file",do_inifile},
	{"version",do_version},
	{"files",do_files},
	{"chdir",do_chdir},
	{"run",do_run},
	{"get_ascii",do_get_ascii},
	{"pass_focus",do_pass_focus},
	{"set_panel",do_set_panel},
	//{"test_function",do_test_function},

	//windows 
	{"window",new_window},
	{"panel",new_panel},
	{"tabbar",new_tabbar},
	{"list",new_list_window},
	{"memo",new_memo},
	{"tree",new_tree},
	{"button",new_button},
	{"checkbox",new_checkbox},
	{"radiobutton",new_radiobutton},
	{"label",new_label},
	{"trackbar",new_trackbar},
	{"listbox",new_listbox},
	{"combobox",do_new_cbox},

	{NULL, NULL},
};

static const struct luaL_Reg ini_methods[] = {
	{"set_section", do_set_section},
	{"write_string", do_write_string},
	{"write_number", do_write_string},
	{"read_string", do_read_string},
	{"read_number", do_read_number},
	{"__gc", do_ini_gc},
	{NULL, NULL},
};

static const struct luaL_Reg window_methods[] = {
	// window
	{"show",window_show},
	{"hide",window_hide},
	{"size",window_resize},
	{"position",window_position},
	{"bounds",window_get_bounds},
	{"client",window_client},
	{"add",window_add},
	{"remove",window_remove},
	{"context_menu",window_context_menu},

	{"on_close",window_on_close},
	{"on_show",window_on_show},
	//{"on_focus",window_on_focus},
	{"on_key",window_on_key},
	{"on_command",window_on_command},
	{"on_scroll",window_on_scroll},

	// tabbar	
	{"add_tab",tabbar_add},
	{"select_tab",tabbar_sel},

	// list, tree
	{"on_select",window_on_select},
	{"on_double_click",window_on_double_click},
	{"clear",window_clear},
	{"set_selected_item",window_select_item},
	{"count", do_list_elements_count},
	{"delete_item",window_delete_item},
	{"insert_item",window_insert_item},
	{"add_item",window_add_item},
	{"add_column",window_add_column},
	{"get_item_text",window_get_item_text},
	{"get_item_data",window_get_item_data},
	{"set_list_colour",window_set_colour},
	{"selected_count",window_selected_count},
	{"get_selected_item",window_selected_item},
	{"get_selected_items",window_selected_items},
	{"autosize",window_autosize},

	// tree
	{"set_tree_colour",do_tree_set_colour},
	{"set_tree_editable",do_tree_set_LabelEditable},
	{"set_theme",do_wnd_set_theme},

	// tree, tab
	{"set_iconlib",do_set_iconlib},

	// treeItem
	{"tree_get_item_selected",tree_get_item_selected},
	{"tree_get_item_text",tree_get_item_text},
	{"tree_set_item_text",tree_set_item_text},
	{"tree_get_item_data",tree_get_item_data},

	// trackbar
	{"get_pos",trbar_get_pos},
	{"set_pos",trbar_set_pos},
	{"select",do_wnd_selection},
	{"sel_clear",trbar_sel_clear},
	{"range",trbar_set_range},
	
	//memo
	{"set_memo_colour",memo_set_colour},

	// memo, label
	{"set_text",window_set_text},
	{"get_text",window_get_text},

	// button, checkbox
	{"check",do_check},
	{"state",do_state},

	// do_listbox_insert
	{"insert",do_listbox_insert},
	{"append",do_listbox_append},
	{"remove",do_listbox_remove},
	{"get_line_text",do_listbox_get_text},
	{"get_line_data",do_listbox_get_data},

	// combobox
	{"cb_append",do_cbox_append_string},
	{"cb_insert",do_cbox_insert_string},
	{"cb_clear",do_cbox_clear},
	{"cb_items_h",do_cbox_set_items_h},
	{"cb_setcursel",do_cb_setcursel},
	{"cb_getcursel",do_cb_getcursel},
	{"cb_getdata",do_cbox_get_data},
	{NULL, NULL},
};

void on_attach()
{
	// at this point, the SciTE window is available. Can't always assume
	// that it is the foreground window, so we hunt through all windows
	// associated with this thread (the main GUI thread) to find a window
	// matching the appropriate class name

	//EnumThreadWindows(GetCurrentThreadId(),CheckSciteWindow,(LPARAM)&hSciTE);
	hSciTE = FindWindow(L"SciTEWindow", NULL);
	s_parent = new TWin(hSciTE);

	// Its first child shold be the content pane (editor+output), 
	// but we check this anyway....	

	//EnumChildWindows(hSciTE,CheckContainerWindow,(LPARAM)&hContent);
	hContent = FindWindowEx(hSciTE, NULL, L"SciTEWindowContent", NULL);

	// the first child of the content pane is the editor pane.
	bool subclassed = false;
	if (hContent != NULL) {
		log_add("gui inited");
		content_window = new TWin(hContent);
		hCode = GetWindow(hContent, GW_CHILD);
		if (hCode != NULL) {
			code_window = new TWin(hCode);
			subclass_scite_window();
			subclassed = true;
		}
	}
	if (!subclassed) {
		get_parent()->message(L"Cannot subclass SciTE Window", 2);
	}
}

void destroy_windows()
{
	if (extra_window) {
		extra_window->hide();
		extra_window->set_parent(NULL);
		extra_window->close();
		//log_add("destroy extra_window");
		delete extra_window;
		extra_window = nullptr;
	}
	if (extra_window_splitter) {
		extra_window_splitter->hide();
		extra_window_splitter->set_parent(NULL);
		extra_window_splitter->close();
		//log_add("destroy extra_window_splitter");
		delete extra_window_splitter;
		extra_window_splitter = nullptr;
	}
	extra.bottom = extra.top = extra.left = extra.right = 0;
	//shake_scite_descendants();
}

extern "C" __declspec(dllexport)
int luaopen_gui(lua_State * L)
{
	destroy_windows();

	if (pCWin)
	{
		pCWin->hide();
		pCWin->destroy();
		pCWin = nullptr;
	}

	luaL_newmetatable(L, INIFILE_MT);  // create metatable for inifile objects
	lua_pushvalue(L, -1);  // push metatable
	lua_setfield(L, -2, "__index");  // metatable.__index = metatable
	luaL_setfuncs(L, ini_methods, 0);

	luaL_newmetatable(L, WINDOW_CLASS);  // create metatable for window objects
	lua_pushvalue(L, -1);  // push metatable
	lua_setfield(L, -2, "__index");  // metatable.__index = metatable
#if LUA_VERSION_NUM < 502
	luaL_register(L, NULL, window_methods);
	luaL_openlib(L, "gui", gui, 0);
#else
	luaL_setfuncs(L, window_methods, 0);
	luaL_newlib(L, gui);
#endif
	lua_pushvalue(L, -1);  /* copy of module */
	lua_setglobal(L, "gui");
	return 1;
}

void on_destroy()
{
	log_add("gui:on_destroy");
	destroy_windows();
	//log_add("destroy s_parent");
	delete s_parent; //scite
	//log_add("destroy content_window");
	delete content_window;
	//log_add("destroy code_window");
	delete code_window; // editor
}