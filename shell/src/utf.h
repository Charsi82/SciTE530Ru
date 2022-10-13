#include <string>

typedef std::wstring gui_string;
typedef wchar_t gui_char;

gui_string StringFromUTF8(const char* s);
std::string UTF8FromString(const std::wstring& s);
