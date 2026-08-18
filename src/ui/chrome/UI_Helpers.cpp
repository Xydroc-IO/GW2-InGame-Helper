#include "UI.h"
#include "UIInternal.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "MumbleIdentity.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
#include "CraftingPad.h"
#include "VaultPad.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "LogManagerPad.h"
#include "EconomyPad.h"
#include "InstancesPad.h"
#include "CompletionPad.h"
#include "FarmingPad.h"
#include "EventAlert.h"
#include "GpsArrow.h"
#include "ZoneBanner.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "PadNav.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "DirectionCompass.h"
#include "SettingsPad.h"
#include "WatchPad.h"
#include "Settings.h"
#include "Sites.h"
#include "AspectLayout.h"
#include "UiScale.h"
#include "WikiBrowser.h"
#include "WinePadOpen.h"
#include "WatchPadInternal.h"
#include "CrashTrail.h"
#include "WikiIpc.h"
#include "AddonPaths.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace UIDetail
{
	float Clampf(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	UiContext gUi;

	bool HelperGeomOffscreen(float dispW, float dispH)
	{
		if (dispW < 100.f || dispH < 100.f)
			return false;
		const float title = 48.f;
		if (G::WindowPosX > dispW - title)
			return true;
		if (G::WindowPosY > dispH - title)
			return true;
		if (G::WindowPosX + G::WindowWidth < title)
			return true;
		if (G::WindowPosY + title < 0.f)
			return true;
		return false;
	}

	void ClampHelperGeomToDisplay()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const float dw = io.DisplaySize.x;
		const float dh = io.DisplaySize.y;
		if (dw < 100.f || dh < 100.f)
			return;

		const AspectLayout::HelperGeom lim = AspectLayout::DefaultHelper(dw, dh);
		const float maxW = Clampf(lim.maxW, 320.f, dw * 0.98f);
		const float maxH = Clampf(lim.maxH, 240.f, dh * 0.98f);
		bool changed = false;
		if (G::WindowWidth > maxW + 0.5f || G::WindowWidth < 320.f)
		{
			G::WindowWidth = Clampf(G::WindowWidth, 320.f, maxW);
			changed = true;
		}
		if (G::WindowHeight > maxH + 0.5f || G::WindowHeight < 240.f)
		{
			G::WindowHeight = Clampf(G::WindowHeight, 240.f, maxH);
			changed = true;
		}

		/* Rescue only when the saved rect is off-screen. Do NOT treat
		   forceHelperOnScreen as a relocate — that flag means “apply G:: geom
		   with Cond_Always on reopen” and was wiping the user’s last position. */
		if (HelperGeomOffscreen(dw, dh))
		{
			G::WindowPosX = lim.posX;
			G::WindowPosY = lim.posY;
			G::HasSavedPos = true;
			gUi.forceHelperOnScreen = true;
			changed = true;
		}
		else
		{
			const float nx = Clampf(G::WindowPosX, 0.f, dw - 48.f);
			const float ny = Clampf(G::WindowPosY, 0.f, dh - 48.f);
			if (std::fabs(nx - G::WindowPosX) > 0.5f || std::fabs(ny - G::WindowPosY) > 0.5f)
			{
				G::WindowPosX = nx;
				G::WindowPosY = ny;
				G::HasSavedPos = true;
				changed = true;
			}
		}
		if (changed)
			Settings::SetDirty();
	}

	const ImVec4& kGold = HelperTheme::Gold;
	const ImVec4& kGoldBright = HelperTheme::GoldBright;
	const ImVec4& kGoldDim = HelperTheme::GoldDim;
	const ImVec4& kGoldMuted = HelperTheme::GoldMuted;
	const ImVec4& kMuted = HelperTheme::Muted;
	const ImVec4& kBg = HelperTheme::Bg;
	const ImVec4& kPanel = HelperTheme::Panel;
	const ImVec4& kBorder = HelperTheme::Border;
	const ImVec4& kTabActive = HelperTheme::TabActive;
	const ImVec4& kTabIdle = HelperTheme::TabIdle;
	const ImVec4& kWarn = HelperTheme::Warn;

	void PushWikiTheme()
	{
		HelperTheme::Push();
	}

	void PopWikiTheme()
	{
		HelperTheme::Pop();
	}

	bool SoftButton(const char* label, bool enabled)
	{
		if (!enabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button(label);
			ImGui::PopStyleVar();
			return false;
		}
		return ImGui::Button(label);
	}

	void BlurBrowser()
	{
		if (!gUi.browserFocused)
			return;
		gUi.browserFocused = false;
		/* Do not clear gUi.overBrowserPage here - the render loop owns hover state.
		   Clearing it stole CEF focus while keys still reached the page (no caret). */
		WikiBrowser::FeedFocus(false);
	}

	void FocusBrowser()
	{
		if (gUi.browserFocused)
			return;
		gUi.browserFocused = true;
		WikiBrowser::FeedFocus(true);
	}

	void FocusBrowserForce()
	{
		gUi.browserFocused = true;
		WikiBrowser::FeedFocus(true);
	}

	unsigned CefModsFromImGui(const ImGuiIO& io)
	{
		unsigned m = 0;
		if (io.KeyShift) m |= 1u << 1;
		if (io.KeyCtrl)  m |= 1u << 2;
		if (io.KeyAlt)   m |= 1u << 3;
		if (io.MouseDown[0]) m |= 1u << 4;
		if (io.MouseDown[2]) m |= 1u << 6;
		if (io.MouseDown[1]) m |= 1u << 5;
		return m;
	}

	void MapToCef(float localX, float localY, float drawW, float drawH, int* outX, int* outY)
	{
		const int fw = WikiBrowser::FrameWidth();
		const int fh = WikiBrowser::FrameHeight();
		if (fw <= 0 || fh <= 0 || drawW < 1.f || drawH < 1.f)
		{
			*outX = static_cast<int>(localX);
			*outY = static_cast<int>(localY);
			return;
		}
		int x = static_cast<int>(localX * (static_cast<float>(fw) / drawW));
		int y = static_cast<int>(localY * (static_cast<float>(fh) / drawH));
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= fw) x = fw - 1;
		if (y >= fh) y = fh - 1;
		*outX = x;
		*outY = y;
	}

	/* NitroPay gates slots with matchMedia min-width up to 1840px. OSR innerWidth is
	   the ImGui panel, so a normal helper window never unlocks desktop rails. These
	   hosts get a desktop-sized CEF view (scaled into the panel) so side rails and
	   large footers lay out like a normal browser. */
	bool HostWantsDesktopAdViewport(const char* url)
	{
		if (!url || !url[0])
			return false;
		return std::strstr(url, "gw2efficiency.com") != nullptr ||
			std::strstr(url, "snowcrows.com") != nullptr ||
			std::strstr(url, "metabattle.com") != nullptr ||
			std::strstr(url, "guildjen.com") != nullptr;
	}

	void DesktopAdCefSize(float panelW, float panelH, float* outW, float* outH)
	{
		/* Full HD layout width so (min-width: 1840px) / footer / video queries pass.
		   Height tracks the panel's aspect ratio so the bitmap scales uniformly into
		   the slot (same feel as resizing a normal browser window) instead of
		   stretching 1920xfixed-H into a mismatched panel. */
		const float maxW = static_cast<float>(kWikiFrameMaxW);
		const float maxH = static_cast<float>(kWikiFrameMaxH);
		const float minH = 900.f;
		*outW = maxW;
		float h = (panelW > 1.f) ? (panelH * (maxW / panelW)) : panelH;
		if (h < minH)
			h = minH;
		if (h > maxH)
			h = maxH;
		*outH = h;
	}

	void FitContentInPanel(float panelW, float panelH, float contentW, float contentH,
		float* outX, float* outY, float* outW, float* outH)
	{
		if (panelW < 1.f || panelH < 1.f || contentW < 1.f || contentH < 1.f)
		{
			*outX = 0.f;
			*outY = 0.f;
			*outW = panelW;
			*outH = panelH;
			return;
		}
		const float scale = std::min(panelW / contentW, panelH / contentH);
		*outW = contentW * scale;
		*outH = contentH * scale;
		*outX = (panelW - *outW) * 0.5f;
		*outY = (panelH - *outH) * 0.5f;
	}

	bool sShowFind = false;
	/* Set while the cursor is over one of our popups (Browse / More / tab menu).
	   Avoids ImGuiHoveredFlags_AnyWindow, which also matched Nexus windows. */
	bool sHelperPopupHovered = false;

	bool sFocusFind = false;
	bool sFocusAddress = false;
	char sFindQuery[128] = {};
	bool sFindMatchCase = false;

	bool AnyToolPadOpen()
	{
		return G::ShowNotes || G::ShowAccount || G::ShowTpWatch || G::ShowLookup ||
			G::ShowWallet || G::ShowVault || G::ShowEvents || G::ShowLogManager ||
			G::ShowEconomy || G::ShowCrafting || G::ShowInstances || G::ShowAchievements || G::ShowFarming ||
			G::ShowPathingGuides ||
			G::ShowCompassPad || G::ShowWatch || G::ShowWatchMirror || G::ShowSettings;
	}

	void RenderCompanionPads()
	{
		/* First soft-open settle frame only: Events Begins before Watch
		   in this list — trail shows which Begin tips. */
		/* soft-open settle frame, or any DetailArmed soft-stop /
		   defer frame: probe all open pads. */
		const bool settleProbe = WinePadOpen::Soft()
			&& WinePadOpen::CompanionSettleFrames() == WinePadOpen::DeferFrames() * 4;
		const bool stopProbe = CrashTrail::DetailArmed()
			&& (WatchPadDetail::gSoftStopPhase > 0
				|| WatchPadDetail::gDeferStopFrames > 0);
		const bool probePads = settleProbe || stopProbe;
		auto probe = [&](const char* name, bool show, bool (*fn)()) -> bool {
			if (probePads && show)
				CrashTrail::NoteF("pads:pre %s", name);
			const bool h = fn();
			if (probePads && show)
				CrashTrail::NoteF("pads:post %s", name);
			return h;
		};
		if (settleProbe)
			CrashTrail::NoteF("pads:begin settle pending=%s",
				WinePadOpen::PendingCompanionName());
		else if (stopProbe)
			CrashTrail::NoteF("pads:begin softstop phase=%d defer=%d",
				WatchPadDetail::gSoftStopPhase, WatchPadDetail::gDeferStopFrames);
		const bool notesHover = probe("Notes", G::ShowNotes, &NotesPad::Render);
		const bool accountHover = probe("Account", G::ShowAccount, &AccountPad::Render);
		const bool tpHover = probe("TpWatch", G::ShowTpWatch, &TpWatchPad::Render);
		const bool lookupHover = probe("Lookup", G::ShowLookup, &LookupPad::Render);
		const bool stashHover = probe("Stash", G::ShowWallet, &WalletPad::Render);
		const bool vaultHover = probe("Vault", G::ShowVault, &VaultPad::Render);
		const bool eventsHover = probe("Events", G::ShowEvents, &EventsPad::Render);
		const bool logsHover = probe("Logs", G::ShowLogManager, &LogManagerPad::Render);
		const bool economyHover = probe("Economy", G::ShowEconomy, &EconomyPad::Render);
		const bool craftingHover = probe("Crafting", G::ShowCrafting, &CraftingPad::Render);
		const bool instancesHover = probe("Instances", G::ShowInstances, &InstancesPad::Render);
		const bool achievementsHover = probe("Achievements", G::ShowAchievements, &CompletionPad::RenderAchievements);
		const bool farmingHover = probe("Farming", G::ShowFarming, &FarmingPad::Render);
		CompletionPad::Tick();
		FarmingPad::Tick();
		if (probePads)
			CrashTrail::Note("pads:pre GpsArrow");
		const bool gpsArrowHover = GpsArrow::Render();
		if (probePads)
			CrashTrail::Note("pads:pre ZoneBanner");
		ZoneBanner::Render();
		if (probePads)
			CrashTrail::Note("pads:pre EventAlert");
		const bool eventAlertHover = EventAlert::Render();
		const bool tekkitHover = probe("Pathing", G::ShowPathingGuides, &PathingGuidesPad::Render);
		const bool compassHover = probe("Compass", G::ShowCompassPad, &DirectionCompass::RenderPad);
		if (probePads)
			CrashTrail::Note("pads:pre Watch");
		const bool watchHover = WatchPad::Render();
		if (probePads)
			CrashTrail::Note("pads:post Watch");
		const bool settingsHover = probe("Settings", G::ShowSettings, &SettingsPad::Render);
		if (probePads)
			CrashTrail::NoteF("pads:end %s", settleProbe ? "settle" : "softstop");
		CaptureForToolPads(notesHover || accountHover || tpHover || lookupHover ||
			stashHover || vaultHover || eventsHover || logsHover ||
			economyHover || craftingHover || instancesHover || achievementsHover || farmingHover ||
			gpsArrowHover || eventAlertHover ||
			tekkitHover || compassHover || watchHover || settingsHover);
		if (probePads)
			CrashTrail::Note("pads:pre NotesPad::Save");
		NotesPad::Save(false);
		if (probePads)
			CrashTrail::Note("pads:pre Settings::Save");
		Settings::Save(false);
		if (probePads)
			CrashTrail::Note("pads:after Save");
	}

	void GlueSideRailDisplayOrder()
	{
		/* Nav is a sibling window. Clicking Vault/Account/Compass focuses those
		   pads above the rail; dragging the helper then fronts the body and
		   leaves pads between rail and content. Keep rail glued under helper. */
		/* Wine: never erase/insert g.Windows — Soft-busy frames and long-idle
		   helper sessions both tip over when glue races Present + pad Begin. */
		if (WinePadOpen::Soft())
			return;
		ImGuiWindow* helper = ImGui::FindWindowByName("In-Game Helper##GW2InGameHelper");
		ImGuiWindow* rail = ImGui::FindWindowByName("##gw2igh_side_dock");
		if (!helper || !rail || !helper->WasActive || !rail->WasActive)
			return;

		ImGuiContext& g = *GImGui;
		int helperIdx = -1;
		int railIdx = -1;
		for (int i = 0; i < g.Windows.Size; ++i)
		{
			if (g.Windows[i] == helper)
				helperIdx = i;
			else if (g.Windows[i] == rail)
				railIdx = i;
		}
		if (helperIdx < 0 || railIdx < 0)
			return;
		if (railIdx == helperIdx - 1)
			return;

		g.Windows.erase(&g.Windows[railIdx]);
		if (railIdx < helperIdx)
			--helperIdx;
		g.Windows.insert(&g.Windows[helperIdx], rail);
	}

	/* Nexus shares one ImGui context. Pie UI / other addons also spawn ##Combo
	   popups — treating those as ours ate their clicks (WndProc return 0 +
	   CaptureMouseFromApp). Only latch combos whose root window is ours. */
	bool WindowIsHelperOwned(const ImGuiWindow* w)
	{
		if (!w)
			return false;
		const ImGuiWindow* root = w->RootWindow ? w->RootWindow : w;
		const char* n = root->Name;
		if (!n || !n[0])
			return false;
		return std::strstr(n, "GW2InGameHelper") != nullptr
			|| std::strstr(n, "gw2igh") != nullptr;
	}

	bool ComboPopupIsOurs(const ImGuiWindow* w)
	{
		if (!w || std::strncmp(w->Name, "##Combo", 7) != 0)
			return false;
		/* Popup RootWindow is the combo itself (##Combo_…) — walk parents. */
		for (const ImGuiWindow* p = w->ParentWindow; p; p = p->ParentWindow)
		{
			if (WindowIsHelperOwned(p))
				return true;
		}
		return false;
	}

	/* BeginCombo / ImGui::Combo lists are separate popup windows. Cursor leaves
	   the pad rect -> pad Hover is false -> we used to stop capturing -> GW2 ate
	   the click. Latch while a combo popup is up after our pads opened it. */
	bool HoveringComboPopup()
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* w = g.HoveredWindow;
		if (!w || (w->Flags & ImGuiWindowFlags_Popup) == 0)
			return false;
		return ComboPopupIsOurs(w);
	}

	bool ComboPopupOpen()
	{
		ImGuiContext& g = *GImGui;
		for (int i = 0; i < g.OpenPopupStack.Size; ++i)
		{
			ImGuiWindow* w = g.OpenPopupStack[i].Window;
			if (ComboPopupIsOurs(w))
				return true;
		}
		return false;
	}

	/* BeginCombo lists live outside the pad rect - keep capturing until closed. */
	void CaptureForToolPads(bool padsHover)
	{
		static bool sComboLatch = false;
		const bool padsOpen = AnyToolPadOpen();
		const bool overCombo = HoveringComboPopup();
		const bool comboOpen = ComboPopupOpen();

		if (!padsOpen || !comboOpen)
			sComboLatch = false;
		else if (padsHover || overCombo)
			sComboLatch = true;

		if (padsHover || sHelperPopupHovered || sComboLatch || overCombo)
		{
			gUi.blockGameMouse = true;
			gUi.blockGameKeyboard = true;
			ImGui::CaptureMouseFromApp(true);
			if (ImGui::GetIO().WantTextInput)
				ImGui::CaptureKeyboardFromApp(true);
		}
	}

} // namespace UIDetail
