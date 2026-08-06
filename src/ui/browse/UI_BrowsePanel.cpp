#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "UI.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Sites.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace UIBrowseDetail
{
void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel, bool pickDefaultSite, bool pickNewTab, float listHArg, float leftWArg)
{
	size_t siteCount = 0;
	const SiteDef* sites = Sites::All(&siteCount);
	size_t catCount = 0;
	const char* const* cats = Sites::Categories(&catCount);
	if (!sites || siteCount == 0 || !cats || catCount == 0)
		return;

	/* Index 0 = virtual Favorites (browse / new-tab); categories follow. */
	const int totalCats = pickDefaultSite
		? static_cast<int>(catCount)
		: static_cast<int>(catCount) + 1;

	if (sSyncCategory)
	{
		sSyncCategory = false;
		const char* focusId = pickDefaultSite ? G::DefaultSiteId : Sites::ActiveId();
		const SiteDef* focus = SiteById(focusId);
		if (!pickDefaultSite && Sites::IsFavorite(focusId))
			sCategoryIndex = 0;
		else
		{
			const char* activeCat = (focus && focus->category) ? focus->category : "";
			sCategoryIndex = pickDefaultSite ? 0 : 1;
			for (int i = 0; i < static_cast<int>(catCount); ++i)
			{
				if (std::strcmp(cats[i] ? cats[i] : "", activeCat) == 0)
				{
					sCategoryIndex = pickDefaultSite ? i : (i + 1);
					break;
				}
			}
		}
	}
	if (sCategoryIndex < 0 || sCategoryIndex >= totalCats)
		sCategoryIndex = 0;

	if (pickDefaultSite)
	{
		ImGui::TextColored(kGold, "Default landing site");
		ImGui::TextColored(kMuted, "Home button - and when no tabs are saved yet.");
	}
	else if (pickNewTab)
	{
		ImGui::TextColored(kGold, "Open in new tab");
		ImGui::TextColored(kMuted, "Pick a site to open beside your current tabs.");
	}

	ImGui::TextColored(kGold, "Search");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-1.f);
	if (sFocusFilter)
	{
		ImGui::SetKeyboardFocusHere();
		sFocusFilter = false;
	}
	/* ### keeps a unique ID in the shared Nexus ImGui context without a visible label. */
	ImGui::InputTextWithHint("###gw2igh_site_filter", "Filter sites...", sFilter, sizeof(sFilter));

	const bool filtering = sFilter[0] != '\0';
	const bool showFavorites = (!filtering && !pickDefaultSite && sCategoryIndex == 0);
	const char* selectedCat = "";
	if (!filtering && !showFavorites)
	{
		const int catIdx = pickDefaultSite ? sCategoryIndex : (sCategoryIndex - 1);
		if (catIdx >= 0 && catIdx < static_cast<int>(catCount))
			selectedCat = cats[catIdx] ? cats[catIdx] : "";
		/* Remap if Browse was left on the retired Cheat Sheets category. */
		if (selectedCat && std::strcmp(selectedCat, "Cheat Sheets") == 0)
		{
			sCategoryIndex = pickDefaultSite ? 0 : 1;
			for (int i = 0; i < static_cast<int>(catCount); ++i)
			{
				if (cats[i] && std::strcmp(cats[i], "Cheat Sheets") != 0)
				{
					sCategoryIndex = pickDefaultSite ? i : (i + 1);
					selectedCat = cats[i];
					break;
				}
			}
		}
	}

	const float listH = (listHArg > 0.f) ? listHArg : (pickDefaultSite ? 300.f : 320.f);
	const float leftW = (leftWArg > 0.f) ? leftWArg : 172.f;

	ImGui::BeginChild("##gw2igh_browse_cats", ImVec2(leftW, listH), true);
	ImGui::PushStyleColor(ImGuiCol_Text, kGold);
	ImGui::TextUnformatted("Categories");
	ImGui::PopStyleColor();
	ImGui::Separator();

	if (!pickDefaultSite)
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
		if (std::strcmp(cat, "Cheat Sheets") == 0)
			continue; /* Side rail hub - about:cheatsheets-hub */
		const int uiIndex = pickDefaultSite ? i : (i + 1);
		const bool selected = (uiIndex == sCategoryIndex);
		char label[96];
		std::snprintf(label, sizeof(label), "%s (%d)", cat, Sites::CountInCategory(cat));
		if (selected)
			ImGui::PushStyleColor(ImGuiCol_Header, HelperTheme::Header);
		if (ImGui::Selectable(label, selected))
		{
			sCategoryIndex = uiIndex;
			sFilter[0] = '\0';
		}
		if (selected)
			ImGui::PopStyleColor();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("##gw2igh_browse_sites", ImVec2(0.f, listH), true);

	ImGui::PushStyleColor(ImGuiCol_Text, kGold);
	if (filtering)
		ImGui::TextUnformatted("Matching sites");
	else if (showFavorites)
		DrawBrowseFavoritesHeader();
	else
		ImGui::TextUnformatted(selectedCat);
	ImGui::PopStyleColor();
	ImGui::Separator();

	const int current = pickDefaultSite
		? Sites::IndexOfId(G::DefaultSiteId)
		: Sites::ActiveIndex();
	int shown = 0;

	BrowseSitesDrawCtx ctx{};
	ctx.sites = sites;
	ctx.siteCount = siteCount;
	ctx.current = current;
	ctx.pickDefaultSite = pickDefaultSite;
	ctx.pickNewTab = pickNewTab;
	ctx.navigateOnChange = navigateOnChange;
	ctx.closePanel = closePanel;
	ctx.showFavorites = showFavorites;
	ctx.shown = &shown;

	if (showFavorites)
		DrawBrowseFavoritesPane(ctx);
	else if (filtering)
		DrawBrowseFilterMatches(ctx);
	else
		DrawBrowseCategorySections(ctx, selectedCat);

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

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
	ImGui::TextUnformatted("Created By Xydroc");
	ImGui::PopStyleColor();
}

} // namespace UIBrowseDetail
