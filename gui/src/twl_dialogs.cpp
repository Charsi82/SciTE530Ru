// twl_dialogs.cpp
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include "twl.h"
#include "twl_dialogs.h"

const int BUFFSIZE = 1024;

static wchar_t s_initial_dir[MAX_PATH];

TOpenFile::TOpenFile(TWin* parent, pchar caption, pchar filter, bool do_prompt)
{
	OPENFILENAME& ofn = *new OPENFILENAME;
	m_ofn = &ofn;
	m_prompt = do_prompt;
	m_filename = new wchar_t[BUFFSIZE];
	m_file_out = new wchar_t[MAX_PATH];
	m_file = nullptr;
	m_path = nullptr;
	*m_filename = '\0';
	wchar_t* p_filter = _wcsdup(filter);
	for (wchar_t* p = p_filter; *p; p++)
		if (*p == '|') *p = '\0';

	ZeroMemory(m_ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = (HWND)parent->handle();
	ofn.lpstrFilter = p_filter;
	ofn.nFilterIndex = 1;
	ofn.nMaxFile = BUFFSIZE;
	ofn.lpstrTitle = (wchar_t*)caption;
	ofn.lpstrFile = m_filename;

	GetCurrentDirectory(MAX_PATH, s_initial_dir);
	initial_dir(s_initial_dir);
}

TOpenFile::~TOpenFile()
{
	delete LPOPENFILENAME(m_ofn);
	delete m_filename;
	delete m_file_out;
}

void TOpenFile::initial_dir(pchar dir)
{
	LPOPENFILENAME(m_ofn)->lpstrInitialDir = dir;
}

bool TOpenFile::go()
{
	LPOPENFILENAME ofn = LPOPENFILENAME(m_ofn);
	if (m_prompt) ofn->Flags = OFN_CREATEPROMPT | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
	int ret = GetOpenFileName(ofn);
	if (ofn->nFileExtension == 0) { // multiple selection
		m_path = m_filename;
		m_file = m_filename + ofn->nFileOffset;
		ofn->nFileOffset = 0;
	}
	else m_path = NULL;
	return ret;
}

pchar TOpenFile::file_name()
{
	if (m_path) { // multiple selection: build up each path individually
		wcscpy_s(m_file_out, MAX_PATH, m_path);
		wcscat_s(m_file_out, MAX_PATH, L"\\");
		wcscat_s(m_file_out, MAX_PATH, m_file);
		return m_file_out;
	}
	else
		return m_filename;
}

bool TOpenFile::next()
{
	if (m_path == NULL) return false;
	m_file += wcslen(m_file) + 1;
	bool finished = *m_file == '\0';
	if (finished) m_path = NULL;
	return !finished;
}

void TOpenFile::file_name(pchar file)
{
	wcscpy_s(m_filename, MAX_PATH, file);
}

/*
bool TSaveFile::go()
{
 if (m_prompt) LPOPENFILENAME(m_ofn)->Flags = OFN_OVERWRITEPROMPT;
 m_path = NULL;
 return GetSaveFileName((LPOPENFILENAME)m_ofn);
}
*/

bool TSaveFile::go()
{
	LPOPENFILENAME ofn = LPOPENFILENAME(m_ofn);
	if (m_prompt) ofn->Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
	int ret = GetSaveFileName(ofn);
	if (ofn->nFileExtension == 0) { // multiple selection
		m_path = m_filename;
		m_file = m_filename + ofn->nFileOffset;
		ofn->nFileOffset = 0;
	}
	else m_path = NULL;
	return ret;
}

static COLORREF custom_colours[16];

TColourDialog::TColourDialog(TWin* win, unsigned int cl)
{
	LPCHOOSECOLOR clr = new CHOOSECOLOR;
	clr->lStructSize = sizeof(CHOOSECOLOR);
	clr->hwndOwner = (HWND)win->handle();
	clr->hInstance = NULL;
	clr->rgbResult = cl;
	clr->lpCustColors = custom_colours;
	clr->Flags = CC_RGBINIT | CC_FULLOPEN;
	clr->lCustData = 0;
	clr->lpfnHook = NULL;
	clr->lpTemplateName = NULL;
	m_choose_color = clr;
}


bool TColourDialog::go()
{
	return ChooseColor(LPCHOOSECOLOR(m_choose_color));
}

int TColourDialog::result()
{
	return LPCHOOSECOLOR(m_choose_color)->rgbResult;
}


TSelectDir::TSelectDir(TWin* _parent, pchar _description, pchar _initialdir)
	: parent(_parent), descr(0), dirPath(new wchar_t[MAX_PATH]), m_hWndTreeView(NULL)
{
	//memset(dirPath, 0, MAX_PATH);
	memset(dirPath, 0, MAX_PATH * sizeof(dirPath[0]));
	//descr = new wchar_t[::lstrlen(_description)+1];
	descr = new wchar_t[lstrlen(_description) + 1];
	lstrcpy(descr, _description);

	//lpszInitialDir=new wchar_t[::lstrlen(_initialdir)+1];
	lpszInitialDir = new wchar_t[lstrlen(_initialdir) + 1];
	lstrcpy(lpszInitialDir, _initialdir);
}

struct SB_INITDATA
{
	wchar_t* lpszInitialDir;
	TSelectDir* pSHBrowseDlg;
};

int WINAPI TSelectDir::SHBrowseCallBack(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		static TSelectDir* pSBDlg = reinterpret_cast<SB_INITDATA*>(lpData)->pSHBrowseDlg;
		// Set initial directory
		if (lpData && *(wchar_t*)lpData)
		{
			::SendMessage(hWnd,
				BFFM_SETSELECTION,
				TRUE,
				LPARAM(reinterpret_cast<SB_INITDATA*>(lpData)->lpszInitialDir)
			);
		}
		pSBDlg->m_hWndTreeView = FindWindowEx(hWnd, NULL, WC_TREEVIEW, NULL);
	}
	return 0;
} // SHBrowseCallBack()

TSelectDir::~TSelectDir()
{
	delete[] descr;
	delete[] dirPath;
	delete[] lpszInitialDir;
}

bool TSelectDir::go()
{
	BROWSEINFO bi = { 0 };
	SB_INITDATA sbInit = { lpszInitialDir, this };

	bi.hwndOwner = (HWND)parent->handle();
	bi.lpszTitle = descr;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bi.lpfn = BFFCALLBACK(SHBrowseCallBack);
	bi.lParam = LPARAM(&sbInit);

	LPMALLOC shellMalloc = 0;
	SHGetMalloc(&shellMalloc);
	LPCITEMIDLIST pidl = ::SHBrowseForFolder(&bi);

	bool result = false;
	if (pidl) {
		result = ::SHGetPathFromIDList(pidl, dirPath) != FALSE;
	}
	shellMalloc->Release();

	return result;
}
