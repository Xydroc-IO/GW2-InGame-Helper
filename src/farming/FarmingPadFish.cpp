#include "FarmingInternal.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>

namespace FarmingDetail
{
	void DrawFishingTab()
	{
		char tot[24];
		std::snprintf(tot, sizeof(tot), "%d", FishTotalCount());
		PadLayout::Hero("###fish_hero", "Session", "Catch log (manual)", tot);
		PadNav::Meta("No memory fish detection — tap to log what you caught.");

		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("##fn", "Fish name…", gFishName, sizeof(gFishName));
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("##fm", "Map (optional)…", gFishMap, sizeof(gFishMap));
		if (PadLayout::GoldButton("Use map###gw2igh_fish_map", false, true))
			FillFishMapFromMumble();
		if (PadLayout::GoldButton("Log catch###gw2igh_fish_add", true, false) && gFishName[0])
		{
			AddFish(gFishName, gFishMap);
			gFishName[0] = 0;
		}
		if (PadLayout::GoldButton("Clear log###gw2igh_fish_clr", false, false))
			ClearFish();

		PadNav::SectionTitle("Quick log");
		static const char* kQuick[] = { "Fish", "Rare Fish", "Junk", "Treasure" };
		for (int i = 0; i < IM_ARRAYSIZE(kQuick); ++i)
		{
			if (PadNav::WrapButton(kQuick[i], false, i == 0))
			{
				if (!gFishMap[0])
					FillFishMapFromMumble();
				AddFish(kQuick[i], gFishMap);
			}
		}

		if (PadLayout::GoldButton("Fishing pathing###gw2igh_fish_path", true, true))
		{
			G::ShowPathingTrails = true;
			PathingTrails::SetMasterEnabled(true);
			PathingTrails::SetCategoryEnabled("tw_guides", true);
			PathingTrails::SetCategoryEnabled("tw_guides.tw_fishing", true);
			PathingGuidesPad::Open();
			std::snprintf(gStatus, sizeof(gStatus), "Pathing fishing category enabled.");
			Settings::SetDirty();
		}
		if (PadLayout::GoldButton("Refresh holes###gw2igh_fish_holes", false, false))
		{
			int fishRun = -1;
			for (size_t i = 0; i < gRuns.size(); ++i)
			{
				if (gRuns[i].tag == RunTag::Fishing) { fishRun = static_cast<int>(i); break; }
			}
			if (fishRun >= 0)
			{
				gSelectedRun = fishRun;
				RefreshLiveNodes(static_cast<size_t>(fishRun));
			}
			else
				std::snprintf(gStatus, sizeof(gStatus), "No fishing run in catalog.");
		}

		PadNav::SectionTitle("Log");
		ImGui::BeginChild("##fish_log", ImVec2(0.f, 0.f), true);
		if (gFishLog.empty())
			ImGui::TextColored(HelperTheme::Muted, "No catches yet.");
		for (const FishEntry& e : gFishLog)
		{
			char qty[16];
			std::snprintf(qty, sizeof(qty), "×%d", e.count);
			PadLayout::NameAndValue(e.name, qty, HelperTheme::GoldBright);
			if (e.map[0])
				ImGui::TextColored(HelperTheme::Muted, "%s", e.map);
		}

		const auto& nodes = LiveNodes();
		if (!nodes.empty() && gSelectedRun >= 0 &&
			static_cast<size_t>(gSelectedRun) < gRuns.size() &&
			gRuns[static_cast<size_t>(gSelectedRun)].tag == RunTag::Fishing)
		{
			ImGui::Separator();
			PadNav::SectionTitle("Nearby holes");
			const size_t showN = nodes.size() < 10 ? nodes.size() : 10;
			for (size_t ni = 0; ni < showN; ++ni)
			{
				ImGui::PushID(static_cast<int>(2000 + ni));
				const float d = nodes[ni].distSq > 0.f ? std::sqrt(nodes[ni].distSq) : 0.f;
				char distBuf[24];
				std::snprintf(distBuf, sizeof(distBuf), "%.0f", d);
				PadLayout::NameAndValue(nodes[ni].label, distBuf, HelperTheme::GoldMuted);
				if (PadLayout::GoldButton("GPS###fish_hole_gps", false, true))
					GuideLiveNode(ni);
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}
} // namespace FarmingDetail
