#pragma once

#include "Sites.h"

#include "imgui/imgui.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* Shared Browse UI state/helpers for UI_Browse*.cpp. */
namespace UIBrowseDetail
{
	float Clampf(float v, float lo, float hi);

	extern const ImVec4& kGold;
	extern const ImVec4& kGoldDim;
	extern const ImVec4& kGoldMuted;
	extern const ImVec4& kMuted;

	extern char sFilter[64];
	extern int sCategoryIndex;
	extern bool sSyncCategory;
	extern bool sFocusFilter;
	extern bool sRequestNewTabPicker;

	extern std::unordered_set<std::string> gBrowseOpen;

	std::string BrowseSectionKey(const char* category, const char* section);
	bool BrowseSectionIsOpen(const char* category, const char* section);
	void BrowseSectionSetOpen(const char* category, const char* section, bool open);
	bool BeginBrowseSection(const char* category, const char* section, int count);
	void ActivateSiteIndex(int index, bool navigate, bool newTab);
	void SetDefaultSiteIndex(int index);
	const SiteDef* SiteById(const char* id);
	void SanitizeForUi(char* dst, size_t dstLen, const char* src);
	void DrawStarShape(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, bool filled);
	bool FavoriteToggleButton(const char* id, bool favorited, bool smallBtn);
	void DrawFavoriteStar(const char* siteId);

	struct BrowsePopupLayout
	{
		float width;
		float height;
		float listH;
		float leftW;
	};
	BrowsePopupLayout CalcBrowsePopupLayout(bool withBanner, bool pickDefaultSite);
	ImVec2 CaptureAnchorBelowItem();
	void PrepareBrowsePopup(ImVec2 anchor, const BrowsePopupLayout& lay);

	/* Dropdown-style picker: pin under the button that opened it (not a free window). */
	constexpr ImGuiWindowFlags kBrowsePopupFlags =
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	extern ImVec2 sBrowseAnchor;
	extern ImVec2 sNewTabBrowseAnchor;
	extern ImVec2 sDefaultSiteBrowseAnchor;

	struct BrowseSitesDrawCtx
	{
		const SiteDef* sites = nullptr;
		size_t siteCount = 0;
		int current = -1;
		bool pickDefaultSite = false;
		bool pickNewTab = false;
		bool navigateOnChange = false;
		bool* closePanel = nullptr;
		bool showFavorites = false;
		int* shown = nullptr;
	};

	void DrawBrowseFavoritesHeader();
	void DrawBrowseFavoritesPane(const BrowseSitesDrawCtx& ctx);

	void DrawBrowseSiteRow(const BrowseSitesDrawCtx& ctx, int siteIndex, bool withCategoryPrefix);
	void DrawBrowseClippedRows(const BrowseSitesDrawCtx& ctx, const std::vector<int>& idxs, bool withCategoryPrefix);
	void DrawBrowseFilterMatches(const BrowseSitesDrawCtx& ctx);
	void DrawBrowseCategorySections(const BrowseSitesDrawCtx& ctx, const char* selectedCat);

	void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel,
		bool pickDefaultSite = false, bool pickNewTab = false,
		float listHArg = -1.f, float leftWArg = -1.f);
}
