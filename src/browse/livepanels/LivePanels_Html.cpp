#include "LivePanels_Html.h"

#include "Globals.h"
#include "HelperThemeCss.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace LivePanelsHtml
{
	const char* SharedCss()
	{
		static std::string sCss;
		static char sTheme[64]{};
		if (sCss.empty() || std::strcmp(sTheme, G::ThemeId) != 0)
		{
			std::snprintf(sTheme, sizeof(sTheme), "%s", G::ThemeId);
			sCss = HelperThemeCss::RootVars();
			HelperThemeCss::AppendUserRoot(sCss);
			sCss += HelperThemeCss::ImmersiveShell();
			sCss += R"CSS(
  .wrap { max-width: 900px; margin-left: auto; margin-right: auto; padding: 28px 22px 64px; }
  .hero {
    margin-bottom: 18px; padding: 1rem 1.1rem 1.1rem;
    background: linear-gradient(165deg, rgba(48, 38, 22, 0.4), transparent 55%), var(--panel-inset);
    border: 1px solid var(--border);
    box-shadow: inset 0 1px 0 rgba(255, 230, 160, 0.1), 0 10px 32px rgba(0, 0, 0, 0.4);
  }
  h1 { margin: 0 0 8px; font-size: 1.95rem; }
  .tagline { margin: 0; color: var(--muted); font-size: 0.98rem; }
  .meta { margin: 10px 0 0; font-size: 0.82rem; color: var(--muted); }
  nav.toc { display: flex; flex-wrap: wrap; gap: 8px; margin: 0 0 20px; }
  nav.toc a {
    color: var(--gold-dim); text-decoration: none; font-size: 0.82rem; letter-spacing: 0.03em;
    padding: 6px 10px; border: 1px solid var(--border-deep); background: var(--accent);
  }
  nav.toc a:hover { border-color: var(--gold); color: var(--gold-bright); background-image: none; }
  section.block { margin-bottom: 16px; }
  section.block > .head {
    padding: 12px 16px; border-bottom: 1px solid var(--border-soft); border-left: 3px solid var(--gold);
    background: linear-gradient(90deg, var(--header) 0%, var(--panel-2) 70%);
  }
  section.block > .head h2 { margin: 0; font-size: 1.12rem; color: var(--gold-bright); }
  section.block > .head p { margin: 4px 0 0; font-size: 0.88rem; color: var(--muted); }
  .body { padding: 14px 16px 16px; }
  .note {
    margin: 0 0 12px; padding: 10px 12px; background: var(--accent);
    border-left: 3px solid var(--gold-dim); color: var(--muted); font-size: 0.9rem;
  }
  .note strong { color: var(--text); }
  ul.rows { list-style: none; margin: 0; padding: 0; }
  ul.rows li {
    padding: 10px 12px; margin: 0 0 8px; background: var(--panel-2);
    border: 1px solid var(--border-soft); border-left: 3px solid var(--gold-dim);
  }
  ul.rows li.done { border-left-color: var(--ok); opacity: 0.85; }
  ul.rows .t { font-weight: 650; color: var(--text); }
  ul.rows .s { display: block; margin-top: 4px; font-size: 0.86rem; color: var(--muted); }
  .bar {
    margin-top: 8px; height: 8px; background: #0c0a07; border: 1px solid var(--border-soft);
  }
  .bar > i { display: block; height: 100%; background: var(--gold-dim); }
  .badge {
    display: inline-block; padding: 2px 7px; font-size: 0.7rem; font-weight: 700;
    letter-spacing: 0.05em; text-transform: uppercase; border: 1px solid var(--border);
    background: var(--accent); color: var(--gold-dim); margin-right: 6px;
  }
  a.link { color: var(--gold); }
  input#dyeFilter {
    width: 100%; padding: 10px 12px; margin: 0 0 12px; border: 1px solid var(--border);
    background: var(--accent); color: var(--text); font-size: 0.95rem;
  }
  .checks { list-style: none; margin: 0; padding: 0; }
  .checks li { margin: 0 0 8px; }
  .check { display: flex; gap: 10px; align-items: flex-start; cursor: pointer; }
  .check input { margin-top: 4px; flex-shrink: 0; }
  .check .box { display: none; }
  .check .txt { color: var(--muted); flex: 1; }
  .check .txt strong { color: var(--text); }
  .check input:checked + .box + .txt strong,
  .check:has(input:checked) .txt { opacity: 0.75; }
  .swatch {
    display: inline-block; width: 14px; height: 14px; border: 1px solid #000;
    vertical-align: -2px; margin-right: 6px;
  }
  .keybox {
    margin: 0 0 16px; padding: 14px 16px; border: 1px solid var(--border);
    background: linear-gradient(90deg, var(--header) 0%, var(--panel-2) 70%);
    box-shadow: inset 0 1px 0 rgba(255, 230, 160, 0.08);
  }
  .keybox.ok { border-left: 3px solid var(--ok); }
  .keybox.warn { border-left: 3px solid var(--warn); }
  .keybox h3 { margin: 0 0 6px; color: var(--gold-bright); font-size: 1.05rem; }
  .keybox p { margin: 0; color: var(--muted); font-size: 0.9rem; }
  .keybox strong { color: var(--text); }
  .muted { color: var(--muted); font-size: 0.86rem; }
  code { color: var(--gold-dim); font-size: 0.88rem; }
)CSS";
		}
		return sCss.c_str();
	}
}
