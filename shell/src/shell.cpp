//build@ gcc -shared -o shell.dll -I shell.cpp scite.la -lstdc++

#include <windows.h>
#include <shlwapi.h>
#include "utf.h" // Needs for WideString and UTF8 conversions
#include "md5.h"
#include "sha-256.h"
#include "lua.hpp"

#pragma warning(push)
#pragma warning(disable: 4710)

template < class T, int defSize >
class CMemBuffer
{
public:
	CMemBuffer()
		: m_pData(NULL)
		, m_iSize(defSize)
	{
		SetLength(defSize);
	}

	~CMemBuffer()
	{
		SetLength(0);
	}

	BOOL IsBufferEmpty()
	{
		return m_pData == NULL;
	}

	T* GetBuffer()
	{
		return m_pData;
	}

	T& operator [] (int nItem)
	{
		return m_pData[nItem];
	}

	int GetBufferLength()
	{
		return m_iSize;
	}

	// установить длину буфера точно
	// 0 - очищает буфер
	BOOL SetLength(int lenNew)
	{
		if (lenNew > 0)
		{
			T* sNew = (T*)malloc(lenNew * sizeof(T));
			//			T* sNew = (T*)::VirtualAlloc( NULL, lenNew * sizeof(T), MEM_COMMIT, PAGE_READWRITE );
			if (sNew != NULL)
			{
				if (!IsBufferEmpty())
				{
					memcpy(sNew,
						m_pData,
						lenNew > m_iSize ? m_iSize * sizeof(T) : lenNew * sizeof(T));
					//					::VirtualFree( m_pData, 0, MEM_RELEASE );
					free(m_pData);
					m_pData = NULL;
				}
				m_pData = sNew;
				m_iSize = lenNew;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			if (!IsBufferEmpty())
			{
				//				::VirtualFree( m_pData, 0, MEM_RELEASE );
				free(m_pData);
				m_pData = NULL;
			}
			m_iSize = 0;
		}
		return TRUE;
	}

private:
	T* m_pData;
	int m_iSize;
};

class CSimpleString
{
public:
	CSimpleString()
		: m_iLen(0)
	{
	}

	const wchar_t* GetString()
	{
		return (m_iLen == 0 || m_sData.IsBufferEmpty()) ? L"" : m_sData.GetBuffer();
	}

	wchar_t& operator [] (int nItem)
	{
		return m_sData[nItem];
	}

	int GetLenght()
	{
		return m_iLen;
	}

	void Empty()
	{
		m_sData.SetLength(0);
		m_iLen = 0;
	}

