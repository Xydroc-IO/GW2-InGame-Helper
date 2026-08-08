#include "UI.h"
#include "UIInternal.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "CharacterProfiles.h"
#include "ConfirmedWaypoints.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Gw2Icons.h"
#include "Gw2Ui.h"
#include "LivePanels.h"
#include "MumbleIdentity.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
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
#include "MapAssist.h"
#include "ZoneBanner.h"
#include "PathingGuidesPad.h"
#include "TrailToolsPad.h"
#include "PathingTrails.h"
#include "PadNav.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "DirectionCompass.h"
#include "SettingsPad.h"
#include "Settings.h"
#include "Sites.h"
#include "AspectLayout.h"
#include "UiScale.h"
#include "WikiBrowser.h"
#include "WikiIpc.h"
#include "AddonPaths.h"
#include "TrailToolsBinds.h"
#include "PanelBinds.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <shellapi.h>

using namespace UIDetail;

void UI_Render()
{
	/* Always poll first - must run while the helper is closed too. */
	HelperHotkeys_Poll();
	TrailToolsBinds::Poll();
	PanelBinds::Poll();
	UiScale::TickAuto();
	Gw2Icons::Tick();
	Gw2Icons::WarmProfessionIcons();
	Gw2Ui::WarmCommon();
	WikiBrowser::Tick();
	MumbleIdentity::Tick();
	CharacterProfiles::Tick();
	ConfirmedWaypoints::Tick();
	MapAssist::Tick();
	/* Tekkit overlays - always, even with the browser closed. */
	CompassOverlay::Render();
	WorldOverlay::Render();
	PathingTrails::DrawMarkerBehaviorOverlay();
	DirectionCompass::Render();
	/* URL-index warm: heavier when closed; light drip while open so Browse stays snappy. */
	if (!G::ShowWiki)
		Sites::TickWarmUrlKeys(96);
	else if (!Sites::UrlKeysReady())
		Sites::TickWarmUrlKeys(16);

	gUi.blockGameKeyboard = false;
	gUi.blockGameMouse = false;
	gUi.overBrowserPage = false;
	gUi.wikiRectValid = false;
	sHelperPopupHovered = false;

	if (gUi.pendingDefocus)
	{
		gUi.pendingDefocus = false;
		ImGui::SetWindowFocus(nullptr);
	}

	static bool sWasOpen = false;
	if (!G::ShowWiki)
	{
		G::SideRailW = 0.f;
		/* Do NOT clear WantTextInput / WantCaptureKeyboard here - those flags are
		   shared with Nexus. Wiping them every frame breaks the library search field
		   (keys fall through to GW2 hotkeys, e.g. G = guild). */

		if (sWasOpen)
		{
			BrowserTabs::PrepareSave();
			Settings::SetDirty();
			UI_ReleaseGameInput();
			sWasOpen = false;
		}
		BlurBrowser();
		WikiBrowser::SetVisible(false);

		/* Notes / TP / Lookup / Wallet can stay open while the helper browser is closed.
		   Only block GW2 while the pointer is over those windows - do not
		   force Capture*FromApp(false) every frame (breaks Nexus / open). */
		RenderCompanionPads();
		return;
	}

	if (!sWasOpen)
	{
		gUi.forceHelperOnScreen = true; /* always verify visible on each open */
		BrowserTabs::NavigateActive();
		sWasOpen = true;
	}

	if (!G::HasSavedSize)
	{
		const ImGuiIO& dio = ImGui::GetIO();
		if (dio.DisplaySize.x > 100.f && dio.DisplaySize.y > 100.f)
		{
			/* First open: aspect-aware defaults (16:9 / 21:9 / 32:9). */
			const AspectLayout::HelperGeom def =
				AspectLayout::DefaultHelper(dio.DisplaySize.x, dio.DisplaySize.y);
			G::WindowWidth = def.width;
			G::WindowHeight = def.height;
			if (!G::HasSavedPos)
			{
				G::WindowPosX = def.posX;
				G::WindowPosY = def.posY;
				G::HasSavedPos = true;
			}
			G::HasSavedSize = true; /* apply once - ImGui FirstUseEver + persist */
			Settings::SetDirty();
		}
	}
	ClampHelperGeomToDisplay();
	const ImGuiIO& sizeIo = ImGui::GetIO();
	/* Previous-frame collapse — allow title-bar min height before Begin.
	   Never cap maxH while collapsed: that clamped Restore to ~40px. */
	static bool sHelperCollapsedPrev = false;
	if (sizeIo.DisplaySize.x > 100.f && sizeIo.DisplaySize.y > 100.f)
	{
		const AspectLayout::HelperGeom lim =
			AspectLayout::DefaultHelper(sizeIo.DisplaySize.x, sizeIo.DisplaySize.y);
		const float minH = sHelperCollapsedPrev ? 28.f : 240.f;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(320.f, minH),
			ImVec2(lim.maxW, lim.maxH));
	}
	const ImGuiCond geomCond = gUi.forceHelperOnScreen ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), geomCond);
	ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), geomCond);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gUi.forceHelperOnScreen)
		ImGui::SetNextWindowFocus();
	gUi.forceHelperOnScreen = false;

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(0.f);

	bool open = G::ShowWiki;
	ImGuiWindowFlags helperFlags =
		HelperTheme::PadFlags(ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar);
	if (sHelperCollapsedPrev)
		helperFlags |= ImGuiWindowFlags_NoResize;
	const bool padBody = ImGui::Begin("In-Game Helper##GW2InGameHelper", &open, helperFlags);
	const bool helperCollapsed = ImGui::GetStateStorage()->GetBool(
		ImGui::GetID("##gw2igh_pad_collapsed"), false);
	sHelperCollapsedPrev = helperCollapsed;
	if (!helperCollapsed)
		/* Left joins nav; outer right gets the Hero ink rim (whole plate silhouette). */
		Gw2Ui::PaintPadChrome(G::Opacity, /*omitLeftEdge=*/true, /*omitRightEdge=*/false);
	const float railW = HelperSideRailWidth();
	G::SideRailW = railW;
	const bool expanded = Gw2Ui::DrawPadTitleBar(
		"Game Helper", &open, G::Opacity,
		/* Flush with side-rail outer edge (UiScale uses rail chrome pads, not helper theme). */
		railW);
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		UI_ReleaseGameInput();
	}
	if (!expanded || !padBody)
	{
		G::SideRailW = 0.f; /* nav column only while helper body is expanded */
		/* Minimized title strip - CEF must be was_hidden (0% viewability otherwise)
		   but keep the process alive so expand does not hitch. */
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 winSize = ImGui::GetWindowSize();
		gUi.wikiMin = pos;
		gUi.wikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
		gUi.wikiRectValid = true;
		BlurBrowser();
		WikiBrowser::SetVisible(false, /*keepProcessAlive=*/true);
		const bool mouseOver =
			ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		gUi.blockGameMouse = mouseOver;
		/* Collapsed bar: only eat keys while the pointer is on it. */
		gUi.blockGameKeyboard = mouseOver;
		if (mouseOver)
		{
			ImGui::GetIO().WantCaptureMouse = true;
			ImGui::CaptureMouseFromApp(true);
			ImGui::GetIO().WantCaptureKeyboard = true;
			ImGui::CaptureKeyboardFromApp(true);
		}
		HelperTheme::EndPad();
		ImGui::PopStyleVar();
		PopWikiTheme();
		/* Do not persist minimized height into G::WindowHeight. */
		if (std::fabs(pos.x - G::WindowPosX) > 0.5f || std::fabs(pos.y - G::WindowPosY) > 0.5f)
		{
			G::WindowPosX = pos.x;
			G::WindowPosY = pos.y;
			G::HasSavedPos = true;
			Settings::SetDirty();
		}
		/* Still draw pads while the main window is collapsed. */
		RenderCompanionPads();
		return;
	}

	ImGui::SetWindowFontScale(UiScale::EffectiveFontScale(
		HelperTheme::kPadFontRefW, HelperTheme::kPadFontRefH));

	BrowserTabs::EnsureDefault();
	BrowserTabs::Tick();

	DrawToolbar();

	/* Tab / find hotkeys - use ImGuiIO (Nexus-filled KeysDown), not GetAsyncKeyState. */
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool typing = io.WantTextInput;
		const bool ctrl = io.KeyCtrl;
		const bool shift = io.KeyShift;
		const bool alt = io.KeyAlt;
		auto keyDown = [&](int vk) -> bool {
			return vk >= 0 && vk < IM_ARRAYSIZE(io.KeysDown) && io.KeysDown[vk];
		};
		const bool keyF = keyDown('F');
		const bool keyT = keyDown('T');
		const bool keyW = keyDown('W');
		const bool keyTab = ImGui::IsKeyDown(ImGuiKey_Tab);

		static bool sCtrlFWasDown = false;
		static bool sCtrlTWasDown = false;
		static bool sCtrlWWasDown = false;
		static bool sCtrlTabWasDown = false;

		const bool ctrlF = !typing && ctrl && !shift && !alt && keyF;
		const bool ctrlT = !typing && ctrl && !shift && !alt && keyT;
		const bool ctrlW = !typing && ctrl && !shift && !alt && keyW;
		const bool ctrlShiftT = !typing && ctrl && shift && !alt && keyT;
		const bool ctrlTab = !typing && ctrl && !alt && keyTab;

		if (ctrlF && !sCtrlFWasDown)
		{
			sShowFind = true;
			sFocusFind = true;
		}
		if (ctrlT && !sCtrlTWasDown)
			UI_Browse_RequestNewTabPicker();
		if (ctrlW && !sCtrlWWasDown)
		{
			const int ai = BrowserTabs::ActiveIndex();
			if (BrowserTabs::Count() > 1 && !BrowserTabs::At(ai).pinned)
				BrowserTabs::Close(ai);
		}
		if (ctrlShiftT && !sCtrlTWasDown && BrowserTabs::CanReopenClosed())
			BrowserTabs::ReopenClosed();
		if (ctrlTab && !sCtrlTabWasDown)
		{
			const int n = BrowserTabs::Count();
			if (n > 1)
			{
				int next = BrowserTabs::ActiveIndex() + (shift ? -1 : 1);
				if (next < 0) next = n - 1;
				if (next >= n) next = 0;
				BrowserTabs::Activate(next);
			}
		}

		sCtrlFWasDown = ctrlF;
		sCtrlTWasDown = ctrlT || ctrlShiftT;
		sCtrlWWasDown = ctrlW;
		sCtrlTabWasDown = ctrlTab;

		if (sShowFind && ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			sShowFind = false;
			WikiBrowser::StopFind(true);
		}
	}

	if (sShowFind)
	{
		ImGui::TextColored(kGoldDim, "Find");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.f);
		if (sFocusFind)
		{
			ImGui::SetKeyboardFocusHere();
			sFocusFind = false;
		}
		const bool findEnter = ImGui::InputTextWithHint("###gw2igh_find_q", "Find in page...", sFindQuery, sizeof(sFindQuery),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::Checkbox("Aa###gw2igh_find_case", &sFindMatchCase);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Match case");
		ImGui::SameLine();
		if (ImGui::Button("Next###gw2igh_find_next") || findEnter)
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Prev###gw2igh_find_prev"))
		{
			if (sFindQuery[0])
				WikiBrowser::Find(sFindQuery, false, sFindMatchCase, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear###gw2igh_find_clear"))
		{
			WikiBrowser::StopFind(true);
			sFindQuery[0] = 0;
		}
		ImGui::SameLine();
		const uint32_t fc = WikiBrowser::FindCount();
		const uint32_t fo = WikiBrowser::FindOrdinal();
		if (fc > 0)
			ImGui::TextColored(kGoldMuted, "%u / %u", fo, fc);
		else if (sFindQuery[0])
			ImGui::TextColored(kGoldMuted, "No matches");
	}

	DrawTabBar();

	const char* url = WikiBrowser::CurrentUrlCStr();
	if (url && url[0] && std::strncmp(url, "about:", 6) != 0)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
		ImGui::TextUnformatted(url);
		ImGui::PopStyleColor();
	}

	ImGui::Separator();

	/* Flush CEF edge-to-edge. Pops MUST happen inside DrawWikiPageSlot
	   before End/PopWikiTheme — that function owns the window teardown.
	   Side dock is a separate window locked to the helper's left edge.
	   Pads render after the rail; then glue rail under helper so Vault /
	   Account / Compass cannot sit between nav and body when the helper moves. */
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, ImGui::GetStyle().ItemSpacing.y));
	DrawWikiPageSlot(open);
	DrawHelperSideRail();
	RenderCompanionPads();
	GlueSideRailDisplayOrder();
}
