#include "FarmingPad.h"
#include "FarmingInternal.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "PadLayout.h"
#include "Settings.h"
#include "UiScale.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace
{
	/* Selectable that wraps instead of clipping long run names. */
	bool SelectableWrapped(const char* label, bool selected)
	{
		const ImGuiStyle& st = ImGui::GetStyle();
		const float wrapW = ImGui::GetContentRegionAvail().x - st.FramePadding.x;
		const float wrapAt = wrapW > 48.f ? wrapW : 48.f;
		const ImVec2 textSz = ImGui::CalcTextSize(label, nullptr, false, wrapAt);
		const float h = textSz.y + st.FramePadding.y * 2.f;
		const bool clicked = ImGui::Selectable("##run", selected, 0, ImVec2(-1.f, h));
		const ImVec2 min = ImGui::GetItemRectMin();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImU32 col = ImGui::GetColorU32(selected ? ImGuiCol_Text : ImGuiCol_Text);
		dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
			ImVec2(min.x + st.FramePadding.x, min.y + st.FramePadding.y),
			col, label, nullptr, wrapAt);
		return clicked;
	}

	void DrawRunsTab()
	{
		using namespace FarmingDetail;
		const char* labels[64];
		const int n = static_cast<int>(gRuns.size() < 64 ? gRuns.size() : 64);
		for (int i = 0; i < n; ++i)
			labels[i] = gRuns[static_cast<size_t>(i)].name;

		/* Size to the longest name (font-scaled). Leave room for the steps pane;
		   if still tight, names wrap instead of clipping. */
		const float fontMul = (G::FontScale > 0.1f) ? G::FontScale : 1.f;
		float listW = UiScale::FitSideRailWidth(labels, n, 160.f * fontMul, 360.f * fontMul);
		listW += PadNav::kScrollGutterPad + 8.f;
		const float avail = ImGui::GetContentRegionAvail().x;
		const float leaveSteps = 200.f * fontMul;
		if (avail > leaveSteps + 120.f && listW > avail - leaveSteps)
			listW = avail - leaveSteps;
		if (listW < 140.f)
			listW = 140.f;

		ImGui::BeginChild("##farm_runs", ImVec2(listW, 0.f), true);
		for (size_t i = 0; i < gRuns.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (SelectableWrapped(gRuns[i].name, gSelectedRun == static_cast<int>(i)))
				gSelectedRun = static_cast<int>(i);
			if (ImGui::IsItemHovered() && gRuns[i].name[0])
				ImGui::SetTooltip("%s", gRuns[i].name);
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine(0.f, 8.f);
		ImGui::BeginChild("##farm_steps", ImVec2(0.f, 0.f), true);
		if (gSelectedRun >= 0 && static_cast<size_t>(gSelectedRun) < gRuns.size())
		{
			Run& r = gRuns[static_cast<size_t>(gSelectedRun)];
			PadNav::PushWrap();
			ImGui::TextWrapped("%s", r.name);
			PadNav::PopWrap();
			if (ImGui::Button("Start -> Pathing###gw2igh_farm_go"))
				StartRunPathing(static_cast<size_t>(gSelectedRun));
			PadNav::WrapSameLine(PadNav::ButtonWidth("Reset"));
			if (ImGui::Button("Reset###gw2igh_farm_rs"))
				ResetRun(static_cast<size_t>(gSelectedRun));
			for (size_t si = 0; si < r.steps.size(); ++si)
			{
				ImGui::PushID(static_cast<int>(si));
				bool d = r.steps[si].done;
				PadNav::PushWrap();
				if (ImGui::Checkbox(r.steps[si].text, &d))
					ToggleStep(static_cast<size_t>(gSelectedRun), si);
				PadNav::PopWrap();
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}

	void DrawFishingTab()
	{
		using namespace FarmingDetail;
		ImGui::InputTextWithHint("##fn", "Fish name...", gFishName, sizeof(gFishName));
		ImGui::InputTextWithHint("##fm", "Map (optional)...", gFishMap, sizeof(gFishMap));
		if (ImGui::Button("Log catch###gw2igh_fish_add") && gFishName[0])
		{
			AddFish(gFishName, gFishMap);
			gFishName[0] = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear log###gw2igh_fish_clr"))
			ClearFish();
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
		ImGui::EndChild();
	}
}

bool FarmingPad::Render()
{
	using namespace FarmingDetail;
	if (!G::ShowFarming) return false;
	EnsureSeed();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(260.f);
	PadDock::SetSizeConstraints("Farming##GW2InGameHelperFarming", 440.f, 280.f, PadDock::MaxW(720.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.46f) : 180.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.18f) : 120.f;
		PadDock::Place(G::PadFarming, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadFarming.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus) { ImGui::SetNextWindowFocus(); gFocus = false; }

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
	PadNav::Blurb("Run checklists | fish log | Pathing handoff.");
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