	void Append(const wchar_t* str, int len = -1)
	{
		if (str != NULL)
		{
			if (len == -1) len = lstrlen(str);
			int newLength = m_iLen + len;
			if (m_sData.SetLength(newLength + 1))
			{
				m_sData[m_iLen] = '\0';
				lstrcpynW(&m_sData[m_iLen], str, len + 1);
				m_iLen = newLength;
			}
		}
	}

private:
	CMemBuffer< wchar_t, 128 > m_sData;
	int m_iLen;
};

class CPath
{
public:
	CPath(const wchar_t* lpszFileName)
	{
		if (lpszFileName != NULL)
		{
			// сохраняем оригинал
			m_sPathOriginal.Append(lpszFileName);

			if (::PathIsURL(lpszFileName) == TRUE)
			{
				m_sPath.Append(lpszFileName);
			}
			else // делаем преобразования
			{
				// 1. Раскрываем переменные окружения
				CMemBuffer< wchar_t, 1024 > sExpanded;
				::ExpandEnvironmentStrings(lpszFileName, sExpanded.GetBuffer(), 1024);
				// 2. Убираем в пути .. и . (приводим к каноническому виду)
				CMemBuffer< wchar_t, 1024 > sCanonical;
				::PathCanonicalize(sCanonical.GetBuffer(), sExpanded.GetBuffer());
				// 3. Убираем лишние пробелы
				::PathRemoveBlanks(sCanonical.GetBuffer());
				// 4. Проверяем существует ли преобразованный путь
				if (::PathFileExists(sCanonical.GetBuffer()) == TRUE)
				{
					::PathMakePretty(sCanonical.GetBuffer());
					::PathRemoveBackslash(sCanonical.GetBuffer());
					m_sPath.Append(sCanonical.GetBuffer());
					if (::PathIsDirectory(sCanonical.GetBuffer()) == FALSE)
					{
						m_sFileName.Append(::PathFindFileName(sCanonical.GetBuffer()));
						::PathRemoveFileSpec(sCanonical.GetBuffer());
					}
					m_sPathDir.Append(sCanonical.GetBuffer());
				}
				else
				{
					// 5. Отделяем аргументы
					wchar_t* pArg = ::PathGetArgs(sCanonical.GetBuffer());
					m_sFileParams.Append(pArg);
					::PathRemoveArgs(sCanonical.GetBuffer());
					// 6. Делаем путь по красивше
					::PathUnquoteSpaces(sCanonical.GetBuffer());
					::PathRemoveBackslash(sCanonical.GetBuffer());
					::PathMakePretty(sCanonical.GetBuffer());
					// 7. Проверяем преобразованный путь это дирректория
					if (::PathIsDirectory(sCanonical.GetBuffer()) != FALSE)
					{
						m_sPath.Append(sCanonical.GetBuffer());
						m_sPathDir.Append(sCanonical.GetBuffer());
					}
					else
					{
						// 8. Добавляем расширение к файлу .exe, если нету
						::PathAddExtension(sCanonical.GetBuffer(), NULL);
						// 9. Проверяем есть ли такой файл
						if (::PathFileExists(sCanonical.GetBuffer()) == TRUE)
						{
							m_sPath.Append(sCanonical.GetBuffer());
							m_sFileName.Append(::PathFindFileName(sCanonical.GetBuffer()));
							::PathRemoveFileSpec(sCanonical.GetBuffer());
							m_sPathDir.Append(sCanonical.GetBuffer());
						}
						else
						{
							// 10. Производим поиск
							::PathFindOnPath(sCanonical.GetBuffer(), NULL);
							::PathMakePretty(sCanonical.GetBuffer());
							m_sPath.Append(sCanonical.GetBuffer());
							if (::PathFileExists(sCanonical.GetBuffer()) == TRUE)
							{
								m_sFileName.Append(::PathFindFileName(sCanonical.GetBuffer()));
								::PathRemoveFileSpec(sCanonical.GetBuffer());
								m_sPathDir.Append(sCanonical.GetBuffer());
							}
						}
					}
				}
			}
		}
	}

	const wchar_t* GetPath()
	{
		return m_sPath.GetLenght() > 0 ? m_sPath.GetString() : NULL;
	}

	const wchar_t* GetDirectory()
	{
		return m_sPathDir.GetLenght() > 0 ? m_sPathDir.GetString() : NULL;
	}

	const wchar_t* GetFileParams()
	{
		return m_sFileParams.GetLenght() > 0 ? m_sFileParams.GetString() : NULL;
	}

private:
	CSimpleString m_sPathOriginal;
	CSimpleString m_sPath;
	CSimpleString m_sPathDir;
	CSimpleString m_sFileName;
	CSimpleString m_sFileParams;

public:
	static DWORD GetFileAttributes(const wchar_t* lpszFileName)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (::GetFileAttributesEx(lpszFileName, GetFileExInfoStandard, &fad) == FALSE)
		{
			return ((DWORD)-1); //INVALID_FILE_ATTRIBUTES;
		}
		return fad.dwFileAttributes;
	}
	static BOOL SetFileAttributes(const wchar_t* lpszFileName, DWORD dwFileAttributes)
	{
		return ::SetFileAttributes(lpszFileName, dwFileAttributes);
	}
	static BOOL IsDirectory(const wchar_t* lpszFileName)
	{
		return ::PathIsDirectory(lpszFileName) != FALSE;
	}
	static BOOL IsFileExists(const wchar_t* lpszFileName)
	{
		return IsPathExist(lpszFileName) == TRUE &&
			IsDirectory(lpszFileName) == FALSE;
	}
	static BOOL IsPathExist(const wchar_t* lpszFileName)
	{
		return ::PathFileExists(lpszFileName) != FALSE;
	}
};

// получить последнее сообщение об ошибке
static gui_string GetLastErrorString(DWORD* lastErrorCode, int* iLenMsg)
{
	gui_string res(L"");
	LPWSTR lpMsgBuf = NULL;
	*lastErrorCode = ::GetLastError();
	::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		*lastErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&lpMsgBuf,
		0,
		NULL);

	*iLenMsg = lstrlen(lpMsgBuf);

	// trim right
	while (*iLenMsg > 0)
	{
		(*iLenMsg)--;
		if (lpMsgBuf[*iLenMsg] == '\n' ||
			lpMsgBuf[*iLenMsg] == '\r' ||
			lpMsgBuf[*iLenMsg] == '.' ||
			lpMsgBuf[*iLenMsg] == ' ')
		{
			lpMsgBuf[*iLenMsg] = 0;
		}
		else
		{
			break;
		}
	}
	(*iLenMsg)++;
	res += lpMsgBuf;
	LocalFree(lpMsgBuf);
	return res;
}

