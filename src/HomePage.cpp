#include "HomePage.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	static constexpr const char* kHomePageVersion = "7";

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
    --bg: #0d0e11;
    --panel: #14161c;
    --border: #473c24;
    --gold: #ebc047;
    --text: #e8eaed;
    --muted: #9aa0a8;
    --accent: #2a2418;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: "Segoe UI", Tahoma, sans-serif;
    background:
      radial-gradient(ellipse at top, #1a1810 0%, var(--bg) 55%),
      var(--bg);
    color: var(--text);
    line-height: 1.55;
  }
  main {
    max-width: 720px;
    margin: 0 auto;
    padding: 36px 28px 56px;
  }
  h1 {
    margin: 0 0 8px;
    font-size: 1.85rem;
    font-weight: 650;
    color: var(--gold);
    letter-spacing: 0.02em;
  }
  .tagline {
    margin: 0 0 28px;
    color: var(--muted);
    font-size: 1.05rem;
  }
  section {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 18px 20px;
    margin-bottom: 16px;
  }
  h2 {
    margin: 0 0 10px;
    font-size: 1.05rem;
    color: var(--gold);
    font-weight: 600;
  }
  p { margin: 0 0 10px; }
  p:last-child { margin-bottom: 0; }
  ol, ul { margin: 8px 0 0; padding-left: 1.2em; }
  li { margin: 4px 0; }
  kbd {
    display: inline-block;
    padding: 1px 7px;
    border-radius: 4px;
    border: 1px solid var(--border);
    background: var(--accent);
    color: var(--gold);
    font-family: inherit;
    font-size: 0.92em;
  }
  .muted { color: var(--muted); }
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px 20px;
    margin-top: 8px;
  }
  @media (max-width: 560px) {
    .grid { grid-template-columns: 1fr; }
  }
  .cat { color: var(--gold); font-weight: 600; }
  footer {
    margin-top: 22px;
    color: var(--muted);
    font-size: 0.9rem;
  }
</style>
</head>
<body>
<main>
  <h1>GW2 In-Game Helper</h1>
  <p class="tagline">Browse useful Guild Wars 2 sites without leaving the game.</p>

  <section>
    <h2>Quick start</h2>
    <ol>
      <li>Use the <strong>site dropdown</strong> in the toolbar (grouped by category).</li>
      <li>Click inside the page to scroll, click links, and type.</li>
      <li>Click <strong>outside</strong> this window to move and use skills again — you do not need to close the helper.</li>
    </ol>
  </section>

  <section>
    <h2>Hotkeys</h2>
    <ul>
      <li><kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>H</kbd> (or <kbd>K</kbd>) — open / close this helper</li>
      <li>Or click the helper icon in the Nexus QuickAccess bar at the top of the screen</li>
    </ul>
    <p class="muted">Rebind the toggle in Nexus (<kbd>Ctrl</kbd>+<kbd>O</kbd>) under <strong>KB_HELPER_TOGGLE</strong>.</p>
    <p class="muted">Item lookup hotkeys were removed — they interfered with mounts and chat.</p>
  </section>

  <section>
    <h2>Toolbar</h2>
    <ul>
      <li><strong>Site picker</strong> — switch between Wiki, builds, tools, and farming sites</li>
      <li><strong>Back / Forward</strong> — browser history</li>
      <li><strong>Home</strong> — return to this how-to page</li>
      <li><strong>Reload</strong> — refresh the current page</li>
    </ul>
  </section>

  <section>
    <h2>Sites by category</h2>
    <div class="grid">
      <div><span class="cat">Help</span><br/>This page</div>
      <div><span class="cat">Official</span><br/>Guild Wars 2, Raidcore</div>
      <div><span class="cat">Wiki</span><br/>Official Guild Wars 2 Wiki</div>
      <div><span class="cat">Builds</span><br/>Snowcrows, MetaBattle, Accessibility Wars</div>
      <div><span class="cat">Tools</span><br/>gw2efficiency, GW2Timer Map, GW2 Crafts, Music Box</div>
      <div><span class="cat">Guides</span><br/>Guildjen</div>
      <div><span class="cat">Farming</span><br/>Fast Farming Community</div>
      <div><span class="cat">Discord</span><br/>Fractal Training, Raidcore, Raid Academy</div>
    </div>
  </section>

  <section>
    <h2>Tips</h2>
    <ul>
      <li>Opacity and font scale are in the addon’s Nexus options.</li>
      <li>Opening the helper always lands on this page first.</li>
      <li>Discord invite pages open in the browser; you may need the Discord app to finish joining.</li>
    </ul>
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

	if (needWrite || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		const char* html = Html();
		const DWORD len = static_cast<DWORD>(std::strlen(html));
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return {};
		DWORD written = 0;
		const BOOL ok = WriteFile(out, html, len, &written, nullptr);
		CloseHandle(out);
		if (!ok || written != len)
			return {};

		HANDLE vout = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (vout != INVALID_HANDLE_VALUE)
		{
			DWORD vw = 0;
			WriteFile(vout, kHomePageVersion, static_cast<DWORD>(std::strlen(kHomePageVersion)), &vw, nullptr);
			CloseHandle(vout);
		}
	}

	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		return {};
	return PathToFileUrl(path);
}
