#include "UI.h"
#include "UI_Browse.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "LookupPad.h"
#include "WalletPad.h"
#include "VaultPad.h"
#include "AccountPad.h"
#include "EventsPad.h"
#include "LogManagerPad.h"
#include "TekkitGuidesPad.h"
#include "CompassOverlay.h"
#include "WorldOverlay.h"
#include "Settings.h"
#include "Sites.h"
#include "SyncQr.h"
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

#include <windows.h>
#include <shellapi.h>

namespace
{
	float Clampf(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	/* Frame/input state for UI_Render + WndProc helpers. Single instance;
	   Nexus render/input path only — not shared with worker threads. */
	struct UiContext
	{
		bool forceHelperOnScreen = false;
		bool browserFocused = false;
		bool overBrowserPage = false; /* pointer over OSR page — keys belong to CEF */
		bool blockGameKeyboard = false;
		bool blockGameMouse = false;
		bool wikiRectValid = false;
		ImVec2 wikiMin{};
		ImVec2 wikiMax{};
		bool pendingDefocus = false;
	};
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

		const float maxW = Clampf(dw * 0.92f, 320.f, dw);
		const float maxH = Clampf(dh * 0.92f, 240.f, dh);
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

		if (HelperGeomOffscreen(dw, dh) || gUi.forceHelperOnScreen)
		{
			G::WindowPosX = Clampf(dw * 0.08f, 24.f, dw - 120.f);
			G::WindowPosY = Clampf(dh * 0.10f, 24.f, dh - 120.f);
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
		/* Do not clear gUi.overBrowserPage here — the render loop owns hover state.
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

	/* gw2efficiency NitroPay gates slots with matchMedia min-width up to 1840px.
	   OSR innerWidth is the ImGui panel, so a normal helper window never unlocks
	   desktop rails. Only this host needs a desktop-sized CEF view (scaled into
	   the panel). Do not apply to Snow Crows / others — their ads already show. */
	bool HostWantsDesktopAdViewport(const char* url)
	{
		return url && url[0] && std::strstr(url, "gw2efficiency.com") != nullptr;
	}

	void DesktopAdCefSize(float /*panelW*/, float panelH, float* outW, float* outH)
	{
		/* Full HD layout width so (min-width: 1840px) / footer / video queries pass. */
		*outW = static_cast<float>(kWikiFrameMaxW);
		float h = panelH;
		if (h < 900.f)
			h = 900.f;
		if (h > static_cast<float>(kWikiFrameMaxH))
			h = static_cast<float>(kWikiFrameMaxH);
		*outH = h;
	}

	static bool sShowFind = false;
	/* Set while the cursor is over one of our popups (Browse / More / tab menu).
	   Avoids ImGuiHoveredFlags_AnyWindow, which also matched Nexus windows. */
	static bool sHelperPopupHovered = false;

	static bool sFocusFind = false;
	static char sFindQuery[128] = {};
	static bool sFindMatchCase = false;

	bool AnyToolPadOpen()
	{
		return G::ShowNotes || G::ShowAccount || G::ShowTpWatch || G::ShowLookup ||
			G::ShowWallet || G::ShowVault || G::ShowEvents || G::ShowLogManager ||
			G::ShowTekkitGuides;
	}

	/* BeginCombo / ImGui::Combo lists are separate popup windows. Cursor leaves
	   the pad rect → pad Hover is false → we used to stop capturing → GW2 ate
	   the click. Latch while a combo popup is up after our pads opened it. */
	bool HoveringComboPopup()
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* w = g.HoveredWindow;
		if (!w || (w->Flags & ImGuiWindowFlags_Popup) == 0)
			return false;
		return std::strncmp(w->Name, "##Combo", 7) == 0;
	}

	bool ComboPopupOpen()
	{
		ImGuiContext& g = *GImGui;
		for (int i = 0; i < g.OpenPopupStack.Size; ++i)
		{
			ImGuiWindow* w = g.OpenPopupStack[i].Window;
			if (w && std::strncmp(w->Name, "##Combo", 7) == 0)
				return true;
		}
		return false;
	}

	/* BeginCombo lists live outside the pad rect — keep capturing until closed. */
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


	void DrawTabBar()
	{
		BrowserTabs::EnsureDefault();
		const int n = BrowserTabs::Count();
		const int active = BrowserTabs::ActiveIndex();
		int pendingClose = -1;

		/* One widget per tab: "Title  x". Separate title+x buttons were easy to
		   mis-hit (last tab's x clipped / click landed on the previous x). */
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 4.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.f, 4.f));
		ImGui::BeginChild("##gw2igh_tab_bar", ImVec2(0.f, ImGui::GetFrameHeightWithSpacing() + 4.f), false,
			ImGuiWindowFlags_HorizontalScrollbar);

