/* CEF runtime path / stamp / tree helpers. */
#include "CefRuntime.h"
#include "CefRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace CefRuntimeDetail
{
void Status(void (*fn)(const char*), const char* msg)
{
	if (fn && msg)
		fn(msg);
}

std::wstring Join(const std::wstring& a, const wchar_t* b)
{
	if (a.empty())
		return b ? b : L"";
	if (!b || !b[0])
		return a;
	if (a.back() == L'\\' || a.back() == L'/')
		return a + b;
	return a + L"\\" + b;
}

std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (n <= 0)
		return {};
	std::string out(static_cast<size_t>(n - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
	return out;
}

bool ReadStamp(const std::wstring& verPath, std::string& out)
{
	out.clear();
	HANDLE h = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	char buf[64]{};
	DWORD got = 0;
	const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
	CloseHandle(h);
	if (!ok || got == 0)
		return false;
	out.assign(buf, got);
	while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
		out.pop_back();
	return true;
}

bool WriteStamp(const std::wstring& verPath, const char* stamp)
{
	HANDLE h = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, stamp, static_cast<DWORD>(std::strlen(stamp)), &written, nullptr);
	CloseHandle(h);
	return ok && written == std::strlen(stamp);
}

bool MkDirDeep(const std::wstring& path)
{
	if (path.empty())
		return false;
	if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		return true;
	const size_t slash = path.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
	{
		if (!MkDirDeep(path.substr(0, slash)))
			return false;
	}
	return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

void RemoveTree(const std::wstring& path)
{
	WIN32_FIND_DATAW fd{};
	const std::wstring glob = Join(path, L"*");
	HANDLE h = FindFirstFileW(glob.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		RemoveDirectoryW(path.c_str());
		DeleteFileW(path.c_str());
		return;
	}
	do
	{
		if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
			continue;
		const std::wstring child = Join(path, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			RemoveTree(child);
		else
			DeleteFileW(child.c_str());
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	RemoveDirectoryW(path.c_str());
}

bool TreeLooksComplete(const std::wstring& cefDir)
{
	if (GetFileAttributesW(Join(cefDir, L"libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	if (GetFileAttributesW(Join(cefDir, L"chrome_elf.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	if (GetFileAttributesW(Join(cefDir, L"icudtl.dat").c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	if (GetFileAttributesW(Join(cefDir, L"resources.pak").c_str()) == INVALID_FILE_ATTRIBUTES &&
		GetFileAttributesW(Join(cefDir, L"chrome_100_percent.pak").c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	if (GetFileAttributesW(Join(cefDir, L"locales").c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;
	return true;
}

bool AlreadyInstalled(const std::wstring& cefDir)
{
	if (!TreeLooksComplete(cefDir))
		return false;
	std::string stamp;
	if (!ReadStamp(Join(cefDir, L"cef.ver"), stamp))
		return false;
	return stamp == CefRuntime::kStamp;
}

std::wstring DllDir()
{
	HMODULE mod = nullptr;
	if (!GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&CefRuntime::EnsureInstalled), &mod) ||
		!mod)
		return {};
	wchar_t path[MAX_PATH]{};
	if (!GetModuleFileNameW(mod, path, MAX_PATH))
		return {};
	std::wstring full = path;
	const size_t slash = full.find_last_of(L"\\/");
	if (slash == std::wstring::npos)
		return {};
	return full.substr(0, slash);
}

void CleanupStrayZipInCef(const std::wstring& cefDir)
{
	DeleteFileW(Join(cefDir, L"cef-runtime-150-windows64.zip").c_str());
	DeleteFileW(Join(cefDir, L"cef-download.tmp").c_str());
}
}
