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
	float ChordButtonWidth(const char* shown)
	{
		const float pad = ImGui::GetStyle().FramePadding.x * 2.f;
		const float textW = ImGui::CalcTextSize(shown, nullptr, true).x + pad;
		const float minW = ImGui::CalcTextSize("CTRL+NUMPAD0", nullptr, true).x + pad;
		return textW > minW ? textW : minW;
	}

	void BindButton(const char* id, TrailToolsBinds::Chord& chord, int captureId)
	{
		auto& st = TrailToolsBinds::Get();
		const bool listening = st.captureTarget == captureId;
		char lab[96]{};
		if (listening)
			std::snprintf(lab, sizeof(lab), "Press key...###%s", id);
		else
			std::snprintf(lab, sizeof(lab), "%s###%s",
				TrailToolsBinds::FormatChord(chord).c_str(), id);

		char vis[96]{};
		if (const char* hash = std::strstr(lab, "###"))
			std::snprintf(vis, sizeof(vis), "%.*s", (int)(hash - lab), lab);
		else
			std::snprintf(vis, sizeof(vis), "%s", lab);

		const float btnW = ChordButtonWidth(vis);
		if (ImGui::Button(lab, ImVec2(btnW, 0.f)))
			st.captureTarget = listening ? -1 : captureId;
		PadNav::WrapSameLine(PadNav::ButtonWidth("Clear"));
		if (ImGui::SmallButton((std::string("Clear###clr_") + id).c_str()))
		{
			chord = {};
			st.captureTarget = -1;
			Settings::SetDirty();
		}
	}

	void DrawTypeCombo(char* typeBuf, size_t typeLen, const char* id, float width)
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
		ImGui::SetNextItemWidth(width);
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

	/* Label column sized to the longest row label so text is never covered by
	   the chord button (fixed SameLine(168) clipped longer names). */
	float LabelColumnWidth(const char* const* labels, int count)
	{
		float w = ImGui::CalcTextSize("Slot 10").x;
		for (int i = 0; i < count; ++i)
		{
			if (!labels[i])
				continue;
			const float tw = ImGui::CalcTextSize(labels[i], nullptr, true).x;
			if (tw > w)
				w = tw;
		}
		return w + ImGui::GetStyle().ItemInnerSpacing.x + 4.f;
	}

	void BindRow(const char* label, float labelColW, const char* id,
		TrailToolsBinds::Chord& chord, int captureId)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		const float chordW = ChordButtonWidth(
			TrailToolsBinds::FormatChord(chord).c_str());
		const float clearW = PadNav::ButtonWidth("Clear");
		const float need = chordW + clearW + ImGui::GetStyle().ItemSpacing.x * 2.f;
		/* Prefer label | bind on one row; wrap bind under label if pad is narrow. */
		PadNav::WrapSameLine(need);
		if (ImGui::GetCursorPosX() < labelColW)
			ImGui::SetCursorPosX(labelColW);
		BindButton(id, chord, captureId);
	}
}

void TrailToolsDetail::DrawKeybindsTab()
{
	using namespace TrailToolsBinds;
	auto& st = Get();

	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Binds work while GW2 is focused (pad can be closed). "
		"%d place-marker slots - each gets a category + chord.",
		kPlaceSlots);
	PadNav::PopWrap();

	if (ImGui::Button("Reset defaults###gw2igh_kb_reset"))
	{
		SetDefaults();
		Settings::SetDirty();
		SetStatus("Keybinds reset to defaults.");
	}
	PadNav::WrapSameLine(ImGui::CalcTextSize("Click a bind, then press the key combo. Esc cancels.").x);
	ImGui::TextDisabled("Click a bind, then press the key combo. Esc cancels.");

	static const char* kTrailLabels[] = {
		"Start / resume recording",
		"Pause / unpause",
		"New trail section",
		"Delete trail segment",
	};
	static const char* kMarkLabels[] = { "Delete marker" };
	const char* allLabels[] = {
		kTrailLabels[0], kTrailLabels[1], kTrailLabels[2], kTrailLabels[3], kMarkLabels[0],
	};
	const float labelCol = LabelColumnWidth(allLabels, 5);

	ImGui::Separator();
	PadNav::SectionTitle("TRAILS");
	if (st.trailRecording)
	{
		ImGui::SameLine();
		ImGui::TextColored(st.trailPaused ? HelperTheme::Muted : HelperTheme::Ok,
			st.trailPaused ? "(paused)" : "(recording)");
	}

	BindRow(kTrailLabels[0], labelCol, "kb_tstart", st.trailStart, 0);
	BindRow(kTrailLabels[1], labelCol, "kb_tpause", st.trailPause, 1);
	BindRow(kTrailLabels[2], labelCol, "kb_tsec", st.trailSection, 2);
	BindRow(kTrailLabels[3], labelCol, "kb_tdel", st.trailDeleteSeg, 3);

	ImGui::Separator();
	PadNav::SectionTitle("MARKERS");
	BindRow(kMarkLabels[0], labelCol, "kb_mdel", st.markerDelete, 4);
	PadNav::PushWrap();
	ImGui::TextDisabled("Selected POI, or last one if none selected.");
	PadNav::PopWrap();

	ImGui::Separator();
	PadNav::SectionTitle("PLACE MARKER SLOTS");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Empty type uses the Markers tab default. List scrolls with the pad.");
	PadNav::PopWrap();

	const float slotLabW = ImGui::CalcTextSize("Slot 10").x +
		ImGui::GetStyle().ItemInnerSpacing.x + 4.f;
	for (int i = 0; i < kPlaceSlots; ++i)
	{
		ImGui::PushID(i);
		char rowLab[32]{};
		std::snprintf(rowLab, sizeof(rowLab), "Slot %d", i + 1);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(rowLab);

		const float labField = ImGui::CalcTextSize("Marker 10").x +
			ImGui::GetStyle().FramePadding.x * 2.f + 8.f;
		PadNav::WrapSameLine(labField);
		if (ImGui::GetCursorPosX() < slotLabW)
			ImGui::SetCursorPosX(slotLabW);
		ImGui::SetNextItemWidth(labField);
		if (ImGui::InputText("###lab", st.place[i].label, sizeof(st.place[i].label)))
			Settings::SetDirty();

		char bid[32]{};
		std::snprintf(bid, sizeof(bid), "kb_p%d", i);
		const float chordW = ChordButtonWidth(
			FormatChord(st.place[i].chord).c_str());
		PadNav::WrapSameLine(chordW + PadNav::ButtonWidth("Clear") + 8.f);
		BindButton(bid, st.place[i].chord, 10 + i);

		const float placeW = PadNav::ButtonWidth("Place now");
		const float comboW = ImGui::CalcTextSize("(default marker type)").x +
			ImGui::GetStyle().FramePadding.x * 2.f + ImGui::GetFrameHeight();
		PadNav::WrapSameLine(comboW + placeW + 16.f);
		DrawTypeCombo(st.place[i].type, sizeof(st.place[i].type), bid, comboW);
		PadNav::WrapSameLine(placeW);
		if (ImGui::SmallButton("Place now###now"))
			ActionPlaceMarker(i);

		if (i + 1 < kPlaceSlots)
			ImGui::Separator();
		ImGui::PopID();
	}

	if (gDraft.status[0])
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Ok, "%s", gDraft.status);
		PadNav::PopWrap();
	}
}
