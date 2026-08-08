#include "EventAlert.h"

#include "EventsData.h"
#include "Globals.h"
#include "HelperTheme.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	constexpr int kWarnWithinSec = 10 * 60;
	constexpr float kToastSec = 5.5f;
	constexpr float kScanIntervalSec = 0.5f;
	constexpr size_t kQueueMax = 6;

	enum class Phase : unsigned char
	{
		Idle = 0,
		Soon = 1,
		Live = 2,
	};

	struct Toast
	{
		char title[96]{};
		char sub[96]{};
		bool live = false;
		float showUntil = 0.f;
	};

	std::unordered_map<std::string, Phase> sPhase;
	std::vector<Toast> sQueue;
	Toast sActive{};
	float sNextScan = 0.f;
	bool sPrimed = false;
	bool sLastTrackedOnly = false;
	bool sLastThisMap = false;
	unsigned sLastMapId = 0;

	unsigned CurrentMapId()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return 0;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		return (ctx && ctx->mapId) ? ctx->mapId : 0;
	}

	void ParseTrackCsv(const char* csv, std::vector<std::string>& out)
	{
		out.clear();
		if (!csv || !csv[0])
			return;
		const char* p = csv;
		while (*p)
		{
			while (*p == ',' || *p == ' ')
				++p;
			if (!*p)
				break;
			const char* start = p;
			while (*p && *p != ',')
				++p;
			std::string key(start, static_cast<size_t>(p - start));
			while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
				key.pop_back();
			if (!key.empty())
				out.push_back(std::move(key));
		}
	}

	void FormatCountdown(int untilStart, char* out, size_t outLen)
	{
		if (untilStart <= 0)
		{
			std::snprintf(out, outLen, "now");
			return;
		}
		const int m = untilStart / 60;
		const int s = untilStart % 60;
		if (m >= 60)
			std::snprintf(out, outLen, "%dh %dm", m / 60, m % 60);
		else if (m > 0)
			std::snprintf(out, outLen, "%dm %02ds", m, s);
		else
			std::snprintf(out, outLen, "%ds", s);
	}

	void Enqueue(const EventsData::Entry& e, bool live, int untilStart)
	{
		if (sQueue.size() >= kQueueMax)
			return;
		Toast t{};
		t.live = live;
		std::snprintf(t.title, sizeof(t.title), "%s", e.title ? e.title : e.key);
		if (live)
		{
			std::snprintf(t.sub, sizeof(t.sub), "LIVE%s%s",
				(e.mapLabel && e.mapLabel[0]) ? " · " : "",
				(e.mapLabel && e.mapLabel[0]) ? e.mapLabel : "");
		}
		else
		{
			char cd[32]{};
			FormatCountdown(untilStart, cd, sizeof(cd));
			if (e.mapLabel && e.mapLabel[0])
				std::snprintf(t.sub, sizeof(t.sub), "Starts in %s · %s", cd, e.mapLabel);
			else
				std::snprintf(t.sub, sizeof(t.sub), "Starts in %s", cd);
		}
		sQueue.push_back(t);
	}

	void CollectWatchKeys(std::vector<std::string>& keys, unsigned mapId)
	{
		keys.clear();
		if (G::EventAlertsThisMap && mapId == 0)
			return;

		if (G::EventAlertsTrackedOnly)
		{
			ParseTrackCsv(G::EventTrackIds, keys);
		}
		else
		{
			size_t n = 0;
			const EventsData::Entry* all = EventsData::All(&n);
			keys.reserve(n);
			for (size_t i = 0; i < n; ++i)
			{
				/* Same default set as Events pad (skip festival/invasion extras). */
				if (!all[i].inDefaultAll || !all[i].key || !all[i].key[0])
					continue;
				keys.emplace_back(all[i].key);
			}
		}

		if (!G::EventAlertsThisMap)
			return;

		std::vector<std::string> filtered;
		filtered.reserve(keys.size());
		for (const auto& id : keys)
		{
			const EventsData::Entry* e = EventsData::FindByKey(id.c_str());
			if (e && EventsData::EntryMatchesMap(*e, mapId))
				filtered.push_back(id);
		}
		keys.swap(filtered);
	}

	void ResetWatchState()
	{
		sPrimed = false;
		sPhase.clear();
		sQueue.clear();
		sActive = {};
	}

	void ScanEvents(float nowImGui)
	{
		if (nowImGui < sNextScan)
			return;
		sNextScan = nowImGui + kScanIntervalSec;

		const unsigned mapId = CurrentMapId();
		if (sLastTrackedOnly != G::EventAlertsTrackedOnly ||
			sLastThisMap != G::EventAlertsThisMap ||
			(G::EventAlertsThisMap && mapId != sLastMapId))
		{
			sLastTrackedOnly = G::EventAlertsTrackedOnly;
			sLastThisMap = G::EventAlertsThisMap;
			sLastMapId = mapId;
			ResetWatchState();
		}
		else
		{
			sLastMapId = mapId;
		}

		std::vector<std::string> keys;
		CollectWatchKeys(keys, mapId);
		if (keys.empty())
		{
			sPhase.clear();
			sPrimed = true;
			return;
		}

		const time_t now = std::time(nullptr);
		std::unordered_map<std::string, Phase> keep;
		keep.reserve(keys.size());
		const bool notify = sPrimed;

		for (const auto& id : keys)
		{
			const EventsData::Entry* e = EventsData::FindByKey(id.c_str());
			if (!e)
				continue;
			const EventsData::Timing t = EventsData::ComputeTiming(*e, now);
			Phase prev = Phase::Idle;
			if (const auto it = sPhase.find(id); it != sPhase.end())
				prev = it->second;

			Phase next = Phase::Idle;
			if (t.live)
				next = Phase::Live;
			else if (t.untilStart >= 0 && t.untilStart <= kWarnWithinSec)
				next = Phase::Soon;

			if (notify)
			{
				if (next == Phase::Live && prev != Phase::Live)
					Enqueue(*e, true, 0);
				else if (next == Phase::Soon && prev == Phase::Idle)
					Enqueue(*e, false, t.untilStart);
			}

			/* Stay in Live/Soon until the window ends so we do not re-toast. */
			if (next == Phase::Idle)
				keep[id] = Phase::Idle;
			else if (prev == Phase::Live || next == Phase::Live)
				keep[id] = Phase::Live;
			else
				keep[id] = Phase::Soon;
		}
		sPhase.swap(keep);
		sPrimed = true;
	}

	void PumpQueue(float nowImGui)
	{
		if (sActive.title[0] && nowImGui <= sActive.showUntil)
			return;
		sActive = {};
		if (sQueue.empty())
			return;
		sActive = sQueue.front();
		sQueue.erase(sQueue.begin());
		sActive.showUntil = nowImGui + kToastSec;
	}

	bool DrawToast(float nowImGui)
	{
		if (!sActive.title[0] || nowImGui > sActive.showUntil)
			return false;

		const float remain = sActive.showUntil - nowImGui;
		const float t = remain / kToastSec;
		const float alpha = t > 0.85f ? (1.f - t) / 0.15f : (t < 0.18f ? t / 0.18f : 1.f);
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 sz((std::min)(460.f, io.DisplaySize.x * 0.48f), 68.f);
		/* Below zone-entry banner so both can show without overlap. */
		ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - sz.x) * 0.5f, io.DisplaySize.y * 0.16f));
		ImGui::SetNextWindowSize(sz);
		HelperTheme::ScopedOverlay theme(0.82f * alpha * G::Opacity);
		ImGui::Begin("##gw2igh_event_alert", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
		ImGui::TextColored(sActive.live ? HelperTheme::Ok : HelperTheme::Warn, "%s",
			sActive.live ? "Event live" : "Event soon");
		ImGui::TextColored(HelperTheme::Gold, "%s", sActive.title);
		if (sActive.sub[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", sActive.sub);
		ImGui::End();
		return false;
	}
}

bool EventAlert::Render()
{
	if (!G::EventAlerts)
		return false;

	const float now = ImGui::GetTime();
	ScanEvents(now);
	PumpQueue(now);
	return DrawToast(now);
}
