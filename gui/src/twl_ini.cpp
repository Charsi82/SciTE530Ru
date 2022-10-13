// TWL_INI.CPP
/*
 * Steve Donovan, 2003
 * This is GPL'd software, and the usual disclaimers apply.
 * See LICENCE
*/
#include <windows.h>
#include <direct.h>
#include "twl_ini.h"
#include "utf.h"

void get_app_path(TCHAR* buff, int sz)
{
	int nLen = GetModuleFileName(NULL, buff, sz);
	while (nLen > 0 && buff[nLen] != L'\\') buff[nLen--] = 0;
}

wchar_t IniFile::_tmpbuff_[BUFSZ];

void IniFile::write_number(pchar key, double val)
{
	wsprintf(_tmpbuff_, L"%f", val);
	write_string(key, _tmpbuff_);
}

double IniFile::read_number(pchar key, double def)
{
	double res = _wtof(read_string(key));
	return (!res) ? def : res;
}

IniFile::IniFile(pchar file, bool in_cwd)
{
	lstrcpy(m_section, L"_section_");
	if (!in_cwd)
		//m_file = _wcsdup(file);
		 lstrcpy(m_file, file);
	else {
		get_app_path(m_file, BUFSZ);
		lstrcat(m_file, file);
	}
}

void IniFile::set_section(pchar section)
{
	lstrcpy(m_section, section);
}

void IniFile::write_string(pchar key, pchar value)
{
	WritePrivateProfileString(m_section, key, value, m_file);
}

TCHAR* IniFile::read_string(pchar key, TCHAR* value, int sz, pchar def)
{
	GetPrivateProfileString(m_section, key, def, value, sz, m_file);
	return value;
}
