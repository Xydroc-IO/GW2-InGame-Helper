/* CEF OnBeforePopup / OnBeforeBrowse / OnOpenUrlFromTab — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "HelperInternal.h"
#include "WikiIpc.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_frame_capi.h"
#include "include/capi/cef_life_span_handler_capi.h"
#include "include/capi/cef_request_capi.h"
#include "include/capi/cef_request_handler_capi.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	int CEF_CALLBACK OnBeforePopup(
		cef_life_span_handler_t*, cef_browser_t* browser, cef_frame_t* frame, int /*popup_id*/,
		const cef_string_t* target_url, const cef_string_t*, cef_window_open_disposition_t disp,
		int user_gesture, const cef_popup_features_t*, cef_window_info_t*, cef_client_t**,
		cef_browser_settings_t*, cef_dictionary_value_t**, int*)
	{
		/* Always cancel native popup windows (OSR has no place for them). */
		if (!target_url)
			return 1;

		const std::string url = CefStringToUtf8(target_url);
		const std::string cur = MainFrameUrl(browser);

		NavLog("POPUP gesture=%d disp=%d fromMain=%d adclick=%d promo=%d\n  url=%s\n  frame=%s\n  page=%s",
			user_gesture, static_cast<int>(disp),
			frame && frame->is_main && frame->is_main(frame) ? 1 : 0,
			IsAdClickUrl(url) ? 1 : 0, IsPromotablePopupUrl(url) ? 1 : 0,
			url.c_str(), FrameUrl(frame).c_str(), cur.c_str());

		/* Middle-click often arrives as a background-tab popup, not OnOpenUrlFromTab. */
		if (user_gesture && static_cast<int>(disp) == 4 && TryOpenUrlInNewAddonTab(url))
			return 1;

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

		/* Back/forward onto a wiped live panel file — rewrite before Chromium
		   paints ERR_FILE_NOT_FOUND. */
		if (url.rfind("file:", 0) == 0 &&
			(url.find("live-browse-") != std::string::npos ||
				url.find("live-cheatsheets-hub") != std::string::npos ||
				url.find("live-legendary-") != std::string::npos ||
				url.find("live-dailies") != std::string::npos ||
				url.find("live-news") != std::string::npos ||
				url.find("live-fashion") != std::string::npos ||
				url.find("live-tp.html") != std::string::npos ||
				url.find("gw2-api-check.html") != std::string::npos))
		{
			const std::string about = AboutFromBrowseFileUrl(url);
			if (!about.empty())
			{
				const std::wstring pages = HelperPagesDir();
				std::wstring leaf;
				const size_t slash = url.find_last_of('/');
				if (slash != std::string::npos)
				{
					std::string name = url.substr(slash + 1);
					const size_t q = name.find('?');
					if (q != std::string::npos)
						name.resize(q);
					const size_t hash = name.find('#');
					if (hash != std::string::npos)
						name.resize(hash);
					leaf.reserve(name.size());
					for (char c : name)
						leaf.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
				}
				if (!pages.empty() && !leaf.empty() &&
					GetFileAttributesW((pages + L"\\" + leaf).c_str()) == INVALID_FILE_ATTRIBUTES)
				{
					const std::string resolved = ResolveBuiltinUrl(about.c_str());
					NavLog("  missing-live rewrite in=%s about=%s out=%s", url.c_str(),
						about.c_str(), resolved.empty() ? "(empty)" : resolved.c_str());
					if (!resolved.empty())
					{
						NavigateTo(resolved.c_str());
						return 1;
					}
				}
			}
		}

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
			if (ConsumeLedgerActionUrl(url, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return 1;
			}
			if (ConsumeBrowseHubActionUrl(url, &navTo))
			{
				if (!navTo.empty())
					NavigateTo(navTo.c_str());
				return 1;
			}
			if (url.rfind("about:", 0) == 0 && url != "about:blank")
			{
				const std::string resolved = ResolveBuiltinUrl(url.c_str());
				NavLog("  about-rewrite in=%s out=%s", url.c_str(),
					resolved.empty() ? "(empty)" : resolved.c_str());
				if (!resolved.empty() && resolved != url &&
					resolved.rfind("about:", 0) != 0)
				{
					NavigateTo(resolved.c_str());
					return 1;
				}
				/* Never let Chromium show its white “blocked about:” page.
				   Unknown about: is rewritten to about:blank#blocked by CEF
				   before this runs — cancel and stay put. */
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
		const cef_string_t* target_url, cef_window_open_disposition_t disp, int user_gesture)
	{
		if (target_url && user_gesture)
		{
			const std::string url = CefStringToUtf8(target_url);
			NavLog("OPENFROMTAB gesture=%d disp=%d promo=%d\n  url=%s", user_gesture,
				static_cast<int>(disp), IsPromotablePopupUrl(url) ? 1 : 0, url.c_str());
			if (IsNewTabOrWindowDisposition(disp) && TryOpenUrlInNewAddonTab(url))
				return 1;
			if (IsPromotablePopupUrl(url))
				OpenExternalUrl(url);
		}
		return 1; /* cancel Chromium's extra browser — OSR has none */
	}

}
