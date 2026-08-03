#include "VaultPad.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kHttpTimeoutMs = 3500;
	constexpr int kBulkTimeoutMs = 8000;
	constexpr DWORD kCacheTtlMs = 3 * 60 * 1000;
	constexpr float kPadW = 480.f;
	constexpr float kPadH = 600.f;

	struct Obj
	{
		std::string title;
		std::string track;
		int cur = 0;
		int need = 0;
		int acclaim = 0;
		bool done = false;
	};

	struct Snapshot
	{
		bool ok = false;
		bool hasKey = false;
		bool scopeFail = false;
		std::string status;
		std::string seasonTitle;
		std::string seasonBlurb;
		std::vector<Obj> daily;
		std::vector<Obj> weekly;
		std::vector<Obj> special;
		std::vector<Obj> easyPreview; /* no key */
		DWORD fetchedAt = 0;
	};

	std::mutex gMu;
	Snapshot gSnap;
	Snapshot gDraw;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	std::atomic<bool> gBusy{false};
	HANDLE gThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;

	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		if (openBrace >= json.size() || json[openBrace] != '{')
			return std::string::npos;
		int depth = 0;
		bool inStr = false, esc = false;
		for (size_t i = openBrace; i < json.size(); ++i)
		{
			char c = json[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') inStr = true;
			else if (c == '{') ++depth;
			else if (c == '}')
			{
				--depth;
				if (depth == 0) return i;
			}
		}
		return std::string::npos;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= json.size() || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			char c = json[k++];
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size()) k += 4;
				else out.push_back(e);
				continue;
			}
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		bool neg = false;
		if (k < json.size() && json[k] == '-') { neg = true; ++k; }
		long long v = 0;
		bool any = false;
		while (k < json.size() && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any) return -1;
		return neg ? -v : v;
	}

	bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return false;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return false;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		return k + 4 <= json.size() && json.compare(k, 4, "true") == 0;
	}

	std::string FormatIsoDateUtc(const std::string& iso)
	{
		int y = 0, mo = 0, d = 0;
		if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 ||
			y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31)
			return iso;
		static const char* kMonths[] = {
			"January", "February", "March", "April", "May", "June",
			"July", "August", "September", "October", "November", "December"
		};
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%s %d, %d", kMonths[mo - 1], d, y);
		return buf;
	}

	bool ParseIsoUtc(const std::string& iso, time_t* out)
	{
		int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
		if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) < 3)
			return false;
		struct tm tm{};
		tm.tm_year = y - 1900;
		tm.tm_mon = mo - 1;
		tm.tm_mday = d;
		tm.tm_hour = h;
		tm.tm_min = mi;
		tm.tm_sec = s;
		*out = _mkgmtime(&tm);
		return *out != (time_t)-1;
	}

	std::string SeasonBlurb(const std::string& /*startIso*/, const std::string& endIso)
	{
		std::string out;
		time_t endT = 0;
		const bool hasEnd = !endIso.empty() && ParseIsoUtc(endIso, &endT);
		const time_t now = time(nullptr);
		if (hasEnd)
		{
			out = "Ends ";
			out += FormatIsoDateUtc(endIso);
			const double days = difftime(endT, now) / 86400.0;
			if (days >= 0)
			{
				const int n = static_cast<int>(days + 0.999);
				out += " · ";
				out += std::to_string(n);
				out += (n == 1 ? " day left" : " days left");
			}
			else
				out += " · ended";
		}
		if (out.empty())
			out = "Season dates unavailable.";
		return out;
	}

	/* GW2 daily = 00:00 UTC. Weekly = Monday 07:30 UTC. */
	std::string FormatCountdown(long long sec)
	{
		if (sec < 0) sec = 0;
		const long long d = sec / 86400;
		const long long h = (sec % 86400) / 3600;
		const long long m = (sec % 3600) / 60;
		const long long s = sec % 60;
		char buf[64];
		if (d > 0)
			std::snprintf(buf, sizeof(buf), "%lldd %lldh %lldm", d, h, m);
		else if (h > 0)
			std::snprintf(buf, sizeof(buf), "%lldh %lldm", h, m);
		else
			std::snprintf(buf, sizeof(buf), "%lldm %llds", m, s);
		return buf;
	}

	void UtcNowParts(int& y, int& mo, int& d, int& h, int& mi, int& s, int& wday)
	{
		const time_t now = time(nullptr);
		struct tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &now);