		const float closeZone = ImGui::CalcTextSize("  x").x + ImGui::GetStyle().FramePadding.x;

		for (int i = 0; i < n; ++i)
		{
			ImGui::PushID(i);
			const BrowserTabs::Tab& tab = BrowserTabs::At(i);
			const bool selected = (i == active);
			const bool canClose = (n > 1 && !tab.pinned);

			char label[96];
			const char* title = tab.title[0] ? tab.title : "Tab";
			if (canClose)
				std::snprintf(label, sizeof(label), "%s  x###tab%d", title, i);
			else if (n > 1 && tab.pinned)
				std::snprintf(label, sizeof(label), "%s  ·###tab%d", title, i);
			else
				std::snprintf(label, sizeof(label), "%s###tab%d", title, i);

			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Button, kTabActive);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, kTabIdle);
			const bool pressed = ImGui::Button(label);
			ImGui::PopStyleColor();

			const ImVec2 rmin = ImGui::GetItemRectMin();
			const ImVec2 rmax = ImGui::GetItemRectMax();

			if (tab.pinned)
			{
				ImGui::GetWindowDrawList()->AddCircleFilled(
					ImVec2(rmin.x + 5.f, rmin.y + 5.f),
					2.6f,
					ImGui::GetColorU32(kGold));
			}
			if (selected)
			{
				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(rmin.x, rmax.y - 2.f),
					ImVec2(rmax.x, rmax.y),
					ImGui::GetColorU32(kGold));
			}

			if (pressed)
			{
				const float mx = ImGui::GetIO().MousePos.x;
				if (canClose && mx >= (rmax.x - closeZone))
					pendingClose = i;
				else
					BrowserTabs::Activate(i);
			}

			if (ImGui::BeginPopupContextItem("##gw2igh_tab_ctx"))
			{
				if (ImGui::MenuItem(tab.pinned ? "Unpin" : "Pin"))
					BrowserTabs::TogglePin(i);
				if (ImGui::MenuItem("Close", nullptr, false, canClose))
					pendingClose = i;
				UI_NoteHelperPopupHover();
				ImGui::EndPopup();
			}

			if (ImGui::IsItemHovered())
			{
				if (tab.pinned)
					ImGui::SetTooltip("%s (pinned — unpin to close)", title);
				else if (canClose)
				{
					const float mx = ImGui::GetIO().MousePos.x;
					if (mx >= (rmax.x - closeZone))
						ImGui::SetTooltip("Close tab");
				}
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && canClose)
					pendingClose = i;
			}

			ImGui::SameLine();
			ImGui::PopID();
		}

		const bool canAdd = (n < BrowserTabs::kMaxTabs);
		if (canAdd)
		{
			const bool plusClicked = ImGui::Button("+##gw2igh_new_tab");
			if (plusClicked || UI_Browse_ConsumeNewTabPickerRequest())
				UI_Browse_OnNewTabButtonClicked();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Open site in a new tab (Ctrl+T)");
			UI_Browse_DrawNewTabPopup();
		}
		else
		{
			(void)UI_Browse_ConsumeNewTabPickerRequest();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button("+##gw2igh_new_tab");
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Tab limit reached (8)");
			UI_Browse_DrawNewTabPopup();
		}

		ImGui::SameLine(0.f, 6.f);
		{
			const bool canRe = BrowserTabs::CanReopenClosed();
			if (canRe)
			{
				if (ImGui::SmallButton("^##gw2igh_reopen"))
					BrowserTabs::ReopenClosed();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reopen closed tab (Ctrl+Shift+T)");
			}
		}

		ImGui::EndChild();
		ImGui::PopStyleVar(2);

		if (pendingClose >= 0)
			BrowserTabs::Close(pendingClose);
	}

	void CopyCurrentUrl()
	{
		std::string copy = WikiBrowser::CurrentUrl();
		if (copy.empty() || copy.rfind("about:", 0) == 0 || copy.rfind("file:", 0) == 0)
			copy = Sites::ResolveUrl(Sites::Active());
		if (copy.empty() || copy.rfind("about:", 0) == 0 || copy.rfind("file:", 0) == 0)
			return;
		if (!OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const SIZE_T bytes = copy.size() + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (mem)
		{
			void* locked = GlobalLock(mem);
			if (locked)
			{
				std::memcpy(locked, copy.c_str(), bytes);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
		}
		CloseClipboard();
	}

	void OpenCurrentExternal()
	{
		std::string openUrl = WikiBrowser::CurrentUrl();
		if (openUrl.empty() || openUrl.rfind("about:", 0) == 0 || openUrl.rfind("file:", 0) == 0)
			openUrl = Sites::ResolveUrl(Sites::Active());
		if (!openUrl.empty() &&
			(openUrl.rfind("http://", 0) == 0 || openUrl.rfind("https://", 0) == 0))
		{
			ShellExecuteA(nullptr, "open", openUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	void DuplicateActiveTab()
	{
		if (BrowserTabs::Count() >= BrowserTabs::kMaxTabs)
			return;
		std::string u = WikiBrowser::CurrentUrl();
		if (u.empty() || u.rfind("about:", 0) == 0 || u.rfind("file:", 0) == 0)
			u = Sites::ResolveUrl(Sites::Active());
		BrowserTabs::OpenNewUrl(Sites::ActiveId(), u);
	}

	/* Friendly status — muted gold; hide Ready / closed noise. */
	void DrawStatusChip()
	{
		const char* raw = WikiBrowser::StatusCStr();
		if (!raw || !raw[0] || std::strcmp(raw, "Ready") == 0)
			return;

		const char* label = nullptr;
		ImVec4 col = kGoldMuted;
		if (std::strstr(raw, "Loading") ||
			std::strstr(raw, "Navigating") ||
			std::strstr(raw, "Launching") ||
			std::strstr(raw, "Creating"))
		{
			label = "Loading...";
			col = kGold;
		}
		else if (std::strstr(raw, "Closed") ||
			std::strstr(raw, "Hidden"))
		{
			return;
		}
		else if (std::strstr(raw, "fail") ||
			std::strstr(raw, "Fail") ||
			std::strstr(raw, "error") ||
			std::strstr(raw, "Error") ||
			std::strstr(raw, "not found") ||
			std::strstr(raw, "disabled"))
		{
			label = "Error - check Nexus log";
			col = kWarn;
		}
		else
		{
			/* Truncate long technical strings. */
			static char buf[48];
			const size_t n = std::strlen(raw);
			if (n > 40)
			{
				std::snprintf(buf, sizeof(buf), "%.37s...", raw);
				label = buf;
			}
			else
				label = raw;
			col = kGoldDim;
		}

		ImGui::SameLine();
		ImGui::TextColored(col, "%s", label);
	}

	void DrawMoreMenu()
	{
		if (ImGui::Button("...##gw2igh_more"))
			ImGui::OpenPopup("##gw2igh_toolbar_more");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("More actions");

		if (ImGui::BeginPopup("##gw2igh_toolbar_more"))
		{
			if (ImGui::MenuItem(sShowFind ? "Hide find" : "Find in page", "Ctrl+F"))
				sShowFind = !sShowFind;
			if (ImGui::MenuItem("Copy URL"))
				CopyCurrentUrl();
			if (ImGui::MenuItem("Open externally"))
				OpenCurrentExternal();
			ImGui::Separator();
			{
				const bool canNew = BrowserTabs::Count() < BrowserTabs::kMaxTabs;
				if (ImGui::MenuItem("New tab (duplicate)", nullptr, false, canNew))
					DuplicateActiveTab();
			}
			{
				const int ai = BrowserTabs::ActiveIndex();
				const bool pinned = BrowserTabs::At(ai).pinned;
				if (ImGui::MenuItem(pinned ? "Unpin tab" : "Pin tab"))
					BrowserTabs::TogglePin(ai);
			}
			{
				const bool canRe = BrowserTabs::CanReopenClosed();
				if (ImGui::MenuItem("Reopen closed tab", "Ctrl+Shift+T", false, canRe))
					BrowserTabs::ReopenClosed();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(G::ShowEvents ? "Hide Events panel" : "Show Events panel"))
			{
				if (G::ShowEvents)
				{
					G::ShowEvents = false;
					Settings::SetDirty();
				}
				else
					EventsPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowLogManager ? "Hide DPS Logs" : "Show DPS Logs"))
			{
				if (G::ShowLogManager)
				{
					G::ShowLogManager = false;
					Settings::SetDirty();
				}
				else
					LogManagerPad::OpenAndRefresh();
			}
			if (ImGui::MenuItem(G::ShowTekkitGuides ? "Hide Tekkit's Guides" : "Show Tekkit's Guides"))
			{
				if (G::ShowTekkitGuides)
				{
					G::ShowTekkitGuides = false;
					Settings::SetDirty();
				}
				else
					TekkitGuidesPad::Open();
			}
			UI_NoteHelperPopupHover();
			ImGui::EndPopup();
		}
	}

	void DrawToolbar()
	{
		const SiteDef& active = Sites::Active();

		/* Compact nav cluster */
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, 6.f));
		if (SoftButton("<###gw2igh_back", BrowserTabs::CanGoBack()))
			BrowserTabs::GoBack();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Back");
		ImGui::SameLine();
		if (SoftButton(">###gw2igh_fwd", BrowserTabs::CanGoForward()))
			BrowserTabs::GoForward();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Forward");
		ImGui::SameLine();
		if (ImGui::Button("Home###gw2igh_home"))
			BrowserTabs::GoHome();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Default landing site");
		ImGui::SameLine();
		if (ImGui::Button("Reload###gw2igh_reload"))
			BrowserTabs::Reload();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Reload");
		ImGui::PopStyleVar();

		ImGui::SameLine(0.f, 12.f);
		{
			float avail = ImGui::GetContentRegionAvail().x - 120.f;
			if (avail < 140.f) avail = 140.f;
			if (avail > 420.f) avail = 420.f;
			ImGui::SetNextItemWidth(avail);
		}
		if (ImGui::InputTextWithHint("###gw2igh_site_query", "Find in page...", G::LastQuery, sizeof(G::LastQuery),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (G::LastQuery[0])
			{
				/* Enter = find on the current page (not Google). */
				sShowFind = true;
				std::snprintf(sFindQuery, sizeof(sFindQuery), "%s", G::LastQuery);
				WikiBrowser::Find(sFindQuery, true, sFindMatchCase, false);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Find text on this page. Enter = next match.\nUse Web for DuckDuckGo / site search.");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Web###gw2igh_web"))
		{
			if (G::LastQuery[0])
			{
				WikiBrowser::Search(G::LastQuery);
				Settings::SetDirty();
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Search the active site (or DuckDuckGo).");

		ImGui::SameLine(0.f, 8.f);
		DrawMoreMenu();
		DrawStatusChip();

		/* Browse + pads row — Browse stays left of Account. */
		if (ImGui::Button("Browse###gw2igh_browse"))
			UI_Browse_OnMainButtonClicked();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s - %s",
				active.category ? active.category : "",
				active.label ? active.label : "");
		UI_Browse_DrawMainPopup();
		ImGui::SameLine(0.f, 4.f);
		UI_Browse_ToolbarFavoriteToggle();
		ImGui::SameLine(0.f, 8.f);
		if (ImGui::Button("Account###gw2igh_account"))
		{
			if (G::ShowAccount)
			{
				G::ShowAccount = false;
				Settings::SetDirty();
			}
			else
				AccountPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Account — stash, vault, TP, item lookup (tabbed)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Tekkit's Guides###gw2igh_tekkit"))
		{
			if (G::ShowTekkitGuides)
			{
				G::ShowTekkitGuides = false;
				Settings::SetDirty();
			}
			else
				TekkitGuidesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Tekkit's Guides — compass trails + category toggles (© Tekkit)");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Events###gw2igh_events"))
		{
			if (G::ShowEvents)
			{
				G::ShowEvents = false;
				Settings::SetDirty();
			}
			else
				EventsPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("World events — UTC timers + track list");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("DPS Logs###gw2igh_logs"))
		{
			if (G::ShowLogManager)
			{
				G::ShowLogManager = false;
				Settings::SetDirty();
			}
			else
				LogManagerPad::OpenAndRefresh();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("DPS Logs — browse ArcDPS EVTC via Elite Insights");
		ImGui::SameLine(0.f, 4.f);
		if (ImGui::Button("Notes & Waypoints###gw2igh_notes"))
		{
			if (G::ShowNotes)
			{
				G::ShowNotes = false;
				Settings::SetDirty();
			}
			else
				NotesPad::Open();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Snippets + Waypoints search (copy chat codes)");

	}

}

void UI_NoteHelperPopupHover()
{
	if (ImGui::IsWindowHovered(
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_ChildWindows))
		sHelperPopupHovered = true;
}

bool UI_BlocksGameKeyboard()
{
	return gUi.blockGameKeyboard;
}

/* True while keystrokes should go to the CEF page (not ImGui filter/search/find). */
bool UI_BrowserKeyboardActive()
{
	/* Pointer must be on the OSR page — sticky gUi.browserFocused alone stole
	   Browse/Find typing and game chat after leaving the page. */
	return G::ShowWiki && gUi.overBrowserPage;
}


bool UI_BlocksGameMouse()
{
	return gUi.blockGameMouse;
}

bool UI_IsPointerOverWiki(int clientX, int clientY)
{
	if (!G::ShowWiki || !gUi.wikiRectValid)
		return false;
	const float x = static_cast<float>(clientX);
	const float y = static_cast<float>(clientY);
	return x >= gUi.wikiMin.x && y >= gUi.wikiMin.y && x < gUi.wikiMax.x && y < gUi.wikiMax.y;
}

void UI_ReleaseGameInput()
{
	BlurBrowser();
	gUi.blockGameMouse = false;
	gUi.blockGameKeyboard = false;
	gUi.pendingDefocus = true;
	UI_ResetKeyRouting();
}

void UI_Render()
{
	/* Always poll first — must run while the helper is closed too. */
	HelperHotkeys_Poll();
	WikiBrowser::Tick();
	/* Tekkit overlays — always, even with the browser closed. */
	CompassOverlay::Render();
	WorldOverlay::Render();
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
		/* Do NOT clear WantTextInput / WantCaptureKeyboard here — those flags are
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
		   Only block GW2 while the pointer is over those windows — do not
		   force Capture*FromApp(false) every frame (breaks Nexus / open). */
		const bool notesHover = NotesPad::Render();
		const bool accountHover = AccountPad::Render();
		const bool tpHover = TpWatchPad::Render();
		const bool lookupHover = LookupPad::Render();
		const bool walletHover = WalletPad::Render();
		const bool vaultHover = VaultPad::Render();
		const bool eventsHover = EventsPad::Render();
		const bool logsHover = LogManagerPad::Render();
		const bool tekkitHover = TekkitGuidesPad::Render();
		CaptureForToolPads(notesHover || accountHover || tpHover || lookupHover ||
			walletHover || vaultHover || eventsHover || logsHover || tekkitHover);
		NotesPad::Save(false);
		Settings::Save(false);
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
			/* First open: ~30% of the display (clamped so it stays usable). */
			G::WindowWidth = Clampf(dio.DisplaySize.x * 0.30f, 640.f, 1600.f);
			G::WindowHeight = Clampf(dio.DisplaySize.y * 0.30f, 420.f, 1000.f);
			G::HasSavedSize = true; /* apply once — ImGui FirstUseEver + persist */
			Settings::SetDirty();
		}
	}
	ClampHelperGeomToDisplay();
	const ImGuiCond geomCond = gUi.forceHelperOnScreen ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), geomCond);
	ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), geomCond);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gUi.forceHelperOnScreen)
		ImGui::SetNextWindowFocus();
	gUi.forceHelperOnScreen = false;

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(G::Opacity);

	bool open = G::ShowWiki;
	if (!ImGui::Begin("In-Game Helper##GW2InGameHelper", &open,
		ImGuiWindowFlags_NoNavInputs))
	{
		/* Collapsed title bar — CEF must be was_hidden (0% viewability otherwise)
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
		ImGui::End();
		ImGui::PopStyleVar();
		PopWikiTheme();
		/* Still draw pads while the main window is collapsed. */
		const bool notesHover = NotesPad::Render();
		const bool accountHover = AccountPad::Render();
		const bool tpHover = TpWatchPad::Render();
		const bool lookupHover = LookupPad::Render();
		const bool walletHover = WalletPad::Render();
		const bool vaultHover = VaultPad::Render();
		const bool eventsHover = EventsPad::Render();
		const bool logsHover = LogManagerPad::Render();
		const bool tekkitHover = TekkitGuidesPad::Render();
		CaptureForToolPads(notesHover || accountHover || tpHover || lookupHover ||
			walletHover || vaultHover || eventsHover || logsHover || tekkitHover);
		NotesPad::Save(false);
		Settings::Save(false);
		return;
	}
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		UI_ReleaseGameInput();
	}

	ImGui::SetWindowFontScale(G::FontScale);

	BrowserTabs::EnsureDefault();
	BrowserTabs::Tick();

	ImGui::TextColored(kGold, "IN-GAME HELPER");
	ImGui::SameLine(0.f, 12.f);
	DrawToolbar();

	/* Tab / find hotkeys — use ImGuiIO (Nexus-filled KeysDown), not GetAsyncKeyState. */
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

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("##gw2igh_wiki_osr_slot", avail, false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	const ImVec2 slotPos = ImGui::GetCursorScreenPos();
	const ImGuiIO& dio = ImGui::GetIO();
	const float dispW = dio.DisplaySize.x;
	const float dispH = dio.DisplaySize.y;
	/* Any overlap with the game display — fully off-screen is not viewable. */
	const bool slotOnScreen =
		dispW < 1.f || dispH < 1.f ||
		(slotPos.x + imageSize.x > 0.f && slotPos.y + imageSize.y > 0.f &&
			slotPos.x < dispW && slotPos.y < dispH);
	/* Hysteresis on size so drag-resize does not flap was_hidden (that can
	   interrupt ad / page timers). Opacity is NOT a gate — users still read
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
		/* Keep process — window may still be open (tiny / off-screen). */
		BlurBrowser();
		WikiBrowser::SetVisible(false, /*keepProcessAlive=*/true);
	}

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		{
			float uvU = 1.f, uvV = 1.f;
			WikiBrowser::FrameUvMax(&uvU, &uvV);
			ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()), imageSize,
				ImVec2(0.f, 0.f), ImVec2(uvU, uvV));
		}

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
			   alone — keep focus while the pointer is on the page. */
			FocusBrowser();

			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x;
			const float localY = mouse.y - cursor.y;
			int cx = 0, cy = 0;
			MapToCef(localX, localY, imageSize.x, imageSize.y, &cx, &cy);

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

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gUi.wikiMin = pos;
	gUi.wikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gUi.wikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	/* Browse / More / tab menus are separate ImGui windows — use sHelperPopupHovered
	   (set while drawing those popups). Do not use AnyWindow: that matched Nexus UI
	   and stole keyboard from the library search field. */
	const bool mouseOverHelperUi = mouseOverWiki || sHelperPopupHovered;

	gUi.overBrowserPage = overPage;
	gUi.blockGameMouse = G::ShowWiki && mouseOverHelperUi;

	/* Snapshot ImGui's own text-input flag BEFORE we adjust capture for CEF.
	   Never force WantTextInput=false — that broke Browse/Find typing and also
	   wiped Nexus library search (shared ImGui context). */
	const bool imguiTyping = io.WantTextInput;

	/* Keys follow the pointer while the helper is open:
	   - over helper chrome/page/popups → addon (ImGui or CEF)
	   - over the game (chat, world) → GW2
	   Blur CEF when the cursor leaves so page focus cannot stick. */
	static bool sWasPointerOnHelper = false;
	if (!mouseOverHelperUi)
	{
		BlurBrowser();
		/* Only drop OUR CaptureKeyboardFromApp claim — leave WantTextInput alone
		   so Nexus / other addons keep receiving typed characters. */
		ImGui::CaptureKeyboardFromApp(false);
		if (sWasPointerOnHelper)
			UI_ResetKeyRouting();
		/* Avoid ImGui::SetWindowFocus(nullptr) here — it closed Browse popups. */	}
	else if (!overPage && ImGui::IsAnyItemActive())
		BlurBrowser(); /* Find/filter/etc. active — release CEF caret/focus */
	sWasPointerOnHelper = mouseOverHelperUi;

	gUi.blockGameKeyboard = G::ShowWiki && mouseOverHelperUi;

	if (overPage)
	{
		/* CEF page under the cursor — Nexus also gates on WantTextInput. */
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
		/* Sticky for next NewFrame — Nexus UiInput gates game clicks on this flag. */
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
	const bool tekkitHover = TekkitGuidesPad::Render();
	CaptureForToolPads(notesHover || accountHover || tpHover || lookupHover ||
		walletHover || vaultHover || eventsHover || logsHover || tekkitHover);
	NotesPad::Save(false);
	Settings::Save(false);
}

