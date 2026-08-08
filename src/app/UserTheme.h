#pragma once

#include <string>
#include <vector>

/* User drop-in color themes under config/themes/<id>/theme.ini.
   Overrides HelperTheme ImGui tokens + emits a CSS :root block for helper pages. */
namespace UserTheme
{
	/* Create themes dir, README, and example theme if missing. */
	void EnsureSeed();

	/* Folder names that contain theme.ini (sorted). */
	std::vector<std::string> ListThemes();

	/* Apply theme id (empty / "default" = builtin). Returns false if folder missing. */
	bool Apply(const char* id);

	/* Re-apply G::ThemeId after EnsureSeed. */
	void Reload();

	/* Non-empty when a user theme is active — append after builtin RootVars. */
	const std::string& CssRootOverride();

	/* True when ThemeId is not default. */
	bool IsCustomActive();
}
