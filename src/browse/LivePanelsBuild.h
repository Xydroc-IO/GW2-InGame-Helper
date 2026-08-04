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
}
