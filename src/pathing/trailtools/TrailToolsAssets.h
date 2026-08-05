#pragma once

#include <string>
#include <vector>

/* Pack-tab texture browser: authoring Markers/ + import from installed .taco. */
namespace TrailToolsAssets
{
	struct Entry
	{
		std::string relPath;   /* pack-relative e.g. Data/Pack/Markers/x.png */
		std::string label;     /* display */
		bool        fromTaco = false;
		std::string tacoName;  /* source pack filename when fromTaco */
		std::string zipEntry;  /* entry inside .taco */
	};

	void RefreshAuthoringList(std::vector<Entry>& out);
	void RefreshInstalledTacoList(std::vector<Entry>& out); /* PNG-like entries */
	/* Copy zipEntry from taco into authoring Markers/; returns new relative path. */
	bool ImportFromTaco(const std::wstring& tacoPath, const std::string& zipEntry,
		std::string& outRelPath, std::string& err);
	void DrawBrowserUi(); /* ImGui section for Pack tab */
}
