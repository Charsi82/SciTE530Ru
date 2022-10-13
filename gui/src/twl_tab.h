#pragma once
typedef int (TEventWindow::* NotifyEventHandler)(intptr_t id);
typedef NotifyEventHandler NEH;

class TTabControl : public TNotifyWin, public TTwinIconed {
	int m_index;
	bool has_image;
protected:
	TEventWindow* form;
public:
	TTabControl(TEventWindow* _form, bool multiline = false);
	void add(pchar caption, void* data, int image_idx = -1);
	void remove(int idx = -1);
	void* get_data(int idx = -1);
	void selected(int idx);
	int selected();
	TEventWindow* get_parent_win() { return (TEventWindow*)form; };
	int load_icons(pchar path, bool small_size = true);
	int getRowCount() const;

	virtual void handle_select(int id) = 0;
	virtual int handle_rclick(int id) = 0;
	// override
	int handle_notify(void* p) override;
};
