#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsXml.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	void DrawSelectedPoiEditor(TrailToolsDetail::DraftPoi& p)
	{
		using namespace TrailToolsDetail;
		ImGui::Separator();
		ImGui::TextUnformatted("Edit selected");
		char type[160]{};
		char guid[96]{};
		std::snprintf(type, sizeof(type), "%s", p.type.c_str());
		std::snprintf(guid, sizeof(guid), "%s", p.guid.c_str());
		if (ImGui::InputText("type###gw2igh_tt_ptype", type, sizeof(type)))
			p.type = type;
		if (ImGui::InputText("GUID###gw2igh_tt_pguid", guid, sizeof(guid)))
			p.guid = guid;
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

		ImGui::SetNextItemWidth(80.f);
		ImGui::InputInt("behavior###gw2igh_tt_pbeh", &p.behavior);
		ImGui::SameLine();
		ImGui::Checkbox("autoTrigger###gw2igh_tt_patr", &p.autoTrigger);
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("triggerRange###gw2igh_tt_ptr", &p.triggerRange, 0.1f, 0.f, 50.f);
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("fadeNear###gw2igh_tt_pfn", &p.fadeNear, 10.f, -1.f, 20000.f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("fadeFar###gw2igh_tt_pff", &p.fadeFar, 10.f, -1.f, 20000.f);

		char tip[96]{}, tipd[384]{}, info[384]{}, copy[256]{}, cmsg[128]{};
		char sched[96]{}, icon[256]{};
		std::snprintf(tip, sizeof(tip), "%s", p.tipName.c_str());
		std::snprintf(tipd, sizeof(tipd), "%s", p.tipDescription.c_str());
		std::snprintf(info, sizeof(info), "%s", p.info.c_str());
		std::snprintf(copy, sizeof(copy), "%s", p.copy.c_str());
		std::snprintf(cmsg, sizeof(cmsg), "%s", p.copyMessage.c_str());
		std::snprintf(sched, sizeof(sched), "%s", p.schedule.c_str());
		std::snprintf(icon, sizeof(icon), "%s", p.iconFile.c_str());
		if (ImGui::InputText("tip-name###gw2igh_tt_ptn", tip, sizeof(tip)))
			p.tipName = tip;
		if (ImGui::InputText("tip-description###gw2igh_tt_ptd", tipd, sizeof(tipd)))
			p.tipDescription = tipd;
		if (ImGui::InputText("info###gw2igh_tt_pinfo", info, sizeof(info)))
			p.info = info;
		if (ImGui::InputText("copy###gw2igh_tt_pcopy", copy, sizeof(copy)))
			p.copy = copy;
		if (ImGui::InputText("copy-message###gw2igh_tt_pcmsg", cmsg, sizeof(cmsg)))
			p.copyMessage = cmsg;
		if (ImGui::InputText("schedule###gw2igh_tt_psched", sched, sizeof(sched)))
			p.schedule = sched;
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("schedule-duration###gw2igh_tt_psd", &p.scheduleDuration,
			1.f, 0.f, 10080.f);
		if (ImGui::InputText("iconFile###gw2igh_tt_picon", icon, sizeof(icon)))
			p.iconFile = icon;

		DrawPoiScriptAttrs(p);

		if (ImGui::Button("Copy POI XML###gw2igh_tt_mcopy"))
		{
			/* Emit via XML helper for full attrs. */
			DraftPack tmp{};
			tmp.pois.push_back(p);
			std::string xml = TrailToolsXml::EmitOverlayData(tmp);
			/* Extract first POI line roughly */
			const size_t a = xml.find("<POI ");
			const size_t b = xml.find("/>", a);
			if (a != std::string::npos && b != std::string::npos)
			{
				CopyClipboard(xml.substr(a, b - a + 2).c_str());
				SetStatus("Copied full POI XML.");
			}
		}
	}

	void DrawCopyFromLoaded()
	{
		using namespace TrailToolsDetail;
		if (!ImGui::CollapsingHeader("Copy from loaded Pathing###gw2igh_tt_copyload"))
			return;
		const auto marks = PathingTrails::CurrentMarkers();
		ImGui::TextDisabled("%zu markers on current map (enabled)", marks.size());
		if (ImGui::BeginChild("###gw2igh_tt_copylist", ImVec2(0.f, 100.f), true))
		{
			for (size_t i = 0; i < marks.size() && i < 80; ++i)
			{
				const auto& m = marks[i];
				ImGui::PushID(static_cast<int>(i));
				char lab[160]{};
				std::snprintf(lab, sizeof(lab), "%s", m.label);
				if (ImGui::Selectable(lab))
				{
					DraftPoi p;
					p.mapId = m.mapId;
					p.x = m.world.x;
					p.y = m.world.y;
					p.z = m.world.z;
					p.type = m.label;
					p.guid = m.guid[0] ? m.guid : MakeGuidBase64();
					p.behavior = m.behavior;
					p.autoTrigger = m.autoTrigger;
					p.triggerRange = m.triggerRange;
					p.tipName = m.tipName;
					p.tipDescription = m.tipDescription;
					p.info = m.info;
					p.copy = m.copy;
					p.copyMessage = m.copyMessage;
					p.schedule = m.schedule;
					p.scheduleDuration = m.scheduleDuration;
					p.scriptOnce = m.scriptOnce;
					p.scriptTrigger = m.scriptTrigger;
					p.scriptFilter = m.scriptFilter;
					p.scriptTick = m.scriptTick;
					p.scriptFocus = m.scriptFocus;
					p.hide = m.hide;
					p.show = m.show;
					p.resetLength = m.resetLength;
					p.invertBehavior = m.invertBehavior;
					p.alpha = m.alpha;
					p.iconSize = m.iconSize;
					p.heightOffset = m.heightOffset;
					gDraft.pois.push_back(std::move(p));
					gDraft.selectedPoi = static_cast<int>(gDraft.pois.size()) - 1;
					SetStatus("Cloned marker into draft.");
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}
}

void TrailToolsDetail::DrawMarkersTab()
{
	ImGui::TextUnformatted("Markers");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Drop a POI at your feet. Preview uses Looks textures while this pad is open.");
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
			SetStatus("Dropped marker #%zu.", gDraft.pois.size());
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
	if (ImGui::BeginChild("###gw2igh_tt_mlist", ImVec2(0.f, 140.f), true))
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
		DrawSelectedPoiEditor(gDraft.pois[static_cast<size_t>(gDraft.selectedPoi)]);

	DrawCopyFromLoaded();

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