static void lua_pushlasterr(lua_State* L, const wchar_t* lpszFunction)
{
	DWORD dw;
	int iLenMsg = 0;
	const gui_string lpMsgBuf = GetLastErrorString(&dw, &iLenMsg);

	if (lpszFunction == NULL)
	{
		lua_pushstring(L, UTF8FromString(lpMsgBuf).c_str());
	}
	else
	{
		UINT uBytes = (iLenMsg + lstrlen(lpszFunction) + 40) * sizeof(char);
		wchar_t* lpDisplayBuf = new wchar_t[uBytes] {};
		swprintf_s(lpDisplayBuf, uBytes, L"%s failed with error %lu: %s", lpszFunction, dw, lpMsgBuf.c_str());
		lua_pushstring(L, UTF8FromString(lpDisplayBuf).c_str());
		delete[] lpDisplayBuf;
	}
}

static int msgbox(lua_State* L)
{
	auto text = StringFromUTF8(luaL_checkstring(L, 1));
	auto title = StringFromUTF8(lua_tostring(L, 2));
	int options = (int)lua_tonumber(L, 3) | MB_TASKMODAL;
	int retCode = ::MessageBox(NULL, text.data(), title.empty() ? L"SciTE" : title.data(), options);
	lua_pushinteger(L, retCode);
	return 1;
}

static int getfileattr(lua_State* L)
{
	auto FN = StringFromUTF8(luaL_checkstring(L, -1));
	lua_pushinteger(L, CPath::GetFileAttributes(FN.data()));
	return 1;
}

static int setfileattr(lua_State* L)
{
	auto FN = StringFromUTF8(luaL_checkstring(L, -2));
	auto attr = luaL_checkinteger(L, -1); //luaL_checkint
	lua_pushboolean(L, CPath::SetFileAttributes(FN.data(), (DWORD)attr));
	return 1;
}

static int fileexists(lua_State* L)
{
	auto FN = StringFromUTF8(luaL_checkstring(L, 1));
	lua_pushboolean(L, CPath::IsPathExist(FN.data()));
	return 1;
}


struct W2MB
{
	W2MB(const wchar_t* src, int cp)
		: buffer(nullptr)
	{
		int len = ::WideCharToMultiByte(cp, 0, src, -1, 0, 0, 0, 0);
		if (len)
		{
			buffer = new char[len];
			::WideCharToMultiByte(cp, 0, src, -1, buffer, len, 0, 0);
		}
	}
	W2MB(W2MB&) = delete;
	W2MB& operator=(W2MB&) = delete;
	~W2MB()
	{
		delete[] buffer;
	}
	const char* c_str() const
	{
		return buffer;
	}
private:
	char* buffer;
};

struct MB2W
{
	MB2W(const char* src, int cp)
		: buffer(nullptr)
	{
		int len = ::MultiByteToWideChar(cp, 0, src, -1, 0, 0);
		if (len)
		{
			buffer = new wchar_t[len];
			::MultiByteToWideChar(cp, 0, src, -1, buffer, len);
		}
	}
	MB2W(MB2W&) = delete;
	MB2W& operator=(MB2W&) = delete;
	~MB2W()
	{
		delete[] buffer;
	}
	const wchar_t* c_str() const
	{
		return buffer;
	}
private:
	wchar_t* buffer;
};

static int internalConv(lua_State* L, bool toUTF8)
{
	bool success = false;
	if (lua_isstring(L, 1))
	{
		size_t len;
		const char* src = lua_tolstring(L, 1, &len);
		int cp = (int)luaL_optinteger(L, 2, CP_ACP);
		MB2W wc(src, toUTF8 ? cp : CP_UTF8);
		if (wc.c_str())
		{
			W2MB nc(wc.c_str(), toUTF8 ? CP_UTF8 : cp);
			if (nc.c_str())
			{
				lua_pushstring(L, nc.c_str());
				success = true;
			}
		}
	}
	if (!success)
		lua_pushnil(L);
	return 1;
}

