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

	void DrawTrailList()
	{
		using namespace TrailToolsDetail;
		ImGui::TextUnformatted("Trails in pack");
		if (ImGui::BeginChild("###gw2igh_tt_tlist", ImVec2(0.f, 90.f), true))
		{
			for (int i = 0; i < static_cast<int>(gDraft.trails.size()); ++i)
			{
				const DraftTrail& t = gDraft.trails[static_cast<size_t>(i)];
				ImGui::PushID(i);
				char lab[200]{};
				std::snprintf(lab, sizeof(lab), "%s  map %u  %zu pts",
					t.fileRel.c_str(), t.mapId, t.points.size());
				if (ImGui::Selectable(lab, gDraft.selectedTrail == i))
				{
					gDraft.selectedTrail = i;
					gDraft.active = t;
					std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "%s", t.type.c_str());
					/* stem from file name */
					const size_t slash = t.fileRel.find_last_of('/');
					std::string stem = slash == std::string::npos ? t.fileRel
						: t.fileRel.substr(slash + 1);
					if (stem.size() > 4 && stem.substr(stem.size() - 4) == ".trl")
						stem.resize(stem.size() - 4);
					std::snprintf(gDraft.trailFileStem, sizeof(gDraft.trailFileStem), "%s",
						stem.c_str());
					SetStatus("Editing trail %s", t.fileRel.c_str());
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Del"))
				{
					gDraft.trails.erase(gDraft.trails.begin() + i);
					if (gDraft.selectedTrail == i)
						gDraft.selectedTrail = -1;
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		if (ImGui::Button("New trail###gw2igh_tt_newtrl"))
		{
			SyncActiveMeta();
			gDraft.active = {};
			gDraft.active.type = gDraft.trailType[0] ? gDraft.trailType
				: (RootCategoryName() + ".t.extrail");
			gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" +
				(gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail") + ".trl";
			gDraft.selectedTrail = -1;
			SetStatus("New empty trail — Insert Vector then Save.");
		}
	}
}

void TrailToolsDetail::DrawTrailTab()
{
	SyncActiveMeta();
	EnsureWorkspace();

	ImGui::TextUnformatted("Trail recorder");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Insert Vector drops a point at your feet. New section inserts a (0,0,0) break.");
	PadNav::PopWrap();

	DrawTrailList();
	ImGui::Separator();

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
	static int sSelPt = -1;
	if (ImGui::BeginChild("###gw2igh_tt_pts", ImVec2(0.f, 120.f), true))
	{
		for (int i = 0; i < static_cast<int>(gDraft.active.points.size()); ++i)
		{
			const auto& p = gDraft.active.points[static_cast<size_t>(i)];
			const bool brk = p.x == 0.f && p.y == 0.f && p.z == 0.f;
			char lab[96]{};
			if (brk)
				std::snprintf(lab, sizeof(lab), "%4d  [section break]", i);
			else
				std::snprintf(lab, sizeof(lab), "%4d  %.3f  %.3f  %.3f", i, p.x, p.y, p.z);
			if (ImGui::Selectable(lab, sSelPt == i))
				sSelPt = i;
		}
	}
	ImGui::EndChild();
	if (sSelPt >= 0 && sSelPt < static_cast<int>(gDraft.active.points.size()))
	{
		auto& pt = gDraft.active.points[static_cast<size_t>(sSelPt)];
		ImGui::DragFloat3("Edit point XYZ###gw2igh_tt_ptedit", &pt.x, 0.05f);
		if (ImGui::SmallButton("Delete point###gw2igh_tt_ptdel"))
		{
			gDraft.active.points.erase(gDraft.active.points.begin() + sSelPt);
			sSelPt = -1;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Insert after###gw2igh_tt_ptins"))
		{
			gDraft.active.points.insert(gDraft.active.points.begin() + sSelPt + 1, pt);
			++sSelPt;
		}
	}

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
