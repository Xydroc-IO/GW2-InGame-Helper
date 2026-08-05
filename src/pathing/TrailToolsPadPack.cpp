#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsBuild.h"
#include "TrailToolsXml.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingTrails.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
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
		ImGui::SetNextItemWidth(90.f);
		ImGui::DragFloat("fadeNear", &n.fadeNear, 10.f, -1.f, 20000.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		ImGui::DragFloat("fadeFar", &n.fadeFar, 10.f, -1.f, 20000.f);

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

	ImGui::Separator();
	ImGui::TextUnformatted("Categories");
	if (ImGui::BeginChild("###gw2igh_tt_cats", ImVec2(0.f, 220.f), true))
		DrawCategoryNode(gDraft.root, 0);
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::Text("%zu trails · %zu markers", gDraft.trails.size() +
		(gDraft.active.points.size() >= 2 ? 1u : 0u), gDraft.pois.size());

	if (ImGui::CollapsingHeader("XML preview###gw2igh_tt_xmlprev"))
	{
		static std::string sXml;
		sXml = TrailToolsXml::EmitOverlayData(gDraft);
		ImGui::BeginChild("###gw2igh_tt_xmlscroll", ImVec2(0.f, 160.f), true);
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
			/* Categories default OFF — enable this pack so markers/trails draw. */
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
