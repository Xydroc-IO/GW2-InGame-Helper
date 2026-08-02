#include "UI_Browse.h"

#include "UI.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"
#include "Sites.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
	float Clampf(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	const ImVec4& kGold = HelperTheme::Gold;
	const ImVec4& kGoldDim = HelperTheme::GoldDim;
	const ImVec4& kGoldMuted = HelperTheme::GoldMuted;
	const ImVec4& kMuted = HelperTheme::Muted;

	static char sFilter[64] = {};
	static int sCategoryIndex = 0;
	static bool sSyncCategory = true;
	static bool sFocusFilter = false;
	static bool sRequestNewTabPicker = false;

/* Browse hierarchy comes from sites.json browsePath / browseSections. */

std::unordered_set<std::string> gBrowseOpen;

std::string BrowseSectionKey(const char* category, const char* section)
{
	std::string k;
	k.reserve(64);
	k += category && category[0] ? category : "_";
	k += '|';
	k += section && section[0] ? section : "_";
	return k;
}

bool BrowseSectionIsOpen(const char* category, const char* section)
{
	return gBrowseOpen.find(BrowseSectionKey(category, section)) != gBrowseOpen.end();
}

void BrowseSectionSetOpen(const char* category, const char* section, bool open)
{
	const std::string key = BrowseSectionKey(category, section);
	const bool was = gBrowseOpen.find(key) != gBrowseOpen.end();
	if (open == was)
		return;
	if (open)
		gBrowseOpen.insert(key);
	else
		gBrowseOpen.erase(key);
	Settings::SetDirty();
}

/* Returns true when the section body should be drawn. Defaults collapsed;
   open state is restored from settings and saved when the user toggles. */
bool BeginBrowseSection(const char* category, const char* section, int count)
{
	if (!section || !section[0])
		return true;
	char label[160];
	std::snprintf(label, sizeof(label), "%s (%d)###bsec_%s_%s",
		section, count,
		category && category[0] ? category : "_",
		section);
	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, kGoldDim);
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.10f, 0.055f, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.18f, 0.09f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.32f, 0.26f, 0.12f, 1.f));
	ImGui::SetNextItemOpen(BrowseSectionIsOpen(category, section), ImGuiCond_Once);
	const bool open = ImGui::CollapsingHeader(label);
	ImGui::PopStyleColor(4);
	if (ImGui::IsItemToggledOpen())
		BrowseSectionSetOpen(category, section, open);
	return open;
}

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

void SetDefaultSiteIndex(int index)
{
	if (index < 0)
		return;
	size_t siteCount = 0;
	const SiteDef* sites = Sites::All(&siteCount);
	if (!sites || index >= static_cast<int>(siteCount) || !sites[index].id)
		return;
	std::snprintf(G::DefaultSiteId, sizeof(G::DefaultSiteId), "%s", sites[index].id);
	Settings::SetDirty();
}

const SiteDef* SiteById(const char* id)
{
	const int idx = Sites::IndexOfId(id);
	if (idx < 0)
		return nullptr;
	size_t n = 0;
	const SiteDef* sites = Sites::All(&n);
	if (!sites || idx >= static_cast<int>(n))
		return nullptr;
	return &sites[idx];
}

/* ProggyClean lacks · — … etc. Keep ImGui labels ASCII-only. */
void SanitizeForUi(char* dst, size_t dstLen, const char* src)
{
	if (!dst || dstLen == 0)
		return;
	dst[0] = 0;
	if (!src)
		return;
	size_t o = 0;
	for (size_t i = 0; src[i] && o + 1 < dstLen; )
	{
		const unsigned char c = static_cast<unsigned char>(src[i]);
		if (c < 0x80)
		{
			dst[o++] = static_cast<char>(c);
			++i;
			continue;
		}
		/* UTF-8 em/en dash → '-' */
		if ((c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
				(static_cast<unsigned char>(src[i + 2]) == 0x94 ||
					static_cast<unsigned char>(src[i + 2]) == 0x93)))
		{
			dst[o++] = '-';
			i += 3;
			continue;
		}
		/* middle dot · */
		if (c == 0xC2 && static_cast<unsigned char>(src[i + 1]) == 0xB7)
		{
			dst[o++] = '-';
			i += 2;
			continue;
		}
		/* ellipsis … */
		if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
			static_cast<unsigned char>(src[i + 2]) == 0xA6)
		{
			if (o + 3 < dstLen)
			{
				dst[o++] = '.';
				dst[o++] = '.';
				dst[o++] = '.';
			}
			i += 3;
			continue;
		}
		/* skip other multibyte sequences */
		if ((c & 0xE0) == 0xC0) i += 2;
		else if ((c & 0xF0) == 0xE0) i += 3;
		else if ((c & 0xF8) == 0xF0) i += 4;
		else ++i;
	}
	dst[o] = 0;
}
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
	const bool pressed = ImGui::InvisibleButton("##gw2igh_star", size);
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

