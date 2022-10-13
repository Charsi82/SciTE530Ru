/* TWL_MENU.H
 */
#pragma once
#include "twl.h"

struct Item;

typedef void (TEventWindow::* EventHandler)();
typedef EventHandler EH;
typedef void (TEventWindow::* SelectionHandler)(intptr_t);
typedef void (TEventWindow::* SelectionHandlerS)(const char*);
typedef void (TEventWindow::* SelectionHandler3)(intptr_t, intptr_t, const char*);
typedef SelectionHandler SH;
typedef void (TEventWindow::* DataEventHandler)(Item*);
typedef EventHandler DEH;

struct Item {
	pchar caption;
	EventHandler handler;
	void* data;
	UINT id;
	bool inactive_data;
	Item(wchar_t* _caption = NULL, EventHandler _handler = NULL, void* data = NULL, UINT id = -1, bool inactive = false);
};

#ifdef IN_MENU_CPP
//#include <list>
#include "list"
typedef std::list<Item> ItemList;
typedef std::list<ACCEL> AccelList;
#else
typedef int ItemList;
typedef int AccelList;
#endif

typedef ItemList* PItemList;
typedef AccelList* PAccelList;

class MessageHandler : public AbstractMessageHandler {
protected:
	PItemList m_list;
public:
	MessageHandler(TEventWindow* form);
	MessageHandler(MessageHandler&) = delete;
	MessageHandler& operator= (MessageHandler&) = delete;
	~MessageHandler();
	void add(Item& item);
	void remove(UINT id);
	bool dispatch(UINT id, int notify, Handle ctrl) override;
	void read() override;
	void write() override;
	void add_handler(AbstractMessageHandler*) override;
};

class Menu {
protected:
	TEventWindow* m_form;
	Handle m_handle;
	MessageHandler* m_menu_handler;
	PAccelList m_accel_list;
public:
	Menu(TEventWindow* form);
	Menu(Menu&) = delete;
	Menu& operator= (Menu&) = delete;
	void add_menu(Item& item);
	void add_menu(Menu& menu);
	void add_separator();
	MessageHandler* get_menu_handler();
	void insert_menu(UINT id_before, Item& item);
	void delete_menu(UINT id);
	operator Handle () { return m_handle; }
	virtual void create();
	virtual void release();
	virtual ~Menu() { /*Menu::release();*/ }
};

class Popup : public Menu {
protected:
	wchar_t* m_name;
public:
	explicit Popup(wchar_t* name);
	explicit Popup(Handle h);
	wchar_t* name() { return m_name; }
	void create() override;
	void release() override;
	//Handle get_handle();
};

class PopupMenu : public Menu {
protected:
	int m_index;
public:
	PopupMenu(TEventWindow* form, int index);
	void create() override;
	void release() override;
};

class ContextMenu : public Menu {
protected:
	TEventWindow* m_main_form;
public:
	explicit ContextMenu(TEventWindow* form);
	void create() override;
	void release() override;
	~ContextMenu() { ContextMenu::release(); }
};

struct Sep {
	int id;
};

// these were all passed by reference...
inline
Menu& operator << (Menu& mnu, Item item)
{
	mnu.add_menu(item);
	return mnu;
}

inline
Menu& operator << (Menu& mnu, Menu sub_mnu)
{
	mnu.add_menu(sub_mnu);
	return mnu;
}

inline
Menu& operator << (Menu& mnu, Sep sep)
{
	mnu.add_separator();
	return mnu;
}
