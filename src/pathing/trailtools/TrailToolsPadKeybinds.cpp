#include "TrailToolsInternal.h"
#include "TrailToolsBinds.h"
#include "TrailToolsShared.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	void BindButton(const char* id, TrailToolsBinds::Chord& chord, int captureId)
	{
		auto& st = TrailToolsBinds::Get();
		const bool listening = st.captureTarget == captureId;
		char lab[96]{};
		if (listening)
			std::snprintf(lab, sizeof(lab), "Press key…###%s", id);
		else
			std::snprintf(lab, sizeof(lab), "%s###%s",
				TrailToolsBinds::FormatChord(chord).c_str(), id);
		if (ImGui::Button(lab, ImVec2(168.f, 0.f)))
			st.captureTarget = listening ? -1 : captureId;
		ImGui::SameLine();
		if (ImGui::SmallButton((std::string("Clear###clr_") + id).c_str()))
		{
			chord = {};
			st.captureTarget = -1;
			Settings::SetDirty();
		}
	}

	void DrawTypeCombo(char* typeBuf, size_t typeLen, const char* id)
	{
		using namespace TrailToolsDetail;
		std::vector<std::string> leaves;
		CollectLeafPaths(gDraft.root, "", leaves, false);
		if (leaves.empty() && gDraft.markerType[0])
			leaves.push_back(gDraft.markerType);
		int cur = -1;
		for (size_t i = 0; i < leaves.size(); ++i)
		{
			if (leaves[i] == typeBuf)
			{
				cur = static_cast<int>(i);
				break;
			}
		}
		const char* preview = typeBuf[0] ? typeBuf
			: (gDraft.markerType[0] ? "(default marker type)" : "(unset)");
		char comboId[64]{};
		std::snprintf(comboId, sizeof(comboId), "###%s_type", id);
		ImGui::SetNextItemWidth(220.f);
		if (ImGui::BeginCombo(comboId, preview))
		{
			if (ImGui::Selectable("(use default marker type)", typeBuf[0] == 0))
			{
				typeBuf[0] = 0;
				Settings::SetDirty();
			}
			for (size_t i = 0; i < leaves.size(); ++i)
			{
				const bool sel = static_cast<int>(i) == cur;
				if (ImGui::Selectable(leaves[i].c_str(), sel))
				{
					std::snprintf(typeBuf, typeLen, "%s", leaves[i].c_str());
					Settings::SetDirty();
				}
				if (sel)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
}

void TrailToolsDetail::DrawKeybindsTab()
{
	using namespace TrailToolsBinds;
	auto& st = Get();

	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Binds work while GW2 is focused (even if this pad is closed). "
		"Place Marker has %d slots — assign each a category (e.g. different mounts) "
		"and a chord (CTRL+NumPad is a common pattern; TacO only offered ~4–5).",
		kPlaceSlots);
	PadNav::PopWrap();

	if (ImGui::Button("Reset defaults###gw2igh_kb_reset"))
	{
		SetDefaults();
		Settings::SetDirty();
		SetStatus("Keybinds reset to defaults.");
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Click a bind, then press the key combo. Esc cancels.");

	ImGui::Separator();
	ImGui::TextUnformatted("TRAILS");
	if (st.trailRecording)
	{
		ImGui::SameLine();
		ImGui::TextColored(st.trailPaused ? HelperTheme::Muted : HelperTheme::Ok,
			st.trailPaused ? "(paused)" : "(recording)");
	}

	ImGui::TextUnformatted("Start / resume recording");
	ImGui::SameLine(220.f);
	BindButton("kb_tstart", st.trailStart, 0);
	ImGui::TextUnformatted("Pause / unpause");
	ImGui::SameLine(220.f);
	BindButton("kb_tpause", st.trailPause, 1);
	ImGui::TextUnformatted("New trail section");
	ImGui::SameLine(220.f);
	BindButton("kb_tsec", st.trailSection, 2);
	ImGui::TextUnformatted("Delete trail segment");
	ImGui::SameLine(220.f);
	BindButton("kb_tdel", st.trailDeleteSeg, 3);

	ImGui::Separator();
	ImGui::TextUnformatted("MARKERS");
	ImGui::TextUnformatted("Delete marker");
	ImGui::SameLine(220.f);
	BindButton("kb_mdel", st.markerDelete, 4);
	ImGui::TextDisabled("Deletes the selected POI, or the last one if none selected.");

	ImGui::Separator();
	ImGui::TextUnformatted("Place marker slots");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Each slot places its own type at your feet. Empty type uses the Markers tab default.");
	PadNav::PopWrap();

	if (ImGui::BeginChild("###gw2igh_kb_places", ImVec2(0.f, 0.f), true))
	{
		for (int i = 0; i < kPlaceSlots; ++i)
		{
			ImGui::PushID(i);
			char rowLab[64]{};
			std::snprintf(rowLab, sizeof(rowLab), "Slot %d", i + 1);
			ImGui::TextUnformatted(rowLab);
			ImGui::SameLine(70.f);
			ImGui::SetNextItemWidth(100.f);
			if (ImGui::InputText("###lab", st.place[i].label, sizeof(st.place[i].label)))
				Settings::SetDirty();
			ImGui::SameLine();
			char bid[32]{};
			std::snprintf(bid, sizeof(bid), "kb_p%d", i);
			BindButton(bid, st.place[i].chord, 10 + i);
			DrawTypeCombo(st.place[i].type, sizeof(st.place[i].type), bid);
			if (ImGui::SmallButton("Place now###now"))
				ActionPlaceMarker(i);
			ImGui::Separator();
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (gDraft.status[0])
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
}
