#include "EventsPad.h"

#include "EventsData.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kMaxTrack = 120;
	constexpr int kHttpTimeoutMs = 2500;
	constexpr int kWarnWithinSec = 10 * 60;
	constexpr int kSoonFilterSec = 30 * 60;
	constexpr float kPadW = 460.f;
	constexpr float kPadH = 560.f;

	struct Timing
	{
		bool live = false;
		int  untilStart = -1; /* 0 when live */
		int  untilEnd = -1;
	};

	int PosMod(int v, int m)
	{
		if (m <= 0) return 0;
		int r = v % m;
		if (r < 0) r += m;
		return r;
	}

	Timing FromStartList(int nowInCycle, int cycleLen, const int* starts, int n, int activeSec)
	{
		Timing t;
		if (!starts || n <= 0 || activeSec <= 0)
			return t;
		for (int i = 0; i < n; ++i)
		{
			const int begin = starts[i];
			if (nowInCycle < begin)
			{
				t.untilStart = begin - nowInCycle;
				return t;
			}
			if (nowInCycle < begin + activeSec)
			{
				t.live = true;
				t.untilStart = 0;
				t.untilEnd = begin + activeSec - nowInCycle;
				return t;
			}
		}
		t.untilStart = cycleLen - nowInCycle + starts[0];
		return t;
	}

	Timing FromRepeat(time_t now, int cycleSec, int phaseSec, int activeSec, int copies)
	{
		Timing t;
		if (cycleSec <= 0 || activeSec <= 0)
			return t;
		if (copies < 1) copies = 1;
		const int nowIn = static_cast<int>(now % cycleSec);
		const int stride = cycleSec / copies;
		int bestWait = cycleSec;
		for (int c = 0; c < copies; ++c)
		{
			const int start = phaseSec + c * stride;
			const int age = PosMod(nowIn - start, cycleSec);
			if (age < activeSec)
			{
				t.live = true;
				t.untilStart = 0;
				t.untilEnd = activeSec - age;
				return t;
			}
			const int wait = cycleSec - age;
			if (wait < bestWait)
				bestWait = wait;
		}
		t.untilStart = bestWait;
		return t;
	}

	Timing ComputeTiming(const EventsData::Entry& e, time_t now)
	{
		using S = EventsData::Sched;
		switch (e.sched)
		{
		case S::DayList:
			return FromStartList(static_cast<int>(now % 86400), 86400,
				e.starts, e.startCount, e.activeSec);
		case S::CycleList:
			return FromStartList(static_cast<int>(now % e.cycleSec), e.cycleSec,
				e.starts, e.startCount, e.activeSec);
		case S::Repeat:
		case S::CycleSlot:
		default:
			return FromRepeat(now, e.cycleSec, e.phaseSec, e.activeSec, e.copies);
		}
	}

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

	bool EntryClaimed(const EventsData::Entry& e)
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (e.bossApi && e.bossApi[0] && gBossDone.count(e.bossApi))
			return true;
		if (e.chestApi && e.chestApi[0] && gChestDone.count(e.chestApi))
			return true;
		return false;
	}

	struct Row
	{
		int index = 0;
		Timing timing;
		bool tracked = false;
		bool claimed = false;
		bool warn = false;
	};

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
				/* Search reaches invasions/festivals; otherwise keep default filter.
				   This-map mode already reveals invasions/fractals for that map. */
				if (!searching && !sectionFilter && !e.inDefaultAll && !Tracked(e.key))
					continue;
			}
			if (searching && !MatchesSearch(e, gSearch))
				continue;

			Row r;
			r.index = static_cast<int>(i);
			r.timing = ComputeTiming(e, now);
			r.tracked = Tracked(e.key);
			r.claimed = EntryClaimed(e);
			r.warn = r.tracked && !r.claimed &&
				(r.timing.live ||
					(r.timing.untilStart >= 0 && r.timing.untilStart <= kWarnWithinSec));

			if (gTrackedOnly && !r.tracked)
				continue;
			if (gSoonOnly)
			{
				const bool soon = r.timing.live ||
					(r.timing.untilStart >= 0 && r.timing.untilStart <= kSoonFilterSec);
				if (!soon) continue;
			}
			rows.push_back(r);
		}

		std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
			if (a.tracked != b.tracked) return a.tracked > b.tracked;
			if (a.warn != b.warn) return a.warn > b.warn;
			if (a.timing.live != b.timing.live) return a.timing.live > b.timing.live;
			const int as = a.timing.live ? a.timing.untilEnd : a.timing.untilStart;
			const int bs = b.timing.live ? b.timing.untilEnd : b.timing.untilStart;
			if (as < 0) return false;
			if (bs < 0) return true;
			return as < bs;
		});
	}
}

