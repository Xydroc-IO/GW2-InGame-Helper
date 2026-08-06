#include "UnlocksPad.h"

#include "Globals.h"
#include "Gw2Icons.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "UnlocksData.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <vector>

void UnlocksPad::RenderContents()
{
	UnlocksData::Tick();

	ImGui::TextColored(HelperTheme::Gold, "UNLOCKS");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Wardrobe and account unlocks from the official API (skins, dyes, minis, ...). "
		"Requires an API key with the unlocks scope.");
	PadNav::PopWrap();
	ImGui::Spacing();

	if (!G::Gw2ApiKey[0])
	{
		ImGui::TextColored(HelperTheme::Warn, "No API key - add one in Settings (helper side rail).");
		return;
	}

	if (Gw2Ui::IconLabelButton("Refresh unlocks###gw2igh_unlocks_ref", Gw2Ui::Icon::Check, 16.f))
		UnlocksData::EnsureAll(true);
	if (UnlocksData::BusyAny())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Loading...");
	}

	static int sKind = 0;
	static char sFilter[96] = {};
	static const char* kKindTabs[] = {
		"Skins", "Dyes", "Minis", "Finishers", "Outfits",
		"Gliders", "Mail carriers", "Novelties", "Titles"
	};
	const int prev = sKind;
	sKind = PadNav::DrawSideRail("###gw2igh_unlock_kinds", kKindTabs,
		static_cast<int>(UnlocksData::Kind::Count), sKind);
	const auto kind = static_cast<UnlocksData::Kind>(sKind);
	if (sKind != prev || !UnlocksData::Ready(kind))
		UnlocksData::EnsureLoaded(kind, false);

	ImGui::BeginChild("###gw2igh_unlock_body", ImVec2(0.f, 0.f), true);
	ImGui::TextDisabled("%s - %zu unlocked | %s",
		UnlocksData::KindLabel(kind),
		UnlocksData::Count(kind),
		UnlocksData::Status(kind));

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_unlock_filter", "Search name or id...",
		sFilter, sizeof(sFilter));

	std::vector<UnlocksData::Row> rows;
	UnlocksData::Search(kind, sFilter, rows, 400);
	ImGui::BeginChild("###gw2igh_unlock_list", ImVec2(0.f, 0.f), true);
	if (!UnlocksData::Ready(kind) && UnlocksData::Busy(kind))
		ImGui::TextDisabled("Loading %s...", UnlocksData::KindLabel(kind));
	else if (rows.empty())
		ImGui::TextDisabled(sFilter[0] ? "No matches." : "No unlocks loaded yet.");
	else
	{
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(rows.size()));
		while (clipper.Step())
		{
			for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n)
			{
				const UnlocksData::Row& r = rows[static_cast<size_t>(n)];
				if (Gw2Icons::Image(r.id, 22.f))
					ImGui::SameLine(0.f, 6.f);
				ImGui::Text("%s", r.name.c_str());
				ImGui::SameLine();
				ImGui::TextDisabled("#%d", r.id);
			}
		}
	}
	ImGui::EndChild();
	ImGui::EndChild();
}
