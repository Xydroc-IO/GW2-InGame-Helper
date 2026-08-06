#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"
#include "TrailToolsBinds.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

void TrailToolsDetail::DrawTrailRawEditor()
{
	EnsureWorkspace();
	SyncActiveType();
	if (gDraft.active.fileRel.empty())
		SyncActiveFileRelFromStem();

	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Raw trail (.trl). New / Load / Save manage the binary. Map+vector starts at your feet. "
		"Use Insert into XML on the Trails desk when ready.");
	PadNav::PopWrap();

	if (ImGui::Button("New###gw2igh_tt_newtrl"))
	{
		SyncActiveFileRelFromStem();
		gDraft.active = {};
		gDraft.active.type = gDraft.trailType[0] ? gDraft.trailType
			: (RootCategoryName() + ".t.extrail");
		SyncActiveFileRelFromStem();
		gDraft.selectedTrail = -1;
		gDraft.selectedPoint = -1;
		gDraft.trailDirty = false;
		SetStatus("New empty trail - Map+vector or Add vector, then Save.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Load..."));
	if (ImGui::Button("Load...###gw2igh_tt_load"))
	{
		std::wstring path;
		if (!DialogPickTrl(false, path))
			SetStatus("Load cancelled.");
		else
		{
			uint32_t mid = 0;
			std::vector<PathingTrails::WorldPoint> pts;
			if (!TrailToolsTrl::Read(path, mid, pts))
				SetStatus("Load failed.");
			else
			{
				RememberDirFromPath(path);
				gDraft.active.mapId = mid;
				gDraft.active.points = std::move(pts);
				gDraft.selectedPoint = -1;
				std::string under;
				if (TryAbsUnderPack(path, under))
				{
					gDraft.active.fileRel = under;
					ApplyStemFromFileRel();
				}
				else
				{
					const size_t slash = path.find_last_of(L"\\/");
					std::wstring name = slash == std::wstring::npos ? path
						: path.substr(slash + 1);
					std::string stem = WideToUtf8(name);
					if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".trl") == 0)
						stem.resize(stem.size() - 4);
					std::snprintf(gDraft.trailFileStem, sizeof(gDraft.trailFileStem), "%s",
						stem.c_str());
					SyncActiveFileRelFromStem();
				}
				SyncActiveType();
				gDraft.trailDirty = false;
				gDraft.selectedTrail = -1;
				SetStatus("Loaded map %u, %zu points.", mid, gDraft.active.points.size());
			}
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Save"));
	if (ImGui::Button("Save###gw2igh_tt_save"))
	{
		SyncActiveFileRelFromStem();
		SaveActiveToPath(ActiveTrlPath());
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Save As..."));
	if (ImGui::Button("Save As...###gw2igh_tt_saveas"))
	{
		std::wstring path;
		if (!DialogPickTrl(true, path))
			SetStatus("Save As cancelled.");
		else
			SaveActiveToPath(path);
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Insert XML"));
	if (ImGui::Button("Insert XML###gw2igh_tt_raw_ins"))
		UpsertActiveTrailInPack();

	PadNav::PushWidthForLabel("Trail file stem###gw2igh_tt_trlstem");
	ImGui::InputText("Trail file stem###gw2igh_tt_trlstem", gDraft.trailFileStem,
		sizeof(gDraft.trailFileStem));
	PadNav::PopWidthForLabel();
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SyncActiveFileRelFromStem();
		MarkDirty();
	}
	ImGui::TextDisabled("%s%s", gDraft.active.fileRel.c_str(),
		gDraft.trailDirty ? " *" : "");

	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	const bool pose = ReadMumblePose(mapId, x, y, z);

	ImGui::Separator();
	ImGui::TextUnformatted("Recording");
	{
		auto& kb = TrailToolsBinds::Get();
		if (kb.trailRecording)
			ImGui::TextColored(kb.trailPaused ? HelperTheme::Muted : HelperTheme::Ok,
				kb.trailPaused ? "Paused - %s" : "Recording - %s",
				TrailToolsBinds::FormatChord(kb.trailStart).c_str());
		else
			ImGui::TextDisabled("Idle - Start: %s",
				TrailToolsBinds::FormatChord(kb.trailStart).c_str());
		if (ImGui::Button("Start / resume###gw2igh_tt_rec"))
			TrailToolsBinds::ActionTrailStart();
		PadNav::WrapSameLine(PadNav::ButtonWidth("Pause"));
		if (ImGui::Button("Pause###gw2igh_tt_recpause"))
			TrailToolsBinds::ActionTrailPause();
		PadNav::WrapSameLine(PadNav::ButtonWidth("Stop"));
		if (ImGui::Button("Stop###gw2igh_tt_recstop"))
		{
			kb.trailRecording = false;
			kb.trailPaused = false;
			SetStatus("Recording stopped.");
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Segments");
	if (ImGui::Button("Map only###gw2igh_tt_insmap"))
	{
		if (!pose)
			SetStatus("No Mumble pose for map.");
		else
		{
			gDraft.active.mapId = mapId;
			MarkDirty();
			SetStatus("Trail map set to %u (no vector).", mapId);
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Map + vector"));
	if (ImGui::Button("Map + vector###gw2igh_tt_mapvec"))
	{
		if (!pose)
			SetStatus("No Mumble pose.");
		else
		{
			gDraft.active.mapId = mapId;
			gDraft.active.points.push_back({ x, y, z });
			gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
			MarkDirty();
			SetStatus("Map %u + point #%zu at feet.", mapId, gDraft.active.points.size());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Add vector"));
	if (ImGui::Button("Add vector###gw2igh_tt_insvec"))
	{
		if (!pose)
			SetStatus("No Mumble pose.");
		else if (gDraft.active.mapId == 0)
			SetStatus("Set map first (Map only or Map + vector).");
		else if (gDraft.active.mapId != mapId)
			SetStatus("Map mismatch - trail %u, you %u.", gDraft.active.mapId, mapId);
		else
		{
			gDraft.active.points.push_back({ x, y, z });
			gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
			MarkDirty();
			SetStatus("Point #%zu at (%.2f, %.2f, %.2f).",
				gDraft.active.points.size(), x, y, z);
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("New section"));
	if (ImGui::Button("New section###gw2igh_tt_sec"))
	{
		gDraft.active.points.push_back({ 0.f, 0.f, 0.f });
		gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
		MarkDirty();
		SetStatus("Section break (0,0,0) added.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Undo"));
	if (ImGui::Button("Undo###gw2igh_tt_undo"))
	{
		if (!gDraft.active.points.empty())
		{
			gDraft.active.points.pop_back();
			if (gDraft.selectedPoint >= static_cast<int>(gDraft.active.points.size()))
				gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
			MarkDirty();
			SetStatus("Undid last point (%zu left).", gDraft.active.points.size());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Clear"));
	if (ImGui::Button("Clear###gw2igh_tt_clr"))
	{
		gDraft.active.points.clear();
		gDraft.selectedPoint = -1;
		MarkDirty();
		SetStatus("Cleared active trail points.");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Edit points");
	if (ImGui::Button("Select nearest###gw2igh_tt_near"))
	{
		if (!pose)
			SetStatus("No Mumble pose.");
		else
		{
			int best = -1;
			float bestD = 1.e30f;
			for (int i = 0; i < static_cast<int>(gDraft.active.points.size()); ++i)
			{
				const auto& p = gDraft.active.points[static_cast<size_t>(i)];
				if (IsSectionBreak(p))
					continue;
				const float dx = p.x - x, dy = p.y - y, dz = p.z - z;
				const float d = dx * dx + dy * dy + dz * dz;
				if (d < bestD)
				{
					bestD = d;
					best = i;
				}
			}
			if (best < 0)
				SetStatus("No vectors to select.");
			else
			{
				gDraft.selectedPoint = best;
				SetStatus("Selected #%d (%.1fm away).", best, std::sqrt(bestD));
			}
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Move to feet"));
	if (ImGui::Button("Move to feet###gw2igh_tt_movefoot"))
	{
		const int s = gDraft.selectedPoint;
		if (s < 0 || s >= static_cast<int>(gDraft.active.points.size()))
			SetStatus("Select a point first.");
		else if (!pose)
			SetStatus("No Mumble pose.");
		else if (IsSectionBreak(gDraft.active.points[static_cast<size_t>(s)]))
			SetStatus("Cannot move a section break - pick a vector.");
		else
		{
			auto& pt = gDraft.active.points[static_cast<size_t>(s)];
			pt.x = x;
			pt.y = y;
			pt.z = z;
			MarkDirty();
			SetStatus("Moved #%d to feet.", s);
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Delete sel"));
	if (ImGui::Button("Delete sel###gw2igh_tt_delsel"))
	{
		const int s = gDraft.selectedPoint;
		if (s < 0 || s >= static_cast<int>(gDraft.active.points.size()))
			SetStatus("Select a point first.");
		else
		{
			gDraft.active.points.erase(gDraft.active.points.begin() + s);
			if (gDraft.selectedPoint >= static_cast<int>(gDraft.active.points.size()))
				gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
			MarkDirty();
			SetStatus("Deleted point.");
		}
	}

	ImGui::Text("Active: map %u | %zu points%s", gDraft.active.mapId, gDraft.active.points.size(),
		gDraft.trailDirty ? " | modified" : "");
	if (ImGui::BeginChild("###gw2igh_tt_pts", ImVec2(0.f, 120.f), true))
	{
		for (int i = 0; i < static_cast<int>(gDraft.active.points.size()); ++i)
		{
			const auto& p = gDraft.active.points[static_cast<size_t>(i)];
			const bool brk = IsSectionBreak(p);
			char lab[96]{};
			if (brk)
				std::snprintf(lab, sizeof(lab), "%4d  [section break]", i);
			else
				std::snprintf(lab, sizeof(lab), "%4d  %.3f  %.3f  %.3f", i, p.x, p.y, p.z);
			if (ImGui::Selectable(lab, gDraft.selectedPoint == i))
				gDraft.selectedPoint = i;
		}
	}
	ImGui::EndChild();
	if (gDraft.selectedPoint >= 0 &&
		gDraft.selectedPoint < static_cast<int>(gDraft.active.points.size()))
	{
		auto& pt = gDraft.active.points[static_cast<size_t>(gDraft.selectedPoint)];
		if (ImGui::DragFloat3("Edit point XYZ###gw2igh_tt_ptedit", &pt.x, 0.05f))
			MarkDirty();
		if (ImGui::SmallButton("Delete point###gw2igh_tt_ptdel"))
		{
			gDraft.active.points.erase(gDraft.active.points.begin() + gDraft.selectedPoint);
			gDraft.selectedPoint = -1;
			MarkDirty();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Insert after###gw2igh_tt_ptins"))
		{
			gDraft.active.points.insert(
				gDraft.active.points.begin() + gDraft.selectedPoint + 1, pt);
			++gDraft.selectedPoint;
			MarkDirty();
		}
	}

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
