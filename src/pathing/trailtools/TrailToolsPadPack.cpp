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
#include <cctype>
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
			PadNav::PushWidthForLabel("Trail texture###gw2igh_tt_ttex");
			if (ImGui::InputText("Trail texture###gw2igh_tt_ttex", tex, sizeof(tex)))
				trailLeaf->texture = tex;
			PadNav::PopWidthForLabel();
			PadNav::PrepLabeled("trailScale###gw2igh_tt_tscale", 120.f, true);
			ImGui::DragFloat("trailScale###gw2igh_tt_tscale", &trailLeaf->trailScale, 0.05f, 0.25f, 4.f);
			PadNav::PrepLabeled("fadeNear###gw2igh_tt_tfn", 100.f);
			ImGui::DragFloat("fadeNear###gw2igh_tt_tfn", &trailLeaf->fadeNear, 10.f, -1.f, 20000.f);
			PadNav::PrepLabeled("fadeFar###gw2igh_tt_tff", 100.f);
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
			PadNav::PushWidthForLabel("Marker icon###gw2igh_tt_micon");
			if (ImGui::InputText("Marker icon###gw2igh_tt_micon", icon, sizeof(icon)))
				markLeaf->iconFile = icon;
			PadNav::PopWidthForLabel();
			PadNav::PrepLabeled("iconSize###gw2igh_tt_misz", 120.f, true);
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
		PadNav::PrepLabeled("name", 100.f, true);
		if (ImGui::InputText("name", nameBuf, sizeof(nameBuf)))
			n.name = nameBuf;
		PadNav::PrepLabeled("label", 160.f);
		if (ImGui::InputText("label", dispBuf, sizeof(dispBuf)))
			n.displayName = dispBuf;

		char icon[256]{};
		char tex[256]{};
		std::snprintf(icon, sizeof(icon), "%s", n.iconFile.c_str());
		std::snprintf(tex, sizeof(tex), "%s", n.texture.c_str());
		PadNav::PushWidthForLabel("iconFile");
		if (ImGui::InputText("iconFile", icon, sizeof(icon)))
			n.iconFile = icon;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("texture");
		if (ImGui::InputText("texture", tex, sizeof(tex)))
			n.texture = tex;
		PadNav::PopWidthForLabel();

		PadNav::PrepLabeled("fadeNear", 80.f, true);
		ImGui::DragFloat("fadeNear", &n.fadeNear, 10.f, -1.f, 20000.f);
		PadNav::PrepLabeled("fadeFar", 80.f);
		ImGui::DragFloat("fadeFar", &n.fadeFar, 10.f, -1.f, 20000.f);
		PadNav::PrepLabeled("scale", 70.f);
		ImGui::DragFloat("scale", &n.trailScale, 0.05f, 0.25f, 4.f);
		PadNav::PrepLabeled("iconSize", 70.f, true);
		ImGui::DragFloat("iconSize", &n.iconSize, 0.05f, 0.25f, 4.f);
		PadNav::PrepLabeled("alpha", 70.f);
		ImGui::DragFloat("alpha", &n.alpha, 0.05f, 0.f, 1.f);

		char sched[96]{};
		std::snprintf(sched, sizeof(sched), "%s", n.schedule.c_str());
		PadNav::PushWidthForLabel("schedule");
		if (ImGui::InputText("schedule", sched, sizeof(sched)))
			n.schedule = sched;
		PadNav::PopWidthForLabel();
		PadNav::PrepLabeled("schedDur", 90.f);
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

	static char sPrevPack[64] = {};
	if (!sPrevPack[0])
		std::snprintf(sPrevPack, sizeof(sPrevPack), "%s", gDraft.packName);
	PadNav::PushWidthForLabel("Pack name###gw2igh_tt_pname");
	ImGui::InputText("Pack name###gw2igh_tt_pname", gDraft.packName, sizeof(gDraft.packName));
	PadNav::PopWidthForLabel();
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		const std::string oldPack = sPrevPack[0] ? sPrevPack : "ExamplePack";
		std::string oldRoot = oldPack;
		for (char& ch : oldRoot)
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		SanitizePackName(gDraft.packName, sizeof(gDraft.packName));
		RemapDraftAfterPackRename(oldPack, oldRoot);
		gDraft.root.name = RootCategoryName();
		const std::string wantPrefix = std::string("Data/") + gDraft.packName + "/";
		if (gDraft.active.fileRel.empty() ||
			gDraft.active.fileRel.compare(0, wantPrefix.size(), wantPrefix) != 0)
		{
			gDraft.active.fileRel = wantPrefix + "Trails/" +
				(gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail") + ".trl";
		}
		std::snprintf(sPrevPack, sizeof(sPrevPack), "%s", gDraft.packName);
		SetStatus("Pack renamed - category paths remapped.");
	}
	else if (!ImGui::IsItemActive())
		std::snprintf(sPrevPack, sizeof(sPrevPack), "%s", gDraft.packName);
	PadNav::PushWidthForLabel("Display name###gw2igh_tt_pdname");
	ImGui::InputText("Display name###gw2igh_tt_pdname", gDraft.displayName, sizeof(gDraft.displayName));
	PadNav::PopWidthForLabel();
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

	ImGui::Separator();
	ImGui::TextUnformatted("Active trail");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"%s%s | map %u | %zu pts | category %s",
		gDraft.active.fileRel.empty() ? "(none)" : gDraft.active.fileRel.c_str(),
		gDraft.trailDirty ? " *" : "",
		gDraft.active.mapId,
		gDraft.active.points.size(),
		gDraft.trailType[0] ? gDraft.trailType : "(unset)");
	PadNav::PopWrap();
	{
		const std::string line = TrailToolsXml::EmitTrailElement(gDraft.active);
		if (!line.empty())
		{
			PadNav::PushWrap();
			ImGui::TextUnformatted(line.c_str());
			PadNav::PopWrap();
		}
		else
			ImGui::TextDisabled("<Trail type=\"...\" trailData=\"...\"/>");
	}
	if (gDraft.lastTrlDir[0])
		ImGui::TextDisabled("Last .trl folder: %s", gDraft.lastTrlDir);

	ImGui::Separator();
	ImGui::TextUnformatted("XML layout");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"MarkerCategory tree = menu. <POIs> = Trail + POI data. Combined keeps both in one "
		"file; Split writes Pack_Menu.xml + Pack_Data.xml (Pathing merges every .xml in the taco).");
	PadNav::PopWrap();
	if (ImGui::RadioButton("Combined (one OverlayData)###gw2igh_tt_xml_comb", gDraft.xmlLayout == 0))
	{
		gDraft.xmlLayout = 0;
		Settings::SetDirty();
	}
	if (ImGui::RadioButton("Split menu + data###gw2igh_tt_xml_split", gDraft.xmlLayout == 1))
	{
		gDraft.xmlLayout = 1;
		Settings::SetDirty();
	}
	if (gDraft.xmlLayout == 1)
		ImGui::TextDisabled("%s_Menu.xml  +  %s_Data.xml", gDraft.packName, gDraft.packName);
	else
		ImGui::TextDisabled("%s.xml", gDraft.packName);

	static char sImportName[96] = "Hero.Blish.Pack.taco";
	PadNav::PushWidthForLabel("Import .taco name###gw2igh_tt_impname");
	ImGui::InputText("Import .taco name###gw2igh_tt_impname", sImportName, sizeof(sImportName));
	PadNav::PopWidthForLabel();
	if (ImGui::Button("Import installed .taco###gw2igh_tt_import"))
	{
		std::wstring path = AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing");
		path += L"\\";
		for (const char* c = sImportName; *c; ++c)
			path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		std::string err;
		if (!ImportTacoToDraft(path, err))
			SetStatus("%s", err.c_str());
		else
			SetStatus("Imported %s into draft.", sImportName);
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
	ImGui::Text("%zu trails | %zu markers", gDraft.trails.size() +
		(gDraft.active.points.size() >= 2 ? 1u : 0u), gDraft.pois.size());

	if (ImGui::CollapsingHeader("XML preview###gw2igh_tt_xmlprev"))
	{
		if (gDraft.xmlLayout == 1)
		{
			static std::string sMenu, sData;
			sMenu = TrailToolsXml::EmitMenuOverlay(gDraft);
			sData = TrailToolsXml::EmitDataOverlay(gDraft);
			ImGui::TextUnformatted("Menu");
			ImGui::BeginChild("###gw2igh_tt_xmlmenu", ImVec2(0.f, 90.f), true);
			ImGui::TextUnformatted(sMenu.c_str());
			ImGui::EndChild();
			if (ImGui::Button("Copy menu XML###gw2igh_tt_copymenu"))
			{
				CopyClipboard(sMenu.c_str());
				SetStatus("Copied menu XML.");
			}
			ImGui::TextUnformatted("Data");
			ImGui::BeginChild("###gw2igh_tt_xmldata", ImVec2(0.f, 90.f), true);
			ImGui::TextUnformatted(sData.c_str());
			ImGui::EndChild();
			if (ImGui::Button("Copy data XML###gw2igh_tt_copydata"))
			{
				CopyClipboard(sData.c_str());
				SetStatus("Copied data XML.");
			}
		}
		else
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
	}

	if (ImGui::Button("Build .taco###gw2igh_tt_build"))
	{
		std::string err;
		if (!TrailToolsBuild::BuildTaco(err))
			SetStatus("%s", err.c_str());
		else
		{
			const std::string root = RootCategoryName();
			/* Enable first so gEnabledPaths is ready when the async reindex finishes. */
			PathingTrails::SetCategoryEnabled(root, true);
			PathingTrails::SerializeEnabledPaths(G::PathingEnabled, sizeof(G::PathingEnabled));
			Settings::SaveNow();
			PathingTrails::ReloadPacks();
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
