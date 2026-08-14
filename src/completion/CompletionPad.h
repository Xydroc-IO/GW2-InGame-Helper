#pragma once

namespace CompletionPad
{
	void OpenAndRefresh();
	void OpenAchievements();
	void ShowChecklistTab();
	bool ShowingAchievements();
	bool Render();
	/* Proximity auto-tick + GPS arrow - call every UI frame. */
	void Tick();
}
