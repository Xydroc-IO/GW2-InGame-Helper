#pragma once

/* Local session snapshots of unlock + inventory totals (no GW2 session API). */
namespace SessionHistoryData
{
	void Load();
	void Save(bool force = false);
	void Tick();

	void RenderOverviewSnippet();
	void RenderContents();
}
