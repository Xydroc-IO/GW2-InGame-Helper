#include "UnlocksPad.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "UnlocksData.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <vector>

void UnlocksPad::RenderContents()
{
	UnlocksData::Tick();

	ImGui::TextColored(HelperTheme::Gold, "UNLOCKS");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(HelperTheme::Muted,
		"Wardrobe and account unlocks from the official API (skins, dyes, minis, …). "
		"Requires an API key with the unlocks scope.");
	ImGui::PopTextWrapPos();
	ImGui::Spacing();

	if (!G::Gw2ApiKey[0])
	{
		ImGui::TextColored(HelperTheme::Warn, "No API key — add one in Nexus Options.");
		return;
	}

	if (ImGui::Button("Refresh unlocks###gw2igh_unlocks_ref"))
		UnlocksData::EnsureAll(true);
	ImGui::SameLine();
	if (UnlocksData::BusyAny())
		ImGui::TextDisabled("Loading…");

	static int sKind = 0;
	static char sFilter[96] = {};
	if (ImGui::BeginTabBar("###gw2igh_unlock_kinds", ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (int i = 0; i < static_cast<int>(UnlocksData::Kind::Count); ++i)
		{
			const auto kind = static_cast<UnlocksData::Kind>(i);
			char label[64];
			std::snprintf(label, sizeof(label), "%s###gw2igh_uk%d",
				UnlocksData::KindLabel(kind), i);
			if (ImGui::BeginTabItem(label))
			{
				sKind = i;
				UnlocksData::EnsureLoaded(kind, false);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	const auto kind = static_cast<UnlocksData::Kind>(sKind);
	ImGui::TextDisabled("%s — %zu unlocked · %s",
		UnlocksData::KindLabel(kind),
		UnlocksData::Count(kind),
		UnlocksData::Status(kind));

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_unlock_filter", "Search name or id…",
		sFilter, sizeof(sFilter));

	std::vector<UnlocksData::Row> rows;
	UnlocksData::Search(kind, sFilter, rows, 400);
	ImGui::BeginChild("###gw2igh_unlock_list", ImVec2(0.f, 0.f), true);
	if (!UnlocksData::Ready(kind) && UnlocksData::Busy(kind))
		ImGui::TextDisabled("Loading %s…", UnlocksData::KindLabel(kind));
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
				ImGui::Text("%s", r.name.c_str());
				ImGui::SameLine();
				ImGui::TextDisabled("#%d", r.id);
			}
		}
	}
	ImGui::EndChild();
}