void EventsPad::OpenAndRefresh()
{
	G::ShowEvents = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	BeginClaimRefresh();
}

bool EventsPad::Render()
{
	ApplyClaimResult();
	if (!G::ShowEvents)
		return false;

	size_t nSec = 0;
	const char* const* sections = EventsData::Sections(&nSec);
	size_t nAll = 0;
	const EventsData::Entry* all = EventsData::All(&nAll);

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = (io.DisplaySize.y > 100.f)
		? (io.DisplaySize.y * 0.90f < 920.f ? io.DisplaySize.y * 0.90f : 920.f)
		: 720.f;
	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 300.f), ImVec2(620.f, maxH));
	if (gPlaceOnce)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	else
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gPlaceOnce)
	{
		const float x = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.46f : 160.f;
		const float y = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.10f : 80.f;
		ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
		ImGui::SetNextWindowFocus();
		gPlaceOnce = false;
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowEvents;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("World Events##GW2InGameHelperEvents", &open))
	{
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowEvents = false;
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowEvents = false;
		Settings::SetDirty();
	}

	ImGui::TextColored(HelperTheme::Gold, "WORLD EVENTS");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(HelperTheme::Muted,
		"UTC timers for bosses and map metas. Track items you care about - "
		"they sort up and highlight within 10 minutes. "
		"Invasions / festivals / fractals stay hidden until you open that section or search/Track them.");
	ImGui::PopTextWrapPos();

	if (ImGui::Button("Refresh claims###gw2igh_ev_ref"))
		BeginClaimRefresh();
	ImGui::SameLine();
	ImGui::Checkbox("Tracked###gw2igh_ev_trackonly", &gTrackedOnly);
	ImGui::SameLine();
	ImGui::Checkbox("<=30m###gw2igh_ev_soon", &gSoonOnly);
	ImGui::SameLine();
	ImGui::Checkbox("This map###gw2igh_ev_thismap", &gThisMapOnly);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Show only events for your current open-world map\n"
			"(read-only MumbleLink map id).");

	if (gThisMapOnly)
	{
		const unsigned mapId = CurrentMapId();
		const char* mapName = EventsData::MapDisplayName(mapId);
		if (mapId == 0)
			ImGui::TextColored(ImVec4(1.f, 0.55f, 0.35f, 1.f),
				"This map: waiting for MumbleLink…");
		else if (mapName)
			ImGui::TextColored(ImVec4(0.55f, 0.78f, 0.95f, 1.f),
				"This map: %s", mapName);
		else
			ImGui::TextColored(ImVec4(0.55f, 0.78f, 0.95f, 1.f),
				"This map: id %u (no timetable rows tagged)", mapId);
	}

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_ev_search", "Search events (name, map, section)...",
		gSearch, sizeof(gSearch));

	/* Inline section list (not BeginCombo). Combo popups are separate ImGui
	   windows — Nexus often won't forward clicks outside the pad rect. */
	char sectionPreview[96];
	if (gSectionPick <= 0 || static_cast<size_t>(gSectionPick) > nSec)
	{
		gSectionPick = 0;
		std::snprintf(sectionPreview, sizeof(sectionPreview), "All (default sections)");
	}
	else
		std::snprintf(sectionPreview, sizeof(sectionPreview), "%s",
			sections[static_cast<size_t>(gSectionPick - 1)]);

	char sectionHeader[128];
	std::snprintf(sectionHeader, sizeof(sectionHeader), "Section: %s###gw2igh_ev_sec", sectionPreview);
	if (ImGui::CollapsingHeader(sectionHeader, ImGuiTreeNodeFlags_None))
	{
		const float rowH = ImGui::GetTextLineHeightWithSpacing();
		const float boxH = rowH * 6.5f;
		ImGui::BeginChild("###gw2igh_ev_sec_list", ImVec2(-1.f, boxH), true);
		if (ImGui::Selectable("All (default sections)", gSectionPick == 0))
			gSectionPick = 0;
		for (size_t i = 0; i < nSec; ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			const int idx = static_cast<int>(i + 1);
			if (ImGui::Selectable(sections[i], gSectionPick == idx))
				gSectionPick = idx;
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	if (gBusy)
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading...");
	else if (gStatus[0])
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", gStatus);

	ImGui::Separator();

	/* Build list after search/section widgets so filters apply this frame. */
	const time_t now = std::time(nullptr);
	std::vector<Row> rows;
	CollectRows(rows, now);

	const float footerH = ImGui::GetTextLineHeightWithSpacing() * 1.6f;
	float listH = ImGui::GetContentRegionAvail().y - footerH;
	if (listH < 120.f) listH = 120.f;
	ImGui::BeginChild("###gw2igh_ev_list", ImVec2(0.f, listH), true);

	if (rows.empty())
	{
		if (gThisMapOnly && CurrentMapId() == 0)
			ImGui::TextWrapped("Turn on MumbleLink / enter the world — then This map can filter.");
		else if (gThisMapOnly)
			ImGui::TextWrapped(
				"No timetable events tagged for this map (cities, instances, and some zones have none).");
		else if (gTrackedOnly)
			ImGui::TextWrapped("Nothing tracked. Uncheck Tracked, then press Track on events you want.");
		else if (gSearch[0])
			ImGui::TextWrapped("No events match that search.");
		else
			ImGui::TextWrapped("No events match these filters.");
	}

	const char* lastMap = nullptr;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const Row& r = rows[i];
		const EventsData::Entry& e = all[static_cast<size_t>(r.index)];
		ImGui::PushID(e.key);

		if (e.mapLabel && e.mapLabel[0] && (!lastMap || std::strcmp(lastMap, e.mapLabel) != 0))
		{
			if (i > 0)
				ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.75f, 0.70f, 0.45f, 1.f), "%s", e.mapLabel);
			lastMap = e.mapLabel;
		}

		if (r.warn)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.35f, 1.f));

		/* ASCII markers only — GW2 ImGui fonts often lack bullet/star glyphs ("?"). */
		char title[192];
		if (r.timing.live)
			std::snprintf(title, sizeof(title), "[LIVE] %s", e.title);
		else if (r.tracked)
			std::snprintf(title, sizeof(title), "* %s", e.title);
		else
			std::snprintf(title, sizeof(title), "%s", e.title);
		ImGui::TextUnformatted(title);

		if (r.claimed)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.70f, 0.50f, 1.f), "[claimed]");
		}

		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "%s", e.section);

		if (r.timing.live)
		{
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.f),
				"Active - ends in %s", FmtRemain(r.timing.untilEnd).c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f),
				"Next in %s", FmtRemain(r.timing.untilStart).c_str());
		}
		if (r.warn && !r.timing.live)
			ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.35f, 1.f), "Tracked - starting soon");

		if (ImGui::SmallButton(r.tracked ? "Untrack" : "Track"))
			FlipTrack(e.key);
		ImGui::SameLine();
		if (e.waypoint && e.waypoint[0] && ImGui::SmallButton("WP"))
		{
			if (CopyText(e.waypoint))
				std::snprintf(gStatus, sizeof(gStatus), "Waypoint copied.");
			else
				std::snprintf(gStatus, sizeof(gStatus), "Clipboard failed.");
		}

		if (r.warn)
			ImGui::PopStyleColor();

		if (i + 1 < rows.size())
			ImGui::Separator();
		ImGui::PopID();
	}

	ImGui::EndChild();

	int trackedN = 0;
	{
		std::vector<std::string> ids;
		ParseTrackCsv(G::EventTrackIds, ids);
		trackedN = static_cast<int>(ids.size());
	}
	ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
		"%d tracked | %d shown | %d in catalog",
		trackedN, static_cast<int>(rows.size()), static_cast<int>(nAll));

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	ImGui::End();
	return hovered;
}
