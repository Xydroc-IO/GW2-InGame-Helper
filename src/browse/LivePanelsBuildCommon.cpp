#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
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
	std::wstring p = addonDir + L"\\";
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
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	CloseHandle(h);
	return ok && written == data.size();
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

bool TryCacheHit(const std::wstring& addonDir, const char* stem, DWORD ttlSec,
	Gw2Http::Result& out)
{
	const std::wstring path = StemPath(addonDir, stem, L".json");
	if (!FileFresh(path, ttlSec))
		return false;
	std::string cached = ReadUtf8File(path);
	if (cached.empty())
		return false;
	out.ok = true;
	out.status = 200;
	out.body = std::move(cached);
	out.error.clear();
	return true;
}

void StoreCache(const std::wstring& addonDir, const char* stem, const Gw2Http::Result& r)
{
	if (r.ok && r.body.size() > 2)
		WriteUtf8File(StemPath(addonDir, stem, L".json"), r.body);
}

void PreferStaleCache(const std::wstring& addonDir, const char* stem, Gw2Http::Result& r)
{
	if (r.ok && r.body.size() > 2)
		return;
	std::string stale = ReadUtf8File(StemPath(addonDir, stem, L".json"));
	if (stale.empty())
		return;
	r.ok = true;
	r.status = 200;
	r.body = std::move(stale);
	r.error.clear();
}

DWORD WINAPI ParallelApiProc(void* param)
{
	auto* j = static_cast<ParallelApiJob*>(param);
	if (j && j->out && j->path && j->path[0])
		*j->out = Gw2Http::Api(j->path, j->bearer, j->timeoutMs);
	return 0;
}

/* Fire independent GETs together — wall clock ≈ slowest call, not sum.
   Cap is small (≤8) so we stay far under the ~600/min API budget. */
void RunParallelApis(ParallelApiJob* jobs, size_t n)
{
	if (!jobs || n == 0)
		return;
	if (n == 1)
	{
		ParallelApiProc(&jobs[0]);
		return;
	}
	std::vector<HANDLE> hs;
	hs.reserve(n);
	for (size_t i = 0; i < n; ++i)
	{
		HANDLE h = CreateThread(nullptr, 0, ParallelApiProc, &jobs[i], 0, nullptr);
		if (h)
			hs.push_back(h);
		else
			ParallelApiProc(&jobs[i]);
	}
	if (!hs.empty())
	{
		/* Wait in chunks of 64 (WaitForMultipleObjects limit). */
		size_t off = 0;
		while (off < hs.size())
		{
			const DWORD chunk = static_cast<DWORD>(
				(hs.size() - off > 64) ? 64 : (hs.size() - off));
			WaitForMultipleObjects(chunk, hs.data() + off, TRUE, 60000);
			off += chunk;
		}
		for (HANDLE h : hs)
			CloseHandle(h);
	}
}

} // namespace LivePanelsBuild
