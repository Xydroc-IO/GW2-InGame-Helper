#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "UI.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"
#include "Sites.h"
#include "LivePanels.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace UIBrowseDetail;

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
	G::ShowWiki = true;
	Settings::SetDirty();
	WikiBrowser::Navigate("about:browse-hub");
}

void UI_Browse_DrawMainPopup()
{
	/* Side-rail Browse opens the HTML hub; ImGui popup retained unused. */
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
		Settings::SaveNow();
		LivePanels::NotifyFavoritesChanged();
		return true;
	}
	return false;
}
