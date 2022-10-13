// TWL_CNTRLS.H
#pragma once
#include "twl.h"
#include <richedit.h>
void subclass_control(TControl* ctrl);
void remove_subclass_control(TControl* ctrl);
const int ID_OK = 1, ID_CANCEL = 2;

class TButton : public TControl {
public:
	enum class ButtonStyle  { // standard Button styles
		PUSHBUTTON,
		DEFPUSHBUTTON,
		CHECKBOX,
		AUTOCHECKBOX,
		RADIOBUTTON,
		B3STATE,
		AUTO3STATE,
		GROUPBOX,
		USERBUTTON,
		AUTORADIOBUTTON,
		LEFTTEXT = 0x20,
		ICON = 0x40,
		BITMAP = 0x80
	};

	TButton(TWin* parent, pchar caption, int id, ButtonStyle style = ButtonStyle::PUSHBUTTON);
	pchar type_name() { return L"TButton"; }
	void check(bool c);
	bool check() const;
	void state(bool s);
	bool state() const;
	void calc_size();
};

class TCheckBox : public TButton {
public:
	TCheckBox(TWin* parent, pchar caption, int id, bool is_auto = false);
	pchar type_name() { return L"TCheckBox"; }
	void calc_size() override;
};

class TRadioButton : public TButton {
public:
	TRadioButton(TWin* parent, pchar caption, int id, bool is_auto = false);
	pchar type_name() { return L"TRadioButton"; }
	void calc_size() override;
};

class TListBox : public TControl {
public:
	TListBox(TWin* parent, int id, bool is_sorted = false);
	pchar type_name() { return L"TListBox"; }
	void add(pchar str, void* ptr = NULL);
	void insert(int i, pchar str);
	void remove(int i);
	void clear();
	void redraw(bool on);
	int  count();
	void selected(int idx);
	int  selected() const;
	void get_text(int idx, wchar_t* buff);
	size_t get_textlen(int idx);
	void  set_data(int i, void* ptr);
	void* get_data(int i);
};

class TMemo : public TControl {
protected:
	CHARFORMAT* m_pfmt;
	wchar_t* m_file_name;
public:
	TMemo(TWin* parent, int id, bool do_scroll = false, bool plain = false);
	TMemo(TMemo&) = delete;
	TMemo& operator= (TMemo&) = delete;
	~TMemo();
	pchar type_name() { return L"TMemo"; }
	pchar file_name();
	void cut();
	void copy();
	void clear();
	void paste();
	//
	void undo();
	void select_all();
	int text_size();
	void replace_selection(pchar str);
	bool modified();
	void modified(bool yesno);
	void go_to_end();
	void scroll_line(int line);
	void scroll_caret();
	//
	int line_count();
	int line_offset(int line);
	int line_from_pos(int pos);
	int line_size(int line);
	int get_line_text(int line, char* buff, int sz);
	void get_selection(int& start, int& finish);
	void set_selection(int start, int finish);
	wchar_t* get_buffer();
	void release_buffer();

	// Rich edit interface!
	void auto_url_detect(bool yn);
	void send_char_format();
	void find_char_format();
	COLORREF get_text_colour();
	void set_text_colour(COLORREF colour);
	void set_font(pchar facename, int size, int flags, bool selection = false);
	void go_to(int idx1, int idx2 = -1, int nscroll = 0);
	int current_pos();
	int current_line();
	void go_to_line(int line);
	COLORREF get_line_colour(int l);
	void set_line_colour(int line, COLORREF colour);
	void set_background_colour(COLORREF colour);
	virtual int handle_onkey(int id) = 0;
	virtual int handle_rclick() = 0;
	int handle_notify(void* p);
};

// edit box notifications
//const int EDIT_ENTER = 0x0100, EDIT_LEAVE = 0x0200, EDIT_CHANGED = 0x0400;

class TTrackBar : public TControl {
protected:
	bool m_redraw;
public:
	TTrackBar(TWin* parent, DWORD style, int id);
	void redraw(bool yes) { m_redraw = yes; }
	bool redraw() { return m_redraw; }
	void selection(int lMin, int lMax);
	void sel_start(int lStart);
	int  sel_start(); // returns starting pos of current selection
	int  sel_end(); // returns end pos
	void sel_clear();
	int  pos();
	void pos(int lPos);
	void range(int lMin, int lMax);

};