static int to_utf8(lua_State* L)
{
	return internalConv(L, true);
}

static int from_utf8(lua_State* L)
{
	return internalConv(L, false);
}

// запустить через CreateProcess в скрытом режиме
static bool RunProcessHide(CPath& path, DWORD* out_exitcode, CSimpleString* strOut)
{
	static const int MAX_CMD = 1024;

	STARTUPINFOW si{};
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	// устанавливаем именованные каналы на потоки ввода/вывода
	BOOL bUsePipes = FALSE;
	HANDLE FWritePipe = NULL;
	HANDLE FReadPipe = NULL;
	SECURITY_ATTRIBUTES pa = { sizeof(pa), NULL, TRUE };
	bUsePipes = ::CreatePipe(&FReadPipe, &FWritePipe, &pa, 0);
	if (bUsePipes != FALSE)
	{
		si.hStdOutput = FWritePipe;
		si.hStdInput = FReadPipe;
		si.hStdError = FWritePipe;
		si.dwFlags = STARTF_USESTDHANDLES | si.dwFlags;
	}

	// запускаем процесс
	CMemBuffer< wchar_t, MAX_CMD > bufCmdLine; // строковой буфер длиной MAX_CMD
	bufCmdLine.GetBuffer()[0] = 0;
	wcscat_s(bufCmdLine.GetBuffer(), MAX_CMD, L"\"");
	wcscat_s(bufCmdLine.GetBuffer(), MAX_CMD, path.GetPath());
	wcscat_s(bufCmdLine.GetBuffer(), MAX_CMD, L"\"");
	if (path.GetFileParams() != NULL)
	{
		wcscat_s(bufCmdLine.GetBuffer(), MAX_CMD, L" ");
		wcscat_s(bufCmdLine.GetBuffer(), MAX_CMD, path.GetFileParams());
	}

	PROCESS_INFORMATION pi = { };
	BOOL RetCode = ::CreateProcess(NULL, // не используем имя файла, все в строке запуска
		bufCmdLine.GetBuffer(), // строка запуска
		NULL, // Process handle not inheritable
		NULL, // Thread handle not inheritable
		TRUE, // Set handle inheritance to FALSE
		0, // No creation flags
		NULL, // Use parent's environment block
		NULL, //path.GetDirectory(), // устанавливаем дирректорию запуска
		&si, // STARTUPINFO
		&pi); // PROCESS_INFORMATION

	// если провалили запуск сообщаем об ошибке
	if (RetCode == FALSE)
	{
		::CloseHandle(FReadPipe);
		::CloseHandle(FWritePipe);
		return FALSE;
	}

	// закрываем описатель потока, в нем нет необходимости
	::CloseHandle(pi.hThread);

	// ожидаем завершение работы процесса
	try
	{
		DWORD BytesToRead = 0;
		DWORD BytesRead = 0;
		DWORD TotalBytesAvail = 0;
		DWORD PipeReaded = 0;
		DWORD exit_code = 0;
		CMemBuffer< char, MAX_CMD > bufStr; // строковой буфер длиной MAX_CMD
		while (::PeekNamedPipe(FReadPipe, NULL, 0, &BytesRead, &TotalBytesAvail, NULL))
		{
			if (TotalBytesAvail == 0)
			{
				if (::GetExitCodeProcess(pi.hProcess, &exit_code) == FALSE ||
					exit_code != STILL_ACTIVE)
				{
					break;
				}
				else
				{
					continue;
				}
			}
			else
			{
				while (TotalBytesAvail > BytesRead)
				{
					if (TotalBytesAvail - BytesRead > MAX_CMD - 1)
					{
						BytesToRead = MAX_CMD - 1;
					}
					else
					{
						BytesToRead = TotalBytesAvail - BytesRead;
					}
					if (::ReadFile(FReadPipe,
						bufStr.GetBuffer(),
						BytesToRead,
						&PipeReaded,
						NULL) == FALSE)
					{
						break;
					}
					if (PipeReaded <= 0) continue;
					BytesRead += PipeReaded;
					bufStr[PipeReaded] = '\0';
					MB2W wc(bufStr.GetBuffer(), 866);
					strOut->Append(wc.c_str());
				}
			}
		}
	}
	catch (...)
	{
	}

	// Код завершения процесса
	::GetExitCodeProcess(pi.hProcess, out_exitcode);
	::CloseHandle(pi.hProcess);
	::CloseHandle(FReadPipe);
	::CloseHandle(FWritePipe);
	return TRUE;
}