#else
		gmtime_r(&now, &tm);
#endif
		y = tm.tm_year + 1900;
		mo = tm.tm_mon + 1;
		d = tm.tm_mday;
		h = tm.tm_hour;
		mi = tm.tm_min;
		s = tm.tm_sec;
		wday = tm.tm_wday; /* 0=Sun … 6=Sat */
	}

	time_t MakeUtc(int y, int mo, int d, int h, int mi, int s)
	{
		struct tm tm{};
		tm.tm_year = y - 1900;
		tm.tm_mon = mo - 1;
		tm.tm_mday = d;
		tm.tm_hour = h;
		tm.tm_min = mi;
		tm.tm_sec = s;
		return _mkgmtime(&tm);
	}

	void AddUtcDays(int& y, int& mo, int& d, int days)
	{
		const time_t t = MakeUtc(y, mo, d, 12, 0, 0) + static_cast<time_t>(days) * 86400;
		struct tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		y = tm.tm_year + 1900;
		mo = tm.tm_mon + 1;
		d = tm.tm_mday;
	}

	long long SecUntilDailyResetUtc()
	{
		int y, mo, d, h, mi, s, wday;
		UtcNowParts(y, mo, d, h, mi, s, wday);
		(void)wday;
		time_t next = MakeUtc(y, mo, d, 0, 0, 0);
		const time_t now = time(nullptr);
		if (next <= now)
		{
			AddUtcDays(y, mo, d, 1);
			next = MakeUtc(y, mo, d, 0, 0, 0);
		}
		return static_cast<long long>(difftime(next, now));
	}

	long long SecUntilWeeklyResetUtc()
	{
		/* Monday 07:30 UTC. tm_wday: 0=Sun, 1=Mon, … */
		int y, mo, d, h, mi, s, wday;
		UtcNowParts(y, mo, d, h, mi, s, wday);
		const time_t now = time(nullptr);
		int daysAhead = (1 - wday + 7) % 7; /* days until Monday */
		int ty = y, tmo = mo, td = d;
		AddUtcDays(ty, tmo, td, daysAhead);
		time_t next = MakeUtc(ty, tmo, td, 7, 30, 0);
		if (next <= now)
		{
			AddUtcDays(ty, tmo, td, 7);
			next = MakeUtc(ty, tmo, td, 7, 30, 0);
		}
		return static_cast<long long>(difftime(next, now));
	}

	void DrawResetCountdowns()
	{
		const std::string daily = FormatCountdown(SecUntilDailyResetUtc());
		const std::string weekly = FormatCountdown(SecUntilWeeklyResetUtc());
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.f),
			"Daily reset in %s", daily.c_str());
		ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.f),
			"Weekly reset in %s", weekly.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"UTC — daily 00:00 · weekly Mon 07:30");
		ImGui::PopTextWrapPos();
	}

	void ParseVaultObjs(const std::string& json, std::vector<Obj>& out, int maxItems = 80,
		int maxAcclaim = -1)
	{
		out.clear();
		size_t pos = json.find('[');
		if (pos == std::string::npos) return;
		size_t i = pos;
		while (static_cast<int>(out.size()) < maxItems)
		{
			size_t obj = json.find('{', i);
			if (obj == std::string::npos) break;
			size_t end = JsonObjectEnd(json, obj);
			if (end == std::string::npos) break;
			const std::string chunk = json.substr(obj, end - obj + 1);
			Obj o;
			o.title = JsonStringAfterKey(chunk, "title");
			if (o.title.empty())
				o.title = JsonStringAfterKey(chunk, "name");
			o.track = JsonStringAfterKey(chunk, "track");
			o.cur = static_cast<int>(JsonIntAfterKey(chunk, "progress_current"));
			o.need = static_cast<int>(JsonIntAfterKey(chunk, "progress_complete"));
			o.acclaim = static_cast<int>(JsonIntAfterKey(chunk, "acclaim"));
			if (o.cur < 0) o.cur = 0;
			if (o.need < 0) o.need = 0;
			if (o.acclaim < 0) o.acclaim = 0;
			const bool claimed = JsonBoolAfterKey(chunk, "claimed");
			o.done = claimed || (o.need > 0 && o.cur >= o.need);
			i = end + 1;
			if (o.title.empty()) continue;
			if (maxAcclaim > 0 && (o.acclaim <= 0 || o.acclaim > maxAcclaim))
				continue;
			out.push_back(std::move(o));
		}
	}

	std::wstring StemJson(const std::wstring& dir, const char* stem)
	{
		std::wstring p = dir + L"\\";
		for (const char* s = stem; *s; ++s)
			p.push_back(static_cast<wchar_t>(*s));
		p += L".json";
		return p;
	}

	bool FileFresh(const std::wstring& path, DWORD ttlSec)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
			return false;
		ULARGE_INTEGER wr{};
		wr.LowPart = fad.ftLastWriteTime.dwLowDateTime;
		wr.HighPart = fad.ftLastWriteTime.dwHighDateTime;
		FILETIME nowFt{};
		GetSystemTimeAsFileTime(&nowFt);
		ULARGE_INTEGER now{};
		now.LowPart = nowFt.dwLowDateTime;
		now.HighPart = nowFt.dwHighDateTime;
		const ULONGLONG age100ns = now.QuadPart - wr.QuadPart;
		const ULONGLONG ttl100ns = static_cast<ULONGLONG>(ttlSec) * 10000000ull;
		return age100ns <= ttl100ns;
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE) return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &rd, nullptr);
		CloseHandle(h);
		out.resize(rd);
		return out;
	}

	bool TryLiveCache(const std::wstring& dir, const char* stem, DWORD ttlSec, Gw2Http::Result& out)
	{
		const std::wstring path = StemJson(dir, stem);
		if (!FileFresh(path, ttlSec)) return false;
		std::string body = ReadUtf8File(path);
		if (body.empty()) return false;
		out.ok = true;
		out.status = 200;
		out.body = std::move(body);
		return true;
	}

	struct FetchPack
	{
		Gw2Http::Result season, vd, vw, vs, pub;
		const char* key = nullptr;
		std::wstring dir;
	};

	DWORD WINAPI FetchSeason(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!TryLiveCache(f->dir, "live-season", 6 * 3600, f->season))
			f->season = Gw2Http::Api("/v2/wizardsvault", nullptr, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVd(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-daily", 180, f->vd))
			f->vd = Gw2Http::Api("/v2/account/wizardsvault/daily", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVw(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-weekly", 180, f->vw))
			f->vw = Gw2Http::Api("/v2/account/wizardsvault/weekly", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchVs(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (!f->key || !f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-special", 180, f->vs))
			f->vs = Gw2Http::Api("/v2/account/wizardsvault/special", f->key, kHttpTimeoutMs);
		return 0;
	}
	DWORD WINAPI FetchPub(void* p)
	{
		FetchPack* f = static_cast<FetchPack*>(p);
		if (f->key && f->key[0]) return 0;
		if (!TryLiveCache(f->dir, "live-vault-obj", 3600, f->pub))
			f->pub = Gw2Http::Api("/v2/wizardsvault/objectives?ids=all", nullptr, kBulkTimeoutMs);
		return 0;
	}

	DWORD WINAPI MasterProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		Snapshot snap;
		snap.hasKey = G::Gw2ApiKey[0] != 0;

		FetchPack pack;
		pack.key = G::Gw2ApiKey;
		pack.dir = AddonPaths::DataDir();

		HANDLE th[5]{};
		th[0] = CreateThread(nullptr, 0, FetchSeason, &pack, 0, nullptr);
		th[1] = CreateThread(nullptr, 0, FetchVd, &pack, 0, nullptr);
		th[2] = CreateThread(nullptr, 0, FetchVw, &pack, 0, nullptr);
		th[3] = CreateThread(nullptr, 0, FetchVs, &pack, 0, nullptr);
		th[4] = CreateThread(nullptr, 0, FetchPub, &pack, 0, nullptr);
		for (HANDLE h : th)
		{
			if (h)
			{
				WaitForSingleObject(h, 20000);
				CloseHandle(h);
			}
		}

		if (pack.season.ok)
		{
			snap.seasonTitle = JsonStringAfterKey(pack.season.body, "title");
			if (snap.seasonTitle.empty())
				snap.seasonTitle = "Wizard’s Vault";
			snap.seasonBlurb = SeasonBlurb(
				JsonStringAfterKey(pack.season.body, "start"),
				JsonStringAfterKey(pack.season.body, "end"));
		}
		else
		{
			snap.seasonTitle = "Wizard’s Vault";
			snap.seasonBlurb = pack.season.error.empty() ? "Season fetch failed." : pack.season.error;
		}

		if (snap.hasKey)
		{
			if (pack.vd.ok)
				ParseVaultObjs(pack.vd.body, snap.daily);
			else if (pack.vd.status == 401 || pack.vd.status == 403)
				snap.scopeFail = true;
			if (pack.vw.ok)
				ParseVaultObjs(pack.vw.body, snap.weekly);
			if (pack.vs.ok)
				ParseVaultObjs(pack.vs.body, snap.special);
		}
		else if (pack.pub.ok)
			ParseVaultObjs(pack.pub.body, snap.easyPreview, 40, 10);

		snap.ok = true;
		snap.fetchedAt = GetTickCount();
		char st[160];
		if (snap.scopeFail)
			std::snprintf(st, sizeof(st), "Need account + progression scopes.");
		else if (snap.hasKey)
			std::snprintf(st, sizeof(st), "Daily %d · Weekly %d · Special %d",
				static_cast<int>(snap.daily.size()),
				static_cast<int>(snap.weekly.size()),
				static_cast<int>(snap.special.size()));
		else
			std::snprintf(st, sizeof(st), "Public preview — add API key for live Vault.");
		snap.status = st;

		{
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(snap);
			gGen.fetch_add(1);
		}
		gBusy = false;
		return 0;
	}

	void StartFetch(bool force)
	{
		if (!force)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gSnap.ok && gSnap.fetchedAt != 0 &&
				(GetTickCount() - gSnap.fetchedAt) < kCacheTtlMs)
				return;
		}
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			if (WaitForSingleObject(gThread, 0) == WAIT_OBJECT_0)
			{
				CloseHandle(gThread);
				gThread = nullptr;
			}
			else
			{
				gBusy = false;
				return;
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			gSnap.status = gSnap.ok ? "Refreshing…" : "Loading…";
			gGen.fetch_add(1);
		}
		gThread = CreateThread(nullptr, 0, MasterProc, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::lock_guard<std::mutex> lock(gMu);
			gSnap.status = "Could not start fetch.";
			gGen.fetch_add(1);
		}
	}

	void SyncDraw()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen) return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gSnap;
		gDrawnGen = gGen.load();
	}

	void DrawObjList(const char* label, const std::vector<Obj>& list)
	{
		if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			return;
		if (list.empty())
		{
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "No objectives.");
			ImGui::PopTextWrapPos();
			ImGui::TreePop();
			return;
		}
		for (size_t i = 0; i < list.size(); ++i)
		{
			const Obj& o = list[i];
			ImGui::PushID(static_cast<int>(i));
			ImVec4 col = o.done
				? ImVec4(0.45f, 0.75f, 0.50f, 1.f)
				: ImVec4(0.88f, 0.88f, 0.90f, 1.f);
			ImGui::PushTextWrapPos(0.f);
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::TextWrapped("%s%s", o.done ? "[Done] " : "", o.title.c_str());
			ImGui::PopStyleColor();
			if (!o.track.empty() || o.acclaim > 0 || o.need > 0)
			{
				std::string m;
				if (!o.track.empty()) m += o.track;
				if (o.acclaim > 0)
				{
					if (!m.empty()) m += " · ";
					m += std::to_string(o.acclaim);
					m += " acclaim";
					if (o.acclaim <= 10) m += " (easy)";
				}
				if (o.need > 0)
				{
					if (!m.empty()) m += " · ";
					m += std::to_string(o.cur);
					m += " / ";
					m += std::to_string(o.need);
				}
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.62f, 1.f));
				ImGui::TextWrapped("%s", m.c_str());
				ImGui::PopStyleColor();
				if (o.need > 0)
				{
					float frac = static_cast<float>(o.cur) / static_cast<float>(o.need);
					if (frac < 0.f) frac = 0.f;
					if (frac > 1.f) frac = 1.f;
					ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 6.f), "");
				}
			}
			ImGui::PopTextWrapPos();
			ImGui::Spacing();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}

