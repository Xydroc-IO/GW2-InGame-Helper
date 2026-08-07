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
#include "VaultPad.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "LogManagerPad.h"
#include "EconomyPad.h"
#include "InstancesPad.h"
#include "CompletionPad.h"
#include "FarmingPad.h"
#include "GpsArrow.h"
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
#include "UiScale.h"
#include "WikiBrowser.h"
#include "WikiIpc.h"
#include "AddonPaths.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include <windows.h>
#include <shellapi.h>

namespace UIDetail
{
	void DrawWikiPageSlot(bool open)
	{
	/* Fill remaining space (0,0) — sizing to GetContentRegionAvail() often
	   overshoots by a pixel and forces a parent scrollbar + grey gutters. */
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::BeginChild("##gw2igh_wiki_osr_slot", ImVec2(0.f, 0.f), false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	const ImVec2 slotPos = ImGui::GetCursorScreenPos();
	const ImGuiIO& dio = ImGui::GetIO();
	const float dispW = dio.DisplaySize.x;
	const float dispH = dio.DisplaySize.y;
	/* Any overlap with the game display - fully off-screen is not viewable. */
	const bool slotOnScreen =
		dispW < 1.f || dispH < 1.f ||
		(slotPos.x + imageSize.x > 0.f && slotPos.y + imageSize.y > 0.f &&
			slotPos.x < dispW && slotPos.y < dispH);
	/* Hysteresis on size so drag-resize does not flap was_hidden (that can
	   interrupt ad / page timers). Opacity is NOT a gate - users still read
	   at low opacity; freezing CEF there would break content. */
	static bool sSlotLargeEnough = true;
	if (sSlotLargeEnough)
		sSlotLargeEnough = imageSize.x >= 36.f && imageSize.y >= 36.f;
	else
		sSlotLargeEnough = imageSize.x >= 48.f && imageSize.y >= 48.f;
	const bool pageViewable =
		G::ShowWiki && open && slotOnScreen && sSlotLargeEnough;
	if (pageViewable)
	{
		float cefW = imageSize.x;
		float cefH = imageSize.y;
		if (HostWantsDesktopAdViewport(WikiBrowser::CurrentUrlCStr()))
			DesktopAdCefSize(imageSize.x, imageSize.y, &cefW, &cefH);
		WikiBrowser::SetVisible(true);
		WikiBrowser::SetBounds(0.f, 0.f, cefW, cefH);
		WikiBrowser::PresentFrame();
	}
	else
	{
		/* Keep process - window may still be open (tiny / off-screen). */
		BlurBrowser();
		WikiBrowser::SetVisible(false, /*keepProcessAlive=*/true);
	}

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		float uvU = 1.f, uvV = 1.f;
		WikiBrowser::FrameUvMax(&uvU, &uvV);

		/* Desktop-ad hosts: letterbox the CEF frame so resize stays uniform
		   (no anamorphic stretch). Other sites still fill the slot 1:1. */
		float drawX = 0.f, drawY = 0.f, drawW = imageSize.x, drawH = imageSize.y;
		const bool desktopAd = HostWantsDesktopAdViewport(WikiBrowser::CurrentUrlCStr());
		if (desktopAd)
		{
			const float fw = static_cast<float>(std::max(1, WikiBrowser::FrameWidth()));
			const float fh = static_cast<float>(std::max(1, WikiBrowser::FrameHeight()));
			FitContentInPanel(imageSize.x, imageSize.y, fw, fh, &drawX, &drawY, &drawW, &drawH);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(cursor, ImVec2(cursor.x + imageSize.x, cursor.y + imageSize.y),
				IM_COL32(11, 10, 16, 255));
		}

		ImGui::SetCursorScreenPos(ImVec2(cursor.x + drawX, cursor.y + drawY));
		ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()),
			ImVec2(drawW, drawH), ImVec2(0.f, 0.f), ImVec2(uvU, uvV));

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##gw2igh_wiki_hit", imageSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		overPage = hovered || active;
		const unsigned mods = CefModsFromImGui(io);
		/* Shared across enter/leave so remainder isn't a different static. */
		static float sWheelAccX = 0.f;
		static float sWheelAccY = 0.f;