// запустить через ShellExecuteEx в скрытом режиме
// (см. шаманство с консолью)
static bool ExecuteHide(CPath& path, DWORD* out_exitcode, CSimpleString* strOut)
{
	HANDLE hSaveStdin = NULL;
	HANDLE hSaveStdout = NULL;
	HANDLE hChildStdoutRdDup = NULL;
	HANDLE hChildStdoutWr = NULL;
	try
	{
		// подключаем консоль
		STARTUPINFO si{};
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi = { };
		wchar_t command_line[] = L"cmd";
		::CreateProcess(NULL, // не используем имя файла, все в строке запуска
			command_line, // Command line
			NULL, // Process handle not inheritable
			NULL, // Thread handle not inheritable
			TRUE, // Set handle inheritance to FALSE
			0, // No creation flags
			NULL, // Use parent's environment block
			NULL, // Use parent's starting directory
			&si, // STARTUPINFO
			&pi); // PROCESS_INFORMATION
		// задержка чтобы консоль успела создаться
		::WaitForSingleObject(pi.hProcess, 100);
		BOOL hResult = FALSE;
		HMODULE hLib = LoadLibrary(L"Kernel32.dll");
		if (hLib != NULL)
		{
			typedef BOOL(STDAPICALLTYPE* ATTACHCONSOLE)(DWORD dwProcessId);
			ATTACHCONSOLE _AttachConsole = NULL;
			_AttachConsole = (ATTACHCONSOLE)GetProcAddress(hLib, "AttachConsole");
			if (_AttachConsole) hResult = _AttachConsole(pi.dwProcessId);
			FreeLibrary(hLib);
		}
		if (hResult == FALSE) AllocConsole();

		TerminateProcess(pi.hProcess, 0);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		HANDLE hChildStdinRd;
		HANDLE hChildStdinWr;
		HANDLE hChildStdinWrDup;
		HANDLE hChildStdoutRd;

		// Set the bInheritHandle flag so pipe handles are inherited.
		SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
		BOOL fSuccess;

		// The steps for redirecting child process's STDOUT:
		//     1. Save current STDOUT, to be restored later.
		//     2. Create anonymous pipe to be STDOUT for child process.
		//     3. Set STDOUT of the parent process to be write handle to
		//        the pipe, so it is inherited by the child process.
		//     4. Create a noninheritable duplicate of the read handle and
		//        close the inheritable read handle.

		// Save the handle to the current STDOUT.
		hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);

		// Create a pipe for the child process's STDOUT.
		if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) throw(1);

		// Set a write handle to the pipe to be STDOUT.
		if (!SetStdHandle(STD_OUTPUT_HANDLE, hChildStdoutWr)) throw(1);

		// Create noninheritable read handle and close the inheritable read
		// handle.
		fSuccess = DuplicateHandle(GetCurrentProcess(),
			hChildStdoutRd,
			GetCurrentProcess(),
			&hChildStdoutRdDup,
			0,
			FALSE,
			DUPLICATE_SAME_ACCESS);
		if (fSuccess == FALSE) throw(1);
		CloseHandle(hChildStdoutRd);

		// The steps for redirecting child process's STDIN:
		//     1.  Save current STDIN, to be restored later.
		//     2.  Create anonymous pipe to be STDIN for child process.
		//     3.  Set STDIN of the parent to be the read handle to the
		//         pipe, so it is inherited by the child process.
		//     4.  Create a noninheritable duplicate of the write handle,
		//         and close the inheritable write handle.

		// Save the handle to the current STDIN.
		hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);

		// Create a pipe for the child process's STDIN.
		if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) throw(1);

		// Set a read handle to the pipe to be STDIN.
		if (!SetStdHandle(STD_INPUT_HANDLE, hChildStdinRd)) throw(1);

		// Duplicate the write handle to the pipe so it is not inherited.
		fSuccess = DuplicateHandle(GetCurrentProcess(),
			hChildStdinWr,
			GetCurrentProcess(),
			&hChildStdinWrDup,
			0,
			FALSE,
			DUPLICATE_SAME_ACCESS);
		if (fSuccess == FALSE) throw(1);

		CloseHandle(hChildStdinWr);
	}
	catch (...)
	{
		return FALSE;
	}

	// Now create the child process.
	SHELLEXECUTEINFOW shinf{};
	shinf.cbSize = sizeof(SHELLEXECUTEINFOW);
	shinf.lpFile = path.GetPath();
	shinf.lpParameters = path.GetFileParams();
	//shinf.lpDirectory = path.GetDirectory();
	shinf.fMask = SEE_MASK_FLAG_NO_UI |
		SEE_MASK_NO_CONSOLE |
		SEE_MASK_FLAG_DDEWAIT |
		SEE_MASK_NOCLOSEPROCESS;
	shinf.nShow = SW_HIDE;
	BOOL bSuccess = ::ShellExecuteEx(&shinf);
	if (bSuccess && shinf.hInstApp <= (HINSTANCE)32) bSuccess = FALSE;
	HANDLE hProcess = shinf.hProcess;

	try
	{
		if (bSuccess == FALSE || hProcess == NULL) throw(1);

		if (hChildStdoutWr != NULL)
		{
			CloseHandle(hChildStdoutWr);
			hChildStdoutWr = NULL;
		}

		// After process creation, restore the saved STDIN and STDOUT.
		if (hSaveStdin != NULL)
		{
			if (!SetStdHandle(STD_INPUT_HANDLE, hSaveStdin)) throw(1);
			CloseHandle(hSaveStdin);
			hSaveStdin = NULL;
		}

		if (hSaveStdout != NULL)
		{
			if (!SetStdHandle(STD_OUTPUT_HANDLE, hSaveStdout)) throw(1);
			CloseHandle(hSaveStdout);
			hSaveStdout = NULL;
		}

		if (hChildStdoutRdDup != NULL)
		{
			// Read output from the child process, and write to parent's STDOUT.
			const int BUFSIZE = 1024;
			DWORD dwRead;
			CMemBuffer< wchar_t, BUFSIZE > bufStr; // строковой буфер
			CMemBuffer< wchar_t, BUFSIZE > bufCmdLine; // строковой буфер
			for (;;)
			{
				if (ReadFile(hChildStdoutRdDup,
					bufCmdLine.GetBuffer(),
					BUFSIZE,
					&dwRead,
					NULL) == FALSE ||
					dwRead == 0)
				{
					DWORD exit_code = 0;
					if (::GetExitCodeProcess(hProcess, &exit_code) == FALSE ||
						exit_code != STILL_ACTIVE)
					{
						break;
					}
					else
					{
						continue;
					}
				}
				bufCmdLine[dwRead] = '\0';
				//::OemToAnsi( bufCmdLine.GetBuffer(), bufStr.GetBuffer() );
				strOut->Append(bufCmdLine.GetBuffer() /*bufStr.GetBuffer()*/);
			}
			CloseHandle(hChildStdoutRdDup);
			hChildStdoutRdDup = NULL;
		}
		FreeConsole();
	}
	catch (...)
	{
		if (hChildStdoutWr != NULL) CloseHandle(hChildStdoutWr);
		if (hSaveStdin != NULL) CloseHandle(hSaveStdin);
		if (hSaveStdout != NULL) CloseHandle(hSaveStdout);
		if (hChildStdoutRdDup != NULL) CloseHandle(hChildStdoutRdDup);
		if (bSuccess == FALSE || hProcess == NULL) return FALSE;
	}

	::GetExitCodeProcess(hProcess, out_exitcode);
	CloseHandle(hProcess);
	return TRUE;
}

