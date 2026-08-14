#include "FarmingInternal.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace FarmingDetail
{
	static ImVec4 TagTextCol(RunTag t)
	{
		switch (t)
		{
		case RunTag::Gather: return HelperTheme::Ok;
		case RunTag::Currency: return HelperTheme::GoldBright;
		case RunTag::Fishing: return HelperTheme::GoldMuted;
		case RunTag::Festival: return HelperTheme::Warn;
		case RunTag::Custom: return HelperTheme::Muted;
		default: return HelperTheme::Gold;
		}
	}

	static void ThinBar(int done, int total)
	{
		if (total <= 0)
			return;
		float frac = static_cast<float>(done) / static_cast<float>(total);
		if (frac < 0.f) frac = 0.f;
		if (frac > 1.f) frac = 1.f;
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
			done >= total ? HelperTheme::Ok : HelperTheme::GoldDim);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, HelperTheme::TabIdle);
		ImGui::ProgressBar(frac, ImVec2(-1.f, 5.f), "");
		ImGui::PopStyleColor(2);
	}

	void DrawRunsTab()
	{
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("###gw2igh_farm_filter", "Filter runs…", gFilter, sizeof(gFilter));

		if (PadNav::WrapButton("★ Fav", gFavoritesOnly, true))
			gFavoritesOnly = !gFavoritesOnly;
		static const RunTag kTags[] = {
			RunTag::All, RunTag::Meta, RunTag::Gather, RunTag::Currency,
			RunTag::Fishing, RunTag::Home, RunTag::Festival, RunTag::Custom
		};
		static const char* kTagNames[] = {
			"All", "Meta", "Gather", "Gold", "Fish", "Home", "Fest", "Custom"
		};
		for (int i = 0; i < IM_ARRAYSIZE(kTags); ++i)
		{
			if (PadNav::WrapButton(kTagNames[i], gFilterTag == kTags[i], false))
				gFilterTag = kTags[i];
		}

		std::vector<size_t> visible;
		visible.reserve(gRuns.size());
		for (size_t i = 0; i < gRuns.size(); ++i)
			if (RunMatchesFilter(gRuns[i])) visible.push_back(i);

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
		const float leaveSteps = 240.f * fontMul;
		float listW = 200.f * fontMul;
		if (avail > leaveSteps + 140.f)
			listW = avail - leaveSteps;
		else if (avail > 280.f)
			listW = avail * 0.42f;
		if (listW < 140.f) listW = 140.f;
		if (listW > 280.f * fontMul) listW = 280.f * fontMul;

		ImGui::BeginChild("##farm_runs", ImVec2(listW, 0.f), true);
		if (visible.empty())
			ImGui::TextColored(HelperTheme::Muted, "No runs match.");
		for (size_t i : visible)
		{
			const Run& run = gRuns[i];
			int done = 0, total = 0;
			RunProgress(run, done, total);
			char prog[24];
			std::snprintf(prog, sizeof(prog), "%d/%d", done, total);
			ImGui::PushID(run.id);
			const ImVec4 chipFill = HelperTheme::TabIdle;
			PadLayout::Chip(TagLabel(run.tag), chipFill, TagTextCol(run.tag));
			ImGui::SameLine(0.f, 6.f);
			char label[160];
			std::snprintf(label, sizeof(label), "%s%s###farm_run_%d",
				run.favorite ? "★ " : "", run.name, run.id);
			const bool sel = gSelectedRun == static_cast<int>(i);
			const float progW = ImGui::CalcTextSize(prog).x + 8.f;
			float selW = ImGui::GetContentRegionAvail().x - progW;
			if (selW < 48.f) selW = 48.f;
			if (ImGui::Selectable(label, sel, 0, ImVec2(selW, 0.f)))
			{
				gSelectedRun = static_cast<int>(i);
				gFocusStep = NextUndoneStep(i);
			}
			if (ImGui::IsItemHovered() && run.blurb[0])
				ImGui::SetTooltip("%s", run.blurb);
			ImGui::SameLine();
			ImGui::TextColored(done >= total && total > 0 ? HelperTheme::Ok : HelperTheme::GoldMuted,
				"%s", prog);
			ThinBar(done, total);
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine(0.f, 8.f);
		ImGui::BeginChild("##farm_steps", ImVec2(0.f, 0.f), true);
		if (gSelectedRun < 0 || static_cast<size_t>(gSelectedRun) >= gRuns.size())
		{
			ImGui::EndChild();
			return;
		}

		Run& r = gRuns[static_cast<size_t>(gSelectedRun)];
		int done = 0, total = 0;
		RunProgress(r, done, total);
		char heroVal[24];
		std::snprintf(heroVal, sizeof(heroVal), "%d/%d", done, total);
		PadLayout::Hero("###farm_hero", TagLabel(r.tag), r.name, heroVal);
		if (r.blurb[0])
			PadNav::Meta(r.blurb);
		{
			char sched[128]{};
			if (RunScheduleHint(r, sched, sizeof(sched)))
			{
				ImGui::TextColored(HelperTheme::GoldMuted, "Window  %s", sched);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Static UTC window from the Events catalog.");
			}
		}
		ThinBar(done, total);

		if (PadLayout::GoldButton("Start pathing###gw2igh_farm_go", true, true))
			StartRunPathing(static_cast<size_t>(gSelectedRun));
		if (PadLayout::GoldButton("Reset###gw2igh_farm_rs", false, false))
			ResetRun(static_cast<size_t>(gSelectedRun));
		if (PadLayout::GoldButton(r.favorite ? "Unstar###gw2igh_farm_star" : "Star###gw2igh_farm_star",
			false, false))
			ToggleFavorite(static_cast<size_t>(gSelectedRun));
		if (PadLayout::GoldButton("GPS next###gw2igh_farm_gpsn", false, false))
			GuideNextStep(static_cast<size_t>(gSelectedRun));
		if (PadLayout::GoldButton("Refresh nodes###gw2igh_farm_nodes", false, false))
			RefreshLiveNodes(static_cast<size_t>(gSelectedRun));
		if (PadLayout::GoldButton("GPS nearest###gw2igh_farm_gps", false, false))
			GuideNearestLiveNode();

		ImGui::Spacing();
		ImGui::Checkbox("Auto-check on arrive###gw2igh_farm_aa", &gAutoArrive);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Check the next step when you reach a Pathing pack marker.");
		if (ImGui::IsItemEdited())
			Save();
		if (gAutoArrive)
		{
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::SliderFloat("###gw2igh_farm_rad", &gArriveRadius, 40.f, 400.f, "Arrive  %.0f m"))
				Save();
		}

		PadNav::SectionTitle("Steps");
		for (size_t si = 0; si < r.steps.size(); ++si)
		{
			ImGui::PushID(static_cast<int>(si));
			bool d = r.steps[si].done;
			const bool focused = (gFocusStep == static_cast<int>(si));
			if (d)
				ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ok);
			else if (focused)
				ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
			char stepLab[160];
			std::snprintf(stepLab, sizeof(stepLab), "%d.  %s",
				static_cast<int>(si) + 1, r.steps[si].text);
			PadNav::PushWrap();
			if (ImGui::Checkbox(stepLab, &d))
			{
				ToggleStep(static_cast<size_t>(gSelectedRun), si);
				gFocusStep = static_cast<int>(si);
			}
			PadNav::PopWrap();
			if (d || focused)
				ImGui::PopStyleColor();
			if (focused && !d)
			{
				ImGui::SameLine();
				PadLayout::Chip("now", HelperTheme::TabActive, HelperTheme::GoldBright);
			}
			if (r.steps[si].hasCoord)
			{
				ImGui::SameLine();
				if (PadLayout::GoldButton("GPS###farm_step_gps", false, true))
					GuideStep(static_cast<size_t>(gSelectedRun), si);
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		PadNav::SectionTitle("Your runs");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("###gw2igh_farm_newrun", "New run name…",
			gNewRunName, sizeof(gNewRunName));
		if (PadLayout::GoldButton("Add run###gw2igh_farm_addrun", true, true) && gNewRunName[0])
		{
			const int mapId = WaypointsData::CurrentMapId();
			if (AddCustomRun(gNewRunName, RunTag::Custom, mapId > 0 ? mapId : 0))
				gNewRunName[0] = 0;
		}
		if (r.custom)
		{
			ImGui::SetNextItemWidth(-1.f);
			ImGui::InputTextWithHint("###gw2igh_farm_newstep", "New step…",
				gNewStepText, sizeof(gNewStepText));
			if (PadLayout::GoldButton("Add step###gw2igh_farm_addstep", false, true) && gNewStepText[0])
			{
				if (AddCustomStep(static_cast<size_t>(gSelectedRun), gNewStepText))
					gNewStepText[0] = 0;
			}
			if (PadLayout::GoldButton("Delete run###gw2igh_farm_del", false, false))
				DeleteCustomRun(static_cast<size_t>(gSelectedRun));
		}

		const auto& nodes = LiveNodes();
		if (!nodes.empty())
		{
			ImGui::Separator();
			PadNav::SectionTitle("Nearby pack markers");
			PadNav::Meta("From Pathing packs on this map.");
			const size_t showN = nodes.size() < 12 ? nodes.size() : 12;
			for (size_t ni = 0; ni < showN; ++ni)
			{
				ImGui::PushID(static_cast<int>(1000 + ni));
				const float dist = nodes[ni].distSq > 0.f ? std::sqrt(nodes[ni].distSq) : 0.f;
				char distBuf[24];
				std::snprintf(distBuf, sizeof(distBuf), "%.0f", dist);
				PadLayout::NameAndValue(nodes[ni].label, distBuf, HelperTheme::GoldMuted);
				if (PadLayout::GoldButton("GPS###farm_node_gps", false, true))
					GuideLiveNode(ni);
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}
} // namespace FarmingDetail
