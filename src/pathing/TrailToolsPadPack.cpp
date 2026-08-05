#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsAssets.h"
#include "TrailToolsBuild.h"
#include "TrailToolsXml.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingTrails.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	void DrawLooksSection()
	{
		using namespace TrailToolsDetail;
		ImGui::TextUnformatted("Looks (Blish-style)");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Presets write texture/icon PNGs into your pack and set color / scale / fade on the "
			"active trail and marker categories. Drop your own PNGs into Markers/ for Custom.");
		PadNav::PopWrap();

		int trailN = 0, markN = 0;
		const char* const* trailNames = TrailLookPresetNames(&trailN);
		const char* const* markNames = MarkerLookPresetNames(&markN);
		static int sTrailPreset = 0;
		static int sMarkPreset = 0;

		ImGui::SetNextItemWidth(200.f);
		if (ImGui::Combo("Trail look###gw2igh_tt_tlook", &sTrailPreset, trailNames, trailN))
			ApplyTrailLookPreset(sTrailPreset);
		PadNav::WrapSameLine(PadNav::ButtonWidth("Apply trail"));
		if (ImGui::Button("Apply trail###gw2igh_tt_tlook_go"))
			ApplyTrailLookPreset(sTrailPreset);

		ImGui::SetNextItemWidth(200.f);
		if (ImGui::Combo("Marker look###gw2igh_tt_mlook", &sMarkPreset, markNames, markN))
			ApplyMarkerLookPreset(sMarkPreset);
		PadNav::WrapSameLine(PadNav::ButtonWidth("Apply marker"));
		if (ImGui::Button("Apply marker###gw2igh_tt_mlook_go"))
			ApplyMarkerLookPreset(sMarkPreset);

		/* Fine-tune active leaf categories. */
		CategoryNode* trailLeaf = FindCategoryByPath(gDraft.root,
			gDraft.trailType[0] ? std::string(gDraft.trailType) : RootCategoryName() + ".t.extrail");
		CategoryNode* markLeaf = FindCategoryByPath(gDraft.root,
			gDraft.markerType[0] ? std::string(gDraft.markerType) : RootCategoryName() + ".m.exm");

		if (trailLeaf)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Trail category: %s", gDraft.trailType);
			char tex[256]{};
			std::snprintf(tex, sizeof(tex), "%s", trailLeaf->texture.c_str());
			if (ImGui::InputText("Trail texture###gw2igh_tt_ttex", tex, sizeof(tex)))
				trailLeaf->texture = tex;
			ImGui::SetNextItemWidth(120.f);
			ImGui::DragFloat("trailScale###gw2igh_tt_tscale", &trailLeaf->trailScale, 0.05f, 0.25f, 4.f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.f);
			ImGui::DragFloat("fadeNear###gw2igh_tt_tfn", &trailLeaf->fadeNear, 10.f, -1.f, 20000.f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.f);
			ImGui::DragFloat("fadeFar###gw2igh_tt_tff", &trailLeaf->fadeFar, 10.f, -1.f, 20000.f);
			float rgba[4] = {
				((trailLeaf->color >> 16) & 0xFFu) / 255.f,
				((trailLeaf->color >> 8) & 0xFFu) / 255.f,
				(trailLeaf->color & 0xFFu) / 255.f,
				((trailLeaf->color >> 24) & 0xFFu) / 255.f,
			};
			if (trailLeaf->color == 0)
			{
				rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.f;
			}
			if (ImGui::ColorEdit4("Trail tint###gw2igh_tt_tcol", rgba,
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex))
			{
				const auto ch = [](float f) -> uint32_t {
					return static_cast<uint32_t>(std::clamp(f, 0.f, 1.f) * 255.f + 0.5f);
				};
				trailLeaf->color = (ch(rgba[3]) << 24) | (ch(rgba[0]) << 16) |
					(ch(rgba[1]) << 8) | ch(rgba[2]);
			}
		}

		if (markLeaf)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Marker category: %s", gDraft.markerType);
			char icon[256]{};
			std::snprintf(icon, sizeof(icon), "%s", markLeaf->iconFile.c_str());
			if (ImGui::InputText("Marker icon###gw2igh_tt_micon", icon, sizeof(icon)))
				markLeaf->iconFile = icon;
			ImGui::SetNextItemWidth(120.f);
			ImGui::DragFloat("iconSize###gw2igh_tt_misz", &markLeaf->iconSize, 0.05f, 0.25f, 4.f);
			float rgba[4] = {
				((markLeaf->color >> 16) & 0xFFu) / 255.f,
				((markLeaf->color >> 8) & 0xFFu) / 255.f,
				(markLeaf->color & 0xFFu) / 255.f,
				((markLeaf->color >> 24) & 0xFFu) / 255.f,
			};
			if (markLeaf->color == 0)
			{
				rgba[0] = 1.f; rgba[1] = 0.8f; rgba[2] = 0.16f; rgba[3] = 1.f;
			}
			if (ImGui::ColorEdit4("Marker tint###gw2igh_tt_mcol", rgba,
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex))
			{
				const auto ch = [](float f) -> uint32_t {
					return static_cast<uint32_t>(std::clamp(f, 0.f, 1.f) * 255.f + 0.5f);
				};
				markLeaf->color = (ch(rgba[3]) << 24) | (ch(rgba[0]) << 16) |
					(ch(rgba[1]) << 8) | ch(rgba[2]);
			}
		}
	}

	void DrawCategoryNode(TrailToolsDetail::CategoryNode& n, int depth)
	{
		ImGui::PushID(&n);
		const float indent = static_cast<float>(depth) * 12.f;
		ImGui::Indent(indent);
		char nameBuf[64]{};
		char dispBuf[96]{};
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", n.name.c_str());
		std::snprintf(dispBuf, sizeof(dispBuf), "%s", n.displayName.c_str());
		ImGui::SetNextItemWidth(100.f);
		if (ImGui::InputText("name", nameBuf, sizeof(nameBuf)))
			n.name = nameBuf;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::InputText("label", dispBuf, sizeof(dispBuf)))
			n.displayName = dispBuf;

		char icon[256]{};
		char tex[256]{};
		std::snprintf(icon, sizeof(icon), "%s", n.iconFile.c_str());
		std::snprintf(tex, sizeof(tex), "%s", n.texture.c_str());
		if (ImGui::InputText("iconFile", icon, sizeof(icon)))
			n.iconFile = icon;
		if (ImGui::InputText("texture", tex, sizeof(tex)))
			n.texture = tex;
		ImGui::SetNextItemWidth(80.f);
		ImGui::DragFloat("fadeNear", &n.fadeNear, 10.f, -1.f, 20000.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.f);
		ImGui::DragFloat("fadeFar", &n.fadeFar, 10.f, -1.f, 20000.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		ImGui::DragFloat("scale", &n.trailScale, 0.05f, 0.25f, 4.f);
		ImGui::SetNextItemWidth(70.f);
		ImGui::DragFloat("iconSize", &n.iconSize, 0.05f, 0.25f, 4.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		ImGui::DragFloat("alpha", &n.alpha, 0.05f, 0.f, 1.f);
		char sched[96]{};
		std::snprintf(sched, sizeof(sched), "%s", n.schedule.c_str());
		if (ImGui::InputText("schedule", sched, sizeof(sched)))
			n.schedule = sched;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		ImGui::DragFloat("schedDur", &n.scheduleDuration, 1.f, 0.f, 10080.f);

		if (ImGui::SmallButton("Add child"))
		{
			TrailToolsDetail::CategoryNode ch;
			ch.name = "new";
			ch.displayName = "New Category";
			n.children.push_back(ch);
		}
		ImGui::Unindent(indent);

		for (size_t i = 0; i < n.children.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			DrawCategoryNode(n.children[i], depth + 1);
			if (ImGui::SmallButton("Remove child"))
			{
				n.children.erase(n.children.begin() + static_cast<std::ptrdiff_t>(i));
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}
		ImGui::PopID();
	}
}

void TrailToolsDetail::DrawPackTab()
{
	ImGui::TextUnformatted("Pack");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Workspace under pathing/authoring/<PackName>/. Build writes <PackName>.taco into pathing/.");
	PadNav::PopWrap();

	if (ImGui::InputText("Pack name###gw2igh_tt_pname", gDraft.packName, sizeof(gDraft.packName)))
	{
		SanitizePackName(gDraft.packName, sizeof(gDraft.packName));
		gDraft.root.name = RootCategoryName();
		gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" +
			(gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail") + ".trl";
	}
	ImGui::InputText("Display name###gw2igh_tt_pdname", gDraft.displayName, sizeof(gDraft.displayName));
	if (ImGui::Button("Reseed Example categories###gw2igh_tt_reseed"))
	{
		SeedDefaultCategories();
		SetStatus("Reseeded Example Pack categories.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Open folder"));
	if (ImGui::Button("Open folder###gw2igh_tt_folder"))
	{
		if (OpenAuthoringFolder())
			SetStatus("Opened authoring folder.");
		else
			SetStatus("Could not open folder.");
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Save draft"));
	if (ImGui::Button("Save draft###gw2igh_tt_savedraft"))
		SaveDraftSession();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Load draft"));
	if (ImGui::Button("Load draft###gw2igh_tt_loaddraft"))
		LoadDraftSession();

	static char sImportName[96] = "Hero.Blish.Pack.taco";
	ImGui::InputText("Import .taco name###gw2igh_tt_impname", sImportName, sizeof(sImportName));
	if (ImGui::Button("Import installed .taco###gw2igh_tt_import"))
	{
		std::wstring path = AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing");
		path += L"\\";
		for (const char* c = sImportName; *c; ++c)
			path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		std::string err;
		if (!ImportTacoToDraft(path, err))
			SetStatus("%s", err.c_str());
	}

	ImGui::Separator();
	DrawLooksSection();

	ImGui::Separator();
	TrailToolsAssets::DrawBrowserUi();

	ImGui::Separator();
	DrawLuaFilesUi();

	ImGui::Separator();
	ImGui::TextUnformatted("Categories");
	if (ImGui::BeginChild("###gw2igh_tt_cats", ImVec2(0.f, 180.f), true))
		DrawCategoryNode(gDraft.root, 0);
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::Text("%zu trails · %zu markers", gDraft.trails.size() +
		(gDraft.active.points.size() >= 2 ? 1u : 0u), gDraft.pois.size());

	if (ImGui::CollapsingHeader("XML preview###gw2igh_tt_xmlprev"))
	{
		static std::string sXml;
		sXml = TrailToolsXml::EmitOverlayData(gDraft);
		ImGui::BeginChild("###gw2igh_tt_xmlscroll", ImVec2(0.f, 140.f), true);
		ImGui::TextUnformatted(sXml.c_str());
		ImGui::EndChild();
		if (ImGui::Button("Copy XML###gw2igh_tt_copyxml"))
		{
			CopyClipboard(sXml.c_str());
			SetStatus("Copied XML.");
		}
	}

	if (ImGui::Button("Build .taco###gw2igh_tt_build"))
	{
		std::string err;
		if (!TrailToolsBuild::BuildTaco(err))
			SetStatus("%s", err.c_str());
		else
		{
			const std::string root = RootCategoryName();
			PathingTrails::ReloadPacks();
			PathingTrails::SetCategoryEnabled(root, true);
			PathingTrails::SerializeEnabledPaths(G::PathingEnabled, sizeof(G::PathingEnabled));
			Settings::SaveNow();
			SetStatus("Built %s.taco, enabled \"%s\", reloaded Pathing.",
				gDraft.packName, root.c_str());
		}
	}
	PadNav::WrapSameLine(PadNav::ButtonWidth("Reload Pathing"));
	if (ImGui::Button("Reload Pathing###gw2igh_tt_reload"))
	{
		PathingTrails::ReloadPacks();
		SetStatus("Pathing packs reloaded.");
	}

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