static int exec(lua_State* L)
{
	// считываем запускаемую команду
	CPath file(StringFromUTF8(luaL_checkstring(L, 1)).data());
	auto verb = StringFromUTF8(lua_tostring(L, 2));
	int noshow = lua_toboolean(L, 3);
	int dowait = lua_toboolean(L, 4);

	BOOL useConsoleOut = dowait && noshow && (verb.empty());

	DWORD exit_code = (DWORD)-1;
	BOOL bSuccess = FALSE;
	CSimpleString strOut;

	if (useConsoleOut != FALSE)
	{
		bSuccess = RunProcessHide(file, &exit_code, &strOut) ||
			ExecuteHide(file, &exit_code, &strOut);
	}
	else
	{
		HANDLE hProcess = NULL;
		// запускаем процесс
		if (verb.size() && // если есть команда запуска
			wcscmp(verb.data(), L"explore") == 0 && // если команда запуска explore
			CPath::IsFileExists(file.GetPath())) // проверяем файл ли это
		{
			SHELLEXECUTEINFOW shinf = { }; shinf.cbSize = sizeof(SHELLEXECUTEINFOW);
			shinf.lpFile = L"explorer.exe";
			CSimpleString sFileParams;
			sFileParams.Append(L"/e, /select,");
			sFileParams.Append(file.GetPath());
			shinf.lpParameters = sFileParams.GetString();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
				SEE_MASK_NO_CONSOLE |
				SEE_MASK_FLAG_DDEWAIT |
				SEE_MASK_NOCLOSEPROCESS;
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx(&shinf);
			if (bSuccess && shinf.hInstApp <= (HINSTANCE)32) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}
		else if (verb.size() && // если есть команда запуска
			wcscmp(verb.data(), L"select") == 0 && // если команда запуска select
			CPath::IsPathExist(file.GetPath())) // проверяем правильный путь
		{
			SHELLEXECUTEINFOW shinf = { }; shinf.cbSize = sizeof(SHELLEXECUTEINFOW);
			shinf.lpFile = L"explorer.exe";
			CSimpleString sFileParams;
			sFileParams.Append(L"/select,");
			sFileParams.Append(file.GetPath());
			shinf.lpParameters = sFileParams.GetString();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
				SEE_MASK_NO_CONSOLE |
				SEE_MASK_FLAG_DDEWAIT |
				SEE_MASK_NOCLOSEPROCESS;
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx(&shinf);
			if (bSuccess && shinf.hInstApp <= (HINSTANCE)32) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}
		else
		{
			SHELLEXECUTEINFOW shinf = {  }; shinf.cbSize = sizeof(SHELLEXECUTEINFOW);
			shinf.lpFile = file.GetPath();
			shinf.lpParameters = file.GetFileParams();
			shinf.lpVerb = verb.data();
			//shinf.lpDirectory = file.GetDirectory();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
				SEE_MASK_NO_CONSOLE |
				SEE_MASK_FLAG_DDEWAIT;
			if (verb.size())
			{
				shinf.fMask |= SEE_MASK_NOCLOSEPROCESS;
			}
			else
			{
				shinf.fMask |= SEE_MASK_INVOKEIDLIST;
			}
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx(&shinf);
			if (bSuccess && shinf.hInstApp <= (HINSTANCE)32) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}

		if (dowait != FALSE && hProcess != NULL)
		{
			// ждем пока процесс не завершится
			::WaitForSingleObject(hProcess, INFINITE);
		}

		if (hProcess != NULL)
		{
			if (dowait != FALSE) ::GetExitCodeProcess(hProcess, &exit_code);
			CloseHandle(hProcess);
		}

		if (bSuccess != FALSE)
		{
			::SetLastError(0);
			DWORD dw;
			int len;
			gui_string lpMsgBuf = GetLastErrorString(&dw, &len);
			strOut.Append(lpMsgBuf.c_str());
		}
	}

	if (bSuccess == FALSE)
	{
		lua_pushboolean(L, FALSE);
		lua_pushlasterr(L, NULL);
	}
	else
	{
		(exit_code != (DWORD)-1) ? lua_pushnumber(L, exit_code) : lua_pushboolean(L, TRUE);
		lua_pushstring(L, UTF8FromString(strOut.GetString()).data());
	}

	return 2;
}

