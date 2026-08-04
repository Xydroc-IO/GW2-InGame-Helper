/* CEF runtime WinHTTP download. */
#include "CefRuntime.h"
#include "CefRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace CefRuntimeDetail
{
bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*))
{
	Status(statusFn, "Downloading CEF…");
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[2048]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(urlW, 0, 0, &uc))
	{
		Status(statusFn, "CEF URL parse failed");
		return false;
	}

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/CEF",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
	{
		Status(statusFn, "CEF WinHttpOpen failed");
		return false;
	}

	const INTERNET_PORT port = uc.nPort ? uc.nPort :
		(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
			: INTERNET_DEFAULT_HTTP_PORT);
	HINTERNET conn = WinHttpConnect(session, host, port, 0);
	if (!conn)
	{
		WinHttpCloseHandle(session);
		Status(statusFn, "CEF WinHttpConnect failed");
		return false;
	}

	DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		Status(statusFn, "CEF WinHttpOpenRequest failed");
		return false;
	}

	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

	bool ok = false;
	if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(req, nullptr))
	{
		Status(statusFn, "CEF download request failed");
		goto done;
	}

	{
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode != 200)
		{
			char buf[96];
			std::snprintf(buf, sizeof(buf), "CEF download HTTP %lu",
				static_cast<unsigned long>(statusCode));
			Status(statusFn, buf);
			goto done;
		}
	}

	{
		DWORD contentLen = 0;
		DWORD lenSize = sizeof(contentLen);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &lenSize, WINHTTP_NO_HEADER_INDEX);

		HANDLE out = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
		{
			Status(statusFn, "CEF temp CreateFile failed");
			goto done;
		}

		DWORD total = 0;
		DWORD lastPct = 999;
		for (;;)
		{
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(req, &avail))
			{
				CloseHandle(out);
				DeleteFileW(outPath.c_str());
				Status(statusFn, "CEF download query failed");
				goto done;
			}
			if (avail == 0)
				break;
			std::vector<char> buf(avail);
			DWORD got = 0;
			if (!WinHttpReadData(req, buf.data(), avail, &got) || got == 0)
			{
				CloseHandle(out);
				DeleteFileW(outPath.c_str());
				Status(statusFn, "CEF download read failed");
				goto done;
			}
			DWORD written = 0;
			if (!WriteFile(out, buf.data(), got, &written, nullptr) || written != got)
			{
				CloseHandle(out);
				DeleteFileW(outPath.c_str());
				Status(statusFn, "CEF download write failed");
				goto done;
			}
			total += got;
			if (contentLen > 0)
			{
				const DWORD pct = static_cast<DWORD>(
					(static_cast<unsigned long long>(total) * 100ull) / contentLen);
				if (pct != lastPct && (pct % 5 == 0 || pct == 100))
				{
					char msg[64];
					std::snprintf(msg, sizeof(msg), "Downloading CEF… %lu%%",
						static_cast<unsigned long>(pct));
					Status(statusFn, msg);
					lastPct = pct;
				}
			}
		}
		CloseHandle(out);
		if (total == 0)
		{
			DeleteFileW(outPath.c_str());
			Status(statusFn, "CEF download empty");
			goto done;
		}
		ok = true;
	}

done:
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	return ok;
}
}
