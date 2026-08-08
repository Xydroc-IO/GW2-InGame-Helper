#include "FarmingPad.h"
#include "FarmingInternal.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "PadLayout.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "WaypointsData.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
	void DrawRunsTab()
	{
		using namespace FarmingDetail;

		ImGui::InputTextWithHint("###gw2igh_farm_filter", "Filter runs...", gFilter, sizeof(gFilter));
		ImGui::Checkbox("Favorites only###gw2igh_farm_favonly", &gFavoritesOnly);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		const char* tagItems[] = {
			"All", "Meta", "Gather", "Currency", "Fishing", "Home", "Festival", "Custom"
		};
		int tagIdx = static_cast<int>(gFilterTag) + 1; /* All = -1 → 0 */
		if (ImGui::Combo("###gw2igh_farm_tag", &tagIdx, tagItems, IM_ARRAYSIZE(tagItems)))
			gFilterTag = static_cast<RunTag>(tagIdx - 1);

		std::vector<size_t> visible;
		visible.reserve(gRuns.size());
		for (size_t i = 0; i < gRuns.size(); ++i)
			if (RunMatchesFilter(gRuns[i])) visible.push_back(i);

		/* Keep selection on a visible run so the detail pane always matches the list. */
		bool selVisible = false;
		for (size_t i : visible)
		{
			if (static_cast<int>(i) == gSelectedRun) { selVisible = true; break; }
		}
		if (!selVisible && !visible.empty())
		{
			gSelectedRun = static_cast<int>(visible[0]);
			gFocusStep = NextUndoneStep(visible[0]);
		}

		const float fontMul = (G::FontScale > 0.1f) ? G::FontScale : 1.f;
		const float avail = ImGui::GetContentRegionAvail().x;
		const float leaveSteps = 220.f * fontMul;
		float listW = 200.f * fontMul;
		if (avail > leaveSteps + 140.f)
			listW = avail - leaveSteps;
		else if (avail > 280.f)
			listW = avail * 0.42f;
		if (listW < 140.f) listW = 140.f;
		if (listW > 280.f * fontMul) listW = 280.f * fontMul;

		ImGui::BeginChild("##farm_runs", ImVec2(listW, 0.f), true);
		if (visible.empty())
			ImGui::TextColored(HelperTheme::Muted, "No runs match filter.");
		for (size_t i : visible)
		{
			const Run& run = gRuns[i];
			int done = 0, total = 0;
			RunProgress(run, done, total);
			char label[160];
			std::snprintf(label, sizeof(label), "%s%s  (%d/%d)###farm_run_%d",
				run.favorite ? "* " : "", run.name, done, total, run.id);
			const bool sel = gSelectedRun == static_cast<int>(i);
			if (ImGui::Selectable(label, sel))
			{
				gSelectedRun = static_cast<int>(i);
				gFocusStep = NextUndoneStep(i);
			}
			if (ImGui::IsItemHovered() && run.blurb[0])
				ImGui::SetTooltip("%s\n[%s]%s", run.blurb, TagLabel(run.tag),
					run.custom ? " · custom" : "");
		}
		ImGui::EndChild();
		ImGui::SameLine(0.f, 8.f);
		ImGui::BeginChild("##farm_steps", ImVec2(0.f, 0.f), true);
		if (gSelectedRun >= 0 && static_cast<size_t>(gSelectedRun) < gRuns.size())
		{
			Run& r = gRuns[static_cast<size_t>(gSelectedRun)];
			int done = 0, total = 0;
			RunProgress(r, done, total);
			PadNav::PushWrap();
			ImGui::TextWrapped("%s", r.name);
			PadNav::PopWrap();
			ImGui::TextColored(HelperTheme::Muted, "%s · %d/%d%s",
				TagLabel(r.tag), done, total, r.custom ? " · custom" : "");
			if (r.blurb[0])
			{
				PadNav::PushWrap();
				ImGui::TextWrapped("%s", r.blurb);
				PadNav::PopWrap();
			}
			{
				char sched[128]{};
				if (RunScheduleHint(r, sched, sizeof(sched)))
				{
					ImGui::TextColored(HelperTheme::GoldMuted, "Schedule: %s", sched);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(
							"Static UTC window from the Events catalog\n"
							"(not live node spawns).");
				}
			}

			if (ImGui::Button("Start -> Pathing###gw2igh_farm_go"))
				StartRunPathing(static_cast<size_t>(gSelectedRun));
			PadNav::WrapSameLine(PadNav::ButtonWidth("Reset"));
			if (ImGui::Button("Reset###gw2igh_farm_rs"))
				ResetRun(static_cast<size_t>(gSelectedRun));
			PadNav::WrapSameLine(PadNav::ButtonWidth(r.favorite ? "Unstar" : "Star"));
			if (ImGui::Button(r.favorite ? "Unstar###gw2igh_farm_star" : "Star###gw2igh_farm_star"))
				ToggleFavorite(static_cast<size_t>(gSelectedRun));
			PadNav::WrapSameLine(PadNav::ButtonWidth("GPS next"));
			if (ImGui::Button("GPS next###gw2igh_farm_gpsn"))
				GuideNextStep(static_cast<size_t>(gSelectedRun));
			if (ImGui::Button("Refresh nodes###gw2igh_farm_nodes"))
				RefreshLiveNodes(static_cast<size_t>(gSelectedRun));
			ImGui::SameLine();
			if (ImGui::Button("GPS nearest###gw2igh_farm_gps"))
				GuideNearestLiveNode();

			ImGui::Checkbox("Auto-check on arrive###gw2igh_farm_aa", &gAutoArrive);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(
					"When GPS guides you to a Pathing pack marker (or landmark),\n"
					"check the next run step when you get close. No custom trails.");
			if (ImGui::IsItemEdited())
				Save();
			if (gAutoArrive)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(100.f);
				if (ImGui::SliderFloat("###gw2igh_farm_rad", &gArriveRadius, 40.f, 400.f, "%.0f"))
					Save();
			}

			for (size_t si = 0; si < r.steps.size(); ++si)
			{
				ImGui::PushID(static_cast<int>(si));
				bool d = r.steps[si].done;
				const bool focused = (gFocusStep == static_cast<int>(si));
				if (focused)
					ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Gold);
				PadNav::PushWrap();
				if (ImGui::Checkbox(r.steps[si].text, &d))
				{
					ToggleStep(static_cast<size_t>(gSelectedRun), si);
					gFocusStep = static_cast<int>(si);
				}
				PadNav::PopWrap();
				if (focused)
					ImGui::PopStyleColor();
				if (r.steps[si].hasCoord)
				{
					ImGui::SameLine();
					if (ImGui::SmallButton("GPS"))
						GuideStep(static_cast<size_t>(gSelectedRun), si);
				}
				ImGui::PopID();
			}

			ImGui::Separator();
			PadNav::SectionTitle("Custom runs");
			ImGui::InputTextWithHint("###gw2igh_farm_newrun", "New run name...",
				gNewRunName, sizeof(gNewRunName));
			if (ImGui::Button("Add run###gw2igh_farm_addrun") && gNewRunName[0])
			{
				const int mapId = WaypointsData::CurrentMapId();
				if (AddCustomRun(gNewRunName, RunTag::Custom, mapId > 0 ? mapId : 0))
					gNewRunName[0] = 0;
			}
			if (r.custom)
			{
				ImGui::InputTextWithHint("###gw2igh_farm_newstep", "New step...",
					gNewStepText, sizeof(gNewStepText));
				if (ImGui::Button("Add step###gw2igh_farm_addstep") && gNewStepText[0])
				{
					if (AddCustomStep(static_cast<size_t>(gSelectedRun), gNewStepText))
						gNewStepText[0] = 0;
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete run###gw2igh_farm_del"))
					DeleteCustomRun(static_cast<size_t>(gSelectedRun));
			}

			const auto& nodes = LiveNodes();
			if (!nodes.empty())
			{
				ImGui::Separator();
				PadNav::SectionTitle("Live nearest nodes");
				ImGui::TextColored(HelperTheme::Muted,
					"From Pathing pack markers on your map (not a spawn API).");
				const size_t showN = nodes.size() < 12 ? nodes.size() : 12;
				for (size_t ni = 0; ni < showN; ++ni)
				{
					ImGui::PushID(static_cast<int>(1000 + ni));
					const float d = nodes[ni].distSq > 0.f ? std::sqrt(nodes[ni].distSq) : 0.f;
					ImGui::TextWrapped("%.0f  %s", d, nodes[ni].label);
					ImGui::SameLine();
					if (ImGui::SmallButton("GPS"))
						GuideLiveNode(ni);
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
	}

	void DrawFishingTab()
	{
		using namespace FarmingDetail;
		ImGui::TextColored(HelperTheme::Muted,
			"Manual catch log only — no memory fish detection.");
		ImGui::Text("Session total: %d", FishTotalCount());

		ImGui::InputTextWithHint("##fn", "Fish name...", gFishName, sizeof(gFishName));
		ImGui::InputTextWithHint("##fm", "Map (optional)...", gFishMap, sizeof(gFishMap));
		if (ImGui::Button("Use current map###gw2igh_fish_map"))
			FillFishMapFromMumble();
		ImGui::SameLine();
		if (ImGui::Button("Log catch###gw2igh_fish_add") && gFishName[0])
		{
			AddFish(gFishName, gFishMap);
			gFishName[0] = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear log###gw2igh_fish_clr"))
			ClearFish();

		PadNav::SectionTitle("Quick log");
		static const char* kQuick[] = {
			"Fish", "Rare Fish", "Junk", "Aquatic Treasure"
		};
		for (int i = 0; i < IM_ARRAYSIZE(kQuick); ++i)
		{
			if (i) ImGui::SameLine();
			ImGui::PushID(i);
			if (ImGui::SmallButton(kQuick[i]))
			{
				if (!gFishMap[0])
					FillFishMapFromMumble();
				AddFish(kQuick[i], gFishMap);
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Enable fishing Pathing###gw2igh_fish_path"))
		{
			G::ShowPathingTrails = true;
			PathingTrails::SetMasterEnabled(true);
			PathingTrails::SetCategoryEnabled("tw_guides", true);
			PathingTrails::SetCategoryEnabled("tw_guides.tw_fishing", true);
			PathingGuidesPad::Open();
			std::snprintf(gStatus, sizeof(gStatus), "Pathing fishing category enabled.");
			Settings::SetDirty();
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh fishing holes###gw2igh_fish_holes"))
		{
			/* Temporarily select a fishing-tagged run if any, else refresh with fishing hint. */
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

		ImGui::BeginChild("##fish_log", ImVec2(0.f, 0.f), true);
		if (gFishLog.empty())
			ImGui::TextColored(HelperTheme::Muted, "No catches logged yet.");
		for (const FishEntry& e : gFishLog)
		{
			if (e.map[0])
				ImGui::Text("%dx  %s  (%s)", e.count, e.name, e.map);
			else
				ImGui::Text("%dx  %s", e.count, e.name);
		}

		const auto& nodes = LiveNodes();
		if (!nodes.empty() && gSelectedRun >= 0 &&
			static_cast<size_t>(gSelectedRun) < gRuns.size() &&
			gRuns[static_cast<size_t>(gSelectedRun)].tag == RunTag::Fishing)
		{
			ImGui::Separator();
			PadNav::SectionTitle("Nearby fishing holes");
			const size_t showN = nodes.size() < 10 ? nodes.size() : 10;
			for (size_t ni = 0; ni < showN; ++ni)
			{
				ImGui::PushID(static_cast<int>(2000 + ni));
				const float d = nodes[ni].distSq > 0.f ? std::sqrt(nodes[ni].distSq) : 0.f;
				ImGui::TextWrapped("%.0f  %s", d, nodes[ni].label);
				ImGui::SameLine();
				if (ImGui::SmallButton("GPS"))
					GuideLiveNode(ni);
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}
}

bool FarmingPad::Render()
{
	using namespace FarmingDetail;
	if (!G::ShowFarming) return false;
	EnsureCatalog();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(260.f);
	PadDock::SetSizeConstraints("Farming##GW2InGameHelperFarming", 480.f, 300.f, PadDock::MaxW(780.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.46f) : 180.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.18f) : 120.f;
		PadDock::Place(G::PadFarming, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadFarming.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	bool open = G::ShowFarming;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Farming##GW2InGameHelperFarming", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Farming", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadFarming)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowFarming = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowFarming = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadFarming)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	static const char* kTabs[] = { "Runs", "Fishing" };
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Map),
		static_cast<int>(Gw2Ui::Icon::Bag),
	};
	gTab = PadNav::DrawSideRail("###gw2igh_farm_nav", kTabs, 2, gTab, 0.f, kTabIcons);

	ImGui::BeginChild("###gw2igh_farm_body", ImVec2(0.f, 0.f), true);
	PadNav::Blurb("Curated + custom farm runs | GPS pack markers | auto-check on arrive | fishing log.");
	ImGui::Separator();

	if (gTab == 0)
		DrawRunsTab();
	else
		DrawFishingTab();

	if (gStatus[0])
		ImGui::TextColored(HelperTheme::Muted, "%s", gStatus);
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
