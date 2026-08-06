#include "LivePanelsInternal.h"

#include "AddonPaths.h"

#include <string>

#include <windows.h>

namespace LivePanelsDetail
{
std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
	if (n > 0)
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
	return out;
}

std::string PathToFileUrl(const std::wstring& path)
{
	std::string utf8 = WideToUtf8(path);
	for (char& c : utf8)
	{
		if (c == '\\')
			c = '/';
	}
	if (utf8.size() >= 2 && utf8[1] == ':')
		return std::string("file:///") + utf8;
	return std::string("file://") + utf8;
}

std::wstring StemPath(const std::wstring& addonDir, const char* stem, const wchar_t* ext)
{
	const std::wstring base = (ext && wcscmp(ext, L".json") == 0)
		? AddonPaths::EnsureUnder(addonDir, L"live\\cache")
		: AddonPaths::EnsureUnder(addonDir, L"pages");
	std::wstring p = base + L"\\";
	for (const char* s = stem; *s; ++s)
		p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s)));
	p += ext;
	return p;
}

bool FileFresh(const std::wstring& path, DWORD ttlSec)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	FILETIME ft{};
	const BOOL ok = GetFileTime(h, nullptr, nullptr, &ft);
	CloseHandle(h);
	if (!ok)
		return false;
	ULARGE_INTEGER u{};
	u.LowPart = ft.dwLowDateTime;
	u.HighPart = ft.dwHighDateTime;
	FILETIME nowFt{};
	GetSystemTimeAsFileTime(&nowFt);
	ULARGE_INTEGER n{};
	n.LowPart = nowFt.dwLowDateTime;
	n.HighPart = nowFt.dwHighDateTime;
	const ULONGLONG age100ns = (n.QuadPart > u.QuadPart) ? (n.QuadPart - u.QuadPart) : 0;
	const ULONGLONG ageSec = age100ns / 10000000ull;
	return ageSec <= ttlSec;
}

bool WriteUtf8File(const std::wstring& path, const std::string& data)
{
	/* Write via .tmp then replace — CEF often holds the live HTML open; exclusive
	   CREATE_ALWAYS on the real path fails under Wine and surfaces as
	   "Failed to write Live panel HTML". */
	const std::wstring tmp = path + L".tmp";
	HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	FlushFileBuffers(h);
	CloseHandle(h);
	if (!ok || written != data.size())
	{
		DeleteFileW(tmp.c_str());
		return false;
	}
	if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return true;
	/* Fallback: try direct open with share. */
	DeleteFileW(tmp.c_str());
	h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	written = 0;
	const BOOL ok2 = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	CloseHandle(h);
	return ok2 && written == data.size();
}

std::string ReadUtf8File(const std::wstring& path)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return {};
	LARGE_INTEGER sz{};
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 12 * 1024 * 1024)
	{
		CloseHandle(h);
		return {};
	}
	std::string out(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD read = 0;
	const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
	CloseHandle(h);
	if (!ok)
		return {};
	out.resize(read);
	return out;
}

} // namespace LivePanelsDetail
