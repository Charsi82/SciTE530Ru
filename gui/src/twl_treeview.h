#pragma once
#include "utf.h"

class TTreeView : public TNotifyWin, public TTwinIconed {
protected:
	TWin*	   m_form;
	bool m_has_images;
	void set_image_list(TImageList* il, bool normal = true);

private:
	virtual void clean_data(int) = 0;
	Handle get_next(Handle itm);

public:
	TTreeView(TWin* form, bool has_lines = true);
	~TTreeView() { clear(); }
	Handle add_item(pchar caption, Handle parent = NULL, int idx1 = 0, int idx2 = -1, Handle data = NULL);
	TEventWindow* get_parent_win() { return (TEventWindow*)m_form; };
	int get_itm_data(Handle pn);
	void select(Handle p);
	int load_icons(pchar path, bool small_size);
	Handle get_selected();
	void clear();
	gui_string get_itm_text(Handle);
	void set_itm_text(void* itm, pchar str);
	void set_foreground(COLORREF clr);
	void set_background(COLORREF clr);
	void makeLabelEditable(bool toBeEnabled);
	void set_theme(bool explorer);

	Handle get_root();
	void iterate_childs(Handle itm);
	void remove_item(Handle itm);

	virtual int handle_rclick(Handle) = 0;
	virtual void handle_select(Handle) = 0;
	virtual void handle_dbclick(Handle) = 0;
	int handle_notify(void* p) override;


};
