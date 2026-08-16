/* CEF navigation / ad / Open Ext policy — HelperDetail. */
#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "HelperInternal.h"
#include "WikiIpc.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_client_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_request_capi.h"
#include "include/capi/cef_request_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	bool IsDiscordProtocolUrl(const std::string& url)
	{
		/* discord://… — Discord desktop app deep link (OAuth handoff). */
		return url.rfind("discord:", 0) == 0 || url.rfind("Discord:", 0) == 0;
	}

	bool IsLaunchableExternalUrl(const std::string& url)
	{
		return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 ||
			IsDiscordProtocolUrl(url);
	}

	/* Navigation decision trace. Silent unless a "navlog.on" marker file sits
	   next to the helper exe, so shipped builds never touch the disk. */
	void NavLog(const char* fmt, ...)
	{
		static int enabled = -1;
		static FILE* out = nullptr;
		if (enabled < 0)
		{
			wchar_t exe[MAX_PATH]{};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			std::wstring dir(exe);
			const size_t slash = dir.find_last_of(L"\\/");
			dir = (slash == std::wstring::npos) ? std::wstring(L".") : dir.substr(0, slash);
			enabled = (GetFileAttributesW((dir + L"\\navlog.on").c_str()) !=
				INVALID_FILE_ATTRIBUTES) ? 1 : 0;
			if (enabled == 1)
				out = _wfopen((dir + L"\\navlog.txt").c_str(), L"a");
		}
		if (enabled != 1 || !out)
			return;
		va_list ap;
		va_start(ap, fmt);
		std::vfprintf(out, fmt, ap);
		va_end(ap);
		std::fputc('\n', out);
		std::fflush(out);
	}

	void OpenExternalUrl(const std::string& url)
	{
		if (!IsLaunchableExternalUrl(url))
		{
			NavLog("  -> DROPPED (not launchable) %s", url.c_str());
			return;
		}
		NavLog("  -> EXTERNAL len=%zu %s", url.size(), url.c_str());
		/* Prefer DLL-side ShellExecute (Proton/Wine: helper process often no-ops). */
		if (gIpc)
		{
			/* Half a click tracker opens a blank error page, which reads as "ads are
			   broken" — refuse the handoff instead of sending a truncated URL. */
			if (url.size() >= sizeof(gIpc->open_ext_url))
			{
				NavLog("  -> REFUSED (too long, %zu bytes)", url.size());
				SetStatus("Link too long to open externally");
				return;
			}
			std::snprintf(gIpc->open_ext_url, sizeof(gIpc->open_ext_url), "%s", url.c_str());
			MemoryBarrier();
			++gIpc->open_ext_seq;
			SetStatus(IsDiscordProtocolUrl(url)
				? "Opening Discord app…"
				: "Opened in system browser (Open Ext)");
			return;
		}
		ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	/* Ask the DLL to open a URL in a new helper tab (keeps the current Live page). */
	void QueueOpenInAddonTab(const std::string& url)
	{
		if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0 &&
			url.rfind("about:", 0) != 0 && url.rfind("file://", 0) != 0)
			return;
		if (!gIpc)
			return;
		if (url.size() >= sizeof(gIpc->open_tab_url))
		{
			SetStatus("Link too long for a new tab");
			return;
		}
		NavLog("  -> ADDON-TAB %s", url.c_str());
		std::snprintf(gIpc->open_tab_url, sizeof(gIpc->open_tab_url), "%s", url.c_str());
		MemoryBarrier();
		++gIpc->open_tab_seq;
		SetStatus("Opening in a new tab…");
	}

	/* Browse hub: open catalog site by id in a new addon tab (works for about: homes). */
	void QueueOpenSiteInAddonTab(const std::string& siteId, bool newTab)
	{
		if (!gIpc || siteId.empty() || siteId.size() >= sizeof(gIpc->open_site_id))
			return;
		for (char c : siteId)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_'))
				return;
		}
		NavLog("  -> ADDON-SITE %s new=%d", siteId.c_str(), newTab ? 1 : 0);
		std::snprintf(gIpc->open_site_id, sizeof(gIpc->open_site_id), "%s", siteId.c_str());
		gIpc->open_site_new_tab = newTab ? 1u : 0u;
		MemoryBarrier();
		++gIpc->open_site_seq;
		SetStatus(newTab ? "Opening in a new tab…" : "Opening…");
	}

	bool IsNewTabOrWindowDisposition(cef_window_open_disposition_t d)
	{
		/* CEF WOD_NEW_FOREGROUND_TAB..NEW_WINDOW (3–6). */
		const int v = static_cast<int>(d);
		return v >= 3 && v <= 6;
	}

	std::string UrlDecodeQueryValue(const std::string& in)
	{
		std::string out;
		out.reserve(in.size());
		for (size_t i = 0; i < in.size(); ++i)
		{
			if (in[i] == '+' )
			{
				out.push_back(' ');
				continue;
			}
			if (in[i] == '%' && i + 2 < in.size())
			{
				auto hex = [](char c) -> int {
					if (c >= '0' && c <= '9') return c - '0';
					if (c >= 'a' && c <= 'f') return c - 'a' + 10;
					if (c >= 'A' && c <= 'F') return c - 'A' + 10;
					return -1;
				};
				const int hi = hex(in[i + 1]);
				const int lo = hex(in[i + 2]);
				if (hi >= 0 && lo >= 0)
				{
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 2;
					continue;
				}
			}
			out.push_back(in[i]);
		}
		return out;
	}

	/* Character → gw2efficiency: file://…?gw2igh-newtab=https%3A%2F%2F…
	   (about: is blocked from file:// pages — same lesson as TP watchlist). */
	bool ConsumeHelperNewTabUrl(const std::string& url)
	{
		static const char kAbout[] = "about:helper-newtab:";
		if (url.rfind(kAbout, 0) == 0)
		{
			QueueOpenInAddonTab(url.substr(sizeof(kAbout) - 1));
			return true;
		}

		const size_t mark = url.find("gw2igh-newtab=");
		if (mark == std::string::npos)
			return false;
		std::string enc = url.substr(mark + 14);
		const size_t cut = enc.find_first_of("&#");
		if (cut != std::string::npos)
			enc.resize(cut);
		const std::string target = UrlDecodeQueryValue(enc);
		if (target.rfind("https://", 0) != 0 && target.rfind("http://", 0) != 0)
			return false;
		QueueOpenInAddonTab(target);
		return true;
	}

	/* Media / CDN / account URLs must never become the main-frame document —
	   promoting them after an embed Play looks like the guide refreshed. */
	bool IsMediaOrCdnUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("googlevideo.com") || has("ytimg.com") || has("ggpht.com") ||
			has("googleusercontent.com") || has("youtube-nocookie.com/embed") ||
			has("youtube.com/embed") || has("youtube.com/live_chat") ||
			has("youtube.com/watch") || has("youtu.be/") ||
			has("accounts.google.com") || has("accounts.youtube.com") ||
			has("consent.youtube.com") || has("consent.google.com") ||
			has("vimeo.com") || has("player.vimeo.com");
	}

	/* Google sign-in, consent, and the /sorry "unusual traffic" captcha cannot be
	   completed in embedded OSR (Google blocks the UA and reCAPTCHA can't be
	   solved). These previously matched IsMediaOrCdnUrl and were cancelled with no
	   action, so the button looked dead. Hand them to the system browser instead. */
	bool IsExternalSignInUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("accounts.google.com") || has("accounts.youtube.com") ||
			has("consent.google.com") || has("consent.youtube.com") ||
			has("google.com/sorry") || has("google.com/recaptcha");
	}

	bool IsYoutubeHostUrl(const std::string& url)
	{
		return url.find("youtube.com") != std::string::npos ||
			url.find("youtu.be") != std::string::npos ||
			url.find("youtube-nocookie.com") != std::string::npos;
	}

	/* Official CEF binaries ship without the proprietary codecs (H.264 / AAC)
	   Twitch streams with, so its player always ends at "Error #4000 — resource
	   format not supported". Enabling them means building Chromium from source
	   and licensing the codecs, so route Twitch to the system browser instead. */
	bool IsTwitchHostUrl(const std::string& url)
	{
		return url.find("twitch.tv") != std::string::npos ||
			url.find("ttvnw.net") != std::string::npos;
	}

	bool IsGuildjenUrl(const std::string& url)
	{
		return url.find("guildjen.com") != std::string::npos;
	}

	bool IsAdFrameUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return has("nitropay.com") || has("s.nitropay.com") ||
			has("googlesyndication.com") || has("doubleclick.net") ||
			has("googleadservices.com") || has("pagead2.googlesyndication") ||
			has("adservice.google") || has("adnxs.com") ||
			has("amazon-adsystem.com") || has("ads-twitter.com") ||
			has("facebook.net") || has("connect.facebook") ||
			has("securepubads.g.doubleclick") || has("pagead") ||
			has("adsystem") || has("advertising");
	}

	/* Never cancel ad / consent / analytics hosts — ads must load for site partners. */
	bool IsAdOrConsentUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return IsAdFrameUrl(url) ||
			has("cookieinformation.com") || has("policy.app.cookieinformation") ||
			has("consent.cookiebot") || has("onetrust.com") ||
			has("cookielaw.org") || has("fundingchoicesmessages") ||
			has("consent.google.com");
	}

	bool HasQueryParam(const std::string& url, const char* key)
	{
		const size_t q = url.find('?');
		if (q == std::string::npos)
			return false;
		const std::string k(key);
		return url.find("?" + k + "=", q) != std::string::npos ||
			url.find("&" + k + "=", q) != std::string::npos;
	}

	/* Ad network click identifiers. A landing page only carries one of these when
	   a network billed the click, so it belongs to the advertiser even when the
	   creative reports the publisher as its referrer (seen on Google display). */
	bool HasAdClickId(const std::string& url)
	{
		static const char* const kClickIds[] = {
			"gclid", "dclid", "gbraid", "wbraid", "gad_source", "gad_campaignid",
			"msclkid", "fbclid", "ttclid", "twclid",
		};
		for (const char* key : kClickIds)
		{
			if (HasQueryParam(url, key))
				return true;
		}
		return false;
	}

	/* Ad click-through navigations must leave the OSR browser. Loading ad
	   resources stays in CEF; only explicit tracker/click URLs are handed off. */
	bool IsAdClickUrl(const std::string& url)
	{
		auto has = [&](const char* s) {
			return url.find(s) != std::string::npos;
		};
		return HasAdClickId(url) ||
			has("adclick.g.doubleclick.net/") ||
			has("googleadservices.com/pagead/aclk") ||
			has("googlesyndication.com/pagead/aclk") ||
			has("googlesyndication.com/pagead/clk") ||
			has("amazon-adsystem.com/x/c/") ||
			has("adnxs.com/click") ||
			(IsAdFrameUrl(url) &&
				(has("/pcs/click") || has("/click?") ||
					has("/click/") || has("/clickthrough")));
	}

	std::string UrlHost(const std::string& url)
	{
		const size_t scheme = url.find("://");
		const size_t start = (scheme == std::string::npos) ? 0 : scheme + 3;
		const size_t end = url.find_first_of("/?#", start);
		std::string host = (end == std::string::npos)
			? url.substr(start)
			: url.substr(start, end - start);
		const size_t at = host.rfind('@');
		if (at != std::string::npos)
			host.erase(0, at + 1);
		const size_t colon = host.find(':');
		if (colon != std::string::npos)
			host.erase(colon);
		for (char& c : host)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return host;
	}

	/* Last two labels — enough to tell "same publisher" from "third-party ad".
	   wiki.guildwars2.com and www.guildwars2.com both reduce to guildwars2.com. */
	std::string BaseDomain(const std::string& host)
	{
		const size_t last = host.rfind('.');
		if (last == std::string::npos || last == 0)
			return host;
		const size_t prev = host.rfind('.', last - 1);
		if (prev == std::string::npos)
			return host;
		return host.substr(prev + 1);
	}

	bool IsSameSite(const std::string& a, const std::string& b)
	{
		const std::string da = BaseDomain(UrlHost(a));
		const std::string db = BaseDomain(UrlHost(b));
		return !da.empty() && da == db;
	}

	bool IsPromotablePopupUrl(const std::string& url)
	{
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
			return false;
		if (IsMediaOrCdnUrl(url) || IsYoutubeHostUrl(url) || IsTwitchHostUrl(url))
			return false;
		return true;
	}

	std::string FrameUrl(cef_frame_t* frame)
	{
		if (!frame || !frame->get_url || !g_userfree_free)
			return {};
		cef_string_userfree_t uf = frame->get_url(frame);
		if (!uf)
			return {};
		const std::string out = CefStringToUtf8(uf);
		g_userfree_free(uf);
		return out;
	}

	std::string MainFrameUrl(cef_browser_t* browser)
	{
		if (!browser || !g_userfree_free)
			return {};
		cef_frame_t* frame = browser->get_main_frame(browser);
		if (!frame)
			return {};
		const std::string out = FrameUrl(frame);
		frame->base.release(&frame->base);
		return out;
	}

	bool TryOpenUrlInNewAddonTab(const std::string& url)
	{
		if (url.empty() || url == "about:blank")
			return false;
		if (IsAdClickUrl(url) || IsYoutubeHostUrl(url) || IsTwitchHostUrl(url) ||
			IsDiscordProtocolUrl(url) || IsExternalSignInUrl(url) || IsMediaOrCdnUrl(url))
			return false;
		if (ConsumeHelperNewTabUrl(url))
			return true;
		if (HasQueryParam(url, "gw2igh-fav-toggle") ||
			HasQueryParam(url, "gw2igh-fav-folder-create") ||
			HasQueryParam(url, "gw2igh-fav-folder-move") ||
			HasQueryParam(url, "gw2igh-fav-folder-delete"))
			return false;

		if (url.find("live-browse-") != std::string::npos ||
			url.find("live-cheatsheets-hub") != std::string::npos)
		{
			size_t q = url.find('?');
			if (q != std::string::npos)
			{
				std::string query = url.substr(q + 1);
				const size_t hash = query.find('#');
				if (hash != std::string::npos)
					query.resize(hash);
				const std::string openSite = ParseQueryValue(query, "gw2igh-open-site");
				const std::string aboutKey = ParseQueryValue(query, "gw2igh-about");
				if (!openSite.empty())
				{
					QueueOpenSiteInAddonTab(openSite, true);
					return true;
				}
				if (!aboutKey.empty())
				{
					for (char c : aboutKey)
					{
						if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
							return false;
					}
					if (aboutKey != "browse-hub" && aboutKey.rfind("browse-cat-", 0) != 0)
						return false;
					QueueOpenInAddonTab(std::string("about:") + aboutKey);
					return true;
				}
			}
		}

		if (url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0 ||
			url.rfind("about:", 0) == 0 || url.rfind("file://", 0) == 0)
		{
			QueueOpenInAddonTab(url);
			return true;
		}
		return false;
	}
}
