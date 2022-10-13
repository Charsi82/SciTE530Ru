#pragma once
#include <string>
typedef std::wstring gui_string;
typedef wchar_t gui_char;
typedef const wchar_t* pchar;

gui_string StringFromUTF8(const char* s);
std::string UTF8FromString(const std::wstring& s);