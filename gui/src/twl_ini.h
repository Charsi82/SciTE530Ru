// TWL_INI.H
#pragma once
#include "twl.h"
const int BUFSZ = MAX_PATH;

class IniFile {
private:
	static TCHAR _tmpbuff_[BUFSZ];
	TCHAR m_file[BUFSZ], m_section[BUFSZ];
public:
	explicit IniFile(pchar file, bool in_cwd = false);
	void set_section(pchar section);
	void write_string(pchar key, pchar value);
	TCHAR* read_string(pchar key, TCHAR* value = _tmpbuff_, int sz = BUFSZ, pchar def = L"");
	void write_number(pchar key, double val);
	double read_number(pchar key, double def = 0.0);
};
