#pragma once
#ifndef IS_IMPLEMENTATION
typedef Handle HIMAGELIST;
#endif

// these are indices for some useful shell icons found in shell32.dll
//enum {
//	ICON_UNKNOWN, ICON_DOCUMENT, ICON_PROGRAM, ICON_CLOSED_FOLDER, ICON_OPEN_FOLDER,
//	ICON_DRIVE = 7, ICON_COMPUTER = 15, ICON_COMPUTERS = 17, ICON_STUFFED_FOLDER = 20, ICON_SEARCH = 22
//};

class TImageList {
	HIMAGELIST m_handle;
	bool m_small_icons;
public:
	Handle handle() { return m_handle; }
	TImageList(int cx, int cy);
	TImageList(bool s = true);
	void create(int cx, int cy);
	int add_icon(pchar iconfile);
	int add(pchar bitmapfile, COLORREF mask_clr = 1);
	int load_icons_from_module(pchar mod, bool icon_small = true);
	void set_back_colour(COLORREF clrRef);
	void load_shell_icons();
};
