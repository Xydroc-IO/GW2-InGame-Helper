#pragma once

#include <string>

/* HTML body builders for LivePanels about: pages (internal). */
namespace LivePanelsBuild
{
	std::string BuildPage(const char* title, const char* eyebrow, const char* heading,
		const char* tagline, const char* toc, const std::string& body,
		const std::string& extraHead = {});

	std::string BuildDailiesHtml(const std::wstring& addonDir, const char* apiKey);
	std::string BuildNewsHtml();
	std::string BuildFashionHtml(const std::wstring& addonDir);
	std::string BuildTpHtml(const char* tpWatchIds, bool fetchApi);
	std::string BuildProgressHtml(const std::wstring& addonDir, const char* apiKey);
	std::string BuildApiCheckHtml(const char* apiKey);
	std::string BuildLegendaryLedgerHtml(const std::wstring& addonDir, const char* apiKey);
	std::string BuildLegendaryDetailHtml(const std::wstring& addonDir, const char* apiKey, int itemId);
	/* Instant Ledger-themed loading page (meta-refresh) while the craft tree builds. */
	std::string BuildLegendaryDetailShellHtml(int itemId);
	std::string BuildCheatSheetsHubHtml(const std::wstring& addonDir, const char* apiKey);
	std::string BuildBrowseHubHtml(const std::wstring& addonDir, const char* apiKey);
	std::string BuildBrowseCategoryHtml(const std::wstring& addonDir, const char* category);
	std::string BuildBrowseCategoryShellHtml(const char* category);
	/* Stable slug for about:browse-cat-<slug> / live-browse-cat-<slug>.html */
	std::string BrowseCategorySlug(const char* category);
	const char* BrowseCategoryFromSlug(const char* slug);
}