void VaultPad::RefreshData()
{
	bool need = true;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gSnap.ok && gSnap.fetchedAt != 0 &&
			(GetTickCount() - gSnap.fetchedAt) < kCacheTtlMs)
			need = false;
	}
	StartFetch(need);
}

void VaultPad::OpenAndRefresh()
{
	G::ShowVault = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	RefreshData();
}

void VaultPad::RenderContents()
{
	SyncDraw();
	const Snapshot& snap = gDraw;

	ImGui::TextUnformatted("Dailies & Wizard’s Vault");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Official API — account + progression scopes. (Account → Vault tab.)");
	ImGui::PopTextWrapPos();

	if (ImGui::Button("Refresh###gw2igh_vault_ref"))
		StartFetch(true);
	if (gBusy)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Updating…");
		ImGui::PopTextWrapPos();
	}
	else if (!snap.status.empty())
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", snap.status.c_str());
		ImGui::PopTextWrapPos();
	}

	ImGui::Separator();

	const float listH = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("###gw2igh_vault_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);

	ImGui::PushTextWrapPos(0.f);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.f));
	ImGui::TextWrapped("%s", snap.seasonTitle.c_str());
	ImGui::PopStyleColor();
	ImGui::TextWrapped("%s", snap.seasonBlurb.c_str());
	ImGui::PopTextWrapPos();
	ImGui::Spacing();
	DrawResetCountdowns();
	ImGui::Spacing();

	if (!snap.hasKey)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextWrapped("Add an API key in Nexus Options (account + progression) for live personal Vault.");
		ImGui::PopTextWrapPos();
		DrawObjList("Easy Vault preview", snap.easyPreview);
	}
	else if (snap.scopeFail)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextWrapped("API key needs account + progression scopes for live Vault progress.");
		ImGui::PopTextWrapPos();
	}
	else
	{
		DrawObjList("Daily Vault", snap.daily);
		DrawObjList("Weekly Vault", snap.weekly);
		DrawObjList("Special Vault", snap.special);
	}

	ImGui::EndChild();
}

bool VaultPad::Render()
{
	if (!G::ShowVault)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(PadDock::MaxW(560.f), maxH));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.38f : 100.f;
		const float fy = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.12f : 80.f;
		PadDock::Place(G::PadVault, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadVault.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowVault;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Dailies & Vault##GW2InGameHelperVault", &open))
	{
		if (PadDock::Capture(G::PadVault))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowVault = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowVault = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadVault))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale;
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
