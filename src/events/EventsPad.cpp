#include "EventsPad.h"
#include "EventsPadInternal.h"

#include "EventsData.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 300.f), ImVec2(PadDock::MaxW(620.f), maxH));
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
	if (!ImGui::Begin("World Events##GW2InGameHelperEvents", &open))
	{
		if (PadDock::Capture(G::PadEvents))
			Settings::SetDirty();
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
	if (PadDock::Capture(G::PadEvents))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale;

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
