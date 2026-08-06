#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "LivePanels.h"
#include "Sites.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace UIBrowseDetail
{
void DrawBrowseSiteRow(const BrowseSitesDrawCtx& ctx, int siteIndex, bool withCategoryPrefix)
{
	if (!ctx.sites || siteIndex < 0 || siteIndex >= static_cast<int>(ctx.siteCount))
		return;
	const SiteDef& site = ctx.sites[siteIndex];
	ImGui::PushID(siteIndex);
	/* Keep star+label on one row without SameLine edge cases that can
	   leave ListClipper with ItemsHeight==0 under nested headers. */
	const float rowStartY = ImGui::GetCursorPosY();
	if (!ctx.pickDefaultSite)
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

	const bool selected = (siteIndex == ctx.current);
	/* Always open catalog sites in a new addon tab (Ctrl still works the same). */
	if (ImGui::Selectable(row, selected))
	{
		if (ctx.pickDefaultSite)
			SetDefaultSiteIndex(siteIndex);
		else if (ctx.pickNewTab)
			ActivateSiteIndex(siteIndex, true, true);
		else
			ActivateSiteIndex(siteIndex, ctx.navigateOnChange, true);
		if (ctx.closePanel)
			*ctx.closePanel = true;
		sSyncCategory = true;
	}
	/* Guarantee the row advanced - empty labels / SameLine quirks must
	   not leave the cursor stuck (breaks clipper height measure). */
	if (ImGui::GetCursorPosY() <= rowStartY + 0.5f)
		ImGui::SetCursorPosY(rowStartY + ImGui::GetFrameHeightWithSpacing());
	if (ImGui::IsItemHovered())
	{
		if (ctx.pickNewTab)
			ImGui::SetTooltip("Open in a new tab");
		else if (!ctx.pickDefaultSite)
		{
			if (site.title && site.title[0] && site.label &&
				std::strcmp(site.title, site.label) != 0)
			{
				char tip[192];
				SanitizeForUi(tip, sizeof(tip), site.title);
				ImGui::SetTooltip("%s\nOpens in a new tab", tip);
			}
			else
				ImGui::SetTooltip("Opens in a new tab");
		}
		else if (site.title && site.title[0])
		{
			char tip[160];
			SanitizeForUi(tip, sizeof(tip), site.title);
			ImGui::SetTooltip("%s", tip);
		}
	}

	/* Drag-reorder favorites within a folder (full Browse panel only). */
	if (ctx.showFavorites && !ctx.pickDefaultSite)
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
		if (!ctx.pickNewTab && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			FavDrag drag{ folderId, favSlot };
			ImGui::SetDragDropPayload("FAV_FOLDER_REORDER", &drag, sizeof(drag));
			ImGui::TextUnformatted(site.label ? site.label : "Favorite");
			ImGui::EndDragDropSource();
		}
		if (!ctx.pickNewTab && ImGui::BeginDragDropTarget())
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
	if (ctx.shown)
		++(*ctx.shown);
}

/* Even-height rows + only submit visible ones (Browse lists can be 1000+).
   Always pass an explicit row height. Auto-measure + favorite-star SameLine
   under nested CollapsingHeaders can yield ItemsHeight==0 (assert-only in
   ImGui 1.80), which then seeks by zero and the expanded section looks empty. */
void DrawBrowseClippedRows(const BrowseSitesDrawCtx& ctx, const std::vector<int>& idxs, bool withCategoryPrefix)
{
	if (idxs.empty())
		return;
	const int n = static_cast<int>(idxs.size());
	const float rowH = ImGui::GetFrameHeightWithSpacing();
	if (n <= 96)
	{
		for (int i = 0; i < n; ++i)
			DrawBrowseSiteRow(ctx, idxs[static_cast<size_t>(i)], withCategoryPrefix);
		return;
	}
	ImGuiListClipper clipper;
	clipper.Begin(n, rowH);
	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			DrawBrowseSiteRow(ctx, idxs[static_cast<size_t>(i)], withCategoryPrefix);
	}
}

void DrawBrowseFilterMatches(const BrowseSitesDrawCtx& ctx)
{
	static char sFilterCache[128]{};
	static std::vector<int> sFilterMatches;
	if (std::strcmp(sFilterCache, sFilter) != 0)
	{
		std::snprintf(sFilterCache, sizeof(sFilterCache), "%s", sFilter);
		sFilterMatches.clear();
		sFilterMatches.reserve(64);
		for (int i = 0; i < static_cast<int>(ctx.siteCount); ++i)
		{
			if (!Sites::MatchesFilter(ctx.sites[i], sFilter))
				continue;
			const char* cat = ctx.sites[i].category ? ctx.sites[i].category : "";
			if (std::strcmp(cat, "Cheat Sheets") == 0)
				continue;
			sFilterMatches.push_back(i);
		}
	}
	DrawBrowseClippedRows(ctx, sFilterMatches, true);
	if (!sFilterMatches.empty())
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
		ImGui::Text("%d match%s", static_cast<int>(sFilterMatches.size()),
			sFilterMatches.size() == 1 ? "" : "es");
		ImGui::PopStyleColor();
	}
}

void DrawBrowseCategorySections(const BrowseSitesDrawCtx& ctx, const char* selectedCat)
{
	static std::string sBrowseCatKey;
	static std::vector<int> sBrowseCatIdx;
	static std::string sSecBucketKey; /* invalidated with cat idx below */
	static std::vector<std::vector<int>> sSecBuckets;
	bool browseCatRebuilt = false;
	{
		const char* key = selectedCat ? selectedCat : "";
		if (sBrowseCatKey != key)
		{
			sBrowseCatKey = key;
			sBrowseCatIdx.clear();
			sSecBucketKey.clear(); /* force section re-bucket with fresh indices */
			browseCatRebuilt = true;
			if (selectedCat && selectedCat[0] && ctx.sites)
			{
				sBrowseCatIdx.reserve(512);
				for (int i = 0; i < static_cast<int>(ctx.siteCount); ++i)
				{
					const char* cat = ctx.sites[i].category ? ctx.sites[i].category : "";
					if (std::strcmp(cat, selectedCat) == 0)
						sBrowseCatIdx.push_back(i);
				}
			}
		}
	}

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
			const SiteDef& site = ctx.sites[i];
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
		DrawBrowseClippedRows(ctx, hubs, false);
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
				const SiteDef& site = ctx.sites[i];
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
		DrawBrowseClippedRows(ctx, sBrowseCatIdx, false);
	}
	/* All sections collapsed still means the category has sites. */
	if (ctx.shown && *ctx.shown == 0 && anyInCategory)
		*ctx.shown = 1;
}

} // namespace UIBrowseDetail