/* Browse popup sized from the display — comfortable on 1080p, a bit roomier on 4K. */
struct BrowsePopupLayout
{
	float width;
	float height;
	float listH;
	float leftW;
};

BrowsePopupLayout CalcBrowsePopupLayout(bool withBanner, bool pickDefaultSite)
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImGuiStyle& st = ImGui::GetStyle();
	const float dispW = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x : 800.f;
	const float dispH = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y : 600.f;

	static float sCacheDispW = -1.f;
	static float sCacheDispH = -1.f;
	static float sCacheFont = -1.f;
	static bool sCacheBanner = false;
	static bool sCacheDefault = false;
	static BrowsePopupLayout sCache{};

	const float fontScale = ImGui::GetFontSize();
	if (sCacheDispW == dispW && sCacheDispH == dispH && sCacheFont == fontScale &&
		sCacheBanner == withBanner && sCacheDefault == pickDefaultSite)
		return sCache;

	/* Compact on 1080p (~540×390), a bit wider on 1440p/4K — never half the screen. */
	const float width = Clampf(dispW * 0.28f, 480.f, 680.f);
	const float maxOuter = Clampf(dispH * 0.36f, 300.f, 480.f);
	const float listMax = pickDefaultSite
		? Clampf(dispH * 0.20f, 160.f, 260.f)
		: Clampf(dispH * 0.24f, 180.f, 300.f);

	float chrome = st.WindowPadding.y * 2.f;
	if (withBanner)
		chrome += ImGui::GetTextLineHeightWithSpacing() * 2.f;
	chrome += ImGui::GetFrameHeightWithSpacing(); /* Search + filter */
	chrome += st.ItemSpacing.y; /* Spacing() */
	chrome += 1.f;             /* Separator */
	chrome += st.ItemSpacing.y;
	chrome += ImGui::GetTextLineHeightWithSpacing(); /* Created by */
	chrome += ImGui::GetTextLineHeight();            /* IGN | Discord */
	chrome += 8.f;

	const float listH = Clampf(maxOuter - chrome, 160.f, listMax);
	BrowsePopupLayout lay{};
	lay.width = width;
	lay.height = chrome + listH;
	lay.listH = listH;
	lay.leftW = Clampf(width * 0.26f, 140.f, 180.f);

	sCacheDispW = dispW;
	sCacheDispH = dispH;
	sCacheFont = fontScale;
	sCacheBanner = withBanner;
	sCacheDefault = pickDefaultSite;
	sCache = lay;
	return lay;
}

/* Dropdown-style picker: pin under the button that opened it (not a free window). */
static constexpr ImGuiWindowFlags kBrowsePopupFlags =
	ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
	ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

ImVec2 CaptureAnchorBelowItem()
{
	return ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2.f);
}

void PrepareBrowsePopup(ImVec2 anchor, const BrowsePopupLayout& lay)
{
	const ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos = anchor;
	if (pos.x + lay.width > io.DisplaySize.x - 8.f)
		pos.x = Clampf(io.DisplaySize.x - lay.width - 8.f, 8.f, io.DisplaySize.x);
	if (pos.x < 8.f)
		pos.x = 8.f;
	/* Flip above the button when there isn't room below. */
	if (pos.y + lay.height > io.DisplaySize.y - 8.f)
		pos.y = Clampf(anchor.y - lay.height - ImGui::GetFrameHeight() - 6.f, 8.f, io.DisplaySize.y);

	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(lay.width, lay.height), ImGuiCond_Always);
}

ImVec2 sBrowseAnchor{};
ImVec2 sNewTabBrowseAnchor{};
ImVec2 sDefaultSiteBrowseAnchor{};

void DrawBrowsePanelContents(bool navigateOnChange, bool* closePanel, bool pickDefaultSite = false, bool pickNewTab = false, float listHArg = -1.f, float leftWArg = -1.f)
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
					if (from >= 0 && to >= 0)
						Sites::MoveFavorite(from, to);
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

}

