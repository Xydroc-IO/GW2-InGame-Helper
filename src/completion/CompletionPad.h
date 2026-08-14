#pragma once

namespace CompletionPad
{
	void OpenAndRefresh();
	void OpenAchievements();
	bool Render();
	bool RenderAchievements();
	/* Proximity auto-tick + GPS arrow - call every UI frame. */
	void Tick();
}
