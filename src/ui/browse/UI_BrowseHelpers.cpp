#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "UI.h"
#include "AspectLayout.h"
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

	char sFilter[64] = {};
	int sCategoryIndex = 0;
	bool sSyncCategory = true;
	bool sFocusFilter = false;
	bool sRequestNewTabPicker = false;

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
	ImGui::PushStyleColor(ImGuiCol_Header, HelperTheme::Header);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.34f, 0.26f, 0.14f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, HelperTheme::TabActive);
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

/* ProggyClean lacks | - ... etc. Keep ImGui labels ASCII-only. */
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
		/* UTF-8 em/en dash -> '-' */
		if ((c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
				(static_cast<unsigned char>(src[i + 2]) == 0x94 ||
					static_cast<unsigned char>(src[i + 2]) == 0x93)))
		{
			dst[o++] = '-';
			i += 3;
			continue;
		}
		/* middle dot | */
		if (c == 0xC2 && static_cast<unsigned char>(src[i + 1]) == 0xB7)
		{
			dst[o++] = '-';
			i += 2;
			continue;
		}
		/* ellipsis ... */
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
		/* right arrow -> */
		if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x86 &&
			static_cast<unsigned char>(src[i + 2]) == 0x92)
		{
			if (o + 2 < dstLen)
			{
				dst[o++] = '-';
				dst[o++] = '>';
			}
			i += 3;
			continue;
		}
		/* multiplication sign x */
		if (c == 0xC3 && static_cast<unsigned char>(src[i + 1]) == 0x97)
		{
			dst[o++] = 'x';
			i += 2;
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
		col = ImGui::GetColorU32(hovered ? HelperTheme::Ink : kMuted);
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
	{
		Sites::ToggleFavorite(siteId);
		Settings::SaveNow();
		LivePanels::NotifyFavoritesChanged();
	}
}

/* Browse popup sized from the display - aspect-aware (16:9 / 21:9 / 32:9). */
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

	const AspectLayout::BrowsePopupSize spec =
		AspectLayout::DefaultBrowsePopup(dispW, dispH);
	const float width = spec.width;
	const float maxOuter = spec.maxOuter;
	const float listMax = pickDefaultSite ? spec.listMaxDefault : spec.listMaxPicker;

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
	lay.leftW = Clampf(width * 0.26f, spec.leftWMin, spec.leftWMax);

	sCacheDispW = dispW;
	sCacheDispH = dispH;
	sCacheFont = fontScale;
	sCacheBanner = withBanner;
	sCacheDefault = pickDefaultSite;
	sCache = lay;
	return lay;
}

/* Dropdown-style picker: pin under the button that opened it (not a free window). */

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

} // namespace UIBrowseDetail
