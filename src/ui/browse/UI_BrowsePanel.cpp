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

	static char sNewFolder[48]{};
	static bool sFocusNewFolder = false;
	static int sRenameFolderId = -1;
	static char sRenameBuf[48]{};

	ImGui::PushStyleColor(ImGuiCol_Text, kGold);
	if (filtering)
		ImGui::TextUnformatted("Matching sites");
	else if (showFavorites)
	{
		ImGui::TextUnformatted("Favorites");
		ImGui::SameLine(0.f, 10.f);
		ImGui::PopStyleColor();
		if (ImGui::SmallButton("+ Folder###gw2igh_fav_add_folder"))
		{
			sNewFolder[0] = 0;
			sFocusNewFolder = true;
			ImGui::OpenPopup("##gw2igh_new_fav_folder");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Create a folder to organize favorites");
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
	}
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

		/* Drag-reorder favorites within a folder (full Browse panel only). */
		if (showFavorites && !pickDefaultSite)
		{
			const int folderId = Sites::FavoriteFolderOf(site.id);
			const int favSlot = [&]() {
				const int favN = Sites::FavoriteCountInFolder(folderId);
				for (int f = 0; f < favN; ++f)
				{
					if (Sites::FavoriteSiteIndexInFolder(folderId, f) == siteIndex)
						return f;
				}
				return -1;
			}();
			struct FavDrag
			{
				int folderId;
				int slot;
			};
			if (!pickNewTab && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				FavDrag drag{ folderId, favSlot };
				ImGui::SetDragDropPayload("FAV_FOLDER_REORDER", &drag, sizeof(drag));
				ImGui::TextUnformatted(site.label ? site.label : "Favorite");
				ImGui::EndDragDropSource();
			}
			if (!pickNewTab && ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_FOLDER_REORDER"))
				{
					const FavDrag from = *static_cast<const FavDrag*>(payload->Data);
					if (from.folderId == folderId && from.slot >= 0 && favSlot >= 0 &&
						Sites::MoveFavoriteInFolder(folderId, from.slot, favSlot))
						LivePanels::NotifyFavoritesChanged();
				}
				ImGui::EndDragDropTarget();
			}
			if (ImGui::BeginPopupContextItem("##gw2igh_fav_ctx"))
			{
				ImGui::TextDisabled("Move to folder");
				ImGui::Separator();
				if (ImGui::MenuItem("Unfiled", nullptr, folderId == 0))
				{
					if (Sites::SetFavoriteFolder(site.id, 0))
						LivePanels::NotifyFavoritesChanged();
				}
				const int folderN = Sites::FavoriteFolderCount();
				for (int fi = 0; fi < folderN; ++fi)
				{
					const int fid = Sites::FavoriteFolderIdAt(fi);
					const char* fname = Sites::FavoriteFolderName(fid);
					if (ImGui::MenuItem(fname ? fname : "Folder", nullptr, folderId == fid))
					{
						if (Sites::SetFavoriteFolder(site.id, fid))
							LivePanels::NotifyFavoritesChanged();
					}
				}
				ImGui::EndPopup();
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
		if (ImGui::BeginPopupModal("##gw2igh_new_fav_folder", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("New favorites folder");
			ImGui::Spacing();
			if (sFocusNewFolder)
			{
				ImGui::SetKeyboardFocusHere();
				sFocusNewFolder = false;
			}
			const bool enter = ImGui::InputTextWithHint("##gw2igh_new_fav_name",
				"Folder name…", sNewFolder, sizeof(sNewFolder),
				ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::Spacing();
			const bool canSave = sNewFolder[0] != 0;
			if ((ImGui::Button("Create", ImVec2(96.f, 0.f)) || enter) && canSave)
			{
				if (Sites::CreateFavoriteFolder(sNewFolder))
				{
					sNewFolder[0] = 0;
					LivePanels::NotifyFavoritesChanged();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(96.f, 0.f)))
			{
				sNewFolder[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		auto DrawFolderBlock = [&](int folderId) {
			const int n = Sites::FavoriteCountInFolder(folderId);
			const char* name = Sites::FavoriteFolderName(folderId);
			char header[96];
			std::snprintf(header, sizeof(header), "%s (%d)###gw2igh_favfold_%d",
				name ? name : "Folder", n, folderId);
			const bool open = ImGui::CollapsingHeader(header,
				ImGuiTreeNodeFlags_DefaultOpen);
			if (folderId != 0 && ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Rename…"))
				{
					sRenameFolderId = folderId;
					std::snprintf(sRenameBuf, sizeof(sRenameBuf), "%s", name ? name : "");
				}
				if (ImGui::MenuItem("Delete folder"))
				{
					if (Sites::DeleteFavoriteFolder(folderId))
						LivePanels::NotifyFavoritesChanged();
				}
				ImGui::EndPopup();
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_FOLDER_REORDER"))
				{
					struct FavDrag { int folderId; int slot; };
					const FavDrag from = *static_cast<const FavDrag*>(payload->Data);
					/* Drop on header: move that item into this folder. */
					const int si = Sites::FavoriteSiteIndexInFolder(from.folderId, from.slot);
					if (si >= 0 && sites)
					{
						const char* id = sites[si].id;
						if (id && Sites::SetFavoriteFolder(id, folderId))
							LivePanels::NotifyFavoritesChanged();
					}
				}
				ImGui::EndDragDropTarget();
			}
			if (!open || n <= 0)
				return;
			std::vector<int> idxs;
			idxs.reserve(static_cast<size_t>(n));
			for (int f = 0; f < n; ++f)
			{
				const int si = Sites::FavoriteSiteIndexInFolder(folderId, f);
				if (si >= 0)
					idxs.push_back(si);
			}
			DrawClippedRows(idxs, true);
		};

		if (sRenameFolderId > 0)
		{
			ImGui::OpenPopup("##gw2igh_rename_fav_folder");
			if (ImGui::BeginPopupModal("##gw2igh_rename_fav_folder", nullptr,
				ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("Rename folder");
				ImGui::InputText("##gw2igh_ren_fav", sRenameBuf, sizeof(sRenameBuf));
				if (ImGui::Button("Save") && sRenameBuf[0])
				{
					if (Sites::RenameFavoriteFolder(sRenameFolderId, sRenameBuf))
						LivePanels::NotifyFavoritesChanged();
					sRenameFolderId = -1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					sRenameFolderId = -1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		/* Unfiled first, then user folders. */
		DrawFolderBlock(0);
		const int folderN = Sites::FavoriteFolderCount();
		for (int fi = 0; fi < folderN; ++fi)
			DrawFolderBlock(Sites::FavoriteFolderIdAt(fi));

		if (Sites::FavoriteCount() == 0)
			ImGui::TextDisabled("No favorites yet — star a site to pin it here.");
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
