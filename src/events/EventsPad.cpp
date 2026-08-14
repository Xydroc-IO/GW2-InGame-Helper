#include "EventsPad.h"
#include "EventsPadInternal.h"

#include "EventsData.h"
#include "AspectLayout.h"
#include "EventAlert.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "PadDock.h"
#include "CrashTrail.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace
{
	/* Curated GW2.dat UI ids — same CDN / ui-chrome pipeline as the helper rail. */
	int IconForEventsSection(const char* section)
	{
		if (!section || !section[0])
			return static_cast<int>(Gw2Ui::Icon::EventsMedal);
		if (std::strcmp(section, "Instanced") == 0)
			return static_cast<int>(Gw2Ui::Icon::InstGate);
		if (std::strcmp(section, "Core bosses") == 0)
			return static_cast<int>(Gw2Ui::Icon::EventsMedal);
		if (std::strcmp(section, "LLA") == 0)
			return static_cast<int>(Gw2Ui::Icon::PvP);
		if (std::strcmp(section, "Invasions") == 0)
			return static_cast<int>(Gw2Ui::Icon::Alert);
		if (std::strcmp(section, "Fractal Incursions") == 0)
			return static_cast<int>(Gw2Ui::Icon::Gem);
		if (std::strcmp(section, "Festivals") == 0)
			return static_cast<int>(Gw2Ui::Icon::Contacts);
		if (std::strcmp(section, "Icebrood Saga") == 0)
			return static_cast<int>(Gw2Ui::Icon::CompassRadar);
		if (std::strcmp(section, "Living World") == 0)
			return static_cast<int>(Gw2Ui::Icon::Story);
		if (std::strcmp(section, "Heart of Thorns") == 0)
			return static_cast<int>(Gw2Ui::Icon::PathingMap);
		if (std::strcmp(section, "Path of Fire") == 0)
			return static_cast<int>(Gw2Ui::Icon::Key);
		if (std::strcmp(section, "End of Dragons") == 0)
			return static_cast<int>(Gw2Ui::Icon::Trophy);
		if (std::strcmp(section, "Secrets of the Obscure") == 0)
			return static_cast<int>(Gw2Ui::Icon::ApiHourglass);
		if (std::strcmp(section, "Janthir Wilds") == 0)
			return static_cast<int>(Gw2Ui::Icon::FarmSack);
		if (std::strcmp(section, "Visions of Eternity") == 0)
			return static_cast<int>(Gw2Ui::Icon::SheetsBook);
		return static_cast<int>(Gw2Ui::Icon::EventsMedal);
	}
}

void EventsPad::OpenAndRefresh()
{
	G::ShowEvents = true;
	EventsPadDetail::gFocus = true;
	EventsPadDetail::gPlaceOnce = true;
	/* Wine soft-open: Save on Soft Begin after Mirror tipped — Capture dirties later. */
	if (!WinePadOpen::Soft())
		Settings::SetDirty();
	EventsPadDetail::gDeferRefresh = WinePadOpen::DeferFrames();
	if (EventsPadDetail::gDeferRefresh <= 0)
		EventsPadDetail::BeginClaimRefresh();
}

