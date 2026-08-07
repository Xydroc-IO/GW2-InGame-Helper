#include "EventsPad.h"
#include "EventsPadInternal.h"

#include "EventsData.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

void EventsPad::OpenAndRefresh()
{
	G::ShowEvents = true;
	EventsPadDetail::gFocus = true;
	EventsPadDetail::gPlaceOnce = true;
	Settings::SetDirty();
	EventsPadDetail::BeginClaimRefresh();
}

bool EventsPad::Render()
{
	using namespace EventsPadDetail;

	ApplyClaimResult();
	if (!G::ShowEvents)
		return false;

	size_t nSec = 0;
	const char* const* sections = EventsData::Sections(&nSec);
	size_t nAll = 0;
	const EventsData::Entry* all = EventsData::All(&nAll);

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(300.f);
	PadDock::SetSizeConstraints("World Events##GW2InGameHelperEvents", 380.f, 300.f, PadDock::MaxW(620.f), maxH);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.46f) : 160.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.1f) : 80.f;
		PadDock::Place(G::PadEvents, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadEvents.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowEvents;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("World Events##GW2InGameHelperEvents", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("World Events", &open) || !padBody)
	{
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
	railIcons[0] = static_cast<int>(Gw2Ui::Icon::Map);
	for (int i = 1; i < railCount; ++i)
		railIcons[i] = static_cast<int>(Gw2Ui::Icon::Achievements);
	gSectionPick = PadNav::DrawSideRail("###gw2igh_ev_nav", railLabels, railCount, gSectionPick,
		0.f, railIcons);

	ImGui::BeginChild("###gw2igh_ev_body", ImVec2(0.f, 0.f), true);
	PadNav::Blurb(
		"UTC timers for bosses and map metas. Track items you care about - "
		"they sort up and highlight within 10 minutes. "
		"Invasions / festivals / fractals stay hidden until you open that section or search/Track them.");

	if (PadNav::RefreshButton("###gw2igh_ev_ref"))
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
	CollectRows(rows, now);

	const float footerH = ImGui::GetTextLineHeightWithSpacing() * 1.6f;
	float listH = ImGui::GetContentRegionAvail().y - footerH;
	if (listH < 120.f) listH = 120.f;
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

		if (r.warn)
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);

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
			ImGui::TextColored(HelperTheme::Ok, "[claimed]");
		}

		ImGui::TextColored(HelperTheme::Muted, "%s", e.section);

		if (r.timing.live)
		{
			ImGui::TextColored(HelperTheme::Ok,
				"Active - ends in %s", FmtRemain(r.timing.untilEnd).c_str());
		}
		else
		{
			ImGui::TextColored(HelperTheme::Ink,
				"Next in %s", FmtRemain(r.timing.untilStart).c_str());
		}
		if (r.warn && !r.timing.live)
			ImGui::TextColored(HelperTheme::GoldBright, "Tracked - starting soon");

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
	ImGui::TextColored(HelperTheme::Muted,
		"%d tracked | %d shown | %d in catalog",
		trackedN, static_cast<int>(rows.size()), static_cast<int>(nAll));
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	HelperTheme::EndPad();
	return hovered;
}
