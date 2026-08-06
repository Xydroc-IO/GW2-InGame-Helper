#include "PanelBinds.h"

#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

void PanelBinds::DrawSettingsTab()
{
	State& st = Get();

	ImGui::TextColored(HelperTheme::Muted,
		"Click a bind, then press the new chord. Esc cancels. Clear = unbound.");
	ImGui::TextColored(HelperTheme::Muted,
		"Helper open (Ctrl+Shift+H) stays in Nexus QuickAccess.");
	ImGui::Spacing();

	if (ImGui::Button("Reset panel defaults###gw2igh_pb_reset"))
	{
		SetDefaults();
		st.captureTarget = -1;
		Settings::SetDirty();
	}
	ImGui::SameLine();
	ImGui::TextColored(HelperTheme::Muted, "Ctrl+Shift+A/G/E/N/M/R/...");

	ImGui::Spacing();
	ImGui::BeginChild("###gw2igh_pb_list", ImVec2(0.f, 0.f), true);

	for (int i = 0; i < Count; ++i)
	{
		const Slot s = static_cast<Slot>(i);
		ImGui::PushID(i);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(SlotLabel(s));
		ImGui::SameLine(140.f);

		const bool listening = st.captureTarget == i;
		char lab[96]{};
		if (listening)
			std::snprintf(lab, sizeof(lab), "Press key...###bind");
		else
			std::snprintf(lab, sizeof(lab), "%s###bind", FormatChord(st.chords[i]).c_str());
		if (ImGui::Button(lab, ImVec2(168.f, 0.f)))
			st.captureTarget = listening ? -1 : i;

		ImGui::SameLine();
		if (ImGui::SmallButton("Clear###clr"))
		{
			st.chords[i] = {};
			st.captureTarget = -1;
			Settings::SetDirty();
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
}
