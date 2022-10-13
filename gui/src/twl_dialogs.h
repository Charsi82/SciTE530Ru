// twl_dialogs.h
#pragma once

class TOpenFile {
protected:
	void* m_ofn;
	wchar_t* m_filename;
	bool m_prompt;
	wchar_t* m_file;
	wchar_t* m_file_out;
	wchar_t* m_path;
public:
	TOpenFile(TWin* parent, pchar caption, pchar filter, bool do_prompt = true);
	TOpenFile(TOpenFile&) = delete;
	TOpenFile& operator= (TOpenFile&) = delete;
	~TOpenFile();
	virtual bool go();
	void initial_dir(pchar dir);
	bool next();
	pchar file_name();
	void file_name(pchar buff);
};

class TSelectDir {
	TWin* parent;
	wchar_t* descr;
	wchar_t* dirPath;
	wchar_t* lpszInitialDir;
	HWND m_hWndTreeView;
	TSelectDir();
public:
	TSelectDir(TWin* _parent, pchar _description = L"", pchar _initialdir = L"");
	TSelectDir(TSelectDir&) = delete;
	TSelectDir& operator= (TSelectDir&) = delete;
	virtual ~TSelectDir();
	virtual bool go();
	pchar path() const
	{
		return dirPath;
	}
private:
	static int WINAPI SHBrowseCallBack(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
};

class TSaveFile : public TOpenFile {
public:
	TSaveFile(TWin* parent, pchar caption, pchar filter, bool do_prompt = true)
		: TOpenFile(parent, caption, filter, false)
	{}
	virtual bool go();
};

class TColourDialog {
protected:
	void* m_choose_color;
public:
	TColourDialog(TWin* parent, unsigned int clr);
	virtual bool go();
	int result();
};
