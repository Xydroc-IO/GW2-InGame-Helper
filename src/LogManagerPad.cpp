#include "LogManagerPad.h"

#include "AddonPaths.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

namespace
{
	/* Default pad size — tuned to fit 1080p game clients with room for chrome. */
	constexpr float kPadW = 1280.f;
	constexpr float kPadH = 700.f;
	/* Screenshot proportions ≈ filters 15% | list 40% | detail 45%. */
	constexpr float kFilterFrac = 0.14f;
	constexpr float kFilterMinW = 140.f;
	constexpr float kFilterMaxW = 188.f;
	constexpr float kLogListFracDef = 0.47f; /* of space after filters → ~40% overall */
	constexpr float kLogListMinW = 260.f;
	constexpr float kRightPaneMinW = 340.f; /* room for KillProof / boon tables */
	constexpr float kSplitHitW = 6.f;
	constexpr float kPaneGap = 4.f;
	constexpr int kMaxPlayersPerLog = 64;
	constexpr int kParseTimeoutMs = 180000;
	constexpr int kUploadTimeoutMs = 120000;
	constexpr int kDaysMap[] = {0, 1, 3, 7, 30};

	enum class ParseState : int
	{
		Pending = 0,
		Parsed,
		Failed,
		Uploading,
		Uploaded
	};

	enum class ResultFilter : int
	{
		All = 0,
		Success,
		Failure,
		Unknown
	};

	enum class ModeFilter : int
	{
		All = 0,
		Normal,
		CM,
		LCM
	};

	struct PlayerInfo
	{
		std::string name;
		std::string account;
		std::string profession;
		std::string guildTag; /* from name [TAG] when present */
		std::string guildId;
		int group = 0;
		int dps = 0;
		int powerDps = 0;
		int condiDps = 0;
		/* Full-fight boon uptimes (%), -1 = unknown */
		float might = -1.f;
		float fury = -1.f;
		float quickness = -1.f;
		float alacrity = -1.f;
		float protection = -1.f;
		float regeneration = -1.f;
		float swiftness = -1.f;
		float vigor = -1.f;
		/* killproof.me — -1 unknown / not loaded */
		int kpLi = -1;
		int kpLd = -1;
		int kpUfe = -1;
		int kpBoss = -1; /* encounter token amount when mapped */
		std::string kpBossLabel;
		std::string kpUrl;
		int kpState = 0; /* 0 unknown, 1 loading, 2 ok, 3 missing, 4 error */
	};

	/* killproof.me item ids (killproofs[]). */
	constexpr int kKpIdLi = 77302;
	constexpr int kKpIdLd = 88485;
	constexpr int kKpIdUfe = 94020;

	struct KillProofCacheEntry
	{
		int li = -1;
		int ld = -1;
		int ufe = -1;
		std::string proofUrl;
		std::unordered_map<int, int> amounts; /* item id → qty */
		DWORD fetchedAtMs = 0;
		bool missing = false;
		bool error = false;
	};

	struct LogEntry
	{
		std::wstring pathW;
		std::string pathUtf8;
		std::string fileName;
		ULARGE_INTEGER fileSize{};
		FILETIME mtime{};
		ParseState state = ParseState::Pending;
		std::string encounter;
		std::string mode; /* "", "CM", "LCM" */
		int result = -1; /* -1 unknown, 0 fail, 1 success */
		long long durationMs = 0;
		time_t encounterTime = 0;
		int compDps = 0; /* squad DPS from dps.report / EI */
		std::string dpsReportUrl;
		std::string parseError;
		std::string jsonPathUtf8;
		std::vector<PlayerInfo> players;
	};

	/* Common squad boon skill IDs (GW2). */
	constexpr long long kBuffMight = 740;
	constexpr long long kBuffFury = 725;
	constexpr long long kBuffQuickness = 1187;
	constexpr long long kBuffAlacrity = 30328;
	constexpr long long kBuffProtection = 717;
	constexpr long long kBuffRegen = 718;
	constexpr long long kBuffSwiftness = 719;
	constexpr long long kBuffVigor = 726;

	struct PlayerAgg
	{
		std::string account;
		std::string displayName;
		std::string profession;
		int logs = 0;
		int success = 0;
	};

	struct GuildAgg
	{
		std::string key; /* tag or guildId */
		std::string label;
		int logs = 0;
		int players = 0;
	};

	struct FastestKill
	{
		std::string encounter;
		long long durationMs = 0;
		std::string fileName;
		std::string pathUtf8;
	};

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
	char gStatus[256] = {};
	char gEiStatus[256] = {};
	char gSearch[96] = {};
	char gEncounterFilter[96] = {};
	int gResultFilter = static_cast<int>(ResultFilter::All);
	int gModeFilter = static_cast<int>(ModeFilter::All);
	int gDaysCombo = 0; /* index into kDaysMap */
	int gSelected = -1;
	bool gFocusSetupTab = false;
	float gLogListFrac = kLogListFracDef; /* synced from G::LogManagerListFrac */

	std::vector<std::string> gUploadQueue; /* pathUtf8 */

	/* ---------- string / path helpers ---------- */

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

	std::string JsonEscape(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (unsigned char c : s)
		{
			switch (c)
			{
			case '"': o += "\\\""; break;
			case '\\': o += "\\\\"; break;
			case '\n': o += "\\n"; break;
			case '\r': o += "\\r"; break;
			case '\t': o += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					o += buf;
				}
				else
					o += static_cast<char>(c);
				break;
			}
		}
		return o;
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

