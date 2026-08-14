#include "EventAlert.h"

#include "EventsData.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"

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
	constexpr int kWarnFiveSec = 5 * 60;
	constexpr float kToastSec = 5.5f;
	constexpr float kPlaceSec = 45.f;
	constexpr float kScanIntervalSec = 2.0f;
	constexpr size_t kQueueMax = 12;

	enum class Phase : unsigned char
	{
		Idle = 0,
		Soon = 1,
		Soon5 = 2,
		Live = 3,
		Ending = 4,
		Ending5 = 5,
	};

	enum class ToastKind : unsigned char
	{
		Soon = 0,
		Soon5 = 1,
		Live = 2,
		Ending = 3,
		Ending5 = 4,
	};

	struct Toast
	{
		char title[96]{};
		char sub[96]{};
		ToastKind kind = ToastKind::Soon;
		bool placing = false;
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
	bool sSnapDefault = false;

	unsigned CurrentMapId()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return 0;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		return (ctx && ctx->mapId) ? ctx->mapId : 0;
	}

	void ParseTrackCsv(const char* csv, std::unordered_map<std::string, bool>& out)
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
				out[std::move(key)] = true;
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

	void Enqueue(const EventsData::Entry& e, ToastKind kind, int secs)
	{
		if (sQueue.size() >= kQueueMax)
			return;
		Toast t{};
		t.kind = kind;
		std::snprintf(t.title, sizeof(t.title), "%s", e.title ? e.title : e.key);
		const char* map = (e.mapLabel && e.mapLabel[0]) ? e.mapLabel : "";
		if (kind == ToastKind::Live)
		{
			if (map[0])
				std::snprintf(t.sub, sizeof(t.sub), "LIVE · %s", map);
			else
				std::snprintf(t.sub, sizeof(t.sub), "LIVE");
		}
		else if (kind == ToastKind::Ending || kind == ToastKind::Ending5)
		{
			char cd[32]{};
			FormatCountdown(secs, cd, sizeof(cd));
			if (map[0])
				std::snprintf(t.sub, sizeof(t.sub), "Ends in %s · %s", cd, map);
			else
				std::snprintf(t.sub, sizeof(t.sub), "Ends in %s", cd);
		}
		else
		{
			char cd[32]{};
			FormatCountdown(secs, cd, sizeof(cd));
			if (map[0])
				std::snprintf(t.sub, sizeof(t.sub), "Starts in %s · %s", cd, map);
			else
				std::snprintf(t.sub, sizeof(t.sub), "Starts in %s", cd);
		}
		sQueue.push_back(t);
	}

	void ResetWatchState()
	{
		sPrimed = false;
		sPhase.clear();
		sQueue.clear();
		if (!sActive.placing)
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

		if (G::EventAlertsThisMap && mapId == 0)
		{
			sPhase.clear();
			sPrimed = true;
			return;
		}

		std::unordered_map<std::string, bool> tracked;
		if (G::EventAlertsTrackedOnly)
		{
			ParseTrackCsv(G::EventTrackIds, tracked);
			if (tracked.empty())
			{
				sPhase.clear();
				sPrimed = true;
				return;
			}
		}

		size_t n = 0;
		const EventsData::Entry* all = EventsData::All(&n);
		const time_t now = std::time(nullptr);
		std::unordered_map<std::string, Phase> keep;
		keep.reserve(n / 2 + 8);
		const bool notify = sPrimed;

		for (size_t i = 0; i < n; ++i)
		{
			const EventsData::Entry& e = all[i];
			if (!e.key || !e.key[0])
				continue;
			if (G::EventAlertsTrackedOnly)
			{
				if (!tracked.count(e.key))
					continue;
			}
			else if (!e.inDefaultAll)
			{
				continue;
			}
			if (G::EventAlertsThisMap && !EventsData::EntryMatchesMap(e, mapId))
				continue;

			const EventsData::Timing t = EventsData::ComputeTiming(e, now);
			Phase prev = Phase::Idle;
			if (const auto it = sPhase.find(e.key); it != sPhase.end())
				prev = it->second;

			Phase next = Phase::Idle;
			if (t.live && t.untilEnd >= 0 && t.untilEnd <= kWarnFiveSec)
				next = Phase::Ending5;
			else if (t.live && t.untilEnd >= 0 && t.untilEnd <= kWarnWithinSec)
				next = Phase::Ending;
			else if (t.live)
				next = Phase::Live;
			else if (t.untilStart >= 0 && t.untilStart <= kWarnFiveSec)
				next = Phase::Soon5;
			else if (t.untilStart >= 0 && t.untilStart <= kWarnWithinSec)
				next = Phase::Soon;

			if (notify)
			{
				const bool nowLive = (next == Phase::Live || next == Phase::Ending ||
					next == Phase::Ending5);
				const bool wasLive = (prev == Phase::Live || prev == Phase::Ending ||
					prev == Phase::Ending5);
				/* Short windows (Prep is 5m) open already inside the ending
				   band — still toast LIVE, then skip the same-scan ending toast. */
				if (nowLive && !wasLive && EventsData::IsSpawnLive(e, t))
					Enqueue(e, ToastKind::Live, 0);
				else if (next == Phase::Ending5 && prev != Phase::Ending5)
					Enqueue(e, ToastKind::Ending5, t.untilEnd);
				else if (next == Phase::Ending && prev != Phase::Ending &&
					prev != Phase::Ending5)
					Enqueue(e, ToastKind::Ending, t.untilEnd);
				else if (next == Phase::Soon5 && prev != Phase::Soon5)
					Enqueue(e, ToastKind::Soon5, t.untilStart);
				else if (next == Phase::Soon && prev == Phase::Idle)
					Enqueue(e, ToastKind::Soon, t.untilStart);
			}

			keep[e.key] = next;
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

	ImVec2 DefaultPos(const ImVec2& sz)
	{
		const ImGuiIO& io = ImGui::GetIO();
		return ImVec2((io.DisplaySize.x - sz.x) * 0.5f, io.DisplaySize.y * 0.16f);
	}

	bool DrawToast(float nowImGui)
	{
		if (!sActive.title[0] || nowImGui > sActive.showUntil)
			return false;

		const float remain = sActive.showUntil - nowImGui;
		const float life = sActive.placing ? kPlaceSec : kToastSec;
		const float t = remain / life;
		const float alpha = t > 0.85f ? (1.f - t) / 0.15f : (t < 0.18f ? t / 0.18f : 1.f);
		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 sz((std::min)(460.f, io.DisplaySize.x * 0.48f), sActive.placing ? 84.f : 68.f);

		/* Cond_Appearing only — Cond_Always would fight the drag. */
		if (PadDock::HasSavedPos(G::PadEventAlert))
		{
			const ImVec2 p = PadDock::ClampPos(
				G::PadEventAlert.x, G::PadEventAlert.y, sz.x, sz.y);
			ImGui::SetNextWindowPos(p, ImGuiCond_Appearing);
		}
		else
			ImGui::SetNextWindowPos(DefaultPos(sz), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(sz, ImGuiCond_Always);

		HelperTheme::ScopedOverlay theme(0.82f * alpha * G::Opacity);
		/* Movable — drag to set where future alerts appear. */
		ImGui::Begin("##gw2igh_event_alert", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNav);

		if (sSnapDefault)
		{
			ImGui::SetWindowPos(DefaultPos(sz));
			sSnapDefault = false;
		}
		PadDock::KeepOnScreen(sz.y);

		const char* head = "Place alert";
		ImVec4 headCol = HelperTheme::Warn;
		if (!sActive.placing)
		{
			if (sActive.kind == ToastKind::Live)
			{
				head = "Event live";
				headCol = HelperTheme::Ok;
			}
			else if (sActive.kind == ToastKind::Soon5 ||
				sActive.kind == ToastKind::Ending5)
			{
				head = "5 minutes";
				headCol = HelperTheme::GoldBright;
			}
			else if (sActive.kind == ToastKind::Ending)
			{
				head = "Event ending";
				headCol = HelperTheme::Warn;
			}
			else
				head = "Event soon";
		}
		ImGui::TextColored(headCol, "%s", head);
		ImGui::TextColored(HelperTheme::Gold, "%s", sActive.title);
		if (sActive.sub[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", sActive.sub);
		if (sActive.placing)
			ImGui::TextColored(HelperTheme::Muted, "Drag anywhere on this toast.");

		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		if (PadDock::Capture(G::PadEventAlert))
			Settings::SetDirty();

		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			ImGui::OpenPopup("##ev_alert_ctx");
		if (ImGui::BeginPopup("##ev_alert_ctx"))
		{
			if (ImGui::MenuItem("Reset position"))
			{
				G::PadEventAlert = {};
				sSnapDefault = true;
				Settings::SetDirty();
			}
			if (sActive.placing && ImGui::MenuItem("Done placing"))
				sActive.showUntil = nowImGui;
			ImGui::EndPopup();
		}

		ImGui::End();
		return hovered;
	}
}

void EventAlert::BeginPlacement()
{
	G::EventAlerts = true;
	sQueue.clear();
	sActive = {};
	sActive.placing = true;
	sActive.kind = ToastKind::Soon;
	std::snprintf(sActive.title, sizeof(sActive.title), "Event alert");
	std::snprintf(sActive.sub, sizeof(sActive.sub),
		"Drag this toast — position is saved for all alerts");
	sActive.showUntil = ImGui::GetTime() + kPlaceSec;
	Settings::SetDirty();
}

void EventAlert::ResetPosition()
{
	G::PadEventAlert = {};
	sSnapDefault = true;
	Settings::SetDirty();
}

bool EventAlert::Render()
{
	if (!G::EventAlerts && !sActive.placing)
		return false;

	const float now = ImGui::GetTime();
	if (G::EventAlerts)
		ScanEvents(now);
	/* Don't interrupt an active placement toast with queued real alerts. */
	if (!sActive.placing)
		PumpQueue(now);
	return DrawToast(now);
}