static int getclipboardtext(lua_State* L)
{
	//CSimpleString clipText;
	gui_string tmp;
	if (::IsClipboardFormatAvailable(CF_UNICODETEXT))
	{
		if (::OpenClipboard(NULL))
		{
			HANDLE hData = ::GetClipboardData(CF_UNICODETEXT);
			if (hData)
			{
				//clipText.Append((wchar_t*)::GlobalLock(hData));
				tmp.append((wchar_t*)::GlobalLock(hData));
				::GlobalUnlock(hData);
			}
			::CloseClipboard();
		}
	}
	//lua_pushstring(L, UTF8FromString(clipText.GetString()).c_str());
	lua_pushstring(L, UTF8FromString(tmp).c_str());
	return 1;
}

static void pushFFTime(lua_State* L, FILETIME* ft)
{
	SYSTEMTIME st;
	FileTimeToSystemTime(ft, &st);
	lua_newtable(L);
	lua_pushstring(L, "year");
	lua_pushnumber(L, st.wYear);
	lua_rawset(L, -3);
	lua_pushstring(L, "month");
	lua_pushnumber(L, st.wMonth);
	lua_rawset(L, -3);
	lua_pushstring(L, "dayofweek");
	lua_pushnumber(L, st.wDayOfWeek);
	lua_rawset(L, -3);
	lua_pushstring(L, "day");
	lua_pushnumber(L, st.wDay);
	lua_rawset(L, -3);
	lua_pushstring(L, "hour");
	lua_pushnumber(L, st.wHour);
	lua_rawset(L, -3);
	lua_pushstring(L, "min");
	lua_pushnumber(L, st.wMinute);
	lua_rawset(L, -3);
	lua_pushstring(L, "sec");
	lua_pushnumber(L, st.wSecond);
	lua_rawset(L, -3);
	lua_pushstring(L, "msec");
	lua_pushnumber(L, st.wMilliseconds);
	lua_rawset(L, -3);
}

