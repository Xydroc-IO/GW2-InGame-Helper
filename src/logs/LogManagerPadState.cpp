#include "LogManagerPad.h"

#include "LogManagerShared.h"
#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "AddonPaths.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace LogManagerDetail
{

	std::mutex gMu;
	std::vector<LogEntry> gLogs;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	std::vector<LogEntry> gDraw;

	std::atomic<bool> gScanBusy{false};
	std::atomic<bool> gParseBusy{false};
	std::atomic<bool> gUploadBusy{false};
	std::atomic<bool> gHydrateBusy{false};
	std::atomic<bool> gHydrateForce{false};
	std::atomic<bool> gEiInstallBusy{false};
	std::atomic<bool> gCancel{false};
	std::atomic<int> gParseDone{0};
	std::atomic<int> gParseTotal{0};
	std::atomic<int> gUploadDone{0};
	std::atomic<int> gUploadTotal{0};

	HANDLE gScanThread = nullptr;
	HANDLE gParseThread = nullptr;
	HANDLE gUploadThread = nullptr;
	HANDLE gHydrateThread = nullptr;
	HANDLE gEiInstallThread = nullptr;
	HANDLE gKillProofThread = nullptr;

	std::atomic<bool> gKillProofBusy{false};
	std::mutex gKpCacheMu;
	std::unordered_map<std::string, KillProofCacheEntry> gKpCache; /* lowercased account */
	std::vector<std::string> gKpQueue; /* accounts to fetch */
	std::atomic<bool> gKpForce{false};

	bool gFocus = false;
	bool gPlaceOnce = false;
	bool gExpandGroupsOnce = false; /* reopen all encounter sections (screenshot default) */
	char gStatus[256] = {};
	char gEiStatus[256] = {};
	char gSearch[96] = {};
	int gResultFilter = static_cast<int>(ResultFilter::All);
	int gModeFilter = static_cast<int>(ModeFilter::All);
	int gDaysCombo = 0; /* index into kDaysMap */
	int gSelected = -1;
	bool gFocusSetupTab = false;
	float gLogListFrac = kLogListFracDef; /* synced from G::LogManagerListFrac */

	std::vector<std::string> gUploadQueue; /* pathUtf8 */

	std::wstring Utf8ToWide(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring out(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
		return out;
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string out(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	bool EndsWithI(const std::wstring& s, const wchar_t* suf)
	{
		const size_t n = std::wcslen(suf);
		if (s.size() < n)
			return false;
		for (size_t i = 0; i < n; ++i)
		{
			wchar_t a = s[s.size() - n + i];
			wchar_t b = suf[i];
			if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
			if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
			if (a != b)
				return false;
		}
		return true;
	}

	std::wstring DefaultLogDirW()
	{
		wchar_t profile[MAX_PATH]{};
		const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
		if (n == 0 || n >= MAX_PATH)
			return {};
		return std::wstring(profile) + L"\\Documents\\Guild Wars 2\\addons\\arcdps\\arcdps.cbtlogs";
	}

	void EnsureDefaultPaths()
	{
		if (!G::LogFolder[0])
		{
			const std::string def = WideToUtf8(DefaultLogDirW());
			if (!def.empty())
				std::snprintf(G::LogFolder, sizeof(G::LogFolder), "%s", def.c_str());
		}
	}

	bool PathExistsUtf8(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return false;
		const std::wstring w = Utf8ToWide(utf8);
		if (w.empty())
			return false;
		return GetFileAttributesW(w.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool IsManagedEiPath(const char* path)
	{
		if (!path || !path[0])
			return false;
		const std::string data = AddonPaths::DataDirUtf8();
		if (data.empty())
			return false;
		std::string needle = data;
		for (char& c : needle)
			if (c == '/')
				c = '\\';
		std::string p = path;
		for (char& c : p)
			if (c == '/')
				c = '\\';
		/* case-insensitive contains "\ei\" under addon data dir */
		auto lower = [](std::string s) {
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		};
		const std::string pl = lower(p);
		const std::string dl = lower(needle);
		if (pl.find(dl) == std::string::npos)
			return false;
		return pl.find("\\ei\\") != std::string::npos ||
			pl.find("\\ei/") != std::string::npos;
	}

	bool ApplyManagedCliPath()
	{
		char cli[512]{};
		if (!EiRuntime::GetCliPathUtf8(AddonPaths::DataDir().c_str(), cli, sizeof(cli)))
			return false;
		if (!G::EliteInsightsPath[0] || IsManagedEiPath(G::EliteInsightsPath) ||
			!PathExistsUtf8(G::EliteInsightsPath))
		{
			std::snprintf(G::EliteInsightsPath, sizeof(G::EliteInsightsPath), "%s", cli);
			Settings::SetDirty();
		}
		return true;
	}

	time_t FileTimeToUnix(const FILETIME& ft)
	{
		ULARGE_INTEGER u;
		u.LowPart = ft.dwLowDateTime;
		u.HighPart = ft.dwHighDateTime;
		/* FILETIME is 100ns since 1601; Unix is seconds since 1970 */
		constexpr ULONGLONG kEpochDiff = 116444736000000000ULL;
		if (u.QuadPart < kEpochDiff)
			return 0;
		return static_cast<time_t>((u.QuadPart - kEpochDiff) / 10000000ULL);
	}

	std::string FmtDuration(long long ms)
	{
		if (ms <= 0)
			return "-";
		const long long totalSec = ms / 1000;
		const int h = static_cast<int>(totalSec / 3600);
		const int m = static_cast<int>((totalSec % 3600) / 60);
		const int s = static_cast<int>(totalSec % 60);
		char buf[32];
		if (h > 0)
			std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
		else
			std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);
		return buf;
	}

	std::string FmtTime(time_t t)
	{
		if (t <= 0)
			return "-";
		const std::tm* tm = std::localtime(&t);
		if (!tm)
			return "-";
		char buf[40];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
		return buf;
	}


	bool ContainsI(const std::string& hay, const char* needle)
	{
		if (!needle || !needle[0])
			return true;
		auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
		std::string h = hay;
		std::string n = needle;
		std::transform(h.begin(), h.end(), h.begin(), lower);
		std::transform(n.begin(), n.end(), n.begin(), lower);
		return h.find(n) != std::string::npos;
	}

	bool CopyText(const char* text)
	{
		if (!text || !text[0])
			return false;
		const size_t n = std::strlen(text) + 1;
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
		if (!mem)
		{
			CloseClipboard();
			return false;
		}
		void* p = GlobalLock(mem);
		if (!p)
		{
			GlobalFree(mem);
			CloseClipboard();
			return false;
		}
		std::memcpy(p, text, n);
		GlobalUnlock(mem);
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
		return true;
	}

	const char* ResultLabel(int r)
	{
		if (r == 1)
			return "Kill";
		if (r == 0)
			return "Fail";
		return "?";
	}

	void SyncDraw()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen)
			return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gLogs;
		gDrawnGen = gGen.load();
	}


	void OpenFolderFor(const std::wstring& path)
	{
		std::wstring dir = path;
		const auto slash = dir.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
			dir.resize(slash);
		ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	void CollectFiltered(std::vector<const LogEntry*>& filtered)
	{
		filtered.clear();
		filtered.reserve(gDraw.size());
		if (gDaysCombo < 0 || gDaysCombo > 4)
			gDaysCombo = 0;
		const int daysCut = kDaysMap[gDaysCombo];
		const time_t now = std::time(nullptr);
		const time_t cut = daysCut > 0 ? now - static_cast<time_t>(daysCut) * 86400 : 0;
		for (const LogEntry& e : gDraw)
		{
			if (gSearch[0] && !ContainsI(e.fileName, gSearch) && !ContainsI(e.encounter, gSearch) &&
				!ContainsI(e.pathUtf8, gSearch))
				continue;
			const auto rf = static_cast<ResultFilter>(gResultFilter);
			if (rf == ResultFilter::Success && e.result != 1)
				continue;
			if (rf == ResultFilter::Failure && e.result != 0)
				continue;
			if (rf == ResultFilter::Unknown && e.result != -1)
				continue;
			const auto mf = static_cast<ModeFilter>(gModeFilter);
			if (mf == ModeFilter::Normal && !e.mode.empty())
				continue;
			if (mf == ModeFilter::CM && e.mode != "CM")
				continue;
			if (mf == ModeFilter::LCM && e.mode != "LCM")
				continue;
			if (cut > 0)
			{
				const time_t t = e.encounterTime > 0 ? e.encounterTime : FileTimeToUnix(e.mtime);
				if (t < cut)
					continue;
			}
			filtered.push_back(&e);
		}
	}

} // namespace LogManagerDetail
