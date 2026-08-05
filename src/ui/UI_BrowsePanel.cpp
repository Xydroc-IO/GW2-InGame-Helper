#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "UI.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"
#include "Sites.h"
#include "LivePanels.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
			continue; /* Side rail hub — about:cheatsheets-hub */
		const int uiIndex = pickDefaultSite ? i : (i + 1);
		const bool selected = (uiIndex == sCategoryIndex);
		char label[96];
		std::snprintf(label, sizeof(label), "%s (%d)", cat, Sites::CountInCategory(cat));
		if (selected)
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.32f, 0.26f, 0.12f, 0.95f));
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
		ImGui::TextUnformatted("Favorites");
	else
		ImGui::TextUnformatted(selectedCat);
	ImGui::PopStyleColor();
	ImGui::Separator();

	const int current = pickDefaultSite
		? Sites::IndexOfId(G::DefaultSiteId)
		: Sites::ActiveIndex();
	int shown = 0;

	/* Cache site indices for the selected category (Wiki alone is 1000+). */
	static std::string sBrowseCatKey;
	static std::vector<int> sBrowseCatIdx;
	static std::string sSecBucketKey; /* invalidated with cat idx below */
	static std::vector<std::vector<int>> sSecBuckets;
	bool browseCatRebuilt = false;
	{
		const char* key = filtering ? "\x01" "filter" : (showFavorites ? "\x01" "fav" : selectedCat);
		if (sBrowseCatKey != key)
		{
			sBrowseCatKey = key;
			sBrowseCatIdx.clear();
			sSecBucketKey.clear(); /* force section re-bucket with fresh indices */
			browseCatRebuilt = true;
			if (!filtering && !showFavorites && selectedCat && selectedCat[0])
			{
				sBrowseCatIdx.reserve(512);
				for (int i = 0; i < static_cast<int>(siteCount); ++i)
				{
					const char* cat = sites[i].category ? sites[i].category : "";
					if (std::strcmp(cat, selectedCat) == 0)
						sBrowseCatIdx.push_back(i);
				}
			}
		}
	}

	auto DrawSiteRow = [&](int siteIndex, bool withCategoryPrefix) {
		if (siteIndex < 0 || siteIndex >= static_cast<int>(siteCount))
			return;
		const SiteDef& site = sites[siteIndex];
		ImGui::PushID(siteIndex);
		/* Keep star+label on one row without SameLine edge cases that can
		   leave ListClipper with ItemsHeight==0 under nested headers. */
		const float rowStartY = ImGui::GetCursorPosY();
		if (!pickDefaultSite)
		{
			DrawFavoriteStar(site.id);
			ImGui::SameLine(0.f, 4.f);
		}
		char row[160];
		if (withCategoryPrefix)
		{
			char safe[160];
			char tmp[160];
			std::snprintf(tmp, sizeof(tmp), "%s - %s",
				site.category ? site.category : "",
				site.label ? site.label : "");
			SanitizeForUi(safe, sizeof(safe), tmp);
			std::snprintf(row, sizeof(row), "%s", safe);
		}
		else
			std::snprintf(row, sizeof(row), "%s", site.label ? site.label : site.id ? site.id : "(site)");

		const bool selected = (siteIndex == current);
		const bool ctrl = ImGui::GetIO().KeyCtrl;
		if (ImGui::Selectable(row, selected))
		{
			if (pickDefaultSite)
				SetDefaultSiteIndex(siteIndex);
			else if (pickNewTab)
				ActivateSiteIndex(siteIndex, true, true);
			else
				ActivateSiteIndex(siteIndex, navigateOnChange, ctrl);
			if (closePanel)
				*closePanel = true;
			sSyncCategory = true;
		}
		/* Guarantee the row advanced — empty labels / SameLine quirks must
		   not leave the cursor stuck (breaks clipper height measure). */
		if (ImGui::GetCursorPosY() <= rowStartY + 0.5f)
			ImGui::SetCursorPosY(rowStartY + ImGui::GetFrameHeightWithSpacing());
		if (ImGui::IsItemHovered())
		{
			if (pickNewTab)
				ImGui::SetTooltip("Open in a new tab");
			else if (!pickDefaultSite)
			{
				if (site.title && site.title[0] && site.label &&
					std::strcmp(site.title, site.label) != 0)
				{
					char tip[192];
					SanitizeForUi(tip, sizeof(tip), site.title);
					ImGui::SetTooltip("%s\nClick: this tab | Ctrl+click: new tab", tip);
				}
				else
					ImGui::SetTooltip("Click: this tab | Ctrl+click: new tab");
			}
			else if (site.title && site.title[0])
			{
				char tip[160];
				SanitizeForUi(tip, sizeof(tip), site.title);
				ImGui::SetTooltip("%s", tip);
			}
		}

		/* Drag-reorder favorites */
		if (showFavorites && !pickDefaultSite && !pickNewTab)
		{
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				const int favSlot = [&]() {
					const int favN = Sites::FavoriteCount();
					for (int f = 0; f < favN; ++f)
					{
						if (Sites::FavoriteSiteIndex(f) == siteIndex)
							return f;
					}
					return -1;
				}();
				ImGui::SetDragDropPayload("FAV_REORDER", &favSlot, sizeof(favSlot));
				ImGui::TextUnformatted(site.label ? site.label : "Favorite");
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_REORDER"))
				{
					const int from = *static_cast<const int*>(payload->Data);
					const int favN = Sites::FavoriteCount();
					int to = -1;
					for (int f = 0; f < favN; ++f)
					{
						if (Sites::FavoriteSiteIndex(f) == siteIndex)
						{
							to = f;
							break;
						}
					}
					if (from >= 0 && to >= 0 && Sites::MoveFavorite(from, to))
						LivePanels::NotifyFavoritesChanged();
				}
				ImGui::EndDragDropTarget();
			}
		}
		if (selected)
			ImGui::SetItemDefaultFocus();
		ImGui::PopID();
		++shown;
	};

	/* Even-height rows + only submit visible ones (Browse lists can be 1000+).
	   Always pass an explicit row height. Auto-measure + favorite-star SameLine
	   under nested CollapsingHeaders can yield ItemsHeight==0 (assert-only in
	   ImGui 1.80), which then seeks by zero and the expanded section looks empty. */
	auto DrawClippedRows = [&](const std::vector<int>& idxs, bool withCategoryPrefix) {
		if (idxs.empty())
			return;
		const int n = static_cast<int>(idxs.size());
		const float rowH = ImGui::GetFrameHeightWithSpacing();
		if (n <= 96)
		{
			for (int i = 0; i < n; ++i)
				DrawSiteRow(idxs[static_cast<size_t>(i)], withCategoryPrefix);
			return;
		}
		ImGuiListClipper clipper;
		clipper.Begin(n, rowH);
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				DrawSiteRow(idxs[static_cast<size_t>(i)], withCategoryPrefix);
		}
	};

	if (showFavorites)
	{
		static unsigned sFavGen = 0;
		static std::vector<int> sFavIdx;
		const unsigned gen = Sites::FavoritesGeneration();
		if (sFavGen != gen)
		{
			sFavGen = gen;
			sFavIdx.clear();
			const int favN = Sites::FavoriteCount();
			sFavIdx.reserve(static_cast<size_t>(favN));
			for (int f = 0; f < favN; ++f)
			{
				const int si = Sites::FavoriteSiteIndex(f);
				if (si >= 0)
					sFavIdx.push_back(si);
			}
		}
		DrawClippedRows(sFavIdx, true);
	}
	else if (filtering)
	{
		static char sFilterCache[128]{};
		static std::vector<int> sFilterMatches;
		if (std::strcmp(sFilterCache, sFilter) != 0)
		{
			std::snprintf(sFilterCache, sizeof(sFilterCache), "%s", sFilter);
			sFilterMatches.clear();
			sFilterMatches.reserve(64);
			for (int i = 0; i < static_cast<int>(siteCount); ++i)
			{
				if (!Sites::MatchesFilter(sites[i], sFilter))
					continue;
				const char* cat = sites[i].category ? sites[i].category : "";
				if (std::strcmp(cat, "Cheat Sheets") == 0)
					continue;
				sFilterMatches.push_back(i);
			}
		}
		DrawClippedRows(sFilterMatches, true);
		if (!sFilterMatches.empty())
		{
			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
			ImGui::Text("%d match%s", static_cast<int>(sFilterMatches.size()),
				sFilterMatches.size() == 1 ? "" : "es");
			ImGui::PopStyleColor();
		}
	}
	else
	{
		size_t secCount = 0;
		const char* const* sections = Sites::BrowseSections(selectedCat, &secCount);
		bool anyInCategory = false;

		std::function<void(const std::vector<int>&, int, const char*)> drawPathTree;
		drawPathTree = [&](const std::vector<int>& indices, int depth, const char* parentKey) {
			if (indices.empty())
				return;
			std::vector<int> hubs;
			hubs.reserve(indices.size());
			std::vector<std::string> childOrder;
			std::unordered_map<std::string, std::vector<int>> byChild;
			for (int i : indices)
			{
				const SiteDef& site = sites[i];
				if (site.browsePathCount <= depth || !site.browsePath)
				{
					hubs.push_back(i);
					continue;
				}
				const char* label = site.browsePath[depth];
				if (!label || !label[0])
				{
					hubs.push_back(i);
					continue;
				}
				auto it = byChild.find(label);
				if (it == byChild.end())
				{
					childOrder.emplace_back(label);
					byChild.emplace(label, std::vector<int>{i});
				}
				else
					it->second.push_back(i);
			}
			DrawClippedRows(hubs, false);
			if (childOrder.empty())
				return;
			ImGui::Indent(10.f);
			for (const std::string& child : childOrder)
			{
				const std::vector<int>& childIdx = byChild[child];
				if (childIdx.empty())
					continue;
				if (!BeginBrowseSection(parentKey, child.c_str(), static_cast<int>(childIdx.size())))
					continue;
				drawPathTree(childIdx, depth + 1, child.c_str());
			}
			ImGui::Unindent(10.f);
		};

		if (sections && secCount > 0)
		{
			if (browseCatRebuilt || sSecBucketKey != selectedCat)
			{
				sSecBucketKey = selectedCat ? selectedCat : "";
				sSecBuckets.assign(secCount, {});
				for (int i : sBrowseCatIdx)
				{
					const SiteDef& site = sites[i];
					if (!site.browsePath || site.browsePathCount <= 0 || !site.browsePath[0])
						continue;
					const char* sec = site.browsePath[0];
					for (size_t s = 0; s < secCount; ++s)
					{
						if (std::strcmp(sec, sections[s]) == 0)
						{
							sSecBuckets[s].push_back(i);
							break;
						}
					}
				}
			}
			if (sSecBuckets.size() < secCount)
				sSecBuckets.resize(secCount);
			for (size_t s = 0; s < secCount; ++s)
			{
				const char* section = sections[s];
				const std::vector<int>& secIdx = sSecBuckets[s];
				const int secSites = static_cast<int>(secIdx.size());
				if (secSites == 0)
					continue;
				anyInCategory = true;
				if (!BeginBrowseSection(selectedCat, section, secSites))
					continue;
				drawPathTree(secIdx, 1, section);
			}
		}
		else
		{
			anyInCategory = !sBrowseCatIdx.empty();
			DrawClippedRows(sBrowseCatIdx, false);
		}
		/* All sections collapsed still means the category has sites. */
		if (shown == 0 && anyInCategory)
			shown = 1;
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

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::PushStyleColor(ImGuiCol_Text, kGoldMuted);
	ImGui::TextUnformatted("Created by Xydroc");
	ImGui::TextUnformatted("IGN - swift shadow kuda.5981 | Discord Name - xydroc");
	ImGui::PopStyleColor();
}

} // namespace UIBrowseDetail