	bool IsLogFileName(const std::wstring& name)
	{
		if (EndsWithI(name, L".zevtc") || EndsWithI(name, L".evtc") || EndsWithI(name, L".evtc.zip"))
			return true;
		return false;
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

	void EiStatusCb(const char* msg)
	{
		if (!msg)
			return;
		std::snprintf(gEiStatus, sizeof(gEiStatus), "%s", msg);
		std::snprintf(gStatus, sizeof(gStatus), "%s", msg);
	}

	DWORD WINAPI EiInstallWorker(LPVOID)
	{
		const std::wstring dir = AddonPaths::DataDir();
		const bool ok = EiRuntime::EnsureInstalled(dir.c_str(), EiStatusCb);
		if (ok)
		{
			ApplyManagedCliPath();
			char stamp[64]{};
			EiRuntime::InvalidateDotNet8Cache();
			if (EiRuntime::GetInstalledStamp(dir.c_str(), stamp, sizeof(stamp)))
			{
				if (EiRuntime::HasDotNet8Runtime())
					std::snprintf(gStatus, sizeof(gStatus), "Elite Insights %s ready.", stamp);
				else
					std::snprintf(gStatus, sizeof(gStatus),
						"Elite Insights %s installed — install .NET 8 Runtime to parse.", stamp);
			}
			else if (EiRuntime::HasDotNet8Runtime())
				std::snprintf(gStatus, sizeof(gStatus), "Elite Insights ready.");
			else
				std::snprintf(gStatus, sizeof(gStatus),
					"Elite Insights installed — install .NET 8 Runtime to parse.");
		}
		else if (!gEiStatus[0])
			std::snprintf(gStatus, sizeof(gStatus), "Elite Insights install failed.");
		gEiInstallBusy.store(false);
		return 0;
	}

	void BeginEiEnsure(bool force)
	{
		if (gEiInstallBusy.exchange(true))
			return;

		/* Custom path already works — skip auto-update unless forced. */
		if (!force && PathExistsUtf8(G::EliteInsightsPath) && !IsManagedEiPath(G::EliteInsightsPath))
		{
			std::snprintf(gEiStatus, sizeof(gEiStatus), "Using custom Elite Insights path.");
			gEiInstallBusy.store(false);
			return;
		}

		std::snprintf(gEiStatus, sizeof(gEiStatus), "Checking Elite Insights updates…");
		std::snprintf(gStatus, sizeof(gStatus), "Checking Elite Insights updates…");
		if (gEiInstallThread)
		{
			CloseHandle(gEiInstallThread);
			gEiInstallThread = nullptr;
		}
		gEiInstallThread = CreateThread(nullptr, 0, EiInstallWorker, nullptr, 0, nullptr);
		if (!gEiInstallThread)
			gEiInstallBusy.store(false);
	}

	std::wstring EiConfPathW()
	{
		return AddonPaths::DataDir() + L"\\ei-helper.conf";
	}

	std::wstring CachePathW()
	{
		return AddonPaths::DataDir() + L"\\log-index.json";
	}

	bool FileExistsW(const std::wstring& path)
	{
		const DWORD a = GetFileAttributesW(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool DirExistsW(const std::wstring& path)
	{
		const DWORD a = GetFileAttributesW(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
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

	std::string ExtractGuildTag(const std::string& name)
	{
		const auto open = name.rfind('[');
		const auto close = name.rfind(']');
		if (open == std::string::npos || close == std::string::npos || close <= open + 1)
			return {};
		if (close != name.size() - 1 && name.find(']', open) != close)
			return {};
		std::string tag = name.substr(open + 1, close - open - 1);
		if (tag.empty() || tag.size() > 8)
			return {};
		for (char c : tag)
		{
			if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
				return {};
		}
		return tag;
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

	/* ---------- lightweight JSON extractors ---------- */

	const char* SkipWs(const char* p)
	{
		while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			++p;
		return p;
	}

	bool JsonStringAfterKey(const char* json, const char* key, std::string& out)
	{
		out.clear();
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p += std::strlen(pat);
		p = SkipWs(p);
		if (*p != ':')
			return false;
		++p;
		p = SkipWs(p);
		if (*p != '"')
			return false;
		++p;
		std::string s;
		while (*p && *p != '"')
		{
			if (*p == '\\' && p[1])
			{
				++p;
				switch (*p)
				{
				case 'n': s += '\n'; break;
				case 'r': s += '\r'; break;
				case 't': s += '\t'; break;
				case '"': s += '"'; break;
				case '\\': s += '\\'; break;
				default: s += *p; break;
				}
			}
			else
				s += *p;
			++p;
		}
		out = std::move(s);
		return true;
	}

	bool JsonBoolAfterKey(const char* json, const char* key, bool& out)
	{
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p += std::strlen(pat);
		p = SkipWs(p);
		if (*p != ':')
			return false;
		++p;
		p = SkipWs(p);
		if (std::strncmp(p, "true", 4) == 0)
		{
			out = true;
			return true;
		}
		if (std::strncmp(p, "false", 5) == 0)
		{
			out = false;
			return true;
		}
		return false;
	}

	bool JsonLongAfterKey(const char* json, const char* key, long long& out)
	{
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p += std::strlen(pat);
		p = SkipWs(p);
		if (*p != ':')
			return false;
		++p;
		p = SkipWs(p);
		char* end = nullptr;
		const long long v = std::strtoll(p, &end, 10);
		if (end == p)
			return false;
		out = v;
		return true;
	}

	const char* ObjectEnd(const char* start); /* defined below */

	bool JsonDoubleAfterKey(const char* json, const char* key, double& out)
	{
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p += std::strlen(pat);
		p = SkipWs(p);
		if (*p != ':')
			return false;
		++p;
		p = SkipWs(p);
		char* end = nullptr;
		const double v = std::strtod(p, &end);
		if (end == p)
			return false;
		out = v;
		return true;
	}

	bool ExtractFirstObjectInArrayAfterKey(const char* json, const char* key, std::string& objOut)
	{
		objOut.clear();
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p = std::strchr(p, '[');
		if (!p)
			return false;
		++p;
		p = SkipWs(p);
		if (*p != '{')
			return false;
		const char* end = ObjectEnd(p);
		if (!end)
			return false;
		objOut.assign(p, end);
		return true;
	}

	float BuffUptimePercent(const char* playerObj, long long buffId)
	{
		if (!playerObj)
			return -1.f;
		const char* bu = std::strstr(playerObj, "\"buffUptimes\"");
		if (!bu)
			bu = std::strstr(playerObj, "\"BuffUptimes\"");
		if (!bu)
			return -1.f;
		const char* arr = std::strchr(bu, '[');
		if (!arr)
			return -1.f;
		char idPat[48];
		std::snprintf(idPat, sizeof(idPat), "\"id\":%lld", buffId);
		char idPatSp[48];
		std::snprintf(idPatSp, sizeof(idPatSp), "\"id\": %lld", buffId);
		const char* hit = std::strstr(arr, idPat);
		if (!hit)
			hit = std::strstr(arr, idPatSp);
		if (!hit)
		{
			std::snprintf(idPat, sizeof(idPat), "\"Id\":%lld", buffId);
			std::snprintf(idPatSp, sizeof(idPatSp), "\"Id\": %lld", buffId);
			hit = std::strstr(arr, idPat);
			if (!hit)
				hit = std::strstr(arr, idPatSp);
		}
		if (!hit)
			return -1.f;
		/* Walk back to owning object '{'. */
		const char* obj = hit;
		while (obj > arr && *obj != '{')
			--obj;
		if (*obj != '{')
			return -1.f;
		const char* objEnd = ObjectEnd(obj);
		if (!objEnd)
			return -1.f;
		std::string slice(obj, objEnd);
		std::string data0;
		if (!ExtractFirstObjectInArrayAfterKey(slice.c_str(), "buffData", data0) &&
			!ExtractFirstObjectInArrayAfterKey(slice.c_str(), "BuffData", data0))
			return -1.f;
		double up = 0.0;
		if (!JsonDoubleAfterKey(data0.c_str(), "uptime", up) &&
			!JsonDoubleAfterKey(data0.c_str(), "Uptime", up))
			return -1.f;
		return static_cast<float>(up);
	}

	void FillPlayerCombatStats(const char* playerObj, PlayerInfo& pi)
	{
		if (!playerObj)
			return;
		std::string dps0;
		if (ExtractFirstObjectInArrayAfterKey(playerObj, "dpsAll", dps0) ||
			ExtractFirstObjectInArrayAfterKey(playerObj, "DpsAll", dps0))
		{
			long long v = 0;
			if (JsonLongAfterKey(dps0.c_str(), "dps", v) || JsonLongAfterKey(dps0.c_str(), "Dps", v))
				pi.dps = static_cast<int>(v);
			if (JsonLongAfterKey(dps0.c_str(), "powerDps", v) || JsonLongAfterKey(dps0.c_str(), "PowerDps", v))
				pi.powerDps = static_cast<int>(v);
			if (JsonLongAfterKey(dps0.c_str(), "condiDps", v) || JsonLongAfterKey(dps0.c_str(), "CondiDps", v))
				pi.condiDps = static_cast<int>(v);
		}
		/* Only overwrite when nested buffUptimes exist — flat cache values must survive. */
		auto setBuff = [](float& dst, float v) {
			if (v >= 0.f)
				dst = v;
		};
		setBuff(pi.might, BuffUptimePercent(playerObj, kBuffMight));
		setBuff(pi.fury, BuffUptimePercent(playerObj, kBuffFury));
		setBuff(pi.quickness, BuffUptimePercent(playerObj, kBuffQuickness));
		setBuff(pi.alacrity, BuffUptimePercent(playerObj, kBuffAlacrity));
		setBuff(pi.protection, BuffUptimePercent(playerObj, kBuffProtection));
		setBuff(pi.regeneration, BuffUptimePercent(playerObj, kBuffRegen));
		setBuff(pi.swiftness, BuffUptimePercent(playerObj, kBuffSwiftness));
		setBuff(pi.vigor, BuffUptimePercent(playerObj, kBuffVigor));
	}

	bool PlayersHaveDps(const std::vector<PlayerInfo>& players)
	{
		for (const auto& p : players)
		{
			if (p.dps > 0)
				return true;
		}
		return false;
	}

	bool PlayersHaveBoons(const std::vector<PlayerInfo>& players)
	{
		for (const auto& p : players)
		{
			if (p.quickness >= 0.f || p.alacrity >= 0.f || p.might >= 0.f ||
				p.fury >= 0.f || p.protection >= 0.f || p.regeneration >= 0.f ||
				p.swiftness >= 0.f || p.vigor >= 0.f)
				return true;
		}
		return false;
	}

	bool PlayersNeedCombatStats(const std::vector<PlayerInfo>& players)
	{
		return !PlayersHaveDps(players) || !PlayersHaveBoons(players);
	}

	const char* ProfessionNameFromId(long long id)
	{
		switch (id)
		{
		case 1: return "Guardian";
		case 2: return "Warrior";
		case 3: return "Engineer";
		case 4: return "Ranger";
		case 5: return "Thief";
		case 6: return "Elementalist";
		case 7: return "Mesmer";
		case 8: return "Necromancer";
		case 9: return "Revenant";
		default: return nullptr;
		}
	}

	const char* EliteSpecNameFromId(long long id)
	{
		switch (id)
		{
		case 5: return "Druid";
		case 7: return "Daredevil";
		case 18: return "Berserker";
		case 27: return "Dragonhunter";
		case 34: return "Reaper";
		case 40: return "Chronomancer";
		case 43: return "Scrapper";
		case 48: return "Tempest";
		case 52: return "Herald";
		case 55: return "Soulbeast";
		case 56: return "Weaver";
		case 57: return "Holosmith";
		case 58: return "Deadeye";
		case 59: return "Mirage";
		case 60: return "Scourge";
		case 61: return "Spellbreaker";
		case 62: return "Firebrand";
		case 63: return "Renegade";
		case 64: return "Harbinger";
		case 65: return "Willbender";
		case 66: return "Virtuoso";
		case 67: return "Catalyst";
		case 68: return "Bladesworn";
		case 69: return "Vindicator";
		case 70: return "Mechanist";
		case 71: return "Specter";
		case 72: return "Untamed";
		case 73: return "Troubadour";
		case 74: return "Paragon";
		case 75: return "Amalgam";
		case 76: return "Ritualist";
		case 77: return "Antiquary";
		case 78: return "Galeshot";
		case 79: return "Conduit";
		case 80: return "Evoker";
		case 81: return "Luminary";
		default: return nullptr;
		}
	}

	std::string FormatProfessionElite(long long prof, long long elite)
	{
		const char* eliteName = elite > 0 ? EliteSpecNameFromId(elite) : nullptr;
		if (eliteName)
			return eliteName;
		const char* profName = ProfessionNameFromId(prof);
		if (profName)
			return profName;
		if (prof <= 0 && elite <= 0)
			return {};
		char buf[48];
		if (elite > 0)
			std::snprintf(buf, sizeof(buf), "%lld / elite %lld", prof, elite);
		else
			std::snprintf(buf, sizeof(buf), "%lld", prof);
		return buf;
	}

	void FillPlayerFromDpsReportObj(const char* obj, const std::string& fallbackName, PlayerInfo& pi)
	{
		pi = PlayerInfo{};
		pi.name = fallbackName;
		if (obj)
		{
			JsonStringAfterKey(obj, "character_name", pi.name);
			JsonStringAfterKey(obj, "display_name", pi.account);
			long long prof = 0, elite = 0;
			JsonLongAfterKey(obj, "profession", prof);
			JsonLongAfterKey(obj, "elite_spec", elite);
			pi.profession = FormatProfessionElite(prof, elite);
		}
		pi.guildTag = ExtractGuildTag(pi.name);
		if (pi.guildTag.empty())
			pi.guildTag = ExtractGuildTag(pi.account);
	}

	const char* FindPlayersArray(const char* json)
	{
		if (!json)
			return nullptr;
		const char* p = std::strstr(json, "\"players\"");
		if (!p)
			p = std::strstr(json, "\"Players\"");
		if (!p)
			return nullptr;
		p = std::strchr(p, '[');
		return p;
	}

	/* Extract object slice starting at '{' — returns end past matching '}'. */
	const char* ObjectEnd(const char* start)
	{
		if (!start || *start != '{')
			return nullptr;
		int depth = 0;
		bool inStr = false;
		bool esc = false;
		for (const char* p = start; *p; ++p)
		{
			if (inStr)
			{
				if (esc)
					esc = false;
				else if (*p == '\\')
					esc = true;
				else if (*p == '"')
					inStr = false;
				continue;
			}
			if (*p == '"')
			{
				inStr = true;
				continue;
			}
			if (*p == '{')
				++depth;
			else if (*p == '}')
			{
				--depth;
				if (depth == 0)
					return p + 1;
			}
		}
		return nullptr;
	}

	void ParsePlayersFromJson(const char* json, std::vector<PlayerInfo>& out)
	{
		out.clear();
		const char* arr = FindPlayersArray(json);
		if (!arr)
			return;
		const char* p = arr + 1;
		while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
		{
			p = SkipWs(p);
			if (*p == ']')
				break;
			if (*p != '{')
			{
				++p;
				continue;
			}
			const char* end = ObjectEnd(p);
			if (!end)
				break;
			std::string obj(p, end);
			PlayerInfo pi;
			JsonStringAfterKey(obj.c_str(), "name", pi.name);
			if (pi.name.empty())
				JsonStringAfterKey(obj.c_str(), "Name", pi.name);
			JsonStringAfterKey(obj.c_str(), "account", pi.account);
			if (pi.account.empty())
				JsonStringAfterKey(obj.c_str(), "Account", pi.account);
			/* Trim whitespace — EI sometimes pads account names. */
			while (!pi.account.empty() && (pi.account.front() == ' ' || pi.account.front() == '\t'))
				pi.account.erase(pi.account.begin());
			while (!pi.account.empty() && (pi.account.back() == ' ' || pi.account.back() == '\t'))
				pi.account.pop_back();
			JsonStringAfterKey(obj.c_str(), "profession", pi.profession);
			if (pi.profession.empty())
				JsonStringAfterKey(obj.c_str(), "Profession", pi.profession);
			JsonStringAfterKey(obj.c_str(), "guildID", pi.guildId);
			if (pi.guildId.empty())
				JsonStringAfterKey(obj.c_str(), "GuildID", pi.guildId);
			if (pi.guildId.empty())
				JsonStringAfterKey(obj.c_str(), "guildId", pi.guildId);
			/* EI uses nil UUID when the player has no guild — treat as empty. */
			if (pi.guildId == "00000000-0000-0000-0000-000000000000")
				pi.guildId.clear();
			if (pi.guildTag.empty())
				JsonStringAfterKey(obj.c_str(), "guildTag", pi.guildTag);
			long long grp = 0;
			if (JsonLongAfterKey(obj.c_str(), "group", grp) || JsonLongAfterKey(obj.c_str(), "Group", grp))
				pi.group = static_cast<int>(grp);
			if (pi.guildTag.empty())
				pi.guildTag = ExtractGuildTag(pi.name);
			if (pi.guildTag.empty())
				pi.guildTag = ExtractGuildTag(pi.account);
			/* Flat cached fields first, then EI nested dpsAll / buffUptimes. */
			long long flat = 0;
			if (JsonLongAfterKey(obj.c_str(), "dps", flat))
				pi.dps = static_cast<int>(flat);
			if (JsonLongAfterKey(obj.c_str(), "powerDps", flat))
				pi.powerDps = static_cast<int>(flat);
			if (JsonLongAfterKey(obj.c_str(), "condiDps", flat))
				pi.condiDps = static_cast<int>(flat);
			auto readF = [&](const char* key, float& dst) {
				double d = 0.0;
				if (JsonDoubleAfterKey(obj.c_str(), key, d))
					dst = static_cast<float>(d);
			};
			readF("might", pi.might);
			readF("fury", pi.fury);
			readF("quickness", pi.quickness);
			readF("alacrity", pi.alacrity);
			readF("protection", pi.protection);
			readF("regeneration", pi.regeneration);
			readF("swiftness", pi.swiftness);
			readF("vigor", pi.vigor);
			FillPlayerCombatStats(obj.c_str(), pi);
			if (!pi.name.empty() || !pi.account.empty())
				out.push_back(std::move(pi));
			p = end;
			p = SkipWs(p);
			if (*p == ',')
				++p;
		}
		std::sort(out.begin(), out.end(), [](const PlayerInfo& a, const PlayerInfo& b) {
			if (a.dps != b.dps)
				return a.dps > b.dps;
			return a.name < b.name;
		});
	}

	/* Resolve EI guildUUID → official API tag (worker thread only). */
	void ResolveGuildTagsForPlayers(std::vector<PlayerInfo>& players)
	{
		static std::mutex sCacheMu;
		static std::unordered_map<std::string, std::string> sTagById;

		for (PlayerInfo& p : players)
		{
			if (p.guildId.empty() || !p.guildTag.empty())
				continue;
			{
				std::lock_guard<std::mutex> lock(sCacheMu);
				auto it = sTagById.find(p.guildId);
				if (it != sTagById.end())
				{
					p.guildTag = it->second;
					continue;
				}
			}
			std::string path = "/v2/guild/";
			path += p.guildId;
			const Gw2Http::Result r = Gw2Http::Api(path.c_str(), nullptr, 6000);
			if (!r.ok || r.body.empty())
				continue;
			std::string tag;
			if (!JsonStringAfterKey(r.body.c_str(), "tag", tag) || tag.empty())
				continue;
			p.guildTag = tag;
			std::lock_guard<std::mutex> lock(sCacheMu);
			sTagById[p.guildId] = tag;
		}
	}

	std::string ToLowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	std::string UrlEncodeAccount(const std::string& account)
	{
		std::string out;
		out.reserve(account.size() * 3);
		for (unsigned char c : account)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == '~')
				out += static_cast<char>(c);
			else
			{
				char hex[8];
				std::snprintf(hex, sizeof(hex), "%%%02X", c);
				out += hex;
			}
		}
		return out;
	}

	/* Encounter name → token item id (from killproof.me tokens / killproofs). */
	bool BossTokenForEncounter(const std::string& encounter, int& outId, const char*& outLabel)
	{
		outId = 0;
		outLabel = nullptr;
		if (encounter.empty())
			return false;
		const std::string low = ToLowerCopy(encounter);
		struct Map
		{
			const char* needle;
			int id;
			const char* label;
		};
		static const Map kMap[] = {
			{"vale guardian", 77705, "VG"},
			{"gorseval", 77751, "Gors"},
			{"sabetha", 77728, "Sab"},
			{"slothasor", 77706, "Sloth"},
			{"matthias", 77679, "Matt"},
			{"keep construct", 78902, "KC"},
			{"xera", 78942, "Xera"},
			{"cairn", 80623, "Cairn"},
			{"mursaat", 80542, "MO"},
			{"samarog", 80269, "Sam"},
			{"deimos", 80269, "Deimos"}, /* floor fragment often used; prefer Impaled if present */
			{"soulless horror", 85993, "SH"},
			{"desmina", 85993, "SH"},
			{"river of souls", 85785, "River"},
			{"statues", 85800, "Statues"},
			{"dhuum", 85633, "Dhuum"},
			{"conjured amalgamate", 88543, "CA"},
			{"twin largos", 88860, "Largos"},
			{"qadim the peerless", 91175, "QTP"},
			{"qadim", 88645, "Qadim"},
			{"cardinal adina", 91246, "Adina"},
			{"cardinal sabir", 91270, "Sabir"},
			{"godsquall decima", 103754, "Decima"},
			{"decima", 103754, "Decima"},
			{"greer", 104047, "Greer"},
			{"ura", 103996, "Ura"},
			{"dagda", 100068, "Dagda"},
			{"cerus", 100858, "Cerus"},
			{"mai trin", 95638, "Mai"},
			{"ankka", 95982, "Ankka"},
			{"minister li", 97451, "Li"},
			{"void", 97132, "Void"},
			{"assault knight", 99165, "AK"},
			{"boneskinner", 93781, "Vial"},
		};
		for (const Map& m : kMap)
		{
			if (low.find(m.needle) != std::string::npos)
			{
				outId = m.id;
				outLabel = m.label;
				return true;
			}
		}
		return false;
	}

	int AmountNearId(const char* json, int itemId)
	{
		if (!json || itemId <= 0)
			return -1;
		char pat[48];
		std::snprintf(pat, sizeof(pat), "\"id\":%d", itemId);
		const char* hit = std::strstr(json, pat);
		if (!hit)
		{
			std::snprintf(pat, sizeof(pat), "\"id\": %d", itemId);
			hit = std::strstr(json, pat);
		}
		if (!hit)
			return -1;
		const char* obj = hit;
		while (obj > json && *obj != '{')
			--obj;
		if (*obj != '{')
			return -1;
		const char* end = ObjectEnd(obj);
		if (!end)
			return -1;
		std::string slice(obj, end);
		long long amt = 0;
		if (!JsonLongAfterKey(slice.c_str(), "amount", amt))
			return -1;
		if (amt < 0)
			amt = 0;
		if (amt > 2000000000ll)
			amt = 2000000000ll;
		return static_cast<int>(amt);
	}

	bool FetchKillProofProfile(const std::string& account, KillProofCacheEntry& out)
	{
		out = KillProofCacheEntry{};
		out.fetchedAtMs = GetTickCount();
		if (account.empty())
		{
			out.missing = true;
			return false;
		}
		std::string url = "https://killproof.me/api/kp/";
		url += UrlEncodeAccount(account);
		url += "?lang=en";
		const Gw2Http::Result r = Gw2Http::Get(url.c_str(), nullptr, 10000);
		if (r.body.find("\"error\"") != std::string::npos &&
			r.body.find("Account not found") != std::string::npos)
		{
			out.missing = true;
			return false;
		}
		if (r.status == 404)
		{
			out.missing = true;
			return false;
		}
		if (!r.ok || r.body.empty())
		{
			out.error = true;
			return false;
		}
		JsonStringAfterKey(r.body.c_str(), "proof_url", out.proofUrl);
		out.li = AmountNearId(r.body.c_str(), kKpIdLi);
		out.ld = AmountNearId(r.body.c_str(), kKpIdLd);
		out.ufe = AmountNearId(r.body.c_str(), kKpIdUfe);
		/* Cache common token amounts from tokens[] + killproofs[]. */
		static const int kExtraIds[] = {
			77705, 77751, 77728, 77706, 77679, 78902, 78942, 80623, 80542, 80269,
			85993, 85785, 85800, 85633, 88543, 88860, 88645, 91175, 91246, 91270,
			103754, 104047, 103996, 100068, 100858, 95638, 95982, 97451, 97132,
			99165, 93781, 78873, 80087
		};
		for (int id : kExtraIds)
		{
			const int amt = AmountNearId(r.body.c_str(), id);
			if (amt >= 0)
				out.amounts[id] = amt;
		}
		if (out.li < 0 && out.ld < 0 && out.ufe < 0 && out.amounts.empty() && out.proofUrl.empty())
		{
			/* Private / empty profile — treat as missing rather than inventing zeros. */
			out.missing = true;
			return false;
		}
		if (out.li < 0) out.li = 0;
		if (out.ld < 0) out.ld = 0;
		if (out.ufe < 0) out.ufe = 0;
		return true;
	}

	void ApplyKillProofEntryToPlayer(PlayerInfo& p, const KillProofCacheEntry& e, const std::string& encounter)
	{
		if (e.missing)
		{
			p.kpState = 3;
			p.kpLi = p.kpLd = p.kpUfe = p.kpBoss = -1;
			p.kpBossLabel.clear();
			p.kpUrl.clear();
			return;
		}
		if (e.error)
		{
			p.kpState = 4;
			return;
		}
		p.kpLi = e.li;
		p.kpLd = e.ld;
		p.kpUfe = e.ufe;
		p.kpUrl = e.proofUrl;
		p.kpState = 2;
		int bossId = 0;
		const char* bossLabel = nullptr;
		if (BossTokenForEncounter(encounter, bossId, bossLabel))
		{
			p.kpBossLabel = bossLabel ? bossLabel : "";
			auto it = e.amounts.find(bossId);
			if (it != e.amounts.end())
				p.kpBoss = it->second;
			else
				p.kpBoss = 0;
		}
		else
		{
			p.kpBoss = -1;
			p.kpBossLabel.clear();
		}
	}

	void ApplyKillProofCacheToPlayersLocked(std::vector<PlayerInfo>& players, const std::string& encounter)
	{
		/* Caller must hold gKpCacheMu. */
		for (PlayerInfo& p : players)
		{
			if (p.account.empty())
				continue;
			const std::string key = ToLowerCopy(p.account);
			auto it = gKpCache.find(key);
			if (it == gKpCache.end())
				continue;
			ApplyKillProofEntryToPlayer(p, it->second, encounter);
		}
	}

	void ApplyKillProofCacheToAllLogsLocked()
	{
		/* Caller must hold gMu and gKpCacheMu. */
		for (LogEntry& e : gLogs)
			ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
	}

	void QueueKillProofAccountsLocked(const std::vector<PlayerInfo>& players, bool force)
	{
		/* Caller must hold gKpCacheMu. */
		const DWORD now = GetTickCount();
		constexpr DWORD kTtlMs = 60u * 60u * 1000u;
		for (const PlayerInfo& p : players)
		{
			if (p.account.empty())
				continue;
			const std::string key = ToLowerCopy(p.account);
			if (!force)
			{
				auto it = gKpCache.find(key);
				if (it != gKpCache.end() && !it->second.error &&
					(now - it->second.fetchedAtMs) < kTtlMs)
					continue;
			}
			bool queued = false;
			for (const std::string& q : gKpQueue)
			{
				if (ToLowerCopy(q) == key)
				{
					queued = true;
					break;
				}
			}
			if (!queued)
				gKpQueue.push_back(p.account);
		}
		if (force)
			gKpForce.store(true);
	}

	DWORD WINAPI KillProofWorker(LPVOID);
	void BeginKillProofFetch(bool force);

	void BeginKillProofFetch(bool force)
	{
		if (force)
			gKpForce.store(true);
		if (gKillProofBusy.exchange(true))
			return;
		if (gKillProofThread)
		{
			CloseHandle(gKillProofThread);
			gKillProofThread = nullptr;
		}
		gKillProofThread = CreateThread(nullptr, 0, KillProofWorker, nullptr, 0, nullptr);
		if (!gKillProofThread)
			gKillProofBusy.store(false);
	}

	DWORD WINAPI KillProofWorker(LPVOID)
	{
		gKpForce.exchange(false);
		int done = 0;
		for (;;)
		{
			std::vector<std::string> jobs;
			{
				std::lock_guard<std::mutex> lock(gKpCacheMu);
				jobs.swap(gKpQueue);
			}
			if (jobs.empty())
				break;
			for (const std::string& account : jobs)
			{
				if (gCancel.load())
					break;
				std::snprintf(gStatus, sizeof(gStatus), "Loading KP %d… (%s)",
					done + 1, account.c_str());
				KillProofCacheEntry entry;
				FetchKillProofProfile(account, entry);
				{
					std::lock_guard<std::mutex> lockLogs(gMu);
					std::lock_guard<std::mutex> lockKp(gKpCacheMu);
					gKpCache[ToLowerCopy(account)] = entry;
					ApplyKillProofCacheToAllLogsLocked();
					gGen.fetch_add(1);
				}
				++done;
				Sleep(80);
			}
		}
		if (done > 0)
			std::snprintf(gStatus, sizeof(gStatus), "KP loaded for %d account(s).", done);
		gKillProofBusy.store(false);
		bool more = false;
		{
			std::lock_guard<std::mutex> lock(gKpCacheMu);
			more = !gKpQueue.empty();
		}
		if (more)
			BeginKillProofFetch(false);
		return 0;
	}

	/* Queue KP for this log. Caller holds gMu. Starts worker after releasing preferred. */
	bool EnsureKillProofForLog(LogEntry& e, bool force)
	{
		std::lock_guard<std::mutex> lockKp(gKpCacheMu);
		ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
		bool needFetch = force;
		if (!needFetch)
		{
			for (const PlayerInfo& p : e.players)
			{
				if (!p.account.empty() && p.kpState == 0)
				{
					needFetch = true;
					break;
				}
			}
		}
		if (!needFetch)
			return false;
		for (PlayerInfo& p : e.players)
		{
			if (p.account.empty())
				continue;
			if (force || p.kpState == 0)
				p.kpState = 1;
		}
		QueueKillProofAccountsLocked(e.players, force);
		return true;
	}

	void ApplyEiJsonToEntry(LogEntry& e, const std::string& json)
	{
		std::string name;
		if (!JsonStringAfterKey(json.c_str(), "name", name))
			JsonStringAfterKey(json.c_str(), "Name", name);
		if (name.empty() && !JsonStringAfterKey(json.c_str(), "fightName", name))
			JsonStringAfterKey(json.c_str(), "FightName", name);
		if (!name.empty())
			e.encounter = name;

		bool success = false;
		if (JsonBoolAfterKey(json.c_str(), "success", success) || JsonBoolAfterKey(json.c_str(), "Success", success))
			e.result = success ? 1 : 0;

		bool isLcm = false;
		bool isCm = false;
		if (JsonBoolAfterKey(json.c_str(), "isLegendaryCM", isLcm) || JsonBoolAfterKey(json.c_str(), "IsLegendaryCM", isLcm))
		{
			/* ok */
		}
		if (JsonBoolAfterKey(json.c_str(), "isCM", isCm) || JsonBoolAfterKey(json.c_str(), "IsCM", isCm))
		{
			/* ok */
		}
		if (isLcm)
			e.mode = "LCM";
		else if (isCm)
			e.mode = "CM";
		else
			e.mode.clear();

		long long dur = 0;
		if (JsonLongAfterKey(json.c_str(), "durationMS", dur) || JsonLongAfterKey(json.c_str(), "DurationMS", dur))
			e.durationMs = dur;

		std::string tStart;
		if (!JsonStringAfterKey(json.c_str(), "timeStartStd", tStart))
			JsonStringAfterKey(json.c_str(), "TimeStartStd", tStart);
		if (tStart.empty() && !JsonStringAfterKey(json.c_str(), "timeStart", tStart))
			JsonStringAfterKey(json.c_str(), "TimeStart", tStart);
		if (!tStart.empty())
		{
			/* "2024-01-15 12:34:56 +00" or similar — parse YYYY-MM-DD HH:MM:SS */
			struct tm tm{};
			int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
			if (std::sscanf(tStart.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) >= 5)
			{
				tm.tm_year = y - 1900;
				tm.tm_mon = mo - 1;
				tm.tm_mday = d;
				tm.tm_hour = h;
				tm.tm_min = mi;
				tm.tm_sec = s;
				tm.tm_isdst = -1;
				const time_t tt = std::mktime(&tm);
				if (tt > 0)
					e.encounterTime = tt;
			}
		}

		ParsePlayersFromJson(json.c_str(), e.players);
		ResolveGuildTagsForPlayers(e.players);
		{
			std::lock_guard<std::mutex> lockKp(gKpCacheMu);
			ApplyKillProofCacheToPlayersLocked(e.players, e.encounter);
		}
		long long squad = 0;
		for (const auto& p : e.players)
			squad += p.dps;
		if (squad > 0)
			e.compDps = static_cast<int>(squad);
		e.state = ParseState::Parsed;
		e.parseError.clear();
	}

	/* ---------- cache ---------- */

	std::string ReadFileUtf8(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 80 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string body(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD got = 0;
		const BOOL ok = ReadFile(h, body.data(), static_cast<DWORD>(body.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok)
			return {};
		body.resize(got);
		return body;
	}

	bool WriteFileUtf8(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD wrote = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &wrote, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	void SaveCacheLocked()
	{
		/* caller holds gMu */
		std::string out;
		out.reserve(gLogs.size() * 256 + 32);
		out += "[\n";
		for (size_t i = 0; i < gLogs.size(); ++i)
		{
			const LogEntry& e = gLogs[i];
			if (i)
				out += ",\n";
			out += "  {\"path\":\"";
			out += JsonEscape(e.pathUtf8);
			out += "\",\"size\":";
			char num[64];
			std::snprintf(num, sizeof(num), "%llu", static_cast<unsigned long long>(e.fileSize.QuadPart));
			out += num;
			out += ",\"mtime\":";
			std::snprintf(num, sizeof(num), "%llu",
				(static_cast<unsigned long long>(e.mtime.dwHighDateTime) << 32) |
					e.mtime.dwLowDateTime);
			out += num;
			out += ",\"state\":";
			std::snprintf(num, sizeof(num), "%d", static_cast<int>(e.state));
			out += num;
			out += ",\"encounter\":\"";
			out += JsonEscape(e.encounter);
			out += "\",\"mode\":\"";
			out += JsonEscape(e.mode);
			out += "\",\"result\":";
			std::snprintf(num, sizeof(num), "%d", e.result);
			out += num;
			out += ",\"durationMs\":";
			std::snprintf(num, sizeof(num), "%lld", static_cast<long long>(e.durationMs));
			out += num;
			out += ",\"encounterTime\":";
			std::snprintf(num, sizeof(num), "%lld", static_cast<long long>(e.encounterTime));
			out += num;
			out += ",\"compDps\":";
			std::snprintf(num, sizeof(num), "%d", e.compDps);
			out += num;
			out += ",\"dpsReportUrl\":\"";
			out += JsonEscape(e.dpsReportUrl);
			out += "\",\"jsonPath\":\"";
			out += JsonEscape(e.jsonPathUtf8);
			out += "\",\"players\":[";
			for (size_t pi = 0; pi < e.players.size(); ++pi)
			{
				const PlayerInfo& p = e.players[pi];
				if (pi)
					out += ',';
				out += "{\"name\":\"";
				out += JsonEscape(p.name);
				out += "\",\"account\":\"";
				out += JsonEscape(p.account);
				out += "\",\"profession\":\"";
				out += JsonEscape(p.profession);
				out += "\",\"guildTag\":\"";
				out += JsonEscape(p.guildTag);
				out += "\",\"guildId\":\"";
				out += JsonEscape(p.guildId);
				out += "\",\"group\":";
				std::snprintf(num, sizeof(num), "%d", p.group);
				out += num;
				out += ",\"dps\":";
				std::snprintf(num, sizeof(num), "%d", p.dps);
				out += num;
				out += ",\"powerDps\":";
				std::snprintf(num, sizeof(num), "%d", p.powerDps);
				out += num;
				out += ",\"condiDps\":";
				std::snprintf(num, sizeof(num), "%d", p.condiDps);
				out += num;
				auto writeF = [&](const char* key, float v) {
					out += ",\"";
					out += key;
					out += "\":";
					if (v < 0.f)
						out += "-1";
					else
					{
						std::snprintf(num, sizeof(num), "%.1f", static_cast<double>(v));
						out += num;
					}
				};
				writeF("might", p.might);
				writeF("fury", p.fury);
				writeF("quickness", p.quickness);
				writeF("alacrity", p.alacrity);
				writeF("protection", p.protection);
				writeF("regeneration", p.regeneration);
				writeF("swiftness", p.swiftness);
				writeF("vigor", p.vigor);
				out += '}';
			}
			out += "]}";
		}
		out += "\n]\n";
		WriteFileUtf8(CachePathW(), out);
	}

	void LoadCacheInto(std::unordered_map<std::string, LogEntry>& byPath)
	{
		byPath.clear();
		const std::string body = ReadFileUtf8(CachePathW());
		if (body.empty())
			return;
		/* Walk top-level objects — simple scan for "path" keys in objects. */
		const char* p = body.c_str();
		while ((p = std::strstr(p, "{\"path\":\"")) != nullptr)
		{
			const char* end = ObjectEnd(p);
			if (!end)
				break;
			std::string obj(p, end);
			LogEntry e;
			JsonStringAfterKey(obj.c_str(), "path", e.pathUtf8);
			if (e.pathUtf8.empty())
			{
				p = end;
				continue;
			}
			e.pathW = Utf8ToWide(e.pathUtf8.c_str());
			const auto slash = e.pathUtf8.find_last_of("\\/");
			e.fileName = slash == std::string::npos ? e.pathUtf8 : e.pathUtf8.substr(slash + 1);
			long long size = 0, mtime = 0, state = 0, result = -1, dur = 0, et = 0, comp = 0;
			JsonLongAfterKey(obj.c_str(), "size", size);
			JsonLongAfterKey(obj.c_str(), "mtime", mtime);
			JsonLongAfterKey(obj.c_str(), "state", state);
			JsonLongAfterKey(obj.c_str(), "result", result);
			JsonLongAfterKey(obj.c_str(), "durationMs", dur);
			JsonLongAfterKey(obj.c_str(), "encounterTime", et);
			JsonLongAfterKey(obj.c_str(), "compDps", comp);
			e.fileSize.QuadPart = static_cast<ULONGLONG>(size);
			e.mtime.dwLowDateTime = static_cast<DWORD>(mtime & 0xffffffffu);
			e.mtime.dwHighDateTime = static_cast<DWORD>((mtime >> 32) & 0xffffffffu);
			e.state = static_cast<ParseState>(static_cast<int>(state));
			if (e.state == ParseState::Uploading)
				e.state = ParseState::Parsed;
			e.result = static_cast<int>(result);
			e.durationMs = dur;
			e.encounterTime = static_cast<time_t>(et);
			e.compDps = static_cast<int>(comp);
			JsonStringAfterKey(obj.c_str(), "encounter", e.encounter);
			JsonStringAfterKey(obj.c_str(), "mode", e.mode);
			JsonStringAfterKey(obj.c_str(), "dpsReportUrl", e.dpsReportUrl);
			JsonStringAfterKey(obj.c_str(), "jsonPath", e.jsonPathUtf8);
			ParsePlayersFromJson(obj.c_str(), e.players);
			byPath[e.pathUtf8] = std::move(e);
			p = end;
		}
	}

	/* ---------- scan ---------- */

	void ScanDirRecursive(const std::wstring& dir, std::vector<LogEntry>& out, int depth)
	{
		if (depth > 12 || gCancel.load())
			return;
		const std::wstring pattern = dir + L"\\*";
		WIN32_FIND_DATAW fd{};
		HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (fd.cFileName[0] == L'.' &&
				(fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
				continue;
			const std::wstring full = dir + L'\\' + fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				ScanDirRecursive(full, out, depth + 1);
				continue;
			}
			if (!IsLogFileName(fd.cFileName))
				continue;
			LogEntry e;
			e.pathW = full;
			e.pathUtf8 = WideToUtf8(full);
			e.fileName = WideToUtf8(fd.cFileName);
			e.fileSize.LowPart = fd.nFileSizeLow;
			e.fileSize.HighPart = fd.nFileSizeHigh;
			e.mtime = fd.ftLastWriteTime;
			e.encounterTime = FileTimeToUnix(fd.ftLastWriteTime);
			out.push_back(std::move(e));
		} while (FindNextFileW(h, &fd) && !gCancel.load());
		FindClose(h);
	}

	void BeginHydrateFromReports(bool force = true);

	DWORD WINAPI ScanWorker(LPVOID)
	{
		EnsureDefaultPaths();
		const std::wstring root = Utf8ToWide(G::LogFolder);
		std::vector<LogEntry> found;
		std::unordered_map<std::string, LogEntry> cache;
		LoadCacheInto(cache);

		if (!root.empty() && DirExistsW(root))
			ScanDirRecursive(root, found, 0);

		for (LogEntry& e : found)
		{
			auto it = cache.find(e.pathUtf8);
			if (it == cache.end())
				continue;
			const LogEntry& c = it->second;
			if (c.fileSize.QuadPart == e.fileSize.QuadPart &&
				c.mtime.dwLowDateTime == e.mtime.dwLowDateTime &&
				c.mtime.dwHighDateTime == e.mtime.dwHighDateTime &&
				(c.state == ParseState::Parsed || c.state == ParseState::Uploaded ||
					c.state == ParseState::Failed))
			{
				e.state = c.state;
				e.encounter = c.encounter;
				e.mode = c.mode;
				e.result = c.result;
				e.durationMs = c.durationMs;
				if (c.encounterTime > 0)
					e.encounterTime = c.encounterTime;
				e.dpsReportUrl = c.dpsReportUrl;
				e.jsonPathUtf8 = c.jsonPathUtf8;
				e.players = c.players;
				e.compDps = c.compDps;
				e.parseError = c.parseError;
			}
		}

		std::sort(found.begin(), found.end(), [](const LogEntry& a, const LogEntry& b) {
			if (a.encounterTime != b.encounterTime)
				return a.encounterTime > b.encounterTime;
			return a.fileName > b.fileName;
		});

		{
			std::lock_guard<std::mutex> lock(gMu);
			gLogs = std::move(found);
			SaveCacheLocked();
			gGen.fetch_add(1);
		}

		int needMeta = 0;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (const auto& e : gLogs)
			{
				if (e.dpsReportUrl.empty())
					continue;
				if (e.encounter.empty() || e.result < 0 || e.durationMs <= 0 ||
					e.players.empty() || PlayersNeedCombatStats(e.players))
					++needMeta;
			}
		}
		std::snprintf(gStatus, sizeof(gStatus), "Found %d logs.", static_cast<int>(gLogs.size()));
		gScanBusy.store(false);
		if (needMeta > 0)
			BeginHydrateFromReports(false);
		return 0;
	}

	void BeginScan()
	{
		if (gScanBusy.exchange(true))
			return;
		gCancel.store(false);
		std::snprintf(gStatus, sizeof(gStatus), "Scanning logs…");
		if (gScanThread)
		{
			WaitForSingleObject(gScanThread, 0);
			CloseHandle(gScanThread);
			gScanThread = nullptr;
		}
		gScanThread = CreateThread(nullptr, 0, ScanWorker, nullptr, 0, nullptr);
		if (!gScanThread)
			gScanBusy.store(false);
	}

	/* ---------- Elite Insights ---------- */

	std::wstring EiOutDirW()
	{
		return AddonPaths::DataDir() + L"\\ei-out";
	}

	bool WriteEiConf()
	{
		const std::wstring outDir = EiOutDirW();
		CreateDirectoryW(outDir.c_str(), nullptr);
		const std::string outUtf8 = WideToUtf8(outDir);
		const std::wstring conf = EiConfPathW();
		std::string body =
			"SaveOutHTML=false\n"
			"SaveOutCSV=false\n"
			"SaveOutJSON=true\n"
			"IndentJSON=false\n"
			"ParseMultipleLogs=false\n"
			"SingleThreaded=true\n"
			"AutoAdd=false\n"
			"AutoParse=false\n"
			"AutoUpload=false\n";
		if (!outUtf8.empty())
		{
			body += "OutLocation=";
			body += outUtf8;
			body += "\n";
		}
		return WriteFileUtf8(conf, body);
	}

	bool RunProcessCapture(const std::wstring& exe, const std::wstring& args, const std::wstring& cwd,
		std::string& stdoutUtf8, DWORD timeoutMs)
	{
		stdoutUtf8.clear();
		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		HANDLE rd = nullptr, wr = nullptr;
		if (!CreatePipe(&rd, &wr, &sa, 0))
			return false;
		SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		si.hStdOutput = wr;
		si.hStdError = wr;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION pi{};
		std::wstring cmd = L"\"" + exe + L"\" " + args;
		std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
		cmdBuf.push_back(0);

		const BOOL ok = CreateProcessW(
			nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW, nullptr,
			cwd.empty() ? nullptr : cwd.c_str(),
			&si, &pi);
		CloseHandle(wr);
		if (!ok)
		{
			CloseHandle(rd);
			return false;
		}

		std::string buf;
		char chunk[4096];
		DWORD got = 0;
		const DWORD start = GetTickCount();
		for (;;)
		{
			DWORD avail = 0;
			if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
			{
				const DWORD toRead = avail > sizeof(chunk) ? sizeof(chunk) : avail;
				if (ReadFile(rd, chunk, toRead, &got, nullptr) && got > 0)
					buf.append(chunk, got);
			}
			const DWORD wait = WaitForSingleObject(pi.hProcess, 50);
			if (wait == WAIT_OBJECT_0)
			{
				while (ReadFile(rd, chunk, sizeof(chunk), &got, nullptr) && got > 0)
					buf.append(chunk, got);
				break;
			}
			if (GetTickCount() - start > timeoutMs)
			{
				TerminateProcess(pi.hProcess, 1);
				break;
			}
			if (gCancel.load())
			{
				TerminateProcess(pi.hProcess, 1);
				break;
			}
		}
		CloseHandle(rd);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		stdoutUtf8 = std::move(buf);
		return true;
	}

	std::wstring GuessJsonBeside(const std::wstring& logPath)
	{
		/* EI typically writes <name>_*.json next to the log or in OutLocation. */
		std::wstring dir = logPath;
		const auto slash = dir.find_last_of(L"\\/");
		std::wstring folder = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
		std::wstring stem = slash == std::wstring::npos ? dir : dir.substr(slash + 1);
		/* strip extensions */
		auto stripExt = [](std::wstring& s) {
			if (EndsWithI(s, L".evtc.zip"))
				s.resize(s.size() - 9);
			else if (EndsWithI(s, L".zevtc"))
				s.resize(s.size() - 6);
			else if (EndsWithI(s, L".evtc"))
				s.resize(s.size() - 5);
		};
		stripExt(stem);

		WIN32_FIND_DATAW fd{};
		const std::wstring pat = folder + L"\\" + stem + L"*.json";
		HANDLE h = FindFirstFileW(pat.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		std::wstring best;
		FILETIME bestTime{};
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			const std::wstring full = folder + L'\\' + fd.cFileName;
			if (best.empty() || CompareFileTime(&fd.ftLastWriteTime, &bestTime) > 0)
			{
				best = full;
				bestTime = fd.ftLastWriteTime;
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		return best;
	}

	bool ParseOneWithEi(LogEntry& e)
	{
		if (!G::EliteInsightsPath[0])
		{
			e.state = ParseState::Failed;
			e.parseError = "Set Elite Insights CLI path in settings.";
			return false;
		}
		const std::wstring exe = Utf8ToWide(G::EliteInsightsPath);
		if (!FileExistsW(exe))
		{
			e.state = ParseState::Failed;
			e.parseError = "Elite Insights CLI not found.";
			return false;
		}
		WriteEiConf();
		const std::wstring conf = EiConfPathW();
		std::wstring args = L"-c \"" + conf + L"\" \"" + e.pathW + L"\"";
		std::string output;
		if (!RunProcessCapture(exe, args, {}, output, kParseTimeoutMs))
		{
			e.state = ParseState::Failed;
			e.parseError = "Failed to launch Elite Insights.";
			return false;
		}

		/* Prefer generatedFiles from EI status line. */
		std::wstring jsonW;
		const char* gen = std::strstr(output.c_str(), "generatedFiles");
		if (gen)
		{
			const char* q = std::strchr(gen, '"');
			/* find first .json path in the status blob */
			const char* j = std::strstr(gen, ".json");
			if (j)
			{
				const char* start = j;
				while (start > gen && *start != '"' && *start != '\'')
					--start;
				if (*start == '"' || *start == '\'')
					++start;
				std::string path(start, static_cast<size_t>(j - start + 5));
				jsonW = Utf8ToWide(path.c_str());
			}
			(void)q;
		}
		if (jsonW.empty() || !FileExistsW(jsonW))
			jsonW = GuessJsonBeside(e.pathW);
		if (jsonW.empty() || !FileExistsW(jsonW))
			jsonW = GuessJsonBeside(EiOutDirW() + L"\\" + Utf8ToWide(e.fileName.c_str()));
		/* Also scan ei-out for newest json matching stem. */
		if (jsonW.empty() || !FileExistsW(jsonW))
		{
			std::wstring stem = Utf8ToWide(e.fileName.c_str());
			if (EndsWithI(stem, L".evtc.zip"))
				stem.resize(stem.size() - 9);
			else if (EndsWithI(stem, L".zevtc"))
				stem.resize(stem.size() - 6);
			else if (EndsWithI(stem, L".evtc"))
				stem.resize(stem.size() - 5);
			jsonW = GuessJsonBeside(EiOutDirW() + L"\\" + stem + L".zevtc");
		}

		if (jsonW.empty() || !FileExistsW(jsonW))
		{
			e.state = ParseState::Failed;
			e.parseError = "EI finished but no JSON found.";
			if (output.size() > 180)
				output.resize(180);
			if (!output.empty())
				e.parseError += " " + output;
			return false;
		}

		const std::string json = ReadFileUtf8(jsonW);
		if (json.empty())
		{
			e.state = ParseState::Failed;
			e.parseError = "Could not read EI JSON.";
			return false;
		}
		e.jsonPathUtf8 = WideToUtf8(jsonW);
		ApplyEiJsonToEntry(e, json);
		return true;
	}

	DWORD WINAPI ParseWorker(LPVOID)
	{
		std::vector<size_t> pending;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (size_t i = 0; i < gLogs.size(); ++i)
			{
				const LogEntry& e = gLogs[i];
				/* Include uploaded logs that never got EI/dps metadata. */
				const bool needsMeta = e.encounter.empty() || e.result < 0 || e.durationMs <= 0;
				if (e.state == ParseState::Pending || e.state == ParseState::Failed ||
					(needsMeta && e.state != ParseState::Uploading))
					pending.push_back(i);
			}
		}
		gParseTotal.store(static_cast<int>(pending.size()));
		gParseDone.store(0);

		for (size_t idx : pending)
		{
			if (gCancel.load())
				break;
			LogEntry local;
			{
				std::lock_guard<std::mutex> lock(gMu);
				if (idx >= gLogs.size())
					continue;
				local = gLogs[idx];
			}
			ParseOneWithEi(local);
			{
				std::lock_guard<std::mutex> lock(gMu);
				if (idx < gLogs.size() && gLogs[idx].pathUtf8 == local.pathUtf8)
					gLogs[idx] = local;
				gParseDone.fetch_add(1);
				if ((gParseDone.load() % 5) == 0)
					SaveCacheLocked();
				gGen.fetch_add(1);
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			SaveCacheLocked();
			gGen.fetch_add(1);
		}
		std::snprintf(gStatus, sizeof(gStatus), "Parse finished (%d).", gParseDone.load());
		gParseBusy.store(false);
		return 0;
	}

	void BeginParsePending()
	{
		if (!G::EliteInsightsPath[0])
		{
			std::snprintf(gStatus, sizeof(gStatus),
				"Set Elite Insights CLI path (GuildWars2EliteInsights-CLI.exe).");
			return;
		}
		EiRuntime::InvalidateDotNet8Cache();
		if (!EiRuntime::HasDotNet8Runtime())
		{
			std::snprintf(gStatus, sizeof(gStatus),
				EiRuntime::IsWine()
					? "Install .NET 8 Desktop Runtime into this Wine/Proton prefix first."
					: "Install .NET 8 Desktop Runtime first (button above).");
			return;
		}
		if (gParseBusy.exchange(true))
			return;
		gCancel.store(false);
		std::snprintf(gStatus, sizeof(gStatus), "Parsing with Elite Insights…");
		if (gParseThread)
		{
			CloseHandle(gParseThread);
			gParseThread = nullptr;
		}
		gParseThread = CreateThread(nullptr, 0, ParseWorker, nullptr, 0, nullptr);
		if (!gParseThread)
			gParseBusy.store(false);
	}

	void BeginParseSelected(const std::string& pathUtf8)
	{
		if (pathUtf8.empty())
			return;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (auto& e : gLogs)
			{
				if (e.pathUtf8 == pathUtf8)
				{
					e.state = ParseState::Pending;
					break;
				}
			}
			gGen.fetch_add(1);
		}
		BeginParsePending();
	}

	/* ---------- dps.report upload / metadata ---------- */

	std::string UrlEncode(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() * 3);
		static const char* hex = "0123456789ABCDEF";
		for (unsigned char c : s)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else
			{
				o.push_back('%');
				o.push_back(hex[c >> 4]);
				o.push_back(hex[c & 15]);
			}
		}
		return o;
	}

	/* dps.report accepts id or URL — query param works most reliably as the bare id. */
	std::string PermalinkQueryValue(const std::string& permalink)
	{
		std::string s = permalink;
		while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		const auto slash = s.find_last_of('/');
		if (slash != std::string::npos && slash + 1 < s.size())
		{
			std::string id = s.substr(slash + 1);
			const auto q = id.find_first_of("?#");
			if (q != std::string::npos)
				id.resize(q);
			if (!id.empty())
				return id;
		}
		return s;
	}

	void ParseDpsReportPlayers(const char* json, std::vector<PlayerInfo>& out)
	{
		out.clear();
		if (!json)
			return;
		const char* p = std::strstr(json, "\"players\"");
		if (!p)
			return;
		p = std::strchr(p, ':');
		if (!p)
			return;
		++p;
		p = SkipWs(p);
		/* Array form — dps.report objects (not EI player shape). */
		if (*p == '[')
		{
			++p;
			while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
			{
				p = SkipWs(p);
				if (*p == ']')
					break;
				if (*p != '{')
				{
					++p;
					continue;
				}
				const char* end = ObjectEnd(p);
				if (!end)
					break;
				std::string obj(p, end);
				PlayerInfo pi;
				FillPlayerFromDpsReportObj(obj.c_str(), {}, pi);
				if (!pi.name.empty() || !pi.account.empty())
					out.push_back(std::move(pi));
				p = end;
				p = SkipWs(p);
				if (*p == ',')
					++p;
			}
			return;
		}
		/* Object form: "Char Name": { "display_name": "...", ... } */
		if (*p != '{')
			return;
		++p;
		while (*p && out.size() < static_cast<size_t>(kMaxPlayersPerLog))
		{
			p = SkipWs(p);
			if (*p == '}')
				break;
			if (*p != '"')
			{
				++p;
				continue;
			}
			++p;
			std::string charName;
			while (*p && *p != '"')
			{
				if (*p == '\\' && p[1])
				{
					++p;
					charName.push_back(*p);
				}
				else
					charName.push_back(*p);
				++p;
			}
			if (*p == '"')
				++p;
			p = SkipWs(p);
			if (*p != ':')
				continue;
			++p;
			p = SkipWs(p);
			if (*p != '{')
				continue;
			const char* end = ObjectEnd(p);
			if (!end)
				break;
			std::string obj(p, end);
			PlayerInfo pi;
			FillPlayerFromDpsReportObj(obj.c_str(), charName, pi);
			if (!pi.name.empty() || !pi.account.empty())
				out.push_back(std::move(pi));
			p = end;
			p = SkipWs(p);
			if (*p == ',')
				++p;
		}
	}

	void ApplyDpsReportMeta(LogEntry& e, const std::string& resp)
	{
		if (resp.empty())
			return;

		std::string link;
		if (JsonStringAfterKey(resp.c_str(), "permalink", link) && !link.empty())
			e.dpsReportUrl = link;

		/* Prefer nested encounter object fields. */
		const char* enc = std::strstr(resp.c_str(), "\"encounter\"");
		std::string encSlice;
		if (enc)
		{
			const char* brace = std::strchr(enc, '{');
			if (brace)
			{
				const char* end = ObjectEnd(brace);
				if (end)
					encSlice.assign(brace, end);
			}
		}
		const char* src = !encSlice.empty() ? encSlice.c_str() : resp.c_str();

		std::string boss;
		if (JsonStringAfterKey(src, "boss", boss) && !boss.empty())
			e.encounter = boss;

		bool success = false;
		if (JsonBoolAfterKey(src, "success", success))
			e.result = success ? 1 : 0;

		bool isCm = false;
		bool isLcm = false;
		JsonBoolAfterKey(src, "isCm", isCm);
		if (!isCm)
			JsonBoolAfterKey(src, "isCM", isCm);
		JsonBoolAfterKey(src, "isLegendaryCm", isLcm);
		if (!isLcm)
			JsonBoolAfterKey(src, "isLCM", isLcm);
		if (isLcm)
			e.mode = "LCM";
		else if (isCm)
			e.mode = "CM";

		long long durSec = 0;
		if (JsonLongAfterKey(src, "duration", durSec) && durSec > 0)
			e.durationMs = durSec * 1000;

		long long comp = 0;
		if (JsonLongAfterKey(src, "compDps", comp) && comp > 0)
			e.compDps = static_cast<int>(comp);

		long long encTime = 0;
		if (JsonLongAfterKey(resp.c_str(), "encounterTime", encTime) && encTime > 0)
			e.encounterTime = static_cast<time_t>(encTime);

		/* Metadata has names only — never replace a squad that already has DPS/boons. */
		if (e.players.empty() || PlayersNeedCombatStats(e.players))
		{
			if (e.players.empty() || !PlayersHaveDps(e.players))
				ParseDpsReportPlayers(resp.c_str(), e.players);
		}
		if (!e.encounter.empty() || e.result >= 0 || e.durationMs > 0)
		{
			e.parseError.clear();
			if (e.state != ParseState::Uploaded)
				e.state = ParseState::Parsed;
		}
	}

	bool FetchEiJsonFromReport(const std::string& permalink, std::string& json, std::string& err)
	{
		json.clear();
		if (permalink.empty())
		{
			err = "No permalink.";
			return false;
		}
		std::string path = "/getJson?permalink=";
		path += UrlEncode(PermalinkQueryValue(permalink));
		/* EI JSON can be large — allow a longer read. */
		const std::wstring host = Utf8ToWide("dps.report");
		const std::wstring pathW = Utf8ToWide(path.c_str());
		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			err = "WinHTTP open failed.";
			return false;
		}
		WinHttpSetTimeouts(session, 20000, 20000, 20000, 180000);
		HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			err = "Connect failed.";
			return false;
		}
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", pathW.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "OpenRequest failed.";
			return false;
		}
		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
		if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
			!WinHttpReceiveResponse(req, nullptr))
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "getJson request failed.";
			return false;
		}
		char chunk[8192];
		DWORD n = 0;
		while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
		{
			json.append(chunk, n);
			if (json.size() > 60 * 1024 * 1024)
			{
				WinHttpCloseHandle(req);
				WinHttpCloseHandle(conn);
				WinHttpCloseHandle(session);
				err = "getJson too large.";
				json.clear();
				return false;
			}
		}
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		if (json.find("\"players\"") == std::string::npos && json.find("\"Players\"") == std::string::npos)
		{
			err = "getJson missing players.";
			return false;
		}
		return true;
	}

