#include "TrailToolsInternal.h"
#include "TrailToolsPad.h"
#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"
#include "TrailToolsXml.h"
#include "TrailToolsBinds.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <commdlg.h>

namespace
{
	std::wstring Utf8ToWide(const char* u)
	{
		if (!u || !*u)
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring w(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u, -1, w.data(), n);
		return w;
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string s(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
		return s;
	}

	std::wstring PackRelToAbs(const std::string& fileRel)
	{
		std::wstring p = TrailToolsDetail::PackDir();
		p.push_back(L'\\');
		for (char c : fileRel)
			p.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		return p;
	}

	std::wstring TrailsFolder()
	{
		using namespace TrailToolsDetail;
		std::wstring p = PackDir();
		p += L"\\Data\\";
		for (const char* c = gDraft.packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += L"\\Trails";
		return p;
	}

	std::wstring ActiveTrlPath()
	{
		return PackRelToAbs(TrailToolsDetail::gDraft.active.fileRel);
	}

	void RememberDirFromPath(const std::wstring& fullPath)
	{
		using namespace TrailToolsDetail;
		const size_t slash = fullPath.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return;
		const std::string dir = WideToUtf8(fullPath.substr(0, slash));
		std::snprintf(gDraft.lastTrlDir, sizeof(gDraft.lastTrlDir), "%s", dir.c_str());
		Settings::SetDirty();
	}

	std::wstring DialogStartDir()
	{
		using namespace TrailToolsDetail;
		if (gDraft.lastTrlDir[0])
			return Utf8ToWide(gDraft.lastTrlDir);
		return TrailsFolder();
	}

	bool IsSectionBreak(const PathingTrails::WorldPoint& p)
	{
		return p.x == 0.f && p.y == 0.f && p.z == 0.f;
	}

	void SyncActiveType()
	{
		using namespace TrailToolsDetail;
		if (gDraft.trailType[0])
			gDraft.active.type = gDraft.trailType;
	}

	void SyncActiveFileRelFromStem()
	{
		using namespace TrailToolsDetail;
		const char* stem = gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail";
		gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" + stem + ".trl";
	}

	void ApplyStemFromFileRel()
	{
		using namespace TrailToolsDetail;
		const size_t slash = gDraft.active.fileRel.find_last_of('/');
		std::string stem = slash == std::string::npos ? gDraft.active.fileRel
			: gDraft.active.fileRel.substr(slash + 1);
		if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".trl") == 0)
			stem.resize(stem.size() - 4);
		std::snprintf(gDraft.trailFileStem, sizeof(gDraft.trailFileStem), "%s", stem.c_str());
	}

	void MarkDirty()
	{
		TrailToolsDetail::gDraft.trailDirty = true;
	}

	bool TryAbsUnderPack(const std::wstring& absPath, std::string& outRel)
	{
		using namespace TrailToolsDetail;
		std::wstring pack = PackDir();
		if (pack.empty() || absPath.size() < pack.size() + 2)
			return false;
		/* Case-insensitive prefix on Windows. */
		std::wstring abs = absPath;
		std::wstring root = pack;
		for (auto& ch : abs)
			if (ch == L'/')
				ch = L'\\';
		for (auto& ch : root)
			if (ch == L'/')
				ch = L'\\';
		for (size_t i = 0; i < root.size(); ++i)
		{
			const wchar_t a = abs[i] >= L'A' && abs[i] <= L'Z' ? abs[i] + 32 : abs[i];
			const wchar_t b = root[i] >= L'A' && root[i] <= L'Z' ? root[i] + 32 : root[i];
			if (a != b)
				return false;
		}
		if (abs[root.size()] != L'\\')
			return false;
		std::string rel = WideToUtf8(abs.substr(root.size() + 1));
		for (char& c : rel)
			if (c == '\\')
				c = '/';
		outRel = std::move(rel);
		return true;
	}

	void RegisterActiveInPack()
	{
		TrailToolsDetail::UpsertActiveTrailInPack();
	}