void UI_ParseBrowseOpen(const char* val)
{
	gBrowseOpen.clear();
	if (!val || !val[0])
		return;
	const char* p = val;
	while (*p)
	{
		while (*p == ';' || *p == ' ')
			++p;
		if (!*p)
			break;
		const char* start = p;
		while (*p && *p != ';')
			++p;
		std::string key(start, p);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\r' || key.back() == '\n'))
			key.pop_back();
		if (!key.empty() && key.find('|') != std::string::npos)
			gBrowseOpen.insert(std::move(key));
	}
}

void UI_WriteBrowseOpen(FILE* f)
{
	if (!f)
		return;
	std::fputs("BrowseOpen=", f);
	bool first = true;
	for (const std::string& key : gBrowseOpen)
	{
		if (key.find('|') == std::string::npos)
			continue;
		if (!first)
			std::fputc(';', f);
		first = false;
		std::fputs(key.c_str(), f);
	}
	std::fputc('\n', f);
}


void UI_Browse_OnMainButtonClicked()
{
	sSyncCategory = true;
	sFocusFilter = true;
	ImGui::OpenPopup("##gw2igh_site_browse");
}

void UI_Browse_DrawMainPopup()
{
	sBrowseAnchor = CaptureAnchorBelowItem();
	const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(false, false);
	PrepareBrowsePopup(sBrowseAnchor, browseLay);
	if (ImGui::BeginPopup("##gw2igh_site_browse", kBrowsePopupFlags))
	{
		bool closePanel = false;
		DrawBrowsePanelContents(true, &closePanel, false, false, browseLay.listH, browseLay.leftW);
		if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();
		UI_NoteHelperPopupHover();
		ImGui::EndPopup();
	}
}

void UI_Browse_OnNewTabButtonClicked()
{
	sSyncCategory = true;
	sFocusFilter = true;
	ImGui::OpenPopup("##gw2igh_site_browse_newtab");
}

void UI_Browse_RequestNewTabPicker()
{
	sRequestNewTabPicker = true;
	sFocusFilter = true;
}

bool UI_Browse_ConsumeNewTabPickerRequest()
{
	if (!sRequestNewTabPicker)
		return false;
	sRequestNewTabPicker = false;
	return true;
}

void UI_Browse_DrawNewTabPopup()
{
	sNewTabBrowseAnchor = CaptureAnchorBelowItem();
	const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(true, false);
	PrepareBrowsePopup(sNewTabBrowseAnchor, browseLay);
	if (ImGui::BeginPopup("##gw2igh_site_browse_newtab", kBrowsePopupFlags))
	{
		bool closePanel = false;
		DrawBrowsePanelContents(true, &closePanel, false, true, browseLay.listH, browseLay.leftW);
		if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();
		UI_NoteHelperPopupHover();
		ImGui::EndPopup();
	}
}

void UI_Browse_DrawDefaultSitePicker()
{
	if (ImGui::Button("Choose default site...###gw2igh_choose_default"))
	{
		sSyncCategory = true;
		sFocusFilter = true;
		ImGui::OpenPopup("##gw2igh_default_site_browse");
	}
	sDefaultSiteBrowseAnchor = CaptureAnchorBelowItem();

	ImGui::SameLine();
	const SiteDef* def = SiteById(G::DefaultSiteId);
	if (def)
		ImGui::TextColored(kMuted, "%s - %s",
			def->category ? def->category : "",
			def->label ? def->label : "");
	else
		ImGui::TextColored(kMuted, "%s", G::DefaultSiteId);

	const BrowsePopupLayout browseLay = CalcBrowsePopupLayout(true, true);
	PrepareBrowsePopup(sDefaultSiteBrowseAnchor, browseLay);
	if (ImGui::BeginPopup("##gw2igh_default_site_browse", kBrowsePopupFlags))
	{
		bool closePanel = false;
		DrawBrowsePanelContents(false, &closePanel, true, false, browseLay.listH, browseLay.leftW);
		if (closePanel || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();
		UI_NoteHelperPopupHover();
		ImGui::EndPopup();
	}

}

bool UI_Browse_ToolbarFavoriteToggle()
{
	const bool fav = Sites::IsFavorite(Sites::ActiveId());
	if (FavoriteToggleButton("toolbar", fav, false))
	{
		Sites::ToggleFavorite(Sites::ActiveId());
		return true;
	}
	return false;
}