		if (overPage)
		{
			/* MediaWiki search / inputs need CEF focus without relying on click
			   alone - keep focus while the pointer is on the page. */
			FocusBrowser();

			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x - drawX;
			const float localY = mouse.y - cursor.y - drawY;
			/* Ignore pointer over letterbox bars. */
			if (localX >= 0.f && localY >= 0.f && localX < drawW && localY < drawH)
			{
				int cx = 0, cy = 0;
				MapToCef(localX, localY, drawW, drawH, &cx, &cy);

				WikiBrowser::FeedMouseMove(cx, cy, false, mods);

				auto click = [&](ImGuiMouseButton btn, int cefBtn) {
					if (ImGui::IsMouseClicked(btn))
					{
						/* Re-assert focus on click so the text caret appears (OSR). */
						FocusBrowserForce();
						WikiBrowser::FeedMouseClick(cx, cy, cefBtn, false,
							ImGui::IsMouseDoubleClicked(btn) ? 2 : 1, mods);
					}
					if (ImGui::IsMouseReleased(btn))
						WikiBrowser::FeedMouseClick(cx, cy, cefBtn, true, 1, mods);
				};
				click(ImGuiMouseButton_Left, 0);
				click(ImGuiMouseButton_Right, 2);
				click(ImGuiMouseButton_Middle, 1);

				if (io.MouseWheel != 0.f || io.MouseWheelH != 0.f)
				{
					/* Accumulate fractional trackpad deltas so smooth scroll isn't
					   quantized away by int(wheel*120). Discrete notches still land as ±120. */
					sWheelAccX += io.MouseWheelH * 120.f;
					sWheelAccY += io.MouseWheel * 120.f;
					const int dx = static_cast<int>(sWheelAccX);
					const int dy = static_cast<int>(sWheelAccY);
					if (dx != 0 || dy != 0)
					{
						sWheelAccX -= static_cast<float>(dx);
						sWheelAccY -= static_cast<float>(dy);
						FocusBrowser();
						WikiBrowser::FeedMouseWheel(cx, cy, dx, dy, mods);
					}
				}
			}
		}
		else
		{
			sWheelAccX = 0.f;
			sWheelAccY = 0.f;
		}
	}
	else
	{
		ImGui::Spacing();
		ImGui::TextColored(kGold, WikiBrowser::IsReady()
			? "Waiting for first paint..."
			: "Loading browser...");
		ImGui::TextWrapped("%s", WikiBrowser::StatusCStr());
		if (WikiBrowser::IsReady())
		{
			const char* why = WikiBrowser::PaintWaitReasonCStr();
			if (why && why[0])
			{
				ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
				ImGui::TextWrapped("%s", why);
				ImGui::PopStyleColor();
			}
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleColor(); /* ChildBg */
	/* Match UI_Render PushStyleVar pair (WindowPadding + ItemSpacing). */
	ImGui::PopStyleVar(2);

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gUi.wikiMin = pos;
	gUi.wikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gUi.wikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	/* Browse / More / tab menus are separate ImGui windows - use sHelperPopupHovered
	   (set while drawing those popups). Do not use AnyWindow: that matched Nexus UI
	   and stole keyboard from the library search field. */
	const bool mouseOverHelperUi = mouseOverWiki || sHelperPopupHovered;

	gUi.overBrowserPage = overPage;
	gUi.blockGameMouse = G::ShowWiki && mouseOverHelperUi;

	/* Snapshot ImGui's own text-input flag BEFORE we adjust capture for CEF.
	   Never force WantTextInput=false - that broke Browse/Find typing and also
	   wiped Nexus library search (shared ImGui context). */
	const bool imguiTyping = io.WantTextInput;

	/* Keys follow the pointer while the helper is open:
	   - over helper chrome/page/popups -> addon (ImGui or CEF)
	   - over the game (chat, world) -> GW2
	   Blur CEF when the cursor leaves so page focus cannot stick. */
	static bool sWasPointerOnHelper = false;
	if (!mouseOverHelperUi)
	{
		BlurBrowser();
		/* Only drop OUR CaptureKeyboardFromApp claim - leave WantTextInput alone
		   so Nexus / other addons keep receiving typed characters. */
		ImGui::CaptureKeyboardFromApp(false);
		if (sWasPointerOnHelper)
			UI_ResetKeyRouting();
		/* Avoid ImGui::SetWindowFocus(nullptr) here - it closed Browse popups. */	}
	else if (!overPage && ImGui::IsAnyItemActive())
		BlurBrowser(); /* Find/filter/etc. active - release CEF caret/focus */
	sWasPointerOnHelper = mouseOverHelperUi;

	gUi.blockGameKeyboard = G::ShowWiki && mouseOverHelperUi;

	if (overPage)
	{
		/* CEF page under the cursor - Nexus also gates on WantTextInput. */
		io.WantTextInput = true;
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}
	else if (mouseOverHelperUi && imguiTyping)
	{
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	if (gUi.blockGameMouse)
	{
		io.WantCaptureMouse = true;
		/* Sticky for next NewFrame - Nexus UiInput gates game clicks on this flag. */
		ImGui::CaptureMouseFromApp(true);
	}
	if (gUi.blockGameKeyboard)
	{
		io.WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	if (std::fabs(pos.x - G::WindowPosX) > 0.5f || std::fabs(pos.y - G::WindowPosY) > 0.5f ||
		std::fabs(winSize.x - G::WindowWidth) > 0.5f || std::fabs(winSize.y - G::WindowHeight) > 0.5f)
	{
		G::WindowPosX = pos.x;
		G::WindowPosY = pos.y;
		G::WindowWidth = winSize.x;
		G::WindowHeight = winSize.y;
		G::HasSavedPos = true;
		G::HasSavedSize = true;
		Settings::SetDirty();
	}

	ImGui::SetWindowFontScale(1.f);
	ImGui::End();
	ImGui::PopStyleVar();
	PopWikiTheme();

	const bool notesHover = NotesPad::Render();
	const bool accountHover = AccountPad::Render();
	const bool tpHover = TpWatchPad::Render();
	const bool lookupHover = LookupPad::Render();
	const bool walletHover = WalletPad::Render();
	const bool vaultHover = VaultPad::Render();
	const bool eventsHover = EventsPad::Render();
	const bool logsHover = LogManagerPad::Render();
	const bool economyHover = EconomyPad::Render();
	const bool instancesHover = InstancesPad::Render();
	const bool completionHover = CompletionPad::Render();
	const bool farmingHover = FarmingPad::Render();
	CompletionPad::Tick();
	const bool gpsArrowHover = GpsArrow::Render();
	ZoneBanner::Render();
	const bool tekkitHover = PathingGuidesPad::Render();
	const bool trailToolsHover = TrailToolsPad::Render();
	const bool compassHover = DirectionCompass::RenderPad();
	const bool settingsHover = SettingsPad::Render();
	CaptureForToolPads(notesHover || accountHover || tpHover || lookupHover ||
		walletHover || vaultHover || eventsHover || logsHover ||
		economyHover || instancesHover || completionHover || farmingHover ||
		gpsArrowHover ||
		tekkitHover || trailToolsHover || compassHover || settingsHover);
	NotesPad::Save(false);
	Settings::Save(false);

	}
} // namespace UIDetail
