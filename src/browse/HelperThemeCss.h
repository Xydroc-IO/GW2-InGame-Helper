#pragma once

#include "UserTheme.h"

#include <string>

/* CSS tokens + immersive shell mirrored from HelperTheme.h.
   Keep in sync when ImGui theme colors change.
   Aim: Tyrian map-board / leather plaque immersion — not a flat dashboard. */

namespace HelperThemeCss
{
	/* Append user :root overrides (if any) after builtin RootVars. */
	inline void AppendUserRoot(std::string& css)
	{
		const std::string& o = UserTheme::CssRootOverride();
		if (!o.empty())
			css += o;
	}

	/* Shared custom-property block for about:/file: pages. */
	inline const char* RootVars()
	{
		return R"CSS(
  :root {
    --bg: #0e0b08;
    --bg-mid: #16110c;
    --panel: rgba(32, 26, 18, 0.96);
    --panel-2: #1a1510;
    --panel-solid: #1f1912;
    --panel-inset: #14100c;
    --border: #a07838;
    --border-soft: rgba(232, 196, 112, 0.28);
    --border-deep: #5a4220;
    --gold: #f0c866;
    --gold-bright: #ffe9a0;
    --gold-dim: #c9a040;
    --gold-muted: #c4a86a;
    --text: #f7f0de;
    --muted: #d4c4a0;
    --accent: #16120c;
    --header: #3d3018;
    --ink: rgba(14, 11, 8, 0.88);
    --ok: #85b86b;
    --warn: #eb8c47;
    --power: #c45c4a;
    --condi: #9b7bb8;
    --heal: #6aaa6a;
    --tank: #5a8fbf;
    --support: #c29438;
    --font-ui: "Segoe UI", Tahoma, sans-serif;
    --font-display: Georgia, "Palatino Linotype", Palatino, "Times New Roman", serif;
  }
)CSS";
	}

	/* Warm parchment wash — layered glow + vignette (no image assets). */
	inline const char* BodyWash()
	{
		return
			"radial-gradient(ellipse 90% 60% at 50% -8%, rgba(232, 196, 112, 0.18) 0%, transparent 52%), "
			"radial-gradient(ellipse 70% 50% at 100% 100%, rgba(90, 55, 20, 0.35) 0%, transparent 55%), "
			"radial-gradient(ellipse 60% 45% at 0% 80%, rgba(40, 28, 14, 0.55) 0%, transparent 50%), "
			"linear-gradient(180deg, #1a1510 0%, var(--bg) 42%, #0a0806 100%)";
	}

	/* Gold Helper scrollbars — CEF/OSR theme (pads keep DAT ImGui chrome separately). */
	inline const char* ScrollbarCss()
	{
		return R"CSS(
  * {
    scrollbar-width: thin;
    scrollbar-color: #8a6a32 #120e0a;
  }
  *::-webkit-scrollbar { width: 11px; height: 11px; }
  *::-webkit-scrollbar-track {
    background: #120e0a;
    border-left: 1px solid #5a4220;
  }
  *::-webkit-scrollbar-thumb {
    background: linear-gradient(180deg, #a07838, #5a4220);
    border: 1px solid #c29438;
    border-radius: 0;
  }
  *::-webkit-scrollbar-thumb:hover {
    background: linear-gradient(180deg, #f0c866, #8a6a32);
  }
  *::-webkit-scrollbar-corner { background: #120e0a; }
)CSS";
	}

	/* Layer extracted ui-chrome panel fill under grain (file:/// URL).
	   Use a fixed inset layer — NOT background-attachment:fixed. CEF OSR leaves
	   a black void under short pages when attachment:fixed only covers the
	   document box; position:fixed always fills the browser viewport. */
	inline std::string FillBackgroundCss(const char* fillUrl)
	{
		if (!fillUrl || !fillUrl[0])
			return {};
		std::string s;
		s.reserve(1200);
		s += "\n  html, body {\n    min-height: 100vh;\n    margin: 0;\n"
			"    background-color: #0e0b08;\n  }\n"
			"  body {\n    background-color: #0e0b08;\n"
			"    background-image: none;\n  }\n"
			"  body::before {\n"
			"    content: \"\";\n"
			"    pointer-events: none;\n"
			"    position: fixed;\n"
			"    inset: 0;\n"
			"    z-index: 0;\n"
			"    opacity: 1;\n"
			"    background-color: #0e0b08;\n"
			"    background-image: url(\"";
		s += fillUrl;
		s += "\"),\n"
			"      radial-gradient(ellipse 90% 60% at 50% -8%, rgba(232, 196, 112, 0.10) 0%, transparent 52%),\n"
			"      radial-gradient(ellipse 70% 50% at 100% 100%, rgba(90, 55, 20, 0.45) 0%, transparent 55%),\n"
			"      linear-gradient(180deg, rgba(14, 11, 8, 0.55) 0%, rgba(8, 6, 4, 0.72) 42%, rgba(6, 4, 3, 0.85) 100%);\n"
			"    background-size: cover, auto, auto, auto;\n"
			"    background-position: center top, center, center, center;\n"
			"    background-repeat: no-repeat;\n"
			"  }\n"
			"  body::after { z-index: 0; }\n"
			"  body > * { position: relative; z-index: 1; }\n";
		/* Ledger / detail pages use Fill without ImmersiveShell — still need gold bars. */
		s += ScrollbarCss();
		return s;
	}

	/* Shared immersive chrome: grain overlay, gold scrollbars, display titles, plaques. */
	inline const char* ImmersiveShell()
	{
		return R"CSS(
  * { box-sizing: border-box; }
  html { scroll-behavior: smooth; }
  html, body {
    margin: 0;
    min-height: 100vh;
  }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: var(--font-ui);
    color: var(--text);
    line-height: 1.55;
    background-color: var(--bg);
    background-image: none;
    position: relative;
  }
  /* Viewport wash (gradients). FillBackgroundCss replaces ::before with panel art. */
  body::before {
    content: "";
    pointer-events: none;
    position: fixed;
    inset: 0;
    z-index: 0;
    background: radial-gradient(ellipse 90% 60% at 50% -8%, rgba(232, 196, 112, 0.18) 0%, transparent 52%),
      radial-gradient(ellipse 70% 50% at 100% 100%, rgba(90, 55, 20, 0.35) 0%, transparent 55%),
      radial-gradient(ellipse 60% 45% at 0% 80%, rgba(40, 28, 14, 0.55) 0%, transparent 50%),
      linear-gradient(180deg, #1a1510 0%, var(--bg) 42%, #0a0806 100%);
  }
  /* Soft film-grain without image files (CSS noise via repeating gradients). */
  body::after {
    content: "";
    pointer-events: none;
    position: fixed;
    inset: 0;
    z-index: 0;
    opacity: 0.07;
    background-image:
      repeating-linear-gradient(0deg, transparent, transparent 2px, rgba(0,0,0,.18) 2px, rgba(0,0,0,.18) 3px),
      repeating-linear-gradient(90deg, transparent, transparent 3px, rgba(255,220,140,.04) 3px, rgba(255,220,140,.04) 4px);
  }
  body > * { position: relative; z-index: 1; }

  /* Gold Helper scrollbars — FillBackgroundCss re-appends for ledger pages. */
  * {
    scrollbar-width: thin;
    scrollbar-color: #8a6a32 #120e0a;
  }
  *::-webkit-scrollbar { width: 11px; height: 11px; }
  *::-webkit-scrollbar-track {
    background: #120e0a;
    border-left: 1px solid var(--border-deep);
  }
  *::-webkit-scrollbar-thumb {
    background: linear-gradient(180deg, #a07838, #5a4220);
    border: 1px solid #c29438;
    border-radius: 0;
  }
  *::-webkit-scrollbar-thumb:hover {
    background: linear-gradient(180deg, var(--gold), #8a6a32);
  }
  *::-webkit-scrollbar-corner { background: #120e0a; }

  .eyebrow {
    margin: 0 0 8px;
    font-size: 0.72rem;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: var(--gold-dim);
    font-family: var(--font-ui);
  }
  h1, .display {
    font-family: var(--font-display);
    font-weight: 700;
    letter-spacing: 0.02em;
    color: var(--gold-bright);
    text-shadow: 0 1px 0 #000, 0 0 24px rgba(232, 196, 112, 0.18);
  }
  h2, h3 {
    font-family: var(--font-ui);
  }

  /* Double-ruled plaque panel (outer deep, inner soft gold).
     UiChrome::DecorCss upgrades these with curated texture fills when extracted. */
  .plaque, section.block, a.tile, .modal {
    background:
      linear-gradient(165deg, rgba(48, 38, 22, 0.55) 0%, transparent 42%),
      linear-gradient(180deg, var(--panel-solid), var(--panel-inset));
    border: 1px solid var(--border);
    box-shadow:
      inset 0 1px 0 rgba(255, 230, 160, 0.12),
      inset 0 0 0 1px rgba(0, 0, 0, 0.35),
      0 8px 28px rgba(0, 0, 0, 0.45);
    position: relative;
  }
  .hairline {
    height: 10px;
    border: 0;
    background: linear-gradient(90deg, transparent, var(--border), transparent);
    margin: 0.85rem 0;
  }
)CSS";
	}
}
