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
	static constexpr const char* kHomePageVersion = "15";

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
    --bg: #06070a;
    --panel: rgba(16, 18, 24, 0.88);
    --panel-solid: #12141a;
    --border: #5a4a28;
    --border-soft: rgba(235, 192, 71, 0.22);
    --gold: #f0c65a;
    --gold-bright: #ffe08a;
    --gold-dim: #c9a227;
    --text: #f0f2f5;
    --muted: #a8aeb8;
    --accent: #1a1510;
    --ink: rgba(6, 7, 10, 0.78);
  }
  * { box-sizing: border-box; }
  html { scroll-behavior: smooth; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: "Segoe UI", Tahoma, sans-serif;
    color: var(--text);
    line-height: 1.55;
    background: var(--bg);
    overflow-x: hidden;
  }

  /* Full-page atmosphere — cover art + vignette */
  .page-bg {
    position: fixed;
    inset: 0;
    z-index: 0;
    background:
      radial-gradient(ellipse 80% 55% at 50% 0%, rgba(235, 192, 71, 0.14) 0%, transparent 55%),
      linear-gradient(180deg, rgba(6,7,10,0.55) 0%, rgba(6,7,10,0.82) 42%, rgba(6,7,10,0.94) 100%),
      url("home-cover.jpg") center top / cover no-repeat;
    animation: bgDrift 28s ease-in-out infinite alternate;
  }
  .page-bg::after {
    content: "";
    position: absolute;
    inset: 0;
    background:
      repeating-linear-gradient(
        0deg,
        transparent,
        transparent 3px,
        rgba(0,0,0,0.03) 3px,
        rgba(0,0,0,0.03) 4px
      );
    pointer-events: none;
    opacity: 0.35;
  }
  @keyframes bgDrift {
    from { background-position: center top; }
    to { background-position: center 8%; }
  }

  .ornament-frame {
    position: fixed;
    inset: 10px;
    z-index: 1;
    border: 1px solid rgba(235, 192, 71, 0.28);
    pointer-events: none;
    box-shadow:
      inset 0 0 80px rgba(235, 192, 71, 0.04),
      0 0 40px rgba(0,0,0,0.4);
  }
  .ornament-frame::before,
  .ornament-frame::after,
  .ornament-frame .c3,
  .ornament-frame .c4 {
    content: "";
    position: absolute;
    width: 28px;
    height: 28px;
    border-color: var(--gold-dim);
    border-style: solid;
  }
  .ornament-frame::before { top: -1px; left: -1px; border-width: 2px 0 0 2px; }
  .ornament-frame::after { top: -1px; right: -1px; border-width: 2px 2px 0 0; }
  .ornament-frame .c3 { bottom: -1px; left: -1px; border-width: 0 0 2px 2px; }
  .ornament-frame .c4 { bottom: -1px; right: -1px; border-width: 0 2px 2px 0; }

  .page {
    position: relative;
    z-index: 2;
    max-width: 900px;
    margin: 0 auto;
    padding: 28px 22px 48px;
    animation: riseIn 0.7s ease-out both;
  }
  @keyframes riseIn {
    from { opacity: 0; transform: translateY(14px); }
    to { opacity: 1; transform: none; }
  }

  .hero {
    position: relative;
    display: flex;
    align-items: center;
    gap: 22px;
    padding: 28px 26px 30px;
    margin-bottom: 22px;
    background:
      linear-gradient(135deg, rgba(26, 22, 14, 0.92) 0%, rgba(12, 14, 18, 0.75) 55%, rgba(12, 14, 18, 0.55) 100%);
    border: 1px solid var(--border);
    box-shadow:
      0 0 0 1px rgba(235, 192, 71, 0.08),
      0 18px 50px rgba(0,0,0,0.45);
    overflow: hidden;
  }
  .hero::before {
    content: "";
    position: absolute;
    inset: 0;
    background:
      radial-gradient(circle at 85% 40%, rgba(235, 192, 71, 0.12) 0%, transparent 45%);
    pointer-events: none;
  }
  .hero::after {
    content: "";
    position: absolute;
    left: 0; right: 0; bottom: 0;
    height: 2px;
    background: linear-gradient(90deg, transparent, var(--gold), transparent);
    animation: shimmer 3.2s ease-in-out infinite;
  }
  @keyframes shimmer {
    0%, 100% { opacity: 0.35; }
    50% { opacity: 1; }
  }

  .logo-wrap {
    position: relative;
    flex-shrink: 0;
  }
  .logo {
    width: 118px;
    height: 118px;
    display: block;
    border: 1px solid var(--border);
    background: rgba(8, 9, 12, 0.7);
    box-shadow:
      0 0 0 1px rgba(235, 192, 71, 0.12),
      0 0 28px rgba(235, 192, 71, 0.18),
      0 12px 32px rgba(0,0,0,0.5);
    animation: logoGlow 4s ease-in-out infinite;
  }
  @keyframes logoGlow {
    0%, 100% { box-shadow: 0 0 0 1px rgba(235,192,71,0.12), 0 0 22px rgba(235,192,71,0.14), 0 12px 32px rgba(0,0,0,0.5); }
    50% { box-shadow: 0 0 0 1px rgba(235,192,71,0.28), 0 0 36px rgba(235,192,71,0.28), 0 12px 32px rgba(0,0,0,0.5); }
  }

  .brand { position: relative; min-width: 0; }
  .eyebrow {
    margin: 0 0 8px;
    font-size: 0.74rem;
    letter-spacing: 0.2em;
    text-transform: uppercase;
    color: var(--gold-dim);
    font-weight: 600;
  }
  h1 {
    margin: 0 0 10px;
    font-family: Georgia, "Palatino Linotype", Palatino, "Times New Roman", serif;
    font-size: clamp(1.75rem, 4vw, 2.45rem);
    font-weight: 700;
    color: var(--gold-bright);
    letter-spacing: 0.02em;
    text-shadow:
      0 0 24px rgba(235, 192, 71, 0.35),
      0 2px 12px rgba(0,0,0,0.65);
    line-height: 1.15;
  }
  .tagline {
    margin: 0;
    max-width: 36rem;
    color: #d4d8de;
    font-size: 1.06rem;
    text-shadow: 0 1px 8px rgba(0,0,0,0.6);
  }
  .rule {
    width: 88px;
    height: 3px;
    margin: 14px 0 0;
    background: linear-gradient(90deg, var(--gold-bright), var(--gold-dim), transparent);
    border-radius: 1px;
  }
  .cta-row {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-top: 16px;
  }
  .pill {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    border: 1px solid var(--border);
    background: rgba(26, 21, 14, 0.85);
    color: var(--gold);
    font-size: 0.82rem;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    font-weight: 650;
  }

  section.block {
    background: var(--panel);
    border: 1px solid var(--border);
    margin-bottom: 14px;
    backdrop-filter: blur(6px);
    box-shadow: 0 10px 28px rgba(0,0,0,0.28);
    animation: riseIn 0.75s ease-out both;
  }
  section.block:nth-of-type(1) { animation-delay: 0.08s; }
  section.block:nth-of-type(2) { animation-delay: 0.14s; }
  section.block:nth-of-type(3) { animation-delay: 0.2s; }
  section.block:nth-of-type(4) { animation-delay: 0.26s; }
  section.block:nth-of-type(5) { animation-delay: 0.32s; }

  section.block > .head {
    position: relative;
    padding: 13px 18px;
    border-bottom: 1px solid var(--border-soft);
    background: linear-gradient(90deg, rgba(42, 34, 18, 0.95) 0%, rgba(18, 20, 26, 0.9) 65%);
  }
  section.block > .head::before {
    content: "";
    position: absolute;
    left: 0; top: 0; bottom: 0;
    width: 3px;
    background: linear-gradient(180deg, var(--gold-bright), var(--gold-dim));
  }
  section.block > .head h2 {
    margin: 0;
    font-family: Georgia, "Palatino Linotype", Palatino, serif;
    font-size: 1.18rem;
    color: var(--gold);
    font-weight: 700;
    letter-spacing: 0.02em;
  }
  .body { padding: 16px 18px 18px; }
  p { margin: 0 0 10px; }
  p:last-child { margin-bottom: 0; }
  ol, ul { margin: 6px 0 0; padding-left: 1.25em; }
  li { margin: 6px 0; }
  strong { color: var(--gold-bright); font-weight: 650; }
  kbd {
    display: inline-block;
    padding: 2px 8px;
    border: 1px solid var(--border);
    background: var(--accent);
    color: var(--gold);
    font-family: Consolas, "Courier New", monospace;
    font-size: 0.88em;
    box-shadow: inset 0 -1px 0 rgba(0,0,0,0.35);
  }
  .muted { color: var(--muted); font-size: 0.92rem; }

  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-top: 6px;
  }
  .card {
    position: relative;
    padding: 14px 14px 12px;
    background: linear-gradient(160deg, rgba(28, 24, 16, 0.9) 0%, rgba(18, 20, 26, 0.95) 100%);
    border: 1px solid var(--border-soft);
    transition: border-color 0.2s ease, transform 0.2s ease;
  }
  .card:hover {
    border-color: rgba(235, 192, 71, 0.45);
    transform: translateY(-1px);
  }
  .card::after {
    content: "";
    position: absolute;
    top: 0; right: 0;
    width: 18px; height: 18px;
    border-top: 1px solid rgba(235, 192, 71, 0.35);
    border-right: 1px solid rgba(235, 192, 71, 0.35);
  }
  .cat {
    display: block;
    margin-bottom: 5px;
    color: var(--gold);
    font-weight: 700;
    font-size: 0.78rem;
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }
  .card span:last-child {
    color: var(--muted);
    font-size: 0.9rem;
    line-height: 1.4;
  }

  footer {
    margin-top: 20px;
    padding: 16px 4px 8px;
    border-top: 1px solid var(--border-soft);
    color: var(--muted);
    font-size: 0.85rem;
    text-align: center;
  }
  footer .sig {
    display: block;
    margin-top: 6px;
    color: var(--gold-dim);
    letter-spacing: 0.12em;
    text-transform: uppercase;
    font-size: 0.72rem;
  }

  @media (max-width: 620px) {
    .hero { flex-direction: column; align-items: flex-start; }
    .logo { width: 92px; height: 92px; }
    .grid { grid-template-columns: 1fr; }
    .ornament-frame { inset: 6px; }
  }
