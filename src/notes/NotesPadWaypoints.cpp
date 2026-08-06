#include "NotesPadInternal.h"
#include "PadNav.h"

#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace NotesPadDetail
{
	void CopyText(const char* text)
	{
		if (!text || !text[0])
			return;
		if (!OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const SIZE_T bytes = std::strlen(text) + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (mem)
		{
			void* locked = GlobalLock(mem);
			if (locked)
			{
				std::memcpy(locked, text, bytes);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
		}
		CloseClipboard();
	}

	void DrawWaypointsTab()
	{
		WaypointsData::EnsureLoaded(false);
		WaypointsData::Tick();

		PadNav::PushWrap();
		ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
			"Official API waypoints & POIs - Copy puts the chat code on your clipboard.");
		PadNav::PopWrap();

		if (WaypointsData::Busy())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s", WaypointsData::Status());
		else if (!WaypointsData::Ready())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", WaypointsData::Status());
			if (ImGui::Button("Load waypoints###gw2igh_wp_load"))
				WaypointsData::EnsureLoaded(true);
			return;
		}
		else
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", WaypointsData::Status());

		ImGui::Checkbox("Waypoints only###gw2igh_wp_wponly", &gWpWaypointsOnly);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reload###gw2igh_wp_reload"))
			WaypointsData::EnsureLoaded(true);

		if (ImGui::RadioButton("Search###gw2igh_wp_m0", gWpMode == 0)) gWpMode = 0;
		ImGui::SameLine();
		if (ImGui::RadioButton("By map###gw2igh_wp_m1", gWpMode == 1)) gWpMode = 1;
		ImGui::SameLine();
		if (ImGui::RadioButton("This map###gw2igh_wp_m2", gWpMode == 2)) gWpMode = 2;

		std::vector<WaypointsData::Poi> hits;
		std::vector<WaypointsData::MapRow> maps;

		if (gWpMode == 0)
		{
			ImGui::SetNextItemWidth(-1.f);
			ImGui::InputTextWithHint("###gw2igh_wp_q", "Waypoint or map name...",
				gWpQuery, sizeof(gWpQuery));
			if (gWpQuery[0])
				WaypointsData::Search(gWpQuery, gWpWaypointsOnly, hits, 100);
			else
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
					"Type a name - e.g. fort trinity, lion's arch.");
		}
		else if (gWpMode == 1)
		{
			ImGui::SetNextItemWidth(-1.f);
			ImGui::InputTextWithHint("###gw2igh_wp_mf", "Filter maps...",
				gWpMapFilter, sizeof(gWpMapFilter));
			WaypointsData::ListMaps(gWpMapFilter, maps, 60);
			const float mapListH = 120.f;
			ImGui::BeginChild("###gw2igh_wp_maps", ImVec2(0.f, mapListH), true);
			for (const WaypointsData::MapRow& m : maps)
			{
				ImGui::PushID(m.id);
				char label[160];
				std::snprintf(label, sizeof(label), "%s  (%d wp)",
					m.name.c_str(), m.waypointCount);
				if (ImGui::Selectable(label, gWpMapId == m.id))
				{
					gWpMapId = m.id;
					gWpMapName = m.name;
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
			if (gWpMapId > 0)
			{
				ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f), "%s",
					gWpMapName.empty() ? "Map" : gWpMapName.c_str());
				WaypointsData::ListForMap(gWpMapId, gWpWaypointsOnly, hits);
			}
		}
		else
		{
			const int cur = WaypointsData::CurrentMapId();
			if (cur <= 0)
			{
				ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
					"Waiting for MumbleLink / enter the world.");
			}
			else
			{
				gWpMapId = cur;
				WaypointsData::ListForMap(cur, gWpWaypointsOnly, hits);
				const char* mapLabel = hits.empty() ? "This map" : hits[0].mapName.c_str();
				ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f),
					"%s  (#%d)", mapLabel, cur);
				if (hits.empty())
					ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
						"No waypoints indexed for this map id.");
			}
		}

		if (gWpCopied[0])
		{
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f),
				"Copied %s", gWpCopied);
		}

		const float listH = ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild("###gw2igh_wp_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);
		for (size_t i = 0; i < hits.size(); ++i)
		{
			const WaypointsData::Poi& p = hits[i];
			ImGui::PushID(static_cast<int>(p.id));
			ImGui::TextUnformatted(p.name.c_str());
			if (gWpMode == 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), " |  %s", p.mapName.c_str());
			}
			if (!gWpWaypointsOnly && p.type != "waypoint")
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.60f, 0.55f, 0.45f, 1.f), "[%s]", p.type.c_str());
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", p.chatLink.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy"))
			{
				CopyText(p.chatLink.c_str());
				std::snprintf(gWpCopied, sizeof(gWpCopied), "%s", p.chatLink.c_str());
			}
			ImGui::PopID();
		}
		if (hits.empty() && ((gWpMode == 0 && gWpQuery[0]) ||
				(gWpMode == 1 && gWpMapId > 0) ||
				(gWpMode == 2 && gWpMapId > 0)))
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "No matches.");
		}
		ImGui::EndChild();
	}
}