	bool HttpGetSimple(const char* hostA, const char* pathAndQuery, std::string& body, std::string& err)
	{
		body.clear();
		err.clear();
		const std::wstring host = Utf8ToWide(hostA);
		const std::wstring path = Utf8ToWide(pathAndQuery);
		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			err = "WinHTTP open failed.";
			return false;
		}
		WinHttpSetTimeouts(session, 15000, 15000, 15000, 30000);
		HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			err = "Connect failed.";
			return false;
		}
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "OpenRequest failed.";
			return false;
		}
		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
		if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
			!WinHttpReceiveResponse(req, nullptr))
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "Request failed.";
			return false;
		}
		char chunk[4096];
		DWORD n = 0;
		while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
			body.append(chunk, n);
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return !body.empty();
	}

	bool FetchDpsReportMeta(const std::string& permalink, std::string& resp, std::string& err)
	{
		resp.clear();
		if (permalink.empty())
		{
			err = "No permalink.";
			return false;
		}
		std::string path = "/getUploadMetadata?permalink=";
		path += UrlEncode(PermalinkQueryValue(permalink));
		if (!HttpGetSimple("dps.report", path.c_str(), resp, err))
			return false;
		if (resp.find("\"permalink\"") == std::string::npos &&
			resp.find("\"encounter\"") == std::string::npos)
		{
			err = "Metadata response unexpected.";
			return false;
		}
		return true;
	}

	bool UploadToDpsReport(const std::wstring& filePath, std::string& respOut, std::string& err)
	{
		respOut.clear();
		err.clear();
		HANDLE h = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
		{
			err = "Cannot open log file.";
			return false;
		}
		LARGE_INTEGER sz{};
		GetFileSizeEx(h, &sz);
		if (sz.QuadPart <= 0 || sz.QuadPart > 120 * 1024 * 1024)
		{
			CloseHandle(h);
			err = "Log file size invalid.";
			return false;
		}
		std::string bodyFile(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD got = 0;
		const BOOL rok = ReadFile(h, bodyFile.data(), static_cast<DWORD>(bodyFile.size()), &got, nullptr);
		CloseHandle(h);
		if (!rok)
		{
			err = "Read failed.";
			return false;
		}
		bodyFile.resize(got);

		std::wstring fileName = filePath;
		const auto slash = fileName.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
			fileName = fileName.substr(slash + 1);
		const std::string fileNameUtf8 = WideToUtf8(fileName);

		char boundary[64];
		std::snprintf(boundary, sizeof(boundary), "----gw2igh%lx%lx",
			static_cast<unsigned long>(GetCurrentProcessId()),
			static_cast<unsigned long>(GetTickCount()));

		std::string form;
		form.reserve(bodyFile.size() + 512);
		form += "--";
		form += boundary;
		form += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
		form += fileNameUtf8;
		form += "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
		form += bodyFile;
		form += "\r\n--";
		form += boundary;
		form += "--\r\n";

		std::string url = "https://dps.report/uploadContent?json=1";
		if (G::DpsReportToken[0])
		{
			url += "&userToken=";
			url += G::DpsReportToken;
		}

		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			err = "WinHTTP open failed.";
			return false;
		}
		WinHttpSetTimeouts(session, kUploadTimeoutMs, kUploadTimeoutMs, kUploadTimeoutMs, kUploadTimeoutMs);

		HINTERNET conn = WinHttpConnect(session, L"dps.report", INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			err = "Connect failed.";
			return false;
		}

		std::string pathQuery = "/uploadContent?json=1";
		if (G::DpsReportToken[0])
		{
			pathQuery += "&userToken=";
			pathQuery += G::DpsReportToken;
		}
		const std::wstring pathW = Utf8ToWide(pathQuery.c_str());

		HINTERNET req = WinHttpOpenRequest(conn, L"POST", pathW.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "OpenRequest failed.";
			return false;
		}

		std::string ctype = "multipart/form-data; boundary=";
		ctype += boundary;
		const std::wstring ctypeW = Utf8ToWide(ctype.c_str());
		std::wstring headers = L"Content-Type: " + ctypeW + L"\r\n";

		BOOL ok = WinHttpSendRequest(req, headers.c_str(), static_cast<DWORD>(-1),
			form.data(), static_cast<DWORD>(form.size()), static_cast<DWORD>(form.size()), 0);
		if (!ok || !WinHttpReceiveResponse(req, nullptr))
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "Upload request failed.";
			return false;
		}

		std::string resp;
		char chunk[4096];
		DWORD n = 0;
		while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
			resp.append(chunk, n);

		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);

		std::string permalink;
		if (!JsonStringAfterKey(resp.c_str(), "permalink", permalink) || permalink.empty())
		{
			std::string apiErr;
			JsonStringAfterKey(resp.c_str(), "error", apiErr);
			err = apiErr.empty() ? "Upload failed (no permalink)." : apiErr;
			return false;
		}
		respOut = std::move(resp);
		(void)url;
		return true;
	}

	DWORD WINAPI UploadWorker(LPVOID)
	{
		std::vector<std::string> queue;
		{
			std::lock_guard<std::mutex> lock(gMu);
			queue.swap(gUploadQueue);
		}
		gUploadTotal.store(static_cast<int>(queue.size()));
		gUploadDone.store(0);

		for (const std::string& path : queue)
		{
			if (gCancel.load())
				break;
			std::wstring pathW = Utf8ToWide(path.c_str());
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 == path)
					{
						e.state = ParseState::Uploading;
						break;
					}
				}
				gGen.fetch_add(1);
			}

			std::string resp, err;
			const bool ok = UploadToDpsReport(pathW, resp, err);
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 != path)
						continue;
					if (ok)
					{
						e.state = ParseState::Uploaded;
						ApplyDpsReportMeta(e, resp);
						e.parseError.clear();
					}
					else
					{
						e.parseError = err;
						if (e.state == ParseState::Uploading)
							e.state = e.encounter.empty() ? ParseState::Pending : ParseState::Parsed;
					}
					break;
				}
				gUploadDone.fetch_add(1);
				SaveCacheLocked();
				gGen.fetch_add(1);
			}
		}
		std::snprintf(gStatus, sizeof(gStatus), "Upload finished (%d).", gUploadDone.load());
		gUploadBusy.store(false);
		return 0;
	}

	DWORD WINAPI HydrateWorker(LPVOID)
	{
		const bool force = gHydrateForce.exchange(false);
		std::vector<std::pair<std::string, std::string>> jobs; /* path, permalink */
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (const auto& e : gLogs)
			{
				if (e.dpsReportUrl.empty())
					continue;
				if (!force)
				{
					const bool needBasics =
						e.encounter.empty() || e.result < 0 || e.durationMs <= 0 || e.players.empty();
					const bool needStats = PlayersNeedCombatStats(e.players);
					if (!needBasics && !needStats)
						continue;
				}
				jobs.emplace_back(e.pathUtf8, e.dpsReportUrl);
			}
		}
		int done = 0;
		int jsonOk = 0;
		int jsonFail = 0;
		for (const auto& job : jobs)
		{
			if (gCancel.load())
				break;
			std::string resp, err;
			std::snprintf(gStatus, sizeof(gStatus), "Loading report stats %d / %d…",
				done + 1, static_cast<int>(jobs.size()));
			if (FetchDpsReportMeta(job.second, resp, err))
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 != job.first)
						continue;
					ApplyDpsReportMeta(e, resp);
					break;
				}
				gGen.fetch_add(1);
			}

			/* Full EI JSON from dps.report — DPS + boon uptimes + guild IDs. */
			std::string eiJson;
			if (FetchEiJsonFromReport(job.second, eiJson, err))
			{
				std::lock_guard<std::mutex> lock(gMu);
				for (auto& e : gLogs)
				{
					if (e.pathUtf8 != job.first)
						continue;
					ApplyEiJsonToEntry(e, eiJson);
					if (!e.dpsReportUrl.empty())
						e.state = ParseState::Uploaded;
					break;
				}
				gGen.fetch_add(1);
				++jsonOk;
			}
			else
			{
				++jsonFail;
				if (!err.empty())
				{
					std::lock_guard<std::mutex> lock(gMu);
					for (auto& e : gLogs)
					{
						if (e.pathUtf8 != job.first)
							continue;
						if (e.parseError.empty())
							e.parseError = err;
						break;
					}
				}
			}
			++done;
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			SaveCacheLocked();
			gGen.fetch_add(1);
		}
		if (jobs.empty())
			std::snprintf(gStatus, sizeof(gStatus), "No reports to load (upload first).");
		else if (jsonFail > 0)
			std::snprintf(gStatus, sizeof(gStatus),
				"Loaded %d report(s); %d getJson ok, %d failed.", done, jsonOk, jsonFail);
		else
		std::snprintf(gStatus, sizeof(gStatus),
			"Loaded DPS/boons/guilds for %d report(s).", jsonOk);
		gHydrateBusy.store(false);
		return 0;
	}

	void BeginHydrateFromReports(bool force)
	{
		if (gHydrateBusy.exchange(true))
			return;
		gHydrateForce.store(force);
		gCancel.store(false);
		std::snprintf(gStatus, sizeof(gStatus),
			force ? "Refreshing DPS/boons from dps.report…" : "Loading metadata from dps.report…");
		if (gHydrateThread)
		{
			CloseHandle(gHydrateThread);
			gHydrateThread = nullptr;
		}
		gHydrateThread = CreateThread(nullptr, 0, HydrateWorker, nullptr, 0, nullptr);
		if (!gHydrateThread)
		{
			gHydrateBusy.store(false);
			gHydrateForce.store(false);
		}
	}

	void BeginUpload(const std::vector<std::string>& paths)
	{
		if (paths.empty())
			return;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (const auto& p : paths)
				gUploadQueue.push_back(p);
		}
		if (gUploadBusy.exchange(true))
			return;
		std::snprintf(gStatus, sizeof(gStatus), "Uploading to dps.report…");
		if (gUploadThread)
		{
			CloseHandle(gUploadThread);
			gUploadThread = nullptr;
		}
		gUploadThread = CreateThread(nullptr, 0, UploadWorker, nullptr, 0, nullptr);
		if (!gUploadThread)
			gUploadBusy.store(false);
	}

	/* ---------- filters / aggregates ---------- */

	void BuildPlayerAggs(const std::vector<const LogEntry*>& filtered, std::vector<PlayerAgg>& out)
	{
		std::unordered_map<std::string, PlayerAgg> map;
		for (const LogEntry* e : filtered)
		{
			std::unordered_map<std::string, bool> seen;
			for (const PlayerInfo& p : e->players)
			{
				const std::string key = !p.account.empty() ? p.account : p.name;
				if (key.empty() || seen[key])
					continue;
				seen[key] = true;
				PlayerAgg& a = map[key];
				a.account = key;
				if (a.displayName.empty())
					a.displayName = p.name.empty() ? key : p.name;
				if (a.profession.empty())
					a.profession = p.profession;
				a.logs += 1;
				if (e->result == 1)
					a.success += 1;
			}
		}
		out.clear();
		out.reserve(map.size());
		for (auto& kv : map)
			out.push_back(std::move(kv.second));
		std::sort(out.begin(), out.end(), [](const PlayerAgg& a, const PlayerAgg& b) {
			if (a.logs != b.logs)
				return a.logs > b.logs;
			return a.account < b.account;
		});
	}

	void BuildGuildAggs(const std::vector<const LogEntry*>& filtered, std::vector<GuildAgg>& out)
	{
		struct Acc
		{
			std::string label;
			int logs = 0;
			std::unordered_map<std::string, bool> players;
		};
		std::unordered_map<std::string, Acc> map;
		for (const LogEntry* e : filtered)
		{
			std::unordered_map<std::string, bool> seenGuild;
			for (const PlayerInfo& p : e->players)
			{
				std::string key = p.guildTag;
				std::string label = p.guildTag;
				if (key.empty() && !p.guildId.empty())
				{
					key = p.guildId;
					label = p.guildId.size() > 8 ? p.guildId.substr(0, 8) + "…" : p.guildId;
				}
				if (key.empty())
					continue;
				if (!seenGuild[key])
				{
					seenGuild[key] = true;
					map[key].logs += 1;
					map[key].label = label;
				}
				const std::string pk = !p.account.empty() ? p.account : p.name;
				if (!pk.empty())
					map[key].players[pk] = true;
			}
		}
		out.clear();
		for (auto& kv : map)
		{
			GuildAgg g;
			g.key = kv.first;
			g.label = kv.second.label;
			g.logs = kv.second.logs;
			g.players = static_cast<int>(kv.second.players.size());
			out.push_back(std::move(g));
		}
		std::sort(out.begin(), out.end(), [](const GuildAgg& a, const GuildAgg& b) {
			if (a.logs != b.logs)
				return a.logs > b.logs;
			return a.label < b.label;
		});
	}

	void BuildFastest(const std::vector<const LogEntry*>& filtered, std::vector<FastestKill>& out)
	{
		std::unordered_map<std::string, FastestKill> best;
		for (const LogEntry* e : filtered)
		{
			if (e->result != 1 || e->durationMs <= 0 || e->encounter.empty())
				continue;
			auto it = best.find(e->encounter);
			if (it == best.end() || e->durationMs < it->second.durationMs)
			{
				FastestKill fk;
				fk.encounter = e->encounter;
				fk.durationMs = e->durationMs;
				fk.fileName = e->fileName;
				fk.pathUtf8 = e->pathUtf8;
				best[e->encounter] = std::move(fk);
			}
		}
		out.clear();
		out.reserve(best.size());
		for (auto& kv : best)
			out.push_back(std::move(kv.second));
		std::sort(out.begin(), out.end(), [](const FastestKill& a, const FastestKill& b) {
			return a.encounter < b.encounter;
		});
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
			if (gEncounterFilter[0] && !ContainsI(e.encounter, gEncounterFilter))
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

	void DrawBusyOrStatus()
	{
		if (gEiInstallBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gEiStatus[0] ? gEiStatus : "Installing Elite Insights…");
		else if (gScanBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Scanning…");
		else if (gParseBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Parsing %d / %d…",
				gParseDone.load(), gParseTotal.load());
		else if (gUploadBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Uploading %d / %d…",
				gUploadDone.load(), gUploadTotal.load());
		else if (gHydrateBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gStatus[0] ? gStatus : "Loading report metadata…");
		else if (gStatus[0])
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", gStatus);
	}

	void DrawToolbar(const std::vector<const LogEntry*>& filtered, bool hasDotNet)
	{
		if (ImGui::Button("Rescan###gw2igh_lm_scan"))
			BeginScan();
		ImGui::SameLine(0.f, 4.f);

		const bool canParse = hasDotNet && !gParseBusy.load() && !gScanBusy.load() &&
			!gEiInstallBusy.load() && G::EliteInsightsPath[0] && PathExistsUtf8(G::EliteInsightsPath);
		if (canParse)
		{
			if (ImGui::Button("Parse pending###gw2igh_lm_parse"))
				BeginParsePending();
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Parse pending###gw2igh_lm_parse");
			ImGui::PopStyleVar();
		}
		ImGui::SameLine(0.f, 4.f);

		if (ImGui::Button("Upload filtered###gw2igh_lm_upall"))
		{
			std::vector<std::string> paths;
			paths.reserve(filtered.size());
			for (const LogEntry* e : filtered)
				paths.push_back(e->pathUtf8);
			BeginUpload(paths);
		}
		ImGui::SameLine(0.f, 4.f);

		if (gHydrateBusy.load())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Load DPS/boons###gw2igh_lm_hydrate");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Load DPS/boons###gw2igh_lm_hydrate"))
			BeginHydrateFromReports();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Fill encounter, DPS, and boon uptimes from dps.report (getJson). No re-upload.");
		ImGui::SameLine(0.f, 4.f);

		if (ImGui::Button("Open folder###gw2igh_lm_openfolder"))
		{
			const std::wstring w = Utf8ToWide(G::LogFolder);
			if (!w.empty())
				ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}

		if (!hasDotNet)
		{
			ImGui::SameLine(0.f, 8.f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.12f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.42f, 0.15f, 1.f));
			if (ImGui::Button("Needs .NET 8###gw2igh_lm_neednet"))
				gFocusSetupTab = true;
			ImGui::PopStyleColor(2);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Elite Insights needs .NET 8 Desktop Runtime — open Setup.");
		}

		ImGui::SameLine(0.f, 10.f);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "%d shown / %d",
			static_cast<int>(filtered.size()), static_cast<int>(gDraw.size()));
		ImGui::SameLine(0.f, 10.f);
		DrawBusyOrStatus();
	}

	void DrawFilterPane()
	{
		ImGui::TextUnformatted("Filters");
		ImGui::Separator();
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("###gw2igh_lm_search", "Search…", gSearch, sizeof(gSearch));
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("###gw2igh_lm_enc", "Encounter…", gEncounterFilter, sizeof(gEncounterFilter));
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Result");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::Combo("###gw2igh_lm_res", &gResultFilter, "All\0Kills\0Fails\0Unknown\0");
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Mode");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::Combo("###gw2igh_lm_mode", &gModeFilter, "All\0Normal\0CM\0LCM\0");
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Time");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::Combo("###gw2igh_lm_days", &gDaysCombo,
			"All time\0Last 1 day\0Last 3 days\0Last 7 days\0Last 30 days\0");
		ImGui::Spacing();
		if (ImGui::Checkbox("Group by encounter###gw2igh_lm_groupby", &G::LogManagerGroupByEncounter))
			Settings::SetDirty();
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			G::LogManagerGroupByEncounter
				? "Grouped · newest first"
				: "Flat list");
		ImGui::Spacing();
		if (ImGui::SmallButton("Clear filters###gw2igh_lm_clearf"))
		{
			gSearch[0] = 0;
			gEncounterFilter[0] = 0;
			gResultFilter = static_cast<int>(ResultFilter::All);
			gModeFilter = static_cast<int>(ModeFilter::All);
			gDaysCombo = 0;
		}
	}

	void SelectLogByPath(const std::string& pathUtf8)
	{
		for (int j = 0; j < static_cast<int>(gDraw.size()); ++j)
		{
			if (gDraw[static_cast<size_t>(j)].pathUtf8 == pathUtf8)
			{
				gSelected = j;
				break;
			}
		}
	}

	void DrawLogEntryRow(const LogEntry* e, bool showEncounter)
	{
		if (!e)
			return;
		ImGui::PushID(e->pathUtf8.c_str());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		const bool sel = (gSelected >= 0 && gSelected < static_cast<int>(gDraw.size()) &&
			gDraw[static_cast<size_t>(gSelected)].pathUtf8 == e->pathUtf8);
		char label[48];
		std::snprintf(label, sizeof(label), "%s", FmtTime(e->encounterTime).c_str());
		if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_SpanAllColumns))
			SelectLogByPath(e->pathUtf8);
		if (showEncounter)
		{
			ImGui::TableNextColumn();
			if (!e->encounter.empty())
				ImGui::TextUnformatted(e->encounter.c_str());
			else
				ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f), "%s", e->fileName.c_str());
		}
		ImGui::TableNextColumn();
		if (e->result == 1)
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.f), "Kill");
		else if (e->result == 0)
			ImGui::TextColored(ImVec4(0.90f, 0.45f, 0.40f, 1.f), "Fail");
		else if (e->state == ParseState::Pending)
			ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.40f, 1.f), "…");
		else
			ImGui::TextUnformatted(ResultLabel(e->result));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(e->mode.empty() ? "-" : e->mode.c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(FmtDuration(e->durationMs).c_str());
		ImGui::TableNextColumn();
		if (e->compDps > 0)
			ImGui::Text("%d", e->compDps);
		else
			ImGui::TextUnformatted("-");
		ImGui::TableNextColumn();
		ImGui::Text("%d", static_cast<int>(e->players.size()));
		ImGui::PopID();
	}

	time_t LogSortTime(const LogEntry* e)
	{
		if (!e)
			return 0;
		if (e->encounterTime > 0)
			return e->encounterTime;
		return FileTimeToUnix(e->mtime);
	}

	void DrawLogTableGrouped(const std::vector<const LogEntry*>& filtered)
	{
		struct EncGroup
		{
			std::string key;
			std::string label;
			std::vector<const LogEntry*> logs;
			time_t lastTime = 0;
			long long bestKillMs = 0;
			int kills = 0;
		};

		std::unordered_map<std::string, EncGroup> map;
		map.reserve(filtered.size());
		for (const LogEntry* e : filtered)
		{
			if (!e)
				continue;
			std::string key = e->encounter;
			std::string label = e->encounter;
			if (key.empty())
			{
				key = "\x01unknown";
				label = "Unknown encounter";
			}
			EncGroup& g = map[key];
			if (g.key.empty())
			{
				g.key = key;
				g.label = label;
			}
			g.logs.push_back(e);
			const time_t t = LogSortTime(e);
			if (t > g.lastTime)
				g.lastTime = t;
			if (e->result == 1)
			{
				g.kills += 1;
				if (e->durationMs > 0 && (g.bestKillMs <= 0 || e->durationMs < g.bestKillMs))
					g.bestKillMs = e->durationMs;
			}
		}

		std::vector<EncGroup*> groups;
		groups.reserve(map.size());
		for (auto& kv : map)
			groups.push_back(&kv.second);
		std::sort(groups.begin(), groups.end(), [](const EncGroup* a, const EncGroup* b) {
			if (a->lastTime != b->lastTime)
				return a->lastTime > b->lastTime;
			return a->label < b->label;
		});

		ImGui::BeginChild("###gw2igh_lm_groupscroll", ImVec2(-FLT_MIN, -FLT_MIN), false);
		for (EncGroup* g : groups)
		{
			std::sort(g->logs.begin(), g->logs.end(), [](const LogEntry* a, const LogEntry* b) {
				return LogSortTime(a) > LogSortTime(b);
			});

			const size_t idHash = std::hash<std::string>{}(g->key);
			char header[288];
			if (g->bestKillMs > 0)
			{
				std::snprintf(header, sizeof(header),
					"%s  (%d)  ·  %d kill%s  ·  best %s  ·  last %s###gw2igh_enc_%zu",
					g->label.c_str(),
					static_cast<int>(g->logs.size()),
					g->kills, g->kills == 1 ? "" : "s",
					FmtDuration(g->bestKillMs).c_str(),
					FmtTime(g->lastTime).c_str(),
					idHash);
			}
			else
			{
				std::snprintf(header, sizeof(header),
					"%s  (%d)  ·  last %s###gw2igh_enc_%zu",
					g->label.c_str(),
					static_cast<int>(g->logs.size()),
					FmtTime(g->lastTime).c_str(),
					idHash);
			}

			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.17f, 0.14f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.22f, 0.16f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.26f, 0.18f, 1.f));
			const bool open = ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::PopStyleColor(3);
			if (!open)
				continue;

			if (ImGui::BeginTable("###gw2igh_lm_gtable", 6,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 0.28f);
				ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 0.12f);
				ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 0.10f);
				ImGui::TableSetupColumn("Dur", ImGuiTableColumnFlags_WidthStretch, 0.14f);
				ImGui::TableSetupColumn("Squad", ImGuiTableColumnFlags_WidthStretch, 0.14f);
				ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch, 0.06f);
				ImGui::TableHeadersRow();
				for (const LogEntry* e : g->logs)
					DrawLogEntryRow(e, false);
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}

	void DrawLogTable(const std::vector<const LogEntry*>& filtered)
	{
		if (filtered.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f), "No logs match filters.");
			return;
		}

		if (G::LogManagerGroupByEncounter)
		{
			DrawLogTableGrouped(filtered);
			return;
		}

		const ImVec2 tableSize(-FLT_MIN, -FLT_MIN);
		if (ImGui::BeginTable("###gw2igh_lm_table", 7,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				tableSize))
		{
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 0.22f);
			ImGui::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthStretch, 0.36f);
			ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 0.08f);
			ImGui::TableSetupColumn("Dur", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("Squad", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch, 0.04f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (const LogEntry* e : filtered)
				DrawLogEntryRow(e, true);
			ImGui::EndTable();
		}
	}

	void DrawDetailTab()
	{
		const LogEntry* sel = nullptr;
		if (gSelected >= 0 && gSelected < static_cast<int>(gDraw.size()))
			sel = &gDraw[static_cast<size_t>(gSelected)];

		if (!sel)
		{
			ImGui::TextWrapped("Select a log from the list.");
			return;
		}

		ImGui::TextWrapped("%s", sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "%s", sel->fileName.c_str());
		ImGui::Text("Result: %s  Mode: %s  Duration: %s",
			ResultLabel(sel->result),
			sel->mode.empty() ? "Normal" : sel->mode.c_str(),
			FmtDuration(sel->durationMs).c_str());
		ImGui::Text("Time: %s", FmtTime(sel->encounterTime).c_str());
		if (sel->compDps > 0)
			ImGui::Text("Squad DPS: %d", sel->compDps);
		if (!sel->parseError.empty())
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1.f), "%s", sel->parseError.c_str());

		if (ImGui::Button("Parse###gw2igh_lm_parsesel"))
			BeginParseSelected(sel->pathUtf8);
		ImGui::SameLine();
		if (ImGui::Button("Upload###gw2igh_lm_upsel"))
			BeginUpload({sel->pathUtf8});
		ImGui::SameLine();
		if (ImGui::Button("Folder###gw2igh_lm_folder"))
			OpenFolderFor(sel->pathW);

		if (!sel->dpsReportUrl.empty())
		{
			ImGui::TextWrapped("%s", sel->dpsReportUrl.c_str());
			if (ImGui::SmallButton("Open report###gw2igh_lm_openrep"))
				ShellExecuteA(nullptr, "open", sel->dpsReportUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy link###gw2igh_lm_copylink"))
				CopyText(sel->dpsReportUrl.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Load DPS/boons###gw2igh_lm_loadstats") && !gHydrateBusy.load())
				BeginHydrateFromReports();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Squad (DPS + boon uptimes %)");
		if (sel->players.empty())
			ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f),
				"No player data — Parse, Upload, or Load DPS/boons.");
		else if (!PlayersHaveDps(sel->players) && !PlayersHaveBoons(sel->players))
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"Names loaded — click Load DPS/boons (or Parse with EI).");
			if (ImGui::BeginTable("###gw2igh_lm_squad_basic", 4,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Account");
				ImGui::TableSetupColumn("Prof");
				ImGui::TableSetupColumn("G");
				ImGui::TableHeadersRow();
				for (const PlayerInfo& p : sel->players)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.account.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.profession.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%d", p.group);
				}
				ImGui::EndTable();
			}
		}
		else if (ImGui::BeginTable("###gw2igh_lm_squad", 10,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
					ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthFixed, 56.f);
			ImGui::TableSetupColumn("Pwr", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Con", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Quick", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("Alac", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Might", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("Fury", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Prot", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();
			auto pct = [](float v) {
				if (v < 0.f)
					return std::string("-");
				char b[16];
				std::snprintf(b, sizeof(b), "%.0f", static_cast<double>(v));
				return std::string(b);
			};
			for (const PlayerInfo& p : sel->players)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.dps);
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.powerDps);
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.condiDps);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.quickness).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.alacrity).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.might).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.fury).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.protection).c_str());
			}
			ImGui::EndTable();
		}
	}

	const LogEntry* SelectedDrawEntry()
	{
		if (gSelected < 0 || gSelected >= static_cast<int>(gDraw.size()))
			return nullptr;
		return &gDraw[static_cast<size_t>(gSelected)];
	}

	std::string GuildLabelFor(const PlayerInfo& p)
	{
		if (!p.guildTag.empty())
			return p.guildTag;
		if (!p.guildId.empty())
		{
			if (p.guildId.size() > 8)
				return p.guildId.substr(0, 8) + "…";
			return p.guildId;
		}
		return {};
	}

	void KickKillProofForSelected(const LogEntry* sel, bool force)
	{
		if (!sel)
			return;
		bool start = false;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (LogEntry& e : gLogs)
			{
				if (e.pathUtf8 == sel->pathUtf8)
				{
					start = EnsureKillProofForLog(e, force);
					gGen.fetch_add(1);
					break;
				}
			}
		}
		if (start || force)
			BeginKillProofFetch(force);
	}

	void DrawPlayersTab(const std::vector<const LogEntry*>& /*filtered*/)
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to see its squad players.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d players in this run", static_cast<int>(sel->players.size()));

		if (sel->players.empty())
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No player data — Parse or Load DPS/boons for this log.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_paggs", 6,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Guild", ImGuiTableColumnFlags_WidthStretch, 0.8f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthStretch, 0.35f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			for (const PlayerInfo& p : sel->players)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.account.empty() ? "-" : p.account.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.empty() ? "-" : p.name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.empty() ? "-" : p.profession.c_str());
				ImGui::TableNextColumn();
				if (p.dps > 0)
					ImGui::Text("%d", p.dps);
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				const std::string g = GuildLabelFor(p);
				ImGui::TextUnformatted(g.empty() ? "-" : g.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.group);
			}
			ImGui::EndTable();
		}
	}

	void DrawKillProofTab()
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to look up KillProof for its squad.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Public killproof.me profiles for this run");

		if (sel->players.empty())
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No player data — Parse or Load DPS/boons first so account names exist.");
			return;
		}

		int withAccount = 0, kpOk = 0, kpMissing = 0, kpPending = 0;
		for (const PlayerInfo& p : sel->players)
		{
			if (p.account.empty())
				continue;
			++withAccount;
			if (p.kpState == 2) ++kpOk;
			else if (p.kpState == 3 || p.kpState == 4) ++kpMissing;
			else ++kpPending;
		}

		if (gKillProofBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gStatus[0] ? gStatus : "Loading killproof.me…");
		else if (withAccount == 0)
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No account names — Load DPS/boons for full EI JSON.");
		else if (kpOk > 0 || kpMissing > 0)
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f),
				"%d loaded · %d none/private · %d pending",
				kpOk, kpMissing, kpPending);
		else
			ImGui::TextColored(ImVec4(0.75f, 0.70f, 0.45f, 1.f),
				"Click Load to fetch LI / LD / tokens from killproof.me.");

		const bool busy = gKillProofBusy.load();
		if (busy)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Loading…###gw2igh_lm_loadkp");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Load KillProof###gw2igh_lm_loadkp"))
			KickKillProofForSelected(sel, true);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip(
				"Fetch Legendary Insights, Divinations, UFE, and encounter tokens.\n"
				"Only public killproof.me profiles are available.");
		ImGui::SameLine();
		if (ImGui::SmallButton("killproof.me###gw2igh_lm_kpweb"))
			ShellExecuteA(nullptr, "open", "https://killproof.me/", nullptr, nullptr, SW_SHOWNORMAL);

		/* Auto-fill once when this tab is open and KP not loaded yet. */
		if (!busy && withAccount > 0)
		{
			bool need = false;
			for (const PlayerInfo& p : sel->players)
			{
				if (!p.account.empty() && p.kpState == 0)
				{
					need = true;
					break;
				}
			}
			if (need)
				KickKillProofForSelected(sel, false);
		}

		const char* kpCol = "Token";
		int bossId = 0;
		const char* bossLabel = nullptr;
		if (BossTokenForEncounter(sel->encounter, bossId, bossLabel) && bossLabel)
			kpCol = bossLabel;

		if (ImGui::BeginTable("###gw2igh_lm_kptab", 8,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
					ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.1f);
			ImGui::TableSetupColumn("LI", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("LD", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("UFE", ImGuiTableColumnFlags_WidthFixed, 52.f);
			ImGui::TableSetupColumn(kpCol, ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 0.9f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, 24.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			auto kpCell = [](int v, int state) {
				if (state == 1)
				{
					ImGui::TextColored(ImVec4(0.70f, 0.68f, 0.45f, 1.f), "…");
					return;
				}
				if (state == 3)
				{
					ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.48f, 1.f), "—");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("No public killproof.me profile");
					return;
				}
				if (state == 4)
				{
					ImGui::TextColored(ImVec4(0.90f, 0.50f, 0.40f, 1.f), "!");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("killproof.me request failed — try Load again");
					return;
				}
				if (state == 0 || v < 0)
				{
					ImGui::TextUnformatted("—");
					return;
				}
				ImGui::Text("%d", v);
			};

			std::vector<const PlayerInfo*> rows;
			rows.reserve(sel->players.size());
			for (const PlayerInfo& p : sel->players)
				rows.push_back(&p);
			std::sort(rows.begin(), rows.end(), [](const PlayerInfo* a, const PlayerInfo* b) {
				if (a->kpLi != b->kpLi)
					return a->kpLi > b->kpLi;
				return a->account < b->account;
			});

			for (const PlayerInfo* pp : rows)
			{
				const PlayerInfo& p = *pp;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (!p.kpUrl.empty() && !p.account.empty())
				{
					if (ImGui::Selectable(p.account.c_str(), false, ImGuiSelectableFlags_None))
						ShellExecuteA(nullptr, "open", p.kpUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Open killproof.me profile");
				}
				else
					ImGui::TextUnformatted(p.account.empty() ? "-" : p.account.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.empty() ? "-" : p.name.c_str());
				ImGui::TableNextColumn();
				kpCell(p.kpLi, p.kpState);
				ImGui::TableNextColumn();
				kpCell(p.kpLd, p.kpState);
				ImGui::TableNextColumn();
				kpCell(p.kpUfe, p.kpState);
				ImGui::TableNextColumn();
				if (bossId > 0)
					kpCell(p.kpBoss, p.kpState);
				else
					ImGui::TextUnformatted("—");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.empty() ? "-" : p.profession.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.group);
			}
			ImGui::EndTable();
		}
	}

	void DrawGuildsTab(const std::vector<const LogEntry*>& /*filtered*/)
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to see guilds in that run.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());

		struct Acc
		{
			std::string label;
			int players = 0;
			int dpsSum = 0;
		};
		std::unordered_map<std::string, Acc> map;
		for (const PlayerInfo& p : sel->players)
		{
			std::string key = p.guildTag;
			std::string label = p.guildTag;
			if (key.empty() && !p.guildId.empty())
			{
				key = p.guildId;
				label = p.guildId.size() > 8 ? p.guildId.substr(0, 8) + "…" : p.guildId;
			}
			if (key.empty())
				continue;
			Acc& a = map[key];
			a.label = label;
			a.players += 1;
			a.dpsSum += p.dps > 0 ? p.dps : 0;
		}

		std::vector<std::pair<std::string, Acc>> rows;
		rows.reserve(map.size());
		for (auto& kv : map)
			rows.emplace_back(kv.first, std::move(kv.second));
		std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
			if (a.second.players != b.second.players)
				return a.second.players > b.second.players;
			return a.second.label < b.second.label;
		});

		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d guilds in this run", static_cast<int>(rows.size()));

		if (rows.empty())
		{
			ImGui::TextWrapped(
				"No guilds on this squad after load. Click Load DPS/boons to refresh; "
				"players without a guild show as empty.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_gaggs", 3,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Guild", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Players", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("DPS sum", ImGuiTableColumnFlags_WidthStretch, 0.9f);
			ImGui::TableHeadersRow();
			for (const auto& row : rows)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.second.label.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", row.second.players);
				ImGui::TableNextColumn();
				if (row.second.dpsSum > 0)
					ImGui::Text("%d", row.second.dpsSum);
				else
					ImGui::TextUnformatted("-");
			}
			ImGui::EndTable();
		}
	}

	void DrawFastestTab(const std::vector<const LogEntry*>& filtered)
	{
		std::vector<FastestKill> kills;
		BuildFastest(filtered, kills);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Best kill time per encounter (filtered)");
		if (ImGui::BeginTable("###gw2igh_lm_fast", 3,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Encounter");
			ImGui::TableSetupColumn("Time");
			ImGui::TableSetupColumn("File");
			ImGui::TableHeadersRow();
			for (const FastestKill& k : kills)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(k.encounter.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FmtDuration(k.durationMs).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(k.fileName.c_str());
			}
			ImGui::EndTable();
		}
	}

	void DrawSetupTab(bool hasDotNet)
	{
		ImGui::TextWrapped(
			"Browse ArcDPS logs. Elite Insights auto-updates from GitHub latest (MIT, baaron4).");
		ImGui::Separator();

		if (!hasDotNet)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.72f, 0.35f, 1.f));
			ImGui::TextWrapped(
				".NET 8 Desktop Runtime not detected — Elite Insights cannot parse until it is installed.");
			ImGui::PopStyleColor();
			if (EiRuntime::IsWine())
			{
				ImGui::TextColored(ImVec4(0.75f, 0.70f, 0.45f, 1.f), "%s",
					"Proton/Wine: install into this game's Windows prefix — Linux distro packages will not work.");
			}
			if (ImGui::Button("Install .NET 8 Runtime###gw2igh_lm_dotnet_install"))
				EiRuntime::OpenDotNet8Installer();
			ImGui::SameLine();
			if (ImGui::Button("Recheck .NET###gw2igh_lm_dotnet_recheck"))
			{
				EiRuntime::InvalidateDotNet8Cache();
				std::snprintf(gStatus, sizeof(gStatus),
					EiRuntime::HasDotNet8Runtime()
						? ".NET 8 runtime found."
						: ".NET 8 still not detected.");
			}
			ImGui::Separator();
		}
		else
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), ".NET 8 Desktop Runtime detected.");

		ImGui::TextUnformatted("Log folder");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_folder", "arcdps.cbtlogs path",
				G::LogFolder, sizeof(G::LogFolder)))
			Settings::SetDirty();

		ImGui::TextUnformatted("Elite Insights CLI");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_ei",
				"Auto-filled after install, or custom path",
				G::EliteInsightsPath, sizeof(G::EliteInsightsPath)))
			Settings::SetDirty();

		ImGui::TextUnformatted("dps.report token (optional)");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_token", "User token",
				G::DpsReportToken, sizeof(G::DpsReportToken),
				ImGuiInputTextFlags_Password))
			Settings::SetDirty();

		ImGui::Spacing();
		if (gEiInstallBusy.load())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Install / Update EI###gw2igh_lm_eiinst");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Install / Update EI###gw2igh_lm_eiinst"))
			BeginEiEnsure(true);
		ImGui::SameLine();
		if (ImGui::Button("EI releases###gw2igh_lm_eihelp"))
			ShellExecuteA(nullptr, "open",
				"https://github.com/baaron4/GW2-Elite-Insights-Parser/releases",
				nullptr, nullptr, SW_SHOWNORMAL);
		ImGui::SameLine();
		if (ImGui::Button("Open log folder###gw2igh_lm_setup_folder"))
		{
			const std::wstring w = Utf8ToWide(G::LogFolder);
			if (!w.empty())
				ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}

		if (gEiInstallBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gEiStatus[0] ? gEiStatus : "Installing Elite Insights…");
	}
}

