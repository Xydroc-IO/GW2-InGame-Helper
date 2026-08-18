#include "Gw2Http.h"

#include <string>

#include <windows.h>
#include <winhttp.h>

namespace
{
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
}

bool Gw2Http::DownloadToFile(const char* url, const wchar_t* outPath, int timeoutMs)
{
	if (!url || !url[0] || !outPath || !outPath[0])
		return false;
	const std::wstring urlW = Utf8ToWide(url);
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[2048]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(urlW.c_str(), 0, 0, &uc))
		return false;

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Pack",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		return false;
	if (timeoutMs < 8000)
		timeoutMs = 8000;
	WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

	const INTERNET_PORT port = uc.nPort ? uc.nPort :
		(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
			: INTERNET_DEFAULT_HTTP_PORT);
	HINTERNET conn = WinHttpConnect(session, host, port, 0);
	if (!conn)
	{
		WinHttpCloseHandle(session);
		return false;
	}
	DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return false;
	}
	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
	/* Do not send Accept-Encoding. Wine may leave gzip bytes. Caller gunzips
	   if the body still starts with 1f 8b (GitHub Content-Encoding). */

	bool ok = false;
	HANDLE out = INVALID_HANDLE_VALUE;
	if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(req, nullptr))
		goto done;
	{
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode != 200)
			goto done;
	}
	out = CreateFileW(outPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (out == INVALID_HANDLE_VALUE)
		goto done;
	{
		unsigned long long total = 0;
		for (;;)
		{
			char buf[16384];
			DWORD got = 0;
			if (!WinHttpReadData(req, buf, sizeof(buf), &got))
			{
				CloseHandle(out);
				out = INVALID_HANDLE_VALUE;
				DeleteFileW(outPath);
				goto done;
			}
			if (got == 0)
				break;
			DWORD wr = 0;
			if (!WriteFile(out, buf, got, &wr, nullptr) || wr != got)
			{
				CloseHandle(out);
				out = INVALID_HANDLE_VALUE;
				DeleteFileW(outPath);
				goto done;
			}
			total += got;
			if (total > 512ull * 1024ull * 1024ull)
			{
				CloseHandle(out);
				out = INVALID_HANDLE_VALUE;
				DeleteFileW(outPath);
				goto done;
			}
		}
		CloseHandle(out);
		out = INVALID_HANDLE_VALUE;
		ok = total > 64;
		if (!ok)
			DeleteFileW(outPath);
	}

done:
	if (out != INVALID_HANDLE_VALUE)
		CloseHandle(out);
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	return ok;
}
