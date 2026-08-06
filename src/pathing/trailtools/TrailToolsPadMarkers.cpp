#include "TrailToolsInternal.h"
#include "TrailToolsPad.h"
#include "TrailToolsShared.h"
#include "TrailToolsXml.h"
#include "TrailToolsBinds.h"

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
	void DrawSelectedPoiEditor(TrailToolsDetail::DraftPoi& p)
	{
		using namespace TrailToolsDetail;
		ImGui::Separator();
		ImGui::TextUnformatted("Edit selected");
		char type[160]{};
		char guid[96]{};
		std::snprintf(type, sizeof(type), "%s", p.type.c_str());
		std::snprintf(guid, sizeof(guid), "%s", p.guid.c_str());
		PadNav::PushWidthForLabel("type###gw2igh_tt_ptype");
		if (ImGui::InputText("type###gw2igh_tt_ptype", type, sizeof(type)))
			p.type = type;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("GUID###gw2igh_tt_pguid");
		if (ImGui::InputText("GUID###gw2igh_tt_pguid", guid, sizeof(guid)))
			p.guid = guid;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("XYZ###gw2igh_tt_mnudge");
		ImGui::DragFloat3("XYZ###gw2igh_tt_mnudge", &p.x, 0.05f);
		PadNav::PopWidthForLabel();
		if (ImGui::SmallButton("+X")) p.x += 0.5f;
		PadNav::WrapSameLine(PadNav::ButtonWidth("-X"));
		if (ImGui::SmallButton("-X")) p.x -= 0.5f;
		PadNav::WrapSameLine(PadNav::ButtonWidth("+Z"));
		if (ImGui::SmallButton("+Z")) p.z += 0.5f;
		PadNav::WrapSameLine(PadNav::ButtonWidth("-Z"));
		if (ImGui::SmallButton("-Z")) p.z -= 0.5f;
		PadNav::WrapSameLine(PadNav::ButtonWidth("+Y"));
		if (ImGui::SmallButton("+Y")) p.y += 0.25f;
		PadNav::WrapSameLine(PadNav::ButtonWidth("-Y"));
		if (ImGui::SmallButton("-Y")) p.y -= 0.25f;

		PadNav::PrepLabeled("behavior###gw2igh_tt_pbeh", 80.f, true);
		ImGui::InputInt("behavior###gw2igh_tt_pbeh", &p.behavior);
		PadNav::WrapSameLine(PadNav::CheckboxWidth("autoTrigger###gw2igh_tt_patr"));
		ImGui::Checkbox("autoTrigger###gw2igh_tt_patr", &p.autoTrigger);
		PadNav::PrepLabeled("triggerRange###gw2igh_tt_ptr", 100.f, true);
		ImGui::DragFloat("triggerRange###gw2igh_tt_ptr", &p.triggerRange, 0.1f, 0.f, 50.f);
		PadNav::PrepLabeled("fadeNear###gw2igh_tt_pfn", 100.f);
		ImGui::DragFloat("fadeNear###gw2igh_tt_pfn", &p.fadeNear, 10.f, -1.f, 20000.f);
		PadNav::PrepLabeled("fadeFar###gw2igh_tt_pff", 100.f);
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
		PadNav::PushWidthForLabel("tip-name###gw2igh_tt_ptn");
		if (ImGui::InputText("tip-name###gw2igh_tt_ptn", tip, sizeof(tip)))
			p.tipName = tip;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("tip-description###gw2igh_tt_ptd");
		if (ImGui::InputText("tip-description###gw2igh_tt_ptd", tipd, sizeof(tipd)))
			p.tipDescription = tipd;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("info###gw2igh_tt_pinfo");
		if (ImGui::InputText("info###gw2igh_tt_pinfo", info, sizeof(info)))
			p.info = info;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("copy###gw2igh_tt_pcopy");
		if (ImGui::InputText("copy###gw2igh_tt_pcopy", copy, sizeof(copy)))
			p.copy = copy;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("copy-message###gw2igh_tt_pcmsg");
		if (ImGui::InputText("copy-message###gw2igh_tt_pcmsg", cmsg, sizeof(cmsg)))
			p.copyMessage = cmsg;
		PadNav::PopWidthForLabel();
		PadNav::PushWidthForLabel("schedule###gw2igh_tt_psched");
		if (ImGui::InputText("schedule###gw2igh_tt_psched", sched, sizeof(sched)))
			p.schedule = sched;
		PadNav::PopWidthForLabel();
		PadNav::PrepLabeled("schedule-duration###gw2igh_tt_psd", 120.f, true);
		ImGui::DragFloat("schedule-duration###gw2igh_tt_psd", &p.scheduleDuration,
			1.f, 0.f, 10080.f);
		PadNav::PushWidthForLabel("iconFile###gw2igh_tt_picon");
		if (ImGui::InputText("iconFile###gw2igh_tt_picon", icon, sizeof(icon)))
			p.iconFile = icon;
		PadNav::PopWidthForLabel();

		DrawPoiScriptAttrs(p);

		ImGui::TextUnformatted("POI XML");
		{
			const std::string line = TrailToolsXml::EmitPoiElement(p);
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted, "%s", line.c_str());
			PadNav::PopWrap();
			if (ImGui::Button("Copy POI XML###gw2igh_tt_mcopy"))
			{
				CopyClipboard(line.c_str());
				SetStatus("Copied POI XML.");
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

void TrailToolsDetail::DrawMarkersDesk()
{
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"XML project desk for markers. Open new marker windows (Markers1, Markers2, …) for "
		"selected POIs. Place slots (Ctrl+Numpad) are under Keybinds.");
	PadNav::PopWrap();

	DrawXmlProjectDesk();
	ImGui::Separator();

	ImGui::TextUnformatted("Default marker type");
	std::vector<std::string> leaves;
	CollectLeafPaths(gDraft.root, "", leaves, false);
	if (leaves.empty())
		leaves.push_back(gDraft.markerType[0] ? gDraft.markerType : "examplepack.m.exm");
	int cur = 0;
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		if (leaves[i] == gDraft.markerType)
		{ cur = static_cast<int>(i); break; }
	}
	if (ImGui::BeginCombo("###gw2igh_tt_mtype", leaves[static_cast<size_t>(cur)].c_str()))
	{
		for (size_t i = 0; i < leaves.size(); ++i)
		{
			const bool sel = static_cast<int>(i) == cur;
			if (ImGui::Selectable(leaves[i].c_str(), sel))
				std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "%s", leaves[i].c_str());
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	PadNav::PushWidthForLabel("Or type path###gw2igh_tt_mtype_edit");
	ImGui::InputText("Or type path###gw2igh_tt_mtype_edit", gDraft.markerType, sizeof(gDraft.markerType));
	PadNav::PopWidthForLabel();

	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	const bool pose = ReadMumblePose(mapId, x, y, z);
	static bool sThisMapOnly = true;
	ImGui::Checkbox("List this map only###gw2igh_tt_mmap", &sThisMapOnly);

	if (ImGui::Button("Drop marker here###gw2igh_tt_drop"))
		TrailToolsBinds::ActionPlaceMarker(-1);
	PadNav::WrapSameLine(PadNav::ButtonWidth("Delete selected"));
	if (ImGui::Button("Delete selected###gw2igh_tt_mdel"))
		TrailToolsBinds::ActionDeleteMarker();

	ImGui::Separator();
	ImGui::Text("%zu markers in project", gDraft.pois.size());
	if (ImGui::BeginChild("###gw2igh_tt_mlist", ImVec2(0.f, 140.f), true))
	{
		for (int i = 0; i < static_cast<int>(gDraft.pois.size()); ++i)
		{
			const DraftPoi& p = gDraft.pois[static_cast<size_t>(i)];
			if (sThisMapOnly && pose && p.mapId != mapId)
				continue;
			ImGui::PushID(i);
			char label[256]{};
			std::snprintf(label, sizeof(label), "%d  map %u  %s###gw2igh_tt_mi",
				i, p.mapId, p.type.c_str());
			if (ImGui::Selectable(label, gDraft.selectedPoi == i))
				gDraft.selectedPoi = i;
			ImGui::SameLine();
			if (ImGui::SmallButton("Win"))
				OpenMarkerEditor(i, true); /* always another window; keep others */
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (ImGui::Button("Open another marker window###gw2igh_tt_open_mkn"))
		OpenNewMarkerEditor();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Open Markers desk"));
	if (ImGui::Button("Open Markers desk###gw2igh_tt_open_mkdesk"))
		TrailToolsPad::OpenMarkersDesk();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Insert into XML"));
	if (ImGui::Button("Insert into XML###gw2igh_tt_ins_mkxml"))
		UpsertSelectedPoiInPack();

	ImGui::TextUnformatted("Marker windows (can all be open at once):");
	for (int i = 0; i < kMaxMarkerEditors; ++i)
	{
		char lab[48]{};
		std::snprintf(lab, sizeof(lab), "%sMarkers%d###gw2igh_tt_mkslot%d",
			gMarkerEditors[i].open ? "*" : "", i + 1, i);
		if (i > 0)
			ImGui::SameLine();
		if (ImGui::SmallButton(lab))
		{
			if (gMarkerEditors[i].open)
				gMarkerEditors[i].focus = true;
			else if (gDraft.selectedPoi >= 0)
				OpenMarkerEditor(gDraft.selectedPoi, true);
			else
				OpenNewMarkerEditor();
		}
	}

	DrawCopyFromLoaded();

	ImGui::Separator();
	ImGui::TextUnformatted("Place slots (Keybinds)");
	{
		auto& kb = TrailToolsBinds::Get();
		for (int i = 0; i < TrailToolsBinds::kPlaceSlots && i < 9; ++i)
		{
			const auto& s = kb.place[i];
			if (!s.label[0] && !s.type[0])
				continue;
			ImGui::TextDisabled("%d  %s  →  %s  [%s]",
				i + 1,
				s.label[0] ? s.label : "(unnamed)",
				s.type[0] ? s.type : "(default type)",
				TrailToolsBinds::FormatChord(s.chord).c_str());
		}
	}

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}

void TrailToolsDetail::DrawMarkerRawEditor()
{
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Raw marker attributes for the selected project POI. Insert marks the project dirty; "
		"Save XML on the desk writes OverlayData.");
	PadNav::PopWrap();

	if (gDraft.selectedPoi < 0 || gDraft.selectedPoi >= static_cast<int>(gDraft.pois.size()))
	{
		ImGui::TextDisabled("No marker selected - pick one on the Markers desk, or Drop here.");
		if (ImGui::Button("Drop marker here###gw2igh_tt_drop_raw"))
			TrailToolsBinds::ActionPlaceMarker(-1);
		return;
	}

	DraftPoi& p = gDraft.pois[static_cast<size_t>(gDraft.selectedPoi)];
	ImGui::Text("Editing marker %d", gDraft.selectedPoi);
	DrawSelectedPoiEditor(p);
	if (ImGui::Button("Insert into XML###gw2igh_tt_mk_raw_ins"))
		UpsertSelectedPoiInPack();

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}

void TrailToolsDetail::DrawMarkerRawEditorForSlot(int slot)
{
	if (slot < 0 || slot >= kMaxMarkerEditors || !gMarkerEditors[slot].open)
	{
		ImGui::TextDisabled("Editor closed.");
		return;
	}
	MarkerEditorSlot& ed = gMarkerEditors[slot];
	if (ed.poiIndex < 0 || ed.poiIndex >= static_cast<int>(gDraft.pois.size()))
	{
		ImGui::TextDisabled("POI %d is gone - close this window or pick another on the desk.",
			ed.poiIndex);
		if (ImGui::Button("Drop marker here###gw2igh_tt_drop_slot"))
			TrailToolsBinds::ActionPlaceMarker(-1);
		return;
	}

	gDraft.selectedPoi = ed.poiIndex;
	DraftPoi& p = gDraft.pois[static_cast<size_t>(ed.poiIndex)];
	ImGui::Text("Markers%d — project POI %d", slot + 1, ed.poiIndex);
	DrawSelectedPoiEditor(p);
	char insLab[64]{};
	std::snprintf(insLab, sizeof(insLab), "Insert %s###gw2igh_tt_mk_slot_ins",
		p.tipName.empty() ? (p.type.empty() ? "marker" : p.type.c_str()) : p.tipName.c_str());
	if (ImGui::Button(insLab))
		UpsertSelectedPoiInPack();

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}

void TrailToolsDetail::DrawMarkersTab()
{
	DrawMarkersDesk();
	ImGui::Separator();
	DrawMarkerRawEditor();
}