void LogManagerPad::OpenAndRefresh()
{
	G::ShowLogManager = true;
	gFocus = true;
	gPlaceOnce = true;
	gLogListFrac = G::LogManagerListFrac;
	EnsureDefaultPaths();
	Settings::SetDirty();
	BeginEiEnsure(false);
	BeginScan();
}

bool LogManagerPad::Render()
{
	if (!G::ShowLogManager)
		return false;

	EnsureDefaultPaths();
	SyncDraw();

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float displayW = display.x > 1.f ? display.x : kPadW;
	const float displayH = display.y > 1.f ? display.y : kPadH;

	if (gPlaceOnce)
	{
		float winW = G::LogManagerWinW;
		float winH = G::LogManagerWinH;
		/* First open (no saved pos): size for 1080p first — leave margin for game UI. */
		if (G::LogManagerWinX < 0.f || G::LogManagerWinY < 0.f)
		{
			winW = displayW * 0.67f;
			if (winW < 1000.f) winW = 1000.f;
			if (winW > 1360.f) winW = 1360.f; /* fits 1920×1080 with side chrome */

			winH = displayH * 0.62f;
			if (winH < 560.f) winH = 560.f;
			if (winH > 780.f) winH = 780.f;
		}
		/* Always clamp to current display (resolution changes / ultrawide → 1080p). */
		{
			const float maxW = displayW > 80.f ? displayW - 40.f : winW;
			const float maxH = displayH > 120.f ? displayH - 80.f : winH;
			if (winW > maxW) winW = maxW;
			if (winH > maxH) winH = maxH;
			if (winW < 880.f && displayW > 920.f) winW = 880.f;
			if (winH < 420.f && displayH > 480.f) winH = 420.f;
			if (winW > displayW * 0.98f) winW = displayW * 0.98f;
			if (winH > displayH * 0.95f) winH = displayH * 0.95f;
		}
		G::LogManagerWinW = winW;
		G::LogManagerWinH = winH;
		if (G::LogManagerWinX >= 0.f && G::LogManagerWinY >= 0.f)
			ImGui::SetNextWindowPos(ImVec2(G::LogManagerWinX, G::LogManagerWinY), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(ImVec2(40.f, 60.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
		gLogListFrac = G::LogManagerListFrac;
		gPlaceOnce = false;
	}

	{
		/* Usable on 1080p and smaller Proton windows; grow freely on 1440p+. */
		float minW = 960.f;
		float minH = 480.f;
		if (minW > displayW * 0.92f) minW = displayW * 0.92f;
		if (minH > displayH * 0.85f) minH = displayH * 0.85f;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(minW, minH),
			ImVec2(displayW * 0.98f, displayH * 0.95f));
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowLogManager;
	if (!ImGui::Begin("DPS Logs###gw2igh_logmgr", &open, ImGuiWindowFlags_None))
	{
		ImGui::End();
		if (!open)
		{
			G::ShowLogManager = false;
			Settings::SetDirty();
		}
		return false;
	}
	if (!open)
	{
		G::ShowLogManager = false;
		Settings::SetDirty();
		ImGui::End();
		return false;
	}

	const bool hasDotNet = EiRuntime::HasDotNet8Runtime();
	std::vector<const LogEntry*> filtered;
	CollectFiltered(filtered);

	DrawToolbar(filtered, hasDotNet);
	ImGui::Separator();

	const float bodyH = ImGui::GetContentRegionAvail().y;
	const float bodyW = ImGui::GetContentRegionAvail().x;
	float filterW = bodyW * kFilterFrac;
	if (filterW < kFilterMinW) filterW = kFilterMinW;
	if (filterW > kFilterMaxW) filterW = kFilterMaxW;
	/* Narrow / 1080p: keep filters compact so list + KillProof keep room. */
	if (bodyW < 1100.f)
	{
		filterW = bodyW * 0.15f;
		if (filterW < 132.f) filterW = 132.f;
		if (filterW > 168.f) filterW = 168.f;
	}
	if (filterW > bodyW * 0.22f)
		filterW = bodyW * 0.22f;

	ImGui::BeginChild("###gw2igh_lm_filters", ImVec2(filterW, bodyH), true);
	DrawFilterPane();
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	/* Log list | drag splitter | detail — fraction of remaining width; tables stretch inside. */
	const float availX = ImGui::GetContentRegionAvail().x;
	const float usable = availX - kSplitHitW - kPaneGap * 2.f;
	float listMin = kLogListMinW;
	float rightMin = kRightPaneMinW;
	if (usable > 1.f && usable < listMin + rightMin)
	{
		const float scale = usable / (listMin + rightMin);
		listMin *= scale;
		rightMin *= scale;
	}
	if (gLogListFrac < 0.20f)
		gLogListFrac = 0.20f;
	if (gLogListFrac > 0.72f)
		gLogListFrac = 0.72f;
	float centerW = usable * gLogListFrac;
	if (centerW < listMin)
		centerW = listMin;
	if (centerW > usable - rightMin)
		centerW = usable - rightMin;
	if (centerW < listMin)
		centerW = listMin;
	if (usable > 1.f)
		gLogListFrac = centerW / usable;

	ImGui::BeginChild("###gw2igh_lm_list", ImVec2(centerW, bodyH), true);
	DrawLogTable(filtered);
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	{
		const ImVec2 splitPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("###gw2igh_lm_split", ImVec2(kSplitHitW, bodyH));
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		if (hovered || active)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (active && usable > 1.f)
		{
			centerW += ImGui::GetIO().MouseDelta.x;
			if (centerW < listMin)
				centerW = listMin;
			if (centerW > usable - rightMin)
				centerW = usable - rightMin;
			gLogListFrac = centerW / usable;
			if (std::fabs(G::LogManagerListFrac - gLogListFrac) > 0.002f)
			{
				G::LogManagerListFrac = gLogListFrac;
				Settings::SetDirty();
			}
		}
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float midX = splitPos.x + kSplitHitW * 0.5f;
		const ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
			: (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
		dl->AddLine(ImVec2(midX, splitPos.y + 4.f),
			ImVec2(midX, splitPos.y + bodyH - 4.f), col, active ? 2.f : 1.f);
		if (hovered)
			ImGui::SetTooltip("Drag to resize panes — tables scale with width");
	}

	ImGui::SameLine(0.f, kPaneGap);
	ImGui::BeginChild("###gw2igh_lm_side", ImVec2(0.f, bodyH), true);
	if (ImGui::BeginTabBar("###gw2igh_lm_tabs"))
	{
		if (ImGui::BeginTabItem("Detail"))
		{
			DrawDetailTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Players"))
		{
			DrawPlayersTab(filtered);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("KillProof"))
		{
			DrawKillProofTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Guilds"))
		{
			DrawGuildsTab(filtered);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Fastest"))
		{
			DrawFastestTab(filtered);
			ImGui::EndTabItem();
		}
		{
			ImGuiTabItemFlags setupFlags = 0;
			if (gFocusSetupTab)
			{
				setupFlags = ImGuiTabItemFlags_SetSelected;
				gFocusSetupTab = false;
			}
			if (ImGui::BeginTabItem("Setup", nullptr, setupFlags))
			{
				DrawSetupTab(hasDotNet);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	{
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 sz = ImGui::GetWindowSize();
		const bool moved =
			std::fabs(pos.x - G::LogManagerWinX) > 0.5f ||
			std::fabs(pos.y - G::LogManagerWinY) > 0.5f ||
			std::fabs(sz.x - G::LogManagerWinW) > 0.5f ||
			std::fabs(sz.y - G::LogManagerWinH) > 0.5f;
		if (moved)
		{
			G::LogManagerWinX = pos.x;
			G::LogManagerWinY = pos.y;
			G::LogManagerWinW = sz.x;
			G::LogManagerWinH = sz.y;
			Settings::SetDirty();
		}
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	ImGui::End();
	return hovered;
}
