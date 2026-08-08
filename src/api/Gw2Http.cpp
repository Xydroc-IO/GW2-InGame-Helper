#include "Gw2Http.h"

#include "ApiBudget.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace
{
	/* Per-thread session/connection so Live workers can fetch in parallel.
	   GW2 allows ~600 req/min — a handful of concurrent GETs is fine and
	   cuts wall-clock from sum(latency) to max(latency). */
	struct TlsHttp
	{
		HINTERNET session = nullptr;
		HINTERNET conn = nullptr;
		std::wstring host;
		INTERNET_PORT port = 0;
		int timeoutMs = 0;
	};

	TlsHttp& Tls()
	{
		thread_local TlsHttp t;
		return t;
	}

	std::wstring Utf8ToWide(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring out(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
		return out;
	}

	void SleepBackoff(int attempt)
	{
		const DWORD ms = 250u << (attempt < 2 ? attempt : 2);
		Sleep(ms);
	}

	void ResetConn(TlsHttp& t)
	{
		if (t.conn)
		{
			WinHttpCloseHandle(t.conn);
			t.conn = nullptr;
		}
		t.host.clear();
		t.port = 0;
	}

	bool EnsureSession(TlsHttp& t, int timeoutMs)
	{
		if (!t.session)
		{
			t.session = WinHttpOpen(L"GW2-InGame-Helper/Live",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0);
			if (!t.session)
				return false;
		}
		if (timeoutMs < 400)
			timeoutMs = 400;
		if (t.timeoutMs != timeoutMs)
		{
			WinHttpSetTimeouts(t.session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
			t.timeoutMs = timeoutMs;
		}
		return true;
	}

	bool EnsureConn(TlsHttp& t, const wchar_t* host, INTERNET_PORT port)
	{
		if (t.conn && (t.host != host || t.port != port))
			ResetConn(t);
		if (!t.conn)
		{
			t.conn = WinHttpConnect(t.session, host, port, 0);
			if (!t.conn)
				return false;
			t.host = host;
			t.port = port;
		}
		return true;
	}
}

Gw2Http::Result Gw2Http::Get(const char* url, const char* bearerToken, int timeoutMs)
{
	Result r;
	if (!url || !url[0])
	{
		r.error = "empty url";
		return r;
	}

	const std::wstring urlW = Utf8ToWide(url);
	if (urlW.empty())
	{
		r.error = "url encode failed";
		return r;
	}

	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[4096]{};
	wchar_t extra[8192]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 4096;
	uc.lpszExtraInfo = extra;
	uc.dwExtraInfoLength = 8192;
	if (!WinHttpCrackUrl(urlW.c_str(), 0, 0, &uc))
	{
		r.error = "url parse failed";
		return r;
	}

	std::wstring objectName = path;
	if (extra[0])
		objectName += extra;

	const INTERNET_PORT port = uc.nPort ? uc.nPort :
		(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
			: INTERNET_DEFAULT_HTTP_PORT);

	TlsHttp& t = Tls();
	if (!EnsureSession(t, timeoutMs))
	{
		r.error = "WinHttpOpen failed";
		return r;
	}
	if (!EnsureConn(t, host, port))
	{
		r.error = "WinHttpConnect failed";
		return r;
	}

	/* Cap concurrent api.guildwars2.com GETs across Wallet/Vault/Crafting/Instances. */
	const bool budgeted = (_wcsicmp(host, L"api.guildwars2.com") == 0);
	const int budgetWait = timeoutMs > 0 ? (timeoutMs + 8000) : 30000;
	bool budgetHeld = false;
	if (budgeted)
	{
		if (!ApiBudget::Acquire(budgetWait))
		{
			r.error = "api concurrency budget";
			return r;
		}
		budgetHeld = true;
	}
	struct BudgetGuard
	{
		bool* held = nullptr;
		~BudgetGuard() { if (held && *held) { ApiBudget::Release(); *held = false; } }
	} budgetGuard{ budgeted ? &budgetHeld : nullptr };

	/* Short budgets = one shot. Keep-alive per thread — do not open/close per call. */
	const int maxAttempts = (timeoutMs <= 4000) ? 1 : 4;
	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(t.conn, L"GET", objectName.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			r.error = "WinHttpOpenRequest failed";
			ResetConn(t);
			break;
		}

		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

		std::wstring headers;
		headers += L"Accept: application/json, application/rss+xml, application/xml, text/xml, */*\r\n";
		headers += L"Accept-Language: en\r\n";
		headers += L"Connection: keep-alive\r\n";
		if (bearerToken && bearerToken[0])
		{
			headers += L"Authorization: Bearer ";
			headers += Utf8ToWide(bearerToken);
			headers += L"\r\n";
		}

		bool sent = WinHttpSendRequest(req, headers.c_str(),
			static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE;
		bool recv = sent && WinHttpReceiveResponse(req, nullptr);

		if (!recv)
		{
			WinHttpCloseHandle(req);
			r.error = "request failed";
			ResetConn(t);
			if (!EnsureConn(t, host, port))
				break;
			if (attempt + 1 < maxAttempts)
				SleepBackoff(attempt);
			continue;
		}

		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		r.status = statusCode;

		if (statusCode == 429)
		{
			WinHttpCloseHandle(req);
			r.error = "rate limited";
			r.status = 429;
			/* Token bucket is ~5/s — wait longer than the generic backoff. */
			if (attempt + 1 < maxAttempts)
				Sleep(1000u + 500u * static_cast<DWORD>(attempt));
			continue;
		}

		std::string body;
		body.reserve(8192);
		for (;;)
		{
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(req, &avail))
				break;
			if (avail == 0)
				break;
			std::vector<char> chunk(avail);
			DWORD read = 0;
			if (!WinHttpReadData(req, chunk.data(), avail, &read) || read == 0)
				break;
			body.append(chunk.data(), read);
			if (body.size() > 8u * 1024u * 1024u)
			{
				r.error = "response too large";
				body.clear();
				break;
			}
		}

		WinHttpCloseHandle(req);

		if (r.error == "response too large")
			break;

		r.body = std::move(body);
		if (statusCode >= 200 && statusCode < 300)
		{
			r.ok = true;
			r.error.clear();
		}
		else
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "HTTP %lu",
				static_cast<unsigned long>(statusCode));
			r.error = buf;
		}
		break;
	}

	return r;
}

Gw2Http::Result Gw2Http::Api(const char* pathAndQuery, const char* bearerToken, int timeoutMs)
{
	if (!pathAndQuery || !pathAndQuery[0])
	{
		Result r;
		r.error = "empty api path";
		return r;
	}
	std::string url = "https://api.guildwars2.com";
	if (pathAndQuery[0] != '/')
		url += '/';
	url += pathAndQuery;
	return Get(url.c_str(), bearerToken, timeoutMs);
}
