#include "EventsPadInternal.h"

#include "BrowserTabs.h"
#include "EventsData.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace EventsPadDetail
{
	std::string FmtRemain(int secs)
	{
		if (secs < 0) return "-";
		const int h = secs / 3600;
		const int m = (secs % 3600) / 60;
		const int s = secs % 60;
		char buf[32];
		if (h > 0)
			std::snprintf(buf, sizeof(buf), "%dh %02dm", h, m);
		else if (m > 0)
			std::snprintf(buf, sizeof(buf), "%dm %02ds", m, s);
		else
			std::snprintf(buf, sizeof(buf), "%ds", s);
		return buf;
	}

	bool CopyText(const char* text)
	{
		if (!text || !text[0]) return false;
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

	void ParseTrackCsv(const char* csv, std::vector<std::string>& out)
	{
		out.clear();
		if (!csv) return;
		const char* p = csv;
		while (*p && out.size() < static_cast<size_t>(kMaxTrack))
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t') ++p;
			if (!*p) break;
			const char* a = p;
			while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t') ++p;
			if (p > a)
			{
				std::string id(a, p);
				bool dup = false;
				for (const auto& x : out) if (x == id) { dup = true; break; }
				if (!dup) out.push_back(std::move(id));
			}
			while (*p && *p != ',' && *p != ';') ++p;
		}
	}

	void WriteTrackCsv(const std::vector<std::string>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += ids[i];
		}
		if (s.size() >= sizeof(G::EventTrackIds))
			s.resize(sizeof(G::EventTrackIds) - 1);
		std::snprintf(G::EventTrackIds, sizeof(G::EventTrackIds), "%s", s.c_str());
		Settings::SetDirty();
	}

	bool Tracked(const char* key)
	{
		std::vector<std::string> ids;
		ParseTrackCsv(G::EventTrackIds, ids);
		for (const auto& x : ids)
			if (x == key) return true;
		return false;
	}

	void FlipTrack(const char* key)
	{
		if (!key || !key[0]) return;
		std::vector<std::string> ids;
		ParseTrackCsv(G::EventTrackIds, ids);
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (ids[i] == key)
			{
				ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(i));
				WriteTrackCsv(ids);
				return;
			}
		}
		if (static_cast<int>(ids.size()) >= kMaxTrack)
			return;
		ids.push_back(key);
		WriteTrackCsv(ids);
	}

	std::mutex gMu;
	std::unordered_set<std::string> gBossDone;
	std::unordered_set<std::string> gChestDone;
	std::string gClaimNote;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gReady{false};
	std::unordered_set<std::string> gPendBoss;
	std::unordered_set<std::string> gPendChest;
	std::string gPendNote;
	HANDLE gThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gDeferRefresh = 0;
	bool gTrackedOnly = false;
	bool gSoonOnly = false;
	bool gThisMapOnly = false;
	char gStatus[128] = {};
	int gSectionPick = 0; /* 0 = default mix */
	char gSearch[96] = {};

	unsigned CurrentMapId()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return 0;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		return (ctx && ctx->mapId) ? ctx->mapId : 0;
	}

	bool ContainsFold(const char* hay, const char* needle)
	{
		if (!needle || !needle[0]) return true;
		if (!hay || !hay[0]) return false;
		for (const char* h = hay; *h; ++h)
		{
			const char* a = h;
			const char* b = needle;
			while (*a && *b)
			{
				char ca = *a;
				char cb = *b;
				if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
				if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
				if (ca != cb) break;
				++a;
				++b;
			}
			if (!*b) return true;
		}
		return false;
	}

	bool MatchesSearch(const EventsData::Entry& e, const char* q)
	{
		if (!q || !q[0]) return true;
		return ContainsFold(e.title, q) ||
			ContainsFold(e.mapLabel, q) ||
			ContainsFold(e.section, q) ||
			ContainsFold(e.key, q);
	}

	void CollectQuotedIds(const std::string& body, std::unordered_set<std::string>& out)
	{
		out.clear();
		size_t p = 0;
		while (p < body.size())
		{
			size_t q1 = body.find('"', p);
			if (q1 == std::string::npos) break;
			size_t q2 = body.find('"', q1 + 1);
			if (q2 == std::string::npos) break;
			std::string id = body.substr(q1 + 1, q2 - q1 - 1);
			if (!id.empty())
				out.insert(std::move(id));
			p = q2 + 1;
		}
	}

	DWORD WINAPI ClaimWorker(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::unordered_set<std::string> bosses;
		std::unordered_set<std::string> chests;
		std::string note;
		if (!G::Gw2ApiKey[0])
		{
			note = "No API key - timers work; claims need progression.";
		}
		else
		{
			auto b = Gw2Http::Api("/v2/account/worldbosses", G::Gw2ApiKey, kHttpTimeoutMs);
			auto c = Gw2Http::Api("/v2/account/mapchests", G::Gw2ApiKey, kHttpTimeoutMs);
			if ((!b.ok && (b.status == 401 || b.status == 403)) ||
				(!c.ok && (c.status == 401 || c.status == 403)))
			{
				note = "Need progression scope for claim marks.";
			}
			else
			{
				if (b.ok) CollectQuotedIds(b.body, bosses);
				if (c.ok) CollectQuotedIds(c.body, chests);
				note = "Claims updated.";
			}
		}
		{
			std::lock_guard<std::mutex> lock(gMu);
			gPendBoss = std::move(bosses);
			gPendChest = std::move(chests);
			gPendNote = std::move(note);
			gReady = true;
			gBusy = false;
		}
		return 0;
	}

	void BeginClaimRefresh()
	{
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		std::snprintf(gStatus, sizeof(gStatus), "Refreshing claims...");
		gThread = CreateThread(nullptr, 0, ClaimWorker, nullptr, 0, nullptr);
		if (!gThread)
		{
			gBusy = false;
			std::snprintf(gStatus, sizeof(gStatus), "Could not start claim fetch.");
		}
	}

	void ApplyClaimResult()
	{
		if (!gReady)
			return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady)
			return;
		gBossDone = std::move(gPendBoss);
		gChestDone = std::move(gPendChest);
		gClaimNote = std::move(gPendNote);
		gReady = false;
		std::snprintf(gStatus, sizeof(gStatus), "%s", gClaimNote.c_str());
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
	}

	bool EntryBossClaimed(const EventsData::Entry& e)
	{
		std::lock_guard<std::mutex> lock(gMu);
		return e.bossApi && e.bossApi[0] && gBossDone.count(e.bossApi) != 0;
	}

	bool EntryChestClaimed(const EventsData::Entry& e)
	{
		std::lock_guard<std::mutex> lock(gMu);
		return e.chestApi && e.chestApi[0] && gChestDone.count(e.chestApi) != 0;
	}

	bool EntryClaimed(const EventsData::Entry& e)
	{
		return EntryBossClaimed(e) || EntryChestClaimed(e);
	}

	void OpenEventWiki(const EventsData::Entry& e)
	{
		char q[160]{};
		if (e.mapLabel && e.mapLabel[0])
			std::snprintf(q, sizeof(q), "%s %s", e.mapLabel, e.title ? e.title : "");
		else
			std::snprintf(q, sizeof(q), "%s", e.title ? e.title : "Guild Wars 2");
		const std::string enc = WikiBrowser::UrlEncode(q);
		const std::string url =
			std::string("https://wiki.guildwars2.com/index.php?search=") + enc +
			"&title=Special%3ASearch&go=Go";
		if (BrowserTabs::OpenNewUrl("wiki", url) < 0)
			WikiBrowser::Navigate(url);
		std::snprintf(gStatus, sizeof(gStatus), "Opened wiki search.");
	}

	void OpenEventMetaBattle(const EventsData::Entry& e)
	{
		(void)e;
		/* Link out only — do not scrape MetaBattle into the addon. */
		ShellExecuteA(nullptr, "open", "https://metabattle.com/wiki/MetaBattle",
			nullptr, nullptr, SW_SHOWNORMAL);
		std::snprintf(gStatus, sizeof(gStatus), "Opened MetaBattle (external).");
	}

	void CollectRows(std::vector<Row>& rows, time_t now)
	{
		rows.clear();
		size_t n = 0;
		const EventsData::Entry* all = EventsData::All(&n);
		size_t nSec = 0;
		const char* const* sections = EventsData::Sections(&nSec);

		const char* sectionFilter = nullptr;
		if (gSectionPick > 0 && static_cast<size_t>(gSectionPick) <= nSec)
			sectionFilter = sections[static_cast<size_t>(gSectionPick - 1)];
		const bool searching = gSearch[0] != 0;
		const unsigned mapId = gThisMapOnly ? CurrentMapId() : 0;

		for (size_t i = 0; i < n; ++i)
		{
			const EventsData::Entry& e = all[i];
			if (sectionFilter && std::strcmp(e.section, sectionFilter) != 0)
				continue;
			if (gThisMapOnly)
			{
				if (mapId == 0 || !EventsData::EntryMatchesMap(e, mapId))
					continue;
			}
			else
			{
				if (!searching && !sectionFilter && !e.inDefaultAll && !Tracked(e.key))
					continue;
			}
			if (searching && !MatchesSearch(e, gSearch))
				continue;

			Row r;
			r.index = static_cast<int>(i);
			r.timing = EventsData::ComputeTiming(e, now);
			r.tracked = Tracked(e.key);
			r.claimed = EntryClaimed(e);
			r.warn = r.tracked && !r.claimed &&
				(EventsData::IsSpawnLive(e, r.timing) ||
					(r.timing.untilStart >= 0 && r.timing.untilStart <= kWarnWithinSec) ||
					(r.timing.live && r.timing.untilEnd >= 0 &&
						r.timing.untilEnd <= kWarnWithinSec));

			if (gTrackedOnly && !r.tracked)
				continue;
			if (gSoonOnly)
			{
				const bool soon = EventsData::IsSpawnLive(e, r.timing) ||
					(r.timing.untilStart >= 0 && r.timing.untilStart <= kSoonFilterSec) ||
					(r.timing.live && r.timing.untilEnd >= 0 &&
						r.timing.untilEnd <= kSoonFilterSec);
				if (!soon) continue;
			}
			rows.push_back(r);
		}

		std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
			size_t n = 0;
			const EventsData::Entry* all = EventsData::All(&n);
			const EventsData::Entry& ea = all[static_cast<size_t>(a.index)];
			const EventsData::Entry& eb = all[static_cast<size_t>(b.index)];
			const bool aSpawn = EventsData::IsSpawnLive(ea, a.timing);
			const bool bSpawn = EventsData::IsSpawnLive(eb, b.timing);
			if (a.tracked != b.tracked) return a.tracked > b.tracked;
			if (aSpawn != bSpawn) return aSpawn > bSpawn;
			if (a.warn != b.warn) return a.warn > b.warn;
			const bool aLong = a.timing.live && !aSpawn;
			const bool bLong = b.timing.live && !bSpawn;
			if (aLong != bLong) return !aLong && bLong;
			const int as = aSpawn ? a.timing.untilEnd
				: (aLong ? a.timing.untilEnd : a.timing.untilStart);
			const int bs = bSpawn ? b.timing.untilEnd
				: (bLong ? b.timing.untilEnd : b.timing.untilStart);
			if (as < 0) return false;
			if (bs < 0) return true;
			return as < bs;
		});
	}
}