</style>
</head>
<body>
  <div class="page-bg" aria-hidden="true"></div>
  <div class="ornament-frame" aria-hidden="true"><span class="c3"></span><span class="c4"></span></div>

  <div class="page">
    <header class="hero">
      <div class="logo-wrap">
        <img class="logo" src="home-logo.png" width="118" height="118" alt="GW2 In-Game Helper logo"/>
      </div>
      <div class="brand">
        <p class="eyebrow">Raidcore Nexus · In-Game Browser</p>
        <h1>GW2 In-Game Helper</h1>
        <p class="tagline">Wiki, builds, timers, guides, and Discords — without leaving Tyria.</p>
        <div class="rule" aria-hidden="true"></div>
        <div class="cta-row">
          <span class="pill">Browse</span>
          <span class="pill">Favorites</span>
          <span class="pill">Ctrl+Shift+H</span>
          <span class="pill">One DLL</span>
        </div>
      </div>
    </header>

    <main>
      <section class="block">
        <div class="head"><h2>Quick start</h2></div>
        <div class="body">
          <ol>
            <li>Open <strong>Browse</strong> — search or pick a category, then a site.</li>
            <li><strong>Ctrl+click</strong> a site (or use <strong>+</strong>) to open another tab — swap tabs anytime.</li>
            <li>Star sites with the <strong>star</strong> button (toolbar or Browse rows) to pin them under <strong>Favorites</strong>.</li>
            <li>Click inside the page to scroll, follow links, and type.</li>
            <li>Click <strong>outside</strong> this window to move and use skills again — you do not need to close the helper.</li>
          </ol>
        </div>
      </section>

      <section class="block">
        <div class="head"><h2>Hotkeys</h2></div>
        <div class="body">
          <ul>
            <li><kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>H</kbd> (or <kbd>K</kbd>) — open / close this helper</li>
            <li>Or click the helper icon in the Nexus QuickAccess bar</li>
          </ul>
          <p class="muted">Rebind in Nexus (<kbd>Ctrl</kbd>+<kbd>O</kbd>) under <strong>KB_HELPER_TOGGLE</strong>.</p>
        </div>
      </section>

      <section class="block">
        <div class="head"><h2>Toolbar</h2></div>
        <div class="body">
          <ul>
            <li><strong>Browse</strong> — search and pick sites by category</li>
            <li><strong>Tabs</strong> — keep up to 8 sites open; <strong>+</strong> or Ctrl+click opens a new tab</li>
            <li><strong>Star</strong> — add or remove the current site from Favorites</li>
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
            <div class="card"><span class="cat">Guides</span><span>Raid Food, Living World, Leveling, Gold, Mounts, Fractals, TLDR</span></div>
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
    </main>

    <footer>
      Not affiliated with ArenaNet, NCSoft, or the listed community sites. Informational overlay only.
      <span class="sig">GW2 In-Game Helper</span>
    </footer>
  </div>
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
