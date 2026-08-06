#pragma once

/* CSS :root tokens mirrored from HelperTheme.h (dark-warm parchment + gold).
   Keep in sync when ImGui theme colors change. */

namespace HelperThemeCss
{
	/* Shared custom-property block for about:/file: pages. */
	inline const char* RootVars()
	{
		return R"CSS(
  :root {
    --bg: #120e0a;
    --panel: rgba(28, 23, 17, 0.94);
    --panel-2: #18140e;
    --panel-solid: #1c1711;
    --border: #8c6b33;
    --border-soft: rgba(240, 199, 97, 0.22);
    --gold: #f0c761;
    --gold-bright: #ffe68c;
    --gold-dim: #c29438;
    --gold-muted: #ccad6b;
    --text: #f5eddb;
    --muted: #b8ad94;
    --accent: #1a160f;
    --header: #47381f;
    --ink: rgba(18, 14, 10, 0.82);
    --ok: #85b86b;
    --warn: #eb8c47;
    --power: #c45c4a;
    --condi: #9b7bb8;
    --heal: #6aaa6a;
    --tank: #5a8fbf;
    --support: #c29438;
  }
)CSS";
	}

	/* Warm page wash used by most about: shells. */
	inline const char* BodyWash()
	{
		return
			"radial-gradient(ellipse 80% 55% at 50% 0%, rgba(240, 199, 97, 0.12) 0%, transparent 55%), "
			"linear-gradient(180deg, #1c1711 0%, var(--bg) 45%), var(--bg)";
	}
}
