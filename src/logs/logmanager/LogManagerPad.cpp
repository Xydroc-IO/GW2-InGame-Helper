#include "LogManagerPad.h"

#include "LogManagerShared.h"
#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "EiRuntime.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Gw2Ui.h"
#include "PadNav.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

using namespace LogManagerDetail;

namespace
{
	int gDeferHeavy = 0;

	void KickHeavyOpen()
	{
		BeginEiEnsure(false);
		BeginScan();
	}
}

void LogManagerPad::OpenAndRefresh()
{
	G::ShowLogManager = true;
	gFocus = true;
	gPlaceOnce = true;
	gExpandGroupsOnce = true;
	gLogListFrac = G::LogManagerListFrac;
	EnsureDefaultPaths();
	Settings::SetDirty();
	gDeferHeavy = WinePadOpen::DeferFrames();
	if (gDeferHeavy <= 0)
		KickHeavyOpen();
}

bool LogManagerPad::Render()
{
	if (!G::ShowLogManager)
		return false;

	if (WinePadOpen::TickDefer(gDeferHeavy))
		KickHeavyOpen();

	EnsureDefaultPaths();
	SyncDraw();

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float displayW = display.x > 1.f ? display.x : kPadW;
	const float displayH = display.y > 1.f ? display.y : kPadH;

	if (gPlaceOnce)
	{
		float winW = G::LogManagerWinW;
		float winH = G::LogManagerWinH;
		/* First open (no saved pos): nearly full client - screenshot three-pane fit.
		   On 32:9 cap width so the pad does not span the entire desk. */
		if (G::LogManagerWinX < 0.f || G::LogManagerWinY < 0.f)
		{
			winW = displayW * 0.92f;
			winH = displayH * 0.84f;
			if (AspectLayout::Classify(displayW, displayH) == AspectLayout::Class::Super_32_9)
				winW = displayW * 0.48f;
			else if (AspectLayout::Classify(displayW, displayH) == AspectLayout::Class::Ultrawide_21_9)
				winW = displayW * 0.72f;
			if (winW < 1100.f && displayW >= 1200.f) winW = 1100.f;
			if (winH < 620.f && displayH >= 720.f) winH = 620.f;
			if (winW > 2200.f) winW = 2200.f;
			if (winH > 1200.f) winH = 1200.f;
		}
		/* Always clamp to current display. */
		{
			const float maxW = displayW > 80.f ? displayW - 24.f : winW;
			const float maxH = displayH > 100.f ? displayH - 48.f : winH;
			if (winW > maxW) winW = maxW;
			if (winH > maxH) winH = maxH;
			if (winW < 880.f && displayW > 920.f) winW = 880.f;
			if (winH < 420.f && displayH > 480.f) winH = 420.f;
			if (winW > displayW * 0.98f) winW = displayW * 0.98f;
			if (winH > displayH * 0.95f) winH = displayH * 0.95f;
		}
		G::LogManagerWinW = winW;
		G::LogManagerWinH = winH;
		if (G::LogManagerWinX >= 0.f && G::LogManagerWinY >= 0.f)
			ImGui::SetNextWindowPos(ImVec2(G::LogManagerWinX, G::LogManagerWinY), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(ImVec2(24.f, 36.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
		gLogListFrac = G::LogManagerListFrac;
		gPlaceOnce = false;
	}

	{
		float minW = 1000.f;
		float minH = 520.f;
		if (minW > displayW * 0.92f) minW = displayW * 0.92f;
		if (minH > displayH * 0.85f) minH = displayH * 0.85f;
		PadDock::SetSizeConstraints("DPS Logs###gw2igh_logmgr",
			minW, minH, displayW * 0.98f, displayH * 0.95f);
	}
	if (gFocus)
	{
		WinePadOpen::ApplyFocus(gFocus);
	}

	bool open = G::ShowLogManager;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("DPS Logs###gw2igh_logmgr", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("DPS Logs", &open) || !padBody)
	{
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowLogManager = false;
			Settings::SetDirty();
		}
		return false;
	}

	if (!open)
	{
		G::ShowLogManager = false;
		Settings::SetDirty();
		HelperTheme::EndPad();
		return false;
	}

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	const bool hasDotNet = EiRuntime::HasDotNet8Runtime();
	MaybeAutoParseAfterScan(hasDotNet);
	std::vector<const LogEntry*> filtered;
	CollectFiltered(filtered);

	DrawToolbar(filtered, hasDotNet);
	ImGui::Separator();

	const float bodyH = ImGui::GetContentRegionAvail().y;
	const float bodyW = ImGui::GetContentRegionAvail().x;
	/* Fit longest filter label (font-scaled) - never let the % cap clip checkboxes. */
	const float filterPad = ImGui::GetStyle().WindowPadding.x * 2.f + 12.f;
	float filterNeed = PadNav::CheckboxWidth("Auto-parse after scan") + filterPad;
	{
		const float g = PadNav::CheckboxWidth("Group by encounter") + filterPad;
		if (g > filterNeed) filterNeed = g;
		const float s = ImGui::CalcTextSize("Search file, encounter, or player...").x +
			ImGui::GetStyle().FramePadding.x * 2.f + filterPad;
		if (s > filterNeed) filterNeed = s;
	}
	float filterW = bodyW * kFilterFrac;
	if (filterW < kFilterMinW) filterW = kFilterMinW;
	if (filterW < filterNeed) filterW = filterNeed;
	if (filterW > kFilterMaxW) filterW = kFilterMaxW;
	if (filterW > bodyW * 0.42f)
		filterW = bodyW * 0.42f;
	if (filterW < filterNeed && filterNeed <= bodyW * 0.48f)
		filterW = filterNeed;
	if (filterW > bodyW - 80.f)
		filterW = std::max(120.f, bodyW - 80.f);

	ImGui::BeginChild("###gw2igh_lm_filters", ImVec2(filterW, bodyH), true);
	DrawFilterPane();
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	/* Log list | drag splitter | detail - fraction of remaining width; tables stretch inside. */
	const float availX = ImGui::GetContentRegionAvail().x;
	const float usable = availX - kSplitHitW - kPaneGap * 2.f;
	float listMin = kLogListMinW;
	float rightMin = kRightPaneMinW;
	if (usable > 1.f && usable < listMin + rightMin)
	{
		const float scale = usable / (listMin + rightMin);
		listMin *= scale;
		rightMin *= scale;
	}
	if (gLogListFrac < 0.20f)
		gLogListFrac = 0.20f;
	if (gLogListFrac > 0.72f)
		gLogListFrac = 0.72f;
	float centerW = usable * gLogListFrac;
	if (centerW < listMin)
		centerW = listMin;
	if (centerW > usable - rightMin)
		centerW = usable - rightMin;
	if (centerW < listMin)
		centerW = listMin;
	if (usable > 1.f)
		gLogListFrac = centerW / usable;

	ImGui::BeginChild("###gw2igh_lm_list", ImVec2(centerW, bodyH), true);
	DrawLogTable(filtered);
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	{
		const ImVec2 splitPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("###gw2igh_lm_split", ImVec2(kSplitHitW, bodyH));
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		if (hovered || active)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (active && usable > 1.f)
		{
			centerW += ImGui::GetIO().MouseDelta.x;
			if (centerW < listMin)
				centerW = listMin;
			if (centerW > usable - rightMin)
				centerW = usable - rightMin;
			gLogListFrac = centerW / usable;
			if (std::fabs(G::LogManagerListFrac - gLogListFrac) > 0.002f)
			{
				G::LogManagerListFrac = gLogListFrac;
				Settings::SetDirty();
			}
		}
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float midX = splitPos.x + kSplitHitW * 0.5f;
		const ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
			: (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
		dl->AddLine(ImVec2(midX, splitPos.y + 4.f),
			ImVec2(midX, splitPos.y + bodyH - 4.f), col, active ? 2.f : 1.f);
		if (hovered)
			ImGui::SetTooltip("Drag to resize panes - tables scale with width");
	}

	ImGui::SameLine(0.f, kPaneGap);
	ImGui::BeginChild("###gw2igh_lm_side", ImVec2(0.f, bodyH), true, PadNav::kLockScroll);
	if (gFocusSetupTab)
	{
		gSideTab = static_cast<int>(SideTab::Setup);
		gFocusSetupTab = false;
	}
	static const char* kTabs[] = {
		"Detail", "Players", "Stats", "KillProof", "Guilds", "Fastest", "Setup"
	};
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::LmDetail),
		static_cast<int>(Gw2Ui::Icon::LmPlayers),
		static_cast<int>(Gw2Ui::Icon::LogsSwords),
		static_cast<int>(Gw2Ui::Icon::LmKillProof),
		static_cast<int>(Gw2Ui::Icon::LmGuilds),
		static_cast<int>(Gw2Ui::Icon::LmFastest),
		static_cast<int>(Gw2Ui::Icon::SettingsGear),
	};
	gSideTab = PadNav::DrawTopBar("###gw2igh_lm_nav", kTabs, static_cast<int>(SideTab::Count),
		gSideTab, kTabIcons);
	ImGui::BeginChild("###gw2igh_lm_side_body", ImVec2(0.f, 0.f), false);
	switch (gSideTab)
	{
	case static_cast<int>(SideTab::Detail): DrawDetailTab(filtered); break;
	case static_cast<int>(SideTab::Players): DrawPlayersTab(filtered); break;
	case static_cast<int>(SideTab::Stats): DrawStatsTab(filtered); break;
	case static_cast<int>(SideTab::KillProof): DrawKillProofTab(); break;
	case static_cast<int>(SideTab::Guilds): DrawGuildsTab(filtered); break;
	case static_cast<int>(SideTab::Fastest): DrawFastestTab(filtered); break;
	case static_cast<int>(SideTab::Setup): DrawSetupTab(hasDotNet); break;
	default: break;
	}
	ImGui::EndChild();
	ImGui::EndChild();

	{
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 sz = ImGui::GetWindowSize();
		const bool moved =
			std::fabs(pos.x - G::LogManagerWinX) > 0.5f ||
			std::fabs(pos.y - G::LogManagerWinY) > 0.5f ||
			std::fabs(sz.x - G::LogManagerWinW) > 0.5f ||
			std::fabs(sz.y - G::LogManagerWinH) > 0.5f;
		if (moved)
		{
			G::LogManagerWinX = pos.x;
			G::LogManagerWinY = pos.y;
			G::LogManagerWinW = sz.x;
			G::LogManagerWinH = sz.y;
			Settings::SetDirty();
		}
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	HelperTheme::EndPad();
	return hovered;
}
