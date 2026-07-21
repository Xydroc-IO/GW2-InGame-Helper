#include "HomePage.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

/* Embedded via ld -r -b binary (see Makefile). */
extern "C" {
	extern const unsigned char _binary_home_logo_png_start[];
	extern const unsigned char _binary_home_logo_png_end[];
	extern const unsigned char _binary_home_cover_jpg_start[];
	extern const unsigned char _binary_home_cover_jpg_end[];
}

namespace
{
	static constexpr const char* kHomePageVersion = "12";

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::string PathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		return std::string("file://") + utf8;
	}

	bool WriteBytes(const std::wstring& path, const void* data, DWORD len)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, data, len, &written, nullptr);
		CloseHandle(out);
		return ok && written == len;
	}
}

const char* HomePage::Html()
{
	return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>GW2 In-Game Helper</title>
<style>
  :root {
    --bg: #08090c;
    --panel: #13151b;
    --panel-2: #191c24;
    --border: #4a3d22;
    --border-soft: rgba(235, 192, 71, 0.16);
    --gold: #ebc047;
    --gold-dim: #c9a227;
    --text: #e8eaed;
    --muted: #9aa0a8;
    --accent: #1c1810;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: "Segoe UI", Tahoma, sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.55;
  }

  .hero {
    position: relative;
    min-height: 220px;
    overflow: hidden;
    border-bottom: 1px solid var(--border);
  }
  .hero-bg {
    position: absolute;
    inset: 0;
    background:
      linear-gradient(105deg, rgba(8,9,12,0.92) 0%, rgba(8,9,12,0.72) 42%, rgba(8,9,12,0.35) 100%),
      url("home-cover.jpg") center / cover no-repeat;
  }
  .hero-frame {
    position: absolute;
    inset: 10px;
    border: 1px solid rgba(235, 192, 71, 0.28);
    pointer-events: none;
  }
  .hero-frame::before,
  .hero-frame::after {
    content: "";
    position: absolute;
    width: 18px;
    height: 18px;
    border: 1px solid var(--gold-dim);
  }
  .hero-frame::before { top: -1px; left: -1px; border-right: 0; border-bottom: 0; }
  .hero-frame::after { bottom: -1px; right: -1px; border-left: 0; border-top: 0; }
  .hero-inner {
    position: relative;
    z-index: 1;
    display: flex;
    align-items: center;
    gap: 20px;
    max-width: 860px;
    margin: 0 auto;
    padding: 28px 24px 32px;
  }
  .logo {
    width: 96px;
    height: 96px;
    flex-shrink: 0;
    border: 1px solid var(--border);
    background: rgba(10, 11, 14, 0.55);
    box-shadow: 0 0 0 1px rgba(235, 192, 71, 0.08), 0 8px 28px rgba(0,0,0,0.45);
  }
  .brand {
    min-width: 0;
  }
  .eyebrow {
    margin: 0 0 6px;
    font-size: 0.72rem;
    letter-spacing: 0.16em;
    text-transform: uppercase;
    color: var(--gold-dim);
  }
  h1 {
    margin: 0 0 8px;
    font-family: Georgia, "Palatino Linotype", Palatino, "Times New Roman", serif;
    font-size: 2rem;
    font-weight: 700;
    color: var(--gold);
    letter-spacing: 0.01em;
    text-shadow: 0 2px 18px rgba(0,0,0,0.55);
  }
  .tagline {
    margin: 0;
    max-width: 34rem;
    color: #c5c8ce;
    font-size: 1.02rem;
  }
  .rule {
    width: 72px;
    height: 2px;
    margin: 12px 0 0;
    background: linear-gradient(90deg, var(--gold), transparent);
  }

  .wrap {
    max-width: 860px;
    margin: 0 auto;
    padding: 22px 22px 56px;
  }
  section.block {
    background: var(--panel);
    border: 1px solid var(--border);
    margin-bottom: 14px;
  }
  section.block > .head {
    padding: 12px 16px;
    border-bottom: 1px solid var(--border-soft);
    background: linear-gradient(90deg, #1a1710 0%, var(--panel) 70%);
  }
  section.block > .head h2 {
    margin: 0;
    font-family: Georgia, "Palatino Linotype", Palatino, serif;
    font-size: 1.12rem;
    color: var(--gold);
    font-weight: 700;
  }
  .body { padding: 14px 16px 16px; }
  p { margin: 0 0 10px; }
  p:last-child { margin-bottom: 0; }
  ol, ul { margin: 6px 0 0; padding-left: 1.2em; }
  li { margin: 5px 0; }
  kbd {
    display: inline-block;
    padding: 1px 7px;
    border: 1px solid var(--border);
    background: var(--accent);
    color: var(--gold);
    font-family: Consolas, "Courier New", monospace;
    font-size: 0.88em;
  }
  .muted { color: var(--muted); font-size: 0.92rem; }
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-top: 4px;
  }
  .card {
    padding: 12px 12px 11px;
    background: var(--panel-2);
    border: 1px solid var(--border-soft);
  }
  .cat {
    display: block;
    margin-bottom: 4px;
    color: var(--gold);
    font-weight: 650;
    font-size: 0.86rem;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }
  .card span:last-child {
    color: var(--muted);
    font-size: 0.9rem;
    line-height: 1.4;
  }
  footer {
    margin-top: 18px;
    padding-top: 14px;
    border-top: 1px solid var(--border-soft);
    color: var(--muted);
    font-size: 0.85rem;
  }
  @media (max-width: 560px) {
    .hero-inner { flex-direction: column; align-items: flex-start; gap: 14px; }
    h1 { font-size: 1.55rem; }
    .logo { width: 80px; height: 80px; }
    .grid { grid-template-columns: 1fr; }
  }
</style>
</head>
<body>
  <header class="hero">
    <div class="hero-bg" aria-hidden="true"></div>
    <div class="hero-frame" aria-hidden="true"></div>
    <div class="hero-inner">
      <img class="logo" src="home-logo.png" width="96" height="96" alt="GW2 In-Game Helper logo"/>
      <div class="brand">
        <p class="eyebrow">Raidcore Nexus Addon</p>
        <h1>GW2 In-Game Helper</h1>
        <p class="tagline">Browse useful Guild Wars 2 sites and Discords without leaving the game.</p>
        <div class="rule" aria-hidden="true"></div>
      </div>
    </div>
  </header>

  <main class="wrap">
    <section class="block">
      <div class="head"><h2>Quick start</h2></div>
      <div class="body">
        <ol>
          <li>Use the <strong>site dropdown</strong> in the toolbar (grouped by category).</li>
          <li>Click inside the page to scroll, click links, and type.</li>
          <li>Click <strong>outside</strong> this window to move and use skills again — you do not need to close the helper.</li>
        </ol>
      </div>
    </section>

    <section class="block">
      <div class="head"><h2>Hotkeys</h2></div>
      <div class="body">
        <ul>
          <li><kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>H</kbd> (or <kbd>K</kbd>) — open / close this helper</li>
          <li>Or click the helper icon in the Nexus QuickAccess bar at the top of the screen</li>
        </ul>
        <p class="muted">Rebind the toggle in Nexus (<kbd>Ctrl</kbd>+<kbd>O</kbd>) under <strong>KB_HELPER_TOGGLE</strong>.</p>
      </div>
    </section>

    <section class="block">
      <div class="head"><h2>Toolbar</h2></div>
      <div class="body">
        <ul>
          <li><strong>Site picker</strong> — switch between Wiki, builds, tools, guides, and more</li>
          <li><strong>Back / Forward</strong> — browser history</li>
          <li><strong>Home</strong> — return to this page</li>
          <li><strong>Reload</strong> — refresh the current page</li>
          <li><strong>Copy URL</strong> — copy the current page link</li>
          <li><strong>Open Ext</strong> — open in your system browser (best for Discord joins)</li>
        </ul>
      </div>
    </section>

    <section class="block">
      <div class="head"><h2>Sites by category</h2></div>
      <div class="body">
        <div class="grid">
          <div class="card"><span class="cat">Help</span><span>This page</span></div>
          <div class="card"><span class="cat">Official</span><span>Guild Wars 2, GW2 News, Raidcore</span></div>
          <div class="card"><span class="cat">Wiki</span><span>Wiki, Game Updates, Legendaries, Mounts</span></div>
          <div class="card"><span class="cat">Builds</span><span>Snowcrows, MetaBattle, MetaBattle OW, Accessibility Wars, Gw2Skills Editor</span></div>
          <div class="card"><span class="cat">PvP / WvW</span><span>MetaBattle PvP, MetaBattle WvW</span></div>
          <div class="card"><span class="cat">Tools</span><span>Efficiency, Legendary Tracker, Blish HUD, Timers, Crafts, Music Box, Peu</span></div>
          <div class="card"><span class="cat">Guides</span><span>Raid Food, Lucky Noobs, Living World, Leveling, Gold, Mounts, Fractals, TLDR</span></div>
          <div class="card"><span class="cat">Farming</span><span>Fast Farming Community</span></div>
          <div class="card"><span class="cat">Discord</span><span>Official, Community, builds, training, PvP, WvW, trading, Raidcore</span></div>
        </div>
      </div>
    </section>

    <section class="block">
      <div class="head"><h2>Tips</h2></div>
      <div class="body">
        <ul>
          <li>Opacity and font scale are in the addon’s Nexus options.</li>
          <li>Opening the helper always lands on this page first.</li>
          <li>Use <strong>Open Ext</strong> for Discord invites and pages that need a full browser login.</li>
        </ul>
      </div>
    </section>

    <footer>Not affiliated with ArenaNet, NCSoft, or the listed community sites. Informational overlay only.</footer>
  </main>
</body>
</html>
)HTML";
}

