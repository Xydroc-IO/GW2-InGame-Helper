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
		if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
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

	int CEF_CALLBACK OnBeforePopup(
		cef_life_span_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int /*popup_id*/,
		const cef_string_t* target_url, const cef_string_t*, cef_window_open_disposition_t,
		int user_gesture, const cef_popup_features_t*, cef_window_info_t*, cef_client_t**,
		cef_browser_settings_t*, cef_dictionary_value_t**, int*)
	{
		/* Always cancel native popup windows (OSR has no place for them). */
		if (!target_url)
			return 1;

		const std::string url = CefStringToUtf8(target_url);
		const std::string cur = MainFrameUrl(browser);

		NavLog("POPUP gesture=%d fromMain=%d adclick=%d promo=%d\n  url=%s\n  frame=%s\n  page=%s",
			user_gesture,
			frame && frame->is_main && frame->is_main(frame) ? 1 : 0,
			IsAdClickUrl(url) ? 1 : 0, IsPromotablePopupUrl(url) ? 1 : 0,
			url.c_str(), FrameUrl(frame).c_str(), cur.c_str());

		/* Some ad wrappers report the popup as main-frame. The tracker URL is the
		   reliable signal; always preserve it and hand it to the system browser. */
		if (user_gesture && IsAdClickUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		/* YouTube cannot stay in OSR — open the system browser instead of
		   replacing the guide (that looked like a mid-play refresh). */
		if (IsYoutubeHostUrl(url))
		{
			if (user_gesture)
				OpenExternalUrl(url);
			return 1;
		}

		if (IsTwitchHostUrl(url))
		{
			if (user_gesture)
			{
				OpenExternalUrl(url);
				SetStatus("Twitch needs codecs CEF omits — opened in your browser");
			}
			return 1;
		}

		/* Discord desktop deep links — launch the app (same as game CEF). */
		if (IsDiscordProtocolUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		/* Google sign-in / consent / captcha — open in the real browser so the
		   login can actually complete (OSR cannot). */
		if (IsExternalSignInUrl(url))
		{
			OpenExternalUrl(url);
			SetStatus("Opening sign-in in your browser (Open Ext)");
			return 1;
		}

		if (IsMediaOrCdnUrl(url) || IsYoutubeHostUrl(cur))
			return 1;

		const bool fromMain = frame && frame->is_main && frame->is_main(frame);
		if (user_gesture && IsPromotablePopupUrl(url))
		{
			/* Ads always target a third-party domain, and some wrappers report the
			   popup as main-frame, so domain is the reliable test rather than which
			   frame asked. Only our own bundled pages and a publisher's own
			   new-window link stay in-tab; everything third-party leaves so the
			   site is credited for the click. */
			const bool localPage = cur.rfind("file://", 0) == 0;
			if (fromMain && (localPage || IsSameSite(url, cur)))
			{
				NavLog("  -> IN-TAB (local=%d sameSite=%d)", localPage ? 1 : 0,
					IsSameSite(url, cur) ? 1 : 0);
				NavigateTo(url.c_str());
			}
			else
			{
				OpenExternalUrl(url);
			}
		}
		else
		{
			NavLog("  -> IGNORED (gesture=%d promo=%d)", user_gesture,
				IsPromotablePopupUrl(url) ? 1 : 0);
		}
		return 1;
	}

	/* Block main-frame navigations that steal the guide after an embed starts. */
	int CEF_CALLBACK OnBeforeBrowse(
		cef_request_handler_t*, cef_browser_t* browser, cef_frame_t* frame,
		cef_request_t* request, int user_gesture, int /*is_redirect*/)
	{
		if (!request || !request->get_url || !g_userfree_free)
			return 0;

		cef_string_userfree_t uf = request->get_url(request);
		if (!uf)
			return 0;
		const std::string url = CefStringToUtf8(uf);
		g_userfree_free(uf);

		const bool isMain = frame && frame->is_main && frame->is_main(frame);
		const bool fromAdFrame = !isMain && IsAdFrameUrl(FrameUrl(frame));

		std::string referrer;
		if (request->get_referrer_url)
		{
			cef_string_userfree_t ruf = request->get_referrer_url(request);
			if (ruf)
			{
				referrer = CefStringToUtf8(ruf);
				g_userfree_free(ruf);
			}
		}
		NavLog("BROWSE gesture=%d isMain=%d adFrame=%d adclick=%d\n  url=%s\n  frame=%s\n  ref=%s",
			user_gesture, isMain ? 1 : 0, fromAdFrame ? 1 : 0, IsAdClickUrl(url) ? 1 : 0,
			url.c_str(), FrameUrl(frame).c_str(), referrer.c_str());

		/* Ads may navigate either their own iframe or the top-level document.
		   A creative targeting _top arrives as a main-frame request, so the frame
		   no longer identifies it as an ad — the referrer still names the ad host,
		   and a publisher's own links never carry one. Anything a tracker, an ad
		   network, or an ad frame is behind leaves with its click URL intact. */
		const bool viaAdReferrer = isMain && IsAdFrameUrl(referrer);
		if (user_gesture &&
			(IsAdClickUrl(url) ||
				(isMain && IsAdFrameUrl(url)) ||
				viaAdReferrer ||
				(fromAdFrame && IsPromotablePopupUrl(url))))
		{
			OpenExternalUrl(url);
			return 1;
		}

		if (!isMain)
			return 0; /* allow iframe / media subloads */

		/* TP watchlist add/remove + any about: builtin — never let Chromium load
		   raw about:live-* (blocked white page). Rewrite to file:// first. */
		{
			if (ConsumeHelperNewTabUrl(url))
				return 1;
			std::string navTo;
			if (ConsumeTpActionUrl(url, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return 1;
			}
			if (url.rfind("about:", 0) == 0 && url != "about:blank")
			{
				const std::string resolved = ResolveBuiltinUrl(url.c_str());
				if (!resolved.empty() && resolved != url)
				{
					NavigateTo(resolved.c_str());
					return 1;
				}
				/* Never let Chromium show its white “blocked about:” page. */
				return 1;
			}
		}

		/* Google sign-in / consent / captcha — cannot complete in OSR. Open the
		   real browser (login) or bounce the user out of a /sorry captcha wall.
		   /sorry often arrives as a redirect (no user_gesture), so route it too. */
		if (IsExternalSignInUrl(url))
		{
			OpenExternalUrl(url);
			SetStatus(url.find("/sorry") != std::string::npos
				? "Google blocked the in-game browser — opened in your browser"
				: "Opening sign-in in your browser (Open Ext)");
			return 1;
		}

		if (IsMediaOrCdnUrl(url))
			return 1;

		/* discord:// cannot render in CEF — open the Discord app and CANCEL so the
		   https authorize page stays (navigating here = black page / web login). */
		if (IsDiscordProtocolUrl(url))
		{
			OpenExternalUrl(url);
			return 1;
		}

		const std::string cur = MainFrameUrl(browser);
		if (!IsYoutubeHostUrl(cur) && IsYoutubeHostUrl(url))
		{
			/* "Watch on YouTube" cards use a normal <a href> — open externally. */
			if (user_gesture)
				OpenExternalUrl(url);
			return 1;
		}

		/* Same for the "Watch on Twitch" card — playback can never work here. */
		if (!IsTwitchHostUrl(cur) && IsTwitchHostUrl(url))
		{
			if (user_gesture)
			{
				OpenExternalUrl(url);
				SetStatus("Twitch needs codecs CEF omits — opened in your browser");
			}
			return 1;
		}

		NavLog("  -> IN-TAB (main-frame navigation allowed)");
		return 0;
	}

	int CEF_CALLBACK OnOpenUrlFromTab(
		cef_request_handler_t*, cef_browser_t*, cef_frame_t*,
		const cef_string_t* target_url, cef_window_open_disposition_t, int user_gesture)
	{
		if (target_url && user_gesture)
		{
			const std::string url = CefStringToUtf8(target_url);
			NavLog("OPENFROMTAB gesture=%d promo=%d\n  url=%s", user_gesture,
				IsPromotablePopupUrl(url) ? 1 : 0, url.c_str());
			if (IsPromotablePopupUrl(url))
				OpenExternalUrl(url);
		}
		return 1; /* no new tabs in OSR */
	}

}
