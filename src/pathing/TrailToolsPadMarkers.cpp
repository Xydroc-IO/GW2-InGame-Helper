#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void TrailToolsDetail::DrawMarkersTab()
{
	ImGui::TextUnformatted("Markers");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Drop a POI at your feet. Gold dots preview on compass + world GPS while this pad is open "
		"(Live → draft preview). After Build, enable the pack under Pathing if needed.");
	PadNav::PopWrap();

	std::vector<std::string> leaves;
	CollectLeafPaths(gDraft.root, "", leaves, false);
	if (leaves.empty())
		leaves.push_back(gDraft.markerType[0] ? gDraft.markerType : "examplepack.m.exm");

	int cur = 0;
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		if (leaves[i] == gDraft.markerType)
		{
			cur = static_cast<int>(i);
			break;
		}
	}
	if (ImGui::BeginCombo("Marker type###gw2igh_tt_mtype", leaves[static_cast<size_t>(cur)].c_str()))
	{
		for (size_t i = 0; i < leaves.size(); ++i)
		{
			const bool sel = static_cast<int>(i) == cur;
			if (ImGui::Selectable(leaves[i].c_str(), sel))
			{
				cur = static_cast<int>(i);
				std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "%s", leaves[i].c_str());
			}
			if (sel)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::InputText("Or type path###gw2igh_tt_mtype_edit", gDraft.markerType, sizeof(gDraft.markerType));

	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	const bool pose = ReadMumblePose(mapId, x, y, z);

	static bool sThisMapOnly = true;
	ImGui::Checkbox("List this map only###gw2igh_tt_mmap", &sThisMapOnly);

	if (ImGui::Button("Drop marker here###gw2igh_tt_drop"))
	{
		if (!pose)
			SetStatus("No Mumble pose.");
		else if (!gDraft.markerType[0])
			SetStatus("Set a marker type path.");
		else
		{
			EnsureWorkspace();
			DraftPoi p;
			p.mapId = mapId;
			p.x = x;
			p.y = y;
			p.z = z;
			p.type = gDraft.markerType;
			p.guid = MakeGuidBase64();
			gDraft.pois.push_back(std::move(p));
			gDraft.selectedPoi = static_cast<int>(gDraft.pois.size()) - 1;
			SetStatus("Dropped marker #%zu — gold preview on compass/world.", gDraft.pois.size());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Delete selected"));
	if (ImGui::Button("Delete selected###gw2igh_tt_mdel") &&
		gDraft.selectedPoi >= 0 &&
		gDraft.selectedPoi < static_cast<int>(gDraft.pois.size()))
	{
		gDraft.pois.erase(gDraft.pois.begin() + gDraft.selectedPoi);
		gDraft.selectedPoi = -1;
		SetStatus("Deleted marker.");
	}

	size_t shown = 0;
	for (const DraftPoi& p : gDraft.pois)
	{
		if (sThisMapOnly && pose && p.mapId != mapId)
			continue;
		++shown;
	}

	ImGui::Separator();
	ImGui::Text("%zu shown / %zu total", shown, gDraft.pois.size());
	if (ImGui::BeginChild("###gw2igh_tt_mlist", ImVec2(0.f, 200.f), true))
	{
		for (int i = 0; i < static_cast<int>(gDraft.pois.size()); ++i)
		{
			const DraftPoi& p = gDraft.pois[static_cast<size_t>(i)];
			if (sThisMapOnly && pose && p.mapId != mapId)
				continue;
			char label[256]{};
			std::snprintf(label, sizeof(label), "%d  map %u  %s###gw2igh_tt_mi%d",
				i, p.mapId, p.type.c_str(), i);
			if (ImGui::Selectable(label, gDraft.selectedPoi == i))
				gDraft.selectedPoi = i;
		}
	}
	ImGui::EndChild();

	if (gDraft.selectedPoi >= 0 && gDraft.selectedPoi < static_cast<int>(gDraft.pois.size()))
	{
		DraftPoi& p = gDraft.pois[static_cast<size_t>(gDraft.selectedPoi)];
		ImGui::Separator();
		ImGui::Text("Edit selected");
		ImGui::DragFloat3("XYZ###gw2igh_tt_mnudge", &p.x, 0.05f);
		if (ImGui::SmallButton("+X")) p.x += 0.5f;
		ImGui::SameLine();
		if (ImGui::SmallButton("-X")) p.x -= 0.5f;
		ImGui::SameLine();
		if (ImGui::SmallButton("+Z")) p.z += 0.5f;
		ImGui::SameLine();
		if (ImGui::SmallButton("-Z")) p.z -= 0.5f;
		ImGui::SameLine();
		if (ImGui::SmallButton("+Y")) p.y += 0.25f;
		ImGui::SameLine();
		if (ImGui::SmallButton("-Y")) p.y -= 0.25f;

		char line[512]{};
		std::snprintf(line, sizeof(line),
			"<POI MapID=\"%u\" xpos=\"%.6g\" ypos=\"%.6g\" zpos=\"%.6g\" type=\"%s\" GUID=\"%s\"/>",
			p.mapId, p.x, p.y, p.z, p.type.c_str(), p.guid.c_str());
		if (ImGui::Button("Copy POI XML###gw2igh_tt_mcopy"))
		{
			CopyClipboard(line);
			SetStatus("Copied POI XML.");
		}
	}

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