std::string HomePage::EnsureFileUrl(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return {};

	CreateDirectoryW(addonDir.c_str(), nullptr);
	const std::wstring path = addonDir + L"\\helper-home.html";
	const std::wstring verPath = addonDir + L"\\helper-home.ver";
	const std::wstring logoPath = addonDir + L"\\home-logo.png";
	const std::wstring coverPath = addonDir + L"\\home-cover.jpg";

	bool needWrite = true;
	HANDLE verFile = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (verFile != INVALID_HANDLE_VALUE)
	{
		char buf[32]{};
		DWORD read = 0;
		if (ReadFile(verFile, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
		{
			buf[read] = 0;
			while (read > 0 && (buf[read - 1] == '\n' || buf[read - 1] == '\r'))
				buf[--read] = 0;
			if (std::strcmp(buf, kHomePageVersion) == 0)
				needWrite = false;
		}
		CloseHandle(verFile);
	}

	const bool missingAssets =
		GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES ||
		GetFileAttributesW(logoPath.c_str()) == INVALID_FILE_ATTRIBUTES ||
		GetFileAttributesW(coverPath.c_str()) == INVALID_FILE_ATTRIBUTES;

	if (needWrite || missingAssets)
	{
		const char* html = Html();
		const DWORD len = static_cast<DWORD>(std::strlen(html));
		if (!WriteBytes(path, html, len))
			return {};

		const DWORD logoLen = static_cast<DWORD>(_binary_home_logo_png_end - _binary_home_logo_png_start);
		const DWORD coverLen = static_cast<DWORD>(_binary_home_cover_jpg_end - _binary_home_cover_jpg_start);
		if (!WriteBytes(logoPath, _binary_home_logo_png_start, logoLen))
			return {};
		if (!WriteBytes(coverPath, _binary_home_cover_jpg_start, coverLen))
			return {};

		WriteBytes(verPath, kHomePageVersion, static_cast<DWORD>(std::strlen(kHomePageVersion)));
	}

	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return {};
	return PathToFileUrl(path);
}