bool EventsPad::Render()
{
	using namespace EventsPadDetail;

	const bool detail = CrashTrail::DetailArmed();
	ApplyClaimResult();
	if (!G::ShowEvents)
		return false;

	if (WinePadOpen::TickDefer(gDeferRefresh))
		BeginClaimRefresh();

	size_t nSec = 0;
	const char* const* sections = EventsData::Sections(&nSec);
	size_t nAll = 0;
	const EventsData::Entry* all = EventsData::All(&nAll);

	/* Only deep-probe the soft-open settle frame (not every 
	   detail frame — that flooded the ring ) . */
	const bool deep = detail
		&& WinePadOpen::CompanionSettleFrames() == WinePadOpen::DeferFrames() * 4;
	if (deep)
		CrashTrail::NoteF("ev:enter nSec=%zu nAll=%zu defer=%d place=%d settle=%d",
			nSec, nAll, gDeferRefresh, gPlaceOnce ? 1 : 0,
			WinePadOpen::CompanionSettleFrames());

	if (deep)
		CrashTrail::Note("ev:pre GetIO");
	const ImGuiIO& io = ImGui::GetIO();
	if (deep)
		CrashTrail::NoteF("ev:post GetIO display=%.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
	if (deep)
		CrashTrail::Note("ev:pre MaxH");
	const float maxH = PadDock::MaxH(300.f);
	if (deep)
		CrashTrail::NoteF("ev:post MaxH=%.0f", maxH);
	if (deep)
		CrashTrail::Note("ev:pre SetSizeConstraints");
	/* Wine soft-open settle : SetSizeConstraints skips FindWindow 
	   (stale ImGuiWindow tip ) ; still applies min / max . */
	PadDock::SetSizeConstraints("World Events##GW2InGameHelperEvents", 380.f, 300.f, PadDock::MaxW(620.f), maxH);
	if (deep)
		CrashTrail::Note("ev:post SetSizeConstraints");
	if (deep)
		CrashTrail::Note("ev:pre SetNextWindowCollapsed");
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (deep)
		CrashTrail::Note("ev:post SetNextWindowCollapsed");
	{
		if (deep)
			CrashTrail::Note("ev:pre PadFallback");
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.46f) : 160.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.1f) : 80.f;
		if (deep)
			CrashTrail::NoteF("ev:post PadFallback fx=%.0f fy=%.0f", fx, fy);
		if (deep)
			CrashTrail::Note("ev:pre Place");
		PadDock::Place(G::PadEvents, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
		if (deep)
			CrashTrail::Note("ev:post Place");
	}
	if (!gPlaceOnce && G::PadEvents.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	bool open = G::ShowEvents;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (deep)
		CrashTrail::Note("ev:pre Begin");
	const bool padBody = ImGui::Begin("World Events##GW2InGameHelperEvents", &open, HelperTheme::PadFlags());
	if (deep)
		CrashTrail::NoteF("ev:post Begin body=%d open=%d", padBody ? 1 : 0, open ? 1 : 0);
	if (!theme.AfterBegin("World Events", &open) || !padBody)
	{
		if (deep)
			CrashTrail::Note("ev:early EndPad");
		if (PadDock::Capture(G::PadEvents))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
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
	if (PadDock::Capture(G::PadEvents))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	/* Section rail: 0 = All, 1..n = named section. */
	if (gSectionPick < 0 || static_cast<size_t>(gSectionPick) > nSec)
		gSectionPick = 0;
	static const char* kAll = "All";
	const char* railLabels[64];
	const int railCount = 1 + static_cast<int>(nSec < 63 ? nSec : 63);
	railLabels[0] = kAll;
	for (int i = 1; i < railCount; ++i)
		railLabels[i] = sections[static_cast<size_t>(i - 1)];
	int railIcons[64];
	railIcons[0] = static_cast<int>(Gw2Ui::Icon::Map); /* All */
	for (int i = 1; i < railCount; ++i)
		railIcons[i] = IconForEventsSection(railLabels[i]);
	if (deep)
		CrashTrail::Note("ev:pre rail");
	gSectionPick = PadNav::DrawSideRail("###gw2igh_ev_nav", railLabels, railCount, gSectionPick,
		0.f, railIcons);
	if (deep)
		CrashTrail::Note("ev:pre body");

	ImGui::BeginChild("###gw2igh_ev_body", ImVec2(0.f, 0.f), true);
	PadNav::Blurb("Catalog times, not live HP. Hover for Wiki / MetaBattle notes.");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"UTC schedule for bosses and map metas.\n"
			"Track pins events. This map uses MumbleLink.\n"
			"Claim badges need an API key.\n"
			"Wiki / MetaBattle open as links only.");

	/* Filter / alert chips wrap — default pad width cannot fit one long SameLine row. */
	auto checkW = [](const char* label) -> float {
		const char* hash = std::strstr(label, "###");
		const float textW = hash
			? ImGui::CalcTextSize(label, hash).x
			: ImGui::CalcTextSize(label).x;
		return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + textW;
	};
	auto wrapCheck = [&](const char* label, bool* v, bool first) -> bool {
		if (!first)
			PadNav::WrapSameLine(checkW(label));
		return ImGui::Checkbox(label, v);
	};

	if (PadNav::RefreshButton("###gw2igh_ev_ref"))
		BeginClaimRefresh();
	wrapCheck("Tracked###gw2igh_ev_trackonly", &gTrackedOnly, false);
	wrapCheck("<=30m###gw2igh_ev_soon", &gSoonOnly, false);
	wrapCheck("This map###gw2igh_ev_thismap", &gThisMapOnly, false);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Show only events for your current open-world map\n"
			"(read-only MumbleLink map id).");

	if (wrapCheck("Alerts###gw2igh_ev_alerts", &G::EventAlerts, false))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Toasts: 10 min and 5 min before start, when a spawn goes live\n"
			"(including short 5-min windows), then 10 min and 5 min before it ends.");
	if (wrapCheck("Alert track###gw2igh_ev_alerts_trk", &G::EventAlertsTrackedOnly, false))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Limit alerts to your Track list only.\n"
			"Off (default): notify for all catalog events.");
	if (wrapCheck("Alert map###gw2igh_ev_alerts_map", &G::EventAlertsThisMap, false))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Limit alerts to events for your current open-world map\n"
			"(read-only MumbleLink map id). Combines with Alert track.");

	if (ImGui::SmallButton("Place alert###gw2igh_ev_place"))
		EventAlert::BeginPlacement();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Show a sample toast you can drag.\n"
			"Position is saved for all future event alerts.");
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset alert pos###gw2igh_ev_resetpos"))
		EventAlert::ResetPosition();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Restore the default (upper-center) alert position.");

	if (gThisMapOnly)
	{
		const unsigned mapId = CurrentMapId();
		const char* mapName = EventsData::MapDisplayName(mapId);
		if (mapId == 0)
			ImGui::TextColored(HelperTheme::Warn, "This map: waiting for MumbleLink...");
		else if (mapName)
			ImGui::TextColored(HelperTheme::GoldMuted, "This map: %s", mapName);
		else
			ImGui::TextColored(HelperTheme::GoldMuted, "This map: id %u (no timetable rows tagged)", mapId);
	}

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_ev_search", "Search events (name, map, section)...",
		gSearch, sizeof(gSearch));

	if (gBusy)
		ImGui::TextColored(HelperTheme::GoldMuted, "Loading...");
	else if (gStatus[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gStatus);

	ImGui::Separator();

	const time_t now = std::time(nullptr);
	std::vector<Row> rows;
	if (deep)
		CrashTrail::Note("ev:pre CollectRows");
	CollectRows(rows, now);
	if (deep)
		CrashTrail::NoteF("ev:post CollectRows n=%zu", rows.size());

	if (!rows.empty())
	{
		size_t heroI = static_cast<size_t>(-1);
		int bestSoon = 0x7fffffff;
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const Row& cand = rows[i];
			const EventsData::Entry& ce = all[static_cast<size_t>(cand.index)];
			if (EventsData::IsSpawnLive(ce, cand.timing))
			{
				heroI = i;
				break;
			}
			if (!cand.timing.live && cand.timing.untilStart >= 0 &&
				cand.timing.untilStart < bestSoon)
			{
				bestSoon = cand.timing.untilStart;
				heroI = i;
			}
		}
		if (heroI != static_cast<size_t>(-1))
		{
			const Row& hr = rows[heroI];
			const EventsData::Entry& he = all[static_cast<size_t>(hr.index)];
			const bool heroLive = EventsData::IsSpawnLive(he, hr.timing);
			char heroVal[48];
			const char* kicker = heroLive ? "Live now" : "Up next";
			if (heroLive)
				std::snprintf(heroVal, sizeof(heroVal), "ends %s",
					FmtRemain(hr.timing.untilEnd).c_str());
			else
				std::snprintf(heroVal, sizeof(heroVal), "%s",
					FmtRemain(hr.timing.untilStart).c_str());
			PadLayout::Hero("###gw2igh_ev_hero", kicker, he.title, heroVal);
		}
	}

	const float footerH = ImGui::GetTextLineHeightWithSpacing() * 1.6f;
	float listH = ImGui::GetContentRegionAvail().y - footerH;
	if (listH < 120.f) listH = 120.f;
	if (deep)
		CrashTrail::Note("ev:pre list");
	ImGui::BeginChild("###gw2igh_ev_list", ImVec2(0.f, listH), true);

	if (rows.empty())
	{
		if (gThisMapOnly && CurrentMapId() == 0)
			ImGui::TextWrapped("Turn on MumbleLink / enter the world - then This map can filter.");
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
			ImGui::TextColored(HelperTheme::GoldDim, "%s", e.mapLabel);
			lastMap = e.mapLabel;
		}

		char when[48];
		ImVec4 whenCol = HelperTheme::Muted;
		const bool spawnLive = EventsData::IsSpawnLive(e, r.timing);
		if (spawnLive)
		{
			std::snprintf(when, sizeof(when), "%s", FmtRemain(r.timing.untilEnd).c_str());
			whenCol = HelperTheme::Ok;
		}
		else if (r.timing.live)
		{
			std::snprintf(when, sizeof(when), "%s", FmtRemain(r.timing.untilEnd).c_str());
			whenCol = HelperTheme::GoldMuted;
		}
		else
		{
			std::snprintf(when, sizeof(when), "%s", FmtRemain(r.timing.untilStart).c_str());
			whenCol = r.warn ? HelperTheme::GoldBright : HelperTheme::Ink;
		}
		const char* chip = nullptr;
		ImVec4 chipFill{}, chipText = HelperTheme::Ink;
		if (spawnLive)
		{
			chip = "LIVE";
			chipFill = ImVec4(0.22f, 0.38f, 0.18f, 1.f);
			chipText = HelperTheme::Ok;
		}
		else if (r.tracked)
		{
			chip = "TRACK";
			chipFill = ImVec4(0.28f, 0.21f, 0.11f, 1.f);
			chipText = HelperTheme::GoldBright;
		}
		PadLayout::TitleRow(chip, chipFill, chipText, e.title, when, whenCol);
		if (r.timing.live && !spawnLive && ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Map phase window (not a spawn). Right side is time left in the phase.");

		const bool boss = EntryBossClaimed(e);
		const bool chest = EntryChestClaimed(e);
		if (boss)
		{
			PadLayout::Chip("boss", ImVec4(0.16f, 0.28f, 0.14f, 1.f), HelperTheme::Ok);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("World boss claimed today (account API).");
			if (chest)
				ImGui::SameLine(0.f, 4.f);
		}
		if (chest)
		{
			PadLayout::Chip("chest", ImVec4(0.16f, 0.28f, 0.14f, 1.f), HelperTheme::Ok);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Map chest claimed today (account API).");
		}
		if (boss || chest)
			ImGui::SameLine(0.f, 6.f);
		ImGui::TextColored(HelperTheme::Muted, "%s", e.section);

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
		ImGui::SameLine();
		if (ImGui::SmallButton("Wiki"))
			OpenEventWiki(e);
		ImGui::SameLine();
		if (ImGui::SmallButton("Meta"))
			OpenEventMetaBattle(e);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Open MetaBattle in your system browser (link only).");

		if (i + 1 < rows.size())
			ImGui::Dummy(ImVec2(0.f, 2.f));
		ImGui::PopID();
	}

	ImGui::EndChild();

	int trackedN = 0;
	{
		std::vector<std::string> ids;
		ParseTrackCsv(G::EventTrackIds, ids);
		trackedN = static_cast<int>(ids.size());
	}
	ImGui::TextColored(HelperTheme::Muted,
		"%d tracked | %d shown | %d in catalog",
		trackedN, static_cast<int>(rows.size()), static_cast<int>(nAll));
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	if (deep)
		CrashTrail::Note("ev:pre EndPad");
	HelperTheme::EndPad();
	if (deep)
		CrashTrail::Note("ev:end");
	return hovered;
}