static int findfiles(lua_State* L)
{
	auto filename = StringFromUTF8(luaL_checkstring(L, 1));

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = ::FindFirstFile(filename.data(), &findFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		// create table for result
		lua_createtable(L, 1, 0);

		lua_Integer num = 1;
		BOOL isFound = TRUE;
		while (isFound != FALSE)
		{
			// store file info
			lua_pushinteger(L, num);
			lua_createtable(L, 0, 7);

			lua_pushstring(L, UTF8FromString(findFileData.cFileName).data());
			lua_setfield(L, -2, "name");

			lua_pushboolean(L, findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			lua_setfield(L, -2, "isdirectory");

			lua_pushnumber(L, findFileData.dwFileAttributes);
			lua_setfield(L, -2, "attributes");

			lua_pushnumber(L, findFileData.nFileSizeHigh * ((lua_Number)MAXDWORD + 1) +
				findFileData.nFileSizeLow);
			lua_setfield(L, -2, "size");

			pushFFTime(L, &(findFileData.ftCreationTime));
			lua_setfield(L, -2, "creationtime");

			pushFFTime(L, &(findFileData.ftLastAccessTime));
			lua_setfield(L, -2, "lastaccesstime");

			pushFFTime(L, &(findFileData.ftLastWriteTime));
			lua_setfield(L, -2, "lastwritetime");

			lua_settable(L, -3);
			num++;

			// next
			isFound = ::FindNextFile(hFind, &findFileData);
		}

		::FindClose(hFind);

		return 1;
	}

	// files not found
	return 0;
}

extern int showinputbox(lua_State*);

#pragma warning(pop)

int do_md5(lua_State* L)
{
	MD5 md5;
	if (lua_toboolean(L, 2))
	{
		auto FN = StringFromUTF8(luaL_checkstring(L, 1));
		if( CPath::IsPathExist(FN.c_str()) )
		{
			lua_pushstring(L, md5.digestFile(lua_tostring(L, 1)));
			return 1;
		}
	}
	lua_pushstring(L, md5.digestString(lua_tostring(L, 1)));
	return 1;
}

int do_sha256(lua_State* L)
{
	std::string content = lua_tostring(L, 1);
	uint8_t sha2hash[32]{};
	calc_sha_256(sha2hash, reinterpret_cast<const uint8_t*>(content.c_str()), content.length());

	wchar_t sha2hashStr[65] = { '\0' };
	for (size_t i = 0; i < 32; i++)
		wsprintf(sha2hashStr + i * 2, L"%02x", sha2hash[i]);

	std::string res = UTF8FromString(sha2hashStr);
	lua_pushstring(L, res.c_str());
	return 1;
}

static const struct luaL_Reg shell[] =
{
	{ "exec", exec },
	{ "msgbox", msgbox },
	{ "getfileattr", getfileattr },
	{ "setfileattr", setfileattr },
	{ "fileexists", fileexists },
	{ "getclipboardtext", getclipboardtext },
	{ "findfiles", findfiles },
	{ "inputbox", showinputbox },
	{ "to_utf8", to_utf8 },
	{ "from_utf8", from_utf8 },
	{ "calc_md5", do_md5},
	{ "calc_sha256", do_sha256},
	{ NULL, NULL }
};

extern "C" __declspec(dllexport)
int luaopen_shell(lua_State * L)
{
#if LUA_VERSION_NUM < 502
	luaL_register(L, "shell", shell); //Lua5.1
#else
	luaL_newlib(L, shell); //Lua5.2+
#endif
	lua_pushvalue(L, -1);  /* copy of module */
	lua_setglobal(L, "shell");
	return 1;
}
