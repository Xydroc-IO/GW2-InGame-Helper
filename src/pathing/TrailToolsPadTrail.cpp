#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	std::wstring ActiveTrlPath()
	{
		std::wstring p = TrailToolsDetail::PackDir();
		p.push_back(L'\\');
		for (char c : TrailToolsDetail::gDraft.active.fileRel)
			p.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		return p;
	}

	void SyncActiveMeta()
	{
		using namespace TrailToolsDetail;
		if (gDraft.trailType[0])
			gDraft.active.type = gDraft.trailType;
		char stem[64]{};
		std::snprintf(stem, sizeof(stem), "%s",
			gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail");
		gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" + stem + ".trl";
	}
}

void TrailToolsDetail::DrawTrailTab()
{
	SyncActiveMeta();
	EnsureWorkspace();

	ImGui::TextUnformatted("Trail recorder");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Insert Vector drops a point at your feet. New section inserts a (0,0,0) break. "
		"One map per .trl file.");
	PadNav::PopWrap();

	ImGui::InputText("Trail file stem###gw2igh_tt_trlstem", gDraft.trailFileStem,
		sizeof(gDraft.trailFileStem));
	ImGui::InputText("Trail type###gw2igh_tt_trltype", gDraft.trailType, sizeof(gDraft.trailType));
	ImGui::TextDisabled("%s", gDraft.active.fileRel.c_str());

	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	const bool pose = ReadMumblePose(mapId, x, y, z);

	if (ImGui::Button("Insert Map###gw2igh_tt_insmap"))
	{
		if (!pose)
			SetStatus("No Mumble pose for map.");
		else
		{
			gDraft.active.mapId = mapId;
			SetStatus("Trail map set to %u.", mapId);
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Insert Vector"));
	if (ImGui::Button("Insert Vector###gw2igh_tt_insvec"))
	{
		if (!pose)
			SetStatus("No Mumble pose.");
		else
		{
			if (gDraft.active.mapId == 0)
				gDraft.active.mapId = mapId;
			if (gDraft.active.mapId != mapId)
				SetStatus("Map mismatch — Insert Map first (trail %u, you %u).",
					gDraft.active.mapId, mapId);
			else
			{
				gDraft.active.points.push_back({ x, y, z });
				SetStatus("Point #%zu at (%.2f, %.2f, %.2f).",
					gDraft.active.points.size(), x, y, z);
			}
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("New section"));
	if (ImGui::Button("New section###gw2igh_tt_sec"))
	{
		gDraft.active.points.push_back({ 0.f, 0.f, 0.f });
		SetStatus("Section break added.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Undo"));
	if (ImGui::Button("Undo###gw2igh_tt_undo"))
	{
		if (!gDraft.active.points.empty())
		{
			gDraft.active.points.pop_back();
			SetStatus("Undid last point (%zu left).", gDraft.active.points.size());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Clear"));
	if (ImGui::Button("Clear###gw2igh_tt_clr"))
	{
		gDraft.active.points.clear();
		SetStatus("Cleared active trail points.");
	}

	if (ImGui::Button("Save .trl###gw2igh_tt_save"))
	{
		SyncActiveMeta();
		if (gDraft.active.mapId == 0 || gDraft.active.points.size() < 2)
			SetStatus("Need map + at least 2 points to save.");
		else if (!TrailToolsTrl::Write(ActiveTrlPath(), gDraft.active.mapId, gDraft.active.points))
			SetStatus("Save failed.");
		else
		{
			bool found = false;
			for (auto& t : gDraft.trails)
			{
				if (t.fileRel == gDraft.active.fileRel)
				{
					t = gDraft.active;
					found = true;
					break;
				}
			}
			if (!found)
				gDraft.trails.push_back(gDraft.active);
			SetStatus("Saved %s (%zu pts).", gDraft.active.fileRel.c_str(),
				gDraft.active.points.size());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Load .trl"));
	if (ImGui::Button("Load .trl###gw2igh_tt_load"))
	{
		SyncActiveMeta();
		uint32_t mid = 0;
		std::vector<PathingTrails::WorldPoint> pts;
		if (!TrailToolsTrl::Read(ActiveTrlPath(), mid, pts))
			SetStatus("Load failed — save a trail first or check path.");
		else
		{
			gDraft.active.mapId = mid;
			gDraft.active.points = std::move(pts);
			SetStatus("Loaded map %u, %zu points.", mid, gDraft.active.points.size());
		}
	}

	ImGui::Separator();
	ImGui::Text("Active: map %u · %zu points", gDraft.active.mapId, gDraft.active.points.size());
	if (ImGui::BeginChild("###gw2igh_tt_pts", ImVec2(0.f, 180.f), true))
	{
		for (size_t i = 0; i < gDraft.active.points.size(); ++i)
		{
			const auto& p = gDraft.active.points[i];
			const bool brk = p.x == 0.f && p.y == 0.f && p.z == 0.f;
			if (brk)
				ImGui::TextDisabled("%4zu  [section break]", i);
			else
				ImGui::Text("%4zu  %.3f  %.3f  %.3f", i, p.x, p.y, p.z);
		}
	}
	ImGui::EndChild();

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