	bool SaveActiveToPath(const std::wstring& path)
	{
		using namespace TrailToolsDetail;
		SyncActiveType();
		if (gDraft.active.mapId == 0 || gDraft.active.points.size() < 2)
		{
			SetStatus("Need map + at least 2 points to save.");
			return false;
		}
		/* Ensure parent folder exists. */
		{
			const size_t slash = path.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				const std::wstring dir = path.substr(0, slash);
				CreateDirectoryW(dir.c_str(), nullptr);
			}
		}
		if (!TrailToolsTrl::Write(path, gDraft.active.mapId, gDraft.active.points))
		{
			SetStatus("Save failed.");
			return false;
		}
		RememberDirFromPath(path);
		std::string under;
		if (TryAbsUnderPack(path, under))
		{
			gDraft.active.fileRel = under;
			ApplyStemFromFileRel();
		}
		else
			SyncActiveFileRelFromStem();
		RegisterActiveInPack();
		SetStatus("Saved %s (%zu pts).", gDraft.active.fileRel.c_str(), gDraft.active.points.size());
		return true;
	}

	bool DialogPickTrl(bool saveAs, std::wstring& outPath)
	{
		using namespace TrailToolsDetail;
		EnsureWorkspace();
		CreateDirectoryW(TrailsFolder().c_str(), nullptr);

		wchar_t fileBuf[MAX_PATH]{};
		if (!saveAs)
			fileBuf[0] = L'\0';
		else
		{
			const char* stem = gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail";
			const std::wstring stemW = Utf8ToWide(stem);
			std::swprintf(fileBuf, MAX_PATH, L"%ls.trl", stemW.c_str());
		}

		const std::wstring start = DialogStartDir();
		wchar_t dirBuf[MAX_PATH]{};
		std::wcsncpy(dirBuf, start.c_str(), MAX_PATH - 1);

		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFilter = L"Trail files (*.trl)\0*.trl\0All files (*.*)\0*.*\0";
		ofn.lpstrFile = fileBuf;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = dirBuf;
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
			(saveAs ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		ofn.lpstrDefExt = L"trl";
		ofn.lpstrTitle = saveAs ? L"Save trail as" : L"Load trail";

		const BOOL ok = saveAs ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
		if (!ok)
			return false;
		outPath.assign(fileBuf);
		return true;
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
					ApplyStemFromFileRel();
					gDraft.selectedPoint = -1;
					gDraft.trailDirty = false;
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
	}

	void DrawDefaultCategory()
	{
		using namespace TrailToolsDetail;
		ImGui::TextUnformatted("Default category");
		{
			std::vector<std::string> leaves;
			CollectLeafPaths(gDraft.root, "", leaves, true);
			if (leaves.empty() && gDraft.trailType[0])
				leaves.push_back(gDraft.trailType);
			int cur = 0;
			for (size_t i = 0; i < leaves.size(); ++i)
			{
				if (leaves[i] == gDraft.trailType)
				{
					cur = static_cast<int>(i);
					break;
				}
			}
			const char* preview = leaves.empty() ? (gDraft.trailType[0] ? gDraft.trailType : "(none)")
				: leaves[static_cast<size_t>(cur)].c_str();
			if (ImGui::BeginCombo("###gw2igh_tt_trltype", preview))
			{
				for (size_t i = 0; i < leaves.size(); ++i)
				{
					const bool sel = static_cast<int>(i) == cur;
					if (ImGui::Selectable(leaves[i].c_str(), sel))
					{
						std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "%s",
							leaves[i].c_str());
						gDraft.active.type = gDraft.trailType;
						MarkDirty();
					}
					if (sel)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		PadNav::PushWidthForLabel("Or type path###gw2igh_tt_trltype_edit");
		ImGui::InputText("Or type path###gw2igh_tt_trltype_edit", gDraft.trailType,
			sizeof(gDraft.trailType));
		PadNav::PopWidthForLabel();
		if (ImGui::IsItemDeactivatedAfterEdit() && gDraft.trailType[0])
		{
			gDraft.active.type = gDraft.trailType;
			MarkDirty();
		}
	}

}

void TrailToolsDetail::DrawTrailDesk()
{
	EnsureWorkspace();
	SyncActiveType();
	if (gDraft.active.fileRel.empty())
		SyncActiveFileRelFromStem();

	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"XML project desk. Open Trails1 for .trl recording/editing. Insert adds the active "
		"trail into the OverlayData list (then Save XML).");
	PadNav::PopWrap();

	DrawXmlProjectDesk();
	ImGui::Separator();
	DrawDefaultCategory();
	ImGui::Separator();
	DrawTrailList();

	if (ImGui::Button("Open Trails1 window###gw2igh_tt_open_tr1"))
		TrailToolsPad::OpenTrailsWindow();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Insert into XML"));
	if (ImGui::Button("Insert into XML###gw2igh_tt_ins_trxml"))
		UpsertActiveTrailInPack();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Copy XML line"));
	{
		const std::string line = TrailToolsXml::EmitTrailElement(gDraft.active);
		if (!line.empty() && ImGui::Button("Copy XML line###gw2igh_tt_copytrxml2"))
		{
			CopyClipboard(line.c_str());
			SetStatus("Copied Trail XML line.");
		}
	}

	ImGui::TextDisabled("Active: %s%s  |  %zu pts  map %u",
		gDraft.active.fileRel.empty() ? "(none)" : gDraft.active.fileRel.c_str(),
		gDraft.trailDirty ? " *" : "",
		gDraft.active.points.size(), gDraft.active.mapId);

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}

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


void TrailToolsDetail::DrawTrailTab()
{
	DrawTrailDesk();
	ImGui::Separator();
	DrawTrailRawEditor();
}
