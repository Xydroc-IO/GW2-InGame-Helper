#include "UI.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <shellapi.h>

namespace
{
	bool gBrowserFocused = false;
	bool gBlockGameKeyboard = false;
	bool gBlockGameMouse = false;
	bool gWikiRectValid = false;
	ImVec2 gWikiMin{};
	ImVec2 gWikiMax{};
	bool gPendingDefocus = false;

	const ImVec4 kGold(0.941f, 0.776f, 0.353f, 1.f);       /* #f0c65a */
	const ImVec4 kGoldBright(1.f, 0.878f, 0.541f, 1.f);     /* #ffe08a */
	const ImVec4 kMuted(0.659f, 0.682f, 0.722f, 1.f);       /* #a8aeb8 */
	const ImVec4 kBg(0.024f, 0.027f, 0.039f, 1.f);          /* #06070a */
	const ImVec4 kPanel(0.071f, 0.078f, 0.102f, 1.f);       /* #12141a */
	const ImVec4 kBorder(0.353f, 0.290f, 0.157f, 0.95f);    /* #5a4a28 */

	void PushWikiTheme()
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.941f, 0.949f, 0.961f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TextDisabled, kMuted);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, kBg);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.047f, 0.051f, 0.067f, 0.92f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.059f, 0.078f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.078f, 0.086f, 0.110f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.12f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.16f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.055f, 0.047f, 0.031f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.12f, 0.10f, 0.055f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.04f, 0.04f, 0.05f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, kPanel);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.04f, 0.04f, 0.05f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.35f, 0.28f, 0.14f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.55f, 0.42f, 0.18f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, kGold);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, kGold);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.75f, 0.58f, 0.22f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.11f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.22f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.40f, 0.30f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.18f, 0.09f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.28f, 0.12f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.35f, 0.14f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.36f, 0.16f, 0.55f));
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, kGold);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImVec4(0.45f, 0.36f, 0.16f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, kGold);
		ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, kGoldBright);
		ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.10f, 0.09f, 0.07f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.28f, 0.22f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.32f, 0.26f, 0.12f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0.08f, 0.08f, 0.09f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.18f, 0.15f, 0.08f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.55f, 0.42f, 0.15f, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, kGold);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 6.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.f);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 2.f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
	}

	void PopWikiTheme()
	{
		ImGui::PopStyleVar(13);
		ImGui::PopStyleColor(40);
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
		if (!gBrowserFocused)
			return;
		gBrowserFocused = false;
		gBlockGameKeyboard = false;
		WikiBrowser::FeedFocus(false);
	}

	void FocusBrowser()
	{
		if (gBrowserFocused)
			return;
		gBrowserFocused = true;
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

	static char sFilter[64] = {};
	static int sCategoryIndex = 0;
	static bool sSyncCategory = true;

	void ActivateSiteIndex(int index, bool navigate, bool newTab)
	{
		if (index < 0)
			return;
		size_t siteCount = 0;
		const SiteDef* sites = Sites::All(&siteCount);
		if (!sites || index >= static_cast<int>(siteCount) || !sites[index].id)
			return;

		if (newTab)
		{
			if (BrowserTabs::OpenNew(sites[index].id, navigate) < 0 && navigate)
				BrowserTabs::OpenInActive(sites[index].id, navigate);
		}
		else
			BrowserTabs::OpenInActive(sites[index].id, navigate);
	}

	/* Draw a 5-point star (ProggyClean has no ★/☆ glyphs). */
	void DrawStarShape(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, bool filled)
	{
		ImVec2 pts[10];
		for (int i = 0; i < 10; ++i)
		{
			const float a = -3.14159265f * 0.5f + static_cast<float>(i) * 3.14159265f / 5.f;
			const float r = (i & 1) ? radius * 0.42f : radius;
			pts[i] = ImVec2(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
		}
		if (filled)
			dl->AddConvexPolyFilled(pts, 10, col);
		else
			dl->AddPolyline(pts, 10, col, true, 1.6f);
	}

	bool FavoriteToggleButton(const char* id, bool favorited, bool smallBtn)
	{
		ImGui::PushID(id);
		const float h = smallBtn ? ImGui::GetFrameHeight() * 0.85f : ImGui::GetFrameHeight();
		const ImVec2 size(h, h);
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const bool pressed = ImGui::InvisibleButton("##star", size);
		const ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
		const float radius = size.x * 0.32f;
		const bool hovered = ImGui::IsItemHovered();
		ImU32 col;
		if (favorited)
			col = ImGui::GetColorU32(hovered ? ImVec4(1.f, 0.85f, 0.35f, 1.f) : kGold);
		else
			col = ImGui::GetColorU32(hovered ? ImVec4(0.85f, 0.88f, 0.92f, 1.f) : kMuted);
		DrawStarShape(dl, center, radius, col, favorited);
		if (hovered)
			ImGui::SetTooltip(favorited ? "Remove from Favorites" : "Add to Favorites");
		ImGui::PopID();
		return pressed;
	}

	void DrawFavoriteStar(const char* siteId)
	{
		if (!siteId || !siteId[0])
			return;
		const bool fav = Sites::IsFavorite(siteId);
		if (FavoriteToggleButton("row", fav, true))
			Sites::ToggleFavorite(siteId);
	}

	void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel)
	{
		size_t siteCount = 0;
		const SiteDef* sites = Sites::All(&siteCount);
		size_t catCount = 0;
		const char* const* cats = Sites::Categories(&catCount);
		if (!sites || siteCount == 0 || !cats || catCount == 0)
			return;

		/* Index 0 = virtual Favorites; 1..catCount = registry categories. */
		const int totalCats = static_cast<int>(catCount) + 1;

		if (sSyncCategory)
		{
			sSyncCategory = false;
			if (Sites::IsFavorite(Sites::ActiveId()))
				sCategoryIndex = 0;
			else
			{
				const char* activeCat = Sites::Active().category ? Sites::Active().category : "";
				sCategoryIndex = 1;
				for (int i = 0; i < static_cast<int>(catCount); ++i)
				{
					if (std::strcmp(cats[i] ? cats[i] : "", activeCat) == 0)
					{
						sCategoryIndex = i + 1;
						break;
					}
				}
			}
		}
		if (sCategoryIndex < 0 || sCategoryIndex >= totalCats)
			sCategoryIndex = 0;

		ImGui::TextColored(kGold, "Search");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("##site_filter", "Filter sites…", sFilter, sizeof(sFilter));

		const bool filtering = sFilter[0] != '\0';
		const bool showFavorites = (!filtering && sCategoryIndex == 0);
		const char* selectedCat = "";
		if (!filtering && !showFavorites && sCategoryIndex >= 1)
			selectedCat = cats[sCategoryIndex - 1] ? cats[sCategoryIndex - 1] : "";

		const float listH = 240.f;
		const float leftW = 140.f;

		ImGui::BeginChild("##browse_cats", ImVec2(leftW, listH), true);
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
		ImGui::TextUnformatted("Categories");
		ImGui::PopStyleColor();
		ImGui::Separator();

		{
			char favLabel[64];
			std::snprintf(favLabel, sizeof(favLabel), "Favorites (%d)", Sites::FavoriteCount());
			if (ImGui::Selectable(favLabel, sCategoryIndex == 0))
			{
				sCategoryIndex = 0;
				sFilter[0] = '\0';
			}
		}
		for (int i = 0; i < static_cast<int>(catCount); ++i)
		{
			const char* cat = cats[i] ? cats[i] : "";
			const int uiIndex = i + 1;
			const bool selected = (uiIndex == sCategoryIndex);
			char label[96];
			std::snprintf(label, sizeof(label), "%s (%d)", cat, Sites::CountInCategory(cat));
			if (ImGui::Selectable(label, selected))
			{
				sCategoryIndex = uiIndex;
				sFilter[0] = '\0';
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("##browse_sites", ImVec2(0.f, listH), true);
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
		if (filtering)
			ImGui::TextUnformatted("Matching sites");
		else if (showFavorites)
			ImGui::TextUnformatted("Favorites");
		else
			ImGui::TextUnformatted(selectedCat);
		ImGui::PopStyleColor();
		ImGui::Separator();

		const int current = Sites::ActiveIndex();
		int shown = 0;

		auto DrawSiteRow = [&](int siteIndex, bool withCategoryPrefix) {
			if (siteIndex < 0 || siteIndex >= static_cast<int>(siteCount))
				return;
			const SiteDef& site = sites[siteIndex];
			ImGui::PushID(siteIndex);
			DrawFavoriteStar(site.id);
			ImGui::SameLine();
			char row[160];
			if (withCategoryPrefix)
				std::snprintf(row, sizeof(row), "%s · %s",
					site.category ? site.category : "",
					site.label ? site.label : "");
			else
				std::snprintf(row, sizeof(row), "%s", site.label ? site.label : "");

			const bool selected = (siteIndex == current);
			const bool ctrl = ImGui::GetIO().KeyCtrl;
			if (ImGui::Selectable(row, selected))
			{
				ActivateSiteIndex(siteIndex, navigateOnChange, ctrl);
				if (closePanel)
					*closePanel = true;
				sSyncCategory = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Click: this tab · Ctrl+click: new tab");
			if (selected)
				ImGui::SetItemDefaultFocus();
			ImGui::PopID();
			++shown;
		};

		if (showFavorites)
		{
			const int favN = Sites::FavoriteCount();
			for (int f = 0; f < favN; ++f)
				DrawSiteRow(Sites::FavoriteSiteIndex(f), true);
		}
		else
		{
			for (int i = 0; i < static_cast<int>(siteCount); ++i)
			{
				const SiteDef& site = sites[i];
				if (filtering)
				{
					if (!Sites::MatchesFilter(site, sFilter))
						continue;
					DrawSiteRow(i, true);
				}
				else
				{
					const char* cat = site.category ? site.category : "";
					if (std::strcmp(cat, selectedCat) != 0)
						continue;
					DrawSiteRow(i, false);
				}
			}
		}

		if (shown == 0)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
			if (filtering)
				ImGui::TextUnformatted("No matches.");
			else if (showFavorites)
				ImGui::TextUnformatted("No favorites yet. Click the star next to a site.");
			else
				ImGui::TextUnformatted("No sites in this category.");
			ImGui::PopStyleColor();
		}
		ImGui::EndChild();
	}

	void DrawTabBar()
	{
		BrowserTabs::EnsureDefault();
		const int n = BrowserTabs::Count();
		const int active = BrowserTabs::ActiveIndex();
		int pendingClose = -1;

		ImGui::BeginChild("##tab_bar", ImVec2(0.f, ImGui::GetFrameHeightWithSpacing() + 4.f), false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		for (int i = 0; i < n; ++i)
		{
			ImGui::PushID(i);
			const BrowserTabs::Tab& tab = BrowserTabs::At(i);
			const bool selected = (i == active);

			char label[64];
			std::snprintf(label, sizeof(label), "%s", tab.title[0] ? tab.title : "Tab");

			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.30f, 0.12f, 1.f));
			if (ImGui::Button(label))
				BrowserTabs::Activate(i);
			if (selected)
				ImGui::PopStyleColor();
			if (selected)
			{
				const ImVec2 rmin = ImGui::GetItemRectMin();
				const ImVec2 rmax = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(rmin.x, rmax.y - 2.f),
					ImVec2(rmax.x, rmax.y),
					ImGui::GetColorU32(kGold));
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && n > 1)
				pendingClose = i;

			if (n > 1)
			{
				ImGui::SameLine(0.f, 2.f);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, ImGui::GetStyle().FramePadding.y));
				if (ImGui::SmallButton("x"))
					pendingClose = i;
				ImGui::PopStyleVar();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Close tab");
			}

			ImGui::SameLine();
			ImGui::PopID();
		}

		const bool canAdd = (n < BrowserTabs::kMaxTabs);
		if (canAdd)
		{
			if (ImGui::Button("+##new_tab"))
				ImGui::OpenPopup("##site_browse_newtab");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Open site in a new tab");
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button("+##new_tab");
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Tab limit reached (8)");
		}

		ImGui::SetNextWindowSize(ImVec2(520.f, 320.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##site_browse_newtab"))
		{
			bool closePanel = false;
			size_t siteCount = 0;
			const SiteDef* sites = Sites::All(&siteCount);
			ImGui::TextColored(kGold, "Open in new tab");
			ImGui::Separator();
			ImGui::BeginChild("##newtab_sites", ImVec2(0.f, 260.f), true);
			if (sites)
			{
				for (int i = 0; i < static_cast<int>(siteCount); ++i)
				{
					const SiteDef& site = sites[i];
					char row[160];
					std::snprintf(row, sizeof(row), "%s · %s",
						site.category ? site.category : "",
						site.label ? site.label : "");
					if (ImGui::Selectable(row))
					{
						BrowserTabs::OpenNew(site.id, true);
						closePanel = true;
					}
				}
			}
			ImGui::EndChild();
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::EndChild();

		if (pendingClose >= 0)
			BrowserTabs::Close(pendingClose);
	}

	void DrawSitePicker()
	{
		const SiteDef& active = Sites::Active();
		char preview[96];
		std::snprintf(preview, sizeof(preview), "%s · %s",
			active.category ? active.category : "",
			active.label ? active.label : "");

		if (ImGui::Button("Browse"))
		{
			sSyncCategory = true;
			ImGui::OpenPopup("##site_browse");
		}

		ImGui::SameLine();
		{
			const bool fav = Sites::IsFavorite(Sites::ActiveId());
			if (FavoriteToggleButton("toolbar", fav, false))
				Sites::ToggleFavorite(Sites::ActiveId());
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
		ImGui::TextUnformatted(preview);
		ImGui::PopStyleColor();

		ImGui::SetNextWindowSize(ImVec2(520.f, 320.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##site_browse"))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(true, &closePanel);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}

	void DrawDefaultSiteBrowse()
	{
		if (ImGui::Button("Choose default site…"))
		{
			sSyncCategory = true;
			ImGui::OpenPopup("##default_site_browse");
		}

		ImGui::SameLine();
		const SiteDef& active = Sites::Active();
		ImGui::TextColored(kMuted, "%s · %s",
			active.category ? active.category : "",
			active.label ? active.label : "");

		ImGui::SetNextWindowSize(ImVec2(520.f, 320.f), ImGuiCond_Always);
		if (ImGui::BeginPopup("##default_site_browse"))
		{
			bool closePanel = false;
			DrawBrowsePanelContents(G::ShowWiki, &closePanel);
			if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
}

bool UI_BlocksGameKeyboard()
{
	return gBlockGameKeyboard;
}

bool UI_BlocksGameMouse()
{
	return gBlockGameMouse;
}

bool UI_IsPointerOverWiki(int clientX, int clientY)
{
	if (!G::ShowWiki || !gWikiRectValid)
		return false;
	const float x = static_cast<float>(clientX);
	const float y = static_cast<float>(clientY);
	return x >= gWikiMin.x && y >= gWikiMin.y && x < gWikiMax.x && y < gWikiMax.y;
}

void UI_ReleaseGameInput()
{
	BlurBrowser();
	gBlockGameMouse = false;
	gPendingDefocus = true;
}

void UI_Render()
{
	/* Always poll first — must run while the helper is closed too. */
	HelperHotkeys_Poll();

	gBlockGameKeyboard = false;
	gBlockGameMouse = false;
	gWikiRectValid = false;

	if (gPendingDefocus)
	{
		gPendingDefocus = false;
		ImGui::SetWindowFocus(nullptr);
	}

	if (!G::ShowWiki)
	{
		BlurBrowser();
		WikiBrowser::SetVisible(false);
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), ImGuiCond_FirstUseEver);
	if (G::HasSavedPos)
		ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), ImGuiCond_FirstUseEver);

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(G::Opacity);

	bool open = G::ShowWiki;
	if (!ImGui::Begin("In-Game Helper##GW2InGameHelper", &open))
	{
		WikiBrowser::SetVisible(false);
		ImGui::End();
		ImGui::PopStyleVar();
		PopWikiTheme();
		return;
	}
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		BlurBrowser();
	}

	ImGui::SetWindowFontScale(G::FontScale);

	BrowserTabs::EnsureDefault();
	BrowserTabs::Tick();

	ImGui::TextColored(kGold, "IN-GAME HELPER");
	ImGui::SameLine();
	DrawSitePicker();
	ImGui::SameLine();
	if (SoftButton("<", BrowserTabs::CanGoBack()))
		BrowserTabs::GoBack();
	ImGui::SameLine();
	if (SoftButton(">", BrowserTabs::CanGoForward()))
		BrowserTabs::GoForward();
	ImGui::SameLine();
	if (ImGui::Button("Home"))
		BrowserTabs::GoHome();
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
		BrowserTabs::Reload();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(150.f);
	if (ImGui::InputTextWithHint("##site_query", "Search…", G::LastQuery, sizeof(G::LastQuery),
		ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if (G::LastQuery[0])
		{
			WikiBrowser::Search(G::LastQuery);
			Settings::SetDirty();
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Search this site (Wiki / Google) or Google for others. Press Enter.");
	ImGui::SameLine();
	if (ImGui::Button("Go"))
	{
		if (G::LastQuery[0])
		{
			WikiBrowser::Search(G::LastQuery);
			Settings::SetDirty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Copy URL"))
	{
		std::string copy = WikiBrowser::CurrentUrl();
		if (copy.empty() || copy.rfind("about:", 0) == 0 || copy.rfind("file:", 0) == 0)
			copy = Sites::ResolveUrl(Sites::Active());
		if (!copy.empty() && copy.rfind("about:", 0) != 0 && copy.rfind("file:", 0) != 0)
		{
			if (OpenClipboard(nullptr))
			{
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
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Open Ext"))
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
	ImGui::SameLine();
	ImGui::TextColored(kMuted, "%s", WikiBrowser::Status().c_str());

	DrawTabBar();

	const std::string url = WikiBrowser::CurrentUrl();
	if (!url.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
		ImGui::TextUnformatted(url.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Separator();

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("##wiki_osr_slot", avail, false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	if (G::ShowWiki && imageSize.x > 40.f && imageSize.y > 40.f)
	{
		WikiBrowser::SetVisible(true);
		WikiBrowser::SetBounds(0.f, 0.f, imageSize.x, imageSize.y);
		WikiBrowser::PresentFrame();
	}
	else
		WikiBrowser::SetVisible(false);

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()), imageSize);

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##wiki_hit", imageSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		overPage = hovered || active;
		const unsigned mods = CefModsFromImGui(io);

		if (overPage)
		{
			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x;
			const float localY = mouse.y - cursor.y;
			int cx = 0, cy = 0;
			MapToCef(localX, localY, imageSize.x, imageSize.y, &cx, &cy);

			WikiBrowser::FeedMouseMove(cx, cy, false, mods);

			auto click = [&](ImGuiMouseButton btn, int cefBtn) {
				if (ImGui::IsMouseClicked(btn))
				{
					FocusBrowser();
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
				WikiBrowser::FeedMouseWheel(cx, cy,
					static_cast<int>(io.MouseWheelH * 120.f),
					static_cast<int>(io.MouseWheel * 120.f),
					mods);
			}
		}
	}
	else
	{
		ImGui::Spacing();
		ImGui::TextColored(kGold, WikiBrowser::IsReady()
			? "Waiting for first paint…"
			: "Loading browser…");
		ImGui::TextWrapped("%s", WikiBrowser::Status().c_str());
	}

	ImGui::EndChild();

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gWikiMin = pos;
	gWikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gWikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	gBlockGameMouse = G::ShowWiki && (overPage || mouseOverWiki);
	gBlockGameKeyboard = G::ShowWiki && gBrowserFocused;

	if (gBlockGameMouse)
		io.WantCaptureMouse = true;
	if (gBlockGameKeyboard)
		io.WantCaptureKeyboard = true;

	if (pos.x != G::WindowPosX || pos.y != G::WindowPosY ||
		winSize.x != G::WindowWidth || winSize.y != G::WindowHeight)
	{
		G::WindowPosX = pos.x;
		G::WindowPosY = pos.y;
		G::WindowWidth = winSize.x;
		G::WindowHeight = winSize.y;
		G::HasSavedPos = true;
		Settings::SetDirty();
	}

	ImGui::SetWindowFontScale(1.f);
	ImGui::End();
	ImGui::PopStyleVar();
	PopWikiTheme();
	Settings::Save();
}

void UI_Options()
{
	ImGui::TextUnformatted("GW2 In-Game Helper");
	ImGui::Separator();
	if (ImGui::Checkbox("Show helper window", &G::ShowWiki))
		Settings::SetDirty();

	size_t count = 0;
	Sites::All(&count);
	if (count > 0)
		DrawDefaultSiteBrowse();

	if (ImGui::SliderFloat("Opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale", &G::FontScale, 0.75f, 2.f, "%.2f"))
		Settings::SetDirty();

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Use Browse to pick a site. Ctrl+click or + opens a new tab (up to 8). "
		"The Search box queries Wiki/Google (falls back to Google). Star sites for Favorites. "
		"Click outside the window to move and use skills again.");
	ImGui::TextWrapped("Hotkeys: Ctrl+Shift+H (or K) open / close the helper");
	ImGui::TextColored(kMuted, "%s", WikiBrowser::Status().c_str());
}
