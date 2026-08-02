#include "WikiBrowserShared.h"

#include "AddonPaths.h"
#include "CheatSheets.h"
#include "Globals.h"
#include "HomePage.h"
#include "LivePanels.h"
#include "RaidFood.h"
#include "Sites.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace WikiBrowserDetail
{
	void WakeHelper()
	{
		if (gWakeEvent)
			SetEvent(gWakeEvent);
	}

	void RefreshUrlCache()
	{
		if (!gIpc)
		{
			gUrlCache[0] = 0;
			gUrlCacheSeq = 0xFFFFFFFFu;
			return;
		}
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const uint32_t s1 = gIpc->url_seq;
			if (s1 == gUrlCacheSeq && (s1 & 1u) == 0)
				return;
			if (s1 & 1u)
				continue;
			uint32_t n = gIpc->url_len;
			if (n >= sizeof(gIpc->url))
				n = static_cast<uint32_t>(sizeof(gIpc->url) - 1);
			if (n >= sizeof(gUrlCache))
				n = static_cast<uint32_t>(sizeof(gUrlCache) - 1);
			char tmp[sizeof(gUrlCache)];
			std::memcpy(tmp, gIpc->url, n);
			tmp[n] = 0;
			if (gIpc->url_seq != s1)
				continue;
			std::memcpy(gUrlCache, tmp, n + 1);
			gUrlCacheSeq = s1;
			return;
		}
	}

	void RefreshTitleCache()
	{
		if (!gIpc)
		{
			gTitleCache[0] = 0;
			gTitleCacheSeq = 0xFFFFFFFFu;
			return;
		}
		for (int attempt = 0; attempt < 4; ++attempt)
		{
			const uint32_t s1 = gIpc->title_seq;
			if (s1 == gTitleCacheSeq && (s1 & 1u) == 0)
				return;
			if (s1 & 1u)
				continue;
			uint32_t n = gIpc->title_len;
			if (n >= sizeof(gIpc->title))
				n = static_cast<uint32_t>(sizeof(gIpc->title) - 1);
			if (n >= sizeof(gTitleCache))
				n = static_cast<uint32_t>(sizeof(gTitleCache) - 1);
			char tmp[sizeof(gTitleCache)];
			std::memcpy(tmp, gIpc->title, n);
			tmp[n] = 0;
			if (gIpc->title_seq != s1)
				continue;
			std::memcpy(gTitleCache, tmp, n + 1);
			gTitleCacheSeq = s1;
			return;
		}
	}

	std::wstring Utf8ToWide(const std::string& utf8)
	{
		if (utf8.empty())
			return L"";
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
		if (n > 0)
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), n);
		return out;
	}

	void SetLocalStatus(const std::string& s)
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gStatus = s;
		std::snprintf(gStatusCache, sizeof(gStatusCache), "%s", s.c_str());
		gStatusCacheFromIpc = false;
		/* Keep IPC status in sync so StatusCStr has one logical source. */
		if (gIpc)
			std::snprintf(gIpc->status, sizeof(gIpc->status), "%s", s.c_str());
		if (G::API && G::API->Log)
			G::API->Log(LOGL_INFO, ADDON_NAME, s.c_str());
	}

	std::wstring AddonDir()
	{
		return AddonPaths::DataDir();
	}

	/* Private CEF only — never use game bin64/cef (headers are CEF 150). */
	std::wstring CefDir()
	{
		const std::wstring addon = AddonDir();
		if (addon.empty())
			return L"";
		return addon + L"\\cef";
	}

	std::wstring HelperPath()
	{
		return AddonDir() + L"\\GW2HelperBrowser.exe";
	}
	void QueuePendingCmd(WikiIpcCmd cmd, const char* arg, int32_t a)
	{
		for (int i = 0; i < gPendingCmdCount; ++i)
		{
			if (gPendingCmds[i].cmd == cmd &&
				(cmd == WIKI_CMD_SET_VISIBLE ||
				 ((gPendingCmds[i].a == a) &&
				  (cmd == WIKI_CMD_ACTIVATE_TAB || cmd == WIKI_CMD_CREATE_TAB ||
				   cmd == WIKI_CMD_CLOSE_TAB))))
			{
				gPendingCmds[i].a = a;
				std::snprintf(gPendingCmds[i].arg, sizeof(gPendingCmds[i].arg), "%s",
					arg ? arg : "");
				return;
			}
		}
		if (gPendingCmdCount >= kPendingCmdMax)
			return;
		PendingCmd& p = gPendingCmds[gPendingCmdCount++];
		p.cmd = cmd;
		p.a = a;
		std::snprintf(p.arg, sizeof(p.arg), "%s", arg ? arg : "");
	}

	bool TryPostCmdImmediate(WikiIpcCmd cmd, const char* arg, int32_t a)
	{
		if (!gIpc || !HelperAlive())
			return false;
		const uint32_t w = gIpc->cmd_write;
		const uint32_t next = (w + 1u) % kWikiCmdQueueSize;
		if (next == gIpc->cmd_read)
			return false;
		WikiCmdEvent& ev = gIpc->cmd_q[w % kWikiCmdQueueSize];
		ev.cmd = cmd;
		ev.a = a;
		std::snprintf(ev.arg, sizeof(ev.arg), "%s", arg ? arg : "");
		gIpc->cmd_write = next;
		std::snprintf(gIpc->cmd_arg, sizeof(gIpc->cmd_arg), "%s", arg ? arg : "");
		gIpc->cmd_a = a;
		gIpc->cmd = cmd;
		WakeHelper();
		return true;
	}

	void FlushPendingCmds()
	{
		while (gPendingCmdCount > 0)
		{
			const PendingCmd& p = gPendingCmds[0];
			if (!TryPostCmdImmediate(p.cmd, p.arg, p.a))
			{
				WakeHelper();
				return;
			}
			--gPendingCmdCount;
			if (gPendingCmdCount > 0)
			{
				std::memmove(&gPendingCmds[0], &gPendingCmds[1],
					static_cast<size_t>(gPendingCmdCount) * sizeof(PendingCmd));
			}
		}
	}

	void PostCmd(WikiIpcCmd cmd, const char* arg, int32_t a)
	{
		if (!gIpc)
			return;
		if (!HelperAlive())
		{
			/* Helper still launching — keep visibility / tab lifecycle for FlushPendingCmds. */
			if (cmd == WIKI_CMD_SET_VISIBLE ||
				cmd == WIKI_CMD_CREATE_TAB ||
				cmd == WIKI_CMD_ACTIVATE_TAB ||
				cmd == WIKI_CMD_CLOSE_TAB)
				QueuePendingCmd(cmd, arg, a);
			return;
		}

		FlushPendingCmds();
		if (TryPostCmdImmediate(cmd, arg, a))
			return;

		/* Ring full: SET_BOUNDS is coalesced in view_w/h already — drop.
		   Tab lifecycle cmds retry next frame (never use legacy single-slot). */
		if (cmd == WIKI_CMD_SET_BOUNDS)
		{
			WakeHelper();
			return;
		}
		if (cmd == WIKI_CMD_SET_VISIBLE ||
			cmd == WIKI_CMD_CREATE_TAB ||
			cmd == WIKI_CMD_ACTIVATE_TAB ||
			cmd == WIKI_CMD_CLOSE_TAB)
		{
			QueuePendingCmd(cmd, arg, a);
			WakeHelper();
			return;
		}

		std::snprintf(gIpc->cmd_arg, sizeof(gIpc->cmd_arg), "%s", arg ? arg : "");
		gIpc->cmd_a = a;
		gIpc->cmd = cmd;
		++gIpc->cmd_seq;
		WakeHelper();
	}

	/* Saved tabs persist file:///…/helper-home.html?v=… — after make install
	   wipes *.html those URLs 404 unless we map them back to about: + rewrite. */
	std::string NormalizeBuiltinNavigateUrl(const std::string& url)
	{
		if (url.empty() || url.rfind("about:", 0) == 0)
			return url;
		auto hasStem = [&](const char* stemHtml) {
			return url.find(stemHtml) != std::string::npos;
		};
		if (hasStem("helper-home.html"))
			return "about:helper-home";
		if (hasStem("raid-food.html"))
			return "about:raid-food";
		size_t n = 0;
		if (const CheatSheets::Sheet* sheets = CheatSheets::All(&n))
		{
			for (size_t i = 0; i < n; ++i)
			{
				const std::string needle = std::string(sheets[i].fileStem) + ".html";
				if (url.find(needle) != std::string::npos)
					return sheets[i].about;
			}
		}
		if (hasStem("live-dailies.html"))
			return "about:live-dailies";
		if (hasStem("live-news.html"))
			return "about:live-news";
		if (hasStem("live-fashion.html"))
			return "about:live-fashion";
		if (hasStem("live-tp.html"))
			return "about:live-tp";
		if (hasStem("live-progress.html"))
			return "about:live-progress";
		return url;
	}

	/* Map built-in about: URLs to file:/// before CEF sees them (CreateTab
	   used to pass about:raid-food raw → blank white page). */
	std::string ResolveNavigateUrl(const std::string& url)
	{
		if (url.empty())
			return {};
		const std::string nav = NormalizeBuiltinNavigateUrl(url);
		if (nav == "about:helper-home")
		{
			const std::string fileUrl = HomePage::EnsureFileUrl(AddonDir());
			if (fileUrl.empty())
				SetLocalStatus("Failed to write helper-home.html");
			return fileUrl;
		}
		if (nav == "about:raid-food")
		{
			const std::string fileUrl = RaidFood::EnsureFileUrl(AddonDir());
			if (fileUrl.empty())
				SetLocalStatus("Failed to write raid-food.html");
			return fileUrl;
		}
		{
			const std::string fileUrl = CheatSheets::ResolveAboutUrl(AddonDir(), nav);
			if (!fileUrl.empty())
				return fileUrl;
			if (CheatSheets::FindByAbout(nav.c_str()))
				SetLocalStatus("Failed to write cheat sheet HTML");
		}
		{
			const std::string fileUrl = LivePanels::ResolveAboutUrl(AddonDir(), nav);
			if (!fileUrl.empty())
				return fileUrl;
			if (LivePanels::IsLiveAbout(nav.c_str()))
			{
				SetLocalStatus("Failed to write Live panel HTML");
				/* Never hand CEF a raw about:live-* — Chromium shows a white “blocked” page. */
				return {};
			}
		}
		return nav;
	}

	/* Single IPC command slot — queue navigates until the helper is ready, then
	   flush once so SET_VISIBLE / SET_BOUNDS cannot wipe the URL. */
	void QueueNavigate(const std::string& url)
	{
		if (url.empty())
			return;
		const std::string resolved = ResolveNavigateUrl(url);
		if (resolved.empty())
			return;
		gPendingNavigate = resolved;
	}

	void FlushPendingNavigate()
	{
		if (gPendingNavigate.empty() || !gIpc || !HelperAlive())
			return;
		if (!gIpc->ready)
			return;
		PostCmd(WIKI_CMD_NAVIGATE, gPendingNavigate.c_str());
		if (G::API && G::API->Log)
		{
			char buf[256];
			std::snprintf(buf, sizeof(buf), "Navigate: %.200s", gPendingNavigate.c_str());
			G::API->Log(LOGL_INFO, ADDON_NAME, buf);
		}
		gPendingNavigate.clear();
	}

	void PushInput(const WikiInputEvent& ev)
	{
		if (!gIpc || !HelperAlive())
			return;
		uint32_t w = gIpc->input_write;
		uint32_t next = (w + 1u) % kWikiInputQueueSize;
		if (next == gIpc->input_read)
		{
			/* Queue full — drop oldest so newest keys/clicks still land. */
			gIpc->input_read = (gIpc->input_read + 1u) % kWikiInputQueueSize;
			static DWORD sLastFullMs = 0;
			const DWORD now = GetTickCount();
			if (sLastFullMs == 0 || (now - sLastFullMs) > 1000u)
			{
				sLastFullMs = now;
				SetLocalStatus("Input queue full — dropping oldest events");
			}
			w = gIpc->input_write;
			next = (w + 1u) % kWikiInputQueueSize;
			if (next == gIpc->input_read)
				return;
		}
		gIpc->input_q[w % kWikiInputQueueSize] = ev;
		gIpc->input_write = next;
		WakeHelper();
	}
}
