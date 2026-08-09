#include "AccountPad.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "ProgressData.h"
#include "SessionHistoryData.h"
#include "Settings.h"
#include "UnlocksData.h"
#include "UnlocksPad.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>

#include <windows.h>

namespace
{
	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;

	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gAccountTab = 0; /* Overview / Progress / Unlocks / History */
	int  gDeferRefresh = 0;

	void KickRefresh()
	{
		ProgressData::RefreshIfNeeded(false);
		UnlocksData::EnsureLoaded(UnlocksData::Kind::Skins, false);
	}

	void SectionLabel(const char* label)
	{
		ImGui::Spacing();
		PadNav::SectionTitle(label);
		ImGui::Separator();
		ImGui::Spacing();
	}

	void DrawOverview()
	{
		const bool hasKey = G::Gw2ApiKey[0] != '\0';

		PadNav::Blurb(
			"Unlocks, legendary progress, and session history - official API, read-only. "
			"Vault is on the side rail. Stash, trading, item lookup, and crafting live under Economy.");
		ImGui::Spacing();

		ImGui::BeginChild("###gw2igh_acct_keycard", ImVec2(0.f, hasKey ? 110.f : 140.f), true);
		if (hasKey)
		{
			ImGui::TextColored(HelperTheme::Ok, "API key saved locally");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Scopes: account | characters | progression | unlocks");
			PadNav::PopWrap();
		}
		else
		{
			ImGui::TextColored(HelperTheme::Warn, "No API key yet");
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Add one under Settings (helper side rail). "
				"Unlocks / progress need it. Vault is its own pad. "
				"Economy stash & trading need inventories / wallet / tradingpost.");
			PadNav::PopWrap();
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (Gw2Ui::IconLabelButton("Refresh all###gw2igh_acct_refall", Gw2Ui::Icon::Bag, 18.f))
		{
			ProgressData::RefreshIfNeeded(true);
			UnlocksData::EnsureAll(true);
		}
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Pulls progress and unlocks.");
		PadNav::PopWrap();

		SessionHistoryData::RenderOverviewSnippet();

		SectionLabel("TOOLS");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Use the tabs on the left - each tool stays in this window.");
		PadNav::PopWrap();
		ImGui::Spacing();

		const float gap = ImGui::GetStyle().ItemSpacing.x;
		const float colW = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
		auto toolCell = [&](const char* title, const char* blurb) {
			ImGui::BeginGroup();
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + colW);
			ImGui::TextColored(HelperTheme::GoldMuted, "%s", title);
			ImGui::TextColored(HelperTheme::Muted, "%s", blurb);
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
		};

		toolCell("Progress", "Legendaries | armory");
		PadNav::WrapSameLine(colW);
		toolCell("Unlocks", "Skins | dyes | minis");

		ImGui::Spacing();
		toolCell("History", "Session snippets");

		ImGui::Spacing();
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Vault is on the helper side rail. Stash, trading, item lookup, and crafting are on Economy.");
		PadNav::PopWrap();
	}
}

void AccountPad::OpenAndRefresh()
{
	G::ShowAccount = true;
	gFocus = true;
	gPlaceOnce = true;
	/* Wine soft-open: Save on Soft Begin after Mirror tipped — Capture dirties later. */
	if (!WinePadOpen::Soft())
		Settings::SetDirty();
	if (ImGuiWindow* w = ImGui::FindWindowByName("Account###GW2InGameHelperAccount"))
		w->StateStorage.SetBool(w->GetID("##gw2igh_pad_collapsed"), false);
	gDeferRefresh = WinePadOpen::DeferFrames();
	if (gDeferRefresh <= 0)
		KickRefresh();
}

bool AccountPad::Render()
{
	if (!G::ShowAccount)
		return false;

	if (WinePadOpen::TickDefer(gDeferRefresh))
		KickRefresh();

	UnlocksData::Tick();
	SessionHistoryData::Tick();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(360.f);
	PadDock::SetSizeConstraints("Account###GW2InGameHelperAccount", 440.f, 360.f, PadDock::MaxW(720.f), maxH);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.34f) : 100.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.1f) : 70.f;
		PadDock::Place(G::PadAccount, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadAccount.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	HelperTheme::ScopedWindow theme(G::Opacity);

	bool open = G::ShowAccount;
	const bool padBody = ImGui::Begin("Account###GW2InGameHelperAccount", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Account", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadAccount))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowAccount = false;
			Settings::SetDirty();
		}
		return hovered;
	}

	if (!open)
	{
		G::ShowAccount = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadAccount))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	static const char* kTabs[] = {
		"Overview", "Progress", "Unlocks", "History"
	};
	static const int kTabIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Hero),
		static_cast<int>(Gw2Ui::Icon::Achievements),
		static_cast<int>(Gw2Ui::Icon::LockBag),
		static_cast<int>(Gw2Ui::Icon::Story),
	};
	if (gAccountTab < 0 || gAccountTab > 3)
		gAccountTab = 0;
	gAccountTab = PadNav::DrawSideRail("###gw2igh_acct_nav", kTabs, 4, gAccountTab, 0.f, kTabIcons);

	ImGui::BeginChild("###gw2igh_acct_body", ImVec2(0.f, 0.f), gAccountTab != 0);
	switch (gAccountTab)
	{
	case 0: DrawOverview(); break;
	case 1:
		ProgressData::RefreshIfNeeded(false);
		ProgressData::RenderContents();
		break;
	case 2: UnlocksPad::RenderContents(); break;
	case 3: SessionHistoryData::RenderContents(); break;
	default: break;
	}
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
